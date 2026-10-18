// Tests de Densites : Bayes 1D (seuils, erreur), Parzen, k-PPV. Valeurs verifiees analytiquement ou a la main.
#include <cmath>
#include <string>
#include <vector>

#include "Densites.h"
#include "Verifier.h"

using namespace decid;

namespace {

const double PI = 3.14159265358979323846;

// Deux classes d'ecart-type estime exactement 1 : {m - s, m + s}, s = 1/sqrt(2) (variance n-1 = 2 s^2 = 1)
ModeleBayes1D DeuxGaussiennes(const std::vector<double>& apriori, const std::vector<std::vector<double>>& couts) {
    const double s = 1.0 / std::sqrt(2.0);
    std::vector<double> v = {-1 - s, -1 + s, 1 - s, 1 + s};
    std::vector<int> c = {0, 0, 1, 1};
    auto r = AjusterBayes1D(v, c, 2, apriori, couts);
    VERIFIER(r.Ok());
    return r.Ok() ? r.Valeur() : ModeleBayes1D{};
}

// Formule de Bayes : P(C_0|x) = Pr_0 f_0 / (Pr_0 f_0 + Pr_1 f_1)
double Posteriori(double pr0, double f0, double pr1, double f1) { return pr0 * f0 / (pr0 * f0 + pr1 * f1); }

int NbMaxima(const std::vector<double>& y) {
    int n = 0;
    for (size_t i = 1; i + 1 < y.size(); ++i) if (y[i] > y[i - 1] && y[i] >= y[i + 1]) ++n;
    return n;
}

} // namespace

TEST(grille_bornes_et_validation) {
    auto g = Grille(-2.0, 3.0, 11);
    VERIFIER_EGAL(g.size(), (size_t)11);
    VERIFIER(g.front() == -2.0 && g.back() == 3.0);
    VERIFIER_PROCHE(g[1], -1.5, 1e-12);
    VERIFIER(Grille(0, 1, 1).empty());
    VERIFIER(Grille(1, 1, 5).empty());
    VERIFIER(Grille(2, 1, 5).empty());
    VERIFIER_EGAL(Grille(0, 1, 2).size(), (size_t)2);
}

TEST(ajuster_validations) {
    std::vector<double> v = {0, 1, 5, 6};
    std::vector<int> c = {0, 0, 1, 1};
    auto ok = AjusterBayes1D(v, c, 2, {}, {});
    VERIFIER(ok.Ok());
    if (ok.Ok()) {
        VERIFIER_PROCHE(ok.Valeur().m_moyennes[0], 0.5, 1e-12);
        VERIFIER_PROCHE(ok.Valeur().m_ecartsTypes[0], std::sqrt(0.5), 1e-12);   // (0.25+0.25)/(2-1)
        VERIFIER_PROCHE(ok.Valeur().m_aPriori[1], 0.5, 1e-12);
        VERIFIER(ok.Valeur().m_couts[0][1] == 1.0 && ok.Valeur().m_couts[1][1] == 0.0);
        VERIFIER(ok.Valeur().VersJson().Contient("ecartsTypes"));
    }
    VERIFIER(!AjusterBayes1D({0, 1, 5}, {0, 0, 1}, 2, {}, {}).Ok());                  // 1 seul echantillon
    VERIFIER(!AjusterBayes1D({1, 1, 5, 6}, {0, 0, 1, 1}, 2, {}, {}).Ok());            // ecart-type nul
    VERIFIER(!AjusterBayes1D(v, c, 2, {0.5, 0.6}, {}).Ok());                           // somme != 1
    VERIFIER(!AjusterBayes1D(v, c, 2, {1.0, 0.0}, {}).Ok());                           // a priori nul
    VERIFIER(AjusterBayes1D(v, c, 2, {0.3, 0.7}, {}).Ok());
    VERIFIER(!AjusterBayes1D(v, c, 2, {}, {{0, 1}, {1, 1}}).Ok());                     // diagonale non nulle
    VERIFIER(!AjusterBayes1D(v, c, 2, {}, {{0, -1}, {1, 0}}).Ok());                    // cout negatif
    VERIFIER(!AjusterBayes1D(v, c, 2, {}, {{0, 1}}).Ok());                             // pas K x K
    VERIFIER(AjusterBayes1D(v, c, 2, {}, {{0, 10}, {1, 0}}).Ok());
    VERIFIER(!AjusterBayes1D(v, {0, 0, 1, 2}, 2, {}, {}).Ok());                        // classe hors bornes
}

TEST(bayes_symetrique_seuil_et_erreur) {
    ModeleBayes1D m = DeuxGaussiennes({0.5, 0.5}, {});
    VERIFIER_PROCHE(m.m_moyennes[0], -1.0, 1e-12);
    VERIFIER_PROCHE(m.m_moyennes[1], 1.0, 1e-12);
    VERIFIER_PROCHE(m.m_ecartsTypes[0], 1.0, 1e-12);
    CourbesBayes1D c = CalculerCourbesBayes1D(m, Grille(-8, 8, 4001));
    VERIFIER_EGAL(c.m_seuils.size(), (size_t)1);
    if (!c.m_seuils.empty()) VERIFIER_PROCHE(c.m_seuils[0], 0.0, 1e-3);
    // erreur de Bayes = Phi(-1) = 0.158655
    VERIFIER_PROCHE(c.m_erreurBayes, 0.158655, 1e-3);
    VERIFIER_EGAL(c.m_decision.front(), 0);
    VERIFIER_EGAL(c.m_decision.back(), 1);
    for (size_t i = 0; i < c.m_x.size(); i += 500)
        VERIFIER_PROCHE(c.m_posteriori[0][i] + c.m_posteriori[1][i], 1.0, 1e-12);
    std::printf("  seuil = %.6f, erreur de Bayes = %.6f\n", c.m_seuils.empty() ? 0.0 : c.m_seuils[0], c.m_erreurBayes);
    VERIFIER(c.VersJson().Contient("erreurBayes"));
    VERIFIER(!c.VersJson().Ecrire().empty());
}

TEST(bayes_apriori_deplace_le_seuil) {
    // Decision classe 1 si 0.7 f1 > 0.3 f0 <=> ln(0.7/0.3) + 2x > 0 <=> x > ln(0.3/0.7)/2 = -0.42365 (vers la classe 0, la moins probable)
    ModeleBayes1D m = DeuxGaussiennes({0.3, 0.7}, {});
    CourbesBayes1D c = CalculerCourbesBayes1D(m, Grille(-8, 8, 4001));
    const double attendu = std::log(0.3 / 0.7) / 2.0;
    VERIFIER_EGAL(c.m_seuils.size(), (size_t)1);
    if (!c.m_seuils.empty()) {
        VERIFIER_PROCHE(c.m_seuils[0], attendu, 1e-3);
        VERIFIER(c.m_seuils[0] < 0);
    }
    std::printf("  seuil a priori 0.3/0.7 = %.6f (attendu %.6f)\n", c.m_seuils.empty() ? 0.0 : c.m_seuils[0], attendu);
}

TEST(bayes_couts_asymetriques) {
    // cout[0][1] = 10 : decider 1 si 10 P0 < P1 <=> f1/f0 = exp(2x) > 10 <=> x > ln(10)/2 = 1.1513
    ModeleBayes1D m = DeuxGaussiennes({0.5, 0.5}, {{0, 10}, {1, 0}});
    CourbesBayes1D c = CalculerCourbesBayes1D(m, Grille(-8, 8, 4001));
    VERIFIER_EGAL(c.m_seuils.size(), (size_t)1);
    if (!c.m_seuils.empty()) {
        VERIFIER_PROCHE(c.m_seuils[0], std::log(10.0) / 2.0, 1e-3);
        VERIFIER(c.m_seuils[0] > 0);
    }
    // cout[1][0] = 10 : decider 0 coute cher quand la verite est 1 -> seuil vers la gauche (-1.1513)
    ModeleBayes1D m2 = DeuxGaussiennes({0.5, 0.5}, {{0, 1}, {10, 0}});
    CourbesBayes1D c2 = CalculerCourbesBayes1D(m2, Grille(-8, 8, 4001));
    VERIFIER_EGAL(c2.m_seuils.size(), (size_t)1);
    if (!c2.m_seuils.empty()) VERIFIER_PROCHE(c2.m_seuils[0], -std::log(10.0) / 2.0, 1e-3);
}

TEST(bayes_exemple_femmes_hommes) {
    // Taille 1,60 m : f_F = 2,38 et f_H = 1,61. On fabrique des gaussiennes qui donnent ces densites en 1,60.
    const double sF = 1.0 / (2.38 * std::sqrt(2.0 * PI));                       // moyenne 1,60 -> f_F(1,60) = 2,38
    const double sH = 0.1;
    const double d = std::sqrt(-2.0 * sH * sH * std::log(1.61 * sH * std::sqrt(2.0 * PI)));
    ModeleBayes1D m;
    m.m_moyennes = {1.60, 1.60 + d};
    m.m_ecartsTypes = {sF, sH};
    m.m_couts = {{0, 1}, {1, 0}};
    std::vector<double> x = {1.60};

    m.m_aPriori = {0.5, 0.5};
    CourbesBayes1D c = CalculerCourbesBayes1D(m, x);
    VERIFIER_PROCHE(c.m_densites[0][0], 2.38, 1e-9);
    VERIFIER_PROCHE(c.m_densites[1][0], 1.61, 1e-9);
    VERIFIER_PROCHE(c.m_posteriori[0][0], Posteriori(0.5, 2.38, 0.5, 1.61), 1e-9);
    VERIFIER_PROCHE(c.m_posteriori[0][0], 0.60, 0.01);          // ~ 60 %
    VERIFIER_PROCHE(c.m_posteriori[1][0], 0.40, 0.01);          // ~ 40 %

    m.m_aPriori = {0.3, 0.7};
    c = CalculerCourbesBayes1D(m, x);
    VERIFIER_PROCHE(c.m_posteriori[0][0], 0.3878, 1e-3);         // 0,3*2,38 / (0,3*2,38 + 0,7*1,61)
    VERIFIER_PROCHE(c.m_posteriori[0][0], 0.39, 0.01);          // ~ 39 %
    VERIFIER_PROCHE(c.m_posteriori[1][0], 0.61, 0.01);          // ~ 61 %
    VERIFIER_EGAL(c.m_decision[0], 1);                           // la classe la plus probable a posteriori l'emporte
}

TEST(bayes_densites_nulles_posteriori_egal_apriori) {
    ModeleBayes1D m;
    m.m_moyennes = {0, 1};
    m.m_ecartsTypes = {1e-3, 1e-3};
    m.m_aPriori = {0.25, 0.75};
    m.m_couts = {{0, 1}, {1, 0}};
    CourbesBayes1D c = CalculerCourbesBayes1D(m, {1000.0});      // densites sous-flottantes -> 0
    VERIFIER(c.m_densites[0][0] == 0.0 && c.m_densites[1][0] == 0.0);
    VERIFIER_PROCHE(c.m_posteriori[0][0], 0.25, 1e-15);
    VERIFIER_PROCHE(c.m_posteriori[1][0], 0.75, 1e-15);
    VERIFIER_EGAL(c.m_decision[0], 1);
}

TEST(bayes_egalite_plus_petit_id) {
    ModeleBayes1D m;
    m.m_moyennes = {0, 0};
    m.m_ecartsTypes = {1, 1};
    m.m_aPriori = {0.5, 0.5};
    m.m_couts = {{0, 1}, {1, 0}};
    CourbesBayes1D c = CalculerCourbesBayes1D(m, Grille(-1, 1, 5));
    for (int d : c.m_decision) VERIFIER_EGAL(d, 0);
    VERIFIER(c.m_seuils.empty());
}

TEST(parzen_integrale_et_contributions) {
    std::vector<double> s = {-2, 0, 1, 3};
    std::vector<double> g = Grille(-10, 10, 20001);
    for (const char* noyau : {"RECTANGLE", "GAUSSIEN"}) {
        auto r = Parzen1D(s, 1.0, noyau, g, true);
        VERIFIER(r.Ok());
        if (!r.Ok()) continue;
        VERIFIER_PROCHE(r.Valeur().m_integrale, 1.0, 2e-3);
        VERIFIER_EGAL(r.Valeur().m_contributions.size(), s.size());
        double ecartMax = 0;
        for (size_t i = 0; i < g.size(); i += 97) {
            double somme = 0;
            for (const auto& cb : r.Valeur().m_contributions) somme += cb[i];
            ecartMax = std::fmax(ecartMax, std::fabs(somme - r.Valeur().m_densite[i]));
        }
        VERIFIER(ecartMax < 1e-12);
        std::printf("  %s : integrale = %.6f\n", noyau, r.Valeur().m_integrale);
    }
    auto sans = Parzen1D(s, 1.0, "GAUSSIEN", g, false);
    VERIFIER(sans.Ok() && sans.Valeur().m_contributions.empty());
    // valeur a la main : un echantillon en 0, h = 2, rectangle : densite = 1/h = 0,5 pour |x| <= 1
    auto un = Parzen1D({0.0}, 2.0, "RECTANGLE", {0.0, 1.0, 1.5}, false);
    VERIFIER(un.Ok());
    if (un.Ok()) {
        VERIFIER_PROCHE(un.Valeur().m_densite[0], 0.5, 1e-12);
        VERIFIER_PROCHE(un.Valeur().m_densite[1], 0.5, 1e-12);
        VERIFIER_PROCHE(un.Valeur().m_densite[2], 0.0, 1e-12);
    }
}

TEST(parzen_effet_de_h) {
    std::vector<double> s = {-3, -1, 1, 3};
    std::vector<double> g = Grille(-10, 10, 4001);
    auto grand = Parzen1D(s, 3.0, "GAUSSIEN", g, false);
    auto petit = Parzen1D(s, 0.2, "GAUSSIEN", g, false);
    VERIFIER(grand.Ok() && petit.Ok());
    if (grand.Ok() && petit.Ok()) {
        VERIFIER_EGAL(NbMaxima(grand.Valeur().m_densite), 1);   // h grand : courbe lisse
        VERIFIER_EGAL(NbMaxima(petit.Valeur().m_densite), 4);   // h petit : un pic par echantillon
    }
}

TEST(parzen_erreurs) {
    std::vector<double> g = Grille(0, 1, 5);
    VERIFIER(!Parzen1D({0.0}, 0.0, "GAUSSIEN", g, false).Ok());
    VERIFIER(!Parzen1D({0.0}, -1.0, "GAUSSIEN", g, false).Ok());
    VERIFIER(!Parzen1D({0.0}, 1.0, "TRIANGLE", g, false).Ok());
    VERIFIER(!Parzen1D({0.0}, 1.0, "gaussien", g, false).Ok());
    VERIFIER(!Parzen1D({}, 1.0, "GAUSSIEN", g, false).Ok());
}

TEST(kppv_valeurs_a_la_main) {
    // echantillons {0, 1, 3}, k = 2, N = 3 ; p = k / (N * 2 d_k)
    //  x = 2   : distances 2,1,1 -> d_2 = 1 -> p = 2/(3*2)  = 1/3
    //  x = 0,5 : distances 0,5 ; 0,5 ; 2,5 -> d_2 = 0,5 -> p = 2/(3*1) = 2/3
    //  x = 5   : distances 5,4,2 -> d_2 = 4 -> p = 2/(3*8) = 1/12
    auto r = DensiteKppv1D({0.0, 1.0, 3.0}, 2, {2.0, 0.5, 5.0});
    VERIFIER(r.Ok());
    if (r.Ok()) {
        VERIFIER_PROCHE(r.Valeur()[0], 1.0 / 3.0, 1e-12);
        VERIFIER_PROCHE(r.Valeur()[1], 2.0 / 3.0, 1e-12);
        VERIFIER_PROCHE(r.Valeur()[2], 1.0 / 12.0, 1e-12);
    }
}

TEST(kppv_erreurs) {
    std::vector<double> e = {0.0, 1.0, 3.0};
    VERIFIER(!DensiteKppv1D(e, 0, {2.0}).Ok());
    VERIFIER(!DensiteKppv1D(e, 4, {2.0}).Ok());
    VERIFIER(!DensiteKppv1D(e, -1, {2.0}).Ok());
    VERIFIER(!DensiteKppv1D({}, 1, {2.0}).Ok());
    VERIFIER(!DensiteKppv1D(e, 1, {1.0}).Ok());      // d_1 = 0 : densite non definie
    VERIFIER(DensiteKppv1D(e, 3, {1.0}).Ok());       // d_3 = 2 > 0
}

int main(int argc, char** argv) { return LancerTests(argc, argv); }
