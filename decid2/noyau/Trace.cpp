#include "Trace.h"

namespace decid {

std::size_t EtatTrace::Octets() const {
    std::size_t total = m_vecteur.size() * sizeof(float) + m_affectations.size() * sizeof(int) + m_phase.size();
    for (const auto& g : m_grandeurs) total += g.first.size() + sizeof(double);
    return total;
}

CollecteurTrace::CollecteurTrace(std::size_t maxEtats, std::size_t maxOctets)
    : m_maxEtats(maxEtats), m_maxOctets(maxOctets) {}

void CollecteurTrace::Recevoir(const EtatTrace& etat) {
    const long rang = m_recus++;
    m_dernier = etat;
    m_aDernier = true;
    if (rang % m_pas != 0) return;
    m_etats.push_back(etat);
    m_octets += etat.Octets();
    while (m_etats.size() > 1 && (m_etats.size() > m_maxEtats || m_octets > m_maxOctets)) {
        Decimer();
    }
}

void CollecteurTrace::Decimer() {
    std::vector<EtatTrace> gardes;
    gardes.reserve(m_etats.size() / 2 + 1);
    std::size_t octets = 0;
    for (std::size_t i = 0; i < m_etats.size(); i += 2) {
        octets += m_etats[i].Octets();
        gardes.push_back(std::move(m_etats[i]));
    }
    m_etats = std::move(gardes);
    m_octets = octets;
    m_pas *= 2;
}

void CollecteurTrace::Terminer() {
    if (!m_aDernier) return;
    const bool dejaConserve = (m_recus - 1) % m_pas == 0;
    if (dejaConserve) return;
    m_etats.push_back(m_dernier);
    m_octets += m_dernier.Octets();
}

ValeurJson CollecteurTrace::VersJson() const {
    ValeurJson racine = ValeurJson::Objet();
    racine["pas"] = m_pas;
    racine["recus"] = m_recus;
    ValeurJson liste = ValeurJson::Tableau();
    for (const auto& e : m_etats) {
        ValeurJson o = ValeurJson::Objet();
        o["iteration"] = e.m_iteration;
        o["phase"] = e.m_phase;
        ValeurJson g = ValeurJson::Objet();
        for (const auto& kv : e.m_grandeurs) g[kv.first] = kv.second;
        o["grandeurs"] = g;
        liste.Ajouter(o);
    }
    racine["etats"] = liste;
    return racine;
}

} // namespace decid
