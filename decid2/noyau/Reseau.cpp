// Reseau de neurones pedagogique Decid : propagation, retropropagation, RPROP (iRprop-) et mini-lots avec moment.
// Calcul en double, sequentiel et deterministe (meme graine -> memes poids, a l'octet pres).
#include "Reseau.h"

#include <algorithm>
#include <cmath>

namespace decid {

namespace {

const double RPROP_PAS_INITIAL = 0.1, RPROP_PAS_MIN = 1e-6, RPROP_PAS_MAX = 50.0;
const double RPROP_HAUSSE = 1.2, RPROP_BAISSE = 0.5;
const double BETA_SYM = 1.7159;

// splitmix64 : meme generateur que les partages
std::uint64_t Suivant(std::uint64_t& etat) {
    std::uint64_t z = (etat += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
double Uniforme(std::uint64_t& etat) { return (double)(Suivant(etat) >> 11) * (1.0 / 9007199254740992.0); }
int Entier(std::uint64_t& etat, int n) {                    // tirage sans biais dans [0, n) (rejet)
    const std::uint64_t borne = ~0ull - (~0ull % (std::uint64_t)n);
    std::uint64_t v;
    do { v = Suivant(etat); } while (v >= borne);
    return (int)(v % (std::uint64_t)n);
}

bool ActivationConnue(const std::string& a) {
    return a == "TANH" || a == "SIGMOIDE_SYM" || a == "SIGMOIDE" || a == "RELU" || a == "IDENTITE";
}

void Softmax(std::vector<double>& z) {
    const double m = *std::max_element(z.begin(), z.end());
    double s = 0;
    for (double& v : z) { v = std::exp(v - m); s += v; }
    for (double& v : z) v /= s;
}

int Argmax(const std::vector<double>& v) {
    return (int)(std::max_element(v.begin(), v.end()) - v.begin());   // egalite : plus petit indice
}

} // namespace

Statut VerifierParametresReseau(const ParametresReseau& p, int nbEntrees, int nbClasses) {
    auto refus = [](const std::string& m) { return FaireErreur(CodeErreur::DonneesInvalides, "apprentissage_reseau", m); };
    if (nbEntrees < 1) return refus("au moins un attribut est necessaire");
    if (nbClasses < 2) return refus("au moins deux classes sont necessaires");
    if (!ActivationConnue(p.m_activation))
        return refus("activation inconnue : " + p.m_activation + " (TANH, SIGMOIDE_SYM, SIGMOIDE, RELU, IDENTITE)");
    if (p.m_methode != "RPROP" && p.m_methode != "RETROPROPAGATION")
        return refus("methode inconnue : " + p.m_methode + " (RPROP, RETROPROPAGATION)");
    if (p.m_couchesCachees.size() > 8) return refus("au plus 8 couches cachees");
    for (int n : p.m_couchesCachees)
        if (n < 1 || n > 256) return refus("chaque couche cachee doit avoir entre 1 et 256 neurones");
    if (!(p.m_eta > 0 && p.m_eta <= 10)) return refus("eta doit etre dans ]0, 10]");
    if (!(p.m_moment >= 0 && p.m_moment < 1)) return refus("moment doit etre dans [0, 1[");
    if (p.m_tailleLot < 1 || p.m_tailleLot > 100000) return refus("taille_lot entre 1 et 100000");
    if (!(p.m_l2 >= 0 && p.m_l2 <= 1)) return refus("l2 doit etre dans [0, 1]");
    if (p.m_epoques < 1 || p.m_epoques > 100000) return refus("iterations_max entre 1 et 100000");
    if (!(p.m_epsilon >= 0) || !std::isfinite(p.m_epsilon)) return refus("epsilon doit etre fini et >= 0");
    return Succes();
}

// ---------------------------------------------------------------- Reseau

Reseau::Reseau(const std::vector<int>& tailles, const std::string& activation, std::uint64_t graine)
    : m_tailles(tailles), m_activation(activation) {
    std::uint64_t etat = graine;
    for (int c = 1; c < (int)m_tailles.size(); ++c) {
        const int e = m_tailles[(size_t)c - 1], s = m_tailles[(size_t)c];
        const bool relu = activation == "RELU" && c < (int)m_tailles.size() - 1;
        const double limite = relu ? std::sqrt(6.0 / e) : std::sqrt(6.0 / (e + s));
        std::vector<double> w((size_t)(e + 1) * s, 0.0);
        for (int i = 0; i < e; ++i)
            for (int j = 0; j < s; ++j) w[(size_t)i * s + j] = (2 * Uniforme(etat) - 1) * limite;
        m_poids.push_back(std::move(w));                      // derniere ligne (biais) a zero
    }
}

Resultat<Reseau> Reseau::DepuisPoids(const std::vector<int>& tailles, const std::string& activation,
                                     const float* poids, std::size_t nbPoids) {
    if (tailles.size() < 2 || !ActivationConnue(activation))
        return FaireErreur(CodeErreur::ModeleIncompatible, "reseau", "architecture ou activation invalide");
    Reseau r(tailles, activation, 0);
    if (r.NbParametres() != nbPoids)
        return FaireErreur(CodeErreur::ModeleIncompatible, "reseau",
                           "nombre de poids incoherent : " + std::to_string(nbPoids) + " lus, " +
                           std::to_string(r.NbParametres()) + " attendus");
    std::size_t k = 0;
    for (auto& w : r.m_poids)
        for (double& v : w) v = poids[k++];
    return r;
}

double Reseau::F(double z) const {
    if (m_activation == "TANH") return std::tanh(z);
    if (m_activation == "SIGMOIDE_SYM") return BETA_SYM * std::tanh(z / 3.0);
    if (m_activation == "SIGMOIDE") return 1.0 / (1.0 + std::exp(-z));
    if (m_activation == "RELU") return z > 0 ? z : 0.0;
    return z;
}

double Reseau::DeriveeF(double z, double a) const {
    if (m_activation == "TANH") return 1.0 - a * a;
    if (m_activation == "SIGMOIDE_SYM") { const double t = a / BETA_SYM; return BETA_SYM / 3.0 * (1.0 - t * t); }
    if (m_activation == "SIGMOIDE") return a * (1.0 - a);
    if (m_activation == "RELU") return z > 0 ? 1.0 : 0.0;
    return 1.0;
}

void Reseau::Propager(const double* x, std::vector<std::vector<double>>& a) const {
    const int L = NbCouches();
    a.resize((size_t)L + 1);
    a[0].assign(x, x + m_tailles[0]);
    for (int c = 1; c <= L; ++c) {
        const int e = m_tailles[(size_t)c - 1], s = m_tailles[(size_t)c];
        const std::vector<double>& w = m_poids[(size_t)c - 1];
        std::vector<double>& z = a[(size_t)c];
        z.assign(w.begin() + (std::ptrdiff_t)e * s, w.end());          // biais
        for (int i = 0; i < e; ++i) {
            const double v = a[(size_t)c - 1][(size_t)i];
            const double* ligne = &w[(size_t)i * s];
            for (int j = 0; j < s; ++j) z[(size_t)j] += v * ligne[j];
        }
        if (c == L) Softmax(z);
        else for (double& v : z) v = F(v);
    }
}

int Reseau::Classe(const double* x) const {
    std::vector<std::vector<double>> a;
    Propager(x, a);
    return Argmax(a.back());
}

MesuresReseau Reseau::Mesurer(const std::vector<double>& x, const std::vector<int>& y, double l2) const {
    MesuresReseau m;
    if (y.empty()) return m;
    std::vector<std::vector<double>> a;
    long justes = 0;
    for (size_t n = 0; n < y.size(); ++n) {
        Propager(&x[n * (size_t)NbEntrees()], a);
        m.m_perte -= std::log(std::max(a.back()[(size_t)y[n]], 1e-300));
        if (Argmax(a.back()) == y[n]) ++justes;
    }
    m.m_perte /= (double)y.size();
    if (l2 > 0) {
        double s = 0;
        for (int c = 1; c <= NbCouches(); ++c) {
            const int e = m_tailles[(size_t)c - 1], sc = m_tailles[(size_t)c];
            for (int k = 0; k < e * sc; ++k) s += m_poids[(size_t)c - 1][(size_t)k] * m_poids[(size_t)c - 1][(size_t)k];
        }
        m.m_perte += 0.5 * l2 * s;
    }
    m.m_exactitude = (double)justes / (double)y.size();
    return m;
}

std::vector<std::vector<double>> Reseau::Gradient(const std::vector<double>& x, const std::vector<int>& y,
                                                  const std::vector<int>& lignes, double l2) const {
    const int L = NbCouches();
    std::vector<std::vector<double>> g;
    for (const auto& w : m_poids) g.emplace_back(w.size(), 0.0);
    std::vector<std::vector<double>> a, delta((size_t)L + 1);
    for (int n : lignes) {
        Propager(&x[(size_t)n * NbEntrees()], a);
        delta[(size_t)L] = a[(size_t)L];                               // softmax + entropie croisee
        delta[(size_t)L][(size_t)y[(size_t)n]] -= 1.0;
        for (int c = L; c >= 1; --c) {
            const int e = m_tailles[(size_t)c - 1], s = m_tailles[(size_t)c];
            std::vector<double>& gc = g[(size_t)c - 1];
            const std::vector<double>& d = delta[(size_t)c];
            for (int i = 0; i < e; ++i) {
                const double v = a[(size_t)c - 1][(size_t)i];
                for (int j = 0; j < s; ++j) gc[(size_t)i * s + j] += v * d[(size_t)j];
            }
            for (int j = 0; j < s; ++j) gc[(size_t)e * s + j] += d[(size_t)j];
            if (c == 1) break;
            std::vector<double>& dp = delta[(size_t)c - 1];
            dp.assign((size_t)e, 0.0);
            const std::vector<double>& w = m_poids[(size_t)c - 1];
            for (int i = 0; i < e; ++i) {
                double s2 = 0;
                for (int j = 0; j < s; ++j) s2 += w[(size_t)i * s + j] * d[(size_t)j];
                // la derivee s'exprime avec l'activation ; pour RELU, a > 0 equivaut a z > 0
                const double ai = a[(size_t)c - 1][(size_t)i];
                dp[(size_t)i] = s2 * DeriveeF(ai, ai);
            }
        }
    }
    const double inv = lignes.empty() ? 0.0 : 1.0 / (double)lignes.size();
    for (int c = 1; c <= L; ++c) {
        const int e = m_tailles[(size_t)c - 1], s = m_tailles[(size_t)c];
        std::vector<double>& gc = g[(size_t)c - 1];
        for (size_t k = 0; k < gc.size(); ++k) gc[k] *= inv;
        if (l2 > 0) for (int k = 0; k < e * s; ++k) gc[(size_t)k] += l2 * m_poids[(size_t)c - 1][(size_t)k];
    }
    return g;
}

std::vector<float> Reseau::PoidsConcatenes() const {
    std::vector<float> v;
    v.reserve(NbParametres());
    for (const auto& w : m_poids) for (double p : w) v.push_back((float)p);
    return v;
}

std::size_t Reseau::NbParametres() const {
    std::size_t n = 0;
    for (const auto& w : m_poids) n += w.size();
    return n;
}

ValeurJson Reseau::Inspection() const {
    ValeurJson j = ValeurJson::Objet();
    j["schema"] = "decid.inspection/1";
    j["famille"] = "mlp";
    j["implementation"] = "decid (pedagogique)";
    j["espace"] = "espace du modele (apres pretraitement)";
    j["convention"] = "couche c : a_c = f(poids^T [a_(c-1); 1]), poids en (entrees+1) x neurones, derniere ligne = biais ; "
                      "sortie : softmax ; classe = sortie maximale";
    j["activation_cachee"] = m_activation;
    j["activation_sortie"] = "SOFTMAX";
    ValeurJson tailles = ValeurJson::Tableau();
    for (int t : m_tailles) tailles.Ajouter(t);
    j["tailles"] = tailles;
    ValeurJson couches = ValeurJson::Tableau();
    for (int c = 1; c <= NbCouches(); ++c) {
        const int e = m_tailles[(size_t)c - 1], s = m_tailles[(size_t)c];
        ValeurJson couche = ValeurJson::Objet();
        couche["couche"] = c;
        couche["entrees"] = e;
        ValeurJson lignes = ValeurJson::Tableau();
        for (int i = 0; i <= e; ++i) {
            ValeurJson l = ValeurJson::Tableau();
            for (int k = 0; k < s; ++k) l.Ajouter(m_poids[(size_t)c - 1][(size_t)i * s + k]);
            lignes.Ajouter(l);
        }
        couche["poids"] = lignes;
        couches.Ajouter(couche);
    }
    j["couches"] = couches;
    return j;
}

// ---------------------------------------------------------------- EntraineurReseau

EntraineurReseau::EntraineurReseau(Reseau& reseau, const ParametresReseau& p, const std::vector<double>& x,
                                   const std::vector<int>& y)
    : m_reseau(reseau), m_p(p), m_x(x), m_y(y), m_etatAlea(p.m_graine ^ 0x5DEECE66Dull) {
    for (int i = 0; i < (int)y.size(); ++i) m_toutes.push_back(i);
    for (const auto& w : reseau.Poids()) {
        m_pas.emplace_back(w.size(), RPROP_PAS_INITIAL);
        m_gradientPrecedent.emplace_back(w.size(), 0.0);
        m_vitesse.emplace_back(w.size(), 0.0);
    }
}

void EntraineurReseau::PasRprop() {
    const auto g = m_reseau.Gradient(m_x, m_y, m_toutes, m_p.m_l2);
    auto& poids = m_reseau.Poids();
    for (size_t c = 0; c < poids.size(); ++c)
        for (size_t k = 0; k < poids[c].size(); ++k) {
            double gk = g[c][k];
            const double produit = gk * m_gradientPrecedent[c][k];
            if (produit > 0) m_pas[c][k] = std::min(m_pas[c][k] * RPROP_HAUSSE, RPROP_PAS_MAX);
            else if (produit < 0) { m_pas[c][k] = std::max(m_pas[c][k] * RPROP_BAISSE, RPROP_PAS_MIN); gk = 0; }
            if (gk > 0) poids[c][k] -= m_pas[c][k];
            else if (gk < 0) poids[c][k] += m_pas[c][k];
            m_gradientPrecedent[c][k] = gk;
        }
}

void EntraineurReseau::PasRetropropagation() {
    std::vector<int> ordre = m_toutes;                         // melange Fisher-Yates a chaque epoque
    for (int i = (int)ordre.size() - 1; i > 0; --i) std::swap(ordre[(size_t)i], ordre[(size_t)Entier(m_etatAlea, i + 1)]);
    auto& poids = m_reseau.Poids();
    for (size_t debut = 0; debut < ordre.size(); debut += (size_t)m_p.m_tailleLot) {
        const size_t fin = std::min(ordre.size(), debut + (size_t)m_p.m_tailleLot);
        const std::vector<int> lot(ordre.begin() + (std::ptrdiff_t)debut, ordre.begin() + (std::ptrdiff_t)fin);
        const auto g = m_reseau.Gradient(m_x, m_y, lot, m_p.m_l2);
        for (size_t c = 0; c < poids.size(); ++c)
            for (size_t k = 0; k < poids[c].size(); ++k) {
                m_vitesse[c][k] = m_p.m_moment * m_vitesse[c][k] - m_p.m_eta * g[c][k];
                poids[c][k] += m_vitesse[c][k];
            }
    }
}

MesuresReseau EntraineurReseau::Epoque() {
    if (m_p.m_methode == "RPROP") PasRprop();
    else PasRetropropagation();
    return m_reseau.Mesurer(m_x, m_y, m_p.m_l2);
}

} // namespace decid
