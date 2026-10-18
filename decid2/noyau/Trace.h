// Contrat "trace" : etats successifs reels d'un apprentissage pas a pas, bornes en nombre et en memoire.
// Aucun cout si aucun observateur n'est branche. Chaque etat est un etat reellement calcule.
#pragma once
#include <cstddef>
#include <string>
#include <utility>
#include <vector>
#include "Json.h"

namespace decid {

struct EtatTrace {
    int m_iteration = 0;
    std::string m_phase;                                   // ex. "affectation", "mise_a_jour"
    std::vector<std::pair<std::string, double>> m_grandeurs; // ex. {"J", 67.7}, {"epsilon", 0.31}
    std::vector<float> m_vecteur;                          // ex. poids des echantillons, centres
    std::vector<int> m_affectations;                       // ex. k-means : centre de chaque ligne (vide sinon)
    std::size_t Octets() const;
};

class ObservateurTrace {
public:
    virtual ~ObservateurTrace() = default;
    virtual void Recevoir(const EtatTrace& etat) = 0;
};

// Collecteur borne : au-dela de m_maxEtats ou m_maxOctets, ne garde qu'un etat sur deux (pas double),
// le premier et le dernier etat etant toujours conserves.
class CollecteurTrace : public ObservateurTrace {
public:
    CollecteurTrace(std::size_t maxEtats, std::size_t maxOctets);
    void Recevoir(const EtatTrace& etat) override;
    void Terminer();                               // fixe le dernier etat recu comme conserve
    const std::vector<EtatTrace>& Etats() const { return m_etats; }
    int Pas() const { return m_pas; }
    std::size_t Octets() const { return m_octets; }
    ValeurJson VersJson() const;                   // grandeurs seulement (vecteurs en binaire a part)
private:
    void Decimer();
    std::size_t m_maxEtats, m_maxOctets;
    std::vector<EtatTrace> m_etats;
    EtatTrace m_dernier;
    bool m_aDernier = false;
    int m_pas = 1;
    long m_recus = 0;
    std::size_t m_octets = 0;
};

} // namespace decid
