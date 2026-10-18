// Tests de la normalisation min-max.
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "Normalisation.h"
#include "Verifier.h"

using namespace decid;
namespace fs = std::filesystem;

namespace {

// 6 lignes, 2 attributs. Lignes 0..3 = apprentissage, 4..5 = test.
JeuDeDonnees Jeu() {
    JeuDeDonnees j;
    j.m_nbLignes = 6;
    j.m_nbAttributs = 2;
    j.m_valeurs = {0, 7, 10, 7, 20, 7, 30, 7, 100, 8, 15, 7};
    j.m_labels = {0, 0, 1, 1, 0, 1};
    j.m_nomsClasses = {"a", "b"};
    j.m_nomsAttributs = {"x1", "x2"};
    return j;
}

const std::vector<int> kApprentissage = {0, 1, 2, 3};

} // namespace

TEST(minmax_formule_et_constante) {
    JeuDeDonnees j = Jeu();
    auto r = AjusterMinMax(j, kApprentissage, 0.0, 1.0, "hashP");
    VERIFIER(r.Ok());
    const Normalisation& n = r.Valeur();
    VERIFIER_PROCHE(n.m_min[0], 0.0, 0.0);
    VERIFIER_PROCHE(n.m_max[0], 30.0, 0.0);
    VERIFIER_EGAL(n.m_attributsConstants.size(), (size_t)1);
    VERIFIER_EGAL(n.m_attributsConstants[0], 1);
    long hors = -1;
    auto m = AppliquerNormalisation(n, j, kApprentissage, &hors);
    VERIFIER(m.Ok());
    VERIFIER_EGAL(m.Valeur().rows, 4);
    VERIFIER_EGAL(m.Valeur().cols, 2);
    VERIFIER_PROCHE(m.Valeur().at<float>(1, 0), 10.0 / 30.0, 1e-6);
    VERIFIER_PROCHE(m.Valeur().at<float>(3, 0), 1.0, 0.0);
    VERIFIER_PROCHE(m.Valeur().at<float>(2, 1), 0.0, 0.0); // constante -> bas
    VERIFIER_EGAL(hors, 0L);
    // plage de sortie [-1, 1]
    auto r2 = AjusterMinMax(j, kApprentissage, -1.0, 1.0, "h");
    auto m2 = AppliquerNormalisation(r2.Valeur(), j, kApprentissage);
    VERIFIER_PROCHE(m2.Valeur().at<float>(0, 0), -1.0, 0.0);
    VERIFIER_PROCHE(m2.Valeur().at<float>(3, 0), 1.0, 0.0);
    VERIFIER_PROCHE(m2.Valeur().at<float>(2, 1), -1.0, 0.0);
}

TEST(lignes_de_test_sans_effet_sur_l_ajustement) {
    JeuDeDonnees j = Jeu();
    auto avant = AjusterMinMax(j, kApprentissage, 0.0, 1.0, "h");
    j.m_valeurs[8] = -5000.0;  // ligne 4, test
    j.m_valeurs[10] = 9999.0;  // ligne 5, test
    auto apres = AjusterMinMax(j, kApprentissage, 0.0, 1.0, "h");
    VERIFIER(avant.Ok() && apres.Ok());
    VERIFIER(avant.Valeur().m_min == apres.Valeur().m_min);
    VERIFIER(avant.Valeur().m_max == apres.Valeur().m_max);
    VERIFIER(avant.Valeur().Hash() == apres.Valeur().Hash());
}

TEST(hors_plage_compte_sans_ecretage) {
    JeuDeDonnees j = Jeu();
    auto n = AjusterMinMax(j, kApprentissage, 0.0, 1.0, "h");
    long hors = 0;
    auto m = AppliquerNormalisation(n.Valeur(), j, {4, 5}, &hors);
    VERIFIER(m.Ok());
    // ligne 4 : attribut 0 = 100 (hors) et attribut constant 8 != 7 (hors) ; ligne 5 : 15 dans la plage
    VERIFIER_EGAL(hors, 2L);
    VERIFIER_PROCHE(m.Valeur().at<float>(0, 0), 100.0 / 30.0, 1e-5); // pas d'ecretage
    VERIFIER_PROCHE(m.Valeur().at<float>(1, 0), 0.5, 1e-6);
}

TEST(double_application_identique) {
    JeuDeDonnees j = Jeu();
    auto n = AjusterMinMax(j, kApprentissage, 0.0, 1.0, "h");
    std::vector<int> toutes = {0, 1, 2, 3, 4, 5};
    auto a = AppliquerNormalisation(n.Valeur(), j, toutes);
    auto b = AppliquerNormalisation(n.Valeur(), j, toutes);
    VERIFIER(a.Ok() && b.Ok());
    VERIFIER_PROCHE(cv::norm(a.Valeur(), b.Valeur(), cv::NORM_INF), 0.0, 0.0);
    // les donnees brutes du jeu n'ont pas ete modifiees
    VERIFIER_PROCHE(j.Valeur(3, 0), 30.0, 0.0);
}

TEST(aucune_et_depassement_float32) {
    JeuDeDonnees j = Jeu();
    Normalisation aucune = AucuneNormalisation(2);
    long hors = 5;
    auto m = AppliquerNormalisation(aucune, j, {0, 1, 2}, &hors);
    VERIFIER(m.Ok());
    VERIFIER_EGAL(hors, 0L);
    VERIFIER_PROCHE(m.Valeur().at<float>(2, 0), 20.0, 0.0);
    j.m_valeurs[2] = 1e300;
    VERIFIER(!AppliquerNormalisation(aucune, j, {1}).Ok());
}

TEST(vecteur_isole_et_erreurs) {
    JeuDeDonnees j = Jeu();
    auto n = AjusterMinMax(j, kApprentissage, 0.0, 1.0, "h");
    std::vector<double> t = TransformerVecteur(n.Valeur(), {15.0, 7.0});
    VERIFIER_EGAL(t.size(), (size_t)2);
    VERIFIER_PROCHE(t[0], 0.5, 1e-12);
    VERIFIER_PROCHE(t[1], 0.0, 0.0);
    VERIFIER(!AjusterMinMax(j, {}, 0.0, 1.0, "h").Ok());
    VERIFIER(!AjusterMinMax(j, {0, 99}, 0.0, 1.0, "h").Ok());
    VERIFIER(!AjusterMinMax(j, kApprentissage, 1.0, 1.0, "h").Ok());
}

TEST(aller_retour_json) {
    JeuDeDonnees j = Jeu();
    j.m_valeurs[0] = 0.1;
    j.m_valeurs[2] = 1.0 / 3.0;
    auto n = AjusterMinMax(j, kApprentissage, -1.0, 1.0, "hashPartage");
    VERIFIER(n.Ok());
    auto lu = LireJson(n.Valeur().VersJson().Ecrire());
    VERIFIER(lu.Ok());
    auto q = NormalisationDepuisJson(lu.Valeur());
    VERIFIER(q.Ok());
    VERIFIER(q.Valeur().m_methode == n.Valeur().m_methode);
    VERIFIER(q.Valeur().m_min == n.Valeur().m_min);
    VERIFIER(q.Valeur().m_max == n.Valeur().m_max);
    VERIFIER(q.Valeur().m_bas == n.Valeur().m_bas);
    VERIFIER(q.Valeur().m_haut == n.Valeur().m_haut);
    VERIFIER(q.Valeur().m_hashPartage == n.Valeur().m_hashPartage);
    VERIFIER(q.Valeur().m_attributsConstants == n.Valeur().m_attributsConstants);
    VERIFIER(q.Valeur().Hash() == n.Valeur().Hash());
}

// ---------- fichier .lim (ancien Decid) ----------

TEST(lim_identique_au_minmax_ajuste) {
    JeuDeDonnees j = Jeu();
    auto ajuste = AjusterMinMax(j, kApprentissage, 0.1, 0.9, "hashP");
    auto lim = LireBornesLim("2\r\n0 30\r\n7\t7\r\n\r\n", "essai.lim", 2, 0.1, 0.9, "hashP");
    VERIFIER(ajuste.Ok() && lim.Ok());
    if (!ajuste.Ok() || !lim.Ok()) return;
    VERIFIER_EGAL(lim.Valeur().m_methode, std::string("minmax"));
    VERIFIER_EGAL(lim.Valeur().Hash(), ajuste.Valeur().Hash());
    VERIFIER_EGAL(lim.Valeur().m_attributsConstants.size(), (size_t)1);
    auto a = AppliquerNormalisation(ajuste.Valeur(), j, {0, 1, 2, 3, 4, 5});
    auto b = AppliquerNormalisation(lim.Valeur(), j, {0, 1, 2, 3, 4, 5});
    VERIFIER(a.Ok() && b.Ok());
    VERIFIER_EGAL(cv::norm(a.Valeur(), b.Valeur(), cv::NORM_INF), 0.0);
    // relu depuis le JSON publie comme n'importe quel min-max
    auto relu = NormalisationDepuisJson(lim.Valeur().VersJson());
    VERIFIER(relu.Ok());
}

TEST(lim_refus_avec_numero_de_ligne) {
    struct Cas { const char* m_texte; long m_ligne; };
    const Cas cas[] = {
        {"3\n0 1\n0 1\n0 1\n", 1},           // nombre d'attributs different de la source
        {"deux\n0 1\n0 1\n", 1},             // premiere ligne non entiere
        {"2.5\n0 1\n0 1\n", 1},
        {"2\n0 1 2\n0 1\n", 2},              // trois valeurs
        {"2\n0\n0 1\n", 2},                  // paire incomplete
        {"2\n0 1\n0,5 1\n", 3},              // virgule decimale
        {"2\n0 1\n5 1\n", 3},                // min > max
        {"2\n0 1\n\n0 inf\n", 4},            // non fini, numero physique (ligne vide comptee)
        {"2\n0 1\n", 2},                     // paire manquante
        {"2\n0 1\n0 1\n0 1\n", 4},           // ligne en trop
        {"", 1},                             // vide
    };
    for (const Cas& c : cas) {
        auto r = LireBornesLim(c.m_texte, "essai.lim", 2, 0.0, 1.0, "");
        VERIFIER(!r.Ok());
        if (r.Ok()) continue;
        VERIFIER_EGAL(r.Err().m_ligne, c.m_ligne);
        VERIFIER_EGAL(r.Err().m_fichier, std::string("essai.lim"));
        VERIFIER(r.Err().m_message.find("fichier .lim") == 0);
    }
    VERIFIER(!LireBornesLim("2\n0 1\n0 1\n", "essai.lim", 2, 1.0, 1.0, "").Ok());   // plage de sortie vide
}

TEST(lim_fichier_reel_t05_en_lecture_seule) {
    // fichier .lim reel de l'ancien Decid (facultatif) : DECID_T05_LIM = chemin de T05.lim
    const char* env = std::getenv("DECID_T05_LIM");
    if (!env || !*env) { std::printf("  (DECID_T05_LIM non defini : test ignore)\n"); return; }
    const fs::path t05 = env;
    if (!fs::exists(t05)) { std::printf("  (T05.lim absent : test ignore)\n"); return; }
    const auto avant = fs::last_write_time(t05);
    auto n = ChargerBornesLim(t05, 2, 0.1, 0.9, "h");
    VERIFIER(n.Ok());
    if (!n.Ok()) return;
    VERIFIER_PROCHE(n.Valeur().m_min[0], 0.013219, 0.0);
    VERIFIER_PROCHE(n.Valeur().m_max[0], 0.999888, 0.0);
    VERIFIER_PROCHE(n.Valeur().m_min[1], 0.030943, 0.0);
    VERIFIER_PROCHE(n.Valeur().m_max[1], 0.999944, 0.0);
    VERIFIER(n.Valeur().m_attributsConstants.empty());
    auto refus = ChargerBornesLim(t05, 3, 0.1, 0.9, "h");
    VERIFIER(!refus.Ok());
    if (!refus.Ok()) VERIFIER_EGAL(refus.Err().m_ligne, 1L);
    VERIFIER(fs::last_write_time(t05) == avant);
}

TEST(normalisation_demandee) {
    JeuDeDonnees j = Jeu();
    auto aucune = NormalisationDemandee("aucune", {}, 0, 1, j, kApprentissage, "hp");
    VERIFIER(aucune.Ok() && aucune.Valeur().m_methode == "aucune" && aucune.Valeur().m_hashPartage == "hp");
    auto minmax = NormalisationDemandee("minmax", {}, 0, 1, j, kApprentissage, "hp");
    VERIFIER(minmax.Ok() && minmax.Valeur().Hash() == AjusterMinMax(j, kApprentissage, 0, 1, "hp").Valeur().Hash());
    VERIFIER(!NormalisationDemandee("minmax_lim", "introuvable.lim", 0, 1, j, kApprentissage, "hp").Ok());
    VERIFIER(!NormalisationDemandee("zscore", {}, 0, 1, j, kApprentissage, "hp").Ok());
}

int main(int argc, char** argv) { return LancerTests(argc, argv); }
