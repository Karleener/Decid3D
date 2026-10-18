#include "RenduPlan.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace app {

namespace {

constexpr Couleur PALETTE[NB_COULEURS_CLASSES] = {
    {0x7A, 0x1F, 0x3D}, {0xFA, 0xCB, 0x39}, {0xFD, 0x9C, 0xF5}, {0xCB, 0xD8, 0xA5},
    {0xA8, 0x54, 0x19}, {0x93, 0x39, 0x8E}, {0xA8, 0x88, 0x91}, {0x6C, 0x0B, 0x04}};
constexpr Couleur CREUX = {0xEC, 0xE5, 0xDA};
constexpr Couleur TEXTE = {0x26, 0x23, 0x22};
constexpr Couleur PRIMAIRE = {0x64, 0x1F, 0x32};

std::uint8_t Arrondi(float v) { return (std::uint8_t)std::clamp((int)(v + 0.5f), 0, 255); }

// Distance signee a un triangle equilateral pointe vers le haut (repere : y vers le haut), rayon r.
float DistanceTriangle(float px, float py, float r) {
    const float k = 1.7320508f;
    float x = std::fabs(px) - r;
    float y = py + r / k;
    if (x + k * y > 0.f) {
        float nx = (x - k * y) * 0.5f, ny = (-k * x - y) * 0.5f;
        x = nx;
        y = ny;
    }
    x -= std::clamp(x, -2.f * r, 0.f);
    float l = std::sqrt(x * x + y * y);
    return y > 0 ? -l : l;
}

float DistanceForme(FormePoint forme, float dx, float dy, float r) {
    switch (forme) {
    case FormePoint::Disque: return std::sqrt(dx * dx + dy * dy) - r;
    case FormePoint::Triangle: return DistanceTriangle(dx, -dy, r * 1.2f);
    case FormePoint::Carre: return std::max(std::fabs(dx), std::fabs(dy)) - r * 0.88f;
    case FormePoint::Losange: return (std::fabs(dx) + std::fabs(dy)) * 0.70710678f - r * 1.05f;
    }
    return 1e9f;
}

float RayonEnglobant(FormePoint forme, float r) {
    return forme == FormePoint::Disque ? r : r * 1.4f;
}

// Dessine un point antialiase (contour de 1 px). `opacite` module l'ensemble.
void DessinerPoint(ImagePlan& img, float cx, float cy, float r, FormePoint forme, int classe, float opacite) {
    const float R = RayonEnglobant(forme, r);
    const int x0 = std::max(0, (int)std::floor(cx - R - 2)), x1 = std::min(img.m_largeur - 1, (int)std::ceil(cx + R + 2));
    const int y0 = std::max(0, (int)std::floor(cy - R - 2)), y1 = std::min(img.m_hauteur - 1, (int)std::ceil(cy + R + 2));
    const Couleur remplissage = CouleurClasse(classe);
    const Couleur contour = TEXTE;
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const float d = DistanceForme(forme, x + 0.5f - cx, y + 0.5f - cy, r);
            const float a = std::clamp(0.5f - d, 0.f, 1.f) * opacite;
            if (a <= 0.f) continue;
            const float t = std::clamp(d + 1.5f, 0.f, 1.f);   // anneau de contour de 1 px
            PixelRgba& q = img.Pixel(x, y);
            q.m_r = Arrondi(q.m_r * (1.f - a) + (remplissage.m_r * (1.f - t) + contour.m_r * t) * a);
            q.m_g = Arrondi(q.m_g * (1.f - a) + (remplissage.m_g * (1.f - t) + contour.m_g * t) * a);
            q.m_b = Arrondi(q.m_b * (1.f - a) + (remplissage.m_b * (1.f - t) + contour.m_b * t) * a);
        }
    }
}

// Anneau de surlignage (couleur primaire, 2 px) autour d'un point.
void DessinerAnneau(ImagePlan& img, float cx, float cy, float rInterieur) {
    const float R = rInterieur + 4.f;
    const int x0 = std::max(0, (int)std::floor(cx - R - 2)), x1 = std::min(img.m_largeur - 1, (int)std::ceil(cx + R + 2));
    const int y0 = std::max(0, (int)std::floor(cy - R - 2)), y1 = std::min(img.m_hauteur - 1, (int)std::ceil(cy + R + 2));
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            const float d = std::sqrt(dx * dx + dy * dy);
            const float milieu = rInterieur + 2.5f;
            const float a = std::clamp(1.0f - (std::fabs(d - milieu) - 1.0f), 0.f, 1.f);
            if (a <= 0.f) continue;
            PixelRgba& q = img.Pixel(x, y);
            q.m_r = Arrondi(q.m_r * (1.f - a) + PRIMAIRE.m_r * a);
            q.m_g = Arrondi(q.m_g * (1.f - a) + PRIMAIRE.m_g * a);
            q.m_b = Arrondi(q.m_b * (1.f - a) + PRIMAIRE.m_b * a);
        }
    }
}

} // namespace

Couleur CouleurClasse(int classe) {
    return PALETTE[((classe % NB_COULEURS_CLASSES) + NB_COULEURS_CLASSES) % NB_COULEURS_CLASSES];
}

Couleur CouleurFond(int classe) {
    const Couleur c = CouleurClasse(classe);
    return {Arrondi(CREUX.m_r * 0.65f + c.m_r * 0.35f), Arrondi(CREUX.m_g * 0.65f + c.m_g * 0.35f),
            Arrondi(CREUX.m_b * 0.65f + c.m_b * 0.35f)};
}

Couleur CouleurCreux() { return CREUX; }
Couleur CouleurTexte() { return TEXTE; }
Couleur CouleurPrimaire() { return PRIMAIRE; }

FormePoint FormeClasse(int classe) {
    switch (((classe % 4) + 4) % 4) {
    case 0: return FormePoint::Disque;
    case 1: return FormePoint::Triangle;
    case 2: return FormePoint::Carre;
    default: return FormePoint::Losange;
    }
}

void DonneesVersPixel(const Domaine& d, int largeur, int hauteur, double x, double y, double& px, double& py) {
    px = (x - d.m_xMin) / (d.m_xMax - d.m_xMin) * largeur;
    py = (d.m_yMax - y) / (d.m_yMax - d.m_yMin) * hauteur;
}

void PixelVersDonnees(const Domaine& d, int largeur, int hauteur, double px, double py, double& x, double& y) {
    x = d.m_xMin + px / largeur * (d.m_xMax - d.m_xMin);
    y = d.m_yMax - py / hauteur * (d.m_yMax - d.m_yMin);
}

ImagePlan RendrePlan(int largeur, int hauteur, const ImageDecision* fond, const Domaine& domaine,
                     const std::vector<PointPlan>& points, const OptionsPlan& options) {
    ImagePlan img;
    img.m_largeur = std::max(largeur, 1);
    img.m_hauteur = std::max(hauteur, 1);
    img.m_pixels.assign((size_t)img.m_largeur * img.m_hauteur, PixelRgba{CREUX.m_r, CREUX.m_g, CREUX.m_b, 255});

    if (fond && fond->Valide()) {
        // une couleur par classe, calculee une fois ; echantillonnage au plus proche voisin
        PixelRgba couleurs[NB_COULEURS_CLASSES];
        for (int c = 0; c < NB_COULEURS_CLASSES; ++c) {
            const Couleur f = CouleurFond(c);
            couleurs[c] = PixelRgba{f.m_r, f.m_g, f.m_b, 255};
        }
        for (int py = 0; py < img.m_hauteur; ++py) {
            const int sy = std::min(fond->m_hauteur - 1, (int)((long long)py * fond->m_hauteur / img.m_hauteur));
            for (int px = 0; px < img.m_largeur; ++px) {
                const int sx = std::min(fond->m_largeur - 1, (int)((long long)px * fond->m_largeur / img.m_largeur));
                const int c = std::clamp<int>(fond->m_classes[(size_t)sy * fond->m_largeur + sx], 0, NB_COULEURS_CLASSES - 1);
                img.Pixel(px, py) = couleurs[c];
            }
        }
    }

    const float echelle = (float)std::min(img.m_largeur, img.m_hauteur) / 720.f;
    const double dx = domaine.m_xMax - domaine.m_xMin, dy = domaine.m_yMax - domaine.m_yMin;
    if (!(dx > 0.0) || !(dy > 0.0)) return img;

    bool unSurligne = false;
    for (const auto& p : points) unSurligne = unSurligne || p.m_surligne;
    const float attenuation = (options.m_estomperAutres && unSurligne) ? 0.35f : 1.0f;

    auto rayon = [&](const PointPlan& p) { return p.m_rayon > 0.f ? p.m_rayon : options.m_rayonDefaut * echelle; };
    auto place = [&](const PointPlan& p, float opacite) {
        double cx, cy;
        DonneesVersPixel(domaine, img.m_largeur, img.m_hauteur, p.m_x, p.m_y, cx, cy);
        if (cx < -20 || cy < -20 || cx > img.m_largeur + 20 || cy > img.m_hauteur + 20) return;
        DessinerPoint(img, (float)cx, (float)cy, rayon(p), FormeClasse(p.m_classe), p.m_classe, opacite);
    };
    for (const auto& p : points)
        if (!p.m_surligne) place(p, attenuation);
    for (const auto& p : points) {
        if (!p.m_surligne) continue;
        double cx, cy;
        DonneesVersPixel(domaine, img.m_largeur, img.m_hauteur, p.m_x, p.m_y, cx, cy);
        if (cx < -20 || cy < -20 || cx > img.m_largeur + 20 || cy > img.m_hauteur + 20) continue;
        DessinerAnneau(img, (float)cx, (float)cy, RayonEnglobant(FormeClasse(p.m_classe), rayon(p)));
        place(p, 1.0f);
    }
    return img;
}

std::string FormatFr(double valeur, int decimales) {
    char tampon[64];
    std::snprintf(tampon, sizeof tampon, "%.*f", std::max(decimales, 0), valeur);
    for (char& c : tampon) {
        if (c == '.') c = ',';
        if (c == 0) break;
    }
    return tampon;
}

std::vector<Graduation> Graduations(double bas, double haut) {
    std::vector<Graduation> t;
    const double etendue = haut - bas;
    if (!(etendue > 0.0) || !std::isfinite(etendue)) return t;
    const double brut = etendue / 5.0;
    const double magnitude = std::pow(10.0, std::floor(std::log10(brut)));
    double pas = magnitude;
    for (double m : {1.0, 2.0, 5.0, 10.0})
        if (m * magnitude >= brut) { pas = m * magnitude; break; }
    const int decimales = std::max(0, (int)-std::floor(std::log10(pas) + 1e-9));
    for (double v = std::ceil(bas / pas - 1e-9) * pas; v <= haut + 1e-9; v += pas)
        t.push_back({(v - bas) / etendue, FormatFr(std::fabs(v) < 1e-12 ? 0.0 : v, decimales)});
    return t;
}

} // namespace app
