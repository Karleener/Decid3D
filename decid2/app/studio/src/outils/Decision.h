// Outil « Appliquer un modele a un fichier » : operation decider du moteur sur le modele d'une experience de l'etude.
#pragma once
#include <string>
#include <utility>
#include <vector>

#include "Etude.h"
#include "Moteur.h"
#include "decid.h"

namespace studio {

class Decision {
public:
    Decision(Fenetre& fenetre, Moteur& moteur, Etude& etude);
    void Actualiser();          // liste des modeles entraines (a chaque ouverture de l'outil)
    void Appliquer();
    bool Occupe() const { return m_calcul; }

private:
    void ChoisirModele(int i);

    Fenetre& m_fenetre;
    Moteur& m_moteur;
    Etude& m_etude;
    std::vector<std::pair<std::string, std::string>> m_modeles;   // titre, tache
    fs::path m_fichier;
    std::string m_tacheDecision;
    bool m_calcul = false;
};

} // namespace studio
