// Normalisation min-max ajustee sur l'apprentissage seulement, ou imposee par un fichier .lim.
#include "Normalisation.h"

#include <cfloat>
#include <charconv>
#include <cmath>
#include <sstream>

#include "Fichiers.h"
#include "Sha256.h"

namespace decid {

namespace {

Erreur ErreurNorm(const std::string& message) {
    return FaireErreur(CodeErreur::DonneesInvalides, "normalisation", message);
}

std::string NombreTexte(double v) {
    char tampon[64];
    auto r = std::to_chars(tampon, tampon + sizeof(tampon), v);
    return std::string(tampon, r.ptr);
}

bool EstConstant(const Normalisation& n, int a) {
    for (int c : n.m_attributsConstants) if (c == a) return true;
    return false;
}

double Transformer(const Normalisation& n, int a, bool constant, double v) {
    if (constant) return n.m_bas;
    return n.m_bas + (n.m_haut - n.m_bas) * (v - n.m_min[a]) / (n.m_max[a] - n.m_min[a]);
}

ValeurJson TableauNombres(const std::vector<double>& v) {
    ValeurJson t = ValeurJson::Tableau();
    for (double x : v) t.Ajouter(ValeurJson(x));
    return t;
}

// ---- fichier .lim

Erreur ErreurLim(const std::string& message, const std::string& fichier, long ligne) {
    return FaireErreur(CodeErreur::DonneesInvalides, "normalisation", "fichier .lim : " + message, fichier, ligne);
}

// Champs separes par espaces ou tabulations (fin de ligne  toleree)
std::vector<std::string> Champs(const std::string& ligne) {
    std::vector<std::string> champs;
    std::istringstream flux(ligne);
    std::string champ;
    while (flux >> champ) champs.push_back(champ);
    return champs;
}

bool NombreStrict(const std::string& texte, double& v) {
    const char* fin = texte.data() + texte.size();
    auto r = std::from_chars(texte.data(), fin, v);
    return r.ec == std::errc() && r.ptr == fin && std::isfinite(v);
}

struct LigneLim { long m_numero; std::vector<std::string> m_champs; };

std::vector<LigneLim> LignesNonVides(const std::string& texte) {
    std::vector<LigneLim> lignes;
    std::istringstream flux(texte);
    std::string ligne;
    long numero = 0;
    while (std::getline(flux, ligne)) {
        ++numero;
        auto champs = Champs(ligne);
        if (!champs.empty()) lignes.push_back({numero, champs});
    }
    return lignes;
}

Statut LireNombreAttributs(const std::vector<LigneLim>& lignes, const std::string& nom, int nbAttributs) {
    if (lignes.empty()) return ErreurLim("fichier vide", nom, 1);
    const LigneLim& l = lignes.front();
    double v = 0;
    if (l.m_champs.size() != 1 || !NombreStrict(l.m_champs[0], v) || v < 1 || std::floor(v) != v)
        return ErreurLim("la premiere ligne doit contenir le nombre d'attributs (entier >= 1)", nom, l.m_numero);
    if ((int)v != nbAttributs)
        return ErreurLim("le fichier decrit " + l.m_champs[0] + " attribut(s), la source en a " +
                         std::to_string(nbAttributs), nom, l.m_numero);
    return Succes();
}

Statut LirePaire(const LigneLim& l, const std::string& nom, int attribut, double& mini, double& maxi) {
    const std::string attr = "attribut " + std::to_string(attribut + 1);
    if (l.m_champs.size() != 2)
        return ErreurLim(attr + " : deux valeurs \"min max\" attendues, " + std::to_string(l.m_champs.size()) +
                         " trouvee(s)", nom, l.m_numero);
    if (!NombreStrict(l.m_champs[0], mini) || !NombreStrict(l.m_champs[1], maxi))
        return ErreurLim(attr + " : valeur non numerique ou non finie (point decimal attendu)", nom, l.m_numero);
    if (mini > maxi) return ErreurLim(attr + " : min superieur a max", nom, l.m_numero);
    return Succes();
}

} // namespace

std::string Normalisation::Hash() const {
    std::string s = "decid.normalisation/v1;methode=" + m_methode;
    s += ";bas=" + NombreTexte(m_bas) + ";haut=" + NombreTexte(m_haut);
    s += ";hashPartage=" + m_hashPartage + ";min=";
    for (double x : m_min) s += NombreTexte(x) + ",";
    s += ";max=";
    for (double x : m_max) s += NombreTexte(x) + ",";
    s += ";constants=";
    for (int c : m_attributsConstants) s += std::to_string(c) + ",";
    return Sha256Texte(s);
}

ValeurJson Normalisation::VersJson() const {
    ValeurJson j = ValeurJson::Objet();
    j["methode"] = ValeurJson(m_methode);
    j["bas"] = ValeurJson(m_bas);
    j["haut"] = ValeurJson(m_haut);
    j["min"] = TableauNombres(m_min);
    j["max"] = TableauNombres(m_max);
    j["hash_partage"] = ValeurJson(m_hashPartage);
    ValeurJson c = ValeurJson::Tableau();
    for (int a : m_attributsConstants) c.Ajouter(ValeurJson(a));
    j["attributs_constants"] = c;
    j["hash"] = ValeurJson(Hash());
    return j;
}

Resultat<Normalisation> AjusterMinMax(const JeuDeDonnees& jeu, const std::vector<int>& lignesApprentissage,
                                      double bas, double haut, const std::string& hashPartage) {
    if (!std::isfinite(bas) || !std::isfinite(haut) || !(haut > bas))
        return ErreurNorm("plage de sortie invalide : le haut doit etre superieur au bas");
    if (lignesApprentissage.empty()) return ErreurNorm("apprentissage vide : normalisation impossible");
    Normalisation n;
    n.m_methode = "minmax";
    n.m_bas = bas;
    n.m_haut = haut;
    n.m_hashPartage = hashPartage;
    n.m_min.assign(jeu.m_nbAttributs, DBL_MAX);
    n.m_max.assign(jeu.m_nbAttributs, -DBL_MAX);
    for (int i : lignesApprentissage) {
        if (i < 0 || i >= jeu.m_nbLignes) return ErreurNorm("indice de ligne hors limites : " + std::to_string(i));
        for (int a = 0; a < jeu.m_nbAttributs; ++a) {
            double v = jeu.Valeur(i, a);
            if (v < n.m_min[a]) n.m_min[a] = v;
            if (v > n.m_max[a]) n.m_max[a] = v;
        }
    }
    for (int a = 0; a < jeu.m_nbAttributs; ++a)
        if (n.m_min[a] == n.m_max[a]) n.m_attributsConstants.push_back(a);
    return n;
}

Normalisation AucuneNormalisation(int nbAttributs) {
    (void)nbAttributs;
    Normalisation n;
    n.m_methode = "aucune";
    return n;
}

Resultat<Normalisation> LireBornesLim(const std::string& texte, const std::string& nomFichier, int nbAttributs,
                                      double bas, double haut, const std::string& hashPartage) {
    if (!std::isfinite(bas) || !std::isfinite(haut) || !(haut > bas))
        return ErreurNorm("plage de sortie invalide : le haut doit etre superieur au bas");
    const std::vector<LigneLim> lignes = LignesNonVides(texte);
    Statut st = LireNombreAttributs(lignes, nomFichier, nbAttributs);
    if (!st.Ok()) return st.Err();
    if ((int)lignes.size() - 1 < nbAttributs)
        return ErreurLim(std::to_string(nbAttributs) + " paires min max attendues, " +
                         std::to_string(lignes.size() - 1) + " trouvee(s)", nomFichier, lignes.back().m_numero);
    if ((int)lignes.size() - 1 > nbAttributs)
        return ErreurLim("ligne en trop apres les " + std::to_string(nbAttributs) + " paires", nomFichier,
                         lignes[(size_t)nbAttributs + 1].m_numero);
    Normalisation n;
    n.m_methode = "minmax";
    n.m_bas = bas;
    n.m_haut = haut;
    n.m_hashPartage = hashPartage;
    n.m_min.assign((size_t)nbAttributs, 0.0);
    n.m_max.assign((size_t)nbAttributs, 0.0);
    for (int a = 0; a < nbAttributs; ++a) {
        st = LirePaire(lignes[(size_t)a + 1], nomFichier, a, n.m_min[(size_t)a], n.m_max[(size_t)a]);
        if (!st.Ok()) return st.Err();
        if (n.m_min[(size_t)a] == n.m_max[(size_t)a]) n.m_attributsConstants.push_back(a);
    }
    return n;
}

Resultat<Normalisation> ChargerBornesLim(const std::filesystem::path& chemin, int nbAttributs, double bas,
                                         double haut, const std::string& hashPartage) {
    auto texte = LireFichierTexte(chemin);
    if (!texte.Ok()) return texte.Err();
    return LireBornesLim(texte.Valeur(), CheminUtf8(chemin), nbAttributs, bas, haut, hashPartage);
}

Resultat<Normalisation> NormalisationDemandee(const std::string& methode, const std::filesystem::path& fichierLim,
                                              double bas, double haut, const JeuDeDonnees& jeu,
                                              const std::vector<int>& lignesApprentissage,
                                              const std::string& hashPartage) {
    if (methode == "minmax") return AjusterMinMax(jeu, lignesApprentissage, bas, haut, hashPartage);
    if (methode == "minmax_lim") return ChargerBornesLim(fichierLim, jeu.m_nbAttributs, bas, haut, hashPartage);
    if (methode != "aucune") return ErreurNorm("methode inconnue : " + methode);
    Normalisation n = AucuneNormalisation(jeu.m_nbAttributs);
    n.m_hashPartage = hashPartage;
    return n;
}

Resultat<cv::Mat> AppliquerNormalisation(const Normalisation& n, const JeuDeDonnees& jeu,
                                          const std::vector<int>& lignes, long* nbHorsPlage) {
    const bool minmax = (n.m_methode == "minmax");
    if (minmax && (int)n.m_min.size() != jeu.m_nbAttributs)
        return ErreurNorm("nombre d'attributs different de celui de la normalisation");
    long hors = 0;
    cv::Mat m((int)lignes.size(), jeu.m_nbAttributs, CV_32F);
    for (size_t r = 0; r < lignes.size(); ++r) {
        const int i = lignes[r];
        if (i < 0 || i >= jeu.m_nbLignes) return ErreurNorm("indice de ligne hors limites : " + std::to_string(i));
        float* sortie = m.ptr<float>((int)r);
        for (int a = 0; a < jeu.m_nbAttributs; ++a) {
            const double brut = jeu.Valeur(i, a);
            double t = brut;
            if (minmax) {
                const bool constant = EstConstant(n, a);
                t = Transformer(n, a, constant, brut);
                if (constant ? (brut != n.m_min[a]) : (brut < n.m_min[a] || brut > n.m_max[a])) ++hors;
            }
            if (!std::isfinite(t) || std::fabs(t) > FLT_MAX)
                return ErreurNorm("ligne " + std::to_string(i + 1) + ", attribut " + std::to_string(a + 1) +
                                  " : valeur non representable en float32");
            sortie[a] = static_cast<float>(t);
        }
    }
    if (nbHorsPlage) *nbHorsPlage = hors;
    return m;
}

std::vector<double> TransformerVecteur(const Normalisation& n, const std::vector<double>& brut) {
    if (n.m_methode != "minmax") return brut;
    if (brut.size() != n.m_min.size()) return {};
    std::vector<double> sortie(brut.size());
    for (size_t a = 0; a < brut.size(); ++a)
        sortie[a] = Transformer(n, (int)a, EstConstant(n, (int)a), brut[a]);
    return sortie;
}

Resultat<Normalisation> NormalisationDepuisJson(const ValeurJson& json) {
    if (json.GetType() != ValeurJson::Type::Objet) return ErreurNorm("normalisation JSON : objet attendu");
    Normalisation n;
    n.m_methode = json.Membre("methode").Texte();
    if (n.m_methode != "aucune" && n.m_methode != "minmax")
        return ErreurNorm("normalisation JSON : methode inconnue");
    n.m_bas = json.Membre("bas").Nombre();
    n.m_haut = json.Membre("haut").Nombre();
    for (const auto& e : json.Membre("min").Elements()) n.m_min.push_back(e.Nombre());
    for (const auto& e : json.Membre("max").Elements()) n.m_max.push_back(e.Nombre());
    n.m_hashPartage = json.Membre("hash_partage").Texte();
    for (const auto& e : json.Membre("attributs_constants").Elements())
        n.m_attributsConstants.push_back(static_cast<int>(e.Entier()));
    if (n.m_min.size() != n.m_max.size())
        return ErreurNorm("normalisation JSON : min et max de tailles differentes");
    if (json.Membre("hash").Texte() != n.Hash())
        return ErreurNorm("normalisation JSON : le hash relu ne correspond pas au contenu");
    return n;
}

} // namespace decid
