// Rendu logiciel du plan : fond par classe, placement des points, orientation (ligne 0 = y max), graduations.
#include "RenduPlan.h"
#include "Verifier.h"

using namespace app;

namespace {

bool Egal(const PixelRgba& p, const Couleur& c) { return p.m_r == c.m_r && p.m_g == c.m_g && p.m_b == c.m_b; }

// 2 x 2 : ligne 0 (haut, y max) = classe 0 ; ligne 1 (bas, y min) = classe 1
ImageDecision FondDeuxLignes() {
    ImageDecision f;
    f.m_largeur = 2;
    f.m_hauteur = 2;
    f.m_classes = {0, 0, 1, 1};
    return f;
}

} // namespace

TEST(fond_couleur_de_la_classe) {
    const ImageDecision fond = FondDeuxLignes();
    const ImagePlan img = RendrePlan(40, 40, &fond, Domaine{}, {});
    VERIFIER_EGAL(img.m_largeur, 40);
    VERIFIER(Egal(img.Pixel(10, 5), CouleurFond(0)));
    VERIFIER(Egal(img.Pixel(30, 35), CouleurFond(1)));
    VERIFIER(!Egal(img.Pixel(10, 5), CouleurFond(1)));
    // sans fond : creux neutre
    const ImagePlan neutre = RendrePlan(10, 10, nullptr, Domaine{}, {});
    VERIFIER(Egal(neutre.Pixel(5, 5), CouleurCreux()));
}

TEST(orientation_ligne_zero_est_y_max) {
    // domaine y de 0 a 10 : le haut de l'image (ligne 0) est y = 10
    Domaine d{0.0, 10.0, 0.0, 10.0};
    const ImageDecision fond = FondDeuxLignes();
    const ImagePlan img = RendrePlan(100, 100, &fond, d, {});
    VERIFIER(Egal(img.Pixel(50, 10), CouleurFond(0)));   // y proche de 9 : haut
    VERIFIER(Egal(img.Pixel(50, 90), CouleurFond(1)));   // y proche de 1 : bas
    double px, py;
    DonneesVersPixel(d, 100, 100, 5.0, 9.0, px, py);
    VERIFIER_PROCHE(px, 50.0, 1e-9);
    VERIFIER_PROCHE(py, 10.0, 1e-9);
    double x, y;
    PixelVersDonnees(d, 100, 100, px, py, x, y);
    VERIFIER_PROCHE(x, 5.0, 1e-9);
    VERIFIER_PROCHE(y, 9.0, 1e-9);
}

TEST(point_dessine_au_bon_endroit) {
    Domaine d{0.0, 10.0, 0.0, 10.0};
    PointPlan p;
    p.m_x = 2.0;
    p.m_y = 8.0;      // x = 20 %, y = 80 % depuis le bas -> pixel (20, 20) pour 100 x 100
    p.m_classe = 1;
    p.m_rayon = 6.0f;
    // le triangle de la classe 1 est plus petit que son rayon englobant : on teste aussi un disque (classe 0)
    PointPlan disque = p;
    disque.m_classe = 0;
    const ImagePlan img = RendrePlan(100, 100, nullptr, d, {disque});
    VERIFIER(Egal(img.Pixel(20, 20), CouleurClasse(0)));            // centre rempli de la couleur de la classe
    VERIFIER(Egal(img.Pixel(80, 80), CouleurCreux()));              // loin du point : fond intact
    VERIFIER(Egal(img.Pixel(20, 80), CouleurCreux()));              // pas de point au miroir vertical
    VERIFIER(!Egal(img.Pixel(24, 20), CouleurCreux()));             // dans le disque (a 4 px du centre)
    VERIFIER(Egal(img.Pixel(32, 20), CouleurCreux()));              // hors du disque (a 12 px)
    const ImagePlan tri = RendrePlan(100, 100, nullptr, d, {p});
    VERIFIER(!Egal(tri.Pixel(20, 20), CouleurCreux()));
    VERIFIER(Egal(tri.Pixel(80, 80), CouleurCreux()));
}

TEST(formes_distinctes_et_palette_sans_bleu) {
    VERIFIER(FormeClasse(0) == FormePoint::Disque);
    VERIFIER(FormeClasse(1) == FormePoint::Triangle);
    VERIFIER(FormeClasse(2) == FormePoint::Carre);
    VERIFIER(FormeClasse(3) == FormePoint::Losange);
    VERIFIER(FormeClasse(4) == FormePoint::Disque);
    for (int c = 0; c < NB_COULEURS_CLASSES; ++c) {
        const Couleur k = CouleurClasse(c);
        VERIFIER(!(k.m_b > k.m_r && k.m_b > k.m_g));
    }
}

TEST(surlignage_marque_le_point) {
    Domaine d{0.0, 10.0, 0.0, 10.0};
    PointPlan p;
    p.m_x = 5.0;
    p.m_y = 5.0;
    p.m_classe = 0;
    p.m_rayon = 5.0f;
    PointPlan q = p;
    q.m_surligne = true;
    const ImagePlan sans = RendrePlan(100, 100, nullptr, d, {p});
    const ImagePlan avec = RendrePlan(100, 100, nullptr, d, {q});
    int differents = 0;
    for (size_t i = 0; i < sans.m_pixels.size(); ++i)
        if (sans.m_pixels[i].m_r != avec.m_pixels[i].m_r || sans.m_pixels[i].m_g != avec.m_pixels[i].m_g) ++differents;
    VERIFIER(differents > 20);
}

TEST(graduations_et_format) {
    const auto g = Graduations(0.0, 1.0);
    VERIFIER(g.size() >= 4);
    if (!g.empty()) VERIFIER_EGAL(g.front().m_libelle, std::string("0,0"));
    VERIFIER_EGAL(FormatFr(3.14159, 2), std::string("3,14"));
    VERIFIER(Graduations(1.0, 1.0).empty());
}

int main(int argc, char** argv) { return LancerTests(argc, argv); }
