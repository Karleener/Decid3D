#include "Validation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "Fichiers.h"
#include "Graphique.h"

namespace studio {

namespace {
slint::SharedString S(const std::string& s) { return slint::SharedString(s); }

struct Param { const char* m_cle; const char* m_libelle; double m_defaut; bool m_entier; const char* m_nomBalayage; double m_debut, m_fin; int m_parDecade; bool m_balayable; };
struct FamilleVal { const char* m_id; const char* m_court; std::vector<Param> m_params; };
const std::vector<FamilleVal>& Familles() {
    static const std::vector<FamilleVal> f = {
        {"kppv", "k-PPV", {{"k", "k, nombre de voisins", 5, true, "k", 1, 100, 2, true}}},
        {"svm", "SVM", {{"gamma", "γ (noyau gaussien)", 10, false, "γ", 0.01, 1000, 1, true}, {"c", "C, pénalité", 10, false, "C", 0.01, 1000, 1, true}}},
        {"foret_aleatoire", "Forêt", {{"nb_arbres", "Nombre d’arbres", 50, true, "nombre d’arbres", 1, 1000, 1, true},
                                      {"profondeur_max", "Profondeur maximale", 10, true, "profondeur maximale", 1, 32, 3, true}}},
        {"bayes_normal", "Bayes", {}},
        {"boost", "Boosting", {{"nb_faibles", "Nombre de faibles", 50, true, "", 0, 0, 0, false}, {"profondeur", "Profondeur des faibles", 1, true, "", 0, 0, 0, false}}},
        {"em", "EM", {{"k", "K, nombre de clusters", 3, true, "k", 1, 20, 4, true}}},
    };
    return f;
}
double Lire(const std::string& t, double defaut) {
    std::string u = t;
    for (auto& c : u) if (c == ',') c = '.';
    char* fin = nullptr;
    const double v = std::strtod(u.c_str(), &fin);
    return fin && fin != u.c_str() && std::isfinite(v) ? v : defaut;
}
std::string Pct(const decid::ValeurJson& m) {
    if (m.Membre("status").Texte() != "defined") return "non défini";
    char b[32];
    std::snprintf(b, sizeof b, "%.1f %%", m.Membre("value").Nombre() * 100.0);
    std::string s(b);
    for (auto& c : s) if (c == '.') c = ',';
    return s;
}
bool Defini(const decid::ValeurJson& m) { return m.Membre("status").Texte() == "defined"; }
} // namespace

Validation::Validation(Fenetre& fenetre, Moteur& moteur, Etude& etude) : m_fenetre(fenetre), m_moteur(moteur), m_etude(etude) {
    auto& g = m_fenetre.global<::Validation>();
    std::vector<slint::SharedString> jeux;
    for (const auto& n : m_etude.NomsBibliotheque()) jeux.push_back(S(n));
    g.set_jeux(std::make_shared<slint::VectorModel<slint::SharedString>>(jeux));
    std::vector<slint::SharedString> fam;
    for (const auto& f : Familles()) fam.push_back(S(f.m_court));
    g.set_familles(std::make_shared<slint::VectorModel<slint::SharedString>>(fam));

    g.on_choisir_jeu([this](int i) { ChoisirJeu(i); });
    g.on_choisir_famille([this](int i) { ChoisirFamille(i); });
    g.on_changer_parametre([this](int i, slint::SharedString t) {
        const auto& f = Familles()[(size_t)std::clamp(m_fenetre.global<::Validation>().get_famille(), 0, (int)Familles().size() - 1)];
        if (i < 0 || i >= (int)f.m_params.size()) return;
        m_valeurs[std::string(f.m_id) + "." + f.m_params[(size_t)i].m_cle] = std::string(t);
        MajParametres();
    });
    g.on_choisir_balaye([this](int i) { m_fenetre.global<::Validation>().set_balaye(i); MajGrille(); });
    g.on_lancer_validation([this] { Lancer(false); });
    g.on_lancer_balayage([this] { Lancer(true); });
    g.on_ajouter([this] { Ajouter(); });

    // iris par defaut
    int iris = 0;
    const auto noms = m_etude.NomsBibliotheque();
    for (size_t i = 0; i < noms.size(); ++i) if (noms[i] == "Iris") iris = (int)i;
    ChoisirJeu(iris);
    ChoisirFamille(1);
}

void Validation::ChoisirJeu(int i) {
    auto& g = m_fenetre.global<::Validation>();
    g.set_jeu(i);
    g.set_erreur("");
    m_jeu.reset();
    const auto source = m_etude.SourceBibliotheque(i, 0);
    std::vector<slint::SharedString> classes;
    if (source) {
        auto lu = decid::LireCsv(source->m_chemin, source->m_contrat);
        if (lu.Ok()) {
            m_jeu = std::move(lu.Valeur());
            g.set_resume(S(std::to_string(m_jeu->m_nbLignes) + " lignes · " + std::to_string(m_jeu->m_nbAttributs) + " attributs · " +
                           std::to_string(m_jeu->m_nomsClasses.size()) + " classes (lu par le même lecteur que le moteur)"));
            for (const auto& c : m_jeu->m_nomsClasses) classes.push_back(S("classe " + c));
        } else {
            g.set_resume("");
            g.set_erreur(S(lu.Err().Texte()));
        }
    }
    g.set_classes(std::make_shared<slint::VectorModel<slint::SharedString>>(classes));
    g.set_classe_positive(0);
}

void Validation::ChoisirFamille(int i) {
    auto& g = m_fenetre.global<::Validation>();
    g.set_famille(std::clamp(i, 0, (int)Familles().size() - 1));
    const auto& f = Familles()[(size_t)g.get_famille()];
    std::vector<slint::SharedString> b;
    for (const auto& p : f.m_params) if (p.m_balayable) b.push_back(S(p.m_nomBalayage));
    g.set_balayables(std::make_shared<slint::VectorModel<slint::SharedString>>(b));
    g.set_balaye(0);
    MajParametres();
    MajGrille();
}

void Validation::MajParametres() {
    auto& g = m_fenetre.global<::Validation>();
    const auto& f = Familles()[(size_t)g.get_famille()];
    std::vector<ParametreSaisi> v;
    for (const auto& p : f.m_params) {
        const auto it = m_valeurs.find(std::string(f.m_id) + "." + p.m_cle);
        v.push_back({S(p.m_cle), S(p.m_libelle), S(it != m_valeurs.end() ? it->second : NombreFr(p.m_defaut, 6))});
    }
    g.set_parametres(std::make_shared<slint::VectorModel<ParametreSaisi>>(v));
}

void Validation::MajGrille() {
    auto& g = m_fenetre.global<::Validation>();
    const auto& f = Familles()[(size_t)g.get_famille()];
    std::vector<const Param*> b;
    for (const auto& p : f.m_params) if (p.m_balayable) b.push_back(&p);
    if (b.empty()) return;
    const Param& p = *b[(size_t)std::clamp(g.get_balaye(), 0, (int)b.size() - 1)];
    const std::string cle = std::string(f.m_id) + "." + p.m_cle;
    if (!m_grilles.count(cle)) m_grilles[cle] = {NombreFr(p.m_debut, 6), NombreFr(p.m_fin, 6), std::to_string(p.m_parDecade)};
    g.set_debut(S(m_grilles[cle][0]));
    g.set_fin(S(m_grilles[cle][1]));
    g.set_par_decade(S(m_grilles[cle][2]));
}

decid::ValeurJson Validation::Base() const {
    const auto& g = m_fenetre.global<::Validation>();
    const auto& f = Familles()[(size_t)g.get_famille()];
    const auto source = m_etude.SourceBibliotheque(g.get_jeu(), 0);
    decid::ValeurJson r = decid::ValeurJson::Objet();
    r["threads"] = 1;
    decid::ValeurJson a = decid::ValeurJson::Objet();
    a["famille"] = f.m_id;
    const std::string id = f.m_id;
    if (id == "svm") a["noyau"] = "RBF";
    if (id == "foret_aleatoire") a["min_echantillons"] = 2;
    if (id == "boost") a["type"] = "GENTLE";
    if (id == "em") a["covariance"] = "DIAGONALE";
    for (const auto& p : f.m_params) {
        const auto it = m_valeurs.find(id + "." + p.m_cle);
        const double v = it == m_valeurs.end() ? p.m_defaut : Lire(it->second, p.m_defaut);
        if (p.m_entier) a[p.m_cle] = (int)std::lround(v); else a[p.m_cle] = v;
    }
    r["algorithme"] = a;
    decid::ValeurJson s = decid::ValeurJson::Objet();
    s["type"] = "csv";
    s["chemin"] = source ? decid::CheminUtf8(source->m_chemin) : std::string();
    s["en_tete"] = source && source->m_contrat.m_enTete == decid::ModeEnTete::Oui ? "oui" : "non";
    s["separateur"] = ","; s["colonne_cible"] = 0;
    r["source"] = s;
    decid::ValeurJson pa = decid::ValeurJson::Objet();
    pa["methode"] = "aleatoire_stratifie";
    pa["ratio"] = g.get_part() == 0 ? 0.25 : g.get_part() == 2 ? 0.75 : 0.5;
    pa["graine"] = (long long)Lire(std::string(g.get_graine()), 42);
    r["partage"] = pa;
    decid::ValeurJson n = decid::ValeurJson::Objet();
    n["methode"] = "minmax"; n["bas"] = 0; n["haut"] = 1;
    r["normalisation"] = n;
    decid::ValeurJson v = decid::ValeurJson::Objet();
    v["k"] = g.get_k() + 2;
    v["mode"] = g.get_mode() == 0 ? "ordre_fichier" : "melange";
    v["stratifie"] = g.get_stratifie();
    v["graine"] = (long long)Lire(std::string(g.get_graine_plis()), 1);
    v["perimetre"] = g.get_perimetre() == 1 ? "tout" : "apprentissage";
    r["validation"] = v;
    return r;
}

std::string Validation::Protocole() const {
    const auto& g = m_fenetre.global<::Validation>();
    const auto noms = m_etude.NomsBibliotheque();
    const auto& f = Familles()[(size_t)g.get_famille()];
    std::string s = noms[(size_t)std::clamp(g.get_jeu(), 0, (int)noms.size() - 1)] + " · " + f.m_court + " · k = " + std::to_string(g.get_k() + 2) + ", ";
    s += g.get_mode() == 0 ? "ordre du fichier" : "mélangé (graine " + std::string(g.get_graine_plis()) + ")";
    if (g.get_stratifie()) s += ", stratifié";
    s += " · ";
    s += g.get_perimetre() == 1 ? "tout le fichier" : "part d’apprentissage " + std::string(g.get_part() == 0 ? "25" : g.get_part() == 2 ? "75" : "50") + " % (graine " + std::string(g.get_graine()) + ")";
    return s;
}

void Validation::Lancer(bool balayage) {
    auto& g = m_fenetre.global<::Validation>();
    if (m_occupe || !m_jeu) return;
    g.set_erreur("");
    decid::ValeurJson r = Base();
    r["operation"] = balayage ? "balayage" : "validation";
    if (balayage) {
        const auto& f = Familles()[(size_t)g.get_famille()];
        std::vector<const Param*> b;
        for (const auto& p : f.m_params) if (p.m_balayable) b.push_back(&p);
        if (b.empty()) return;
        const Param& p = *b[(size_t)std::clamp(g.get_balaye(), 0, (int)b.size() - 1)];
        const double debut = Lire(std::string(g.get_debut()), 0), fin = Lire(std::string(g.get_fin()), 0);
        const int parDecade = std::clamp((int)Lire(std::string(g.get_par_decade()), 1), 1, 20);
        if (!(debut > 0 && debut <= fin)) { g.set_erreur("Grille invalide : il faut 0 < début ≤ fin."); return; }
        m_grilles[std::string(f.m_id) + "." + p.m_cle] = {std::string(g.get_debut()), std::string(g.get_fin()), std::to_string(parDecade)};
        decid::ValeurJson bal = decid::ValeurJson::Objet();
        bal["parametre"] = p.m_cle;
        decid::ValeurJson lg = decid::ValeurJson::Objet();
        lg["debut"] = debut; lg["fin"] = fin; lg["par_decade"] = parDecade;
        bal["log"] = lg;
        r["balayage"] = bal;
        r["classe_positive"] = g.get_classe_positive();
    }
    m_occupe = true;
    g.set_occupe(balayage ? "balayage" : "validation");
    const std::string protocole = Protocole();
    m_moteur.Executer(std::move(r), [this, balayage, protocole](const FinCalcul& fin) {
        m_occupe = false;
        auto& g = m_fenetre.global<::Validation>();
        g.set_occupe("");
        if (!fin.m_ok) { g.set_erreur(S("Le moteur a refusé : " + fin.m_erreur)); return; }
        if (balayage) { g.set_bal_protocole(S(protocole)); AfficherBalayage(fin.m_resultat); }
        else { m_protocoleValidation = protocole; AfficherValidation(fin.m_resultat, fin.m_id); }
    });
}

void Validation::AfficherValidation(const fs::path& resultat, const std::string& tache) {
    auto& g = m_fenetre.global<::Validation>();
    auto js = LireJson(resultat / "validation.json");
    if (!js.Ok()) { g.set_erreur(S(js.Err().Texte())); return; }
    const auto& j = js.Valeur();
    m_tacheValidation = tache;
    const auto& f = Familles()[(size_t)std::clamp((int)g.get_famille(), 0, (int)Familles().size() - 1)];
    const int k = (int)j.Membre("plan").Membre("k").Entier();
    g.set_val_titre(S(std::string("Validation croisée · ") + f.m_court));
    g.set_val_protocole(S(m_protocoleValidation));
    std::vector<Pli> plis;
    int n = 0;
    for (const auto& p : j.Membre("par_pli").Elements()) {
        const auto& ex = p.Membre("agregats").Membre("exactitude");
        plis.push_back({++n, (int)p.Membre("confusion").Membre("total").Entier(), S(Pct(ex)), Defini(ex) ? (float)ex.Membre("value").Nombre() : 0.0f});
    }
    g.set_plis(std::make_shared<slint::VectorModel<Pli>>(plis));
    const auto& moy = j.Membre("moyenne_par_pli");
    const auto& ec = j.Membre("ecart_type_par_pli");
    const auto& hp = j.Membre("hors_pli");
    std::vector<Fait> faits = {
        {S("Moyenne des " + std::to_string(k) + " plis"), S(Pct(moy))},
        {"Écart-type entre plis", S(Defini(ec) ? NombreFr(ec.Membre("value").Nombre() * 100, 3) + " points" : "non défini")},
        {"Exactitude hors pli", S(Pct(hp.Membre("agregats").Membre("exactitude")))},
        {"F1 macro hors pli", S(Pct(hp.Membre("agregats").Membre("f1_macro")))}};
    g.set_val_faits(std::make_shared<slint::VectorModel<Fait>>(faits));
    const long long total = hp.Membre("confusion").Membre("total").Entier();
    g.set_val_note(S("Hors pli : chaque point est prédit une seule fois, par le modèle qui ne l’a pas vu, puis toutes ces prédictions sont comptées ensemble (" +
                     std::to_string(total) + " points). La moyenne des plis fait la moyenne des " + std::to_string(k) + " exactitudes."));
    std::vector<slint::SharedString> cl;
    for (const auto& c : hp.Membre("confusion").Membre("classes").Elements()) cl.push_back(S(c.Texte()));
    g.set_conf_classes(std::make_shared<slint::VectorModel<slint::SharedString>>(cl));
    std::vector<LigneConfusion> lignes;
    size_t r = 0;
    for (const auto& l : hp.Membre("confusion").Membre("comptes").Elements()) {
        std::vector<int> c;
        int t = 0;
        for (const auto& v : l.Elements()) { c.push_back((int)v.Entier()); t += (int)v.Entier(); }
        lignes.push_back({r < cl.size() ? cl[r] : S(std::to_string(r)), std::make_shared<slint::VectorModel<int>>(c), t});
        ++r;
    }
    g.set_confusion(std::make_shared<slint::VectorModel<LigneConfusion>>(lignes));
    m_scoreValidation = Defini(moy) ? moy.Membre("value").Nombre() : Defini(hp.Membre("agregats").Membre("exactitude")) ? hp.Membre("agregats").Membre("exactitude").Membre("value").Nombre() : -1;
    g.set_ajoutee(m_etude.ContientTache(tache));
    g.set_val_presente(true);
}

void Validation::AfficherBalayage(const fs::path& resultat) {
    auto& g = m_fenetre.global<::Validation>();
    auto jb = LireJson(resultat / "balayage.json");
    auto jr = LireJson(resultat / "points_fnr_fpr.json");
    if (!jb.Ok()) { g.set_erreur(S(jb.Err().Texte())); return; }
    const auto& b = jb.Valeur();
    const auto& f = Familles()[(size_t)g.get_famille()];
    std::string nom = b.Membre("parametre").Texte();
    for (const auto& p : f.m_params) if (nom == p.m_cle) nom = p.m_nomBalayage;
    g.set_bal_titre(S("Balayage de " + nom + " · " + f.m_court));
    const auto& cand = b.Membre("candidats").Elements();
    const int im = (int)b.Membre("indice_meilleur").Entier();
    const bool bord = b.Membre("meilleur_en_bord").Booleen();
    g.set_bal_protocole(S(std::string(g.get_bal_protocole()) + " · " + std::to_string(cand.size()) + " valeurs de " +
                          (cand.empty() ? std::string("—") : NombreFr(cand.front().Membre("valeur").Nombre(), 3)) + " à " +
                          (cand.empty() ? std::string("—") : NombreFr(cand.back().Membre("valeur").Nombre(), 3))));
    if (im >= 0 && im < (int)cand.size())
        g.set_meilleur(S(nom + " = " + NombreFr(cand[(size_t)im].Membre("valeur").Nombre(), 4) + " · " +
                         Pct(cand[(size_t)im].Membre("validation").Membre("moyenne_par_pli")) + " en moyenne par pli"));
    g.set_critere(S(b.Membre("critere").Texte()));
    g.set_en_bord(bord);
    const bool petit = im == 0;
    g.set_en_bord_texte(S(std::string("En bord de grille : le meilleur réglage est la ") + (petit ? "plus petite" : "plus grande") +
                          " valeur essayée. L’optimum est peut-être au-delà ; élargissez la grille vers les " + (petit ? "petites" : "grandes") + " valeurs."));

    // ---- courbe d'exactitude (repere 640 x 300, marges 52 / 18 / 16 / 40)
    const float L = 640, H = 300, mg = 52, md = 18, mh = 16, mb = 40;
    Graphique gr(L, H);
    double lo = 1e300, hi = -1e300;
    for (const auto& c : cand) { const double v = std::log10(std::max(1e-12, c.Membre("valeur").Nombre())); lo = std::min(lo, v); hi = std::max(hi, v); }
    if (!(hi > lo)) { lo -= 0.5; hi += 0.5; }
    auto px = [&](double v) { return mg + (float)((std::log10(std::max(1e-12, v)) - lo) / (hi - lo)) * (L - mg - md); };
    auto py = [&](double a) { return mh + (float)(1.0 - std::clamp(a, 0.0, 1.0)) * (H - mh - mb); };
    for (int t = 0; t <= 4; ++t) {
        const float y = py(t / 4.0);
        gr.Segment(mg, y, L - md, y, 3, 1);
        gr.Texte(mg - 6, y, std::to_string(t * 25) + " %", 2);
    }
    for (int e = (int)std::ceil(lo - 1e-9); e <= (int)std::floor(hi + 1e-9); ++e) {
        const float x = px(std::pow(10.0, e));
        gr.Segment(x, H - mb, x, H - mb + 5, 3, 1);
        const double v = std::pow(10.0, e);
        gr.Texte(x, H - mb + 16, v >= 100000 || v < 0.001 ? "1e" + std::to_string(e) : NombreFr(v, 3), 1);
    }
    gr.Texte(mg + (L - mg - md) / 2, H - 6, nom, 1);
    std::vector<std::pair<float, float>> haut, bas, moyenne, hors;
    for (const auto& c : cand) {
        const double v = c.Membre("valeur").Nombre();
        const auto& m = c.Membre("validation").Membre("moyenne_par_pli");
        const auto& e = c.Membre("validation").Membre("ecart_type_par_pli");
        const auto& h = c.Membre("validation").Membre("hors_pli").Membre("agregats").Membre("exactitude");
        if (Defini(m)) {
            const double a = m.Membre("value").Nombre(), s = Defini(e) ? e.Membre("value").Nombre() : 0;
            moyenne.push_back({px(v), py(a)});
            haut.push_back({px(v), py(a + s)});
            bas.push_back({px(v), py(a - s)});
        }
        if (Defini(h)) hors.push_back({px(v), py(h.Membre("value").Nombre())});
    }
    gr.Bande(haut, bas);
    gr.Pointilles(hors, 5, 4, 1, 1.6f);
    gr.Ligne(moyenne, 0, 2.2f);
    for (const auto& p : moyenne) gr.Point(p.first, p.second, 3.5f, 4);
    if (im >= 0 && im < (int)cand.size() && Defini(cand[(size_t)im].Membre("validation").Membre("moyenne_par_pli"))) {
        const float x = px(cand[(size_t)im].Membre("valeur").Nombre()), y = py(cand[(size_t)im].Membre("validation").Membre("moyenne_par_pli").Membre("value").Nombre());
        gr.Point(x, y, 8, 5, 2);
        gr.Texte(x, y < mh + 30 ? y + 24 : y - 16, bord ? "meilleur · en bord de grille" : "meilleur", x < 120 ? 0 : x > L - 120 ? 2 : 1, true);
    }
    g.set_courbe(std::make_shared<slint::VectorModel<Trace>>(gr.Traces()));
    g.set_courbe_textes(std::make_shared<slint::VectorModel<Etiquette>>(gr.Etiquettes()));

    std::vector<LigneBalayage> t;
    int k = 0;
    for (const auto& c : cand) {
        const auto& e = c.Membre("validation").Membre("ecart_type_par_pli");
        t.push_back({S(NombreFr(c.Membre("valeur").Nombre(), 4)), S(Pct(c.Membre("validation").Membre("moyenne_par_pli"))),
                     S(Defini(e) ? NombreFr(e.Membre("value").Nombre() * 100, 3) + " pts" : "—"),
                     S(Pct(c.Membre("validation").Membre("hors_pli").Membre("agregats").Membre("exactitude"))), k == im});
        ++k;
    }
    g.set_tableau(std::make_shared<slint::VectorModel<LigneBalayage>>(t));

    // ---- graphe FNR / (100 - FPR) : un point par modele, jamais relies
    if (jr.Ok()) {
        const auto& r = jr.Valeur();
        g.set_roc_texte(S("Chaque point est un modèle entier, une valeur de " + nom + ", mesuré hors pli pour la classe positive « " +
                          r.Membre("nom_classe_positive").Texte() + " ». Abscisse : taux de faux négatifs (FNR). Ordonnée : 100 − taux de faux positifs (FPR). Les points ne sont pas reliés : il n’y a ni seuil qui varie, ni aire sous la courbe."));
        g.set_roc_note(S("Moteur : " + r.Membre("avertissement").Texte() + ". Le coin en haut à gauche (0 % de faux négatifs, 100 % de vrais négatifs) est le modèle parfait."));
        const float cote = 280, g0 = 52, h0 = 34;
        Graphique gr2(cote + 52 + 20, cote + 34 + 40);
        auto rx = [&](double v) { return g0 + (float)(std::clamp(v, 0.0, 100.0) / 100.0) * cote; };
        auto ry = [&](double v) { return h0 + (float)(1.0 - std::clamp(v, 0.0, 100.0) / 100.0) * cote; };
        for (int t2 = 0; t2 <= 4; ++t2) {
            gr2.Segment(g0, ry(t2 * 25), g0 + cote, ry(t2 * 25), 3, 1);
            gr2.Segment(rx(t2 * 25), h0, rx(t2 * 25), h0 + cote, 3, 1);
            gr2.Texte(g0 - 6, ry(t2 * 25), std::to_string(t2 * 25), 2);
            gr2.Texte(rx(t2 * 25), h0 + cote + 14, std::to_string(t2 * 25), 1);
        }
        gr2.Texte(g0 + cote / 2, h0 + cote + 32, "FNR (%)", 1);
        gr2.Texte(g0 - 40, 10, "100 − FPR (%)", 0);
        // etiquettes : les points confondus partagent une etiquette ; anti-chevauchement simple
        struct Et { float x, y; std::string t; };
        std::vector<Et> ets;
        int ip = 0;
        for (const auto& p : r.Membre("points").Elements()) {
            const double fx = p.Membre("fnr_pourcent").Nombre(), fy = p.Membre("cent_moins_fpr_pourcent").Nombre();
            const float x = rx(fx), y = ry(fy);
            gr2.Point(x, y, ip == im ? 6.0f : 4.5f, ip == im ? 5 : 4, 2);
            bool fusion = false;
            for (auto& e : ets) if (std::abs(e.x - x) < 0.01f && std::abs(e.y - y) < 0.01f) { e.t += " · " + NombreFr(p.Membre("valeur_parametre").Nombre(), 3); fusion = true; }
            if (!fusion) ets.push_back({x, y, nom + " = " + NombreFr(p.Membre("valeur_parametre").Nombre(), 3)});
            ++ip;
        }
        std::sort(ets.begin(), ets.end(), [](const Et& a, const Et& c) { return a.y < c.y; });
        for (size_t i = 0; i < ets.size(); ++i) {
            float y = ets[i].y + (ets[i].y > ry(92) ? 20.0f : 0.0f);
            for (size_t j = 0; j < i; ++j) if (std::abs(ets[j].x - ets[i].x) < 140 && std::abs(ets[j].y - y) < 14) y = ets[j].y + 14;
            ets[i].y = y;
            const bool gauche = ets[i].x > g0 + cote - 130;
            gr2.Texte(ets[i].x + (gauche ? -10.0f : 10.0f), y, ets[i].t, gauche ? 2 : 0);
        }
        g.set_roc(std::make_shared<slint::VectorModel<Trace>>(gr2.Traces()));
        g.set_roc_textes(std::make_shared<slint::VectorModel<Etiquette>>(gr2.Etiquettes()));
    }
    g.set_bal_presente(true);
}

void Validation::Ajouter() {
    auto& g = m_fenetre.global<::Validation>();
    if (m_tacheValidation.empty() || m_etude.ContientTache(m_tacheValidation)) return;
    const auto& f = Familles()[(size_t)g.get_famille()];
    const auto noms = m_etude.NomsBibliotheque();
    const int jeu = std::clamp(g.get_jeu(), 0, (int)noms.size() - 1);
    const int k = g.get_k() + 2;
    const std::string mode = g.get_mode() == 0 ? "ordre_fichier" : "melange";
    m_etude.AjouterExperienceModule("validation", m_tacheValidation, std::string(f.m_court) + " · validation croisée k = " + std::to_string(k), noms[(size_t)jeu],
                                    "validation croisée k = " + std::to_string(k) + (g.get_mode() == 0 ? ", ordre du fichier" : ", mélangé"),
                                    m_scoreValidation, m_etude.CleBibliotheque(jeu), "validation-k" + std::to_string(k) + "-" + mode);
    g.set_ajoutee(true);
}

void Validation::MontrerTache(const std::string& tache) {
    m_protocoleValidation = "Expérience rouverte depuis les Expériences.";
    AfficherValidation(m_moteur.DossierResultat(tache), tache);
}

} // namespace studio
