// Scores de confiance par famille (voir Scores.h). Calcul par lots, a partir des sorties reelles d'OpenCV.
#include "Scores.h"

#include <algorithm>
#include <cmath>
#include <opencv2/ml.hpp>

namespace decid {

namespace {

using cv::ml::StatModel;

Erreur ErreurScore(const std::string& message) {
    return FaireErreur(CodeErreur::Calcul, "score_confiance", message);
}

// Ecart entre les deux plus grandes valeurs d'une ligne de K valeurs
double EcartDeuxPremiers(const std::vector<double>& v) {
    double a = -INFINITY, b = -INFINITY;
    for (double x : v) {
        if (x > a) { b = a; a = x; }
        else if (x > b) b = x;
    }
    return v.size() < 2 ? 1.0 : a - b;
}

float Borne01(double v) { return (float)std::min(1.0, std::max(0.0, v)); }

double Valeur(const cv::Mat& m, int i, int j) {
    return m.type() == CV_64F ? m.at<double>(i, j) : m.type() == CV_32S ? (double)m.at<int>(i, j) : (double)m.at<float>(i, j);
}

Resultat<std::vector<float>> ScoresSvm(const Modele& m, const cv::Mat& x) {
    cv::Mat f;
    m.m_opencv->predict(x, f, StatModel::RAW_OUTPUT);
    std::vector<float> s((size_t)x.rows);
    for (int i = 0; i < x.rows; ++i) s[(size_t)i] = Borne01(std::fabs(Valeur(f, i, 0)));
    return s;
}

std::vector<float> ScoresVotes(const cv::Mat& votesParLigne, int nbClasses, double total) {
    std::vector<float> s((size_t)votesParLigne.rows);
    std::vector<double> v((size_t)nbClasses);
    for (int i = 0; i < votesParLigne.rows; ++i) {
        for (int c = 0; c < nbClasses; ++c) v[(size_t)c] = Valeur(votesParLigne, i, c);
        s[(size_t)i] = Borne01(EcartDeuxPremiers(v) / total);
    }
    return s;
}

Resultat<std::vector<float>> ScoresKppv(const Modele& m, const cv::Mat& x) {
    auto knn = m.m_opencv.dynamicCast<cv::ml::KNearest>();
    const int k = knn->getDefaultK();
    cv::Mat reponses, voisins, distances;
    knn->findNearest(x, k, reponses, voisins, distances);
    cv::Mat votes = cv::Mat::zeros(x.rows, m.m_nbClasses, CV_64F);
    for (int i = 0; i < x.rows; ++i)
        for (int j = 0; j < voisins.cols; ++j) {
            const int c = (int)std::lround(Valeur(voisins, i, j));
            if (c < 0 || c >= m.m_nbClasses) return ErreurScore("voisin de classe inconnue");
            votes.at<double>(i, c) += 1.0;
        }
    return ScoresVotes(votes, m.m_nbClasses, (double)voisins.cols);
}

Resultat<std::vector<float>> ScoresForet(const Modele& m, const cv::Mat& x) {
    auto foret = m.m_opencv.dynamicCast<cv::ml::RTrees>();
    cv::Mat brut;
    foret->getVotes(x, brut, 0);                   // ligne 0 : etiquettes de classe ; puis une ligne par echantillon
    cv::Mat votes = cv::Mat::zeros(x.rows, m.m_nbClasses, CV_64F);
    double total = 0;
    for (int j = 0; j < brut.cols; ++j) {
        const int c = (int)std::lround(Valeur(brut, 0, j));
        if (c < 0 || c >= m.m_nbClasses) return ErreurScore("etiquette de vote de foret inconnue");
        for (int i = 0; i < x.rows; ++i) votes.at<double>(i, c) = Valeur(brut, i + 1, j);
    }
    for (int j = 0; j < brut.cols; ++j) total += Valeur(brut, 1, j);   // nombre d'arbres votants
    if (!(total > 0)) return ErreurScore("aucun vote d'arbre");
    return ScoresVotes(votes, m.m_nbClasses, total);
}

Resultat<std::vector<float>> ScoresProbabilites(const cv::Mat& probas, int nbClasses) {
    std::vector<float> s((size_t)probas.rows);
    std::vector<double> v((size_t)nbClasses);
    for (int i = 0; i < probas.rows; ++i) {
        double somme = 0;
        for (int c = 0; c < nbClasses; ++c) { v[(size_t)c] = Valeur(probas, i, c); somme += v[(size_t)c]; }
        if (!(somme > 0) || !std::isfinite(somme))
            return ErreurScore("probabilites a posteriori non normalisables a la ligne " + std::to_string(i + 1));
        for (double& p : v) p /= somme;
        s[(size_t)i] = Borne01(EcartDeuxPremiers(v));
    }
    return s;
}

Resultat<std::vector<float>> ScoresBayes(const Modele& m, const cv::Mat& x) {
    auto nb = m.m_opencv.dynamicCast<cv::ml::NormalBayesClassifier>();
    cv::Mat sorties, probas;
    nb->predictProb(x, sorties, probas);
    return ScoresProbabilites(probas, m.m_nbClasses);
}

Resultat<std::vector<float>> ScoresEm(const Modele& m, const cv::Mat& x) {
    auto em = m.m_opencv.dynamicCast<cv::ml::EM>();
    cv::Mat probasClusters;
    em->predict(x, probasClusters);
    cv::Mat probas = cv::Mat::zeros(x.rows, m.m_nbClasses, CV_64F);
    for (int k = 0; k < probasClusters.cols; ++k) {
        const int c = (size_t)k < m.m_correspondanceClusters.size() ? m.m_correspondanceClusters[(size_t)k] : -1;
        if (c < 0) continue;                       // cluster non associe : sa masse n'appuie aucune classe
        for (int i = 0; i < x.rows; ++i) probas.at<double>(i, c) += Valeur(probasClusters, i, k);
    }
    return ScoresProbabilites(probas, m.m_nbClasses);
}

Resultat<std::vector<float>> ScoresMlp(const Modele& m, const cv::Mat& x) {
    cv::Mat sorties;
    m.m_opencv->predict(x, sorties);
    std::vector<float> s((size_t)x.rows);
    std::vector<double> v((size_t)m.m_nbClasses);
    for (int i = 0; i < x.rows; ++i) {
        for (int c = 0; c < m.m_nbClasses; ++c) v[(size_t)c] = Valeur(sorties, i, c);
        s[(size_t)i] = Borne01(EcartDeuxPremiers(v));
    }
    return s;
}

Resultat<std::vector<float>> ScoresBoostOva(const Modele& m, const cv::Mat& x) {
    auto scores = ScoresBoost(m, x);
    if (!scores.Ok()) return scores.Err();
    std::vector<float> s((size_t)x.rows);
    std::vector<double> v((size_t)m.m_nbClasses);
    for (int i = 0; i < x.rows; ++i) {
        for (int c = 0; c < m.m_nbClasses; ++c) v[(size_t)c] = scores.Valeur().at<float>(i, c);
        s[(size_t)i] = Borne01(std::tanh(EcartDeuxPremiers(v)));
    }
    return s;
}

} // namespace

DefinitionScore DefinirScore(const Modele& m) {
    DefinitionScore d;
    switch (m.m_famille) {
    case Famille::Svm:
        if (m.m_nbClasses != 2) {
            d.m_defini = false;
            d.m_raison = "SVM a plus de deux classes (un-contre-un) : pas de valeur de decision unique";
        }
        d.m_definition = "min(|f(x)|, 1), f = valeur de decision du SVM (RAW_OUTPUT) ; 1 = hors de la marge";
        break;
    case Famille::Kppv: d.m_definition = "(votes de la 1re classe - votes de la 2e) / k"; break;
    case Famille::ForetAleatoire: d.m_definition = "(votes de la 1re classe - votes de la 2e) / nombre d'arbres"; break;
    case Famille::BayesNormal: d.m_definition = "p1 - p2, probabilites a posteriori normalisees (a priori egaux)"; break;
    case Famille::Em: d.m_definition = "p1 - p2, probabilites a posteriori des clusters cumulees par classe"; break;
    case Famille::Mlp: d.m_definition = "ecart des deux sorties les plus fortes (cibles one-hot 0/1), borne a [0, 1]"; break;
    case Famille::Boost: d.m_definition = "tanh(s1 - s2), s = scores bruts un-contre-tous (PREDICT_SUM | RAW_OUTPUT)"; break;
    }
    return d;
}

Resultat<std::vector<float>> ScoresConfiance(const Modele& m, const cv::Mat& x) {
    const DefinitionScore d = DefinirScore(m);
    if (!d.m_defini) return ErreurScore(d.m_raison);
    if (x.type() != CV_32F || x.cols != m.m_nbAttributs) return ErreurScore("dimension incompatible avec le modele");
    try {
        switch (m.m_famille) {
        case Famille::Svm: return ScoresSvm(m, x);
        case Famille::Kppv: return ScoresKppv(m, x);
        case Famille::ForetAleatoire: return ScoresForet(m, x);
        case Famille::BayesNormal: return ScoresBayes(m, x);
        case Famille::Em: return ScoresEm(m, x);
        case Famille::Mlp: return ScoresMlp(m, x);
        case Famille::Boost: return ScoresBoostOva(m, x);
        }
    } catch (const cv::Exception& e) {
        return ErreurScore(std::string("OpenCV : ") + e.what());
    }
    return ErreurScore("famille inconnue");
}

} // namespace decid
