// Rendu du plan 2D aux couleurs du systeme v3 : regions de decision du moteur teintees par classe, plus ou moins
// soutenues selon le score de confiance (pale pres de la frontiere), puis les points du jeu. Rendu logiciel dans un
// tampon RGBA8, converti en slint::Image ; refait seulement quand les donnees, la taille ou le theme changent.
#pragma once
#include <cstdint>
#include <vector>

#include <slint.h>

#include "Moteur.h"

namespace studio {

struct PointPlan {
    double m_x = 0, m_y = 0;   // coordonnees dans le domaine du plan
    int m_classe = 0;
    bool m_attenue = false;    // point d'apprentissage quand on montre le test, par exemple
    float m_opacite = -1;      // < 0 : opacite par defaut (0,92, ou 0,35 si attenue)
    bool m_devant = false;     // dessine apres les autres, un peu plus gros (erreur, point choisi)
};

// Interieur d'un modele, dessine par-dessus : anneaux (vecteurs de support), ellipses (gaussiennes de Bayes a 1 et 2
// ecarts-types). Les coordonnees sont celles du plan.
struct EllipsePlan {
    double m_cx = 0, m_cy = 0;
    double m_vxx = 0, m_vxy = 0, m_vyy = 0;   // covariance 2 x 2 dans le plan
    int m_classe = 0;
};
struct DecorPlan {
    std::vector<std::pair<double, double>> m_anneaux;
    std::vector<EllipsePlan> m_ellipses;
};

struct DomainePlan {
    double m_xMin = 0, m_xMax = 1, m_yMin = 0, m_yMax = 1;
};

// Couleur d'une classe (palette categorielle v3, ordre fixe, 6 teintes), selon le theme
void CouleurClasse(int classe, bool sombre, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b);

// fond : peut etre nul (nuage de points seul). largeur et hauteur en pixels physiques.
slint::Image RendrePlan(int largeur, int hauteur, bool sombre, const DomainePlan& domaine, const ImageMoteur* fond,
                        const std::vector<PointPlan>& points, float rayon, const DecorPlan* decor = nullptr,
                        float alphaRegion = -1.0f);   // < 0 : 0,26 en sombre, 0,16 en clair

// Domaine englobant des points (avec 5 % de marge), pour un apercu sans image de decision
DomainePlan DomaineDesPoints(const std::vector<PointPlan>& points);

} // namespace studio
