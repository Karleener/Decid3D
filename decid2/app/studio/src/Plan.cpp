#include "Plan.h"

#include <algorithm>
#include <cmath>

namespace studio {

namespace {
struct Rgb { float r, g, b; };
constexpr std::uint32_t CLASSES_SOMBRE[6] = {0xF07A62, 0x7FB2E8, 0xF2C46B, 0x6CC4B4, 0xA99BF0, 0xE58FC0};
constexpr std::uint32_t CLASSES_CLAIR[6] = {0xC2462F, 0x3672B5, 0xB8862A, 0x2F8C7E, 0x6656C2, 0xB24C8A};
Rgb Hex(std::uint32_t c) { return {float((c >> 16) & 255), float((c >> 8) & 255), float(c & 255)}; }
Rgb Classe(int k, bool sombre) { return Hex((sombre ? CLASSES_SOMBRE : CLASSES_CLAIR)[((k % 6) + 6) % 6]); }
Rgb Melange(Rgb a, Rgb b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
} // namespace

void CouleurClasse(int classe, bool sombre, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) {
    const Rgb c = Classe(classe, sombre);
    r = (std::uint8_t)c.r; g = (std::uint8_t)c.g; b = (std::uint8_t)c.b;
}

DomainePlan DomaineDesPoints(const std::vector<PointPlan>& points) {
    DomainePlan d;
    if (points.empty()) return d;
    d.m_xMin = d.m_xMax = points[0].m_x;
    d.m_yMin = d.m_yMax = points[0].m_y;
    for (const auto& p : points) {
        d.m_xMin = std::min(d.m_xMin, p.m_x); d.m_xMax = std::max(d.m_xMax, p.m_x);
        d.m_yMin = std::min(d.m_yMin, p.m_y); d.m_yMax = std::max(d.m_yMax, p.m_y);
    }
    const double mx = std::max(1e-9, d.m_xMax - d.m_xMin) * 0.05, my = std::max(1e-9, d.m_yMax - d.m_yMin) * 0.05;
    d.m_xMin -= mx; d.m_xMax += mx; d.m_yMin -= my; d.m_yMax += my;
    return d;
}

slint::Image RendrePlan(int L, int H, bool sombre, const DomainePlan& dom, const ImageMoteur* fond,
                        const std::vector<PointPlan>& points, float rayon, const DecorPlan* decor, float alphaDemande) {
    L = std::max(L, 8);
    H = std::max(H, 8);
    const Rgb plot = Hex(sombre ? 0x0F1338 : 0xFBF8F4);
    const float alphaRegion = alphaDemande >= 0 ? alphaDemande : sombre ? 0.26f : 0.16f;
    std::vector<Rgb> px((size_t)L * H, plot);

    // 1. regions : couleur par pixel source, puis echantillonnage bilineaire (frontiere lissee)
    if (fond && fond->m_largeur > 0 && fond->m_hauteur > 0) {
        const int fl = fond->m_largeur, fh = fond->m_hauteur;
        std::vector<Rgb> src((size_t)fl * fh);
        for (size_t i = 0; i < src.size(); ++i) {
            const float s = fond->m_scores.empty() ? 1.0f : fond->m_scores[i] / 255.0f;
            src[i] = Melange(plot, Classe(fond->m_classes[i], sombre), alphaRegion * (0.25f + 0.75f * s));
        }
        // le pixel (i,j) du plan correspond au point de donnees (x,y) ; le fond couvre son propre domaine
        for (int j = 0; j < H; ++j) {
            const double y = dom.m_yMax - (j + 0.5) * (dom.m_yMax - dom.m_yMin) / H;
            const double fy = (fond->m_yMax - y) / (fond->m_yMax - fond->m_yMin) * fh - 0.5;
            if (fy < -0.5 || fy > fh - 0.5) continue;
            for (int i = 0; i < L; ++i) {
                const double x = dom.m_xMin + (i + 0.5) * (dom.m_xMax - dom.m_xMin) / L;
                const double fx = (x - fond->m_xMin) / (fond->m_xMax - fond->m_xMin) * fl - 0.5;
                if (fx < -0.5 || fx > fl - 0.5) continue;
                const int x0 = std::clamp((int)std::floor(fx), 0, fl - 1), y0 = std::clamp((int)std::floor(fy), 0, fh - 1);
                const int x1 = std::min(x0 + 1, fl - 1), y1 = std::min(y0 + 1, fh - 1);
                const float tx = (float)std::clamp(fx - x0, 0.0, 1.0), ty = (float)std::clamp(fy - y0, 0.0, 1.0);
                const Rgb haut = Melange(src[(size_t)y0 * fl + x0], src[(size_t)y0 * fl + x1], tx);
                const Rgb bas = Melange(src[(size_t)y1 * fl + x0], src[(size_t)y1 * fl + x1], tx);
                px[(size_t)j * L + i] = Melange(haut, bas, ty);
            }
        }
    }

    auto versPx = [&](double x, double y, double& px, double& py) {
        px = (x - dom.m_xMin) / (dom.m_xMax - dom.m_xMin) * L;
        py = (dom.m_yMax - y) / (dom.m_yMax - dom.m_yMin) * H;
    };
    // trait antialiase d'epaisseur e autour d'une courbe donnee par sa distance signee d(i,j)
    auto tracer = [&](int i0, int i1, int j0, int j1, Rgb couleur, float opacite, auto distance) {
        i0 = std::max(0, i0); j0 = std::max(0, j0); i1 = std::min(L - 1, i1); j1 = std::min(H - 1, j1);
        for (int j = j0; j <= j1; ++j)
            for (int i = i0; i <= i1; ++i) {
                const float a = std::clamp(1.0f - (float)distance(i + 0.5, j + 0.5), 0.0f, 1.0f) * opacite;
                if (a > 0) px[(size_t)j * L + i] = Melange(px[(size_t)j * L + i], couleur, a);
            }
    };

    // 2. points : disque antialiase, liseré couleur du fond pour la lisibilite sur les regions ; ceux « devant » en dernier
    const float r0 = std::max(1.5f, rayon);
    std::vector<const PointPlan*> ordre;
    for (const auto& p : points) if (!p.m_devant) ordre.push_back(&p);
    for (const auto& p : points) if (p.m_devant) ordre.push_back(&p);
    for (const PointPlan* q : ordre) {
        const PointPlan& p = *q;
        const float r = p.m_devant ? r0 * 1.35f : r0, bord = std::max(1.0f, r * 0.35f);
        double cx, cy;
        versPx(p.m_x, p.m_y, cx, cy);
        const Rgb c = Classe(p.m_classe, sombre);
        const float opacite = p.m_opacite >= 0 ? p.m_opacite : p.m_attenue ? 0.35f : 0.92f;
        const int i0 = std::max(0, (int)std::floor(cx - r - bord - 1)), i1 = std::min(L - 1, (int)std::ceil(cx + r + bord + 1));
        const int j0 = std::max(0, (int)std::floor(cy - r - bord - 1)), j1 = std::min(H - 1, (int)std::ceil(cy + r + bord + 1));
        for (int j = j0; j <= j1; ++j)
            for (int i = i0; i <= i1; ++i) {
                const float d = (float)std::hypot(i + 0.5 - cx, j + 0.5 - cy);
                const float cBord = std::clamp(r + bord - d + 0.5f, 0.0f, 1.0f) * opacite * 0.85f;
                const float cDisque = std::clamp(r - d + 0.5f, 0.0f, 1.0f) * opacite;
                Rgb& o = px[(size_t)j * L + i];
                if (cBord > 0) o = Melange(o, plot, cBord);
                if (cDisque > 0) o = Melange(o, c, cDisque);
            }
    }

    // 3. interieur du modele
    if (decor) {
        const Rgb encre = Hex(sombre ? 0xEBE1D5 : 0x151946);
        const float e = std::max(1.0f, r0 * 0.3f);
        for (const auto& [x, y] : decor->m_anneaux) {
            double cx, cy;
            versPx(x, y, cx, cy);
            const double ra = r0 + 2.5;
            tracer((int)(cx - ra - 3), (int)(cx + ra + 3), (int)(cy - ra - 3), (int)(cy + ra + 3), encre, 0.8f,
                   [&](double i, double j) { return std::abs(std::hypot(i - cx, j - cy) - ra) / e; });
        }
        const double sx = L / (dom.m_xMax - dom.m_xMin), sy = H / (dom.m_yMax - dom.m_yMin);
        for (const auto& el : decor->m_ellipses) {
            double cx, cy;
            versPx(el.m_cx, el.m_cy, cx, cy);
            // covariance en pixels (y vers le bas : le terme croise change de signe)
            const double a = el.m_vxx * sx * sx, b = -el.m_vxy * sx * sy, d = el.m_vyy * sy * sy;
            const double det = a * d - b * b;
            if (det <= 1e-12) continue;
            const double ia = d / det, ib = -b / det, id = a / det;     // inverse : distance de Mahalanobis
            const double demi = std::sqrt(std::max(a, d)) * 2.2 + 4;
            for (int k : {1, 2}) {
                const float ep = k == 1 ? 1.6f : 1.0f;
                tracer((int)(cx - demi), (int)(cx + demi), (int)(cy - demi), (int)(cy + demi), Classe(el.m_classe, sombre), k == 1 ? 0.95f : 0.7f,
                       [&](double i, double j) {
                           const double u = i - cx, v = j - cy;
                           const double m = std::sqrt(std::max(0.0, ia * u * u + 2 * ib * u * v + id * v * v));
                           // distance approchee a la courbe m = k : |m - k| divise par le gradient de m
                           const double gu = (ia * u + ib * v) / std::max(m, 1e-9), gv = (ib * u + id * v) / std::max(m, 1e-9);
                           return std::abs(m - k) / std::max(std::hypot(gu, gv), 1e-9) / ep;
                       });
            }
        }
    }

    slint::SharedPixelBuffer<slint::Rgba8Pixel> tampon(L, H);
    auto* sortie = tampon.begin();
    for (size_t k = 0; k < px.size(); ++k)
        sortie[k] = {(std::uint8_t)std::lround(px[k].r), (std::uint8_t)std::lround(px[k].g), (std::uint8_t)std::lround(px[k].b), 255};
    return slint::Image(tampon);
}

} // namespace studio
