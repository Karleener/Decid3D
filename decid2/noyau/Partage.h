// Partage apprentissage/test : indices explicites, immuables, persistes.
// Jamais delegue a TrainData::setTrainTestSplitRatio (cas limites non conformes au contrat).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "Donnees.h"
#include "Erreur.h"
#include "Json.h"

namespace decid {

enum class MethodePartage {
    PremiersN,          // floor(N*ratio) premieres lignes du fichier (ordre global)
    PremiersNParClasse, // floor(n_c*ratio) premieres lignes de chaque classe, ordre du fichier
    AleatoireStratifie, // melange de chaque classe (graine), puis floor(n_c*ratio) par classe
    ParRoles            // roles des fichiers sources (apprentissage / test), aucun remelange
};
const char* NomMethode(MethodePartage m);
bool MethodeDepuisNom(const std::string& nom, MethodePartage& m);

struct Partage {
    MethodePartage m_methode = MethodePartage::PremiersN;
    double m_ratio = 0.5;                 // part demandee pour l'apprentissage (sans objet pour ParRoles)
    std::uint64_t m_graine = 0;           // utilise seulement en AleatoireStratifie
    std::string m_generateur = "splitmix64+rejet+fisher_yates_decid_v1";
    bool m_inverse = false;               // echange effectif des ensembles
    std::string m_hashJeu;                // HashCanonique du jeu partage
    std::vector<int> m_apprentissage;     // indices tries croissants
    std::vector<int> m_test;              // indices tries croissants
    std::vector<std::string> m_diagnostics; // ex. "classe 3 : 0 ligne en test"

    std::string Hash() const;             // SHA-256 de (methode, ratio, graine, inverse, hashJeu, indices)
    ValeurJson VersJson() const;
};

Resultat<Partage> CreerPartage(const JeuDeDonnees& jeu, MethodePartage methode, double ratio,
                               std::uint64_t graine, bool inverse);

// Invariants : indices dans [0,N), sans doublon, disjoints, union = N ; apprentissage non vide.
Statut ValiderPartage(const Partage& partage, int nbLignes);

Resultat<Partage> PartageDepuisJson(const ValeurJson& json);

// Comptes par classe d'un ensemble d'indices
std::vector<long> ComptesClasses(const JeuDeDonnees& jeu, const std::vector<int>& indices);

// Generateur deterministe portable : entier uniforme dans [0, borne) par rejet (independant de la STL)
std::uint64_t TirageUniforme(std::uint64_t& etat, std::uint64_t borne);

} // namespace decid
