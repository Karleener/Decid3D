#include "Graphique.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace studio {

namespace {
std::string F(float v) {
    char b[32];
    std::snprintf(b, sizeof b, "%.2f", v);
    return b;
}
} // namespace

std::string NombreFr(double v, int chiffres) {
    char b[48];
    // pas d'ecriture scientifique pour les nombres usuels : 1000 et non 1e+03
    const double a = std::abs(v);
    if (a >= 1 && a < 1e7) chiffres = std::max(chiffres, (int)std::floor(std::log10(a)) + 1);
    std::snprintf(b, sizeof b, "%.*g", chiffres, v);
    std::string s(b);
    for (auto& c : s) if (c == '.') c = ',';
    return s;
}

void Graphique::Ligne(const std::vector<std::pair<float, float>>& p, int genre, float epaisseur) {
    if (p.size() < 2) return;
    std::string c = "M" + F(p[0].first) + " " + F(p[0].second);
    for (size_t i = 1; i < p.size(); ++i) c += " L" + F(p[i].first) + " " + F(p[i].second);
    m_traces.push_back({slint::SharedString(c), genre, epaisseur});
}

void Graphique::Pointilles(const std::vector<std::pair<float, float>>& p, float trait, float vide, int genre, float epaisseur) {
    std::string c;
    bool dessine = true;
    float reste = trait;
    for (size_t i = 1; i < p.size(); ++i) {
        float x0 = p[i - 1].first, y0 = p[i - 1].second;
        const float dx = p[i].first - x0, dy = p[i].second - y0, l = std::hypot(dx, dy);
        float fait = 0;
        while (fait < l) {
            const float pas = std::min(reste, l - fait);
            const float x1 = x0 + dx * pas / l, y1 = y0 + dy * pas / l;
            if (dessine) c += "M" + F(x0) + " " + F(y0) + " L" + F(x1) + " " + F(y1) + " ";
            x0 = x1; y0 = y1; fait += pas; reste -= pas;
            if (reste <= 1e-4f) { dessine = !dessine; reste = dessine ? trait : vide; }
        }
    }
    if (!c.empty()) m_traces.push_back({slint::SharedString(c), genre, epaisseur});
}

void Graphique::Bande(const std::vector<std::pair<float, float>>& haut, const std::vector<std::pair<float, float>>& bas) {
    if (haut.size() < 2 || bas.size() != haut.size()) return;
    std::string c = "M" + F(haut[0].first) + " " + F(haut[0].second);
    for (size_t i = 1; i < haut.size(); ++i) c += " L" + F(haut[i].first) + " " + F(haut[i].second);
    for (size_t i = bas.size(); i-- > 0;) c += " L" + F(bas[i].first) + " " + F(bas[i].second);
    c += " Z";
    m_traces.push_back({slint::SharedString(c), 2, 0});
}

void Graphique::Point(float x, float y, float r, int genre, float epaisseur) {
    const std::string c = "M" + F(x - r) + " " + F(y) + " a" + F(r) + " " + F(r) + " 0 1 0 " + F(2 * r) + " 0 a" + F(r) + " " + F(r) + " 0 1 0 " + F(-2 * r) + " 0 Z";
    m_traces.push_back({slint::SharedString(c), genre, epaisseur});
}

void Graphique::Segment(float x0, float y0, float x1, float y1, int genre, float epaisseur) {
    m_traces.push_back({slint::SharedString("M" + F(x0) + " " + F(y0) + " L" + F(x1) + " " + F(y1)), genre, epaisseur});
}

void Graphique::Texte(float x, float y, const std::string& t, int ancre, bool fort) {
    m_etiquettes.push_back({x, y, slint::SharedString(t), ancre, fort});
}

} // namespace studio
