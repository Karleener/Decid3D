// Score de confiance des decisions : marge entre la classe choisie et la suivante, ramenee a [0, 1]
// (0 = sur la frontiere, 1 = decision franche). Sert a rendre les frontieres en degrade de confiance.
// La classe reste donnee par Predire ; le score ne la modifie jamais.
//
// Definition par famille (texte publie dans image.json / volume.json) :
//  SVM 2 classes : min(|f(x)|, 1), f = valeur de decision OpenCV (RAW_OUTPUT) ; 1 = hors de la marge du SVM.
//                  SVM a plus de 2 classes (un-contre-un) : non defini (pas de valeur de decision unique).
//  k-PPV         : (votes de la 1re classe - votes de la 2e) / k parmi les k voisins.
//  Foret         : (votes de la 1re classe - votes de la 2e) / nombre d'arbres.
//  Bayes normal  : p1 - p2, probabilites a posteriori normalisees (a priori egaux, comme OpenCV).
//  EM            : p1 - p2, probabilites a posteriori des clusters cumulees par classe associee.
//  MLP (OpenCV)  : ecart des deux sorties les plus fortes (cibles one-hot 0/1), borne a [0, 1].
//  Boost         : tanh(s1 - s2), s = scores bruts un-contre-tous (PREDICT_SUM | RAW_OUTPUT).
#pragma once
#include <string>
#include <vector>
#include <opencv2/core.hpp>
#include "Erreur.h"
#include "Modeles.h"

namespace decid {

struct DefinitionScore {
    bool m_defini = true;
    std::string m_definition;     // texte publie
    std::string m_raison;         // si non defini
};
DefinitionScore DefinirScore(const Modele& m);

// Un score par ligne de x (CV_32F, espace du modele). Erreur si le score n'est pas defini pour ce modele.
Resultat<std::vector<float>> ScoresConfiance(const Modele& m, const cv::Mat& x);

} // namespace decid
