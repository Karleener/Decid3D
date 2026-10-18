#include "Toile.h"

#include <algorithm>
#include <cmath>

namespace studio {

namespace {
constexpr std::uint32_t CLASSES_SOMBRE[6] = {0xF07A62, 0x7FB2E8, 0xF2C46B, 0x6CC4B4, 0xA99BF0, 0xE58FC0};
constexpr std::uint32_t CLASSES_CLAIR[6] = {0xC2462F, 0x3672B5, 0xB8862A, 0x2F8C7E, 0x6656C2, 0xB24C8A};
} // namespace

Toile::Toile(int L, int H, bool sombre, const DomainePlan& d) : m_l(std::max(L, 8)), m_h(std::max(H, 8)), m_sombre(sombre), m_dom(d) {
    m_px.assign((size_t)m_l * m_h, CouleurEncre(Encre::Fond));
}

Toile::Rgb Toile::Couleur(int k) const {
    if (k < 0) return CouleurEncre(Encre::Gris);
    const std::uint32_t c = (m_sombre ? CLASSES_SOMBRE : CLASSES_CLAIR)[k % 6];
    return {float((c >> 16) & 255), float((c >> 8) & 255), float(c & 255)};
}
Toile::Rgb Toile::CouleurEncre(Encre e) const {
    switch (e) {
    case Encre::Texte: return m_sombre ? Rgb{235, 225, 213} : Rgb{21, 25, 70};
    case Encre::Gris: return m_sombre ? Rgb{150, 146, 160} : Rgb{120, 122, 150};
    case Encre::Accent: return m_sombre ? Rgb{238, 139, 116} : Rgb{124, 23, 13};
    default: return m_sombre ? Rgb{15, 19, 56} : Rgb{251, 248, 244};
    }
}
void Toile::Melanger(int i, int j, Rgb c, float a) {
    if (i < 0 || j < 0 || i >= m_l || j >= m_h || a <= 0) return;
    Rgb& o = m_px[(size_t)j * m_l + i];
    a = std::min(a, 1.0f);
    o = {o.r + (c.r - o.r) * a, o.g + (c.g - o.g) * a, o.b + (c.b - o.b) * a};
}
void Toile::VersPx(double x, double y, double& px, double& py) const {
    px = (x - m_dom.m_xMin) / (m_dom.m_xMax - m_dom.m_xMin) * m_l;
    py = (m_dom.m_yMax - y) / (m_dom.m_yMax - m_dom.m_yMin) * m_h;
}
template <typename F> void Toile::Peindre(double i0, double i1, double j0, double j1, Rgb c, float opacite, F distance) {
    const int a = std::max(0, (int)std::floor(i0)), b = std::min(m_l - 1, (int)std::ceil(i1));
    const int u = std::max(0, (int)std::floor(j0)), v = std::min(m_h - 1, (int)std::ceil(j1));
    for (int j = u; j <= v; ++j)
        for (int i = a; i <= b; ++i) Melanger(i, j, c, std::clamp(1.0f - (float)distance(i + 0.5, j + 0.5), 0.0f, 1.0f) * opacite);
}

void Toile::Fond(const ImageMoteur& f, float alphaDemande) {
    const float alpha = alphaDemande >= 0 ? alphaDemande : m_sombre ? 0.26f : 0.16f;
    const Rgb fond = CouleurEncre(Encre::Fond);
    const int fl = f.m_largeur, fh = f.m_hauteur;
    if (fl <= 0 || fh <= 0) return;
    std::vector<Rgb> src((size_t)fl * fh);
    for (size_t i = 0; i < src.size(); ++i) {
        const float s = f.m_scores.empty() ? 1.0f : f.m_scores[i] / 255.0f, k = alpha * (0.25f + 0.75f * s);
        const Rgb c = Couleur(f.m_classes[i]);
        src[i] = {fond.r + (c.r - fond.r) * k, fond.g + (c.g - fond.g) * k, fond.b + (c.b - fond.b) * k};
    }
    for (int j = 0; j < m_h; ++j) {
        const double y = m_dom.m_yMax - (j + 0.5) * (m_dom.m_yMax - m_dom.m_yMin) / m_h;
        const double fy = (f.m_yMax - y) / (f.m_yMax - f.m_yMin) * fh - 0.5;
        if (fy < -0.5 || fy > fh - 0.5) continue;
        for (int i = 0; i < m_l; ++i) {
            const double x = m_dom.m_xMin + (i + 0.5) * (m_dom.m_xMax - m_dom.m_xMin) / m_l;
            const double fx = (x - f.m_xMin) / (f.m_xMax - f.m_xMin) * fl - 0.5;
            if (fx < -0.5 || fx > fl - 0.5) continue;
            const int x0 = std::clamp((int)std::floor(fx), 0, fl - 1), y0 = std::clamp((int)std::floor(fy), 0, fh - 1);
            const int x1 = std::min(x0 + 1, fl - 1), y1 = std::min(y0 + 1, fh - 1);
            const float tx = (float)std::clamp(fx - x0, 0.0, 1.0), ty = (float)std::clamp(fy - y0, 0.0, 1.0);
            auto mel = [](Rgb a, Rgb b, float t) { return Rgb{a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; };
            m_px[(size_t)j * m_l + i] = mel(mel(src[(size_t)y0 * fl + x0], src[(size_t)y0 * fl + x1], tx), mel(src[(size_t)y1 * fl + x0], src[(size_t)y1 * fl + x1], tx), ty);
        }
    }
}

void Toile::Point(double x, double y, int classe, float r, float opacite) {
    double cx, cy;
    VersPx(x, y, cx, cy);
    const float bord = std::max(1.0f, r * 0.35f);
    const Rgb fond = CouleurEncre(Encre::Fond), c = Couleur(classe);
    Peindre(cx - r - bord - 1, cx + r + bord + 1, cy - r - bord - 1, cy + r + bord + 1, fond, opacite * 0.85f,
            [&](double i, double j) { return std::hypot(i - cx, j - cy) - (r + bord) + 0.5; });
    Peindre(cx - r - 1, cx + r + 1, cy - r - 1, cy + r + 1, c, opacite, [&](double i, double j) { return std::hypot(i - cx, j - cy) - r + 0.5; });
}
void Toile::PointEncre(double x, double y, Encre e, float r, float opacite) {
    double cx, cy;
    VersPx(x, y, cx, cy);
    Peindre(cx - r - 1, cx + r + 1, cy - r - 1, cy + r + 1, CouleurEncre(e), opacite, [&](double i, double j) { return std::hypot(i - cx, j - cy) - r + 0.5; });
}
void Toile::Anneau(double x, double y, float r, Encre e, float ep, float opacite) {
    double cx, cy;
    VersPx(x, y, cx, cy);
    Peindre(cx - r - ep - 1, cx + r + ep + 1, cy - r - ep - 1, cy + r + ep + 1, CouleurEncre(e), opacite,
            [&](double i, double j) { return std::abs(std::hypot(i - cx, j - cy) - r) - ep / 2 + 0.5; });
}
void Toile::Segment(double x0, double y0, double x1, double y1, int classe, float ep, float opacite) {
    SegmentCouleur(x0, y0, x1, y1, classe < 0 ? CouleurEncre(Encre::Texte) : Couleur(classe), ep, opacite);
}
void Toile::SegmentCouleur(double x0, double y0, double x1, double y1, Rgb col, float ep, float opacite) {
    double a, b, c, d;
    VersPx(x0, y0, a, b);
    VersPx(x1, y1, c, d);
    const double lx = c - a, ly = d - b, l2 = std::max(1e-9, lx * lx + ly * ly);
    Peindre(std::min(a, c) - ep - 1, std::max(a, c) + ep + 1, std::min(b, d) - ep - 1, std::max(b, d) + ep + 1, col, opacite, [&](double i, double j) {
        const double t = std::clamp(((i - a) * lx + (j - b) * ly) / l2, 0.0, 1.0);
        return std::hypot(i - (a + t * lx), j - (b + t * ly)) - ep / 2 + 0.5;
    });
}
void Toile::Trajet(const std::vector<std::pair<double, double>>& p, int classe, float ep, float opacite) {
    for (size_t k = 1; k < p.size(); ++k) Segment(p[k - 1].first, p[k - 1].second, p[k].first, p[k].second, classe, ep, opacite);
}
void Toile::DemiPlan(double nx, double ny, double s, int classe, float alpha, float ep) {
    const Rgb c = Couleur(classe);
    for (int j = 0; j < m_h; ++j) {
        const double y = m_dom.m_yMax - (j + 0.5) * (m_dom.m_yMax - m_dom.m_yMin) / m_h;
        for (int i = 0; i < m_l; ++i) {
            const double x = m_dom.m_xMin + (i + 0.5) * (m_dom.m_xMax - m_dom.m_xMin) / m_l;
            if (nx * x + ny * y > s) Melanger(i, j, c, alpha);
        }
    }
    if (ep <= 0) return;
    // frontiere n . p = s : intersection avec le domaine
    std::vector<std::pair<double, double>> q;
    auto ajouter = [&](double x, double y) {
        if (x >= m_dom.m_xMin - 1e-9 && x <= m_dom.m_xMax + 1e-9 && y >= m_dom.m_yMin - 1e-9 && y <= m_dom.m_yMax + 1e-9) q.push_back({x, y});
    };
    if (std::abs(ny) > 1e-12) { ajouter(m_dom.m_xMin, (s - nx * m_dom.m_xMin) / ny); ajouter(m_dom.m_xMax, (s - nx * m_dom.m_xMax) / ny); }
    if (std::abs(nx) > 1e-12) { ajouter((s - ny * m_dom.m_yMin) / nx, m_dom.m_yMin); ajouter((s - ny * m_dom.m_yMax) / nx, m_dom.m_yMax); }
    if (q.size() >= 2) Segment(q[0].first, q[0].second, q[1].first, q[1].second, classe, ep, 0.95f);
}
void Toile::Croix(double x, double y, int classe, float bras) {
    // croix « + » : trait large couleur du fond, puis trait de la classe (centres des k-moyennes et d'EM)
    const double ex = bras * (m_dom.m_xMax - m_dom.m_xMin) / m_l, ey = bras * (m_dom.m_yMax - m_dom.m_yMin) / m_h;
    SegmentCouleur(x - ex, y, x + ex, y, CouleurEncre(Encre::Fond), bras * 0.7f, 1.0f);
    SegmentCouleur(x, y - ey, x, y + ey, CouleurEncre(Encre::Fond), bras * 0.7f, 1.0f);
    SegmentCouleur(x - ex, y, x + ex, y, Couleur(classe), bras * 0.36f, 1.0f);
    SegmentCouleur(x, y - ey, x, y + ey, Couleur(classe), bras * 0.36f, 1.0f);
}
void Toile::EllipseAxiale(double cx0, double cy0, double ex, double ey, int classe, float alpha, float ep) {
    double cx, cy;
    VersPx(cx0, cy0, cx, cy);
    const double rx = std::max(0.5, ex * m_l / (m_dom.m_xMax - m_dom.m_xMin)), ry = std::max(0.5, ey * m_h / (m_dom.m_yMax - m_dom.m_yMin));
    const Rgb c = Couleur(classe);
    if (alpha > 0)
        Peindre(cx - rx - 1, cx + rx + 1, cy - ry - 1, cy + ry + 1, c, alpha, [&](double i, double j) {
            const double m = std::hypot((i - cx) / rx, (j - cy) / ry);
            return (m - 1) * std::min(rx, ry) + 0.5;
        });
    if (ep > 0)
        Peindre(cx - rx - ep - 1, cx + rx + ep + 1, cy - ry - ep - 1, cy + ry + ep + 1, c, 0.9f, [&](double i, double j) {
            const double m = std::hypot((i - cx) / rx, (j - cy) / ry);
            return std::abs(m - 1) * std::min(rx, ry) - ep / 2 + 0.5;
        });
}

slint::Image Toile::Image() const {
    slint::SharedPixelBuffer<slint::Rgba8Pixel> t(m_l, m_h);
    auto* o = t.begin();
    for (size_t k = 0; k < m_px.size(); ++k)
        o[k] = {(std::uint8_t)std::lround(m_px[k].r), (std::uint8_t)std::lround(m_px[k].g), (std::uint8_t)std::lround(m_px[k].b), 255};
    return slint::Image(t);
}

} // namespace studio
