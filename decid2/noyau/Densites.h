// Bayes et estimation de densite (cours : chap. 2 Bayes, Parzen, k-PPV ; TD Bayes et Parzen).
// Implementation pedagogique Decid, transparente : sert a montrer le seuil de Bayes, les a posteriori,
// la surface d'erreur et l'effet de la fenetre h. Une variable (1D) ; la 2D viendra si un TP l'exige.
#pragma once
#include <string>
#include <vector>
#include "Erreur.h"
#include "Json.h"

namespace decid {

// Grille reguliere [debut, fin] de n points (n >= 2), bornes incluses.
std::vector<double> Grille(double debut, double fin, int n);

// Gaussienne parametrique par classe (moyenne, ecart-type estime en n-1), a priori par denombrement
// ou fournis (somme 1), couts de decision (cout[i][j] = cout de decider j quand la verite est i, 0 sur la diagonale).
struct ModeleBayes1D {
    std::vector<double> m_moyennes, m_ecartsTypes, m_aPriori;
    std::vector<std::vector<double>> m_couts;
    ValeurJson VersJson() const;
};
Resultat<ModeleBayes1D> AjusterBayes1D(const std::vector<double>& valeurs, const std::vector<int>& classes, int nbClasses,
                                       const std::vector<double>& aPrioriImposes /* vide = denombrement */,
                                       const std::vector<std::vector<double>>& couts /* vide = 0/1 */);

struct CourbesBayes1D {
    std::vector<double> m_x;
    std::vector<std::vector<double>> m_densites;      // f_k(x)
    std::vector<std::vector<double>> m_posteriori;    // P(C_k | x), somme 1 en chaque x
    std::vector<int> m_decision;                       // classe de risque minimal en chaque x
    std::vector<double> m_seuils;                      // abscisses ou la decision change (interpolees)
    double m_erreurBayes = 0;                          // erreur theorique des gaussiennes estimees (integration)
    ValeurJson VersJson() const;
};
CourbesBayes1D CalculerCourbesBayes1D(const ModeleBayes1D& modele, const std::vector<double>& grille);

// Parzen 1D : p_N(x) = (1/N) somme (1/h) phi((x - x_i)/h) ; noyau "RECTANGLE" (phi = 1 si |u| <= 1/2) ou
// "GAUSSIEN". Renvoie aussi la contribution de chaque echantillon (pour la visualiser empilee) si demande.
struct EstimationParzen1D {
    std::vector<double> m_x, m_densite;
    std::vector<std::vector<double>> m_contributions; // une courbe par echantillon (vide si non demande)
    double m_integrale = 0;                           // controle : proche de 1 sur une grille assez large
};
Resultat<EstimationParzen1D> Parzen1D(const std::vector<double>& echantillons, double h, const std::string& noyau,
                                      const std::vector<double>& grille, bool avecContributions);

// k-PPV 1D : p_N(x) = k / (N * V(x)), V(x) = 2 * distance au k-ieme voisin (cours chap. 2 p.18)
Resultat<std::vector<double>> DensiteKppv1D(const std::vector<double>& echantillons, int k,
                                            const std::vector<double>& grille);

} // namespace decid
