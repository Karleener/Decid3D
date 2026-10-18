// Rendu logiciel du plan 2D (repris du banc Slint) : image de decision colorisee + points (forme et couleur
// par classe) + surlignage + rayons variables. Aucune dependance a Slint ni a OpenCV : le resultat est un
// tampon RGBA8 que la couche d'interface copie dans un slint::SharedPixelBuffer (voir ImageSlint.h).
//
// Conventions (contrat avec l'image du worker, image.i16) :
//  - ImageDecision::m_classes est stockee ligne par ligne, DE HAUT EN BAS : la ligne 0 correspond a y = yMax ;
//  - la colonne 0 correspond a x = xMin ;
//  - un point (x, y) est place en pixel ((x-xMin)/(xMax-xMin)*L, (yMax-y)/(yMax-yMin)*H).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace app {

struct Couleur {
    std::uint8_t m_r = 0, m_g = 0, m_b = 0;
};

// Meme disposition memoire que slint::Rgba8Pixel.
struct PixelRgba {
    std::uint8_t m_r = 0, m_g = 0, m_b = 0, m_a = 255;
};

constexpr int NB_COULEURS_CLASSES = 8;

Couleur CouleurClasse(int classe);   // palette de 8 couleurs sans bleu (indice modulo 8)
Couleur CouleurFond(int classe);     // couleur du fond de decision : 65 % creux + 35 % palette
Couleur CouleurCreux();              // fond neutre du plan
Couleur CouleurTexte();              // contour des points
Couleur CouleurPrimaire();           // surlignage

enum class FormePoint { Disque, Triangle, Carre, Losange };
FormePoint FormeClasse(int classe);  // classe % 4 : disque, triangle, carre, losange

struct ImageDecision {
    int m_largeur = 0;
    int m_hauteur = 0;
    std::vector<std::int16_t> m_classes;   // m_hauteur lignes de m_largeur classes, ligne 0 = y max
    bool Valide() const { return m_largeur > 0 && m_hauteur > 0 && m_classes.size() == (size_t)m_largeur * m_hauteur; }
};

struct Domaine {
    double m_xMin = 0.0, m_xMax = 1.0, m_yMin = 0.0, m_yMax = 1.0;
};

struct PointPlan {
    double m_x = 0.0, m_y = 0.0;   // coordonnees dans le domaine des donnees
    int m_classe = 0;
    float m_rayon = 0.0f;          // rayon en pixels ; 0 = rayon par defaut des options
    bool m_surligne = false;       // anneau de surlignage (matrice de confusion cliquable)
};

struct OptionsPlan {
    float m_rayonDefaut = 3.4f;    // pixels pour un plan de 720 px de cote ; mis a l'echelle de la taille reelle
    bool m_estomperAutres = false; // si vrai et qu'un point est surligne, les autres sont attenues
};

struct ImagePlan {
    int m_largeur = 0;
    int m_hauteur = 0;
    std::vector<PixelRgba> m_pixels;   // m_largeur x m_hauteur, ligne 0 en haut
    PixelRgba& Pixel(int x, int y) { return m_pixels[(size_t)y * m_largeur + x]; }
    const PixelRgba& Pixel(int x, int y) const { return m_pixels[(size_t)y * m_largeur + x]; }
};

// fond peut etre nul (plan neutre). Les points hors domaine sont ignores.
ImagePlan RendrePlan(int largeur, int hauteur, const ImageDecision* fond, const Domaine& domaine,
                     const std::vector<PointPlan>& points, const OptionsPlan& options = {});

// Correspondance donnees <-> pixels (utile pour le clic sur le plan).
void DonneesVersPixel(const Domaine& d, int largeur, int hauteur, double x, double y, double& px, double& py);
void PixelVersDonnees(const Domaine& d, int largeur, int hauteur, double px, double py, double& x, double& y);

struct Graduation {
    double m_fraction = 0.0;   // 0..1 le long de l'axe (de bas en haut ou de gauche a droite)
    std::string m_libelle;
};
std::vector<Graduation> Graduations(double bas, double haut);   // ~5 graduations "rondes"
std::string FormatFr(double valeur, int decimales);             // virgule decimale

} // namespace app
