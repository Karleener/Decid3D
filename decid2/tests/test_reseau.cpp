// Tests du reseau pedagogique Decid (noyau/Reseau.h) : gradient analytique contre gradient numerique,
// determinisme, rejeu des poids publies, convergence sur XOR et progression nette sur une spirale.
// Donnees SYNTHETIQUES deterministes (aucun fichier externe).
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "Reseau.h"
#include "Verifier.h"

using namespace decid;

namespace {

struct Corpus {
    std::vector<double> m_x;   // lignes a la suite
    std::vector<int> m_y;
};

double Alea(std::uint64_t& e) {                       // LCG simple, suffisant pour fabriquer des donnees
    e = e * 6364136223846793005ull + 1442695040888963407ull;
    return (double)(e >> 11) * (1.0 / 9007199254740992.0);
}

// XOR : quatre nuages aux coins du carre [0,1]^2, classes en damier
Corpus Xor(int parCoin, std::uint64_t graine) {
    Corpus c;
    const double centres[4][2] = {{0.2, 0.2}, {0.8, 0.8}, {0.2, 0.8}, {0.8, 0.2}};
    for (int k = 0; k < 4; ++k)
        for (int i = 0; i < parCoin; ++i) {
            c.m_x.push_back(centres[k][0] + (Alea(graine) - 0.5) * 0.25);
            c.m_x.push_back(centres[k][1] + (Alea(graine) - 0.5) * 0.25);
            c.m_y.push_back(k < 2 ? 0 : 1);
        }
    return c;
}

// Deux spirales entrelacees (deux tours) dans [0,1]^2
Corpus Spirale(int parClasse, std::uint64_t graine) {
    Corpus c;
    const double pi = 3.14159265358979323846;
    for (int classe = 0; classe < 2; ++classe)
        for (int i = 0; i < parClasse; ++i) {
            const double t = 0.25 + 1.75 * i / parClasse;                      // tours
            const double r = 0.45 * t / 2.0;
            const double angle = 2 * pi * t + classe * pi;
            c.m_x.push_back(0.5 + r * std::cos(angle) + (Alea(graine) - 0.5) * 0.02);
            c.m_x.push_back(0.5 + r * std::sin(angle) + (Alea(graine) - 0.5) * 0.02);
            c.m_y.push_back(classe);
        }
    return c;
}

std::vector<int> Toutes(const Corpus& c) {
    std::vector<int> t;
    for (int i = 0; i < (int)c.m_y.size(); ++i) t.push_back(i);
    return t;
}

double Entrainer(Reseau& r, const ParametresReseau& p, const Corpus& c) {
    EntraineurReseau e(r, p, c.m_x, c.m_y);
    MesuresReseau m;
    for (int i = 0; i < p.m_epoques; ++i) m = e.Epoque();
    return m.m_exactitude;
}

} // namespace

TEST(gradient_analytique_egal_au_gradient_numerique_pour_chaque_activation) {
    const Corpus c = Xor(5, 3);
    for (const char* activation : {"TANH", "SIGMOIDE_SYM", "SIGMOIDE", "IDENTITE", "RELU"}) {
        Reseau r({2, 4, 3, 2}, activation, 11);
        // decaler les biais pour eviter le point anguleux de RELU en 0
        for (auto& w : r.Poids()) for (double& v : w) v += 0.013;
        const double l2 = 0.01;
        const auto g = r.Gradient(c.m_x, c.m_y, Toutes(c), l2);
        double ecartMax = 0;
        for (size_t couche = 0; couche < r.Poids().size(); ++couche)
            for (size_t k = 0; k < r.Poids()[couche].size(); ++k) {
                double& w = r.Poids()[couche][k];
                const double h = 1e-6, sauve = w;
                w = sauve + h; const double plus = r.Mesurer(c.m_x, c.m_y, l2).m_perte;
                w = sauve - h; const double moins = r.Mesurer(c.m_x, c.m_y, l2).m_perte;
                w = sauve;
                ecartMax = std::max(ecartMax, std::fabs((plus - moins) / (2 * h) - g[couche][k]));
            }
        VERIFIER(ecartMax < 1e-6);
        if (ecartMax >= 1e-6) std::printf("  activation %s : ecart %g\n", activation, ecartMax);
    }
}

TEST(meme_graine_memes_poids_et_parametres_comptes) {
    const Corpus c = Xor(10, 5);
    ParametresReseau p;
    p.m_couchesCachees = {5, 4};
    p.m_epoques = 30;
    p.m_methode = "RETROPROPAGATION";
    p.m_eta = 0.1;
    p.m_tailleLot = 7;
    Reseau a({2, 5, 4, 2}, "TANH", 42), b({2, 5, 4, 2}, "TANH", 42), autre({2, 5, 4, 2}, "TANH", 43);
    Entrainer(a, p, c);
    Entrainer(b, p, c);
    VERIFIER(a.PoidsConcatenes() == b.PoidsConcatenes());
    VERIFIER(a.PoidsConcatenes() != autre.PoidsConcatenes());
    VERIFIER_EGAL(a.NbParametres(), (size_t)(3 * 5 + 6 * 4 + 5 * 2));
}

TEST(poids_publies_rejoues_a_l_identique) {
    const Corpus c = Spirale(40, 9);
    Reseau r({2, 6, 2}, "SIGMOIDE_SYM", 3);
    ParametresReseau p;
    p.m_couchesCachees = {6};
    p.m_activation = "SIGMOIDE_SYM";
    p.m_epoques = 10;
    Entrainer(r, p, c);
    const std::vector<float> plat = r.PoidsConcatenes();
    auto relu = Reseau::DepuisPoids({2, 6, 2}, "SIGMOIDE_SYM", plat.data(), plat.size());
    if (!relu.Ok()) { VERIFIER(false); return; }
    for (size_t i = 0; i < c.m_y.size(); ++i)
        VERIFIER_EGAL(relu.Valeur().Classe(&c.m_x[i * 2]), Reseau(r).Classe(&c.m_x[i * 2]));
    VERIFIER(!Reseau::DepuisPoids({2, 6, 2}, "TANH", plat.data(), plat.size() - 1).Ok());
    // SIGMOIDE_SYM : fonction effectivement calculee par OpenCV, 1,7159 tanh(x/3)
    Reseau un({1, 1, 2}, "SIGMOIDE_SYM", 1);
    un.Poids()[0] = {1.0, 0.0};
    std::vector<std::vector<double>> act;
    const double x = 1.5;
    un.Propager(&x, act);
    VERIFIER_PROCHE(act[1][0], 1.7159 * std::tanh(0.5), 1e-12);
}

TEST(xor_appris_par_rprop_et_par_retropropagation) {
    const Corpus app = Xor(30, 17), test = Xor(30, 99);
    for (const char* methode : {"RPROP", "RETROPROPAGATION"}) {
        ParametresReseau p;
        p.m_couchesCachees = {4};
        p.m_methode = methode;
        p.m_eta = 0.3;
        p.m_moment = 0.9;
        p.m_tailleLot = 8;
        p.m_epoques = 300;
        VERIFIER(VerifierParametresReseau(p, 2, 2).Ok());
        Reseau r({2, 4, 2}, "TANH", 7);
        Entrainer(r, p, app);
        const MesuresReseau m = r.Mesurer(test.m_x, test.m_y, 0);
        VERIFIER(m.m_exactitude > 0.97);
        if (m.m_exactitude <= 0.97) std::printf("  %s : exactitude test %g\n", methode, m.m_exactitude);
    }
}

TEST(spirale_progression_nette_grace_a_l_etat_conserve) {
    const Corpus app = Spirale(150, 21), test = Spirale(150, 77);
    ParametresReseau p;
    p.m_couchesCachees = {20, 20};
    p.m_methode = "RPROP";
    p.m_epoques = 600;
    Reseau r({2, 20, 20, 2}, "TANH", 5);
    const double avant = r.Mesurer(test.m_x, test.m_y, 0).m_exactitude;
    Entrainer(r, p, app);
    const double apres = r.Mesurer(test.m_x, test.m_y, 0).m_exactitude;
    std::printf("  spirale synthetique : %.3f -> %.3f\n", avant, apres);
    VERIFIER(apres > 0.95);
    VERIFIER(apres > avant + 0.3);
}

TEST(parametres_refuses_explicitement) {
    ParametresReseau p;
    p.m_activation = "SOFTPLUS";
    VERIFIER(!VerifierParametresReseau(p, 2, 2).Ok());
    p = ParametresReseau{};
    p.m_couchesCachees = {0};
    VERIFIER(!VerifierParametresReseau(p, 2, 2).Ok());
    p = ParametresReseau{};
    p.m_methode = "ADAM";
    VERIFIER(!VerifierParametresReseau(p, 2, 2).Ok());
    p = ParametresReseau{};
    VERIFIER(!VerifierParametresReseau(p, 2, 1).Ok());
    VERIFIER(VerifierParametresReseau(ParametresReseau{}, 3, 4).Ok());
}

int main(int argc, char** argv) { return LancerTests(argc, argv); }
