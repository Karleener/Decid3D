// Segmentation couleur par fenetres glissantes (voir Segmentation.h).
// Requetes :
//  {"operation":"echantillonner_couleurs", "image":"<png|jpg>", "espace":"Lab"|"HSV"|"RGB"|"YCrCb", "fenetre":5,
//   "graine":1, "par_zone":200, "zones":[{"classe":"ciel", "x":0, "y":0, "largeur":50, "hauteur":40}, ...]}
//  {"operation":"segmenter", "experience":"<tache publiee>", "image":"<png|jpg>", "espace":"Lab", "fenetre":5,
//   "echantillons":"<tache echantillonner_couleurs>" (facultatif : espace et fenetre doivent alors concorder)}
// Coordonnees en pixels de l'image source, origine en haut a gauche. Les fenetres sont CENTREES sur le pixel tire
// (l'ancien Decid prenait la fenetre a partir de son coin haut gauche) ; bords : reflexion (BORDER_REFLECT_101).
// Tirage des positions : TirageUniforme (splitmix64, Partage.h), x puis y, zone par zone.
#include "Segmentation.h"

#include <algorithm>
#include <cstdio>
#include <set>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include "Fichiers.h"
#include "Operations.h"
#include "Partage.h"
#include "Sha256.h"
#include "Vue.h"

namespace decid {

namespace {

const long long PIXELS_MAX = 4LL << 20;
const std::size_t OCTETS_IMAGE_MAX = 32u << 20;
const std::size_t ZONES_MAX = 64;
const int FENETRE_MAX = 63;
const int LIGNES_PAR_LOT = 32;

Erreur Proto(const std::string& message) { return FaireErreur(CodeErreur::Protocole, "image", message); }
Statut Annule(const std::atomic<bool>& annulation) {
    if (annulation.load())
        return FaireErreur(CodeErreur::Annulation, "annulation", "calcul annule a la demande, rien n'est publie");
    return Succes();
}

fs::path Resoudre(const std::string& texte, const fs::path& dossier) {
    fs::path p = CheminDepuisUtf8(texte);
    return p.is_relative() ? dossier / p : p;
}

std::string TexteNombre(double x) {
    char tampon[40];
    std::snprintf(tampon, sizeof(tampon), "%.9g", x);
    return tampon;
}

bool NomValide(const std::string& nom) {
    if (nom.empty() || nom.size() > 64) return false;
    for (char ch : nom) if (ch == ',' || ch == ';' || ch == '"' || ch == '\n' || ch == '\r' || ch == '\t') return false;
    return true;
}

ValeurJson Textes(const std::vector<std::string>& v) {
    ValeurJson t = ValeurJson::Tableau();
    for (auto& s : v) t.Ajouter(s);
    return t;
}

// Parametres communs : image, espace, fenetre.
struct ParametresCouleur {
    fs::path m_image;
    EspaceCouleur m_espace = EspaceCouleur::Lab;
    int m_fenetre = 5;
};

Resultat<ParametresCouleur> LireParametres(const ValeurJson& j, const fs::path& dossier) {
    ParametresCouleur p;
    const std::string image = j.Membre("image").Texte();
    if (image.empty()) return Proto("champ \"image\" manquant");
    p.m_image = Resoudre(image, dossier);
    if (!EspaceDepuisNom(j.Membre("espace").Texte(), p.m_espace)) return Proto("espace : Lab, HSV, RGB ou YCrCb");
    p.m_fenetre = j.Contient("fenetre") ? (int)j.Membre("fenetre").Entier() : 5;
    if (p.m_fenetre < 1 || p.m_fenetre > FENETRE_MAX || p.m_fenetre % 2 == 0)
        return Proto("fenetre : entier impair entre 1 et 63");
    std::error_code ec;
    if (!fs::is_regular_file(p.m_image, ec))
        return FaireErreur(CodeErreur::Lecture, "image", "image introuvable", CheminUtf8(p.m_image));
    return p;
}

Statut LireZones(const ValeurJson& j, std::vector<ZoneEtiquetee>& zones) {
    const auto& liste = j.Membre("zones").Elements();
    if (liste.empty() || liste.size() > ZONES_MAX) return Proto("zones : entre 1 et 64 zones etiquetees");
    std::set<std::string> classes;
    for (auto& z : liste) {
        ZoneEtiquetee e;
        e.m_classe = z.Membre("classe").Texte();
        if (!NomValide(e.m_classe)) return Proto("classe de zone vide, trop longue ou avec separateur");
        e.m_x = (int)z.Membre("x").Entier();
        e.m_y = (int)z.Membre("y").Entier();
        e.m_largeur = (int)z.Membre("largeur").Entier();
        e.m_hauteur = (int)z.Membre("hauteur").Entier();
        if (e.m_x < 0 || e.m_y < 0 || e.m_largeur < 1 || e.m_hauteur < 1) return Proto("zone : x, y >= 0, largeur et hauteur >= 1");
        classes.insert(e.m_classe);
        zones.push_back(e);
    }
    if (classes.size() < 2) return Proto("au moins deux classes differentes parmi les zones");
    return Succes();
}

Statut ZonesDansImage(const std::vector<ZoneEtiquetee>& zones, const cv::Mat& image) {
    for (auto& z : zones)
        if (z.m_x + z.m_largeur > image.cols || z.m_y + z.m_hauteur > image.rows)
            return Proto("zone \"" + z.m_classe + "\" hors de l'image (" + std::to_string(image.cols) + " x " +
                         std::to_string(image.rows) + ")");
    return Succes();
}

ValeurJson DescriptionImage(const fs::path& chemin, const cv::Mat& image) {
    ValeurJson j = ValeurJson::Objet();
    j["sha256"] = Sha256Fichier(chemin);
    j["largeur"] = image.cols;
    j["hauteur"] = image.rows;
    return j;
}

void DecrireCaracteristique(ValeurJson& j, const ParametresCouleur& p) {
    j["espace"] = NomEspace(p.m_espace);
    j["canaux"] = Textes(NomsCanaux(p.m_espace));
    j["fenetre"] = p.m_fenetre;
    j["caracteristique"] = "moyenne de chaque canal (cvtColor OpenCV 8 bits) sur la fenetre carree centree, divisee "
                           "par 255 ; bord reflechi";
}

// ---------------------------------------------------------------- echantillonnage

struct Echantillons {
    std::string m_csv;
    std::vector<std::string> m_classes;     // ordre de premiere apparition
    std::vector<long> m_comptes;
};

void EchantillonnerZone(const ZoneEtiquetee& z, const cv::Mat& carac, int nombre, std::uint64_t& etat, Echantillons& e) {
    for (int i = 0; i < nombre; ++i) {
        const int x = z.m_x + (int)TirageUniforme(etat, (std::uint64_t)z.m_largeur);
        const int y = z.m_y + (int)TirageUniforme(etat, (std::uint64_t)z.m_hauteur);
        const cv::Vec3f v = carac.at<cv::Vec3f>(y, x);
        e.m_csv += z.m_classe + "," + TexteNombre(v[0]) + "," + TexteNombre(v[1]) + "," + TexteNombre(v[2]) + "\n";
    }
    std::size_t k = 0;
    while (k < e.m_classes.size() && e.m_classes[k] != z.m_classe) ++k;
    if (k == e.m_classes.size()) { e.m_classes.push_back(z.m_classe); e.m_comptes.push_back(0); }
    e.m_comptes[k] += nombre;
}

ValeurJson ZonesJson(const std::vector<ZoneEtiquetee>& zones) {
    ValeurJson t = ValeurJson::Tableau();
    for (auto& z : zones) {
        ValeurJson j = ValeurJson::Objet();
        j["classe"] = z.m_classe;
        j["x"] = z.m_x;
        j["y"] = z.m_y;
        j["largeur"] = z.m_largeur;
        j["hauteur"] = z.m_hauteur;
        t.Ajouter(j);
    }
    return t;
}

// ---------------------------------------------------------------- segmentation

// Espace et fenetre de l'echantillonnage d'origine, s'il est fourni : refus si discordants.
Statut ControlerEchantillons(const ValeurJson& j, const fs::path& dossier, const ParametresCouleur& p, bool& verifie) {
    verifie = false;
    if (!j.Contient("echantillons")) return Succes();
    const fs::path tache = Resoudre(j.Membre("echantillons").Texte(), dossier);
    auto texte = LireFichierTexte(tache / "resultat" / "echantillons.json");
    if (!texte.Ok()) return texte.Err();
    auto d = LireJson(texte.Valeur());
    if (!d.Ok()) return d.Err();
    if (d.Valeur().Membre("espace").Texte() != NomEspace(p.m_espace) || d.Valeur().Membre("fenetre").Entier() != p.m_fenetre)
        return Proto("espace ou fenetre differents de ceux de l'echantillonnage qui a servi a l'apprentissage");
    verifie = true;
    return Succes();
}

Resultat<std::vector<std::int16_t>> ClasserPixels(const ExperiencePubliee& e, const cv::Mat& carac,
                                                  const std::atomic<bool>& annulation) {
    std::vector<std::int16_t> classes;
    classes.reserve(carac.total());
    for (int y0 = 0; y0 < carac.rows; y0 += LIGNES_PAR_LOT) {
        Statut st = Annule(annulation);
        if (!st.Ok()) return st.Err();
        const int y1 = std::min(carac.rows, y0 + LIGNES_PAR_LOT);
        cv::Mat lot((y1 - y0) * carac.cols, 3, CV_32F);
        int ligne = 0;
        for (int y = y0; y < y1; ++y)
            for (int x = 0; x < carac.cols; ++x, ++ligne) {
                const cv::Vec3f v = carac.at<cv::Vec3f>(y, x);
                const std::vector<double> t = TransformerVecteur(e.m_normalisation, {v[0], v[1], v[2]});
                for (int k = 0; k < 3; ++k) lot.at<float>(ligne, k) = (float)t[(size_t)k];
            }
        auto pred = Predire(e.m_modele, lot);
        if (!pred.Ok()) return pred.Err();
        for (int c : pred.Valeur()) classes.push_back((std::int16_t)c);
    }
    return classes;
}

Resultat<std::vector<std::uint8_t>> PngColorise(const ImageDecision& image) {
    std::vector<std::uint8_t> rgba = Coloriser(image, PaletteParDefaut());
    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) std::swap(rgba[i], rgba[i + 2]);   // RGBA -> BGRA
    cv::Mat m(image.m_hauteur, image.m_largeur, CV_8UC4, rgba.data());
    std::vector<std::uint8_t> png;
    try {
        if (!cv::imencode(".png", m, png)) return FaireErreur(CodeErreur::Calcul, "image", "encodage PNG refuse");
    } catch (const cv::Exception& ex) {
        return FaireErreur(CodeErreur::Calcul, "image", std::string("encodage PNG : ") + ex.what());
    }
    return png;
}

ValeurJson ClassesJson(const ExperiencePubliee& e, const std::vector<std::int16_t>& classes) {
    std::vector<long> comptes((size_t)e.m_jeu.NbClasses(), 0);
    for (std::int16_t c : classes) ++comptes[(size_t)c];
    const Palette palette = PaletteParDefaut();
    ValeurJson t = ValeurJson::Tableau();
    for (std::size_t k = 0; k < comptes.size(); ++k) {
        ValeurJson c = ValeurJson::Objet();
        c["id"] = (int)k;
        c["nom"] = e.m_jeu.m_nomsClasses[k];
        c["pixels"] = comptes[k];
        c["part"] = classes.empty() ? 0.0 : (double)comptes[k] / (double)classes.size();
        const std::uint32_t rgba = palette.m_couleurs[k % palette.m_couleurs.size()];
        char hex[8];
        std::snprintf(hex, sizeof(hex), "#%06x", (unsigned)(rgba >> 8));
        c["couleur"] = hex;
        t.Ajouter(c);
    }
    return t;
}

} // namespace

// ---------------------------------------------------------------- API

const char* NomEspace(EspaceCouleur e) {
    switch (e) {
        case EspaceCouleur::Lab: return "Lab";
        case EspaceCouleur::Hsv: return "HSV";
        case EspaceCouleur::Rgb: return "RGB";
        case EspaceCouleur::YCrCb: return "YCrCb";
    }
    return "?";
}

bool EspaceDepuisNom(const std::string& nom, EspaceCouleur& e) {
    for (auto c : {EspaceCouleur::Lab, EspaceCouleur::Hsv, EspaceCouleur::Rgb, EspaceCouleur::YCrCb})
        if (nom == NomEspace(c)) { e = c; return true; }
    return false;
}

std::vector<std::string> NomsCanaux(EspaceCouleur e) {
    switch (e) {
        case EspaceCouleur::Lab: return {"L", "a", "b"};
        case EspaceCouleur::Hsv: return {"H", "S", "V"};
        case EspaceCouleur::Rgb: return {"R", "G", "B"};
        case EspaceCouleur::YCrCb: return {"Y", "Cr", "Cb"};
    }
    return {};
}

Resultat<cv::Mat> LireImageCouleur(const fs::path& chemin) {
    auto octets = LireFichierBinaire(chemin);
    if (!octets.Ok()) return octets.Err();
    const std::vector<char>& b = octets.Valeur();
    if (b.size() > OCTETS_IMAGE_MAX) return FaireErreur(CodeErreur::Ressources, "image", "image de plus de 32 Mo refusee");
    const bool png = b.size() > 8 && (unsigned char)b[0] == 0x89 && b[1] == 'P' && b[2] == 'N' && b[3] == 'G';
    const bool jpeg = b.size() > 3 && (unsigned char)b[0] == 0xFF && (unsigned char)b[1] == 0xD8;
    if (!png && !jpeg) return FaireErreur(CodeErreur::DonneesInvalides, "image", "format d'image : PNG ou JPEG attendu",
                                          CheminUtf8(chemin));
    cv::Mat image;
    try {
        image = cv::imdecode(cv::Mat(1, (int)b.size(), CV_8U, const_cast<char*>(b.data())), cv::IMREAD_COLOR);
    } catch (const cv::Exception& ex) {
        return FaireErreur(CodeErreur::DonneesInvalides, "image", std::string("decodage : ") + ex.what());
    }
    if (image.empty()) return FaireErreur(CodeErreur::DonneesInvalides, "image", "image illisible", CheminUtf8(chemin));
    if ((long long)image.cols * image.rows > PIXELS_MAX)
        return FaireErreur(CodeErreur::Ressources, "image", "image de plus de 4 millions de pixels refusee");
    return image;
}

Resultat<cv::Mat> CaracteristiquesCouleur(const cv::Mat& bgr, EspaceCouleur espace, int fenetre) {
    if (bgr.type() != CV_8UC3) return FaireErreur(CodeErreur::DonneesInvalides, "image", "image BGR 8 bits attendue");
    if (fenetre < 1 || fenetre % 2 == 0) return Proto("fenetre impaire attendue");
    const int code = espace == EspaceCouleur::Lab ? cv::COLOR_BGR2Lab : espace == EspaceCouleur::Hsv ? cv::COLOR_BGR2HSV
                   : espace == EspaceCouleur::Rgb ? cv::COLOR_BGR2RGB : cv::COLOR_BGR2YCrCb;
    cv::Mat converti, carac;
    try {
        cv::cvtColor(bgr, converti, code);
        converti.convertTo(carac, CV_32FC3, 1.0 / 255.0);
        if (fenetre > 1)
            cv::boxFilter(carac, carac, -1, cv::Size(fenetre, fenetre), cv::Point(-1, -1), true, cv::BORDER_REFLECT_101);
    } catch (const cv::Exception& ex) {
        return FaireErreur(CodeErreur::Calcul, "image", std::string("OpenCV : ") + ex.what());
    }
    return carac;
}

Statut ValiderEchantillonnage(const ValeurJson& j, const fs::path& dossier) {
    auto p = LireParametres(j, dossier);
    if (!p.Ok()) return p.Err();
    std::vector<ZoneEtiquetee> zones;
    Statut st = LireZones(j, zones);
    if (!st.Ok()) return st;
    const long long parZone = j.Contient("par_zone") ? j.Membre("par_zone").Entier() : 200;
    if (parZone < 1 || parZone > 5000) return Proto("par_zone : entre 1 et 5000");
    if (j.Membre("graine").Entier() < 0) return Proto("graine : entier positif ou nul");
    return Succes();
}

Statut ValiderParametresSegmentation(const ValeurJson& j, const fs::path& dossier) {
    auto p = LireParametres(j, dossier);
    if (!p.Ok()) return p.Err();
    if (j.Membre("experience").Texte().empty()) return Proto("champ \"experience\" (tache publiee) manquant");
    return Succes();
}

Statut ExecuterEchantillonnage(const ValeurJson& j, const fs::path& dossier, const fs::path& staging,
                               const std::atomic<bool>& annulation) {
    Statut st = ValiderEchantillonnage(j, dossier);
    if (!st.Ok()) return st;
    ParametresCouleur p = LireParametres(j, dossier).Valeur();
    std::vector<ZoneEtiquetee> zones;
    if (!(st = LireZones(j, zones)).Ok()) return st;
    auto image = LireImageCouleur(p.m_image);
    if (!image.Ok()) return image.Err();
    if (!(st = ZonesDansImage(zones, image.Valeur())).Ok()) return st;
    auto carac = CaracteristiquesCouleur(image.Valeur(), p.m_espace, p.m_fenetre);
    if (!carac.Ok()) return carac.Err();
    if (!(st = Annule(annulation)).Ok()) return st;
    const int parZone = j.Contient("par_zone") ? (int)j.Membre("par_zone").Entier() : 200;
    std::uint64_t etat = (std::uint64_t)j.Membre("graine").Entier();
    Echantillons e;
    for (auto& z : zones) EchantillonnerZone(z, carac.Valeur(), parZone, etat, e);
    if (!(st = EcrireFichierAtomique(staging / "echantillons.csv", e.m_csv, false)).Ok()) return st;
    ValeurJson d = ValeurJson::Objet();
    d["schema"] = "decid.echantillons-couleur/1";
    d["request_id"] = j.Membre("request_id").Texte();
    d["operation"] = "echantillonner_couleurs";
    d["image"] = DescriptionImage(p.m_image, image.Valeur());
    DecrireCaracteristique(d, p);
    d["graine"] = j.Membre("graine").Entier();
    d["par_zone"] = parZone;
    d["zones"] = ZonesJson(zones);
    ValeurJson comptes = ValeurJson::Objet();
    for (std::size_t k = 0; k < e.m_classes.size(); ++k) comptes[e.m_classes[k]] = e.m_comptes[k];
    d["comptes"] = comptes;
    d["fichier"] = "echantillons.csv";
    d["format"] = "CSV sans en-tete, premiere colonne = classe, puis les trois canaux (9 chiffres significatifs)";
    d["sha256_csv"] = Sha256Texte(e.m_csv);
    return EcrireFichierAtomique(staging / "echantillons.json", d.Ecrire(), false);
}

Statut ExecuterSegmentation(const ValeurJson& j, const fs::path& dossier, const fs::path& staging,
                            const std::atomic<bool>& annulation) {
    Statut st = ValiderParametresSegmentation(j, dossier);
    if (!st.Ok()) return st;
    ParametresCouleur p = LireParametres(j, dossier).Valeur();
    bool echantillonsVerifies = false;
    if (!(st = ControlerEchantillons(j, dossier, p, echantillonsVerifies)).Ok()) return st;
    auto e = ChargerExperiencePubliee(Resoudre(j.Membre("experience").Texte(), dossier));
    if (!e.Ok()) return e.Err();
    if (e.Valeur().m_modele.m_nbAttributs != 3)
        return FaireErreur(CodeErreur::ModeleIncompatible, "experience_source",
                           "le modele doit avoir ete entraine sur 3 attributs de couleur");
    auto image = LireImageCouleur(p.m_image);
    if (!image.Ok()) return image.Err();
    auto carac = CaracteristiquesCouleur(image.Valeur(), p.m_espace, p.m_fenetre);
    if (!carac.Ok()) return carac.Err();
    auto classes = ClasserPixels(e.Valeur(), carac.Valeur(), annulation);
    if (!classes.Ok()) return classes.Err();
    ImageDecision decision;
    decision.m_largeur = image.Valeur().cols;
    decision.m_hauteur = image.Valeur().rows;
    decision.m_classes = classes.Valeur();
    auto png = PngColorise(decision);
    if (!png.Ok()) return png.Err();
    if (!(st = Annule(annulation)).Ok()) return st;
    const auto& c = decision.m_classes;
    if (!(st = EcrireFichierAtomique(staging / "segmentation.i16", c.data(), c.size() * sizeof(std::int16_t), false)).Ok())
        return st;
    if (!(st = EcrireFichierAtomique(staging / "segmentation.png", png.Valeur().data(), png.Valeur().size(), false)).Ok())
        return st;
    ValeurJson d = ValeurJson::Objet();
    d["schema"] = "decid.segmentation/1";
    d["request_id"] = j.Membre("request_id").Texte();
    d["operation"] = "segmenter";
    d["experience"] = j.Membre("experience").Texte();
    d["hash_modele_source"] = e.Valeur().m_hashModele;
    d["famille"] = NomFamille(e.Valeur().m_modele.m_famille);
    d["image"] = DescriptionImage(p.m_image, image.Valeur());
    DecrireCaracteristique(d, p);
    d["echantillons_verifies"] = echantillonsVerifies;
    d["largeur"] = decision.m_largeur;
    d["hauteur"] = decision.m_hauteur;
    d["classes"] = ClassesJson(e.Valeur(), c);
    d["palette"] = PaletteParDefaut().m_version;
    d["format_i16"] = "int16 little-endian, lignes de haut en bas, identifiant interne de classe";
    d["sha256_classes"] = Sha256Fichier(staging / "segmentation.i16");
    return EcrireFichierAtomique(staging / "segmentation.json", d.Ecrire(), false);
}

} // namespace decid
