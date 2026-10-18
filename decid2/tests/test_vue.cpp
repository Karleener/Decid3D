// Tests des images et volumes de decision (ACC-13, ACC-27) : donnees synthetiques deterministes.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "Modeles.h"
#include "Normalisation.h"
#include "Verifier.h"
#include "Vue.h"

using namespace decid;

namespace {

struct Lcg {
    std::uint64_t m_etat;
    double Suivant() {
        m_etat = m_etat * 6364136223846793005ULL + 1442695040888963407ULL;
        return (double)(m_etat >> 11) / 9007199254740992.0;
    }
};

template <class T>
bool Reussi(const Resultat<T>& r, const char* contexte) {
    if (!r.Ok()) verif::Echec(__FILE__, __LINE__, std::string(contexte) + " : " + r.Err().Texte());
    return r.Ok();
}

template <class T>
bool Refuse(const Resultat<T>& r, CodeErreur code, const char* contexte) {
    if (r.Ok()) { verif::Echec(__FILE__, __LINE__, std::string(contexte) + " : succes inattendu"); return false; }
    if (r.Err().m_code != code) verif::Echec(__FILE__, __LINE__, std::string(contexte) + " : mauvais code d'erreur");
    return r.Err().m_code == code;
}

// Normalisation min-max construite a la main sur [mini, maxi] pour chaque attribut
Normalisation MinMax(int p, double mini, double maxi) {
    Normalisation n;
    n.m_methode = "minmax";
    n.m_bas = 0.0; n.m_haut = 1.0;
    n.m_min.assign((size_t)p, mini);
    n.m_max.assign((size_t)p, maxi);
    return n;
}

using Regle = int (*)(const std::vector<double>&);

// Echantillons uniformes dans [mini, maxi]^p, etiquetes par la regle, puis transformes par la normalisation
struct Appris { Modele m_modele; Normalisation m_norme; bool m_ok = false; };

cv::Mat MatriceTransformee(const Normalisation& n, const std::vector<std::vector<double>>& brut, int p) {
    cv::Mat x((int)brut.size(), p, CV_32F);
    for (int i = 0; i < x.rows; ++i) {
        std::vector<double> t = TransformerVecteur(n, brut[(size_t)i]);
        for (int k = 0; k < p; ++k) x.at<float>(i, k) = (float)t[(size_t)k];
    }
    return x;
}

Appris Entrainer(bool svm, const Normalisation& n, int p, double mini, double maxi, int nbClasses, Regle regle, int nb) {
    Appris a;
    a.m_norme = n;
    Lcg g{12345};
    std::vector<std::vector<double>> brut;
    std::vector<int> labels;
    for (int i = 0; i < nb; ++i) {
        std::vector<double> v((size_t)p);
        for (auto& e : v) e = mini + g.Suivant() * (maxi - mini);
        brut.push_back(v);
        labels.push_back(regle(v));
    }
    cv::Mat x = MatriceTransformee(n, brut, p);
    if (svm) {
        ParametresSvm ps; ps.m_noyau = "RBF"; ps.m_c = 10.0; ps.m_gamma = 4.0;
        auto r = EntrainerSvm(x, labels, nbClasses, ps);
        if (!Reussi(r, "entrainement SVM")) return a;
        a.m_modele = r.Valeur();
    } else {
        ParametresForet pf; pf.m_nbArbres = 30;
        auto r = EntrainerForet(x, labels, nbClasses, pf, 5);
        if (!Reussi(r, "entrainement foret")) return a;
        a.m_modele = r.Valeur();
    }
    a.m_ok = true;
    return a;
}

// Regles de classification (domaine brut [-10, 20])
int QuatreQuadrants(const std::vector<double>& v) { return (v[0] > 5.0 ? 1 : 0) + (v[1] > 5.0 ? 2 : 0); }
int QuadrantsUnite(const std::vector<double>& v) { return (v[0] > 0.5 ? 1 : 0) + (v[1] > 0.5 ? 2 : 0); }
int SelonY(const std::vector<double>& v) { return v[1] > 5.0 ? 1 : 0; }
int SelonX(const std::vector<double>& v) { return v[0] > 5.0 ? 1 : 0; }
int TroisAttributs(const std::vector<double>& v) { return (v[0] + 0.6 * v[2] > 8.0) ? 1 : 0; }

RequeteImage Requete(int l, int h, double mini, double maxi, std::vector<double> fixes) {
    RequeteImage r;
    r.m_largeur = l; r.m_hauteur = h;
    r.m_xmin = mini; r.m_xmax = maxi; r.m_ymin = mini; r.m_ymax = maxi;
    r.m_valeursFixes = std::move(fixes);
    return r;
}

// ACC-13 : la classe d'un pixel est celle que Predire donne sur le meme vecteur brut normalise
void VerifierPixels(const Appris& a, const RequeteImage& req, const ImageDecision& img) {
    const std::pair<int, int> pixels[] = {{0, 0}, {req.m_largeur - 1, 0}, {0, req.m_hauteur - 1},
                                          {req.m_largeur - 1, req.m_hauteur - 1}, {req.m_largeur / 2, req.m_hauteur / 2},
                                          {3, 7}, {req.m_largeur / 3, req.m_hauteur * 2 / 3}, {req.m_largeur - 5, 4}};
    for (auto& pix : pixels) {
        double x = 0, y = 0;
        CentrePixel(req, pix.first, pix.second, x, y);
        std::vector<double> brut = req.m_valeursFixes;
        brut[(size_t)req.m_attributX] = x;
        brut[(size_t)req.m_attributY] = y;
        std::vector<double> t = TransformerVecteur(a.m_norme, brut);
        cv::Mat ligne(1, (int)t.size(), CV_32F);
        for (size_t k = 0; k < t.size(); ++k) ligne.at<float>(0, (int)k) = (float)t[k];
        auto pred = Predire(a.m_modele, ligne);
        if (!Reussi(pred, "Predire")) return;
        VERIFIER_EGAL((int)img.m_classes[(size_t)pix.second * (size_t)req.m_largeur + (size_t)pix.first], pred.Valeur()[0]);
    }
}

void CasAcc13(bool svm, const Normalisation& n, double mini, double maxi) {
    const Regle regle = maxi <= 1.0 ? QuadrantsUnite : QuatreQuadrants;
    Appris a = Entrainer(svm, n, 2, mini, maxi, 4, regle, 400);
    if (!a.m_ok) return;
    RequeteImage req = Requete(64, 48, mini, maxi, {0.0, 0.0});
    auto img = CalculerImage(a.m_modele, a.m_norme, req, nullptr);
    if (!Reussi(img, "CalculerImage")) return;
    VERIFIER_EGAL(img.Valeur().m_classes.size(), (size_t)64 * 48);
    VerifierPixels(a, req, img.Valeur());
}

} // namespace

TEST(acc13_svm_minmax_domaine_hors_01) { CasAcc13(true, MinMax(2, -10.0, 20.0), -10.0, 20.0); }
TEST(acc13_foret_minmax_domaine_hors_01) { CasAcc13(false, MinMax(2, -10.0, 20.0), -10.0, 20.0); }
TEST(acc13_svm_sans_normalisation) { CasAcc13(true, AucuneNormalisation(2), 0.0, 1.0); }

TEST(acc13_trois_attributs_troisieme_fixe) {
    for (int svm = 0; svm < 2; ++svm) {
        Appris a = Entrainer(svm == 1, MinMax(3, -10.0, 20.0), 3, -10.0, 20.0, 2, TroisAttributs, 500);
        if (!a.m_ok) return;
        RequeteImage req = Requete(40, 30, -10.0, 20.0, {0.0, 0.0, 12.0});
        auto img = CalculerImage(a.m_modele, a.m_norme, req, nullptr);
        if (!Reussi(img, "CalculerImage 3 attributs")) return;
        VerifierPixels(a, req, img.Valeur());
        // X et Y choisis autrement : attributs 2 (x) et 0 (y), le 2e fixe
        req.m_attributX = 2; req.m_attributY = 0;
        req.m_valeursFixes = {0.0, -3.0, 0.0};
        auto img2 = CalculerImage(a.m_modele, a.m_norme, req, nullptr);
        if (!Reussi(img2, "CalculerImage axes permutes")) return;
        VerifierPixels(a, req, img2.Valeur());
    }
}

TEST(orientation_y_vers_le_haut) {
    for (int svm = 0; svm < 2; ++svm) {
        Appris a = Entrainer(svm == 1, MinMax(2, -10.0, 20.0), 2, -10.0, 20.0, 2, SelonY, 600);
        if (!a.m_ok) return;
        // la ligne 0 est tout en haut (y = 19.x) : classe 1 ; la derniere ligne est en bas : classe 0
        RequeteImage req = Requete(30, 30, -10.0, 20.0, {0.0, 0.0});
        auto img = CalculerImage(a.m_modele, a.m_norme, req, nullptr);
        if (!Reussi(img, "orientation y")) return;
        const auto& c = img.Valeur().m_classes;
        for (int i = 8; i < 22; ++i) {
            VERIFIER_EGAL((int)c[(size_t)0 * 30 + (size_t)i], 1);
            VERIFIER_EGAL((int)c[(size_t)29 * 30 + (size_t)i], 0);
        }
    }
}

TEST(orientation_x_vers_la_droite) {
    for (int svm = 0; svm < 2; ++svm) {
        Appris a = Entrainer(svm == 1, MinMax(2, -10.0, 20.0), 2, -10.0, 20.0, 2, SelonX, 600);
        if (!a.m_ok) return;
        RequeteImage req = Requete(30, 30, -10.0, 20.0, {0.0, 0.0});
        auto img = CalculerImage(a.m_modele, a.m_norme, req, nullptr);
        if (!Reussi(img, "orientation x")) return;
        const auto& c = img.Valeur().m_classes;
        for (int j = 8; j < 22; ++j) {
            VERIFIER_EGAL((int)c[(size_t)j * 30 + 29], 1);
            VERIFIER_EGAL((int)c[(size_t)j * 30 + 0], 0);
        }
    }
}

TEST(centre_pixel_formule) {
    RequeteImage r = Requete(10, 4, 0.0, 10.0, {0.0, 0.0});
    r.m_ymin = -2.0; r.m_ymax = 6.0;
    double x = 0, y = 0;
    CentrePixel(r, 0, 0, x, y);
    VERIFIER_PROCHE(x, 0.5, 1e-12);
    VERIFIER_PROCHE(y, 5.0, 1e-12);       // 6 - 0.5 * 2
    CentrePixel(r, 9, 3, x, y);
    VERIFIER_PROCHE(x, 9.5, 1e-12);
    VERIFIER_PROCHE(y, -1.0, 1e-12);      // 6 - 3.5 * 2
}

TEST(image_500x500_reelle_et_hash) {
    Appris a = Entrainer(false, MinMax(2, -10.0, 20.0), 2, -10.0, 20.0, 4, QuatreQuadrants, 800);
    if (!a.m_ok) return;
    RequeteImage req = Requete(500, 500, -10.0, 20.0, {0.0, 0.0});
    auto debut = std::chrono::steady_clock::now();
    auto img = CalculerImage(a.m_modele, a.m_norme, req, nullptr);
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - debut).count();
    std::printf("  [info] image 500x500 (foret 30 arbres) : %.0f ms\n", ms);
    if (!Reussi(img, "image 500x500")) return;
    VERIFIER_EGAL(img.Valeur().m_largeur, 500);
    VERIFIER_EGAL(img.Valeur().m_hauteur, 500);
    VERIFIER_EGAL(img.Valeur().m_classes.size(), (size_t)250000);
    VERIFIER_EGAL(img.Valeur().m_hash.size(), (size_t)64);
    auto encore = CalculerImage(a.m_modele, a.m_norme, req, nullptr);
    if (!Reussi(encore, "image 500x500 bis")) return;
    VERIFIER(encore.Valeur().m_hash == img.Valeur().m_hash);
    VERIFIER(encore.Valeur().m_classes == img.Valeur().m_classes);
    RequeteImage autre = req;
    autre.m_xmax = 8.0;
    auto changee = CalculerImage(a.m_modele, a.m_norme, autre, nullptr);
    if (!Reussi(changee, "image domaine change")) return;
    VERIFIER(changee.Valeur().m_hash != img.Valeur().m_hash);
}

TEST(volume_40_cube_reel_et_hash) {
    Appris a = Entrainer(false, MinMax(3, -10.0, 20.0), 3, -10.0, 20.0, 2, TroisAttributs, 800);
    if (!a.m_ok) return;
    RequeteVolume req;
    req.m_resolution = 40;
    for (int k = 0; k < 3; ++k) { req.m_min[k] = -10.0; req.m_max[k] = 20.0; }
    req.m_valeursFixes = {0.0, 0.0, 0.0};
    auto vol = CalculerVolume(a.m_modele, a.m_norme, req, nullptr);
    if (!Reussi(vol, "volume 40")) return;
    VERIFIER_EGAL(vol.Valeur().m_resolution, 40);
    VERIFIER_EGAL(vol.Valeur().m_classes.size(), (size_t)64000);
    auto encore = CalculerVolume(a.m_modele, a.m_norme, req, nullptr);
    if (!Reussi(encore, "volume 40 bis")) return;
    VERIFIER(encore.Valeur().m_hash == vol.Valeur().m_hash);
    // ordre z, y, x (x le plus rapide) : verification sur quelques voxels
    const int pts[][3] = {{0, 0, 0}, {39, 0, 0}, {0, 39, 0}, {0, 0, 39}, {17, 5, 31}};
    for (auto& q : pts) {
        std::vector<double> brut(3);
        for (int k = 0; k < 3; ++k) brut[(size_t)k] = -10.0 + (q[k] + 0.5) * 30.0 / 40.0;
        std::vector<double> t = TransformerVecteur(a.m_norme, brut);
        cv::Mat ligne(1, 3, CV_32F);
        for (int k = 0; k < 3; ++k) ligne.at<float>(0, k) = (float)t[(size_t)k];
        auto pred = Predire(a.m_modele, ligne);
        if (!Reussi(pred, "Predire voxel")) return;
        const size_t idx = ((size_t)q[2] * 40 + (size_t)q[1]) * 40 + (size_t)q[0];
        VERIFIER_EGAL((int)vol.Valeur().m_classes[idx], pred.Valeur()[0]);
    }
    RequeteVolume autre = req;
    autre.m_max[0] = 5.0;
    auto changee = CalculerVolume(a.m_modele, a.m_norme, autre, nullptr);
    if (!Reussi(changee, "volume domaine change")) return;
    VERIFIER(changee.Valeur().m_hash != vol.Valeur().m_hash);
}

TEST(budget_annulation_et_requetes_invalides) {
    Appris a = Entrainer(false, MinMax(3, -10.0, 20.0), 3, -10.0, 20.0, 2, TroisAttributs, 300);
    if (!a.m_ok) return;
    const RequeteImage ok = Requete(10, 10, -10.0, 20.0, {0.0, 0.0, 0.0});
    VERIFIER(CalculerImage(a.m_modele, a.m_norme, ok, nullptr, 200).Ok());                       // 100 * 2 octets
    Refuse(CalculerImage(a.m_modele, a.m_norme, ok, nullptr, 199), CodeErreur::Ressources, "budget image");
    RequeteVolume vol;
    vol.m_valeursFixes = {0.0, 0.0, 0.0};
    vol.m_resolution = 10;
    VERIFIER(CalculerVolume(a.m_modele, a.m_norme, vol, nullptr, 2000).Ok());
    Refuse(CalculerVolume(a.m_modele, a.m_norme, vol, nullptr, 1999), CodeErreur::Ressources, "budget volume");

    std::atomic<bool> annule{true};
    Refuse(CalculerImage(a.m_modele, a.m_norme, ok, &annule), CodeErreur::Annulation, "annulation image");
    Refuse(CalculerVolume(a.m_modele, a.m_norme, vol, &annule), CodeErreur::Annulation, "annulation volume");
    std::atomic<bool> libre{false};
    VERIFIER(CalculerImage(a.m_modele, a.m_norme, ok, &libre).Ok());

    auto invalide = [&](auto modifier, const char* nom) {
        RequeteImage r = ok;
        modifier(r);
        Refuse(CalculerImage(a.m_modele, a.m_norme, r, nullptr), CodeErreur::DonneesInvalides, nom);
    };
    invalide([](RequeteImage& r) { r.m_largeur = 0; }, "largeur 0");
    invalide([](RequeteImage& r) { r.m_hauteur = 4097; }, "hauteur 4097");
    invalide([](RequeteImage& r) { r.m_attributY = 0; }, "X == Y");
    invalide([](RequeteImage& r) { r.m_attributY = 3; }, "Y hors plage");
    invalide([](RequeteImage& r) { r.m_attributX = -1; }, "X negatif");
    invalide([](RequeteImage& r) { r.m_xmax = r.m_xmin; }, "xmin == xmax");
    invalide([](RequeteImage& r) { r.m_ymin = std::nan(""); }, "ymin NaN");
    invalide([](RequeteImage& r) { r.m_valeursFixes.pop_back(); }, "valeurs fixes trop courtes");
    invalide([](RequeteImage& r) { r.m_valeursFixes[2] = INFINITY; }, "valeur fixe infinie");

    auto invalideVol = [&](auto modifier, const char* nom) {
        RequeteVolume r = vol;
        modifier(r);
        Refuse(CalculerVolume(a.m_modele, a.m_norme, r, nullptr), CodeErreur::DonneesInvalides, nom);
    };
    invalideVol([](RequeteVolume& r) { r.m_resolution = 0; }, "resolution 0");
    invalideVol([](RequeteVolume& r) { r.m_resolution = 257; }, "resolution 257");
    invalideVol([](RequeteVolume& r) { r.m_attributs[2] = 0; }, "attributs identiques");
    invalideVol([](RequeteVolume& r) { r.m_attributs[2] = 3; }, "attribut hors plage");
    invalideVol([](RequeteVolume& r) { r.m_max[1] = r.m_min[1]; }, "domaine degenere");
}

TEST(coloriser_palette_et_alpha) {
    Palette p = PaletteParDefaut();
    VERIFIER_EGAL(p.m_couleurs.size(), (size_t)8);
    VERIFIER_EGAL(p.m_couleurs[0], 0x7A1F3DFFu);
    VERIFIER_EGAL(p.m_couleurs[1], 0xFACB39FFu);
    VERIFIER_EGAL(p.m_couleurs[7], 0x6C0B04FFu);
    for (std::uint32_t c : p.m_couleurs) {
        const unsigned r = (c >> 24) & 0xFF, g = (c >> 16) & 0xFF, b = (c >> 8) & 0xFF;
        VERIFIER(b <= std::max(r, g));       // aucune composante bleue dominante
    }
    ImageDecision img;
    img.m_largeur = 3; img.m_hauteur = 2;
    img.m_classes = {0, 1, 8, 9, -1, 7};
    auto rgba = Coloriser(img, p, 128);
    VERIFIER_EGAL(rgba.size(), (size_t)4 * 3 * 2);
    if (rgba.size() != 24) return;
    const std::uint8_t attendu[6][3] = {{0x7A, 0x1F, 0x3D}, {0xFA, 0xCB, 0x39}, {0x7A, 0x1F, 0x3D},
                                        {0xFA, 0xCB, 0x39}, {0x80, 0x80, 0x80}, {0x6C, 0x0B, 0x04}};
    for (size_t i = 0; i < 6; ++i) {
        VERIFIER_EGAL((int)rgba[4 * i], (int)attendu[i][0]);
        VERIFIER_EGAL((int)rgba[4 * i + 1], (int)attendu[i][1]);
        VERIFIER_EGAL((int)rgba[4 * i + 2], (int)attendu[i][2]);
        VERIFIER_EGAL((int)rgba[4 * i + 3], 128);
    }
    auto opaque = Coloriser(img, p);
    VERIFIER_EGAL((int)opaque[3], 255);
}

TEST(requetes_json_et_cle_cache) {
    RequeteImage r = Requete(50, 40, -1.0, 2.0, {0.5, 0.25, 7.0});
    r.m_attributX = 2; r.m_attributY = 0;
    ValeurJson j = r.VersJson();
    for (const char* cle : {"largeur", "hauteur", "attribut_x", "attribut_y", "xmin", "xmax", "ymin", "ymax", "valeurs_fixes"})
        VERIFIER(j.Contient(cle));
    VERIFIER_EGAL(j.Membre("valeurs_fixes").Elements().size(), (size_t)3);
    VERIFIER_EGAL(j.Membre("attribut_x").Entier(), 2LL);

    RequeteVolume v;
    v.m_valeursFixes = {1.0, 2.0, 3.0, 4.0};
    v.m_attributs[0] = 3;
    ValeurJson jv = v.VersJson();
    for (const char* cle : {"resolution", "attributs", "min", "max", "valeurs_fixes"}) VERIFIER(jv.Contient(cle));
    VERIFIER_EGAL(jv.Membre("attributs").Elements().size(), (size_t)3);
    VERIFIER_EGAL(jv.Membre("attributs").Elements()[0].Entier(), 3LL);

    Normalisation n = AucuneNormalisation(3);
    const std::string cle = CleCache("abc", n, j);
    VERIFIER_EGAL(cle.size(), (size_t)64);
    VERIFIER(cle == CleCache("abc", n, r.VersJson()));
    RequeteImage r2 = r;
    r2.m_xmax = 2.5;
    VERIFIER(cle != CleCache("abc", n, r2.VersJson()));
    RequeteImage r3 = r;
    r3.m_valeursFixes[1] = 0.26;
    VERIFIER(cle != CleCache("abc", n, r3.VersJson()));
    VERIFIER(cle != CleCache("abd", n, j));
    VERIFIER(cle != CleCache("abc", MinMax(3, 0.0, 1.0), j));
    const std::string cleVolume = CleCache("abc", n, v.VersJson());
    v.m_resolution = 41;
    VERIFIER(cleVolume != CleCache("abc", n, v.VersJson()));
}

// ---------------------------------------------------------------- scores de confiance (Scores.h)

namespace {

// Moyenne du score (0..255) des colonnes de pixels dont le centre x tombe dans [x0, x1]
double ScoreMoyenColonnes(const RequeteImage& req, const ImageDecision& img, double x0, double x1) {
    double somme = 0;
    long n = 0;
    for (int j = 0; j < req.m_hauteur; ++j)
        for (int i = 0; i < req.m_largeur; ++i) {
            double x = 0, y = 0;
            CentrePixel(req, i, j, x, y);
            if (x < x0 || x > x1) continue;
            somme += img.m_scores[(size_t)j * (size_t)req.m_largeur + (size_t)i];
            ++n;
        }
    return n ? somme / n : -1;
}

// Frontiere verticale en x = 5 : score faible pres de la frontiere, eleve loin d'elle
// exigeIncertitude : faux pour la foret, dont les arbres coupent presque tous au meme seuil (elle reste
// sure d'elle jusque contre la frontiere) ; sa coherence avec VotesForet est verifiee a part.
void VerifierScoreFrontiere(const Modele& m, const Normalisation& n, const char* nom, bool exigeIncertitude = true) {
    RequeteImage req = Requete(300, 4, -10.0, 20.0, {0.0, 0.0});   // colonnes de 0,1
    auto img = CalculerImage(m, n, req, nullptr);
    if (!Reussi(img, nom)) return;
    VERIFIER(img.Valeur().m_score.m_defini);
    VERIFIER_EGAL(img.Valeur().m_scores.size(), img.Valeur().m_classes.size());
    const double pres = ScoreMoyenColonnes(req, img.Valeur(), 4.0, 6.0);
    const double loin = (ScoreMoyenColonnes(req, img.Valeur(), -10.0, -6.0) + ScoreMoyenColonnes(req, img.Valeur(), 16.0, 20.0)) / 2;
    std::printf("  %s : score moyen pres de la frontiere %.0f, loin %.0f (sur 255)\n", nom, pres, loin);
    // colonne la moins sure : elle doit etre sur la frontiere (x proche de 5) et nettement incertaine
    double minimum = 256, xMin = 0;
    for (int i = 0; i < req.m_largeur; ++i) {
        double x = 0, y = 0;
        CentrePixel(req, i, 0, x, y);
        const double s = ScoreMoyenColonnes(req, img.Valeur(), x - 1e-9, x + 1e-9);
        if (s < minimum) { minimum = s; xMin = x; }
    }
    std::printf("    colonne la moins sure : x = %.2f, score %.0f\n", xMin, minimum);
    VERIFIER(pres < loin);
    VERIFIER(loin > 150);
    VERIFIER(std::fabs(xMin - 5.0) < 1.5);
    if (exigeIncertitude) VERIFIER(minimum < 128);
    auto bis = CalculerImage(m, n, req, nullptr);                         // determinisme
    if (Reussi(bis, "second calcul")) VERIFIER(bis.Valeur().m_scores == img.Valeur().m_scores);
}

} // namespace

TEST(score_confiance_faible_pres_de_la_frontiere_pour_chaque_famille) {
    const Normalisation n = MinMax(2, -10.0, 20.0);
    Lcg g{777};
    std::vector<std::vector<double>> brut;
    std::vector<int> labels;
    for (int i = 0; i < 400; ++i) {
        std::vector<double> v{-10.0 + g.Suivant() * 30.0, -10.0 + g.Suivant() * 30.0};
        brut.push_back(v);
        labels.push_back(SelonX(v));
    }
    const cv::Mat x = MatriceTransformee(n, brut, 2);
    ParametresSvm ps; ps.m_noyau = "LINEAIRE"; ps.m_c = 10.0;
    auto svm = EntrainerSvm(x, labels, 2, ps);
    if (Reussi(svm, "svm")) VerifierScoreFrontiere(svm.Valeur(), n, "SVM lineaire");
    ParametresForet pf; pf.m_nbArbres = 40;
    auto foret = EntrainerForet(x, labels, 2, pf, 3);
    if (Reussi(foret, "foret")) {
        VerifierScoreFrontiere(foret.Valeur(), n, "foret", false);
        // score = |votes(1re) - votes(2e)| / nombre de votes, recalcule ici avec VotesForet
        for (double bx : {4.9, 5.0, 5.05, 5.1, 12.0}) {
            const std::vector<double> t = TransformerVecteur(n, {bx, 3.0});
            cv::Mat ligne(1, 2, CV_32F);
            ligne.at<float>(0, 0) = (float)t[0]; ligne.at<float>(0, 1) = (float)t[1];
            auto votes = VotesForet(foret.Valeur(), ligne);
            auto score = ScoresConfiance(foret.Valeur(), ligne);
            if (!Reussi(votes, "votes") || !Reussi(score, "score")) continue;
            const double v0 = votes.Valeur()[0], v1 = votes.Valeur()[1];
            VERIFIER_PROCHE((double)score.Valeur()[0], std::fabs(v0 - v1) / (v0 + v1), 1e-6);
        }
    }
    ParametresKppv pk; pk.m_k = 15;
    auto kppv = EntrainerKppv(x, labels, 2, pk);
    if (Reussi(kppv, "kppv")) VerifierScoreFrontiere(kppv.Valeur(), n, "k-PPV");
    auto bayes = EntrainerBayes(x, labels, 2);
    if (Reussi(bayes, "bayes")) VerifierScoreFrontiere(bayes.Valeur(), n, "Bayes normal");
}

TEST(score_non_defini_motive_et_classes_inchangees) {
    // SVM a 4 classes : pas de valeur de decision unique -> score non defini, classes toujours calculees
    Appris a = Entrainer(true, MinMax(2, -10.0, 20.0), 2, -10.0, 20.0, 4, QuatreQuadrants, 400);
    if (!a.m_ok) return;
    RequeteImage req = Requete(32, 24, -10.0, 20.0, {0.0, 0.0});
    auto img = CalculerImage(a.m_modele, a.m_norme, req, nullptr);
    if (!Reussi(img, "image svm 4 classes")) return;
    VERIFIER(!img.Valeur().m_score.m_defini);
    VERIFIER(!img.Valeur().m_score.m_raison.empty());
    VERIFIER(img.Valeur().m_scores.empty());
    VerifierPixels(a, req, img.Valeur());
    // Foret a 4 classes : score defini, et le volume publie un score par voxel
    Appris f = Entrainer(false, MinMax(3, -10.0, 20.0), 3, -10.0, 20.0, 2, TroisAttributs, 300);
    if (!f.m_ok) return;
    RequeteVolume rv;
    rv.m_resolution = 12;
    for (int k = 0; k < 3; ++k) { rv.m_min[k] = -10.0; rv.m_max[k] = 20.0; }
    rv.m_valeursFixes = {0.0, 0.0, 0.0};
    auto vol = CalculerVolume(f.m_modele, f.m_norme, rv, nullptr);
    if (!Reussi(vol, "volume foret")) return;
    VERIFIER(vol.Valeur().m_score.m_defini);
    VERIFIER_EGAL(vol.Valeur().m_scores.size(), (size_t)12 * 12 * 12);
}

int main(int argc, char** argv) { return LancerTests(argc, argv); }
