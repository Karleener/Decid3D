#include "Regression.h"

#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "Fichiers.h"
#include "Graphique.h"
#include "Plateforme.h"
#include "Toile.h"

namespace studio {

namespace {
slint::SharedString S(const std::string& s) { return slint::SharedString(s); }
struct Param { const char* m_cle; const char* m_libelle; const char* m_sigle; double m_defaut; bool m_entier; };
struct FamilleR { const char* m_id; const char* m_court; const char* m_nom; std::vector<Param> m_params; };
const std::vector<FamilleR>& Familles() {
    static const std::vector<FamilleR> f = {
        {"svm", "SVM", "SVM (ε-régression)", {{"gamma", "γ (noyau gaussien)", "γ", 2, false}, {"c", "C, pénalité", "C", 10, false}, {"epsilon", "ε, tube sans pénalité", "ε", 0.05, false}}},
        {"mlp", "Réseau", "Réseau de neurones (une sortie)", {{"neurones", "Neurones cachés", "neurones", 10, true}, {"iterations_max", "Itérations", "itér.", 1000, true}}},
        {"foret_aleatoire", "Forêt", "Forêt aléatoire", {{"nb_arbres", "Nombre d’arbres", "arbres", 100, true}, {"profondeur_max", "Profondeur maximale", "prof.", 10, true}}},
        {"kppv", "k-PPV", "k plus proches voisins (moyenne)", {{"k", "k, nombre de voisins", "k", 5, true}}},
    };
    return f;
}
double LireNombre(const std::string& t, double defaut) {
    std::string u = t;
    for (auto& c : u) if (c == ',') c = '.';
    char* fin = nullptr;
    const double v = std::strtod(u.c_str(), &fin);
    return fin && fin != u.c_str() && std::isfinite(v) ? v : defaut;
}
std::vector<std::string> Decouper(const std::string& l, char sep) {
    std::vector<std::string> v;
    size_t a = 0;
    while (true) {
        const size_t b = l.find(sep, a);
        std::string c = l.substr(a, b == std::string::npos ? std::string::npos : b - a);
        while (!c.empty() && (c.front() == ' ' || c.front() == '"')) c.erase(c.begin());
        while (!c.empty() && (c.back() == ' ' || c.back() == '"' || c.back() == '\r')) c.pop_back();
        v.push_back(c);
        if (b == std::string::npos) break;
        a = b + 1;
    }
    return v;
}
std::string TexteMesure(const decid::ValeurJson& m, const char* cle) {
    return m.Contient(cle) && !m.Membre(cle).EstNul() ? NombreFr(m.Membre(cle).Nombre(), 4) : "—";
}
} // namespace

Regression::Regression(Fenetre& fenetre, Moteur& moteur, Etude& etude) : m_fenetre(fenetre), m_moteur(moteur), m_etude(etude) {
    auto& g = m_fenetre.global<::Regression>();
    std::vector<slint::SharedString> jeux;
    int iris = 0, k = 0;
    for (const auto& n : m_etude.NomsBibliotheque()) { jeux.push_back(S(n)); if (n == "Iris") iris = k; ++k; }
    g.set_jeux(std::make_shared<slint::VectorModel<slint::SharedString>>(jeux));
    g.set_jeu(iris);
    g.on_choisir_jeu([this](int i) { m_fichierLocal.clear(); m_fenetre.global<::Regression>().set_fichier(""); m_fenetre.global<::Regression>().set_jeu(i); m_fenetre.global<::Regression>().set_lu(false); });
    g.on_choisir_fichier([this] {
        const auto c = app::plateforme::ChoisirFichier(L"Fichier de données", {{L"Fichiers CSV", L"*.csv;*.txt"}, {L"Tous les fichiers", L"*.*"}});
        if (!c) return;
        m_fichierLocal = *c;
        m_fenetre.global<::Regression>().set_fichier(S(decid::CheminUtf8(c->filename())));
        m_fenetre.global<::Regression>().set_lu(false);
    });
    g.on_lire([this] { Lire(); });
    g.on_choisir_cible([this](int i) { m_fenetre.global<::Regression>().set_cible(i); MajEntrees(); });
    g.on_choisir_famille([this](int i) { ChoisirFamille(i); });
    g.on_changer_parametre([this](int i, slint::SharedString t) {
        const auto& f = Familles()[(size_t)std::clamp(m_fenetre.global<::Regression>().get_famille(), 0, 3)];
        if (i < 0 || i >= (int)f.m_params.size()) return;
        m_valeurs[std::string(f.m_id) + "." + f.m_params[(size_t)i].m_cle] = std::string(t);
        MajParametres();
    });
    g.on_apprendre([this] { Apprendre(); });
    g.on_choisir_essai([this](int k) { m_courant = k; Afficher(); });
    g.on_changer_vue([this] { Peindre(); });
    g.on_exporter([this] {
        if (m_courant < 0) return;
        const auto& e = m_essais[(size_t)m_courant];
        const auto c = app::plateforme::ChoisirFichierEnregistrement(L"Enregistrer les prédictions", L"predictions.csv", L"csv", {{L"Fichier CSV", L"*.csv"}});
        if (!c) return;
        std::error_code ec;
        fs::copy_file(m_moteur.DossierResultat(e.m_tache) / "regression_predictions.csv", *c, fs::copy_options::overwrite_existing, ec);
        m_fenetre.invoke_montrer_toast(S(ec ? "Copie impossible : " + ec.message() : "Prédictions enregistrées : " + decid::CheminUtf8(c->filename())));
    });
    g.on_taille([this](float l, float h) { if (std::abs(l - m_l) < 1 && std::abs(h - m_h) < 1) return; const bool premier = m_l < 1; m_l = l; m_h = h; if (premier) Peindre(); else m_redim.start(slint::TimerMode::SingleShot, std::chrono::milliseconds(80), [this] { Peindre(); }); });
    ChoisirFamille(0);
}

void Regression::ChoisirFamille(int i) {
    auto& g = m_fenetre.global<::Regression>();
    g.set_famille(std::clamp(i, 0, 3));
    g.set_famille_nom(S(Familles()[(size_t)g.get_famille()].m_nom));
    MajParametres();
}

void Regression::MajParametres() {
    auto& g = m_fenetre.global<::Regression>();
    const auto& f = Familles()[(size_t)g.get_famille()];
    std::vector<ParametreSaisi> v;
    for (const auto& p : f.m_params) {
        const auto it = m_valeurs.find(std::string(f.m_id) + "." + p.m_cle);
        v.push_back({S(p.m_cle), S(p.m_libelle), S(it != m_valeurs.end() ? it->second : NombreFr(p.m_defaut, 6))});
    }
    g.set_parametres(std::make_shared<slint::VectorModel<ParametreSaisi>>(v));
}

// Lecture brute : noms des colonnes et deux premieres lignes ; le moteur relira le fichier avec ce contrat
void Regression::Lire() {
    auto& g = m_fenetre.global<::Regression>();
    g.set_erreur("");
    m_chemin = m_fichierLocal;
    if (m_chemin.empty()) {
        const auto s = m_etude.SourceBibliotheque(g.get_jeu(), 0);
        if (!s) { g.set_erreur("Bibliothèque introuvable."); return; }
        m_chemin = s->m_chemin;
    }
    auto texte = decid::LireFichierTexte(m_chemin);
    if (!texte.Ok()) { g.set_erreur(S(texte.Err().Texte())); return; }
    const char sep = g.get_separateur() == 1 ? ';' : g.get_separateur() == 2 ? '\t' : ',';
    std::vector<std::string> lignes;
    size_t debut = 0;
    const std::string& t = texte.Valeur();
    while (debut < t.size() && lignes.size() < 3) {
        size_t fin = t.find('\n', debut);
        if (fin == std::string::npos) fin = t.size();
        std::string l = t.substr(debut, fin - debut);
        if (!l.empty() && l.back() == '\r') l.pop_back();
        if (!l.empty()) lignes.push_back(l);
        debut = fin + 1;
    }
    if (lignes.empty()) { g.set_erreur("Fichier vide."); return; }
    const auto premiere = Decouper(lignes[0], sep);
    m_colonnes.clear();
    for (size_t i = 0; i < premiere.size(); ++i)
        m_colonnes.push_back(g.get_en_tete() == 1 && !premiere[i].empty() ? premiere[i] : "colonne " + std::to_string(i + 1));
    if (m_colonnes.size() < 2) { g.set_erreur("Il faut au moins deux colonnes : une entrée et la valeur à prédire."); return; }
    std::vector<slint::SharedString> cols;
    for (const auto& c : m_colonnes) cols.push_back(S(c));
    g.set_colonnes(std::make_shared<slint::VectorModel<slint::SharedString>>(cols));
    g.set_cible((int)m_colonnes.size() - 1);   // ancien Decid : cible en derniere colonne
    std::string pl;
    for (size_t i = 0; i < lignes.size() && i < 2; ++i) pl += (i ? " | " : "") + lignes[i].substr(0, 60);
    g.set_premieres_lignes(S(pl));
    m_essais.clear();
    m_courant = -1;
    g.set_lu(true);
    MajEntrees();
    Afficher();
}

void Regression::MajEntrees() {
    auto& g = m_fenetre.global<::Regression>();
    std::vector<slint::SharedString> e;
    for (int i = 0; i < (int)m_colonnes.size(); ++i) if (i != g.get_cible()) e.push_back(S(m_colonnes[(size_t)i]));
    g.set_entrees(std::make_shared<slint::VectorModel<slint::SharedString>>(e));
    g.set_coupe(std::max(0, (int)e.size() - 1));
}

void Regression::Apprendre() {
    auto& g = m_fenetre.global<::Regression>();
    if (m_calcul || m_colonnes.empty()) return;
    g.set_erreur("");
    const auto& f = Familles()[(size_t)g.get_famille()];
    decid::ValeurJson r = decid::ValeurJson::Objet();
    r["operation"] = "regression";
    decid::ValeurJson s = decid::ValeurJson::Objet();
    s["type"] = "csv"; s["chemin"] = decid::CheminUtf8(m_chemin);
    s["en_tete"] = g.get_en_tete() == 1 ? "oui" : "non";
    s["separateur"] = std::string(1, g.get_separateur() == 1 ? ';' : g.get_separateur() == 2 ? '\t' : ',');
    s["colonne_cible"] = g.get_cible();
    r["source"] = s;
    decid::ValeurJson pa = decid::ValeurJson::Objet();
    pa["methode"] = "aleatoire";
    pa["ratio"] = g.get_part() == 0 ? 0.5 : g.get_part() == 2 ? 0.8 : 0.7;
    pa["graine"] = (long long)LireNombre(std::string(g.get_graine()), 1);
    r["partage"] = pa;
    decid::ValeurJson n = decid::ValeurJson::Objet();
    if (g.get_normaliser()) { n["methode"] = "minmax"; n["bas"] = 0; n["haut"] = 1; } else n["methode"] = "aucune";
    r["normalisation"] = n;
    decid::ValeurJson a = decid::ValeurJson::Objet();
    a["famille"] = f.m_id;
    const std::string id = f.m_id;
    if (id == "svm") a["noyau"] = "RBF";
    if (id == "mlp") { a["activation"] = "SIGMOIDE_SYM"; a["methode"] = "RPROP"; }
    if (id == "foret_aleatoire") a["min_echantillons"] = 2;
    std::string titre = f.m_court, details;
    for (const auto& p : f.m_params) {
        const auto it = m_valeurs.find(id + "." + p.m_cle);
        const double v = it == m_valeurs.end() ? p.m_defaut : LireNombre(it->second, p.m_defaut);
        if (std::string(p.m_cle) == "neurones") { decid::ValeurJson c = decid::ValeurJson::Tableau(); c.Ajouter((int)std::lround(v)); a["couches_cachees"] = c; }
        else if (p.m_entier) a[p.m_cle] = (int)std::lround(v);
        else a[p.m_cle] = v;
        details += (details.empty() ? "" : ", ") + std::string(p.m_sigle) + " " + NombreFr(v, 4);
    }
    if (!details.empty()) titre += " (" + details + ")";
    r["algorithme"] = a;
    decid::ValeurJson c = decid::ValeurJson::Objet();
    c["attribut"] = g.get_coupe(); c["points"] = 200;
    r["courbe"] = c;
    m_calcul = true;
    g.set_calcul(true);
    const std::string nomCible = g.get_cible() < (int)m_colonnes.size() ? m_colonnes[(size_t)g.get_cible()] : "cible";
    m_moteur.Executer(std::move(r), [this, titre, nomCible](const FinCalcul& fin) {
        m_calcul = false;
        auto& g = m_fenetre.global<::Regression>();
        g.set_calcul(false);
        if (!fin.m_ok) { g.set_erreur(S("Le moteur a refusé la régression : " + fin.m_erreur)); return; }
        auto rj = LireJson(fin.m_resultat / "regression.json");
        auto cj = LireJson(fin.m_resultat / "regression_courbe.json");
        if (!rj.Ok() || !cj.Ok()) { g.set_erreur("Résultats de la régression illisibles."); return; }
        EssaiR e;
        e.m_titre = titre; e.m_tache = fin.m_id; e.m_nomCible = nomCible;
        e.m_mesures = rj.Valeur().Membre("metriques");
        e.m_nomAttribut = cj.Valeur().Membre("nom_attribut").Texte();
        for (const auto& v : cj.Valeur().Membre("x").Elements()) e.m_cx.push_back(v.Nombre());
        for (const auto& v : cj.Valeur().Membre("y").Elements()) e.m_cy.push_back(v.Nombre());
        for (const auto& p : cj.Valeur().Membre("points").Elements()) {
            const bool test = p.Membre("ensemble").Texte() == "test";
            e.m_points.push_back({p.Membre("x").Nombre(), p.Membre("cible").Nombre(), p.Membre("prediction").Nombre(), test});
        }
        e.m_nTest = (int)e.m_mesures.Membre("test").Membre("n").Entier();
        e.m_nApp = (int)e.m_mesures.Membre("apprentissage").Membre("n").Entier();
        m_essais.push_back(std::move(e));
        if (m_essais.size() > 8) m_essais.erase(m_essais.begin());
        m_courant = (int)m_essais.size() - 1;
        Afficher();
    });
}

void Regression::Afficher() {
    auto& g = m_fenetre.global<::Regression>();
    std::vector<Essai> v;
    for (size_t k = 0; k < m_essais.size(); ++k) {
        const auto& t = m_essais[k].m_mesures.Membre("test");
        v.push_back({S(m_essais[k].m_titre), S(t.Contient("r2") ? NombreFr(t.Membre("r2").Nombre(), 3) : "—"), S(t.Contient("eqm") ? NombreFr(t.Membre("eqm").Nombre(), 3) : "—"), (int)k == m_courant});
    }
    g.set_essais(std::make_shared<slint::VectorModel<Essai>>(v));
    g.set_essai_present(m_courant >= 0);
    if (m_courant < 0) return;
    const auto& e = m_essais[(size_t)m_courant];
    const auto& t = e.m_mesures.Membre("test");
    const auto& a = e.m_mesures.Membre("apprentissage");
    g.set_titre_essai(S(e.m_titre));
    g.set_r2(S(t.Contient("r2") ? NombreFr(t.Membre("r2").Nombre(), 3) : "—"));
    g.set_note_mesures(S("1 = prédiction parfaite, 0 = pas mieux que la moyenne. " + std::to_string(e.m_nTest) + " lignes de test, " + std::to_string(e.m_nApp) + " d’apprentissage."));
    std::vector<::Mesure> m = {{"EQM", S(TexteMesure(t, "eqm")), S(TexteMesure(a, "eqm"))}, {"RMSE", S(TexteMesure(t, "rmse")), S(TexteMesure(a, "rmse"))},
                             {"MAE", S(TexteMesure(t, "mae")), S(TexteMesure(a, "mae"))}, {"R²", S(TexteMesure(t, "r2")), S(TexteMesure(a, "r2"))}};
    g.set_mesures(std::make_shared<slint::VectorModel<::Mesure>>(m));
    Peindre();
}

void Regression::Peindre() {
    auto& g = m_fenetre.global<::Regression>();
    if (m_courant < 0 || m_l < 8 || m_h < 8) return;
    const auto& e = m_essais[(size_t)m_courant];
    const bool coupe = g.get_vue() == 0;
    double x0 = 1e300, x1 = -1e300, y0 = 1e300, y1 = -1e300;
    auto etendre = [](double& lo, double& hi, double v) { lo = std::min(lo, v); hi = std::max(hi, v); };
    for (const auto& p : e.m_points) {
        if (coupe) { etendre(x0, x1, p.m_x); etendre(y0, y1, p.m_cible); }
        else { etendre(x0, x1, p.m_cible); etendre(x0, x1, p.m_prediction); }
    }
    if (coupe) { for (double v : e.m_cx) etendre(x0, x1, v); for (double v : e.m_cy) etendre(y0, y1, v); }
    else { y0 = x0; y1 = x1; }
    auto marge = [](double& lo, double& hi) { if (!(hi > lo)) { lo -= 0.5; hi += 0.5; } const double m = (hi - lo) * 0.06; lo -= m; hi += m; };
    marge(x0, x1);
    marge(y0, y1);
    const float s = m_fenetre.window().scale_factor();
    Toile t((int)std::lround(m_l * s), (int)std::lround(m_h * s), m_fenetre.global<Theme>().get_sombre(), {x0, x1, y0, y1});
    if (!coupe) {
        // diagonale y = x en pointilles : prediction parfaite
        const int n = 40;
        for (int k = 0; k < n; k += 2) t.Segment(x0 + (x1 - x0) * k / n, x0 + (x1 - x0) * k / n, x0 + (x1 - x0) * (k + 1) / n, x0 + (x1 - x0) * (k + 1) / n, -1, 1.2f * s, 0.6f);
    }
    for (int passe = 0; passe < 2; ++passe)   // apprentissage (pale) puis test (vif, devant)
        for (const auto& p : e.m_points) {
            if (p.m_test != (passe == 1)) continue;
            t.Point(coupe ? p.m_x : p.m_cible, coupe ? p.m_cible : p.m_prediction, p.m_test ? 0 : 1, 3.0f * s, p.m_test ? 0.9f : 0.6f);
        }
    if (coupe) {
        std::vector<std::pair<double, double>> c;
        for (size_t k = 0; k < e.m_cx.size() && k < e.m_cy.size(); ++k) c.push_back({e.m_cx[k], e.m_cy[k]});
        t.Trajet(c, -1, 2.0f * s, 0.95f);
        g.set_legende(S(e.m_nomAttribut + " → · " + e.m_nomCible + " ↑ · trait : prédiction du moteur, autres entrées fixées à leur moyenne d’apprentissage · points : test (vif) et apprentissage (pâle)"));
    } else {
        g.set_legende(S(e.m_nomCible + " réel → · prédit ↑ · pointillés : prédiction parfaite · points : test (vif) et apprentissage (pâle)"));
    }
    g.set_plan(t.Image());
}

} // namespace studio
