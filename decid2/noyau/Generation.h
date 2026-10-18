// Generation de jeux synthetiques (operation "generer") : gaussiennes 2D et 3D, spirales 3D.
// Reprend Edition -> Gaussiennes 2D / 3D et Spirales 3D de l'ancien Decid (ChildFrm.cpp), avec une graine
// explicite et un generateur portable (splitmix64 + Box-Muller) : meme requete => memes octets publies.
// Fichiers ecrits dans le staging : generation.csv (format Decid : classe puis attributs, sans en-tete,
// 17 chiffres significatifs) et generation.json (description complete, hash du CSV).
#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include "Erreur.h"
#include "Json.h"

namespace decid {

namespace fs = std::filesystem;

enum class ModeleGeneration { Gaussiennes2D, Gaussiennes3D, Spirales3D };
const char* NomModeleGeneration(ModeleGeneration m);

// Une classe generee. Gaussiennes : centre et ecart-type par axe (taille 2 ou 3).
// Spirales (ressort de l'ancien Decid) : centre = origine de l'axe, diametre, longueur le long de l'axe,
// tours, phase (radians), bruit gaussien isotrope ; axe unitaire tire avec la graine s'il n'est pas donne.
struct ClasseGeneree {
    std::string m_nom;
    int m_effectif = 100;
    std::vector<double> m_centre;
    std::vector<double> m_ecartType;
    double m_diametre = 0.3, m_longueur = 0.6, m_tours = 3, m_phase = 0, m_bruit = 0.01;
    std::vector<double> m_axe;      // vide = tire avec la graine
};

struct RequeteGeneration {
    ModeleGeneration m_modele = ModeleGeneration::Gaussiennes2D;
    std::uint64_t m_graine = 1;
    std::vector<ClasseGeneree> m_classes;
    bool m_ecreter = false;         // ancien Decid 3D : valeurs ramenees dans [m_min, m_max]
    double m_min = 0.0, m_max = 1.0;
    int Dimension() const { return m_modele == ModeleGeneration::Gaussiennes2D ? 2 : 3; }
};

// Lecture et controle complet (sans rien ecrire) : utilisee aussi par la validation de l'orchestrateur.
Resultat<RequeteGeneration> RequeteGenerationDepuisJson(const ValeurJson& j);

struct JeuGenere {
    int m_dimension = 2;
    std::vector<int> m_labels;            // indice de classe de chaque ligne (ordre des classes)
    std::vector<double> m_valeurs;        // lignes x dimension
    std::vector<std::vector<double>> m_axes;   // spirales : axe effectivement utilise par classe
};
Resultat<JeuGenere> Generer(const RequeteGeneration& r, const std::atomic<bool>* annulation);

// Operation complete : lit la requete, genere, ecrit generation.csv puis generation.json.
Statut ExecuterGeneration(const ValeurJson& requete, const fs::path& staging, const std::atomic<bool>& annulation);

} // namespace decid
