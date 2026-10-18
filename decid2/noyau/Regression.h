// Regression (operation "regression") : SVM (epsilon-SVR), MLP, foret aleatoire et KPPV d'OpenCV, cible numerique.
// Reprend les menus de regression de l'ancien Decid (ChildFrm.cpp : SVM, KPPV, RandomForest, MLP en regression,
// export _Result_regression_*.csv), avec partage, graine et normalisation explicites.
// Fichiers ecrits dans le staging : regression.json (metriques EQM, MAE, R2 sur apprentissage et test,
// parametres demandes et effectifs), regression_predictions.csv, regression_courbe.json, regression_modele.yml.
// Aucun modele.json n'est ecrit : la publication n'est pas confondue avec une experience de classification.
#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/ml.hpp>
#include "Erreur.h"
#include "Experience.h"
#include "Json.h"

namespace decid {

namespace fs = std::filesystem;

enum class FamilleRegression { Svm, Mlp, ForetAleatoire, Kppv };
const char* NomFamilleRegression(FamilleRegression f);

struct ParametresRegression {
    FamilleRegression m_famille = FamilleRegression::Svm;
    // SVM (EPS_SVR)
    std::string m_noyau = "RBF";
    double m_c = 1.0, m_gamma = 1.0, m_epsilon = 0.1, m_degre = 3.0, m_coef0 = 0.0;
    // MLP (une sortie, mise a l'echelle des entrees et de la sortie par OpenCV)
    std::vector<int> m_couchesCachees{10};
    std::string m_activation = "SIGMOIDE_SYM";
    std::string m_methode = "RPROP";
    double m_eta = 0.1, m_moment = 0.1;
    int m_iterationsMax = 1000;
    double m_tolerance = 1e-6;
    // Foret
    int m_nbArbres = 100, m_profondeurMax = 10, m_minEchantillons = 2, m_nbVariablesActives = 0;
    // KPPV (moyenne des k plus proches voisins, distance euclidienne)
    int m_k = 5;
    ValeurJson VersJson() const;
};
Resultat<ParametresRegression> ParametresRegressionDepuisJson(const ValeurJson& algorithme);

struct MetriquesRegression {
    long m_n = 0;
    double m_eqm = 0, m_mae = 0;
    double m_r2 = 0;
    bool m_r2Defini = false;       // faux si la cible est constante sur l'ensemble
    ValeurJson VersJson() const;
};
MetriquesRegression CalculerMetriques(const std::vector<double>& cible, const std::vector<double>& prediction);

// Requete lue : source (meme bloc que l'experience), partage, normalisation, algorithme, courbe.
struct RequeteRegression {
    RequeteExperience m_source;              // seuls source, normalisation et graine servent
    std::string m_partage = "aleatoire";     // "aleatoire" (melange avec la graine) ou "premiers_n"
    double m_ratio = 0.7;
    std::uint64_t m_graine = 1;
    ParametresRegression m_algorithme;
    int m_attributCourbe = 0;
    int m_pointsCourbe = 200;
};
Resultat<RequeteRegression> RequeteRegressionDepuisJson(const ValeurJson& j, const fs::path& dossierRequete);

// Apprentissage seul (X CV_32F normalise, y reel) ; graine appliquee a theRNG() du thread appelant.
Resultat<cv::Ptr<cv::ml::StatModel>> EntrainerRegression(const cv::Mat& x, const std::vector<double>& y,
                                                         const ParametresRegression& p, std::uint64_t graine);
Resultat<std::vector<double>> PredireRegression(const cv::Ptr<cv::ml::StatModel>& modele, const cv::Mat& x);

Statut ExecuterRegression(const ValeurJson& requete, const fs::path& dossierRequete, const fs::path& staging,
                          const std::atomic<bool>& annulation);

} // namespace decid
