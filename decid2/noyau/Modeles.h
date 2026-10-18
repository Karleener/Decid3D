// Familles de classifieurs (SVM, foret, KPPV, Bayes normal, MLP, EM, Boost un-contre-tous) : appels cv::ml
// directs, parametres demandes et effectifs relus sur le modele, inspection de l'etat interne.
#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/ml.hpp>
#include "Erreur.h"
#include "Json.h"

namespace decid {

namespace fs = std::filesystem;

// Familles L1 (Svm, ForetAleatoire) puis L5. Noms JSON : svm, foret_aleatoire, kppv, bayes_normal, mlp, em, boost.
enum class Famille { Svm, ForetAleatoire, Kppv, BayesNormal, Mlp, Em, Boost };
const char* NomFamille(Famille f);
bool FamilleDepuisNom(const std::string& nom, Famille& f);

struct ParametresSvm {
    std::string m_noyau = "RBF";   // "LINEAIRE", "POLY", "RBF", "SIGMOIDE"
    double m_c = 1.0;
    double m_gamma = 1.0;          // gamma OpenCV : K = exp(-gamma*||x-y||^2) ; pas de conversion en sigma
    double m_degre = 3.0;
    double m_coef0 = 0.0;
};

struct ParametresForet {
    int m_nbArbres = 50;           // nombre exact d'arbres (critere MAX_ITER seul)
    int m_profondeurMax = 10;
    int m_minEchantillons = 2;
    int m_nbVariablesActives = 0;  // 0 = racine du nombre d'attributs (defaut OpenCV)
};

struct ParametresKppv {
    int m_k = 3;                   // explicite et enregistre (l'ancien Decid gardait 10 par defaut)
};
// NormalBayesClassifier d'OpenCV : une gaussienne a covariance PLEINE par classe (pas "naive" malgre le nom
// historique du menu) ; aucun parametre.
struct ParametresMlp {
    std::vector<int> m_couchesCachees{10};
    std::string m_activation = "SIGMOIDE_SYM";   // "SIGMOIDE_SYM", "RELU", "IDENTITE"
    std::string m_methode = "RETROPROPAGATION";  // "RETROPROPAGATION" (eta, moment) ou "RPROP"
    double m_eta = 0.1, m_moment = 0.1;
    int m_iterationsMax = 300;
    double m_epsilon = 1e-6;
};
struct ParametresEm {
    int m_k = 2;                   // 2 <= K <= min(20, N_apprentissage) ; refus explicite sinon (jamais rabattu)
    std::string m_covariance = "DIAGONALE";      // "SPHERIQUE", "DIAGONALE", "GENERIQUE"
    int m_iterationsMax = 100;
    double m_epsilon = 1e-6;
};
// Boost OpenCV un-contre-tous (equivalent du MultiClassBoost historique) : un cv::ml::Boost binaire par
// classe ; decision = classe de score brut maximal (RAW_OUTPUT), egalite -> plus petit id ("ova-score-max/1").
struct ParametresBoost {
    std::string m_type = "DISCRETE";             // "DISCRETE", "REEL", "LOGIT", "GENTLE"
    int m_nbFaibles = 50;
    int m_profondeur = 1;
};

struct ParametresAlgorithme {
    Famille m_famille = Famille::Svm;
    ParametresSvm m_svm;
    ParametresForet m_foret;
    ParametresKppv m_kppv;
    ParametresMlp m_mlp;
    ParametresEm m_em;
    ParametresBoost m_boost;
    ValeurJson VersJson() const;                  // {"famille":..., parametres de la famille seulement}
};
Resultat<ParametresAlgorithme> ParametresDepuisJson(const ValeurJson& algorithme);

struct Modele {
    Famille m_famille = Famille::Svm;
    cv::Ptr<cv::ml::StatModel> m_opencv;               // modele principal (Boost : vide, voir m_sousModeles)
    std::vector<cv::Ptr<cv::ml::StatModel>> m_sousModeles;   // Boost : un modele binaire par classe
    std::vector<int> m_correspondanceClusters;         // EM : cluster -> classe (-1 = non associe), appris sur l'apprentissage
    int m_nbAttributs = 0;
    int m_nbClasses = 0;
    ValeurJson m_parametresDemandes;
    ValeurJson m_parametresEffectifs;   // relus sur le modele apres apprentissage
    // Bayes normal : moyennes et covariances par classe RECALCULEES par Decid sur les donnees passees a
    // Entrainer (OpenCV ne les expose pas) ; apres rechargement, reconstruites depuis l'etat OpenCV du fichier.
    // Non sauvegarde dans modele.yml. Null pour les autres familles.
    ValeurJson m_recalcul;
};

// X : CV_32F, une ligne par echantillon ; labels : identifiants internes 0..K-1.
// graine : appliquee a theRNG() du thread appelant (celui qui calcule).
Resultat<Modele> EntrainerSvm(const cv::Mat& x, const std::vector<int>& labels, int nbClasses,
                              const ParametresSvm& p);
Resultat<Modele> EntrainerForet(const cv::Mat& x, const std::vector<int>& labels, int nbClasses,
                                const ParametresForet& p, std::uint64_t graine);

Resultat<Modele> EntrainerKppv(const cv::Mat& x, const std::vector<int>& labels, int nbClasses, const ParametresKppv& p);
Resultat<Modele> EntrainerBayes(const cv::Mat& x, const std::vector<int>& labels, int nbClasses);
Resultat<Modele> EntrainerMlp(const cv::Mat& x, const std::vector<int>& labels, int nbClasses, const ParametresMlp& p);
Resultat<Modele> EntrainerEm(const cv::Mat& x, const std::vector<int>& labels, int nbClasses, const ParametresEm& p,
                             std::uint64_t graine);
Resultat<Modele> EntrainerBoost(const cv::Mat& x, const std::vector<int>& labels, int nbClasses, const ParametresBoost& p);

// Point d'entree unique pour toutes les familles (graine appliquee dans le thread appelant).
Resultat<Modele> Entrainer(const cv::Mat& x, const std::vector<int>& labels, int nbClasses,
                           const ParametresAlgorithme& p, std::uint64_t graine);

// MLP pas a pas : une epoque par appel a OpenCV avec UPDATE_WEIGHTS (vraie continuation des poids).
// Trace par epoque : grandeurs {epoque, perte_apprentissage, exactitude_apprentissage, exactitude_validation}
// et m_vecteur = poids de toutes les couches concatenes. xValidation peut etre vide (pas de validation).
Resultat<Modele> EntrainerMlpPasAPas(const cv::Mat& x, const std::vector<int>& labels, int nbClasses,
                                     const ParametresMlp& p, const cv::Mat& xValidation,
                                     const std::vector<int>& labelsValidation, std::uint64_t graine,
                                     class ObservateurTrace* observateur);

// EM pas a pas : iterations E puis M emises vers l'observateur (grandeurs {iteration, log_vraisemblance},
// m_vecteur = moyennes, covariances diagonales et poids concatenes).
Resultat<Modele> EntrainerEmPasAPas(const cv::Mat& x, const std::vector<int>& labels, int nbClasses,
                                    const ParametresEm& p, std::uint64_t graine, class ObservateurTrace* observateur);

// KPPV : voisins reels d'un echantillon (indices dans l'apprentissage embarque, distances EUCLIDIENNES
// croissantes, classes, vote par classe). Les indices sont retrouves par Decid (OpenCV ne les donne pas) en
// recalculant les distances aux echantillons embarques ; egalite de distance -> plus petit indice (ordre
// d'insertion d'OpenCV) ; la coherence avec les distances, reponses et decision d'OpenCV est verifiee (erreur sinon).
// Vote : classe la plus frequente parmi les k voisins ; egalite -> plus petit id (regle de knearest.cpp).
struct VoisinsKppv { std::vector<int> m_indices; std::vector<double> m_distances; std::vector<int> m_classes;
                     std::vector<int> m_vote; int m_decision = -1; };
Resultat<VoisinsKppv> VoisinsKppvPour(const Modele& m, const float* echantillon);

// Prediction par lots ; refuse une dimension incompatible ; verifie que chaque sortie est un id valide.
// EM : un echantillon tombant dans un cluster non associe -> erreur (jamais exclu en silence).
Resultat<std::vector<int>> Predire(const Modele& m, const cv::Mat& x);

// Format OpenCV (YAML) en un seul fichier. Boost : noeud "decid_boost_ova" puis "classe_<i>" ;
// EM : noeud du modele EM puis "decid_correspondance" (cluster -> classe).
Statut SauverModele(const Modele& m, const fs::path& fichier);
Resultat<Modele> ChargerModele(Famille famille, const fs::path& fichier, int nbClasses);

// Inspection de l'etat interne reel (espace du modele, c'est-a-dire apres normalisation) :
//  SVM : noyau, C, gamma, vecteurs de support, fonctions de decision (alpha, indices, rho) ;
//  Foret : arbres (noeuds, coupures), erreur hors sac (OOB), importance des variables ;
//  KPPV : k, nombre d'echantillons embarques ; BayesNormal : moyenne et covariance par classe RECALCULEES par
//  Decid sur l'apprentissage (OpenCV ne les expose pas ; marque "recalcul_decid") ; MLP : couches et poids ;
//  EM : moyennes, covariances, poids, correspondance cluster -> classe ; Boost : par classe, faibles (arbres).
ValeurJson InspecterModele(const Modele& m);

// Valeur de decision recalculee depuis l'inspection SVM (2 classes) : somme alpha_k K(sv_k, x) - rho.
// Positive => premiere classe (convention OpenCV svm.cpp). Sert a verifier l'inspection.
Resultat<double> DecisionSvmRecalculee(const Modele& m, const float* x);

// Boost : score brut de chaque classifieur binaire (matrice n x K, CV_32F) = predict(PREDICT_SUM | RAW_OUTPUT).
// Qualifie sur toutes les lignes : score > 0 <=> predict sans RAW_OUTPUT donne la classe positive (label 1).
Resultat<cv::Mat> ScoresBoost(const Modele& m, const cv::Mat& x);

// EM : cluster (indice OpenCV, premier maximum) de chaque ligne, sans passer par la correspondance.
Resultat<std::vector<int>> ClustersEm(const Modele& m, const cv::Mat& x);
// Correspondance cluster -> classe : classe majoritaire parmi les lignes d'apprentissage du cluster ;
// egalite -> plus petit id de classe ; cluster sans ligne -> -1.
std::vector<int> CorrespondanceClusters(const std::vector<int>& clusters, const std::vector<int>& labels,
                                        int nbClusters, int nbClasses);

// Votes de la foret pour un echantillon (comptes par classe)
Resultat<std::vector<int>> VotesForet(const Modele& m, const cv::Mat& echantillon);

} // namespace decid
