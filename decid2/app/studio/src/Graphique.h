// Construction des traces du composant Slint « Graphique » : chemins SVG dans un repere fixe (vb-l x vb-h).
#pragma once
#include <string>
#include <utility>
#include <vector>

#include "decid.h"

namespace studio {

class Graphique {
public:
    Graphique(float largeur, float hauteur) : m_l(largeur), m_h(hauteur) {}
    // genres : voir graphique.slint (0 accent, 1 gris, 2 bande, 3 axe, 4 point plein, 5 cercle)
    void Ligne(const std::vector<std::pair<float, float>>& p, int genre, float epaisseur);
    void Pointilles(const std::vector<std::pair<float, float>>& p, float trait, float vide, int genre, float epaisseur);
    void Bande(const std::vector<std::pair<float, float>>& haut, const std::vector<std::pair<float, float>>& bas);
    void Point(float x, float y, float r, int genre, float epaisseur = 2.0f);
    void Segment(float x0, float y0, float x1, float y1, int genre, float epaisseur);
    void Texte(float x, float y, const std::string& t, int ancre, bool fort = false);
    std::vector<Trace> Traces() const { return m_traces; }
    std::vector<Etiquette> Etiquettes() const { return m_etiquettes; }

private:
    float m_l, m_h;
    std::vector<Trace> m_traces;
    std::vector<Etiquette> m_etiquettes;
};

std::string NombreFr(double v, int chiffres);   // chiffres significatifs, virgule decimale

} // namespace studio
