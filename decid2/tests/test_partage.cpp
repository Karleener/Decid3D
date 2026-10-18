// Tests du partage apprentissage/test.
#include <algorithm>
#include <string>
#include <vector>

#include "Partage.h"
#include "Verifier.h"

using namespace decid;

namespace {

JeuDeDonnees Jeu(const std::vector<int>& labels, int nbClasses) {
    JeuDeDonnees j;
    j.m_nbLignes = (int)labels.size();
    j.m_nbAttributs = 1;
    j.m_labels = labels;
    for (int i = 0; i < nbClasses; ++i) j.m_nomsClasses.push_back(std::string(1, (char)('a' + i)));
    j.m_nomsAttributs = {"x1"};
    for (int i = 0; i < j.m_nbLignes; ++i) {
        j.m_valeurs.push_back((double)i);
        OrigineLigne o;
        o.m_source = 0;
        o.m_ligne = i + 1;
        j.m_origines.push_back(o);
    }
    j.m_sources.resize(1);
    return j;
}

std::vector<int> Suite(int debut, int fin) {
    std::vector<int> v;
    for (int i = debut; i < fin; ++i) v.push_back(i);
    return v;
}

bool Contient(const std::vector<std::string>& v, const std::string& s) {
    return std::find(v.begin(), v.end(), s) != v.end();
}

} // namespace

TEST(premiers_n_impair) {
    JeuDeDonnees j = Jeu(std::vector<int>(7, 0), 1);
    auto r = CreerPartage(j, MethodePartage::PremiersN, 0.5, 0, false);
    VERIFIER(r.Ok());
    VERIFIER(r.Valeur().m_apprentissage == Suite(0, 3));
    VERIFIER(r.Valeur().m_test == Suite(3, 7));
    VERIFIER_EGAL(r.Valeur().m_generateur, std::string("splitmix64+rejet+fisher_yates_decid_v1"));
}

TEST(deux_moities_identifiables) {
    JeuDeDonnees j = Jeu(std::vector<int>(10, 0), 1);
    auto r = CreerPartage(j, MethodePartage::PremiersN, 0.5, 0, false);
    VERIFIER(r.Valeur().m_apprentissage == Suite(0, 5));
    VERIFIER(r.Valeur().m_test == Suite(5, 10));
}

TEST(vingt_cinq_pour_cent_inverse) {
    JeuDeDonnees j = Jeu(std::vector<int>(20, 0), 1);
    auto r = CreerPartage(j, MethodePartage::PremiersN, 0.25, 0, true);
    VERIFIER(r.Ok());
    VERIFIER(r.Valeur().m_apprentissage == Suite(5, 20));
    VERIFIER(r.Valeur().m_test == Suite(0, 5));
    VERIFIER(r.Valeur().m_inverse);
}

TEST(cent_pour_cent_test_vide) {
    JeuDeDonnees j = Jeu({0, 0, 1, 1}, 2);
    auto r = CreerPartage(j, MethodePartage::PremiersN, 1.0, 0, false);
    VERIFIER(r.Ok());
    VERIFIER(r.Valeur().m_test.empty());
    VERIFIER_EGAL(r.Valeur().m_apprentissage.size(), (size_t)4);
    VERIFIER(Contient(r.Valeur().m_diagnostics, "test vide : evaluation impossible, apprentissage seul autorise"));
    VERIFIER(Contient(r.Valeur().m_diagnostics, "classe a : 0 ligne en test"));
    // l'inversion d'un 100 % donne un apprentissage vide : erreur
    VERIFIER(!CreerPartage(j, MethodePartage::PremiersN, 1.0, 0, true).Ok());
}

TEST(ratio_invalide) {
    JeuDeDonnees j = Jeu({0, 0, 1, 1}, 2);
    VERIFIER(!CreerPartage(j, MethodePartage::PremiersN, 0.0, 0, false).Ok());
    VERIFIER(!CreerPartage(j, MethodePartage::PremiersN, -0.5, 0, false).Ok());
    VERIFIER(!CreerPartage(j, MethodePartage::PremiersN, 1.5, 0, false).Ok());
    // floor(4*0.2) = 0 : apprentissage vide
    VERIFIER(!CreerPartage(j, MethodePartage::PremiersN, 0.2, 0, false).Ok());
}

TEST(petites_classes) {
    // classe b : une seule ligne ; 50 % par classe -> 0 en apprentissage
    JeuDeDonnees j = Jeu({0, 0, 0, 0, 1}, 2);
    auto r = CreerPartage(j, MethodePartage::PremiersNParClasse, 0.5, 0, false);
    VERIFIER(r.Ok());
    VERIFIER((r.Valeur().m_apprentissage == std::vector<int>{0, 1}));
    VERIFIER((r.Valeur().m_test == std::vector<int>{2, 3, 4}));
    VERIFIER(Contient(r.Valeur().m_diagnostics, "classe b : 0 ligne en apprentissage"));
    auto c = ComptesClasses(j, r.Valeur().m_test);
    VERIFIER_EGAL(c[0], 2L);
    VERIFIER_EGAL(c[1], 1L);
}

TEST(premiers_n_par_classe_ordre_fichier) {
    JeuDeDonnees j = Jeu({0, 1, 0, 1, 0, 1, 0, 1}, 2);
    auto r = CreerPartage(j, MethodePartage::PremiersNParClasse, 0.5, 0, false);
    VERIFIER(r.Ok());
    VERIFIER((r.Valeur().m_apprentissage == std::vector<int>{0, 1, 2, 3}));
    VERIFIER((r.Valeur().m_test == std::vector<int>{4, 5, 6, 7}));
}

TEST(aleatoire_graine) {
    std::vector<int> labels;
    for (int i = 0; i < 200; ++i) labels.push_back(i % 3);
    JeuDeDonnees j = Jeu(labels, 3);
    auto a = CreerPartage(j, MethodePartage::AleatoireStratifie, 0.5, 42, false);
    auto b = CreerPartage(j, MethodePartage::AleatoireStratifie, 0.5, 42, false);
    auto c = CreerPartage(j, MethodePartage::AleatoireStratifie, 0.5, 43, false);
    VERIFIER(a.Ok() && b.Ok() && c.Ok());
    VERIFIER(a.Valeur().m_apprentissage == b.Valeur().m_apprentissage);
    VERIFIER(a.Valeur().m_test == b.Valeur().m_test);
    VERIFIER(a.Valeur().Hash() == b.Valeur().Hash());
    VERIFIER(a.Valeur().m_apprentissage != c.Valeur().m_apprentissage);
    VERIFIER(a.Valeur().Hash() != c.Valeur().Hash());
    VERIFIER(std::is_sorted(a.Valeur().m_apprentissage.begin(), a.Valeur().m_apprentissage.end()));
    // stratification : floor(67*0.5)=33, floor(67*0.5)=33, floor(66*0.5)=33
    auto cpt = ComptesClasses(j, a.Valeur().m_apprentissage);
    VERIFIER_EGAL(cpt[0], 33L);
    VERIFIER_EGAL(cpt[1], 33L);
    VERIFIER_EGAL(cpt[2], 33L);
    VERIFIER(ValiderPartage(a.Valeur(), 200).Ok());
}

TEST(tirage_uniforme_borne) {
    std::uint64_t etat = 7;
    bool tousDansBorne = true;
    for (int i = 0; i < 1000; ++i) if (TirageUniforme(etat, 10) >= 10) tousDansBorne = false;
    VERIFIER(tousDansBorne);
    std::uint64_t e1 = 123, e2 = 123;
    VERIFIER_EGAL(TirageUniforme(e1, 1000), TirageUniforme(e2, 1000));
    // SplitMix64 de reference (graine 0) : premiere sortie connue 0xE220A8397B1DCDAF
    std::uint64_t e0 = 0;
    VERIFIER_EGAL(TirageUniforme(e0, 0xFFFFFFFFFFFFFFFFULL) , (0xE220A8397B1DCDAFULL % 0xFFFFFFFFFFFFFFFFULL));
}

TEST(par_roles) {
    JeuDeDonnees j = Jeu({0, 0, 1, 1, 0}, 2);
    j.m_sources.resize(2);
    j.m_sources[0].m_role = RoleFichier::Apprentissage;
    j.m_sources[1].m_role = RoleFichier::Test;
    j.m_origines[3].m_source = 1;
    j.m_origines[4].m_source = 1;
    auto r = CreerPartage(j, MethodePartage::ParRoles, 0.5, 99, false);
    VERIFIER(r.Ok());
    VERIFIER((r.Valeur().m_apprentissage == std::vector<int>{0, 1, 2}));
    VERIFIER((r.Valeur().m_test == std::vector<int>{3, 4}));
    auto inv = CreerPartage(j, MethodePartage::ParRoles, 0.5, 99, true);
    VERIFIER(inv.Ok());
    VERIFIER((inv.Valeur().m_apprentissage == std::vector<int>{3, 4}));
    j.m_sources[1].m_role = RoleFichier::Aucun;
    VERIFIER(!CreerPartage(j, MethodePartage::ParRoles, 0.5, 0, false).Ok());
}

TEST(aller_retour_json) {
    std::vector<int> labels;
    for (int i = 0; i < 50; ++i) labels.push_back(i % 2);
    JeuDeDonnees j = Jeu(labels, 2);
    auto r = CreerPartage(j, MethodePartage::AleatoireStratifie, 0.7, 0xFFFFFFFFFFFFFFF1ULL, true);
    VERIFIER(r.Ok());
    const Partage& p = r.Valeur();
    auto lu = LireJson(p.VersJson().Ecrire());
    VERIFIER(lu.Ok());
    auto q = PartageDepuisJson(lu.Valeur());
    VERIFIER(q.Ok());
    const Partage& s = q.Valeur();
    VERIFIER(s.m_apprentissage == p.m_apprentissage);
    VERIFIER(s.m_test == p.m_test);
    VERIFIER(s.m_methode == p.m_methode);
    VERIFIER(s.m_ratio == p.m_ratio);
    VERIFIER(s.m_graine == p.m_graine);
    VERIFIER(s.m_inverse == p.m_inverse);
    VERIFIER(s.m_generateur == p.m_generateur);
    VERIFIER(s.m_hashJeu == p.m_hashJeu);
    VERIFIER(s.m_diagnostics == p.m_diagnostics);
    VERIFIER(s.Hash() == p.Hash());
    // alteration d'un indice : le hash ne correspond plus
    ValeurJson altere = p.VersJson();
    altere["hash"] = ValeurJson("0000");
    VERIFIER(!PartageDepuisJson(altere).Ok());
}

TEST(validation) {
    Partage p;
    p.m_apprentissage = {0, 1};
    p.m_test = {2, 3};
    VERIFIER(ValiderPartage(p, 4).Ok());
    VERIFIER(!ValiderPartage(p, 5).Ok());          // union incomplete
    p.m_test = {1, 3};
    VERIFIER(!ValiderPartage(p, 4).Ok());          // non disjoints
    p.m_test = {2, 7};
    VERIFIER(!ValiderPartage(p, 4).Ok());          // hors limites
    p.m_apprentissage = {};
    p.m_test = {0, 1, 2, 3};
    VERIFIER(!ValiderPartage(p, 4).Ok());          // apprentissage vide
}

int main(int argc, char** argv) { return LancerTests(argc, argv); }
