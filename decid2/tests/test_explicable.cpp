// Tests du moteur explicable : AdaBoost, perceptron, k-means (implementations pedagogiques Decid).
// Donnees SYNTHETIQUES generees de facon deterministe (LCG maison).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

#include "Explicable.h"
#include "Verifier.h"

using namespace decid;

namespace {

struct Lcg {
    std::uint64_t m_etat;
    double Suivant() {
        m_etat = m_etat * 6364136223846793005ULL + 1442695040888963407ULL;
        return (double)(m_etat >> 11) / 9007199254740992.0;
    }
    double Bruit() { return Suivant() + Suivant() + Suivant() - 1.5; }   // ~ cloche, ecart-type 0.5
};

struct Corpus {
    cv::Mat m_x;                 // CV_32F
    std::vector<int> m_labels;
};

// Nuages 2D : centres donnes, n points par classe, bruit d'amplitude `echelle`.
Corpus Nuages(const std::vector<std::pair<double, double>>& centres, int n, std::uint64_t graine, double echelle = 1.0) {
    Corpus c;
    c.m_x = cv::Mat((int)centres.size() * n, 2, CV_32F);
    Lcg g{graine};
    int ligne = 0;
    for (size_t k = 0; k < centres.size(); ++k)
        for (int i = 0; i < n; ++i, ++ligne) {
            c.m_x.at<float>(ligne, 0) = (float)(centres[k].first + echelle * g.Bruit());
            c.m_x.at<float>(ligne, 1) = (float)(centres[k].second + echelle * g.Bruit());
            c.m_labels.push_back((int)k);
        }
    return c;
}

std::vector<int> PlusMoins(const std::vector<int>& labels, int classePositive) {
    std::vector<int> y(labels.size());
    for (size_t i = 0; i < labels.size(); ++i) y[i] = labels[i] == classePositive ? 1 : -1;
    return y;
}

template <class T>
bool Reussi(const Resultat<T>& r, const char* contexte) {
    if (!r.Ok()) verif::Echec(__FILE__, __LINE__, std::string(contexte) + " : " + r.Err().Texte());
    return r.Ok();
}

template <class T>
bool Refuse(const Resultat<T>& r, const char* contexte) {
    if (r.Ok()) verif::Echec(__FILE__, __LINE__, std::string(contexte) + " : succes inattendu, une erreur etait attendue");
    return !r.Ok();
}

double SommePoids(const AdaBoostPasAPas& a) {
    double s = 0;
    for (double d : a.Poids()) s += d;
    return s;
}

// Itere jusqu'a `max` fois ou arret ; renvoie le nombre d'iterations ajoutees.
int Iterer(AdaBoostPasAPas& a, int max, ObservateurTrace* obs = nullptr) {
    for (int t = 0; t < max; ++t) {
        auto r = a.Iterer(obs);
        if (!Reussi(r, "Iterer") || !r.Valeur()) break;
    }
    return (int)a.Iterations().size();
}

double ErreurFort(const AdaBoostPasAPas& a, const Corpus& c, const std::vector<int>& y) {
    int faux = 0;
    for (int i = 0; i < c.m_x.rows; ++i) faux += a.Decider(c.m_x.ptr<float>(i)) != y[(size_t)i] ? 1 : 0;
    return (double)faux / (double)c.m_x.rows;
}

// ---- jeux de donnees des tests AdaBoost ----

Corpus QuatrePoints(std::vector<int>& y) {
    Corpus c;
    c.m_x = (cv::Mat_<float>(4, 1) << 1, 2, 3, 4);
    y = {1, -1, -1, 1};
    return c;
}

// Classe + : (t, t+1), classe - : (t, t-1), t dans [-5,5] : separables par une diagonale, pas par un axe.
Corpus Diagonale(std::vector<int>& y) {
    Corpus c;
    const int n = 60;
    c.m_x = cv::Mat(2 * n, 2, CV_32F);
    Lcg g{5};
    for (int i = 0; i < 2 * n; ++i) {
        double t = -5.0 + 10.0 * g.Suivant();
        double s = (i < n) ? 1.0 : -1.0;
        c.m_x.at<float>(i, 0) = (float)t;
        c.m_x.at<float>(i, 1) = (float)(t + s + 0.2 * g.Bruit());
        y.push_back(i < n ? 1 : -1);
    }
    return c;
}

// XOR a 4 groupes : (0,0) et (4,4) positifs, (0,4) et (4,0) negatifs.
Corpus Xor(std::vector<int>& y, double echelle) {
    Corpus c = Nuages({{0, 0}, {4, 4}, {0, 4}, {4, 0}}, 30, 3, echelle);
    for (int k : c.m_labels) y.push_back(k < 2 ? 1 : -1);
    return c;
}

} // namespace

TEST(adaboost_seuil_nuages_separables) {
    Corpus c = Nuages({{0.0, 0.0}, {6.0, 0.0}}, 40, 7, 0.5);
    std::vector<int> y = PlusMoins(c.m_labels, 1);
    AdaBoostPasAPas a;
    VERIFIER(Reussi(a.Initialiser(c.m_x, y, TypeFaible::Seuil), "Initialiser"));
    { auto r1 = a.Iterer(nullptr); VERIFIER(Reussi(r1, "Iterer")); }
    VERIFIER(!a.Iterations().empty());
    if (a.Iterations().empty()) return;
    VERIFIER_EGAL(a.Iterations()[0].m_faible.m_variable, 0);
    VERIFIER(a.Iterations()[0].m_epsilon <= 0.1);
    VERIFIER_PROCHE(SommePoids(a), 1.0, 1e-12);
    VERIFIER_PROCHE(ErreurFort(a, c, y), 0.0, 1e-12);
}

TEST(adaboost_quatre_points_a_la_main) {
    // x = 1,2,3,4 ; y = +,-,-,+ ; d_i = 1/4. Meilleur faible (egalite -> plus petit seuil) : seuil 1,5 sens -1,
    // h = (+,-,-,-) : seul le point 4 est mal classe, eps = 1/4, alpha = 1/2 ln 3, Z = 2 sqrt(3/16) = sqrt(3)/2.
    // Poids : bien classes 1/4 e^-alpha / Z = 1/6 ; mal classe 1/4 e^alpha / Z = 1/2.
    std::vector<int> y;
    Corpus c = QuatrePoints(y);
    AdaBoostPasAPas a;
    VERIFIER(Reussi(a.Initialiser(c.m_x, y, TypeFaible::Seuil), "Initialiser"));
    for (double d : a.Poids()) VERIFIER_PROCHE(d, 0.25, 1e-15);
    auto r = a.Iterer(nullptr);
    VERIFIER(Reussi(r, "Iterer") && r.Valeur());
    if (a.Iterations().size() != 1) { VERIFIER_EGAL((int)a.Iterations().size(), 1); return; }
    const IterationAdaBoost& it = a.Iterations()[0];
    VERIFIER_PROCHE(it.m_faible.m_seuil, 1.5, 1e-12);
    VERIFIER_EGAL(it.m_faible.m_sens, -1);
    VERIFIER_PROCHE(it.m_epsilon, 0.25, 1e-12);
    VERIFIER_PROCHE(it.m_alpha, 0.5 * std::log(3.0), 1e-12);
    VERIFIER_PROCHE(it.m_z, std::sqrt(3.0) / 2.0, 1e-12);
    VERIFIER_PROCHE(a.Poids()[0], 1.0 / 6.0, 1e-12);
    VERIFIER_PROCHE(a.Poids()[1], 1.0 / 6.0, 1e-12);
    VERIFIER_PROCHE(a.Poids()[2], 1.0 / 6.0, 1e-12);
    VERIFIER_PROCHE(a.Poids()[3], 0.5, 1e-12);
    VERIFIER_PROCHE(SommePoids(a), 1.0, 1e-12);
    VERIFIER(a.Poids()[3] > 0.25);           // le mal classe voit son poids augmenter
    VERIFIER_PROCHE(it.m_erreurApprentissage, 0.25, 1e-12);
    VERIFIER_PROCHE(it.m_borne, std::exp(-2.0 * 0.25 * 0.25), 1e-12);
}

TEST(adaboost_invariants_par_iteration) {
    Corpus c = Nuages({{0.0, 0.0}, {1.2, 0.5}}, 60, 9);       // nuages qui se chevauchent
    std::vector<int> y = PlusMoins(c.m_labels, 1);
    AdaBoostPasAPas a;
    VERIFIER(Reussi(a.Initialiser(c.m_x, y, TypeFaible::Seuil), "Initialiser"));
    int n = Iterer(a, 12);
    VERIFIER(n >= 5);
    for (const IterationAdaBoost& it : a.Iterations()) {
        VERIFIER_PROCHE(it.m_z, 2.0 * std::sqrt(it.m_epsilon * (1.0 - it.m_epsilon)), 1e-12);
        VERIFIER(it.m_borne >= it.m_erreurApprentissage - 1e-12);
        VERIFIER(it.m_epsilon > 0 && it.m_epsilon < 0.5);
    }
    VERIFIER_PROCHE(SommePoids(a), 1.0, 1e-12);
}

TEST(adaboost_xor_seuil_limite_du_faible) {
    // Lecon du cours : un seuil ne peut pas separer un XOR. Cas symetrique exact : eps = 1/2 -> arret immediat.
    std::vector<int> y = {1, 1, -1, -1};
    cv::Mat x = (cv::Mat_<float>(4, 2) << 0, 0, 1, 1, 0, 1, 1, 0);
    AdaBoostPasAPas exact;
    VERIFIER(Reussi(exact.Initialiser(x, y, TypeFaible::Seuil), "Initialiser"));
    auto r = exact.Iterer(nullptr);
    VERIFIER(Reussi(r, "Iterer") && !r.Valeur());
    VERIFIER(exact.Iterations().empty());
    VERIFIER_EGAL(exact.Arret(), std::string("epsilon >= 1/2 : plus de faible utile"));
    // Nuages bruites : on constate une erreur d'apprentissage elevee (cours : ~48 %).
    std::vector<int> yb;
    Corpus c = Xor(yb, 0.5);
    AdaBoostPasAPas a;
    VERIFIER(Reussi(a.Initialiser(c.m_x, yb, TypeFaible::Seuil), "Initialiser"));
    int n = Iterer(a, 3);
    double err = a.Iterations().empty() ? 0.5 : a.Iterations().back().m_erreurApprentissage;
    std::printf("  [constat XOR bruite] %d iteration(s), eps_1 = %.4f, erreur du fort = %.4f, arret = \"%s\"\n", n,
                a.Iterations().empty() ? -1.0 : a.Iterations()[0].m_epsilon, err, a.Arret().c_str());
    VERIFIER(err >= 0.25);
    VERIFIER(a.Iterations().empty() || a.Iterations()[0].m_epsilon >= 0.4);
}

TEST(adaboost_hyperplan_mieux_que_seuil_sur_diagonale) {
    std::vector<int> y;
    Corpus c = Diagonale(y);
    AdaBoostPasAPas seuil, hyper;
    VERIFIER(Reussi(seuil.Initialiser(c.m_x, y, TypeFaible::Seuil), "Initialiser Seuil"));
    VERIFIER(Reussi(hyper.Initialiser(c.m_x, y, TypeFaible::Hyperplan), "Initialiser Hyperplan"));
    const int nb = 3;
    Iterer(seuil, nb);
    Iterer(hyper, nb);
    double es = ErreurFort(seuil, c, y), eh = ErreurFort(hyper, c, y);
    std::printf("  [constat diagonale] erreur Seuil = %.4f, Hyperplan = %.4f (%zu / %zu iterations)\n", es, eh,
                seuil.Iterations().size(), hyper.Iterations().size());
    VERIFIER(eh < es);
    VERIFIER(eh <= 0.05);
    if (!hyper.Iterations().empty()) {
        const Faible& f = hyper.Iterations()[0].m_faible;
        double norme = 0;
        for (double v : f.m_direction) norme += v * v;
        VERIFIER_PROCHE(norme, 1.0, 1e-9);
    }
}

TEST(adaboost_determinisme_apres_reinitialiser) {
    Corpus c = Nuages({{0.0, 0.0}, {2.0, 1.0}}, 50, 21);
    std::vector<int> y = PlusMoins(c.m_labels, 1);
    for (TypeFaible type : {TypeFaible::Seuil, TypeFaible::Hyperplan}) {
        AdaBoostPasAPas a;
        VERIFIER(Reussi(a.Initialiser(c.m_x, y, type), "Initialiser"));
        Iterer(a, 6);
        std::vector<IterationAdaBoost> premiere = a.Iterations();
        std::vector<double> poids = a.Poids();
        a.Reinitialiser();
        VERIFIER(a.Iterations().empty());
        VERIFIER_PROCHE(a.Poids()[0], 1.0 / (double)c.m_x.rows, 1e-15);
        Iterer(a, 6);
        VERIFIER_EGAL(premiere.size(), a.Iterations().size());
        for (size_t t = 0; t < premiere.size() && t < a.Iterations().size(); ++t) {
            VERIFIER(premiere[t].m_epsilon == a.Iterations()[t].m_epsilon);
            VERIFIER(premiere[t].m_alpha == a.Iterations()[t].m_alpha);
            VERIFIER(premiere[t].m_z == a.Iterations()[t].m_z);
        }
        VERIFIER(poids == a.Poids());
    }
}

TEST(adaboost_trace_un_etat_par_iteration) {
    Corpus c = Nuages({{0.0, 0.0}, {2.0, 0.5}}, 40, 4);
    std::vector<int> y = PlusMoins(c.m_labels, 1);
    AdaBoostPasAPas a;
    VERIFIER(Reussi(a.Initialiser(c.m_x, y, TypeFaible::Seuil), "Initialiser"));
    CollecteurTrace col(1000, 1 << 24);
    int n = Iterer(a, 8, &col);
    VERIFIER_EGAL((int)col.Etats().size(), n);
    for (size_t t = 0; t < col.Etats().size(); ++t) {
        const EtatTrace& e = col.Etats()[t];
        VERIFIER_EGAL(e.m_phase, std::string("iteration"));
        VERIFIER_EGAL((int)e.m_vecteur.size(), c.m_x.rows);
        VERIFIER_EGAL(e.m_grandeurs.size(), (size_t)6);
        VERIFIER_EGAL(e.m_grandeurs[0].first, std::string("t"));
        VERIFIER_PROCHE(e.m_grandeurs[0].second, (double)t + 1, 0);
        VERIFIER_PROCHE(e.m_grandeurs[1].second, a.Iterations()[t].m_epsilon, 0);
    }
    if (!col.Etats().empty()) {          // le dernier etat contient exactement les poids courants
        const std::vector<float>& v = col.Etats().back().m_vecteur;
        for (size_t i = 0; i < v.size(); ++i) VERIFIER(v[i] == (float)a.Poids()[i]);
    }
}

TEST(adaboost_parfait_alpha_fixe_et_erreurs) {
    std::vector<int> y = {-1, -1, 1, 1};
    cv::Mat x = (cv::Mat_<float>(4, 1) << 0, 1, 5, 6);
    AdaBoostPasAPas a;
    VERIFIER(Reussi(a.Initialiser(x, y, TypeFaible::Seuil), "Initialiser"));
    auto r = a.Iterer(nullptr);
    VERIFIER(Reussi(r, "Iterer") && !r.Valeur());
    VERIFIER_EGAL((int)a.Iterations().size(), 1);          // faible parfait des la 1re iteration : ajoutee, alpha = 10
    if (!a.Iterations().empty()) VERIFIER_PROCHE(a.Iterations()[0].m_alpha, 10.0, 0);
    VERIFIER_EGAL(a.Arret(), std::string("epsilon = 0 : faible parfait"));
    // Erreurs d'entree
    AdaBoostPasAPas b;
    Refuse(b.Initialiser(x, {0, 1, 1, 1}, TypeFaible::Seuil), "label 0 interdit");
    Refuse(b.Initialiser(x, {1, 1, 1, 1}, TypeFaible::Seuil), "une seule classe");
    Refuse(b.Initialiser(x, {1, -1}, TypeFaible::Seuil), "taille des labels");
    Refuse(b.Iterer(nullptr), "Iterer sans Initialiser");
}

TEST(adaboost_multiclasse_trois_classes) {
    Corpus c = Nuages({{0.0, 0.0}, {5.0, 0.0}, {2.5, 5.0}}, 40, 11, 0.8);
    AdaBoostMulticlasse m;
    VERIFIER(Reussi(m.Entrainer(c.m_x, c.m_labels, 3, TypeFaible::Seuil, 15), "Entrainer"));
    int bons = 0;
    for (int i = 0; i < c.m_x.rows; ++i) bons += m.Decider(c.m_x.ptr<float>(i)) == c.m_labels[(size_t)i] ? 1 : 0;
    double exactitude = (double)bons / (double)c.m_x.rows;
    std::printf("  [constat multiclasse] exactitude d'apprentissage = %.4f\n", exactitude);
    VERIFIER(exactitude >= 0.9);
    VERIFIER_EGAL((int)m.ParClasse().size(), 3);
    Refuse(m.Entrainer(c.m_x, c.m_labels, 4, TypeFaible::Seuil, 5), "classe absente");
}

TEST(perceptron_et_logique_converge) {
    cv::Mat x = (cv::Mat_<float>(4, 2) << 0, 0, 0, 1, 1, 0, 1, 1);
    std::vector<int> y = {0, 0, 0, 1};
    ParametresPerceptron p;
    p.m_melanger = false; p.m_eta = 0.1; p.m_epoquesMax = 100;
    auto r = EntrainerPerceptron(x, y, p, nullptr);
    if (!Reussi(r, "perceptron ET")) return;
    VERIFIER(r.Valeur().m_converge);
    VERIFIER(r.Valeur().m_epoques < 100);
    VERIFIER_EGAL(r.Valeur().m_erreursParEpoque.back(), 0);
    for (int i = 0; i < 4; ++i) {                      // frontiere correcte : H(w.x + b) = y
        double a = r.Valeur().m_biais;
        for (int j = 0; j < 2; ++j) a += r.Valeur().m_poids[(size_t)j] * x.at<float>(i, j);
        VERIFIER_EGAL(a > 0 ? 1 : 0, y[(size_t)i]);
    }
}

TEST(perceptron_xor_ne_converge_pas) {
    cv::Mat x = (cv::Mat_<float>(4, 2) << 0, 0, 0, 1, 1, 0, 1, 1);
    std::vector<int> y = {0, 1, 1, 0};
    for (bool melanger : {false, true}) {
        ParametresPerceptron p;
        p.m_melanger = melanger; p.m_epoquesMax = 100;
        auto r = EntrainerPerceptron(x, y, p, nullptr);
        if (!Reussi(r, "perceptron XOR")) return;
        VERIFIER(!r.Valeur().m_converge);
        VERIFIER_EGAL(r.Valeur().m_epoques, 100);
        VERIFIER_EGAL((int)r.Valeur().m_erreursParEpoque.size(), 100);
        VERIFIER(*std::min_element(r.Valeur().m_erreursParEpoque.begin(), r.Valeur().m_erreursParEpoque.end()) > 0);
    }
}

TEST(perceptron_trace_et_determinisme) {
    cv::Mat x = (cv::Mat_<float>(4, 2) << 0, 0, 0, 1, 1, 0, 1, 1);
    std::vector<int> y = {0, 0, 0, 1};
    ParametresPerceptron p;
    p.m_melanger = true; p.m_graine = 42;
    CollecteurTrace col(100000, 1 << 26);
    auto r = EntrainerPerceptron(x, y, p, &col);
    if (!Reussi(r, "perceptron trace")) return;
    int exemples = 0, epoques = 0;
    for (const EtatTrace& e : col.Etats()) {
        if (e.m_phase == "exemple") { ++exemples; VERIFIER_EGAL(e.m_grandeurs.size(), (size_t)5); }
        if (e.m_phase == "epoque") ++epoques;
        VERIFIER_EGAL(e.m_vecteur.size(), (size_t)3);   // [w1, w2, b]
    }
    VERIFIER_EGAL(exemples, 4 * r.Valeur().m_epoques);  // un etat par exemple presente
    VERIFIER_EGAL(epoques, r.Valeur().m_epoques);
    auto r2 = EntrainerPerceptron(x, y, p, nullptr);
    VERIFIER(Reussi(r2, "perceptron rejoue") && r2.Valeur().m_poids == r.Valeur().m_poids &&
             r2.Valeur().m_biais == r.Valeur().m_biais);
    ParametresPerceptron mauvais = p;
    mauvais.m_poidsInitiaux = {1.0};
    Refuse(EntrainerPerceptron(x, y, mauvais, nullptr), "poids initiaux de mauvaise taille");
    Refuse(EntrainerPerceptron(x, {0, 1, 2, 0}, p, nullptr), "label hors {0,1}");
}

TEST(kmeans_j_decroissant_et_aucun_changement) {
    Corpus c = Nuages({{0.0, 0.0}, {5.0, 0.0}, {2.5, 5.0}}, 40, 13, 0.8);
    ParametresKmeans p;
    p.m_k = 3; p.m_init = InitKmeans::KmeansPlusPlus; p.m_graine = 5;
    CollecteurTrace col(100000, 1 << 26);
    auto r = Kmeans(c.m_x, p, &col);
    if (!Reussi(r, "Kmeans")) return;
    const ResultatKmeans& k = r.Valeur();
    VERIFIER(k.m_aucunChangement);
    for (size_t i = 1; i < k.m_jParPhase.size(); ++i)
        VERIFIER(k.m_jParPhase[i] <= k.m_jParPhase[i - 1] * (1.0 + 1e-9) + 1e-9);
    VERIFIER_EGAL(k.m_jParPhase.size(), col.Etats().size());
    for (size_t i = 0; i < col.Etats().size(); ++i) {
        VERIFIER_PROCHE(col.Etats()[i].m_grandeurs[0].second, k.m_jParPhase[i], 0);
        VERIFIER_EGAL(col.Etats()[i].m_vecteur.size(), (size_t)(3 * 2));
        VERIFIER_EGAL(col.Etats()[i].m_phase, std::string(i % 2 == 0 ? "affectation" : "mise_a_jour"));
    }
    VERIFIER_EGAL(k.m_centres.rows, 3);
}

TEST(kmeans_egalite_avec_opencv) {
    Corpus c = Nuages({{0.0, 0.0}, {5.0, 0.0}, {2.5, 5.0}}, 40, 17, 0.9);
    cv::Mat init = (cv::Mat_<float>(3, 2) << 1, 1, 3, 1, 2, 3);
    ParametresKmeans p;
    p.m_k = 3; p.m_init = InitKmeans::Fournie; p.m_centresInitiaux = init; p.m_iterMax = 100;
    auto r = Kmeans(c.m_x, p, nullptr);
    if (!Reussi(r, "Kmeans fourni")) return;
    // OpenCV : labels initiaux = affectation au plus proche des centres fournis (OpenCV recalcule alors les
    // barycentres : meme trajectoire que affectation -> mise a jour).
    cv::Mat labels(c.m_x.rows, 1, CV_32S), centres;
    for (int i = 0; i < c.m_x.rows; ++i) {
        int meilleur = 0; double dm = 1e300;
        for (int j = 0; j < 3; ++j) {
            double d = cv::norm(c.m_x.row(i), init.row(j), cv::NORM_L2SQR);
            if (d < dm) { dm = d; meilleur = j; }
        }
        labels.at<int>(i) = meilleur;
    }
    cv::kmeans(c.m_x, 3, labels, cv::TermCriteria(cv::TermCriteria::MAX_ITER + cv::TermCriteria::EPS, 100, 0), 1,
               cv::KMEANS_USE_INITIAL_LABELS, centres);
    int ecartsAff = 0;
    double ecartCentres = 0;
    for (int i = 0; i < c.m_x.rows; ++i) ecartsAff += labels.at<int>(i) != r.Valeur().m_affectations[(size_t)i] ? 1 : 0;
    for (int j = 0; j < 3; ++j)
        for (int d = 0; d < 2; ++d)
            ecartCentres = std::max(ecartCentres, (double)std::fabs(centres.at<float>(j, d) - r.Valeur().m_centres.at<float>(j, d)));
    std::printf("  [constat OpenCV] affectations differentes = %d / %d ; ecart max des centres = %.3g\n", ecartsAff,
                c.m_x.rows, ecartCentres);
    VERIFIER_EGAL(ecartsAff, 0);
    VERIFIER(ecartCentres <= 1e-4);
}

TEST(kmeans_initialisations_deterministes_et_erreurs) {
    Corpus c = Nuages({{0.0, 0.0}, {5.0, 0.0}, {2.5, 5.0}}, 30, 19, 0.7);
    for (InitKmeans init : {InitKmeans::Aleatoire, InitKmeans::KmeansPlusPlus}) {
        ParametresKmeans p;
        p.m_k = 3; p.m_init = init; p.m_graine = 99;
        auto a = Kmeans(c.m_x, p, nullptr), b = Kmeans(c.m_x, p, nullptr);
        if (!Reussi(a, "Kmeans a") || !Reussi(b, "Kmeans b")) return;
        VERIFIER(a.Valeur().m_affectations == b.Valeur().m_affectations);
        VERIFIER(cv::norm(a.Valeur().m_centres, b.Valeur().m_centres, cv::NORM_INF) == 0.0);
        VERIFIER(a.Valeur().m_jParPhase == b.Valeur().m_jParPhase);
    }
    ParametresKmeans p;
    p.m_k = 0;
    Refuse(Kmeans(c.m_x, p, nullptr), "k = 0");
    p.m_k = c.m_x.rows + 1;
    Refuse(Kmeans(c.m_x, p, nullptr), "k > n");
    p.m_k = 3; p.m_init = InitKmeans::Fournie;
    Refuse(Kmeans(c.m_x, p, nullptr), "centres fournis absents");
    p.m_k = c.m_x.rows; p.m_init = InitKmeans::Aleatoire;     // k = n : un centre par point, J = 0
    auto r = Kmeans(c.m_x, p, nullptr);
    if (Reussi(r, "k = n")) VERIFIER_PROCHE(r.Valeur().m_jParPhase.back(), 0.0, 1e-9);
}

int main(int argc, char** argv) { return LancerTests(argc, argv); }
