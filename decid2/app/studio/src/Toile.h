// Toile de dessin du plan (rendu logiciel antialiase) pour les ecrans qui superposent des elements a une image du moteur :
// demi-plans, droites, croix, trajets, ellipses, points de taille variable. Coordonnees en unites des donnees.
#pragma once
#include <cstdint>
#include <vector>

#include <slint.h>

#include "Moteur.h"
#include "Plan.h"

namespace studio {

class Toile {
public:
    enum class Encre { Texte, Gris, Accent, Fond };
    Toile(int largeur, int hauteur, bool sombre, const DomainePlan& domaine);

    void Fond(const ImageMoteur& image, float alphaRegion = -1.0f);
    void Point(double x, double y, int classe, float rayon, float opacite);            // classe < 0 : gris
    void PointEncre(double x, double y, Encre e, float rayon, float opacite);
    void Anneau(double x, double y, float rayon, Encre e, float epaisseur, float opacite);
    void Segment(double x0, double y0, double x1, double y1, int classe, float epaisseur, float opacite);   // classe < 0 : texte
    void Trajet(const std::vector<std::pair<double, double>>& p, int classe, float epaisseur, float opacite);
    // demi-plan n . p > s teinte de la couleur de classe ; sa frontiere tracee si epaisseur > 0
    void DemiPlan(double nx, double ny, double s, int classe, float alpha, float epaisseur);
    void Croix(double x, double y, int classe, float bras);
    void EllipseAxiale(double cx, double cy, double ex, double ey, int classe, float alphaRemplissage, float epaisseurContour);
    slint::Image Image() const;

    void VersPx(double x, double y, double& px, double& py) const;

private:
    struct Rgb { float r, g, b; };
    Rgb Couleur(int classe) const;
    Rgb CouleurEncre(Encre e) const;
    void Melanger(int i, int j, Rgb c, float a);
    void SegmentCouleur(double x0, double y0, double x1, double y1, Rgb c, float epaisseur, float opacite);
    template <typename F> void Peindre(double i0, double i1, double j0, double j1, Rgb c, float opacite, F distance);

    int m_l, m_h;
    bool m_sombre;
    DomainePlan m_dom;
    std::vector<Rgb> m_px;
};

} // namespace studio
