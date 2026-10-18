// Generation de jeux synthetiques (voir Generation.h).
// Requete :
//  {"operation":"generer", "protocol_version":1, "request_id":"...", "modele":"gaussiennes_2d" | "gaussiennes_3d" |
//   "spirales_3d", "graine": 7, "ecretage": {"min":0, "max":1} (facultatif),
//   "classes":[{"nom":"A", "effectif":100, "centre":[x,y(,z)],
//               gaussiennes : "ecart_type": s ou [sx,sy(,sz)] ;
//               spirales    : "diametre", "longueur", "tours", "phase", "bruit", "axe":[vx,vy,vz] (facultatif)}]}
// Tirage portable : uniformes = TirageUniforme (splitmix64, Partage.h) sur 2^53, normales par Box-Muller
// (deux uniformes par valeur, cosinus seul). Ordre : classes dans l'ordre de la requete, puis points.
#include "Generation.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include "Fichiers.h"
#include "Partage.h"
#include "Sha256.h"

namespace decid {

namespace {

const double PI = 3.14159265358979323846;
const long long EFFECTIF_MAX = 1000000;
const std::size_t CLASSES_MAX = 64;
const std::uint64_t DEUX_53 = 1ULL << 53;

Erreur Proto(const std::string& message) { return FaireErreur(CodeErreur::Protocole, "generer", message); }

bool EstNombre(const ValeurJson& j) {
    return j.GetType() == ValeurJson::Type::Nombre || j.GetType() == ValeurJson::Type::Entier;
}

// Generateur portable : meme graine => memes valeurs sur toute plateforme.
struct Tirage {
    std::uint64_t m_etat;
    double Uniforme() { return ((double)TirageUniforme(m_etat, DEUX_53) + 0.5) / (double)DEUX_53; }   // ]0,1[
    double Normale(double moyenne, double ecart) {
        const double u1 = Uniforme(), u2 = Uniforme();
        return moyenne + ecart * std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * PI * u2);
    }
};

// Vecteur de nombres finis de taille n ; un nombre seul est repete (ecart-type isotrope) si repeter.
Statut LireVecteur(const ValeurJson& j, std::size_t n, bool repeter, const std::string& nom, std::vector<double>& v) {
    v.clear();
    if (repeter && EstNombre(j)) v.assign(n, j.Nombre());
    else for (auto& e : j.Elements()) {
        if (!EstNombre(e)) return Proto(nom + " : nombres attendus");
        v.push_back(e.Nombre());
    }
    if (v.size() != n) return Proto(nom + " : " + std::to_string(n) + " valeurs attendues");
    for (double x : v) if (!std::isfinite(x)) return Proto(nom + " : valeur non finie");
    return Succes();
}

double Nb(const ValeurJson& j, const char* cle, double defaut) {
    return j.Contient(cle) ? j.Membre(cle).Nombre() : defaut;
}

bool NomValide(const std::string& nom) {
    if (nom.empty() || nom.size() > 64) return false;
    for (char ch : nom) if (ch == ',' || ch == ';' || ch == '"' || ch == '\n' || ch == '\r' || ch == '\t') return false;
    return true;
}

Statut LireParametresSpirale(const ValeurJson& c, ClasseGeneree& g) {
    g.m_diametre = Nb(c, "diametre", g.m_diametre);
    g.m_longueur = Nb(c, "longueur", g.m_longueur);
    g.m_tours = Nb(c, "tours", g.m_tours);
    g.m_phase = Nb(c, "phase", g.m_phase);
    g.m_bruit = Nb(c, "bruit", g.m_bruit);
    for (double x : {g.m_diametre, g.m_longueur, g.m_tours, g.m_phase, g.m_bruit})
        if (!std::isfinite(x)) return Proto("parametre de spirale non fini");
    if (g.m_diametre < 0 || g.m_bruit < 0 || g.m_tours < 0) return Proto("diametre, tours et bruit doivent etre >= 0");
    if (!c.Contient("axe")) return Succes();
    Statut st = LireVecteur(c.Membre("axe"), 3, false, "axe", g.m_axe);
    if (!st.Ok()) return st;
    const double norme = std::sqrt(g.m_axe[0] * g.m_axe[0] + g.m_axe[1] * g.m_axe[1] + g.m_axe[2] * g.m_axe[2]);
    if (norme < 1e-9) return Proto("axe : vecteur nul");
    for (double& x : g.m_axe) x /= norme;
    return Succes();
}

Statut LireClasse(const ValeurJson& c, const RequeteGeneration& r, ClasseGeneree& g) {
    g.m_nom = c.Membre("nom").Texte();
    if (!NomValide(g.m_nom)) return Proto("nom de classe vide, trop long ou avec separateur");
    const long long effectif = c.Contient("effectif") ? c.Membre("effectif").Entier() : 100;
    if (effectif < 1 || effectif > EFFECTIF_MAX) return Proto("effectif : entre 1 et 1000000 par classe");
    g.m_effectif = (int)effectif;
    const std::size_t d = (std::size_t)r.Dimension();
    Statut st = LireVecteur(c.Membre("centre"), d, false, "centre", g.m_centre);
    if (!st.Ok()) return st;
    if (r.m_modele == ModeleGeneration::Spirales3D) return LireParametresSpirale(c, g);
    st = LireVecteur(c.Membre("ecart_type"), d, true, "ecart_type", g.m_ecartType);
    if (!st.Ok()) return st;
    for (double s : g.m_ecartType) if (s < 0) return Proto("ecart_type : valeur negative");
    return Succes();
}

// Repere de la spirale : axe w, puis u perpendiculaire et v = w x u (formules de l'ancien Decid).
void Repere(const std::vector<double>& w, double u[3], double v[3]) {
    u[0] = -w[1]; u[1] = w[0]; u[2] = 0;
    double n = std::sqrt(u[0] * u[0] + u[1] * u[1]);
    if (n < 1e-6) { u[0] = -w[2]; u[1] = 0; u[2] = w[0]; n = std::sqrt(u[0] * u[0] + u[2] * u[2]); }
    for (int k = 0; k < 3; ++k) u[k] /= n;
    v[0] = w[1] * u[2] - w[2] * u[1];
    v[1] = w[2] * u[0] - w[0] * u[2];
    v[2] = w[0] * u[1] - w[1] * u[0];
}

std::vector<double> AxeTire(Tirage& t) {
    for (;;) {
        std::vector<double> w{2 * t.Uniforme() - 1, 2 * t.Uniforme() - 1, 2 * t.Uniforme() - 1};
        const double n = std::sqrt(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]);
        if (n < 1e-6) continue;
        for (double& x : w) x /= n;
        return w;
    }
}

void AjouterSpirale(const ClasseGeneree& g, const std::vector<double>& w, Tirage& t, std::vector<double>& valeurs) {
    double u[3], v[3];
    Repere(w, u, v);
    const double rayon = g.m_diametre / 2.0;
    for (int i = 0; i < g.m_effectif; ++i) {
        const double s = (double)i / g.m_effectif;
        const double angle = g.m_phase + 2.0 * PI * g.m_tours * s;
        const double xl = rayon * std::cos(angle), yl = rayon * std::sin(angle), zl = s * g.m_longueur;
        for (int k = 0; k < 3; ++k)
            valeurs.push_back(t.Normale(g.m_centre[(size_t)k] + xl * u[k] + yl * v[k] + zl * w[(size_t)k], g.m_bruit));
    }
}

void AjouterGaussienne(const ClasseGeneree& g, int d, Tirage& t, std::vector<double>& valeurs) {
    for (int i = 0; i < g.m_effectif; ++i)
        for (int k = 0; k < d; ++k) valeurs.push_back(t.Normale(g.m_centre[(size_t)k], g.m_ecartType[(size_t)k]));
}

std::string TexteNombre(double x) {
    char tampon[40];
    std::snprintf(tampon, sizeof(tampon), "%.17g", x);
    return tampon;
}

std::string Csv(const RequeteGeneration& r, const JeuGenere& jeu) {
    std::string sortie;
    const std::size_t d = (std::size_t)jeu.m_dimension;
    for (std::size_t l = 0; l < jeu.m_labels.size(); ++l) {
        sortie += r.m_classes[(size_t)jeu.m_labels[l]].m_nom;
        for (std::size_t k = 0; k < d; ++k) sortie += "," + TexteNombre(jeu.m_valeurs[l * d + k]);
        sortie += "\n";
    }
    return sortie;
}

ValeurJson Nombres(const std::vector<double>& v) {
    ValeurJson t = ValeurJson::Tableau();
    for (double x : v) t.Ajouter(x);
    return t;
}

ValeurJson DescriptionClasse(const RequeteGeneration& r, const JeuGenere& jeu, std::size_t i) {
    const ClasseGeneree& g = r.m_classes[i];
    ValeurJson c = ValeurJson::Objet();
    c["nom"] = g.m_nom;
    c["effectif"] = g.m_effectif;
    c["centre"] = Nombres(g.m_centre);
    if (r.m_modele != ModeleGeneration::Spirales3D) {
        c["ecart_type"] = Nombres(g.m_ecartType);
        return c;
    }
    c["diametre"] = g.m_diametre;
    c["longueur"] = g.m_longueur;
    c["tours"] = g.m_tours;
    c["phase"] = g.m_phase;
    c["bruit"] = g.m_bruit;
    c["axe"] = Nombres(jeu.m_axes[i]);
    c["axe_mode"] = g.m_axe.empty() ? "tire" : "explicite";
    return c;
}

ValeurJson Description(const ValeurJson& requete, const RequeteGeneration& r, const JeuGenere& jeu,
                       const std::string& csv) {
    ValeurJson j = ValeurJson::Objet();
    j["schema"] = "decid.generation/1";
    j["request_id"] = requete.Membre("request_id").Texte();
    j["operation"] = "generer";
    j["modele"] = NomModeleGeneration(r.m_modele);
    j["graine"] = (unsigned long long)r.m_graine;
    j["generateur"] = "splitmix64+box_muller_decid_v1";
    const int d = jeu.m_dimension;
    j["nb_lignes"] = (long long)jeu.m_labels.size();
    j["nb_attributs"] = d;
    ValeurJson noms = ValeurJson::Tableau();
    const char* axes[3] = {"x", "y", "z"};
    for (int k = 0; k < d; ++k) noms.Ajouter(axes[k]);
    j["noms_attributs"] = noms;
    ValeurJson classes = ValeurJson::Tableau();
    for (std::size_t i = 0; i < r.m_classes.size(); ++i) classes.Ajouter(DescriptionClasse(r, jeu, i));
    j["classes"] = classes;
    std::vector<double> mini((size_t)d, INFINITY), maxi((size_t)d, -INFINITY);
    for (std::size_t p = 0; p < jeu.m_valeurs.size(); ++p) {
        mini[p % (size_t)d] = std::min(mini[p % (size_t)d], jeu.m_valeurs[p]);
        maxi[p % (size_t)d] = std::max(maxi[p % (size_t)d], jeu.m_valeurs[p]);
    }
    j["min"] = Nombres(mini);
    j["max"] = Nombres(maxi);
    if (r.m_ecreter) j["ecretage"] = Nombres({r.m_min, r.m_max});
    j["fichier"] = "generation.csv";
    j["format"] = "CSV sans en-tete, separateur virgule, premiere colonne = classe, 17 chiffres significatifs";
    j["sha256_csv"] = Sha256Texte(csv);
    j["requete_brute"] = requete;
    return j;
}

} // namespace

const char* NomModeleGeneration(ModeleGeneration m) {
    switch (m) {
        case ModeleGeneration::Gaussiennes2D: return "gaussiennes_2d";
        case ModeleGeneration::Gaussiennes3D: return "gaussiennes_3d";
        case ModeleGeneration::Spirales3D: return "spirales_3d";
    }
    return "?";
}

Resultat<RequeteGeneration> RequeteGenerationDepuisJson(const ValeurJson& j) {
    RequeteGeneration r;
    const std::string modele = j.Membre("modele").Texte();
    bool connu = false;
    for (auto m : {ModeleGeneration::Gaussiennes2D, ModeleGeneration::Gaussiennes3D, ModeleGeneration::Spirales3D})
        if (modele == NomModeleGeneration(m)) { r.m_modele = m; connu = true; }
    if (!connu) return Proto("modele : gaussiennes_2d, gaussiennes_3d ou spirales_3d");
    const long long graine = j.Contient("graine") ? j.Membre("graine").Entier() : 1;
    if (graine < 0) return Proto("graine : entier positif ou nul");
    r.m_graine = (std::uint64_t)graine;
    const auto& classes = j.Membre("classes").Elements();
    if (classes.empty() || classes.size() > CLASSES_MAX) return Proto("classes : entre 1 et 64");
    std::set<std::string> noms;
    long long total = 0;
    for (auto& c : classes) {
        ClasseGeneree g;
        Statut st = LireClasse(c, r, g);
        if (!st.Ok()) return st.Err();
        if (!noms.insert(g.m_nom).second) return Proto("nom de classe en double : " + g.m_nom);
        total += g.m_effectif;
        r.m_classes.push_back(g);
    }
    if (total > EFFECTIF_MAX) return Proto("au plus 1000000 lignes au total");
    if (j.Contient("ecretage")) {
        r.m_ecreter = true;
        r.m_min = j.Membre("ecretage").Membre("min").Nombre();
        r.m_max = j.Membre("ecretage").Membre("max").Nombre();
        if (!std::isfinite(r.m_min) || !std::isfinite(r.m_max) || !(r.m_max > r.m_min))
            return Proto("ecretage : min < max attendus");
    }
    return r;
}

Resultat<JeuGenere> Generer(const RequeteGeneration& r, const std::atomic<bool>* annulation) {
    JeuGenere jeu;
    jeu.m_dimension = r.Dimension();
    Tirage t{r.m_graine};
    for (std::size_t i = 0; i < r.m_classes.size(); ++i) {
        if (annulation && annulation->load())
            return FaireErreur(CodeErreur::Annulation, "annulation", "calcul annule a la demande, rien n'est publie");
        const ClasseGeneree& g = r.m_classes[i];
        if (r.m_modele == ModeleGeneration::Spirales3D) {
            jeu.m_axes.push_back(g.m_axe.empty() ? AxeTire(t) : g.m_axe);
            AjouterSpirale(g, jeu.m_axes.back(), t, jeu.m_valeurs);
        } else {
            AjouterGaussienne(g, jeu.m_dimension, t, jeu.m_valeurs);
        }
        jeu.m_labels.insert(jeu.m_labels.end(), (size_t)g.m_effectif, (int)i);
    }
    if (r.m_ecreter)
        for (double& x : jeu.m_valeurs) x = std::min(std::max(x, r.m_min), r.m_max);
    return jeu;
}

Statut ExecuterGeneration(const ValeurJson& requete, const fs::path& staging, const std::atomic<bool>& annulation) {
    auto r = RequeteGenerationDepuisJson(requete);
    if (!r.Ok()) return r.Err();
    auto jeu = Generer(r.Valeur(), &annulation);
    if (!jeu.Ok()) return jeu.Err();
    const std::string csv = Csv(r.Valeur(), jeu.Valeur());
    Statut st = EcrireFichierAtomique(staging / "generation.csv", csv, false);
    if (!st.Ok()) return st;
    return EcrireFichierAtomique(staging / "generation.json", Description(requete, r.Valeur(), jeu.Valeur(), csv).Ecrire(), false);
}

} // namespace decid
