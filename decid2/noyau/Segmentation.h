// Atelier image (ancien Decid : Image -> Segmentation Lab / HSV / RGB / YCrCb, webcam) : segmentation couleur par
// fenetres glissantes. Caracteristique d'un pixel = moyenne de chaque canal de l'espace couleur sur la fenetre
// carree centree (cote impair), divisee par 255, comme l'ancien Decid (cvtColor 8 bits puis moyenne / 255).
// Deux operations partagent exactement cette caracteristique :
//  "echantillonner_couleurs" : fenetres tirees (graine) dans des zones etiquetees -> CSV d'apprentissage
//                              (echantillons.csv + echantillons.json) a entrainer ensuite comme toute experience ;
//  "segmenter"               : chaque pixel classe par le modele publie -> segmentation.i16 (classes),
//                              segmentation.png (colorisee, palette des classes), segmentation.json.
#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <opencv2/core.hpp>
#include "Erreur.h"
#include "Json.h"

namespace decid {

namespace fs = std::filesystem;

enum class EspaceCouleur { Lab, Hsv, Rgb, YCrCb };
const char* NomEspace(EspaceCouleur e);
bool EspaceDepuisNom(const std::string& nom, EspaceCouleur& e);
std::vector<std::string> NomsCanaux(EspaceCouleur e);   // ex. {"L","a","b"}

// Image source : PNG ou JPEG lus en memoire (chemins Unicode), 3 canaux BGR 8 bits, au plus 4 millions de pixels.
Resultat<cv::Mat> LireImageCouleur(const fs::path& chemin);

// Caracteristiques de tous les pixels : CV_32FC3, valeurs dans [0,1]. fenetre impaire >= 1 (bord reflechi).
Resultat<cv::Mat> CaracteristiquesCouleur(const cv::Mat& bgr, EspaceCouleur espace, int fenetre);

struct ZoneEtiquetee { std::string m_classe; int m_x = 0, m_y = 0, m_largeur = 0, m_hauteur = 0; };

Statut ExecuterEchantillonnage(const ValeurJson& requete, const fs::path& dossierRequete, const fs::path& staging,
                               const std::atomic<bool>& annulation);
Statut ExecuterSegmentation(const ValeurJson& requete, const fs::path& dossierRequete, const fs::path& staging,
                            const std::atomic<bool>& annulation);

// Validation sans execution (orchestrateur) : parametres et presence de l'image.
Statut ValiderEchantillonnage(const ValeurJson& requete, const fs::path& dossierRequete);
Statut ValiderParametresSegmentation(const ValeurJson& requete, const fs::path& dossierRequete);

} // namespace decid
