// Outils communs d'inspection : conversion de matrices OpenCV en JSON (petites tailles seulement).
#pragma once
#include <opencv2/core.hpp>
#include "Json.h"

namespace decid {

// Matrice -> tableau de lignes ; refuse (renvoie null) au-dela de maxElements.
ValeurJson MatriceVersJson(const cv::Mat& m, std::size_t maxElements = 2'000'000);

} // namespace decid
