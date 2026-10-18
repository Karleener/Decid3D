// Images et volumes de decision : implementation (contrat dans Vue.h).
#include "Vue.h"

#include <algorithm>
#include <cmath>
#include <functional>

#include "Sha256.h"

namespace decid {

namespace {

constexpr int kDimensionMax = 4096;       // cote maximale d'une image
constexpr int kResolutionMax = 256;       // resolution maximale d'un volume
constexpr int kLignesParLot = 32;         // lignes d'image par lot de prediction
constexpr std::size_t kVoxelsParLot = 4096;

Erreur ErreurVue(CodeErreur code, const std::string& message) {
    return FaireErreur(code, "vue", message);
}

bool DomaineValide(double mini, double maxi) {
    return std::isfinite(mini) && std::isfinite(maxi) && mini < maxi;
}

bool ValeursFinies(const std::vector<double>& v) {
    for (double x : v)
        if (!std::isfinite(x)) return false;
    return true;
}

// Controles communs a l'image et au volume : modele, normalisation et valeurs fixes coherents entre eux.
bool CompatibleAvecModele(const Modele& modele, const Normalisation& n, const std::vector<double>& fixes,
                          Erreur& erreur) {
    const std::size_t p = fixes.size();
    if (p == 0 || (int)p != modele.m_nbAttributs) {
        erreur = ErreurVue(CodeErreur::DonneesInvalides, "Le nombre de valeurs fixes ne correspond pas au nombre d'attributs du modele.");
        return false;
    }
    if (!ValeursFinies(fixes)) {
        erreur = ErreurVue(CodeErreur::DonneesInvalides, "Une valeur fixe n'est pas un nombre fini.");
        return false;
    }
    if (n.m_methode == "minmax" && (n.m_min.size() != p || n.m_max.size() != p)) {
        erreur = ErreurVue(CodeErreur::DonneesInvalides, "La normalisation ne correspond pas au nombre d'attributs.");
        return false;
    }
    return true;
}

bool BudgetRespecte(std::size_t nbCellules, std::size_t maxOctets, Erreur& erreur) {
    if (nbCellules > maxOctets / 2) {
        erreur = ErreurVue(CodeErreur::Ressources, "La taille demandee depasse le budget memoire autorise.");
        return false;
    }
    return true;
}

bool ValiderImage(const Modele& modele, const Normalisation& n, const RequeteImage& r, Erreur& erreur) {
    if (r.m_largeur < 1 || r.m_largeur > kDimensionMax || r.m_hauteur < 1 || r.m_hauteur > kDimensionMax) {
        erreur = ErreurVue(CodeErreur::DonneesInvalides, "Dimensions d'image hors de 1..4096.");
        return false;
    }
    const int p = (int)r.m_valeursFixes.size();
    if (r.m_attributX == r.m_attributY || r.m_attributX < 0 || r.m_attributY < 0 || r.m_attributX >= p || r.m_attributY >= p) {
        erreur = ErreurVue(CodeErreur::DonneesInvalides, "Attributs X et Y invalides (distincts et dans la plage).");
        return false;
    }
    if (!DomaineValide(r.m_xmin, r.m_xmax) || !DomaineValide(r.m_ymin, r.m_ymax)) {
        erreur = ErreurVue(CodeErreur::DonneesInvalides, "Domaine invalide (bornes finies avec min < max).");
        return false;
    }
    return CompatibleAvecModele(modele, n, r.m_valeursFixes, erreur);
}

bool ValiderVolume(const Modele& modele, const Normalisation& n, const RequeteVolume& r, Erreur& erreur) {
    if (r.m_resolution < 1 || r.m_resolution > kResolutionMax) {
        erreur = ErreurVue(CodeErreur::DonneesInvalides, "Resolution de volume hors de 1..256.");
        return false;
    }
    const int p = (int)r.m_valeursFixes.size();
    for (int a = 0; a < 3; ++a) {
        const int idx = r.m_attributs[a];
        if (idx < 0 || idx >= p || !DomaineValide(r.m_min[a], r.m_max[a])) {
            erreur = ErreurVue(CodeErreur::DonneesInvalides, "Attribut ou domaine de volume invalide.");
            return false;
        }
    }
    if (r.m_attributs[0] == r.m_attributs[1] || r.m_attributs[0] == r.m_attributs[2] || r.m_attributs[1] == r.m_attributs[2]) {
        erreur = ErreurVue(CodeErreur::DonneesInvalides, "Les trois attributs du volume doivent etre distincts.");
        return false;
    }
    return CompatibleAvecModele(modele, n, r.m_valeursFixes, erreur);
}

std::string HashClasses(const std::vector<std::int16_t>& classes) {
    std::vector<std::uint8_t> octets;
    octets.reserve(classes.size() * 2);
    for (std::int16_t c : classes) {
        const std::uint16_t u = (std::uint16_t)c;
        octets.push_back((std::uint8_t)(u & 0xFF));
        octets.push_back((std::uint8_t)(u >> 8));
    }
    Sha256 sha;
    sha.Ajouter(octets.data(), octets.size());
    return sha.FinirHex();
}

// Remplit 'brut' (vecteur brut complet) pour la cellule d'indice lineaire 'indice'.
using Remplisseur = std::function<void(std::size_t indice, std::vector<double>& brut)>;

// Normalise un vecteur brut dans la ligne 'ligne' de la matrice ; faux si une valeur ne tient pas en float32.
bool EcrireLigne(const std::vector<double>& transforme, cv::Mat& lot, int ligne) {
    float* dest = lot.ptr<float>(ligne);
    for (std::size_t k = 0; k < transforme.size(); ++k) {
        const float f = (float)transforme[k];
        if (!std::isfinite(f)) return false;
        dest[k] = f;
    }
    return true;
}

Resultat<cv::Mat> ConstruireLot(const Modele& modele, const Normalisation& n, const Remplisseur& remplir,
                                 std::size_t debut, std::size_t nb, std::vector<double>& brut) {
    cv::Mat lot((int)nb, modele.m_nbAttributs, CV_32F);
    for (std::size_t i = 0; i < nb; ++i) {
        remplir(debut + i, brut);
        if (!EcrireLigne(TransformerVecteur(n, brut), lot, (int)i))
            return ErreurVue(CodeErreur::DonneesInvalides, "Une valeur transformee ne tient pas en float32.");
    }
    return lot;
}

// Classe 'total' cellules par lots de 'tailleLot' ; l'annulation est testee avant chaque lot.
// Classe 'total' cellules par lots ; si 'scores' n'est pas nul, calcule aussi le score de confiance
// (u8 = round(255 x score)) sur le meme lot. L'annulation est testee avant chaque lot.
Resultat<std::vector<std::int16_t>> ClasserParLots(const Modele& modele, const Normalisation& n,
                                                   const std::vector<double>& fixes, std::size_t total,
                                                   std::size_t tailleLot, const Remplisseur& remplir,
                                                   const std::atomic<bool>* annulation,
                                                   std::vector<std::uint8_t>* scores) {
    std::vector<std::int16_t> classes;
    classes.reserve(total);
    if (scores) scores->reserve(total);
    std::vector<double> brut = fixes;
    for (std::size_t debut = 0; debut < total; debut += tailleLot) {
        if (annulation && annulation->load())
            return ErreurVue(CodeErreur::Annulation, "Calcul annule.");
        const std::size_t nb = std::min(tailleLot, total - debut);
        auto lot = ConstruireLot(modele, n, remplir, debut, nb, brut);
        if (!lot.Ok()) return lot.Err();
        auto sortie = Predire(modele, lot.Valeur());
        if (!sortie.Ok()) return sortie.Err();
        for (int c : sortie.Valeur()) classes.push_back((std::int16_t)c);
        if (scores) {
            auto s = ScoresConfiance(modele, lot.Valeur());
            if (!s.Ok()) return s.Err();
            for (float v : s.Valeur()) scores->push_back((std::uint8_t)std::lround(255.0 * v));
        }
    }
    return classes;
}

double CentreAxe(double mini, double maxi, int k, int nb) {
    return mini + (k + 0.5) * (maxi - mini) / nb;
}

} // namespace

// ---------------------------------------------------------------- JSON

ValeurJson RequeteImage::VersJson() const {
    ValeurJson o = ValeurJson::Objet();
    o["largeur"] = m_largeur;
    o["hauteur"] = m_hauteur;
    o["attribut_x"] = m_attributX;
    o["attribut_y"] = m_attributY;
    o["xmin"] = m_xmin;
    o["xmax"] = m_xmax;
    o["ymin"] = m_ymin;
    o["ymax"] = m_ymax;
    ValeurJson fixes = ValeurJson::Tableau();
    for (double v : m_valeursFixes) fixes.Ajouter(v);
    o["valeurs_fixes"] = fixes;
    return o;
}

ValeurJson RequeteVolume::VersJson() const {
    ValeurJson o = ValeurJson::Objet();
    o["resolution"] = m_resolution;
    ValeurJson attributs = ValeurJson::Tableau(), mins = ValeurJson::Tableau(), maxs = ValeurJson::Tableau();
    for (int a = 0; a < 3; ++a) {
        attributs.Ajouter(m_attributs[a]);
        mins.Ajouter(m_min[a]);
        maxs.Ajouter(m_max[a]);
    }
    o["attributs"] = attributs;
    o["min"] = mins;
    o["max"] = maxs;
    ValeurJson fixes = ValeurJson::Tableau();
    for (double v : m_valeursFixes) fixes.Ajouter(v);
    o["valeurs_fixes"] = fixes;
    return o;
}

// ---------------------------------------------------------------- image

void CentrePixel(const RequeteImage& r, int i, int j, double& x, double& y) {
    x = r.m_xmin + (i + 0.5) * (r.m_xmax - r.m_xmin) / r.m_largeur;
    y = r.m_ymax - (j + 0.5) * (r.m_ymax - r.m_ymin) / r.m_hauteur;
}

Resultat<ImageDecision> CalculerImage(const Modele& modele, const Normalisation& normalisation,
                                      const RequeteImage& requete, const std::atomic<bool>* annulation,
                                      std::size_t maxOctets) {
    Erreur erreur;
    if (!ValiderImage(modele, normalisation, requete, erreur)) return erreur;
    const std::size_t largeur = (std::size_t)requete.m_largeur;
    const std::size_t total = largeur * (std::size_t)requete.m_hauteur;
    if (!BudgetRespecte(total, maxOctets, erreur)) return erreur;

    Remplisseur remplir = [&requete, largeur](std::size_t indice, std::vector<double>& brut) {
        double x = 0, y = 0;
        CentrePixel(requete, (int)(indice % largeur), (int)(indice / largeur), x, y);
        brut = requete.m_valeursFixes;
        brut[(std::size_t)requete.m_attributX] = x;
        brut[(std::size_t)requete.m_attributY] = y;
    };
    ImageDecision image;
    image.m_score = DefinirScore(modele);
    auto classes = ClasserParLots(modele, normalisation, requete.m_valeursFixes, total,
                                  largeur * (std::size_t)kLignesParLot, remplir, annulation,
                                  image.m_score.m_defini ? &image.m_scores : nullptr);
    if (!classes.Ok()) return classes.Err();
    image.m_largeur = requete.m_largeur;
    image.m_hauteur = requete.m_hauteur;
    image.m_classes = std::move(classes.Valeur());
    image.m_hash = HashClasses(image.m_classes);
    return image;
}

// ---------------------------------------------------------------- volume

Resultat<VolumeDecision> CalculerVolume(const Modele& modele, const Normalisation& normalisation,
                                        const RequeteVolume& requete, const std::atomic<bool>* annulation,
                                        std::size_t maxOctets) {
    Erreur erreur;
    if (!ValiderVolume(modele, normalisation, requete, erreur)) return erreur;
    const std::size_t res = (std::size_t)requete.m_resolution;
    const std::size_t total = res * res * res;
    if (!BudgetRespecte(total, maxOctets, erreur)) return erreur;

    Remplisseur remplir = [&requete, res](std::size_t indice, std::vector<double>& brut) {
        const int pos[3] = {(int)(indice % res), (int)((indice / res) % res), (int)(indice / (res * res))};
        brut = requete.m_valeursFixes;
        for (int a = 0; a < 3; ++a)
            brut[(std::size_t)requete.m_attributs[a]] =
                CentreAxe(requete.m_min[a], requete.m_max[a], pos[a], requete.m_resolution);
    };
    VolumeDecision volume;
    volume.m_score = DefinirScore(modele);
    auto classes = ClasserParLots(modele, normalisation, requete.m_valeursFixes, total, kVoxelsParLot, remplir,
                                  annulation, volume.m_score.m_defini ? &volume.m_scores : nullptr);
    if (!classes.Ok()) return classes.Err();
    volume.m_resolution = requete.m_resolution;
    volume.m_classes = std::move(classes.Valeur());
    volume.m_hash = HashClasses(volume.m_classes);
    return volume;
}

// ---------------------------------------------------------------- couleurs

Palette PaletteParDefaut() {
    Palette p;
    const std::uint32_t rgb[8] = {0x7A1F3D, 0xFACB39, 0xFD9CF5, 0xCBD8A5, 0xA85419, 0x93398E, 0xA88891, 0x6C0B04};
    for (std::uint32_t c : rgb) p.m_couleurs.push_back((c << 8) | 0xFFu);
    return p;
}

std::vector<std::uint8_t> Coloriser(const ImageDecision& image, const Palette& palette, std::uint8_t alpha) {
    std::vector<std::uint8_t> rgba;
    rgba.reserve(image.m_classes.size() * 4);
    const std::size_t nbCouleurs = palette.m_couleurs.size();
    for (std::int16_t c : image.m_classes) {
        std::uint32_t couleur = 0x808080FFu;
        if (c >= 0 && nbCouleurs > 0) couleur = palette.m_couleurs[(std::size_t)c % nbCouleurs];
        rgba.push_back((std::uint8_t)(couleur >> 24));
        rgba.push_back((std::uint8_t)(couleur >> 16));
        rgba.push_back((std::uint8_t)(couleur >> 8));
        rgba.push_back(alpha);
    }
    return rgba;
}

// ---------------------------------------------------------------- cache

std::string CleCache(const std::string& hashModele, const Normalisation& n, const ValeurJson& requete) {
    return Sha256Texte(hashModele + "|" + n.Hash() + "|" + requete.Ecrire(false));
}

} // namespace decid
