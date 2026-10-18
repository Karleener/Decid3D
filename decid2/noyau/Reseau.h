// Reseau de neurones pedagogique implemente par Decid (sans OpenCV), pour l'apprentissage pas a pas.
// Motif : l'apprentissage epoque par epoque d'OpenCV (ANN_MLP::train + UPDATE_WEIGHTS) repart a chaque appel
// d'un pas RPROP ou d'un moment nuls, et stagne sur la spirale. Ici l'etat de l'optimiseur est conserve.
//
// Conventions (publiees dans l'inspection et reprises par l'interface) :
//  - couche c (c >= 1) : a_c = f(W_c^T [a_(c-1) ; 1]) ; W_c stocke en (entrees + 1) x neurones, ligne par ligne,
//    derniere ligne = biais ; trace.f32 concatene les W_c dans l'ordre des couches ;
//  - couches cachees : activation choisie ; sortie : softmax ; perte : entropie croisee moyenne + l2/2 sum(w^2)
//    (biais exclus de la regularisation) ;
//  - SIGMOIDE_SYM = 1,7159 tanh(x/3) : fonction effectivement calculee par OpenCV (mesure du 16/10/2026).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "Erreur.h"
#include "Json.h"

namespace decid {

struct ParametresReseau {
    std::vector<int> m_couchesCachees{8};
    std::string m_activation = "TANH";      // TANH, SIGMOIDE_SYM, SIGMOIDE, RELU, IDENTITE
    std::string m_methode = "RPROP";        // RPROP (lot complet, iRprop-) ou RETROPROPAGATION (mini-lots, moment)
    double m_eta = 0.1, m_moment = 0.9;     // RETROPROPAGATION seulement
    int m_tailleLot = 16;                   // RETROPROPAGATION seulement
    double m_l2 = 0.0;
    int m_epoques = 300;
    double m_epsilon = 0.0;                 // arret si |perte(t) - perte(t-1)| < epsilon ; 0 = jamais
    std::uint64_t m_graine = 1;
};
Statut VerifierParametresReseau(const ParametresReseau& p, int nbEntrees, int nbClasses);

struct MesuresReseau {
    double m_perte = 0;
    double m_exactitude = 0;
};

class Reseau {
public:
    // tailles = {entrees, cachees..., classes} ; poids initialises (Xavier, He pour RELU), biais nuls
    Reseau(const std::vector<int>& tailles, const std::string& activation, std::uint64_t graine);
    static Resultat<Reseau> DepuisPoids(const std::vector<int>& tailles, const std::string& activation,
                                        const float* poids, std::size_t nbPoids);

    // Activations de chaque couche (couche 0 = entree recue telle quelle)
    void Propager(const double* x, std::vector<std::vector<double>>& activations) const;
    int Classe(const double* x) const;
    // x : n lignes de NbEntrees() valeurs, a la suite
    MesuresReseau Mesurer(const std::vector<double>& x, const std::vector<int>& y, double l2) const;
    // Gradient de la perte moyenne sur les lignes donnees (meme disposition que les poids)
    std::vector<std::vector<double>> Gradient(const std::vector<double>& x, const std::vector<int>& y,
                                              const std::vector<int>& lignes, double l2) const;

    int NbEntrees() const { return m_tailles.front(); }
    int NbCouches() const { return (int)m_tailles.size() - 1; }
    const std::vector<int>& Tailles() const { return m_tailles; }
    const std::string& Activation() const { return m_activation; }
    std::vector<std::vector<double>>& Poids() { return m_poids; }
    const std::vector<std::vector<double>>& Poids() const { return m_poids; }
    std::vector<float> PoidsConcatenes() const;
    std::size_t NbParametres() const;
    ValeurJson Inspection() const;

private:
    double F(double z) const;
    double DeriveeF(double z, double a) const;
    std::vector<int> m_tailles;
    std::string m_activation;
    std::vector<std::vector<double>> m_poids;     // par couche : (entrees + 1) * neurones
};

// Apprentissage epoque par epoque ; l'etat de l'optimiseur (pas RPROP, vitesses) est conserve entre epoques.
class EntraineurReseau {
public:
    EntraineurReseau(Reseau& reseau, const ParametresReseau& p, const std::vector<double>& x, const std::vector<int>& y);
    MesuresReseau Epoque();                   // une epoque, puis mesure sur l'apprentissage
private:
    void PasRprop();
    void PasRetropropagation();
    Reseau& m_reseau;
    ParametresReseau m_p;
    const std::vector<double>& m_x;
    const std::vector<int>& m_y;
    std::vector<int> m_toutes;
    std::vector<std::vector<double>> m_pas, m_gradientPrecedent, m_vitesse;
    std::uint64_t m_etatAlea;
};

} // namespace decid
