// Images et volumes de decision calcules par le moteur (spec v2 §5.2). L'interface ne fait qu'afficher.
// Chaque pixel / voxel est classe par le vrai modele, dans l'espace brut transforme par la normalisation du modele.
#pragma once
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>
#include "Erreur.h"
#include "Json.h"
#include "Modeles.h"
#include "Normalisation.h"
#include "Scores.h"

namespace decid {

// Domaine brut et convention d'axes : x croit vers la droite, y croit vers le HAUT.
// Ligne 0 de l'image = y maximal. Centre du pixel (i, j) :
//   x = xmin + (i + 0.5) * (xmax - xmin) / largeur ; y = ymax - (j + 0.5) * (ymax - ymin) / hauteur
struct RequeteImage {
    int m_largeur = 500, m_hauteur = 500;
    int m_attributX = 0, m_attributY = 1;     // indices des attributs affiches
    double m_xmin = 0, m_xmax = 1, m_ymin = 0, m_ymax = 1;
    std::vector<double> m_valeursFixes;       // valeur brute de chaque attribut (taille = nb attributs) ; X et Y ignores
    ValeurJson VersJson() const;
};

struct ImageDecision {
    int m_largeur = 0, m_hauteur = 0;
    std::vector<std::int16_t> m_classes;      // id interne par pixel, lignes de haut en bas
    std::string m_hash;                       // SHA-256 de (m_classes) pour comparer deux calculs
    // Score de confiance par pixel (Scores.h), quantifie : u8 = round(255 x score) ; vide si non defini
    std::vector<std::uint8_t> m_scores;
    DefinitionScore m_score;
};

// Budget memoire : refuse si largeur*hauteur*2 octets > maxOctets. Prediction par lots de lignes ;
// annulation verifiee entre lots (annulation peut etre nul).
Resultat<ImageDecision> CalculerImage(const Modele& modele, const Normalisation& normalisation,
                                      const RequeteImage& requete, const std::atomic<bool>* annulation,
                                      std::size_t maxOctets = 64u << 20);

// Coordonnees brutes du centre d'un pixel (meme formule que CalculerImage ; sert aux tests ACC-13)
void CentrePixel(const RequeteImage& r, int i, int j, double& x, double& y);

// Volume cubique res^3 sur trois attributs ; ordre des voxels : z, puis y, puis x (x varie le plus vite) ;
// centre de voxel : v = min + (k + 0.5) * (max - min) / res, sur chaque axe (axes croissants).
struct RequeteVolume {
    int m_resolution = 40;
    int m_attributs[3] = {0, 1, 2};
    double m_min[3] = {0, 0, 0}, m_max[3] = {1, 1, 1};
    std::vector<double> m_valeursFixes;
    ValeurJson VersJson() const;
};
struct VolumeDecision {
    int m_resolution = 0;
    std::vector<std::int16_t> m_classes;      // res^3
    std::string m_hash;
    std::vector<std::uint8_t> m_scores;       // meme ordre que m_classes ; vide si non defini
    DefinitionScore m_score;
};
Resultat<VolumeDecision> CalculerVolume(const Modele& modele, const Normalisation& normalisation,
                                        const RequeteVolume& requete, const std::atomic<bool>* annulation,
                                        std::size_t maxOctets = 64u << 20);

// Palette versionnee sans bleu (spec UX §2.5), 8 teintes puis recyclage ; RGBA 8 bits.
struct Palette {
    std::string m_version = "decid.palette-classes/1";
    std::vector<std::uint32_t> m_couleurs;   // 0xRRGGBBAA
};
Palette PaletteParDefaut();
// Recolorisation sans recalcul : RGBA (4 octets par pixel), alpha attenue pour le fond de decision.
std::vector<std::uint8_t> Coloriser(const ImageDecision& image, const Palette& palette, std::uint8_t alpha = 255);

// Cle de cache : hash du modele (octets du fichier), de la normalisation et de la requete.
std::string CleCache(const std::string& hashModele, const Normalisation& n, const ValeurJson& requete);

} // namespace decid
