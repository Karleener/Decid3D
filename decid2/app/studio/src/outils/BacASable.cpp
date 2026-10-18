#include "BacASable.h"

#include "Fichiers.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace studio {

namespace {
slint::SharedString S(const std::string& s) { return slint::SharedString(s); }

// [famille moteur, cle, libelle, defaut] 
struct Param { const char* m_cle; const char* m_libelle; double m_defaut; double m_min; double m_max; };
struct FamilleBac { const char* m_id; std::vector<Param> m_params; };
const std::vector<FamilleBac>& Familles() {
    static const std::vector<FamilleBac> f = {
        {"kppv", {{"k", "k, nombre de voisins", 3, 1, 999}}},
        {"svm", {{"gamma", "γ (noyau gaussien)", 10, 0.001, 100000}, {"c", "C, pénalité", 10, 0.001, 100000}}},
        {"foret_aleatoire", {{"nb_arbres", "Nombre d’arbres", 50, 1, 1000}, {"profondeur_max", "Profondeur maximale", 10, 1, 50}}},
        {"bayes_normal", {}},
    };
    return f;
}
std::string Nombre(double v) {
    char b[32];
    std::snprintf(b, sizeof b, "%g", v);
    std::string s(b);
    for (auto& c : s) if (c == '.') c = ',';
    return s;
}
double Lire(const std::string& t, double defaut) {
    std::string u = t;
    for (auto& c : u) if (c == ',') c = '.';
    char* fin = nullptr;
    const double v = std::strtod(u.c_str(), &fin);
    return fin && fin != u.c_str() && std::isfinite(v) ? v : defaut;
}
} // namespace

BacASable::BacASable(Fenetre& fenetre, Moteur& moteur) : m_fenetre(fenetre), m_moteur(moteur) {
    auto& g = m_fenetre.global<::BacASable>();
    g.on_poser([this](float x, float y, int b, bool maj) { Poser(x, y, b, maj); });
    g.on_exemple([this] {
        // damier XOR de 4 x 12 points, tirage congruentiel de graine 7
        long long graine = 7;
        auto alea = [&] { graine = (graine * 16807) % 2147483647; return (double)graine / 2147483647.0; };
        m_points.clear();
        const double centres[4][3] = {{0.27, 0.27, 0}, {0.73, 0.73, 0}, {0.27, 0.73, 1}, {0.73, 0.27, 1}};
        for (const auto& c : centres)
            for (int k = 0; k < 12; ++k) {
                const double x = std::round((c[0] + (alea() - 0.5) * 0.3) * 1e4) / 1e4;
                const double y = std::round((c[1] + (alea() - 0.5) * 0.3) * 1e4) / 1e4;
                m_points.push_back({x, y, (int)c[2]});
            }
        Modifie();
    });
    g.on_annuler([this] { Annuler(); });
    g.on_effacer([this] { m_points.clear(); Modifie(); });
    g.on_changer_famille([this](int i) { m_fenetre.global<::BacASable>().set_famille(i); MajParametres(); Modifie(); });
    g.on_changer_parametre([this](int i, slint::SharedString t) {
        const auto& f = Familles()[(size_t)std::clamp(m_fenetre.global<::BacASable>().get_famille(), 0, 3)];
        if (i < 0 || i >= (int)f.m_params.size()) return;
        const auto& p = f.m_params[(size_t)i];
        const double v = std::clamp(Lire(std::string(t), p.m_defaut), p.m_min, p.m_max);
        m_valeurs[std::string(f.m_id) + "." + p.m_cle] = Nombre(v);
        MajParametres();
        Modifie();
    });
    g.on_changer_trois_classes([this](bool v) {
        m_fenetre.global<::BacASable>().set_trois_classes(v);
        if (!v) {
            const size_t avant = m_points.size();
            m_points.erase(std::remove_if(m_points.begin(), m_points.end(), [](const Point& p) { return p.m_classe == 2; }), m_points.end());
            if (m_points.size() != avant) Modifie();
        }
        MajCompteurs();
    });
    g.on_taille([this](float l, float h) {
        if (std::abs(l - m_l) < 1 && std::abs(h - m_h) < 1) return;
        const bool premier = m_l < 1; m_l = l; m_h = h; if (premier) Peindre(); else m_redim.start(slint::TimerMode::SingleShot, std::chrono::milliseconds(80), [this] { Peindre(); });
    });
    MajParametres();
    MajCompteurs();
}

void BacASable::Poser(float x, float y, int bouton, bool maj) {
    if (x < 0 || x > 1 || y < 0 || y > 1) return;
    const bool trois = m_fenetre.global<::BacASable>().get_trois_classes();
    const int c = trois && maj ? 2 : bouton == 2 ? 1 : 0;
    m_points.push_back({std::round(x * 1e4) / 1e4, std::round(y * 1e4) / 1e4, c});
    Modifie();
}

void BacASable::Annuler() {
    if (m_points.empty()) return;
    m_points.pop_back();
    Modifie();
}

void BacASable::Modifie() {
    auto& g = m_fenetre.global<::BacASable>();
    m_minuteur.stop();
    ++m_demande;
    g.set_erreur("");
    bool classes[3] = {false, false, false};
    for (const auto& p : m_points) classes[std::clamp(p.m_classe, 0, 2)] = true;
    const int n = (int)classes[0] + (int)classes[1] + (int)classes[2];
    if (n < 2) {
        m_image.reset();
        g.set_etat("vide");
        g.set_image_presente(false);
    } else {
        g.set_etat("attente");
        m_minuteur.start(slint::TimerMode::SingleShot, std::chrono::milliseconds(400), [this] { Lancer(); });
    }
    MajCompteurs();
    Peindre();
}

std::string BacASable::Csv() const {
    std::string csv;
    char b[96];
    for (const auto& p : m_points) {
        std::snprintf(b, sizeof b, "%d,%.4f,%.4f\n", p.m_classe, p.m_x, p.m_y);
        csv += b;
    }
    return csv;
}

void BacASable::Lancer() {
    auto& g = m_fenetre.global<::BacASable>();
    if (m_enCours) return;                 // a la fin du calcul en cours, la demande la plus recente sera relancee
    const int demande = m_demande;
    auto fichier = m_moteur.EcrireDonnees(Csv(), "csv");
    if (!fichier.Ok()) { g.set_etat("erreur"); g.set_erreur(S("Le moteur refuse : " + fichier.Err().Texte())); return; }
    const auto& f = Familles()[(size_t)std::clamp(g.get_famille(), 0, 3)];

    decid::ValeurJson r = decid::ValeurJson::Objet();
    decid::ValeurJson source = decid::ValeurJson::Objet();
    source["type"] = "csv"; source["chemin"] = decid::CheminUtf8(fichier.Valeur());
    source["en_tete"] = "non"; source["separateur"] = ","; source["colonne_cible"] = 0;
    r["source"] = source;
    r["threads"] = 1;
    decid::ValeurJson a = decid::ValeurJson::Objet();
    a["famille"] = f.m_id;
    if (std::string(f.m_id) == "svm") a["noyau"] = "RBF";
    if (std::string(f.m_id) == "foret_aleatoire") a["min_echantillons"] = 2;
    for (const auto& p : f.m_params) {
        const auto it = m_valeurs.find(std::string(f.m_id) + "." + p.m_cle);
        const double v = it == m_valeurs.end() ? p.m_defaut : Lire(it->second, p.m_defaut);
        if (std::string(p.m_cle) == "k" || std::string(p.m_cle) == "nb_arbres" || std::string(p.m_cle) == "profondeur_max") a[p.m_cle] = (int)std::lround(v);
        else a[p.m_cle] = v;
    }
    r["algorithme"] = a;
    decid::ValeurJson pa = decid::ValeurJson::Objet();
    pa["methode"] = "premiers_n_par_classe"; pa["ratio"] = 1; pa["graine"] = 1;   // tous les points apprennent
    r["partage"] = pa;
    decid::ValeurJson n = decid::ValeurJson::Objet();
    n["methode"] = "minmax"; n["bas"] = 0; n["haut"] = 1;
    r["normalisation"] = n;

    m_enCours = true;
    g.set_etat("calcul");
    const auto debut = std::chrono::steady_clock::now();
    auto fin = [this, demande](const std::string& erreur) {
        m_enCours = false;
        auto& g = m_fenetre.global<::BacASable>();
        if (demande != m_demande) {          // perime : relancer avec l'etat courant
            if (g.get_etat() != "vide") m_minuteur.start(slint::TimerMode::SingleShot, std::chrono::milliseconds(0), [this] { Lancer(); });
            return;
        }
        if (!erreur.empty()) { g.set_etat("erreur"); g.set_erreur(S("Le moteur refuse : " + erreur)); }
        Peindre();
    };
    const int nbPoints = (int)m_points.size();
    m_moteur.Executer(std::move(r), [this, demande, fin, debut, nbPoints](const FinCalcul& f1) {
        if (!f1.m_ok || demande != m_demande) { fin(f1.m_ok ? "" : f1.m_erreur); return; }
        decid::ValeurJson im = decid::ValeurJson::Objet();
        im["operation"] = "image";
        im["experience"] = Moteur::RefTache(f1.m_id);
        im["largeur"] = 160; im["hauteur"] = 160;
        im["attribut_x"] = 0; im["attribut_y"] = 1;
        decid::ValeurJson dom = decid::ValeurJson::Tableau();
        dom.Ajouter(0); dom.Ajouter(1); dom.Ajouter(0); dom.Ajouter(1);
        im["domaine"] = dom;
        im["valeurs_fixes"] = "moyenne_apprentissage";
        m_moteur.Executer(std::move(im), [this, demande, fin, debut, nbPoints](const FinCalcul& f2) {
            if (!f2.m_ok || demande != m_demande) { fin(f2.m_ok ? "" : f2.m_erreur); return; }
            auto image = LireImage(f2.m_resultat);
            if (!image.Ok()) { fin(image.Err().Texte()); return; }
            m_image = std::move(image.Valeur());
            auto& g = m_fenetre.global<::BacASable>();
            g.set_image_presente(true);
            g.set_etat("ajour");
            g.set_score_defini(!m_image->m_scores.empty());
            g.set_legende(S(m_image->m_scores.empty() ? "Régions en classes simples. Score de confiance non défini : " + m_image->m_definitionScore
                                                      : "Score de confiance du moteur : " + m_image->m_definitionScore));
            const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - debut).count();
            char d[32];
            std::snprintf(d, sizeof d, "%.2f", s);
            std::string ds(d);
            for (auto& c : ds) if (c == '.') c = ',';
            g.set_faits(S("Points appris : " + std::to_string(nbPoints) + "\nImage : " + std::to_string(m_image->m_largeur) + " × " +
                          std::to_string(m_image->m_hauteur) + "\nDurée (apprentissage et image) : " + ds + " s"));
            fin("");
        });
    });
}

void BacASable::MajCompteurs() {
    std::vector<int> c(3, 0);
    for (const auto& p : m_points) ++c[(size_t)std::clamp(p.m_classe, 0, 2)];
    auto& g = m_fenetre.global<::BacASable>();
    g.set_comptes(std::make_shared<slint::VectorModel<int>>(c));
    g.set_nb_points((int)m_points.size());
}

void BacASable::MajParametres() {
    auto& g = m_fenetre.global<::BacASable>();
    const auto& f = Familles()[(size_t)std::clamp(g.get_famille(), 0, 3)];
    std::vector<ParametreSaisi> v;
    for (const auto& p : f.m_params) {
        const auto it = m_valeurs.find(std::string(f.m_id) + "." + p.m_cle);
        v.push_back({S(p.m_cle), S(p.m_libelle), S(it == m_valeurs.end() ? Nombre(p.m_defaut) : it->second)});
    }
    g.set_parametres(std::make_shared<slint::VectorModel<ParametreSaisi>>(v));
}

void BacASable::Peindre() {
    auto& g = m_fenetre.global<::BacASable>();
    const std::string etat(g.get_etat());
    g.set_aide(S(m_image ? (etat == "ajour" ? "régions et confiance calculées par le moteur" : etat == "erreur" ? "régions du dernier calcul accepté"
                                                                                                           : "régions du calcul précédent · nouveau calcul demandé")
                         : "posez des points d’au moins deux classes"));
    if (m_l < 8 || m_h < 8) return;
    const float e = m_fenetre.window().scale_factor();
    std::vector<PointPlan> pts;
    for (const auto& p : m_points) pts.push_back({p.m_x, p.m_y, p.m_classe, false, 0.95f});
    g.set_plan(RendrePlan((int)std::lround(m_l * e), (int)std::lround(m_h * e), m_fenetre.global<Theme>().get_sombre(), {0, 1, 0, 1},
                          m_image ? &*m_image : nullptr, pts, 5.0f * e));
}

void BacASable::Repeindre() { Peindre(); }

} // namespace studio
