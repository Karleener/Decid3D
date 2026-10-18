#include "Inspection.h"

namespace decid {

ValeurJson MatriceVersJson(const cv::Mat& m, std::size_t maxElements) {
    if (m.empty()) return ValeurJson::Tableau();
    if (m.total() > maxElements || m.channels() != 1) return ValeurJson();
    cv::Mat d;
    m.convertTo(d, CV_64F);
    ValeurJson lignes = ValeurJson::Tableau();
    for (int i = 0; i < d.rows; ++i) {
        ValeurJson ligne = ValeurJson::Tableau();
        for (int j = 0; j < d.cols; ++j) ligne.Ajouter(d.at<double>(i, j));
        lignes.Ajouter(ligne);
    }
    return lignes;
}

} // namespace decid
