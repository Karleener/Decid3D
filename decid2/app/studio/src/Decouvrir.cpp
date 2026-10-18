#include "Decouvrir.h"

#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace studio {

namespace {
slint::SharedString S(const std::string& s) { return slint::SharedString(s); }
std::string Pct(double v) {
    char b[32];
    std::snprintf(b, sizeof b, "%.1f %%", v * 100.0);
    std::string s(b);
    for (auto& c : s) if (c == '.') c = ',';
    return s;
}
constexpr int SPIRALE = 1, XOR = 0, SPIRALES_3D = 10;   // indices du catalogue de la bibliotheque
} // namespace



Decouvrir::Decouvrir(Fenetre& fenetre, Moteur& moteur, Etude& etude) : m_fenetre(fenetre), m_moteur(moteur), m_etude(etude) {
    // Questions, paris et explications fixes ; les scores viennent du moteur.
    // Familles : 0 SVM, 1 k-PPV, 2 foret ; valeurs : voir Etude.cpp. Partages : 0 50 % au hasard, 3 debut de chaque classe.
    Definition droite;
    droite.m_titre = "Pourquoi une droite ne suffit pas"; droite.m_resume = "Un SVM linéaire face à une spirale."; droite.m_badges = {"Spirale", "SVM"};
    droite.m_question = "Un SVM à noyau linéaire peut-il séparer les deux bras de la spirale ?";
    droite.m_paris = {{"Oui, presque parfaitement", "> 90 %"}, {"À moitié environ", "≈ 75 %"}, {"Non, pas mieux que le hasard", "≈ 50 %"}};
    droite.m_bon = 2;
    droite.m_explication = "Un noyau linéaire ne trace qu’une droite. Deux bras enroulés l’un dans l’autre ne peuvent pas être séparés par une droite : la moitié des points tombe toujours du mauvais côté.";
    droite.m_principal = {0, 0, 0, "linéaire"};
    droite.m_explorerNom = "Noyau et γ";
    droite.m_explorer = {{0, 0, 0, "linéaire"}, {0, 0, 3, "γ 30"}, {0, 0, 4, "γ 100"}, {0, 0, 5, "γ 300"}, {0, 0, 7, "γ 3000"}};
    droite.m_explorerAide = "Le noyau gaussien plie la frontière. γ règle la portée de chaque point : trop petit, la frontière reste lisse ; trop grand, elle se fragmente.";

    Definition piege;
    piege.m_titre = "Le piège du partage"; piege.m_resume = "Apprendre sur le début du fichier, tester sur la fin."; piege.m_badges = {"Spirale", "k-PPV", "Protocole"};
    piege.m_question = "On apprend sur la première moitié de chaque classe du fichier, on teste sur l’autre. Quel score pour un k-PPV, qui fait ≈ 99 % d’habitude ?";
    piege.m_paris = {{"Toujours ≈ 99 %", "l’algorithme est bon"}, {"Un peu moins, ≈ 90 %", ""}, {"Proche du hasard, ≈ 50 %", ""}};
    piege.m_bon = 2;
    piege.m_explication = "Dans ce fichier, chaque classe est rangée du centre vers l’extérieur. Le modèle n’a vu que le cœur de la spirale ; il est testé sur des bords qu’il n’a jamais vus. Le protocole compte autant que l’algorithme.";
    piege.m_principal = {3, 1, 2, "début du fichier"};
    piege.m_explorerNom = "Partage";
    piege.m_explorer = {{3, 1, 2, "début du fichier"}, {0, 1, 2, "tirage au hasard"}};
    piege.m_explorerAide = "Même algorithme, mêmes données : seul le choix des points d’apprentissage change.";

    Definition voisins;
    voisins.m_titre = "Combien de voisins ?"; voisins.m_resume = "Plus de votes, meilleure décision ?"; voisins.m_badges = {"Spirale", "k-PPV"};
    voisins.m_question = "Avec k = 101 voisins au lieu de 5, le score du k-PPV…";
    voisins.m_paris = {{"augmente : plus de votes, plus fiable", ""}, {"reste à peu près le même", ""}, {"s’effondre", ""}};
    voisins.m_bon = 2;
    voisins.m_explication = "Avec 101 voisins, le vote déborde sur les bras voisins de la spirale, qui sont de l’autre classe. Plus de votes n’est pas mieux quand les classes sont imbriquées.";
    voisins.m_principal = {0, 1, 7, "k = 101"};
    voisins.m_explorerNom = "k";
    voisins.m_explorer = {{0, 1, 0, "k = 1"}, {0, 1, 2, "k = 5"}, {0, 1, 5, "k = 25"}, {0, 1, 7, "k = 101"}};
    voisins.m_explorerAide = "Peu de voisins : décision très locale. Beaucoup : décision moyennée sur une large zone.";

    Definition arbres;
    arbres.m_titre = "Les arbres découpent en escalier"; arbres.m_resume = "Que peut faire un arbre qui pose deux questions ?"; arbres.m_badges = {"Spirale", "Forêt"};
    arbres.m_question = "Une forêt d’arbres de profondeur 2 (deux questions « x ≤ seuil ? » par arbre) sur la spirale obtient…";
    arbres.m_paris = {{"≈ 50 %", ""}, {"≈ 75 %", ""}, {"≈ 95 %", ""}};
    arbres.m_bon = 0;
    arbres.m_explication = "Deux questions découpent le plan en quatre rectangles au plus. Une spirale demande des centaines de petits rectangles : il faut des arbres beaucoup plus profonds.";
    arbres.m_principal = {0, 2, 1, "2"};
    arbres.m_explorerNom = "Profondeur";
    arbres.m_explorer = {{0, 2, 1, "2"}, {0, 2, 2, "4"}, {0, 2, 3, "8"}, {0, 2, 4, "12"}};
    arbres.m_explorerAide = "Chaque niveau ajoute une question et double le nombre de rectangles possibles.";

    Definition reseau;
    reseau.m_titre = "Ce que voit chaque neurone"; reseau.m_resume = "Construisez un réseau qui sépare XOR."; reseau.m_badges = {"XOR", "Réseau"};
    reseau.m_jeu = XOR;
    reseau.m_reseau = true;
    reseau.m_question = "Objectif : séparer les deux classes de XOR avec un réseau de neurones, et voir ce que chaque neurone apprend.";
    reseau.m_explication = "Ce scénario se joue dans l’outil Pas à pas du réseau, calculé par le moteur : architecture, propagation et carte de chaque neurone, époque par époque.";

    Definition espace;
    espace.m_titre = "Cinq spirales dans l’espace"; espace.m_resume = "Les frontières en 3D de trois algorithmes."; espace.m_badges = {"Spirales 3D", "3D"};
    espace.m_jeu = SPIRALES_3D;
    espace.m_espace = true;
    espace.m_question = "Trois modèles apprennent les spirales 3D. Lequel trace des frontières en forme de boîtes ?";
    espace.m_paris = {{"SVM gaussien", ""}, {"k plus proches voisins", ""}, {"Forêt aléatoire", ""}};
    espace.m_bon = 2;
    espace.m_explication = "Une forêt ne pose que des questions « x ≤ seuil ? » sur un seul axe à la fois : ses frontières sont faites de faces parallèles aux axes. Le SVM gaussien trace des surfaces lisses ; le k-PPV suit les points de près.";
    espace.m_principal = {0, 0, 2, "SVM"};
    espace.m_explorer = {{0, 0, 2, "SVM"}, {0, 1, 2, "k-PPV"}, {0, 2, 4, "Forêt"}};

    m_definitions = {droite, piege, voisins, arbres, reseau, espace};

    auto& g = m_fenetre.global<::Decouvrir>();
    g.on_ouvrir([this](int i) { Ouvrir(i); });
    g.on_fermer([this] { Fermer(); });
    g.on_lancer([this] { Lancer(); });
    g.on_manipuler([this] { Manipuler(); });
    g.on_explorer([this](int k) { Explorer(k); });
    g.on_choisir_algo_3d([this](int k) { ChoisirAlgo3D(k); });
    g.on_vers_etude([this] { VersEtude(); });
    g.on_taille_vue([this](float l, float h) { if (std::abs(l - m_l) < 1 && std::abs(h - m_h) < 1) return; const bool premier = m_l < 1; m_l = l; m_h = h; if (premier) Peindre(); else m_redim.start(slint::TimerMode::SingleShot, std::chrono::milliseconds(80), [this] { Peindre(); }); });
    g.on_tourner([this](float dx, float dy) { m_vue3d.Tourner(dx, dy); Peindre(); });
    g.on_zoomer([this](float c) { m_vue3d.Zoomer(-c); Peindre(); });

    // jeux lus une fois (etape du pari, vignettes) ; ils ne sont jamais modifies
    for (int jeu : {SPIRALE, XOR, SPIRALES_3D}) {
        const auto source = m_etude.SourceBibliotheque(jeu, 0);
        if (!source) continue;
        auto lu = decid::LireCsv(source->m_chemin, source->m_contrat);
        if (lu.Ok()) m_donnees.emplace(jeu, std::move(lu.Valeur()));
    }
    Repeindre();
}

const Decouvrir::Definition& Decouvrir::Def() const { return m_definitions[(size_t)std::max(0, m_courant)]; }
std::string Decouvrir::Cle(const Config& c) const {
    return std::to_string(Def().m_jeu) + "|" + std::to_string(c.m_partage) + "|" + std::to_string(c.m_famille) + "|" + std::to_string(c.m_valeur);
}

// Vignettes de la liste : les donnees seules (aucun resultat n'est montre avant le pari)
void Decouvrir::Repeindre() {
    const bool sombre = m_fenetre.global<Theme>().get_sombre();
    const float e = m_fenetre.window().scale_factor();
    const int L = (int)std::lround(400 * e), H = (int)std::lround(168 * e);
    std::vector<Scenario> v;
    for (const auto& d : m_definitions) {
        slint::Image vignette;
        const auto it = m_donnees.find(d.m_jeu);
        if (it != m_donnees.end()) {
            const auto& j = it->second;
            if (d.m_espace) {
                std::vector<Vue3D::Point> pts;
                VolumeMoteur vide;
                for (int k = 0; k < 3; ++k) { vide.m_min[k] = 1e300; vide.m_max[k] = -1e300; }
                for (int l = 0; l < j.m_nbLignes; ++l)
                    for (int k = 0; k < 3; ++k) {
                        const double x = j.m_valeurs[(size_t)l * j.m_nbAttributs + k];
                        vide.m_min[k] = std::min(vide.m_min[k], x); vide.m_max[k] = std::max(vide.m_max[k], x);
                    }
                for (int l = 0; l < j.m_nbLignes; l += 2)
                    pts.push_back({j.m_valeurs[(size_t)l * j.m_nbAttributs], j.m_valeurs[(size_t)l * j.m_nbAttributs + 1], j.m_valeurs[(size_t)l * j.m_nbAttributs + 2], j.m_labels[l], 0.85f});
                Vue3D v3;
                v3.Preparer(vide, pts);
                vignette = v3.Rendre(L, H, sombre, e * 0.7f);
            } else {
                std::vector<PointPlan> pts;
                for (int l = 0; l < j.m_nbLignes; l += 2)
                    pts.push_back({j.m_valeurs[(size_t)l * j.m_nbAttributs], j.m_valeurs[(size_t)l * j.m_nbAttributs + 1], j.m_labels[l], false, 0.8f});
                DomainePlan dom = DomaineDesPoints(pts);
                // cadre 400 x 168 : elargir le domaine en x pour garder les proportions
                const double cx = (dom.m_xMin + dom.m_xMax) / 2, demi = (dom.m_yMax - dom.m_yMin) * (400.0 / 168.0) / 2;
                dom.m_xMin = cx - demi; dom.m_xMax = cx + demi;
                vignette = RendrePlan(L, H, sombre, dom, nullptr, pts, 1.3f * e);
            }
        }
        std::vector<slint::SharedString> badges;
        for (const auto& b : d.m_badges) badges.push_back(S(b));
        v.push_back({S(d.m_titre), S(d.m_resume), std::make_shared<slint::VectorModel<slint::SharedString>>(badges), vignette});
    }
    m_fenetre.global<::Decouvrir>().set_scenarios(std::make_shared<slint::VectorModel<Scenario>>(v));
    if (m_courant >= 0) Peindre();
}

void Decouvrir::Ouvrir(int i) {
    if (i < 0 || i >= (int)m_definitions.size()) return;
    m_courant = i;
    m_etape = 0;
    m_explore = 0;
    m_algo3d = 0;
    const auto& d = Def();
    auto& g = m_fenetre.global<::Decouvrir>();
    g.set_courant(i);
    g.set_pari(-1);
    g.set_message("");
    if (d.m_reseau) {
        // pas de pari : le scenario se joue dans l'outil Pas a pas du reseau
        m_fenetre.invoke_ouvrir_outil("pas-a-pas");
        m_fenetre.global<::PasAPas>().invoke_choisir_jeu(XOR);
        m_fenetre.global<::PasAPas>().invoke_choisir_algorithme(4);   // reseau Decid
        g.set_courant(-1);
        m_courant = -1;
        return;
    }
    // le moteur prepare tout de suite le resultat principal (et, en 3D, les trois modeles)
    if (d.m_espace) for (const auto& c : d.m_explorer) Demander(c, true);
    else Demander(d.m_principal, false);
    Afficher();
}

void Decouvrir::Fermer() {
    m_courant = -1;
    m_fenetre.global<::Decouvrir>().set_courant(-1);
}

void Decouvrir::Lancer() {
    if (m_fenetre.global<::Decouvrir>().get_pari() < 0) return;
    m_etape = 1;
    Afficher();
}

void Decouvrir::Manipuler() {
    const auto& d = Def();
    m_etape = 2;
    if (!d.m_espace)
        for (const auto& c : d.m_explorer) Demander(c, false);
    for (size_t k = 0; k < d.m_explorer.size(); ++k)
        if (Cle(d.m_explorer[k]) == Cle(d.m_principal)) m_explore = (int)k;
    Afficher();
}

void Decouvrir::Explorer(int k) {
    m_explore = std::clamp(k, 0, std::max(0, (int)Def().m_explorer.size() - 1));
    Afficher();
}

void Decouvrir::ChoisirAlgo3D(int k) {
    m_algo3d = std::clamp(k, 0, 2);
    m_vue3dCle.clear();
    Afficher();
}

void Decouvrir::VersEtude() {
    const auto& d = Def();
    const Config c = d.m_espace ? d.m_explorer[(size_t)m_algo3d] : d.m_explorer.empty() ? d.m_principal : d.m_explorer[(size_t)m_explore];
    Fermer();
    m_etude.OuvrirConfiguration(d.m_jeu, c.m_partage, c.m_famille, c.m_valeur);
}

// ---------- calculs : file d'attente, une tache lourde a la fois ----------
void Decouvrir::Demander(const Config& c, bool volume) {
    if (m_resultats.count(Cle(c))) return;
    for (const auto& [x, v] : m_file) if (Cle(x) == Cle(c)) return;
    m_file.push_back({c, volume});
    if (!m_enCours) Suivant();
}

void Decouvrir::Suivant() {
    auto& g = m_fenetre.global<::Decouvrir>();
    if (m_file.empty()) { m_enCours = false; g.set_calcul(false); if (std::string(g.get_message()).rfind("Le moteur calcule", 0) == 0) g.set_message(""); Afficher(); return; }
    m_enCours = true;
    g.set_calcul(true);
    const auto [c, volume] = m_file.front();
    const std::string cle = Cle(c);
    const int jeu = Def().m_jeu;
    const auto source = m_etude.SourceBibliotheque(jeu, c.m_partage);
    if (!source) { m_file.clear(); m_enCours = false; g.set_calcul(false); g.set_message("Bibliothèque databasesCSV introuvable."); return; }
    g.set_message(S("Le moteur calcule : " + c.m_libelle + "…"));
    auto echec = [this](const std::string& m) {
        m_file.pop_front();
        m_fenetre.global<::Decouvrir>().set_message(S("Le moteur a refusé : " + m));
        Suivant();
    };
    const bool lance = m_moteur.Executer(m_etude.Requete(*source, c.m_famille, c.m_valeur), [this, cle, volume, echec](const FinCalcul& fin) {
        if (!fin.m_ok) { echec(fin.m_erreur); return; }
        auto r = std::make_shared<Resultat>();
        r->m_tache = fin.m_id;
        if (auto ev = LireEvaluation(fin.m_resultat); ev.Ok()) { r->m_exactitude = ev.Valeur().m_exactitude; r->m_definie = ev.Valeur().m_definie; }
        if (auto jeu = LireJeuPublie(fin.m_resultat); jeu.Ok()) r->m_jeu = std::move(jeu.Valeur());
        r->m_prediction = LirePredictions(fin.m_resultat, r->m_jeu.m_classes, r->m_jeu.Lignes());
        decid::ValeurJson op = decid::ValeurJson::Objet();
        op["experience"] = Moteur::RefTache(fin.m_id);
        op["domaine"] = "auto";
        op["valeurs_fixes"] = "moyenne_apprentissage";
        if (volume) {
            op["operation"] = "volume";
            op["resolution"] = 50;
            decid::ValeurJson at = decid::ValeurJson::Tableau();
            at.Ajouter(0); at.Ajouter(1); at.Ajouter(2);
            op["attributs"] = at;
        } else {
            op["operation"] = "image";
            op["largeur"] = 200; op["hauteur"] = 200;
            op["attribut_x"] = 0; op["attribut_y"] = 1;
        }
        m_moteur.Executer(std::move(op), [this, cle, volume, r, echec](const FinCalcul& fi) {
            if (!fi.m_ok) { echec(fi.m_erreur); return; }
            if (volume) { if (auto v = LireVolume(fi.m_resultat); v.Ok()) r->m_volume = std::move(v.Valeur()); }
            else if (auto im = LireImage(fi.m_resultat); im.Ok()) r->m_image = std::move(im.Valeur());
            m_resultats[cle] = r;
            m_file.pop_front();
            Afficher();
            Suivant();
        });
    });
    if (!lance) return;   // la suite a deja ete appelee avec l'erreur (par exemple : l'etude calcule deja)
}

// ---------- affichage ----------
void Decouvrir::Afficher() {
    if (m_courant < 0) return;
    const auto& d = Def();
    auto& g = m_fenetre.global<::Decouvrir>();
    g.set_etape(m_etape);
    g.set_question(S(d.m_question));
    std::vector<Pari> paris;
    for (const auto& [t, x] : d.m_paris) paris.push_back({S(t), S(x)});
    g.set_paris(std::make_shared<slint::VectorModel<Pari>>(paris));
    g.set_bon(d.m_bon);
    g.set_trois_d(d.m_espace);
    g.set_algo_3d(m_algo3d);
    g.set_explication(S(d.m_explication));
    const auto principal = m_resultats.find(Cle(d.m_principal));
    const bool juste = g.get_pari() == d.m_bon;
    std::string verdict = juste ? "Bien vu." : "Pas tout à fait.";
    if (d.m_espace) verdict += " Faites tourner le volume : comparez les trois frontières.";
    else if (principal != m_resultats.end()) verdict += " Score sur le test : " + (principal->second->m_definie ? Pct(principal->second->m_exactitude) : std::string("—")) + ".";
    else verdict += " Le moteur termine le calcul…";
    g.set_verdict(S(verdict));
    std::vector<slint::SharedString> vals;
    if (!d.m_espace) for (const auto& c : d.m_explorer) vals.push_back(S(c.m_libelle));
    g.set_explorer_nom(S(d.m_explorerNom));
    g.set_explorer_valeurs(std::make_shared<slint::VectorModel<slint::SharedString>>(vals));
    g.set_explorer_valeur(m_explore);
    g.set_explorer_aide(S(d.m_explorerAide));
    std::string score;
    if (m_etape == 2 && !d.m_espace && !d.m_explorer.empty()) {
        const auto it = m_resultats.find(Cle(d.m_explorer[(size_t)m_explore]));
        score = it == m_resultats.end() ? "calcul…" : (it->second->m_definie ? Pct(it->second->m_exactitude) : "—") + " sur le test";
    }
    g.set_explorer_score(S(score));
    Peindre();
}

void Decouvrir::Peindre() {
    if (m_courant < 0 || m_l < 8) return;
    const auto& d = Def();
    auto& g = m_fenetre.global<::Decouvrir>();
    const bool sombre = m_fenetre.global<Theme>().get_sombre();
    const float e = m_fenetre.window().scale_factor();
    const int L = (int)std::lround(m_l * e), H = (int)std::lround(m_h * e);

    if (d.m_espace) {
        // pari : les points seuls ; ensuite : le volume du modele choisi
        const Config& c = d.m_explorer[(size_t)m_algo3d];
        const auto it = m_resultats.find(Cle(c));
        const std::string cle = m_etape == 0 ? "points" : it != m_resultats.end() ? Cle(c) : "points";
        if (cle != m_vue3dCle) {
            m_vue3dCle = cle;
            const auto j = m_donnees.find(d.m_jeu);
            if (cle == "points" && j != m_donnees.end()) {
                const auto& jd = j->second;
                VolumeMoteur vide;
                for (int k = 0; k < 3; ++k) { vide.m_min[k] = 1e300; vide.m_max[k] = -1e300; }
                for (int l = 0; l < jd.m_nbLignes; ++l)
                    for (int k = 0; k < 3; ++k) {
                        const double x = jd.m_valeurs[(size_t)l * jd.m_nbAttributs + k];
                        vide.m_min[k] = std::min(vide.m_min[k], x); vide.m_max[k] = std::max(vide.m_max[k], x);
                    }
                std::vector<Vue3D::Point> pts;
                for (int l = 0; l < jd.m_nbLignes; ++l)
                    pts.push_back({jd.m_valeurs[(size_t)l * jd.m_nbAttributs], jd.m_valeurs[(size_t)l * jd.m_nbAttributs + 1], jd.m_valeurs[(size_t)l * jd.m_nbAttributs + 2], jd.m_labels[l], 0.85f});
                m_vue3d.Preparer(vide, pts);
            } else if (it != m_resultats.end() && it->second->m_volume) {
                const auto& r = *it->second;
                std::vector<Vue3D::Point> pts;
                const int dd = r.m_jeu.Attributs();
                for (int l = 0; l < r.m_jeu.Lignes(); ++l)
                    if (l < (int)r.m_prediction.size() && r.m_prediction[l] >= 0)
                        pts.push_back({r.m_jeu.m_valeurs[(size_t)l * dd], r.m_jeu.m_valeurs[(size_t)l * dd + 1], r.m_jeu.m_valeurs[(size_t)l * dd + 2], r.m_jeu.m_labels[l], 0.9f});
                m_vue3d.Preparer(*r.m_volume, pts);
            }
        }
        g.set_vue(m_vue3d.Rendre(L, H, sombre, e));
        g.set_legende(S(m_etape == 0 ? "Les cinq spirales · glisser pour tourner, molette pour zoomer"
                                     : std::string("Volume de décision calculé par le moteur (") + c.m_libelle + ") · glisser pour tourner"));
        return;
    }

    // 2D : au pari, les points du fichier ; ensuite, la frontiere du moteur et les points de test (erreurs devant)
    const Config& c = m_etape == 2 && !d.m_explorer.empty() ? d.m_explorer[(size_t)m_explore] : d.m_principal;
    const auto it = m_resultats.find(Cle(c));
    if (m_etape == 0 || it == m_resultats.end()) {
        std::vector<PointPlan> pts;
        const auto j = m_donnees.find(d.m_jeu);
        if (j != m_donnees.end())
            for (int l = 0; l < j->second.m_nbLignes; ++l)
                pts.push_back({j->second.m_valeurs[(size_t)l * j->second.m_nbAttributs], j->second.m_valeurs[(size_t)l * j->second.m_nbAttributs + 1], j->second.m_labels[l], false, 0.8f});
        const float rayon = std::clamp(2.4f * e * std::sqrt(4000.0f / std::max<size_t>(pts.size(), 400)), 1.4f * e, 3.6f * e);
        g.set_vue(RendrePlan(L, H, sombre, DomaineDesPoints(pts), nullptr, pts, rayon));
        g.set_legende(m_etape == 0 ? "Les données, avant tout apprentissage" : "Le moteur calcule…");
        return;
    }
    const auto& r = *it->second;
    std::vector<PointPlan> pts;
    const int dd = r.m_jeu.Attributs();
    for (int l = 0; l < r.m_jeu.Lignes(); ++l) {
        const int p = l < (int)r.m_prediction.size() ? r.m_prediction[l] : -1;
        if (p < 0) continue;
        PointPlan q{r.m_jeu.m_valeurs[(size_t)l * dd], r.m_jeu.m_valeurs[(size_t)l * dd + 1], r.m_jeu.m_labels[l], false};
        if (p != q.m_classe) { q.m_opacite = 1.0f; q.m_devant = true; } else q.m_opacite = 0.55f;
        pts.push_back(q);
    }
    DomainePlan dom = DomaineDesPoints(pts);
    if (r.m_image) dom = {r.m_image->m_xMin, r.m_image->m_xMax, r.m_image->m_yMin, r.m_image->m_yMax};
    const float rayon = std::clamp(2.4f * e * std::sqrt(4000.0f / std::max<size_t>(pts.size(), 400)), 1.4f * e, 3.6f * e);
    g.set_vue(RendrePlan(L, H, sombre, dom, r.m_image ? &*r.m_image : nullptr, pts, rayon));
    g.set_legende(S("Frontière et confiance calculées par le moteur (" + c.m_libelle + ") · points de test, erreurs en avant"));
}

} // namespace studio
