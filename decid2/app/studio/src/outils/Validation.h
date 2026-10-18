// Outil « Validation croisee et balayage » : operations validation et balayage du moteur, lecture de validation.json,
// balayage.json et points_fnr_fpr.json, courbe d'exactitude et graphe FNR / (100 - FPR) construits en traces.
#pragma once
#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "Etude.h"
#include "Moteur.h"
#include "decid.h"

namespace studio {

class Validation {
public:
    Validation(Fenetre& fenetre, Moteur& moteur, Etude& etude);
    bool Occupe() const { return m_occupe; }
    void Lancer(bool balayage);               // pour la verification sans souris
    void MontrerTache(const std::string& tache);   // experience « validation » rouverte depuis les Experiences

private:
    void ChoisirJeu(int i);
    void ChoisirFamille(int i);
    void MajParametres();
    void MajGrille();
    decid::ValeurJson Base() const;
    std::string Protocole() const;
    void AfficherValidation(const fs::path& resultat, const std::string& tache);
    void AfficherBalayage(const fs::path& resultat);
    void Ajouter();

    Fenetre& m_fenetre;
    Moteur& m_moteur;
    Etude& m_etude;
    std::map<std::string, std::string> m_valeurs;   // « famille.parametre » -> valeur
    std::map<std::string, std::array<std::string, 3>> m_grilles;   // « famille.parametre » -> debut, fin, par decade
    std::optional<decid::JeuDeDonnees> m_jeu;
    bool m_occupe = false;
    std::string m_tacheValidation;
    double m_scoreValidation = -1;
    std::string m_protocoleValidation;
};

} // namespace studio
