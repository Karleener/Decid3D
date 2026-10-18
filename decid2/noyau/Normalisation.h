// Pretraitement : aucun, min-max ajuste sur l'apprentissage seulement, ou min-max aux bornes imposees par un
// fichier .lim de l'ancien Decid. Appliquer repart toujours des donnees brutes : pas de double normalisation possible.
#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <opencv2/core.hpp>
#include "Donnees.h"
#include "Erreur.h"
#include "Json.h"

namespace decid {

struct Normalisation {
    std::string m_methode = "aucune";     // "aucune" ou "minmax"
    double m_bas = 0.0, m_haut = 1.0;     // plage de sortie
    std::vector<double> m_min, m_max;     // ajustes sur les lignes d'apprentissage
    std::string m_hashPartage;            // partage dont les indices ont servi a l'ajustement
    std::vector<int> m_attributsConstants;// colonnes constantes sur l'apprentissage -> projetees sur m_bas

    std::string Hash() const;
    ValeurJson VersJson() const;
};

// Ajustement sur les seules lignes listees (apprentissage)
Resultat<Normalisation> AjusterMinMax(const JeuDeDonnees& jeu, const std::vector<int>& lignesApprentissage,
                                      double bas, double haut, const std::string& hashPartage);
Normalisation AucuneNormalisation(int nbAttributs);

// Fichier .lim (ancien Decid) : premiere ligne = nombre d'attributs, puis une ligne "min max" par attribut.
// Lecture stricte : lignes vides ignorees, tout le reste refuse avec son numero de ligne (nombre d'attributs
// different de la source, paire incomplete, valeur non numerique, min > max, ligne en trop ou manquante).
// Le resultat est un min-max ordinaire (methode "minmax", meme hash, meme publication) ; min == max -> constant.
Resultat<Normalisation> LireBornesLim(const std::string& texte, const std::string& nomFichier, int nbAttributs,
                                      double bas, double haut, const std::string& hashPartage);
Resultat<Normalisation> ChargerBornesLim(const std::filesystem::path& chemin, int nbAttributs, double bas,
                                         double haut, const std::string& hashPartage);

// Normalisation d'une requete : "aucune", "minmax" (ajustee sur lignesApprentissage) ou "minmax_lim" (bornes du
// fichier .lim, les memes pour toutes les lignes). m_hashPartage recoit hashPartage dans tous les cas.
Resultat<Normalisation> NormalisationDemandee(const std::string& methode, const std::filesystem::path& fichierLim,
                                              double bas, double haut, const JeuDeDonnees& jeu,
                                              const std::vector<int>& lignesApprentissage,
                                              const std::string& hashPartage);

// Matrice CV_32F (une ligne par indice) des donnees transformees ; aucune ecretage tacite.
// nbHorsPlage recoit le nombre de valeurs hors [bas,haut] apres transformation (test, nouvelles donnees).
// Erreur si une valeur ne tient pas en float32.
Resultat<cv::Mat> AppliquerNormalisation(const Normalisation& n, const JeuDeDonnees& jeu,
                                          const std::vector<int>& lignes, long* nbHorsPlage = nullptr);

// Transformation d'un vecteur brut isole (meme formule)
std::vector<double> TransformerVecteur(const Normalisation& n, const std::vector<double>& brut);

Resultat<Normalisation> NormalisationDepuisJson(const ValeurJson& json);

} // namespace decid
