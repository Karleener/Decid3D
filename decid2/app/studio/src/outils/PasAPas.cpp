#include "PasAPas.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "Fichiers.h"
#include "Graphique.h"
#include "Plateforme.h"

namespace studio {

namespace {
slint::SharedString S(const std::string& s) { return slint::SharedString(s); }
enum { ADABOOST = 0, PERCEPTRON = 1, KMEANS = 2, EM = 3, RESEAU = 4 };
const char* NOMS[] = {"AdaBoost", "Perceptron", "k-moyennes", "EM", "Réseau"};

struct Param { const char* m_cle; const char* m_libelle; double m_defaut; };
const std::vector<Param>& Params(int algo) {
    static const std::vector<std::vector<Param>> p = {
        {{"iterations", "Itérations maximales", 30}},
        {{"eta", "η, pas d’apprentissage", 0.1}, {"epoques_max", "Époques maximales", 10}},
        {{"k", "k, centres", 3}, {"iter_max", "Itérations maximales", 50}},
        {{"k", "K, gaussiennes", 3}, {"iterations_max", "Itérations maximales", 50}},
        {{"neurones", "Neurones cachés", 8}, {"eta", "η, pas d’apprentissage", 0.1}, {"iterations_max", "Époques", 150}},
    };
    return p[(size_t)std::clamp(algo, 0, 4)];
}
double Lire(const std::string& t, double defaut) {
    std::string u = t;
    for (auto& c : u) if (c == ',') c = '.';
    char* fin = nullptr;
    const double v = std::strtod(u.c_str(), &fin);
    return fin && fin != u.c_str() && std::isfinite(v) ? v : defaut;
}
std::string Pct(double v, int dec = 1) {
    char b[32];
    std::snprintf(b, sizeof b, "%.*f %%", dec, v * 100.0);
    std::string s(b);
    for (auto& c : s) if (c == '.') c = ',';
    return s;
}
double G(const std::map<std::string, double>& g, const char* c, double d = NAN) {
    const auto it = g.find(c);
    return it == g.end() ? d : it->second;
}
} // namespace

PasAPas::PasAPas(Fenetre& fenetre, Moteur& moteur, Etude& etude) : m_fenetre(fenetre), m_moteur(moteur), m_etude(etude) {
    auto& g = m_fenetre.global<::PasAPas>();
    std::vector<slint::SharedString> jeux;
    for (const auto& n : m_etude.NomsBibliotheque()) jeux.push_back(S(n));
    g.set_jeux(std::make_shared<slint::VectorModel<slint::SharedString>>(jeux));
    g.on_choisir_algorithme([this](int i) { ChoisirAlgorithme(i); });
    g.on_choisir_jeu([this](int i) { ChoisirJeu(i); });
    g.on_changer_parametre([this](int i, slint::SharedString t) {
        const int a = m_fenetre.global<::PasAPas>().get_algorithme();
        if (i < 0 || i >= (int)Params(a).size()) return;
        m_valeurs[std::to_string(a) + "." + Params(a)[(size_t)i].m_cle] = std::string(t);
        MajParametres();
    });
    g.on_axes_changes([this] { m_trace.reset(); m_fenetre.global<::PasAPas>().set_trace_presente(false); Afficher(); });
    g.on_calculer([this] { Calculer(); });
    g.on_une_etape([this] { UneEtape(); });
    g.on_reinitialiser([this] { Reinitialiser(); });
    g.on_lire([this] {
        auto& g = m_fenetre.global<::PasAPas>();
        if (!m_trace) return;
        if (m_lecture.running()) { m_lecture.stop(); g.set_lecture(false); return; }
        if (g.get_etape() >= (int)m_trace->m_etats.size() - 1) g.set_etape(0);
        g.set_lecture(true);
        const int v = g.get_vitesse();
        m_lecture.start(slint::TimerMode::Repeated, std::chrono::milliseconds(v == 0 ? 700 : v == 2 ? 70 : 280), [this] {
            auto& g = m_fenetre.global<::PasAPas>();
            if (!m_trace || g.get_etape() >= (int)m_trace->m_etats.size() - 1) { m_lecture.stop(); g.set_lecture(false); return; }
            g.set_etape(g.get_etape() + 1);
            Afficher();
        });
        Afficher();
    });
    g.on_aller_etape([this](int k) {
        m_lecture.stop();
        m_fenetre.global<::PasAPas>().set_lecture(false);
        m_fenetre.global<::PasAPas>().set_etape(k);
        Afficher();
    });
    g.on_ajouter([this] { Ajouter(); });
    g.on_exporter_em([this] { ExporterEm(); });
    g.on_taille([this](float l, float h) { if (std::abs(l - m_l) < 1 && std::abs(h - m_h) < 1) return; const bool premier = m_l < 1; m_l = l; m_h = h; if (premier) Peindre(); else m_redim.start(slint::TimerMode::SingleShot, std::chrono::milliseconds(80), [this] { Peindre(); }); });

    int deuxD = 0;
    const auto noms = m_etude.NomsBibliotheque();
    for (size_t i = 0; i < noms.size(); ++i) if (noms[i] == "Trois classes en 2D") deuxD = (int)i;
    ChoisirJeu(deuxD);
    ChoisirAlgorithme(0);
}

void PasAPas::ChoisirAlgorithme(int i) {
    auto& g = m_fenetre.global<::PasAPas>();
    g.set_algorithme(std::clamp(i, 0, 4));
    m_lecture.stop();
    g.set_lecture(false);
    m_trace.reset();
    g.set_trace_presente(false);
    g.set_note_calcul(S(std::string("Le moteur apprend sur les deux attributs affichés, ") +
                        (i == RESEAU ? "normalisés entre 0 et 1 (régions redessinées dans les unités du fichier)" : "sans normalisation : droites, centres et variances sont dans les unités du fichier") +
                        ". Points d’apprentissage tirés au hasard dans chaque classe."));
    MajParametres();
    Afficher();
}

void PasAPas::ChoisirJeu(int i) {
    auto& g = m_fenetre.global<::PasAPas>();
    g.set_jeu(i);
    g.set_erreur("");
    m_trace.reset();
    g.set_trace_presente(false);
    m_jeu.reset();
    std::vector<slint::SharedString> attrs, classes;
    if (const auto s = m_etude.SourceBibliotheque(i, 0)) {
        auto lu = decid::LireCsv(s->m_chemin, s->m_contrat);
        if (lu.Ok()) {
            m_jeu = std::move(lu.Valeur());
            for (int a = 0; a < m_jeu->m_nbAttributs; ++a) attrs.push_back(S("attribut " + std::to_string(a + 1)));
            for (const auto& c : m_jeu->m_nomsClasses) classes.push_back(S("classe " + c));
        } else g.set_erreur(S(lu.Err().Texte()));
    }
    g.set_attributs(std::make_shared<slint::VectorModel<slint::SharedString>>(attrs));
    g.set_classes(std::make_shared<slint::VectorModel<slint::SharedString>>(classes));
    g.set_axe_x(0);
    g.set_axe_y(m_jeu && m_jeu->m_nbAttributs >= 2 ? 1 : 0);
    g.set_classe_positive(0);
    if (m_jeu && m_jeu->m_nbAttributs < 2) g.set_erreur("Ce jeu n’a qu’un attribut : il en faut deux pour le plan.");
    Afficher();
}

void PasAPas::MajParametres() {
    auto& g = m_fenetre.global<::PasAPas>();
    const int a = g.get_algorithme();
    std::vector<ParametreSaisi> v;
    for (const auto& p : Params(a)) {
        const auto it = m_valeurs.find(std::to_string(a) + "." + p.m_cle);
        v.push_back({S(p.m_cle), S(p.m_libelle), S(it != m_valeurs.end() ? it->second : NombreFr(p.m_defaut, 6))});
    }
    g.set_parametres(std::make_shared<slint::VectorModel<ParametreSaisi>>(v));
}

double PasAPas::Param(const std::string& cle, double defaut) const {
    const int a = m_fenetre.global<::PasAPas>().get_algorithme();
    const auto it = m_valeurs.find(std::to_string(a) + "." + cle);
    return it == m_valeurs.end() ? defaut : Lire(it->second, defaut);
}

// ---------------------------------------------------------------- calcul
void PasAPas::Calculer() {
    auto& g = m_fenetre.global<::PasAPas>();
    if (m_calcul || !m_jeu || m_jeu->m_nbAttributs < 2) return;
    const int algo = g.get_algorithme(), ax = g.get_axe_x(), ay = g.get_axe_y();
    if (ax == ay) { g.set_erreur("Choisissez deux attributs différents."); return; }
    g.set_erreur("");
    const auto source = m_etude.SourceBibliotheque(g.get_jeu(), 0);
    if (!source) return;
    // le moteur apprend sur les deux attributs du plan : fichier d'origine s'il n'en a que deux, sinon un CSV a 3 colonnes
    fs::path chemin = source->m_chemin;
    std::string enTete = source->m_contrat.m_enTete == decid::ModeEnTete::Oui ? "oui" : "non";
    if (!(m_jeu->m_nbAttributs == 2 && ax == 0 && ay == 1)) {
        std::string csv;
        char b[96];
        for (int l = 0; l < m_jeu->m_nbLignes; ++l) {
            std::snprintf(b, sizeof b, ",%.17g,%.17g\n", m_jeu->m_valeurs[(size_t)l * m_jeu->m_nbAttributs + ax], m_jeu->m_valeurs[(size_t)l * m_jeu->m_nbAttributs + ay]);
            csv += m_jeu->m_nomsClasses[(size_t)m_jeu->m_labels[l]] + b;
        }
        auto f = m_moteur.EcrireDonnees(csv, "csv");
        if (!f.Ok()) { g.set_erreur(S(f.Err().Texte())); return; }
        chemin = f.Valeur();
        enTete = "non";
    }
    const double ratio = g.get_part() == 0 ? 0.25 : g.get_part() == 2 ? 0.75 : 0.5;
    const long long graine = (long long)Lire(std::string(g.get_graine()), 1);
    decid::ValeurJson r = decid::ValeurJson::Objet();
    const char* ops[] = {"adaboost_pas_a_pas", "perceptron", "kmeans", "em_pas_a_pas", "mlp_pas_a_pas"};
    r["operation"] = ops[algo];
    r["threads"] = 1;
    decid::ValeurJson s = decid::ValeurJson::Objet();
    s["type"] = "csv"; s["chemin"] = decid::CheminUtf8(chemin); s["en_tete"] = enTete; s["separateur"] = ","; s["colonne_cible"] = 0;
    r["source"] = s;
    decid::ValeurJson pa = decid::ValeurJson::Objet();
    pa["methode"] = "aleatoire_stratifie"; pa["ratio"] = ratio; pa["graine"] = graine;
    r["partage"] = pa;
    decid::ValeurJson n = decid::ValeurJson::Objet();
    if (algo == RESEAU) { n["methode"] = "minmax"; n["bas"] = 0; n["haut"] = 1; } else n["methode"] = "aucune";
    r["normalisation"] = n;
    decid::ValeurJson tr = decid::ValeurJson::Objet();
    tr["max_etats"] = 2000; tr["max_octets"] = 67108864;
    r["trace"] = tr;
    const int cp = g.get_classe_positive();
    std::string reglage;
    if (algo == ADABOOST) {
        decid::ValeurJson o = decid::ValeurJson::Objet();
        o["faible"] = g.get_faible() == 1 ? "hyperplan" : "seuil";
        o["iterations"] = (int)Param("iterations", 30);
        o["classe_positive"] = cp;
        r["adaboost"] = o;
        reglage = std::string("faible = ") + (g.get_faible() == 1 ? "hyperplan" : "seuil") + ", iterations = " + std::to_string((int)Param("iterations", 30));
    } else if (algo == PERCEPTRON) {
        decid::ValeurJson o = decid::ValeurJson::Objet();
        o["eta"] = Param("eta", 0.1); o["epoques_max"] = (int)Param("epoques_max", 10); o["melanger"] = g.get_melanger();
        o["graine"] = graine; o["classe_positive"] = cp;
        r["perceptron"] = o;
        reglage = "eta = " + NombreFr(Param("eta", 0.1), 4) + ", epoques_max = " + std::to_string((int)Param("epoques_max", 10));
    } else if (algo == KMEANS) {
        decid::ValeurJson o = decid::ValeurJson::Objet();
        o["k"] = (int)Param("k", 3); o["init"] = g.get_init_kmeans() == 1 ? "aleatoire" : "kmeans++"; o["graine"] = graine; o["iter_max"] = (int)Param("iter_max", 50);
        r["kmeans"] = o;
        reglage = "k = " + std::to_string((int)Param("k", 3));
    } else if (algo == EM) {
        decid::ValeurJson a = decid::ValeurJson::Objet();
        a["famille"] = "em"; a["k"] = (int)Param("k", 3); a["covariance"] = "DIAGONALE"; a["iterations_max"] = (int)Param("iterations_max", 50); a["epsilon"] = 1e-6;
        r["algorithme"] = a;
        reglage = "K = " + std::to_string((int)Param("k", 3));
    } else {
        decid::ValeurJson a = decid::ValeurJson::Objet();
        decid::ValeurJson c = decid::ValeurJson::Tableau(); c.Ajouter((int)Param("neurones", 8));
        a["famille"] = "mlp"; a["couches_cachees"] = c; a["activation"] = "SIGMOIDE_SYM"; a["methode"] = "RETROPROPAGATION";
        a["eta"] = Param("eta", 0.1); a["moment"] = 0.1; a["iterations_max"] = (int)Param("iterations_max", 150); a["epsilon"] = 0;
        r["algorithme"] = a;
        r["suivi_validation"] = "test";
        reglage = "neurones = " + std::to_string((int)Param("neurones", 8)) + ", eta = " + NombreFr(Param("eta", 0.1), 4) + ", epoques = " + std::to_string((int)Param("iterations_max", 150));
    }
    m_calcul = true;
    g.set_calcul(true);
    m_lecture.stop();
    g.set_lecture(false);
    const auto noms = m_etude.NomsBibliotheque();
    const std::string jeuNom = noms[(size_t)std::clamp(g.get_jeu(), 0, (int)noms.size() - 1)];
    m_moteur.Executer(std::move(r), [this, algo, ax, ay, cp, ratio, graine, jeuNom, reglage](const FinCalcul& fin) {
        m_calcul = false;
        auto& g = m_fenetre.global<::PasAPas>();
        g.set_calcul(false);
        if (!fin.m_ok) { g.set_erreur(S("Le moteur a refusé : " + fin.m_erreur)); return; }
        auto t = std::make_shared<Trace>();
        t->m_id = fin.m_id; t->m_algo = algo; t->m_axeX = ax; t->m_axeY = ay; t->m_classePositive = cp;
        t->m_ratio = ratio; t->m_graine = graine; t->m_jeuNom = jeuNom; t->m_reglage = reglage;
        auto tj = LireJson(fin.m_resultat / "trace.json");
        auto fj = LireJson(fin.m_resultat / "etat_final.json");
        auto pj = LireJson(fin.m_resultat / "partage.json");
        if (!tj.Ok() || !fj.Ok() || !pj.Ok()) { g.set_erreur("Trace du moteur illisible."); return; }
        const auto f32 = LireBinaire<float>(fin.m_resultat / "trace.f32");
        const auto i32 = LireBinaire<std::int32_t>(fin.m_resultat / "trace_affectations.i32");
        t->m_final = fj.Valeur();
        for (const auto& i : pj.Valeur().Membre("apprentissage").Elements()) t->m_apprentissage.push_back((int)i.Entier());
        t->m_pas = (long)tj.Valeur().Membre("pas").Entier();
        t->m_recus = (long)tj.Valeur().Membre("recus").Entier();
        if (algo == ADABOOST || algo == PERCEPTRON) { Etat d; d.m_phase = "depart"; d.m_depart = true; t->m_etats.push_back(d); }
        for (const auto& e : tj.Valeur().Membre("etats").Elements()) {
            Etat x;
            x.m_phase = e.Membre("phase").Texte();
            x.m_iteration = (long)e.Membre("iteration").Entier();
            for (const auto& [k, v] : e.Membre("grandeurs").Membres()) x.m_g[k] = v.Nombre();
            const size_t dec = (size_t)e.Membre("decalage").Entier(), tai = (size_t)e.Membre("taille").Entier();
            if (dec + tai <= f32.size()) x.m_v.assign(f32.begin() + (long long)dec, f32.begin() + (long long)(dec + tai));
            if (e.Contient("taille_affectations")) {
                const size_t da = (size_t)e.Membre("decalage_affectations").Entier(), ta = (size_t)e.Membre("taille_affectations").Entier();
                if (da + ta <= i32.size()) x.m_aff.assign(i32.begin() + (long long)da, i32.begin() + (long long)(da + ta));
            }
            t->m_etats.push_back(std::move(x));
        }
        m_trace = t;
        m_traces[t->m_id] = t;
        m_regions.clear();
        g.set_etape(0);
        g.set_nb_etapes((int)t->m_etats.size());
        g.set_trace_presente(true);
        g.set_ajoute(m_etude.ContientTache(t->m_id));
        g.set_note_trace(S(t->m_pas > 1 ? "Trace bornée par le moteur : un état sur " + std::to_string(t->m_pas) + " est conservé (" + std::to_string(t->m_recus) +
                                              " reçus), le premier et le dernier toujours." : ""));
        Afficher();
    });
}

void PasAPas::UneEtape() {
    auto& g = m_fenetre.global<::PasAPas>();
    if (!m_trace) { Calculer(); return; }
    m_lecture.stop();
    g.set_lecture(false);
    g.set_etape(std::min(g.get_etape() + 1, (int)m_trace->m_etats.size() - 1));
    Afficher();
}
void PasAPas::Reinitialiser() {
    auto& g = m_fenetre.global<::PasAPas>();
    if (!m_trace) return;
    m_lecture.stop();
    g.set_lecture(false);
    g.set_etape(0);
    Afficher();
}

// ---------------------------------------------------------------- affichage
void PasAPas::Afficher() {
    auto& g = m_fenetre.global<::PasAPas>();
    if (!m_trace) {
        g.set_legende("Choisissez un algorithme, puis « Calculer la trace ».");
        Peindre();
        return;
    }
    TexteEtape();
    Courbe();
    Peindre();
    std::string population;
    const auto [texte, valeur] = ScoreFinal(population);
    g.set_score_titre(S(texte));
    g.set_score(S(valeur >= 0 ? Pct(valeur) : "—"));
    g.set_export_em(m_trace->m_algo == EM && !m_trace->m_final.Membre("clusters_toutes_lignes").Elements().empty());
}

std::pair<std::string, double> PasAPas::ScoreFinal(std::string& population) const {
    const auto& t = *m_trace;
    const auto& d = t.m_etats.back().m_g;
    if (t.m_algo == ADABOOST) { population = "apprentissage"; return {"Score apprentissage", 1.0 - G(d, "erreur_apprentissage", 1)}; }
    if (t.m_algo == PERCEPTRON) {
        population = "apprentissage";
        for (size_t k = t.m_etats.size(); k-- > 0;)
            if (t.m_etats[k].m_phase == "epoque") return {"Score apprentissage", 1.0 - G(t.m_etats[k].m_g, "erreurs", 0) / std::max<size_t>(1, t.m_apprentissage.size())};
        return {"Score apprentissage", -1};
    }
    if (t.m_algo == EM) { population = "test"; return {"Score test", t.m_final.Membre("exactitude_test").Membre("valeur").Nombre()}; }
    if (t.m_algo == RESEAU) {
        if (!std::isnan(G(d, "exactitude_validation"))) { population = "test"; return {"Score test", G(d, "exactitude_validation")}; }
        population = "apprentissage";
        return {"Score apprentissage", G(d, "exactitude_apprentissage", -1)};
    }
    population = "aucun (non supervisé)";
    return {"Score : aucun (non supervisé)", -1};
}

void PasAPas::Courbe() {
    auto& g = m_fenetre.global<::PasAPas>();
    const auto& t = *m_trace;
    const char* titres[] = {"Erreur du vote des faibles (apprentissage)", "Erreurs par époque", "Critère J", "Log-vraisemblance", "Perte d’apprentissage"};
    const char* cles[] = {"erreur_apprentissage", "erreurs", "J", "log_vraisemblance", "perte_apprentissage"};
    g.set_courbe_titre(titres[t.m_algo]);
    std::vector<std::pair<int, double>> serie;
    for (size_t k = 0; k < t.m_etats.size(); ++k) {
        if (t.m_algo == PERCEPTRON && t.m_etats[k].m_phase != "epoque") continue;
        const double v = G(t.m_etats[k].m_g, cles[t.m_algo]);
        if (!std::isnan(v)) serie.push_back({(int)k, t.m_algo == ADABOOST ? v * 100 : v});
    }
    Graphique gr(640, 120);
    const int cur = g.get_etape(), n = std::max<int>(1, (int)t.m_etats.size() - 1);
    if (!serie.empty()) {
        double lo = serie[0].second, hi = lo;
        for (const auto& [k, v] : serie) { lo = std::min(lo, v); hi = std::max(hi, v); }
        if (!(hi > lo)) { hi += 1; if (t.m_algo != ADABOOST) lo -= 1; }
        auto px = [&](int k) { return 60.0f + (float)k / n * 560.0f; };
        auto py = [&](double v) { return 10.0f + (float)((hi - v) / (hi - lo)) * 100.0f; };
        std::vector<std::pair<float, float>> tout, avant;
        for (const auto& [k, v] : serie) { tout.push_back({px(k), py(v)}); if (k <= cur) avant.push_back({px(k), py(v)}); }
        gr.Ligne(tout, 1, 1);
        gr.Ligne(avant, 0, 2);
        if (!avant.empty()) gr.Point(avant.back().first, avant.back().second, 4, 4);
        const std::string u = t.m_algo == ADABOOST ? " %" : "";
        gr.Texte(4, 12, "max " + NombreFr(hi, 4) + u, 0);
        gr.Texte(4, 108, "min " + NombreFr(lo, 4) + u, 0);
        if (!avant.empty()) {
            double vc = 0;
            for (const auto& [k, v] : serie) if (k <= cur) vc = v;
            gr.Texte(636, 12, NombreFr(vc, 4) + u, 2, true);
        }
    }
    g.set_courbe(std::make_shared<slint::VectorModel<::Trace>>(gr.Traces()));
    g.set_courbe_textes(std::make_shared<slint::VectorModel<Etiquette>>(gr.Etiquettes()));
}

void PasAPas::TexteEtape() {
    auto& g = m_fenetre.global<::PasAPas>();
    const auto& t = *m_trace;
    const int k = std::clamp(g.get_etape(), 0, (int)t.m_etats.size() - 1);
    const Etat& e = t.m_etats[(size_t)k];
    const bool dernier = k == (int)t.m_etats.size() - 1;
    const size_t p = t.m_apprentissage.size();
    const std::string cp = m_jeu && t.m_classePositive < (int)m_jeu->m_nomsClasses.size() ? "classe " + m_jeu->m_nomsClasses[(size_t)t.m_classePositive] : "classe positive";
    std::string titre, badge, texte, fin;
    std::vector<Fait> faits;
    auto nb = [](double v, int c = 3) { return NombreFr(v, c); };
    if (t.m_algo == ADABOOST) {
        if (e.m_depart) {
            titre = "Départ";
            texte = "Les " + std::to_string(p) + " points d’apprentissage ont le même poids, 1/" + std::to_string(p) + ". " + cp + " contre toutes les autres. F5 ajoute un classifieur faible.";
            faits = {{"Points d’apprentissage", S(std::to_string(p))}, {"Faibles retenus", "0"}};
        } else {
            const int it = (int)G(e.m_g, "t", (double)e.m_iteration);
            titre = "Itération " + std::to_string(it);
            badge = "α = " + nb(G(e.m_g, "alpha"));
            std::string quoi = "un hyperplan oblique";
            const auto& its = t.m_final.Membre("adaboost").Membre("iterations").Elements();
            if (it >= 1 && it <= (int)its.size()) {
                const auto& f = its[(size_t)it - 1].Membre("faible");
                if (f.Membre("type").Texte() == "seuil") quoi = "un seuil sur l’attribut " + std::to_string(f.Membre("variable").Entier() + 1) + " (" + nb(f.Membre("seuil").Nombre(), 4) + ")";
            }
            texte = "Le faible choisi est " + quoi + ". Il se trompe sur " + Pct(G(e.m_g, "epsilon")) + " du poids total (ε), d’où son vote α = ½ ln((1−ε)/ε) = " + nb(G(e.m_g, "alpha")) +
                    ". Les points qu’il classe mal grossissent (poids × e^α), les autres rapetissent, puis tout est divisé par Z = " + nb(G(e.m_g, "Z")) + ".";
            faits = {{"Erreur pondérée ε", S(Pct(G(e.m_g, "epsilon")))}, {"Erreur du vote (apprentissage)", S(Pct(G(e.m_g, "erreur_apprentissage")))},
                     {"Exactitude d’apprentissage", S(Pct(1 - G(e.m_g, "erreur_apprentissage")))}, {"Borne théorique", S(nb(G(e.m_g, "borne")))}};
        }
        if (dernier) fin = "Arrêt : " + t.m_final.Membre("raison_arret").Texte() + " (" + std::to_string(t.m_final.Membre("iterations_effectuees").Entier()) + " sur " +
                           std::to_string(t.m_final.Membre("iterations_demandees").Entier()) + " demandées).";
    } else if (t.m_algo == PERCEPTRON) {
        auto droite = [&]() { return e.m_v.size() >= 3 ? "w = (" + nb(e.m_v[0]) + " ; " + nb(e.m_v[1]) + "), b = " + nb(e.m_v[2]) : std::string("—"); };
        if (e.m_depart) {
            titre = "Départ";
            texte = "Poids et biais nuls : aucune droite encore. " + cp + " (y = 1) contre toutes les autres (y = 0). Chaque étape présente un exemple.";
        } else if (e.m_phase == "exemple") {
            const bool err = G(e.m_g, "erreur", 0) > 0;
            titre = "Exemple " + std::to_string(e.m_iteration) + " · époque " + nb(G(e.m_g, "epoque"));
            badge = err ? "erreur" : "bien classé";
            texte = err ? "L’exemple entouré (y = " + nb(G(e.m_g, "y")) + ") est prédit ŷ = " + nb(G(e.m_g, "y_chapeau")) + ". Correction : w ← w + η(y − ŷ)x et b ← b + η(y − ŷ) ; la droite bouge."
                        : "L’exemple entouré (y = " + nb(G(e.m_g, "y")) + ") est bien classé (ŷ = " + nb(G(e.m_g, "y_chapeau")) + ") : la droite ne bouge pas.";
            faits = {{"Droite", S(droite())}};
        } else {
            const double n = G(e.m_g, "erreurs", 0);
            titre = "Fin de l’époque " + std::to_string(e.m_iteration);
            badge = nb(n) + " erreurs";
            texte = n == 0 ? "Aucune erreur sur toute l’époque : le perceptron a convergé, la droite sépare les deux groupes."
                           : nb(n) + " exemples mal classés pendant cette époque : la droite a encore bougé.";
            faits = {{"Erreurs dans l’époque", S(nb(n) + " / " + std::to_string(p))}, {"Droite", S(droite())}};
        }
        if (dernier) fin = "Arrêt : " + t.m_final.Membre("raison_arret").Texte() + ".";
    } else if (t.m_algo == KMEANS) {
        const bool maj = e.m_phase == "mise_a_jour";
        titre = "Itération " + std::to_string(e.m_iteration) + (maj ? " · mise à jour" : " · affectation");
        badge = "J = " + nb(G(e.m_g, "J"), 4);
        texte = maj ? "Chaque centre se place au barycentre des points qui lui sont affectés. Le critère J (somme des distances au carré) vaut " + nb(G(e.m_g, "J"), 4) + "."
                    : "Chaque point rejoint le centre le plus proche : " + nb(G(e.m_g, "changements")) + " affectations ont changé." + (G(e.m_g, "changements", 1) == 0 ? " Plus rien ne bouge." : "");
        faits = {{"Centres", S(std::to_string(e.m_v.size() / 2))}, {"Affectations modifiées", S(nb(G(e.m_g, "changements")))}, {"Critère J", S(nb(G(e.m_g, "J"), 4))}};
        if (dernier) fin = "Arrêt : " + t.m_final.Membre("raison_arret").Texte() + ". Couleurs des points : affectations publiées par le moteur à chaque étape.";
    } else if (t.m_algo == EM) {
        const size_t K = e.m_v.size() / 5;
        const bool init = e.m_phase == "initialisation";
        titre = init ? "Initialisation" : "Itération " + std::to_string(e.m_iteration);
        badge = "log L = " + nb(G(e.m_g, "log_vraisemblance"), 5);
        if (init) texte = "Départ d’OpenCV : k-moyennes, puis une étape E. Chaque ellipse montre une gaussienne (1 et 2 écarts-types).";
        else {
            const double prec = k > 0 ? G(t.m_etats[(size_t)k - 1].m_g, "log_vraisemblance") : G(e.m_g, "log_vraisemblance");
            const double lv = G(e.m_g, "log_vraisemblance");
            texte = "Étape M puis E : moyennes, variances et proportions recalculées à partir des responsabilités. La log-vraisemblance " +
                    std::string(lv >= prec ? "monte" : "baisse") + " de " + nb(std::abs(lv - prec)) + ".";
        }
        std::string pis;
        for (size_t c = 0; c < K; ++c) pis += (c ? " · " : "") + NombreFr(e.m_v[4 * K + c], 2);
        faits = {{"Gaussiennes", S(std::to_string(K))}, {"Log-vraisemblance", S(nb(G(e.m_g, "log_vraisemblance"), 5))}, {"Proportions π", S(pis)}};
        if (dernier) fin = "Couleurs des points : cluster final publié par le moteur.";
    } else {
        titre = e.m_phase == "initial" ? "Poids initiaux" : "Époque " + nb(G(e.m_g, "epoque"));
        badge = "perte " + nb(G(e.m_g, "perte_apprentissage"));
        texte = e.m_phase == "initial" ? "Poids tirés au hasard, avant toute correction. Les régions montrent la classe de sortie maximale du réseau."
                                       : "Une époque de rétropropagation : les poids ont été corrigés sur tous les points d’apprentissage. Le test sert uniquement à l’affichage.";
        faits = {{"Perte d’apprentissage", S(nb(G(e.m_g, "perte_apprentissage")))}, {"Exactitude d’apprentissage", S(Pct(G(e.m_g, "exactitude_apprentissage")))}};
        if (!std::isnan(G(e.m_g, "exactitude_validation"))) faits.push_back({"Exactitude sur le test (affichage)", S(Pct(G(e.m_g, "exactitude_validation")))});
        if (dernier) fin = std::to_string(t.m_final.Membre("epoques_effectuees").Entier()) + " époques effectuées.";
    }
    g.set_etat_titre(S(titre));
    g.set_etat_badge(S(badge));
    g.set_etat_texte(S(texte));
    g.set_etat_faits(std::make_shared<slint::VectorModel<Fait>>(faits));
    g.set_etat_fin(S(fin));
}

void PasAPas::Peindre() {
    auto& g = m_fenetre.global<::PasAPas>();
    if (m_l < 8 || m_h < 8 || !m_jeu || m_jeu->m_nbAttributs < 2) return;
    const float e = m_fenetre.window().scale_factor();
    const int L = (int)std::lround(m_l * e), H = (int)std::lround(m_h * e);
    const bool sombre = m_fenetre.global<Theme>().get_sombre();
    const int ax = m_trace ? m_trace->m_axeX : g.get_axe_x(), ay = m_trace ? m_trace->m_axeY : g.get_axe_y();
    const auto& j = *m_jeu;
    const int d = j.m_nbAttributs;
    auto X = [&](int l) { return j.m_valeurs[(size_t)l * d + ax]; };
    auto Y = [&](int l) { return j.m_valeurs[(size_t)l * d + ay]; };
    // domaine : toutes les lignes du jeu sur les deux axes, 5 % de marge
    double x0 = X(0), x1 = x0, y0 = Y(0), y1 = y0;
    for (int l = 0; l < j.m_nbLignes; ++l) { x0 = std::min(x0, X(l)); x1 = std::max(x1, X(l)); y0 = std::min(y0, Y(l)); y1 = std::max(y1, Y(l)); }
    const double mx = x1 > x0 ? (x1 - x0) * 0.05 : 0.5, my = y1 > y0 ? (y1 - y0) * 0.05 : 0.5;
    m_dom = {x0 - mx, x1 + mx, y0 - my, y1 + my};
    Toile t(L, H, sombre, m_dom);
    const float r = 3.0f * e;
    const std::string noms = "attribut " + std::to_string(ax + 1) + " → · attribut " + std::to_string(ay + 1) + " ↑";

    if (!m_trace) {
        for (int l = 0; l < j.m_nbLignes; ++l) t.Point(X(l), Y(l), j.m_labels[l], r, 0.6f);
        g.set_plan(t.Image());
        return;
    }
    const auto& tr = *m_trace;
    const int k = std::clamp(g.get_etape(), 0, (int)tr.m_etats.size() - 1);
    const Etat& et = tr.m_etats[(size_t)k];
    const auto& A = tr.m_apprentissage;
    const size_t p = A.size();
    std::string legende = noms + " · " + std::to_string(p) + " points d’apprentissage · ";
    const std::string cp = tr.m_classePositive < (int)j.m_nomsClasses.size() ? "classe " + j.m_nomsClasses[(size_t)tr.m_classePositive] : "classe positive";
    auto couleurBinaire = [&](int l) { return j.m_labels[l] == tr.m_classePositive ? tr.m_classePositive : -1; };

    if (tr.m_algo == ADABOOST) {
        const auto& its = tr.m_final.Membre("adaboost").Membre("iterations").Elements();
        const int tcur = et.m_depart ? 0 : (int)G(et.m_g, "t", (double)et.m_iteration);
        auto demiPlan = [&](const decid::ValeurJson& f, double& nx, double& ny, double& s) {
            const double sens = f.Contient("sens") ? f.Membre("sens").Nombre() : 1;
            if (f.Membre("type").Texte() == "seuil") { const bool v0 = f.Membre("variable").Entier() == 0; nx = v0 ? 1 : 0; ny = v0 ? 0 : 1; }
            else { const auto& dir = f.Membre("direction").Elements(); nx = dir.size() > 0 ? dir[0].Nombre() : 1; ny = dir.size() > 1 ? dir[1].Nombre() : 0; }
            nx *= sens; ny *= sens; s = f.Membre("seuil").Nombre() * sens;
        };
        for (const auto& it : its) {
            const int ti = (int)it.Membre("t").Entier();
            double nx, ny, s;
            demiPlan(it.Membre("faible"), nx, ny, s);
            if (ti < tcur) t.DemiPlan(nx, ny, s, -1, 0.0f, 1.0f * e);
            if (ti == tcur) t.DemiPlan(nx, ny, s, tr.m_classePositive, 0.12f, 2.0f * e);
        }
        for (size_t i = 0; i < p; ++i) {
            const double v = et.m_depart || et.m_v.size() != p ? 1.0 / p : et.m_v[i];
            const float rr = std::clamp(r * 1.3f * (float)std::sqrt(v * p), 0.8f * e, 9 * r);
            t.Point(X(A[i]), Y(A[i]), couleurBinaire(A[i]), rr, 0.7f);
        }
        legende += "rayon ∝ √poids · zone teintée : côté « " + cp + " » du faible courant";
    } else if (tr.m_algo == PERCEPTRON) {
        if (!et.m_depart && et.m_v.size() >= 3) t.DemiPlan(et.m_v[0], et.m_v[1], -et.m_v[2], tr.m_classePositive, 0.12f, 2.0f * e);
        for (size_t i = 0; i < p; ++i) t.Point(X(A[i]), Y(A[i]), couleurBinaire(A[i]), r * 1.3f, 0.75f);
        if (et.m_phase == "exemple") {
            const int i = (int)G(et.m_g, "indice", -1);
            if (i >= 0 && i < (int)p) {
                t.Point(X(A[(size_t)i]), Y(A[(size_t)i]), couleurBinaire(A[(size_t)i]), r * 2.4f, 1.0f);
                t.Anneau(X(A[(size_t)i]), Y(A[(size_t)i]), r * 5, G(et.m_g, "erreur", 0) > 0 ? Toile::Encre::Accent : Toile::Encre::Texte, 2.0f * e, 0.95f);
            }
        }
        legende += "zone teintée : w·x + b > 0, prédit « " + cp + " »";
    } else if (tr.m_algo == KMEANS) {
        const size_t K = et.m_v.size() / 2;
        std::vector<int> aff = et.m_aff;
        bool parEtape = !aff.empty();
        if (aff.empty() && k == (int)tr.m_etats.size() - 1)
            for (const auto& a : tr.m_final.Membre("affectations").Elements()) aff.push_back((int)a.Entier());
        for (size_t i = 0; i < p; ++i) {
            if (i < aff.size()) t.Point(X(A[i]), Y(A[i]), aff[i], r, 0.8f);
            else t.Point(X(A[i]), Y(A[i]), -1, r, 0.5f);
        }
        for (size_t c = 0; c < K; ++c) {
            std::vector<std::pair<double, double>> chemin;
            for (int s = 0; s <= k; ++s)
                if (tr.m_etats[(size_t)s].m_v.size() > 2 * c + 1) chemin.push_back({tr.m_etats[(size_t)s].m_v[2 * c], tr.m_etats[(size_t)s].m_v[2 * c + 1]});
            t.Trajet(chemin, (int)c, 1.5f * e, 0.7f);
            t.Croix(et.m_v[2 * c], et.m_v[2 * c + 1], (int)c, 7.0f * e);
        }
        legende += std::string(parEtape ? "couleur : centre affecté à cette étape (moteur)" : "couleurs finales au dernier état") + " · croix : centres · traits : trajet des centres";
    } else if (tr.m_algo == EM) {
        const size_t K = et.m_v.size() / 5;
        const bool dernier = k == (int)tr.m_etats.size() - 1;
        const auto& cl = tr.m_final.Membre("clusters").Elements();
        for (size_t c = 0; c < K; ++c) {
            const double mxc = et.m_v[2 * c], myc = et.m_v[2 * c + 1];
            const double sx = std::sqrt(std::max(0.0f, et.m_v[2 * K + 2 * c])), sy = std::sqrt(std::max(0.0f, et.m_v[2 * K + 2 * c + 1]));
            t.EllipseAxiale(mxc, myc, 2 * sx, 2 * sy, (int)c, 0.08f, 0);
            t.EllipseAxiale(mxc, myc, sx, sy, (int)c, 0.16f, 1.5f * e);
        }
        for (size_t i = 0; i < p; ++i) {
            if (dernier && i < cl.size()) t.Point(X(A[i]), Y(A[i]), (int)cl[i].Entier(), r, 0.75f);
            else t.Point(X(A[i]), Y(A[i]), -1, r, 0.5f);
        }
        for (size_t c = 0; c < K; ++c) t.Croix(et.m_v[2 * c], et.m_v[2 * c + 1], (int)c, 6.0f * e);
        legende += "ellipses : 1 et 2 écarts-types de chaque gaussienne";
    } else {
        const long ep = (long)G(et.m_g, "epoque", (double)et.m_iteration);
        // regions : celles de l'epoque, ou la plus proche deja calculee en attendant
        const ImageMoteur* im = nullptr;
        long epAff = -1;
        for (const auto& [e2, img] : m_regions) if (epAff < 0 || std::abs(e2 - ep) < std::abs(epAff - ep)) { epAff = e2; im = &img; }
        if (im) t.Fond(*im);
        for (size_t i = 0; i < p; ++i) t.Point(X(A[i]), Y(A[i]), j.m_labels[A[i]], r * 1.2f, 0.8f);
        if (!m_regions.count(ep)) DemanderActivations(ep);
        legende += !im ? "régions : calcul par le moteur…" : epAff != ep ? "régions de l’époque " + std::to_string(epAff) + ", époque " + std::to_string(ep) + " en cours de calcul…"
                                                                       : "régions : classe de sortie maximale à l’époque " + std::to_string(ep) + " (moteur)";
    }
    g.set_legende(S(legende));
    g.set_plan(t.Image());
}

void PasAPas::DemanderActivations(long epoque) {
    m_epoqueVoulue = epoque;
    if (m_activationsEnCours || !m_trace) return;
    m_activationsEnCours = true;
    decid::ValeurJson r = decid::ValeurJson::Objet();
    r["operation"] = "activations";
    r["reseau_source"] = Moteur::RefTache(m_trace->m_id);
    r["epoque"] = epoque;
    decid::ValeurJson plan = decid::ValeurJson::Objet();
    plan["attribut_x"] = 0; plan["attribut_y"] = 1; plan["largeur"] = 90; plan["hauteur"] = 90;
    decid::ValeurJson dom = decid::ValeurJson::Objet(), dx = decid::ValeurJson::Tableau(), dy = decid::ValeurJson::Tableau();
    dx.Ajouter(m_dom.m_xMin); dx.Ajouter(m_dom.m_xMax); dy.Ajouter(m_dom.m_yMin); dy.Ajouter(m_dom.m_yMax);
    dom["x"] = dx; dom["y"] = dy;
    plan["domaine"] = dom;
    r["plan"] = plan;
    const std::string id = m_trace->m_id;
    const bool lance = m_moteur.Executer(std::move(r), [this, epoque, id](const FinCalcul& fin) {
        m_activationsEnCours = false;
        if (fin.m_ok && m_trace && m_trace->m_id == id) {
            auto js = LireJson(fin.m_resultat / "activations.json");
            const auto f = LireBinaire<float>(fin.m_resultat / "neurones.f32");
            if (js.Ok()) {
                const auto& pl = js.Valeur().Membre("plan");
                ImageMoteur im;
                im.m_largeur = (int)pl.Membre("largeur").Entier();
                im.m_hauteur = (int)pl.Membre("hauteur").Entier();
                const auto& px = pl.Membre("x").Elements();
                const auto& py = pl.Membre("y").Elements();
                im.m_xMin = px.size() > 1 ? px[0].Nombre() : m_dom.m_xMin; im.m_xMax = px.size() > 1 ? px[1].Nombre() : m_dom.m_xMax;
                im.m_yMin = py.size() > 1 ? py[0].Nombre() : m_dom.m_yMin; im.m_yMax = py.size() > 1 ? py[1].Nombre() : m_dom.m_yMax;
                long derniere = -1;
                for (const auto& c : pl.Membre("cartes").Elements()) derniere = std::max<long>(derniere, (long)c.Membre("couche").Entier());
                std::vector<size_t> sorties;
                for (const auto& c : pl.Membre("cartes").Elements()) if ((long)c.Membre("couche").Entier() == derniere) sorties.push_back((size_t)c.Membre("decalage").Entier());
                const size_t n = (size_t)im.m_largeur * im.m_hauteur;
                if (!sorties.empty() && sorties.back() + n <= f.size()) {
                    im.m_classes.resize(n);
                    im.m_scores.resize(n);
                    const double u = 1.0 / sorties.size();
                    for (size_t i = 0; i < n; ++i) {
                        size_t best = 0;
                        for (size_t c = 1; c < sorties.size(); ++c) if (f[sorties[c] + i] > f[sorties[best] + i]) best = c;
                        const double pm = f[sorties[best] + i];
                        im.m_classes[i] = (std::int16_t)best;
                        im.m_scores[i] = (std::uint8_t)std::lround(std::clamp((pm - u) / (1 - u), 0.0, 1.0) * 255);
                    }
                    m_regions[epoque] = std::move(im);
                }
            }
        }
        if (m_trace && m_trace->m_algo == RESEAU) Peindre();
        if (m_epoqueVoulue != epoque && m_trace && !m_regions.count(m_epoqueVoulue)) DemanderActivations(m_epoqueVoulue);
    });
    if (!lance) m_activationsEnCours = false;
}

void PasAPas::Ajouter() {
    auto& g = m_fenetre.global<::PasAPas>();
    if (!m_trace || m_etude.ContientTache(m_trace->m_id)) return;
    const auto& t = *m_trace;
    std::string population;
    const double score = ScoreFinal(population).second;
    const std::string ratio = NombreFr(t.m_ratio * 100, 3);
    m_etude.AjouterExperienceModule("pas-a-pas", t.m_id, std::string(NOMS[t.m_algo]) + " pas à pas" + (population == "apprentissage" ? " (apprentissage)" : ""),
                                    t.m_jeuNom + " · attributs " + std::to_string(t.m_axeX + 1) + " et " + std::to_string(t.m_axeY + 1),
                                    ratio + " % au hasard dans chaque classe, graine " + std::to_string(t.m_graine) + " · score " + population, score,
                                    "bibliotheque:" + t.m_jeuNom,
                                    "pas-a-pas|" + t.m_jeuNom + "|a" + std::to_string(t.m_axeX + 1) + "-a" + std::to_string(t.m_axeY + 1) + "|aleatoire_stratifie|" +
                                        ratio + "|" + std::to_string(t.m_graine) + "|" + (t.m_algo == RESEAU ? "minmax" : "aucune") + "|score-" + population);
    g.set_ajoute(true);
}

void PasAPas::ExporterEm() {
    if (!m_trace || m_trace->m_algo != EM || !m_jeu) return;
    const auto& cl = m_trace->m_final.Membre("clusters_toutes_lignes").Elements();
    if (cl.empty()) return;
    const auto source = m_etude.SourceBibliotheque(m_fenetre.global<::PasAPas>().get_jeu(), 0);
    if (!source) return;
    // valeurs recopiees telles quelles depuis le fichier d'origine (aucune reecriture des nombres)
    auto texte = decid::LireFichierTexte(source->m_chemin);
    if (!texte.Ok()) return;
    std::vector<std::vector<std::string>> lignes;
    size_t debut = 0;
    const std::string& tx = texte.Valeur();
    bool premiere = true;
    while (debut < tx.size()) {
        size_t fin = tx.find('\n', debut);
        if (fin == std::string::npos) fin = tx.size();
        std::string l = tx.substr(debut, fin - debut);
        debut = fin + 1;
        if (!l.empty() && l.back() == '\r') l.pop_back();
        if (l.empty()) continue;
        if (premiere && source->m_contrat.m_enTete == decid::ModeEnTete::Oui) { premiere = false; continue; }
        premiere = false;
        std::vector<std::string> champs;
        size_t a = 0;
        while (true) { const size_t b = l.find(',', a); champs.push_back(l.substr(a, b == std::string::npos ? std::string::npos : b - a)); if (b == std::string::npos) break; a = b + 1; }
        lignes.push_back(std::move(champs));
    }
    auto net = [](std::string s) { while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin()); while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back(); return s; };
    const int a0 = m_trace->m_axeX + 1, a1 = m_trace->m_axeY + 1;
    std::string csv;
    for (size_t i = 0; i < lignes.size() && i < cl.size(); ++i) {
        if ((int)lignes[i].size() <= std::max(a0, a1)) continue;
        csv += std::to_string(cl[i].Entier()) + "," + net(lignes[i][(size_t)a0]) + "," + net(lignes[i][(size_t)a1]) + "\n";
    }
    const std::wstring nom = source->m_chemin.stem().wstring() + L"-em-classees.csv";
    const auto chemin = app::plateforme::ChoisirFichierEnregistrement(L"Exporter les données classées par EM", nom, L"csv", {{L"Fichier CSV", L"*.csv"}});
    if (!chemin) return;
    auto st = decid::EcrireFichierAtomique(*chemin, csv, true);
    m_fenetre.invoke_montrer_toast(S(st.Ok() ? "Données classées exportées : " + decid::CheminUtf8(chemin->filename()) : "Export impossible : " + st.Err().Texte()));
}

void PasAPas::MontrerTache(const std::string& tache) {
    auto& g = m_fenetre.global<::PasAPas>();
    const auto it = m_traces.find(tache);
    if (it == m_traces.end()) {
        g.set_erreur("La trace de cette expérience n’est plus en mémoire : relancez « Calculer la trace » avec les mêmes réglages.");
        return;
    }
    m_trace = it->second;
    g.set_algorithme(m_trace->m_algo);
    g.set_etape((int)m_trace->m_etats.size() - 1);
    g.set_nb_etapes((int)m_trace->m_etats.size());
    g.set_trace_presente(true);
    g.set_ajoute(true);
    Afficher();
}

} // namespace studio
