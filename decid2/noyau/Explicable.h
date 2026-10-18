// Moteur explicable : implementations transparentes, courtes, dans la notation du cours, pour rendre
// visibles des etats qu'OpenCV ne donne pas (conception-pedagogique.md §4). Chaque iteration emise vers
// un ObservateurTrace est un etat reellement calcule. Etiquette : "implementation pedagogique Decid".
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <opencv2/core.hpp>
#include "Erreur.h"
#include "Json.h"
#include "Trace.h"

namespace decid {

// ---------- AdaBoost binaire (cours du prof, Boosting p.6) ----------
// y dans {-1,+1} ; d_i initial = 1/p ; eps_t = somme d_i [y_i != h_t(x_i)] ; alpha_t = 1/2 ln((1-eps)/eps) ;
// d_i <- d_i exp(-alpha_t y_i h_t(x_i)) / Z_t ; arret si eps_t = 0 ou eps_t >= 1/2 (l'iteration n'est pas ajoutee).
enum class TypeFaible {
    Seuil,      // seuil parallele a un axe (cours p.9) : variable, seuil, sens
    Hyperplan   // [idee] hyperplan oblique : direction de Fisher ponderee puis meilleur seuil sur la projection
};
const char* NomFaible(TypeFaible t);

struct Faible {
    TypeFaible m_type = TypeFaible::Seuil;
    int m_variable = 0;                  // Seuil
    std::vector<double> m_direction;     // Hyperplan (norme 1)
    double m_seuil = 0.0;
    int m_sens = 1;                      // h(x) = m_sens si projection > seuil, -m_sens sinon
    int Decider(const float* x, int nbAttributs) const;   // -1 ou +1
    ValeurJson VersJson() const;
};

struct IterationAdaBoost {
    int m_t = 0;
    Faible m_faible;
    double m_epsilon = 0, m_alpha = 0, m_z = 0;
    double m_erreurApprentissage = 0;    // erreur du classifieur fort apres cette iteration
    double m_borne = 1;                  // exp(-2 somme gamma^2), gamma = 1/2 - eps
};

// Etat pas a pas : Initialiser puis Iterer() une fois par "F5" ; Reinitialiser = "F1".
class AdaBoostPasAPas {
public:
    Statut Initialiser(const cv::Mat& x, const std::vector<int>& y_moins1_plus1, TypeFaible type);
    // Ajoute une iteration ; renvoie faux (sans erreur) si le critere d'arret est atteint (raison dans Arret()).
    Resultat<bool> Iterer(ObservateurTrace* observateur);
    void Reinitialiser();
    double Score(const float* x) const;            // somme alpha_t h_t(x)
    int Decider(const float* x) const;             // signe du score (0 -> +1)
    const std::vector<IterationAdaBoost>& Iterations() const { return m_iterations; }
    const std::vector<double>& Poids() const { return m_poids; }   // d_i courants
    const std::string& Arret() const { return m_arret; }
    ValeurJson VersJson() const;
private:
    cv::Mat m_x;
    std::vector<int> m_y;
    TypeFaible m_type = TypeFaible::Seuil;
    std::vector<double> m_poids;
    std::vector<IterationAdaBoost> m_iterations;
    std::vector<double> m_scores;                   // score cumule par echantillon d'apprentissage
    double m_sommeGamma2 = 0;
    std::string m_arret;
};
// Trace emise par Iterer : phase "iteration", grandeurs {epsilon, alpha, Z, erreur_apprentissage, borne},
// m_vecteur = poids d_i apres mise a jour (float).

// Multiclasse un-contre-tous [idee : le cours ne traite pas le multiclasse] : un AdaBoost par classe,
// decision = classe de score maximal, egalite -> plus petit id (regle versionnee "ova-score-max/1").
class AdaBoostMulticlasse {
public:
    Statut Entrainer(const cv::Mat& x, const std::vector<int>& labels, int nbClasses, TypeFaible type, int nbIterations);
    int Decider(const float* x) const;
    const std::vector<AdaBoostPasAPas>& ParClasse() const { return m_parClasse; }
private:
    std::vector<AdaBoostPasAPas> m_parClasse;
};

// ---------- Perceptron de Rosenblatt (cours reseaux p.8-18) ----------
// y dans {0,1}, y^ = H(w.x + b), mise a jour w_j += eta (y - y^) x_j, b += eta (y - y^) ;
// exemples presentes un par un, dans un ordre melange par epoque (graine, splitmix64) ou dans l'ordre.
struct ParametresPerceptron { double m_eta = 0.1; int m_epoquesMax = 100; bool m_melanger = true; std::uint64_t m_graine = 1;
                              std::vector<double> m_poidsInitiaux; double m_biaisInitial = 0; };
struct ResultatPerceptron {
    std::vector<double> m_poids; double m_biais = 0;
    int m_epoques = 0; bool m_converge = false;   // converge = une epoque complete sans erreur
    std::vector<int> m_erreursParEpoque;
};
// Trace : une entree PAR EXEMPLE PRESENTE (phase "exemple") avec grandeurs {epoque, indice, y, y_chapeau, erreur}
// et m_vecteur = [w..., b] apres mise a jour ; et une entree par epoque (phase "epoque", {erreurs}).
Resultat<ResultatPerceptron> EntrainerPerceptron(const cv::Mat& x, const std::vector<int>& y01,
                                                 const ParametresPerceptron& p, ObservateurTrace* observateur);

// ---------- K-means de Lloyd (cours kmeans p.3-7) ----------
// J = somme des distances au carre au centre ; affectation (egalite -> plus petit indice de centre) puis
// mise a jour (barycentre ; un centre sans point reste en place) ; arret "Aucun changement" ou iterMax.
enum class InitKmeans { Aleatoire, KmeansPlusPlus, Fournie };
struct ParametresKmeans { int m_k = 3; InitKmeans m_init = InitKmeans::KmeansPlusPlus; std::uint64_t m_graine = 1;
                          int m_iterMax = 100; cv::Mat m_centresInitiaux; };
struct ResultatKmeans { cv::Mat m_centres; std::vector<int> m_affectations; std::vector<double> m_jParPhase;
                        int m_iterations = 0; bool m_aucunChangement = false; };
// Trace : phase "affectation" puis "mise_a_jour" a chaque iteration, grandeurs {J, changements},
// m_vecteur = centres (k x p, float), m_affectations = centre de chaque ligne (affectations en vigueur a cet etat).
Resultat<ResultatKmeans> Kmeans(const cv::Mat& x, const ParametresKmeans& p, ObservateurTrace* observateur);

} // namespace decid
