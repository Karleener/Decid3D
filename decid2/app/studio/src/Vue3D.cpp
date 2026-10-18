#include "Vue3D.h"

#include <algorithm>
#include <cmath>

#include "Moteur.h"

namespace studio {

namespace {
struct Rgb { float r, g, b; };
constexpr std::uint32_t CLASSES_SOMBRE[6] = {0xF07A62, 0x7FB2E8, 0xF2C46B, 0x6CC4B4, 0xA99BF0, 0xE58FC0};
constexpr std::uint32_t CLASSES_CLAIR[6] = {0xC2462F, 0x3672B5, 0xB8862A, 0x2F8C7E, 0x6656C2, 0xB24C8A};
Rgb Hex(std::uint32_t c) { return {float((c >> 16) & 255), float((c >> 8) & 255), float(c & 255)}; }
Rgb Classe(int k, bool sombre) { return Hex((sombre ? CLASSES_SOMBRE : CLASSES_CLAIR)[((k % 6) + 6) % 6]); }
} // namespace

decid::Resultat<VolumeMoteur> LireVolume(const fs::path& resultat) {
    auto js = LireJson(resultat / "volume.json");
    if (!js.Ok()) return js.Err();
    const auto& j = js.Valeur();
    VolumeMoteur v;
    v.m_resolution = (int)j.Membre("resolution").Entier();
    const auto& r = j.Membre("requete");
    for (int k = 0; k < 3; ++k) {
        v.m_min[k] = r.Membre("min").Elements().size() > (size_t)k ? r.Membre("min").Elements()[k].Nombre() : 0;
        v.m_max[k] = r.Membre("max").Elements().size() > (size_t)k ? r.Membre("max").Elements()[k].Nombre() : 1;
    }
    v.m_classes = LireBinaire<std::int16_t>(resultat / "volume.i16");
    if (j.Membre("score").Membre("statut").Texte() == "defini") v.m_scores = LireBinaire<std::uint8_t>(resultat / "volume_score.u8");
    return v;
}

void Vue3D::Preparer(const VolumeMoteur& v, const std::vector<Point>& points) {
    m_voxels.clear();
    m_points.clear();
    const int R = v.m_resolution;
    auto n = [](double t) { return (float)(t * 2.0 - 1.0); };
    auto norm = [&](double x, int k) { return n((x - v.m_min[k]) / (v.m_max[k] - v.m_min[k])); };
    for (const auto& p : points) m_points.push_back({norm(p.m_x, 0), norm(p.m_y, 1), norm(p.m_z, 2), p.m_classe, p.m_opacite});
    if (R > 2 && v.m_classes.size() == (size_t)R * R * R) {
        auto at = [&](int x, int y, int z) { return v.m_classes[((size_t)z * R + y) * R + x]; };
        for (int z = 1; z < R - 1; ++z)
            for (int y = 1; y < R - 1; ++y)
                for (int x = 1; x < R - 1; ++x) {
                    const int c = at(x, y, z);
                    if (at(x + 1, y, z) == c && at(x - 1, y, z) == c && at(x, y + 1, z) == c && at(x, y - 1, z) == c &&
                        at(x, y, z + 1) == c && at(x, y, z - 1) == c) continue;
                    const size_t i = ((size_t)z * R + y) * R + x;
                    m_voxels.push_back({n((x + 0.5) / R), n((y + 0.5) / R), n((z + 0.5) / R), c,
                                        v.m_scores.size() == v.m_classes.size() ? v.m_scores[i] / 255.0f : -1.0f});
                }
        m_taille = 2.0f / R;
    }
    m_pret = true;
}

void Vue3D::Tourner(float dx, float dy) {
    m_yaw += dx * 0.008f;
    m_pitch = std::clamp(m_pitch + dy * 0.008f, -1.4f, 1.4f);
}
void Vue3D::Zoomer(float crans) { m_zoom = std::clamp(m_zoom * std::pow(1.08f, crans), 0.6f, 2.4f); }

slint::Image Vue3D::Rendre(int L, int H, bool sombre, float echelleEcran) const {
    L = std::max(L, 8);
    H = std::max(H, 8);
    const Rgb fond = Hex(sombre ? 0x0F1338 : 0xFBF8F4), filet = sombre ? Rgb{235, 225, 213} : Rgb{21, 25, 70};
    std::vector<Rgb> px((size_t)L * H, fond);
    auto melanger = [&](int i, int j, Rgb c, float a) {
        if (i < 0 || j < 0 || i >= L || j >= H || a <= 0) return;
        Rgb& o = px[(size_t)j * L + i];
        o = {o.r + (c.r - o.r) * a, o.g + (c.g - o.g) * a, o.b + (c.b - o.b) * a};
    };
    const float cy = std::cos(m_yaw), sy = std::sin(m_yaw), cp = std::cos(m_pitch), sp = std::sin(m_pitch);
    const float echelle = std::min(L, H) * 0.30f * m_zoom, ox = L / 2.0f, oy = H / 2.0f;
    struct Proj { float x, y, prof, f; };
    auto proj = [&](float x, float y, float z) {
        const float x1 = x * cy - y * sy, y1 = x * sy + y * cy;   // rotation autour de z (vertical = z)
        const float y2 = y1 * cp - z * sp, z2 = y1 * sp + z * cp;
        const float f = 3.2f / (3.2f + y2);
        return Proj{ox + x1 * echelle * f, oy - z2 * echelle * f, y2, f};
    };

    // cube de repere (trait antialiase simple : echantillonnage le long du segment)
    const float S[8][3] = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}, {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}};
    const int A[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (const auto& a : A) {
        const Proj p = proj(S[a[0]][0], S[a[0]][1], S[a[0]][2]), q = proj(S[a[1]][0], S[a[1]][1], S[a[1]][2]);
        const int pas = (int)std::ceil(std::hypot(q.x - p.x, q.y - p.y));
        for (int k = 0; k <= pas; ++k) {
            const float t = pas ? (float)k / pas : 0, x = p.x + (q.x - p.x) * t, y = p.y + (q.y - p.y) * t;
            melanger((int)x, (int)y, filet, 0.2f);
        }
    }

    // elements tries du plus loin au plus proche
    struct Element { float prof; int genre; size_t i; Proj p; };
    std::vector<Element> e;
    e.reserve(m_voxels.size() + m_points.size());
    if (!m_donneesSeules)
        for (size_t i = 0; i < m_voxels.size(); ++i) e.push_back({0, 0, i, proj(m_voxels[i].m_x, m_voxels[i].m_y, m_voxels[i].m_z)});
    for (size_t i = 0; i < m_points.size(); ++i) e.push_back({0, 1, i, proj(m_points[i].m_x, m_points[i].m_y, m_points[i].m_z)});
    for (auto& x : e) x.prof = x.p.prof;
    std::sort(e.begin(), e.end(), [](const Element& a, const Element& b) { return a.prof > b.prof; });

    const float tv = m_taille * echelle * 1.15f, rp = 1.7f * echelleEcran;
    for (const auto& x : e) {
        if (x.genre == 0) {
            const Voxel& v = m_voxels[x.i];
            const float a = v.m_score < 0 ? m_transparence : std::min(1.0f, m_transparence * (0.3f + 1.2f * v.m_score));
            const float s = tv * x.p.f;
            const Rgb c = Classe(v.m_classe, sombre);
            const int i0 = (int)std::floor(x.p.x - s / 2), i1 = (int)std::ceil(x.p.x + s / 2), j0 = (int)std::floor(x.p.y - s / 2), j1 = (int)std::ceil(x.p.y + s / 2);
            for (int j = j0; j < j1; ++j)
                for (int i = i0; i < i1; ++i) melanger(i, j, c, a);
        } else {
            const PointN& p = m_points[x.i];
            const float r = rp * x.p.f;
            const Rgb c = Classe(p.m_classe, sombre);
            for (int j = (int)(x.p.y - r - 1); j <= (int)(x.p.y + r + 1); ++j)
                for (int i = (int)(x.p.x - r - 1); i <= (int)(x.p.x + r + 1); ++i) {
                    const float d = std::hypot(i + 0.5f - x.p.x, j + 0.5f - x.p.y);
                    melanger(i, j, c, std::clamp(r - d + 0.5f, 0.0f, 1.0f) * p.m_opacite);
                }
        }
    }

    slint::SharedPixelBuffer<slint::Rgba8Pixel> tampon(L, H);
    auto* o = tampon.begin();
    for (size_t k = 0; k < px.size(); ++k)
        o[k] = {(std::uint8_t)std::lround(px[k].r), (std::uint8_t)std::lround(px[k].g), (std::uint8_t)std::lround(px[k].b), 255};
    return slint::Image(tampon);
}

} // namespace studio
