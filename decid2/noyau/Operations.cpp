// Operations L2 : image, volume, validation, balayage, adaboost pas a pas, perceptron, kmeans, decider (Decision.cpp) ;
// L5 : mlp_pas_a_pas, em_pas_a_pas (bloc "algorithme" famille mlp / em, injecte par defaut ; "suivi_validation":
// "test" pour afficher l exactitude sur le test, sans jamais s en servir). Les operations pas a pas ecrivent aussi
// partage.json.
// Memes conventions que Experience.cpp : ecriture atomique sans remplacement dans le staging,
// annulation verifiee entre etapes, artefacts.json ecrit en dernier.
//
// Conventions de requete documentees ici (le .h ne les fixe pas) :
//  - les operations "pas a pas" (adaboost, perceptron, kmeans), "validation" et "balayage" reutilisent
//    RequeteDepuisJson ; sans bloc "algorithme", {"famille":"svm"} est injecte avant lecture ;
//  - bloc "validation" optionnel pour "balayage" (defaut : k 10, melange, non stratifie, graine 1, apprentissage) ;
//  - classe_positive = identifiant interne de classe (0..K-1) ;
//  - trace.json : decalage et taille sont comptes en float32 (elements), pas en octets.
#include "Operations.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <opencv2/imgcodecs.hpp>
#include "Decision.h"
#include "Explicable.h"
#include "Generation.h"
#include "Regression.h"
#include "Segmentation.h"
#include "Reseau.h"
#include "Fichiers.h"
#include "Sha256.h"
#include "Trace.h"
#include "Validation.h"
#include "Vue.h"

namespace decid {

namespace {

const std::size_t TRACE_MAX_ETATS = 2000;
const std::size_t TRACE_MAX_OCTETS = 64u << 20;

Erreur Proto(const std::string& message) { return FaireErreur(CodeErreur::Protocole, "requete", message); }

struct Contexte {
    const ValeurJson& m_requete;
    const fs::path& m_dossier;
    const fs::path& m_staging;
    const std::atomic<bool>& m_annulation;
    const RapporteurEvenement& m_rapporter;

    std::string Id() const { return m_requete.Membre("request_id").Texte(); }
    Statut Annule() const {
        if (m_annulation.load())
            return FaireErreur(CodeErreur::Annulation, "annulation", "calcul annule a la demande, rien n'est publie");
        return Succes();
    }
    void Rapporter(const std::string& phase, long i, long n, const std::string& detail = "") const {
        if (m_rapporter) m_rapporter(phase, i, n, detail);
    }
    Statut Ecrire(const char* nom, const std::string& texte) const {
        return EcrireFichierAtomique(m_staging / nom, texte, false);
    }
    Statut EcrireOctets(const char* nom, const void* donnees, std::size_t taille) const {
        static const char vide = 0;
        return EcrireFichierAtomique(m_staging / nom, taille ? donnees : &vide, taille, false);
    }
};

// ---------------------------------------------------------------- lecture de parametres

bool EstNombre(const ValeurJson& j) {
    return j.GetType() == ValeurJson::Type::Nombre || j.GetType() == ValeurJson::Type::Entier;
}
double Nb(const ValeurJson& j, const char* cle, double defaut) {
    return j.Contient(cle) ? j.Membre(cle).Nombre() : defaut;
}
long long Ent(const ValeurJson& j, const char* cle, long long defaut) {
    return j.Contient(cle) ? j.Membre(cle).Entier() : defaut;
}

Resultat<fs::path> ResoudreChemin(const std::string& texte, const fs::path& dossier) {
    if (texte.empty()) return Proto("chemin d'experience manquant");
    fs::path p = CheminDepuisUtf8(texte);
    return p.is_relative() ? dossier / p : p;
}

Resultat<ValeurJson> LireJsonFichier(const fs::path& chemin) {
    auto texte = LireFichierTexte(chemin);
    if (!texte.Ok()) return texte.Err();
    return LireJson(texte.Valeur());
}

Erreur NonPubliee(const fs::path& tache, const std::string& raison) {
    return FaireErreur(CodeErreur::Lecture, "experience_source",
                       "experience source non publiee : " + raison, CheminUtf8(tache));
}

// Commit coherent : commit.json present, sha256_artefacts = hash de resultat/artefacts.json, artefacts intacts.
Statut VerifierPublication(const fs::path& tache) {
    const fs::path resultat = tache / "resultat";
    auto commit = LireJsonFichier(tache / "commit.json");
    if (!commit.Ok()) return NonPubliee(tache, "commit.json absent ou illisible");
    const std::string hash = Sha256Fichier(resultat / "artefacts.json");
    if (hash.empty() || hash != commit.Valeur().Membre("sha256_artefacts").Texte())
        return NonPubliee(tache, "commit.json incoherent avec resultat/artefacts.json");
    return VerifierStaging(resultat);
}

// ---------------------------------------------------------------- domaines et valeurs fixes

void DomaineAuto(const JeuDeDonnees& jeu, int attribut, double& mini, double& maxi) {
    mini = jeu.Valeur(0, attribut);
    maxi = mini;
    for (int i = 1; i < jeu.m_nbLignes; ++i) {
        mini = std::min(mini, jeu.Valeur(i, attribut));
        maxi = std::max(maxi, jeu.Valeur(i, attribut));
    }
    const double etendue = maxi - mini;
    const double marge = etendue > 0 ? 0.05 * etendue : 0.5;
    mini -= marge;
    maxi += marge;
}

Statut ControlerAttribut(int a, const JeuDeDonnees& jeu) {
    if (a < 0 || a >= jeu.m_nbAttributs) return Proto("indice d'attribut hors plage");
    return Succes();
}

bool EstAuto(const ValeurJson& d) {
    return d.EstNul() || (d.GetType() == ValeurJson::Type::Texte && d.Texte() == "auto");
}

Resultat<std::vector<double>> ValeursFixes(const ValeurJson& j, const ExperiencePubliee& e, std::string& mode) {
    const ValeurJson& v = j.Membre("valeurs_fixes");
    const JeuDeDonnees& jeu = e.m_jeu;
    std::vector<double> fixes((size_t)jeu.m_nbAttributs, 0.0);
    if (v.EstNul() || (v.GetType() == ValeurJson::Type::Texte && v.Texte() == "moyenne_apprentissage")) {
        mode = "moyenne_apprentissage";
        for (int i : e.m_partage.m_apprentissage)
            for (int a = 0; a < jeu.m_nbAttributs; ++a) fixes[(size_t)a] += jeu.Valeur(i, a);
        for (double& f : fixes) f /= (double)e.m_partage.m_apprentissage.size();
        return fixes;
    }
    if (v.GetType() != ValeurJson::Type::Tableau || (int)v.Elements().size() != jeu.m_nbAttributs)
        return Proto("valeurs_fixes : \"moyenne_apprentissage\" ou un tableau d'une valeur brute par attribut");
    mode = "explicite";
    for (size_t a = 0; a < fixes.size(); ++a) {
        if (!EstNombre(v.Elements()[a])) return Proto("valeurs_fixes : nombres attendus");
        fixes[a] = v.Elements()[a].Nombre();
    }
    return fixes;
}

ValeurJson TableauNombres(const std::vector<double>& v) {
    ValeurJson t = ValeurJson::Tableau();
    for (double x : v) t.Ajouter(x);
    return t;
}

Statut DomaineImage(const ValeurJson& d, const JeuDeDonnees& jeu, RequeteImage& r, bool& automatique) {
    automatique = EstAuto(d);
    if (automatique) {
        DomaineAuto(jeu, r.m_attributX, r.m_xmin, r.m_xmax);
        DomaineAuto(jeu, r.m_attributY, r.m_ymin, r.m_ymax);
        return Succes();
    }
    if (d.GetType() != ValeurJson::Type::Tableau || d.Elements().size() != 4)
        return Proto("domaine : \"auto\" ou [xmin, xmax, ymin, ymax]");
    for (const auto& v : d.Elements()) if (!EstNombre(v)) return Proto("domaine : nombres attendus");
    r.m_xmin = d.Elements()[0].Nombre(); r.m_xmax = d.Elements()[1].Nombre();
    r.m_ymin = d.Elements()[2].Nombre(); r.m_ymax = d.Elements()[3].Nombre();
    return Succes();
}

Statut DomaineVolume(const ValeurJson& d, const JeuDeDonnees& jeu, RequeteVolume& r, bool& automatique) {
    automatique = EstAuto(d);
    if (automatique) {
        for (int a = 0; a < 3; ++a) DomaineAuto(jeu, r.m_attributs[a], r.m_min[a], r.m_max[a]);
        return Succes();
    }
    if (d.GetType() != ValeurJson::Type::Tableau || d.Elements().size() != 3)
        return Proto("domaine : \"auto\" ou [[min,max],[min,max],[min,max]]");
    for (size_t a = 0; a < 3; ++a) {
        const ValeurJson& p = d.Elements()[a];
        if (p.GetType() != ValeurJson::Type::Tableau || p.Elements().size() != 2 ||
            !EstNombre(p.Elements()[0]) || !EstNombre(p.Elements()[1]))
            return Proto("domaine : trois paires [min,max] de nombres attendues");
        r.m_min[a] = p.Elements()[0].Nombre();
        r.m_max[a] = p.Elements()[1].Nombre();
    }
    return Succes();
}

// ---------------------------------------------------------------- artefacts

ValeurJson ListeNoms(const std::vector<std::string>& noms) {
    ValeurJson t = ValeurJson::Tableau();
    for (const auto& n : noms) t.Ajouter(n);
    return t;
}

ValeurJson TableauEntiers(const std::vector<int>& v) {
    ValeurJson t = ValeurJson::Tableau();
    for (int x : v) t.Ajouter(x);
    return t;
}

// Source commune des operations qui chargent une experience publiee.
Resultat<ExperiencePubliee> ChargerSourcePubliee(const Contexte& c, std::string& experience) {
    experience = c.m_requete.Membre("experience").Texte();
    auto chemin = ResoudreChemin(experience, c.m_dossier);
    if (!chemin.Ok()) return chemin.Err();
    return ChargerExperiencePubliee(chemin.Valeur());
}

ValeurJson EnTeteSortie(const Contexte& c, const char* schema) {
    ValeurJson j = ValeurJson::Objet();
    j["schema"] = schema;
    j["request_id"] = c.Id();
    j["operation"] = c.m_requete.Membre("operation").Texte();
    return j;
}

// ---------------------------------------------------------------- image

Resultat<RequeteImage> LireRequeteImage(const ValeurJson& j, const ExperiencePubliee& e, std::string& modeDomaine,
                                        std::string& modeFixes) {
    RequeteImage r;
    r.m_largeur = (int)Ent(j, "largeur", 500);
    r.m_hauteur = (int)Ent(j, "hauteur", 500);
    r.m_attributX = (int)Ent(j, "attribut_x", 0);
    r.m_attributY = (int)Ent(j, "attribut_y", 1);
    for (int a : {r.m_attributX, r.m_attributY}) {
        Statut s = ControlerAttribut(a, e.m_jeu);
        if (!s.Ok()) return s.Err();
    }
    bool automatique = false;
    Statut sd = DomaineImage(j.Membre("domaine"), e.m_jeu, r, automatique);
    if (!sd.Ok()) return sd.Err();
    modeDomaine = automatique ? "auto" : "explicite";
    auto fixes = ValeursFixes(j, e, modeFixes);
    if (!fixes.Ok()) return fixes.Err();
    r.m_valeursFixes = fixes.Valeur();
    return r;
}

Resultat<std::vector<std::uint8_t>> EncoderPng(const ImageDecision& image) {
    std::vector<std::uint8_t> rgba = Coloriser(image, PaletteParDefaut());
    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) std::swap(rgba[i], rgba[i + 2]);   // RGBA -> BGRA
    cv::Mat m(image.m_hauteur, image.m_largeur, CV_8UC4, rgba.data());
    std::vector<std::uint8_t> png;
    try {
        if (!cv::imencode(".png", m, png))
            return FaireErreur(CodeErreur::Calcul, "image", "encodage PNG refuse");
    } catch (const cv::Exception& ex) {
        return FaireErreur(CodeErreur::Calcul, "image", std::string("encodage PNG : ") + ex.what());
    }
    return png;
}

ValeurJson DescriptionImage(const Contexte& c, const ExperiencePubliee& e, const RequeteImage& r,
                            const ImageDecision& image, const std::string& experience,
                            const std::string& modeDomaine, const std::string& modeFixes) {
    ValeurJson j = EnTeteSortie(c, "decid.image/1");
    j["experience"] = experience;
    j["requete"] = r.VersJson();
    j["requete_brute"] = c.m_requete;
    j["largeur"] = r.m_largeur;
    j["hauteur"] = r.m_hauteur;
    ValeurJson axes = ValeurJson::Objet();
    axes["x"] = r.m_attributX;
    axes["y"] = r.m_attributY;
    axes["nom_x"] = e.m_jeu.m_nomsAttributs[(size_t)r.m_attributX];
    axes["nom_y"] = e.m_jeu.m_nomsAttributs[(size_t)r.m_attributY];
    j["axes"] = axes;
    ValeurJson domaine = ValeurJson::Objet();
    domaine["mode"] = modeDomaine;
    domaine["x"] = TableauNombres({r.m_xmin, r.m_xmax});
    domaine["y"] = TableauNombres({r.m_ymin, r.m_ymax});
    j["domaine"] = domaine;
    j["valeurs_fixes"] = TableauNombres(r.m_valeursFixes);
    j["valeurs_fixes_mode"] = modeFixes;
    j["convention"] = "ligne 0 = y max ; x croit vers la droite, y vers le haut ; centre du pixel (i,j) : "
                      "x = xmin + (i+0.5)*(xmax-xmin)/largeur, y = ymax - (j+0.5)*(ymax-ymin)/hauteur";
    j["format_i16"] = "int16 little-endian, lignes de haut en bas, identifiant interne de classe";
    j["classes"] = ListeNoms(e.m_jeu.m_nomsClasses);
    j["palette"] = PaletteParDefaut().m_version;
    j["hash_image"] = image.m_hash;
    j["hash_modele_source"] = e.m_hashModele;
    j["hash_normalisation"] = e.m_normalisation.Hash();
    j["cle_cache"] = CleCache(e.m_hashModele, e.m_normalisation, r.VersJson());
    return j;
}

// Bloc "score" publie avec une image ou un volume ; ecrit le fichier de scores s'il est defini.
Statut EcrireScore(const Contexte& c, const char* fichier, const DefinitionScore& d,
                   const std::vector<std::uint8_t>& scores, ValeurJson& description) {
    ValeurJson b = ValeurJson::Objet();
    b["schema"] = "decid.score-confiance/1";
    b["definition"] = d.m_definition;
    b["bornes"] = TableauNombres({0.0, 1.0});
    b["sens"] = "0 = sur la frontiere, 1 = decision franche ; la classe publiee n'est jamais modifiee";
    if (!d.m_defini) {
        b["statut"] = "non_defini";
        b["raison"] = d.m_raison;
        description["score"] = b;
        return Succes();
    }
    b["statut"] = "defini";
    b["fichier"] = fichier;
    b["format"] = "uint8, meme ordre que les classes ; score = valeur / 255";
    description["score"] = b;
    return c.EcrireOctets(fichier, scores.data(), scores.size());
}

Statut OperationImage(const Contexte& c) {
    c.Rapporter("chargement", 1, 4, "experience publiee");
    std::string experience, modeDomaine, modeFixes;
    auto e = ChargerSourcePubliee(c, experience);
    if (!e.Ok()) return e.Err();
    auto r = LireRequeteImage(c.m_requete, e.Valeur(), modeDomaine, modeFixes);
    if (!r.Ok()) return r.Err();
    Statut st = c.Annule();
    if (!st.Ok()) return st;
    c.Rapporter("calcul", 2, 4, "classification des pixels");
    auto image = CalculerImage(e.Valeur().m_modele, e.Valeur().m_normalisation, r.Valeur(), &c.m_annulation);
    if (!image.Ok()) return image.Err();
    auto png = EncoderPng(image.Valeur());
    if (!png.Ok()) return png.Err();
    if (!(st = c.Annule()).Ok()) return st;
    c.Rapporter("ecriture", 3, 4, "");
    const auto& classes = image.Valeur().m_classes;
    if (!(st = c.EcrireOctets("image.i16", classes.data(), classes.size() * sizeof(std::int16_t))).Ok()) return st;
    if (!(st = c.EcrireOctets("image.png", png.Valeur().data(), png.Valeur().size())).Ok()) return st;
    ValeurJson description = DescriptionImage(c, e.Valeur(), r.Valeur(), image.Valeur(), experience, modeDomaine, modeFixes);
    if (!(st = EcrireScore(c, "image_score.u8", image.Valeur().m_score, image.Valeur().m_scores, description)).Ok()) return st;
    return c.Ecrire("image.json", description.Ecrire());
}

// ---------------------------------------------------------------- volume

Resultat<RequeteVolume> LireRequeteVolume(const ValeurJson& j, const ExperiencePubliee& e, std::string& modeDomaine,
                                          std::string& modeFixes) {
    RequeteVolume r;
    r.m_resolution = (int)Ent(j, "resolution", 40);
    const ValeurJson& attributs = j.Membre("attributs");
    if (!attributs.EstNul()) {
        if (attributs.GetType() != ValeurJson::Type::Tableau || attributs.Elements().size() != 3)
            return Proto("attributs : trois indices attendus");
        for (size_t a = 0; a < 3; ++a) r.m_attributs[a] = (int)attributs.Elements()[a].Entier();
    }
    for (int a : r.m_attributs) {
        Statut s = ControlerAttribut(a, e.m_jeu);
        if (!s.Ok()) return s.Err();
    }
    bool automatique = false;
    Statut sd = DomaineVolume(j.Membre("domaine"), e.m_jeu, r, automatique);
    if (!sd.Ok()) return sd.Err();
    modeDomaine = automatique ? "auto" : "explicite";
    auto fixes = ValeursFixes(j, e, modeFixes);
    if (!fixes.Ok()) return fixes.Err();
    r.m_valeursFixes = fixes.Valeur();
    return r;
}

ValeurJson DescriptionVolume(const Contexte& c, const ExperiencePubliee& e, const RequeteVolume& r,
                             const VolumeDecision& volume, const std::string& experience,
                             const std::string& modeDomaine, const std::string& modeFixes) {
    ValeurJson j = EnTeteSortie(c, "decid.volume/1");
    j["experience"] = experience;
    j["requete"] = r.VersJson();
    j["requete_brute"] = c.m_requete;
    j["resolution"] = r.m_resolution;
    ValeurJson attributs = ValeurJson::Tableau(), noms = ValeurJson::Tableau();
    for (int a : r.m_attributs) { attributs.Ajouter(a); noms.Ajouter(e.m_jeu.m_nomsAttributs[(size_t)a]); }
    j["attributs"] = attributs;
    j["noms_attributs"] = noms;
    j["domaine_mode"] = modeDomaine;
    j["valeurs_fixes"] = TableauNombres(r.m_valeursFixes);
    j["valeurs_fixes_mode"] = modeFixes;
    j["convention"] = "voxels ordonnes z, puis y, puis x (x varie le plus vite) ; axes croissants ; "
                      "centre : v = min + (k+0.5)*(max-min)/resolution";
    j["format_i16"] = "int16 little-endian, identifiant interne de classe";
    j["classes"] = ListeNoms(e.m_jeu.m_nomsClasses);
    j["hash_volume"] = volume.m_hash;
    j["hash_modele_source"] = e.m_hashModele;
    j["hash_normalisation"] = e.m_normalisation.Hash();
    j["cle_cache"] = CleCache(e.m_hashModele, e.m_normalisation, r.VersJson());
    return j;
}

Statut OperationVolume(const Contexte& c) {
    c.Rapporter("chargement", 1, 4, "experience publiee");
    std::string experience, modeDomaine, modeFixes;
    auto e = ChargerSourcePubliee(c, experience);
    if (!e.Ok()) return e.Err();
    auto r = LireRequeteVolume(c.m_requete, e.Valeur(), modeDomaine, modeFixes);
    if (!r.Ok()) return r.Err();
    Statut st = c.Annule();
    if (!st.Ok()) return st;
    c.Rapporter("calcul", 2, 4, "classification des voxels");
    auto volume = CalculerVolume(e.Valeur().m_modele, e.Valeur().m_normalisation, r.Valeur(), &c.m_annulation);
    if (!volume.Ok()) return volume.Err();
    if (!(st = c.Annule()).Ok()) return st;
    c.Rapporter("ecriture", 3, 4, "");
    const auto& classes = volume.Valeur().m_classes;
    if (!(st = c.EcrireOctets("volume.i16", classes.data(), classes.size() * sizeof(std::int16_t))).Ok()) return st;
    ValeurJson description = DescriptionVolume(c, e.Valeur(), r.Valeur(), volume.Valeur(), experience, modeDomaine, modeFixes);
    if (!(st = EcrireScore(c, "volume_score.u8", volume.Valeur().m_score, volume.Valeur().m_scores, description)).Ok()) return st;
    return c.Ecrire("volume.json", description.Ecrire());
}

// ---------------------------------------------------------------- requetes avec source (validation, pas a pas)

// Famille injectee quand la requete n'a pas de bloc "algorithme" (mlp_pas_a_pas -> mlp, em_pas_a_pas -> em)
const char* FamilleParDefaut(const std::string& operation) {
    if (operation == "mlp_pas_a_pas") return "mlp";
    if (operation == "em_pas_a_pas") return "em";
    return "svm";
}

ValeurJson AvecAlgorithme(const ValeurJson& requete) {
    ValeurJson copie = requete;
    if (!copie.Contient("algorithme")) {
        ValeurJson a = ValeurJson::Objet();
        a["famille"] = FamilleParDefaut(requete.Membre("operation").Texte());
        copie["algorithme"] = a;
    }
    return copie;
}

Resultat<RequeteExperience> RequeteSource(const Contexte& c) {
    return RequeteDepuisJson(AvecAlgorithme(c.m_requete), c.m_dossier);
}

Resultat<Normalisation> NormaliserRequete(const RequeteExperience& req, const JeuDeDonnees& jeu, const Partage& pa) {
    return NormalisationDemandee(req.m_normalisation, req.m_fichierLim, req.m_bas, req.m_haut, jeu, pa.m_apprentissage,
                                 pa.Hash());
}

struct Apprentissage {
    RequeteExperience m_req;
    JeuDeDonnees m_jeu;
    Partage m_partage;
    Normalisation m_normalisation;
    cv::Mat m_x;                    // lignes d'apprentissage normalisees
    std::vector<int> m_labels;      // labels internes des lignes d'apprentissage
};

Resultat<Apprentissage> PreparerApprentissage(const Contexte& c) {
    auto req = RequeteSource(c);
    if (!req.Ok()) return req.Err();
    c.Rapporter("import", 1, 4, "");
    auto jeu = LireSourceRequete(req.Valeur());
    if (!jeu.Ok()) return jeu.Err();
    Statut st = c.Annule();
    if (!st.Ok()) return st.Err();
    c.Rapporter("preparation", 2, 4, "partage et normalisation");
    Apprentissage a;
    a.m_req = req.Valeur();
    a.m_jeu = std::move(jeu.Valeur());
    auto partage = CreerPartage(a.m_jeu, a.m_req.m_methode, a.m_req.m_ratio, a.m_req.m_graine, a.m_req.m_inverse);
    if (!partage.Ok()) return partage.Err();
    a.m_partage = std::move(partage.Valeur());
    st = c.Ecrire("partage.json", a.m_partage.VersJson().Ecrire());   // partage persiste pour les operations pas a pas
    if (!st.Ok()) return st.Err();
    auto norm = NormaliserRequete(a.m_req, a.m_jeu, a.m_partage);
    if (!norm.Ok()) return norm.Err();
    a.m_normalisation = norm.Valeur();
    auto x = AppliquerNormalisation(a.m_normalisation, a.m_jeu, a.m_partage.m_apprentissage);
    if (!x.Ok()) return x.Err();
    a.m_x = x.Valeur();
    for (int i : a.m_partage.m_apprentissage) a.m_labels.push_back(a.m_jeu.m_labels[(size_t)i]);
    return a;
}

ConfigApprentissage ConfigDepuisRequete(const RequeteExperience& r) {
    ConfigApprentissage cfg;
    cfg.m_algorithme = r.m_algorithme;
    cfg.m_normalisation = r.m_normalisation;
    cfg.m_fichierLim = r.m_fichierLim;
    cfg.m_bas = r.m_bas;
    cfg.m_haut = r.m_haut;
    cfg.m_graine = r.m_graine;
    return cfg;
}

// ---------------------------------------------------------------- validation et balayage

struct ParametresValidation {
    int m_k = 10;
    ModePlis m_mode = ModePlis::Melange;
    bool m_stratifie = false;
    std::uint64_t m_graine = 1;
    std::string m_perimetre = "apprentissage";
};

Resultat<ParametresValidation> LireValidation(const ValeurJson& j) {
    ParametresValidation p;
    const ValeurJson& v = j.Membre("validation");
    if (v.EstNul()) return p;
    p.m_k = (int)Ent(v, "k", 10);
    const std::string mode = v.Contient("mode") ? v.Membre("mode").Texte() : "melange";
    if (mode == "ordre_fichier") p.m_mode = ModePlis::OrdreFichier;
    else if (mode != "melange") return Proto("validation.mode : melange ou ordre_fichier");
    p.m_stratifie = v.Membre("stratifie").Booleen();
    p.m_graine = (std::uint64_t)Ent(v, "graine", 1);
    if (v.Contient("perimetre")) p.m_perimetre = v.Membre("perimetre").Texte();
    if (p.m_perimetre != "apprentissage" && p.m_perimetre != "tout")
        return Proto("validation.perimetre : apprentissage ou tout");
    return p;
}

struct Etude {
    RequeteExperience m_req;
    JeuDeDonnees m_jeu;
    Partage m_partage;
    ParametresValidation m_params;
    PlanPlis m_plan;
};

Resultat<Etude> PreparerEtude(const Contexte& c) {
    auto req = RequeteSource(c);
    if (!req.Ok()) return req.Err();
    auto params = LireValidation(c.m_requete);
    if (!params.Ok()) return params.Err();
    c.Rapporter("import", 1, 4, "");
    auto jeu = LireSourceRequete(req.Valeur());
    if (!jeu.Ok()) return jeu.Err();
    Statut st = c.Annule();
    if (!st.Ok()) return st.Err();
    c.Rapporter("plis", 2, 4, "partage et plis");
    Etude e;
    e.m_req = req.Valeur();
    e.m_jeu = std::move(jeu.Valeur());
    e.m_params = params.Valeur();
    auto partage = CreerPartage(e.m_jeu, e.m_req.m_methode, e.m_req.m_ratio, e.m_req.m_graine, e.m_req.m_inverse);
    if (!partage.Ok()) return partage.Err();
    e.m_partage = std::move(partage.Valeur());
    std::vector<int> perimetre = e.m_partage.m_apprentissage;      // le test final n'y entre jamais
    if (e.m_params.m_perimetre == "tout") {
        perimetre.resize((size_t)e.m_jeu.m_nbLignes);
        for (int i = 0; i < e.m_jeu.m_nbLignes; ++i) perimetre[(size_t)i] = i;
    }
    auto plan = CreerPlis(e.m_jeu, perimetre, e.m_params.m_k, e.m_params.m_mode, e.m_params.m_stratifie,
                          e.m_params.m_graine);
    if (!plan.Ok()) return plan.Err();
    e.m_plan = std::move(plan.Valeur());
    return e;
}

RapporteurProgression RapporteurValidation(const Contexte& c) {
    return [&c](const std::string& phase, long i, long n) { c.Rapporter(phase, i, n, ""); };
}

void AjouterProvenance(ValeurJson& j, const Contexte& c, const Etude& e) {
    j["request_id"] = c.Id();
    j["operation"] = c.m_requete.Membre("operation").Texte();
    j["requete"] = c.m_requete;
    j["perimetre"] = e.m_params.m_perimetre;
    j["hash_jeu"] = e.m_jeu.HashCanonique();
    j["hash_partage"] = e.m_partage.Hash();
}

Statut OperationValidation(const Contexte& c) {
    auto e = PreparerEtude(c);
    if (!e.Ok()) return e.Err();
    Statut st = c.Annule();
    if (!st.Ok()) return st;
    c.Rapporter("validation", 3, 4, "validation croisee");
    auto rapport = ValiderCroise(e.Valeur().m_jeu, e.Valeur().m_plan, ConfigDepuisRequete(e.Valeur().m_req),
                                 &c.m_annulation, RapporteurValidation(c));
    if (!rapport.Ok()) return rapport.Err();
    if (!(st = c.Annule()).Ok()) return st;
    c.Rapporter("ecriture", 4, 4, "");
    if (!(st = c.Ecrire("plis.json", e.Valeur().m_plan.VersJson().Ecrire())).Ok()) return st;
    ValeurJson j = rapport.Valeur().VersJson();
    j["schema"] = "decid.validation/1";
    AjouterProvenance(j, c, e.Valeur());
    return c.Ecrire("validation.json", j.Ecrire());
}

Resultat<std::vector<double>> LireValeursBalayage(const ValeurJson& b) {
    std::vector<double> valeurs;
    if (b.Contient("valeurs")) {
        const ValeurJson& t = b.Membre("valeurs");
        if (t.GetType() != ValeurJson::Type::Tableau) return Proto("balayage.valeurs : tableau attendu");
        for (const auto& v : t.Elements()) {
            if (!EstNombre(v)) return Proto("balayage.valeurs : nombres attendus");
            valeurs.push_back(v.Nombre());
        }
    } else if (b.Contient("log")) {
        const ValeurJson& l = b.Membre("log");
        valeurs = ValeursLogarithmiques(Nb(l, "debut", 0), Nb(l, "fin", 0), (int)Ent(l, "par_decade", 1));
    }
    if (valeurs.empty()) return Proto("balayage : \"valeurs\" ou \"log\" {debut, fin, par_decade} valides requis");
    return valeurs;
}

Statut EcrirePointsFnrFpr(const Contexte& c, const RapportBalayage& rapport, int classePositive,
                          const std::string& nomClasse) {
    ValeurJson j = ValeurJson::Objet();
    j["schema"] = "decid.points-fnr-fpr/1";
    j["avertissement"] = "ce n'est pas une courbe ROC : un point par modele (par valeur du parametre), "
                         "abscisse FNR %, ordonnee 100 - FPR %, aucune AUC";
    j["parametre"] = rapport.m_parametre;
    j["classe_positive"] = classePositive;
    j["nom_classe_positive"] = nomClasse;
    ValeurJson points = ValeurJson::Tableau();
    for (const auto& p : PointsFnrFpr(rapport, classePositive)) {
        ValeurJson o = ValeurJson::Objet();
        o["valeur_parametre"] = p.m_valeurParametre;
        o["fnr_pourcent"] = p.m_fnrPourcent;
        o["cent_moins_fpr_pourcent"] = p.m_cent_moins_fprPourcent;
        points.Ajouter(o);
    }
    j["points"] = points;
    return c.Ecrire("points_fnr_fpr.json", j.Ecrire());
}

Statut OperationBalayage(const Contexte& c) {
    const ValeurJson& b = c.m_requete.Membre("balayage");
    const std::string parametre = b.Membre("parametre").Texte();
    if (parametre.empty()) return Proto("balayage.parametre manquant");
    auto valeurs = LireValeursBalayage(b);
    if (!valeurs.Ok()) return valeurs.Err();
    const int positive = (int)Ent(c.m_requete, "classe_positive", 0);
    auto e = PreparerEtude(c);
    if (!e.Ok()) return e.Err();
    if (positive < 0 || positive >= e.Valeur().m_jeu.NbClasses()) return Proto("classe_positive hors plage");
    Statut st = c.Annule();
    if (!st.Ok()) return st;
    c.Rapporter("balayage", 3, 4, parametre);
    auto rapport = Balayer(e.Valeur().m_jeu, e.Valeur().m_plan, ConfigDepuisRequete(e.Valeur().m_req), parametre,
                           valeurs.Valeur(), &c.m_annulation, RapporteurValidation(c));
    if (!rapport.Ok()) return rapport.Err();
    if (!(st = c.Annule()).Ok()) return st;
    c.Rapporter("ecriture", 4, 4, "");
    ValeurJson j = rapport.Valeur().VersJson();
    j["schema"] = "decid.balayage/1";
    j["classe_positive"] = positive;
    AjouterProvenance(j, c, e.Valeur());
    if (!(st = c.Ecrire("balayage.json", j.Ecrire())).Ok()) return st;
    return EcrirePointsFnrFpr(c, rapport.Valeur(), positive, e.Valeur().m_jeu.m_nomsClasses[(size_t)positive]);
}

// ---------------------------------------------------------------- operations pas a pas

CollecteurTrace NouveauCollecteur(const ValeurJson& requete) {
    const ValeurJson& t = requete.Membre("trace");
    return CollecteurTrace((std::size_t)Ent(t, "max_etats", (long long)TRACE_MAX_ETATS),
                           (std::size_t)Ent(t, "max_octets", (long long)TRACE_MAX_OCTETS));
}

// Ajout compatible avec decid.trace/1 : si un etat porte des affectations (k-means), elles sont concatenees dans
// trace_affectations.i32 (int32 little-endian) et chaque etat recoit "decalage_affectations" et "taille_affectations"
// (comptes en int32) ; les champs existants ne changent pas.
Statut EcrireTrace(const Contexte& c, const CollecteurTrace& collecteur) {
    std::vector<float> flux;
    std::vector<std::int32_t> affectations;
    bool avecAffectations = false;
    for (const EtatTrace& e : collecteur.Etats()) avecAffectations = avecAffectations || !e.m_affectations.empty();
    ValeurJson etats = ValeurJson::Tableau();
    for (const EtatTrace& e : collecteur.Etats()) {
        ValeurJson o = ValeurJson::Objet();
        o["iteration"] = e.m_iteration;
        o["phase"] = e.m_phase;
        ValeurJson g = ValeurJson::Objet();
        for (const auto& kv : e.m_grandeurs) g[kv.first] = kv.second;
        o["grandeurs"] = g;
        o["decalage"] = (long long)flux.size();
        o["taille"] = (long long)e.m_vecteur.size();
        flux.insert(flux.end(), e.m_vecteur.begin(), e.m_vecteur.end());
        if (avecAffectations) {
            o["decalage_affectations"] = (long long)affectations.size();
            o["taille_affectations"] = (long long)e.m_affectations.size();
            affectations.insert(affectations.end(), e.m_affectations.begin(), e.m_affectations.end());
        }
        etats.Ajouter(o);
    }
    ValeurJson j = ValeurJson::Objet();
    j["schema"] = "decid.trace/1";
    j["operation"] = c.m_requete.Membre("operation").Texte();
    j["pas"] = collecteur.Pas();
    j["recus"] = collecteur.VersJson().Membre("recus");
    j["unite_decalage"] = "float32 (index dans trace.f32, float32 little-endian concatenes)";
    if (avecAffectations)
        j["fichier_affectations"] = "trace_affectations.i32 (int32 little-endian, index des lignes d'apprentissage)";
    j["etats"] = etats;
    Statut st = c.Ecrire("trace.json", j.Ecrire());
    if (!st.Ok()) return st;
    if (avecAffectations &&
        !(st = c.EcrireOctets("trace_affectations.i32", affectations.data(), affectations.size() * sizeof(std::int32_t))).Ok())
        return st;
    return c.EcrireOctets("trace.f32", flux.data(), flux.size() * sizeof(float));
}

ValeurJson EtatFinalDeBase(const Contexte& c, const Apprentissage& a) {
    ValeurJson j = EnTeteSortie(c, "decid.etat-final/1");
    j["requete"] = c.m_requete;
    j["hash_jeu"] = a.m_jeu.HashCanonique();
    j["hash_partage"] = a.m_partage.Hash();
    j["hash_normalisation"] = a.m_normalisation.Hash();
    j["lignes_apprentissage"] = (long long)a.m_partage.m_apprentissage.size();
    j["etiquette"] = "implementation pedagogique Decid";
    return j;
}

Statut ControlerClassePositive(const ValeurJson& j, const JeuDeDonnees& jeu, int& positive) {
    positive = (int)Ent(j, "classe_positive", 0);
    if (positive < 0 || positive >= jeu.NbClasses()) return Proto("classe_positive hors plage");
    return Succes();
}

std::vector<int> LabelsBinaires(const std::vector<int>& labels, int positive, int valeurNegative) {
    std::vector<int> y;
    for (int l : labels) y.push_back(l == positive ? 1 : valeurNegative);
    return y;
}

Statut OperationAdaBoost(const Contexte& c) {
    const ValeurJson& p = c.m_requete.Membre("adaboost");
    const std::string faible = p.Contient("faible") ? p.Membre("faible").Texte() : "seuil";
    if (faible != "seuil" && faible != "hyperplan") return Proto("adaboost.faible : seuil ou hyperplan");
    const long iterations = (long)Ent(p, "iterations", 50);
    if (iterations < 1 || iterations > 100000) return Proto("adaboost.iterations hors plage (1..100000)");
    auto a = PreparerApprentissage(c);
    if (!a.Ok()) return a.Err();
    int positive = 0;
    Statut st = ControlerClassePositive(p, a.Valeur().m_jeu, positive);
    if (!st.Ok()) return st;
    AdaBoostPasAPas ada;
    st = ada.Initialiser(a.Valeur().m_x, LabelsBinaires(a.Valeur().m_labels, positive, -1),
                         faible == "seuil" ? TypeFaible::Seuil : TypeFaible::Hyperplan);
    if (!st.Ok()) return st;
    CollecteurTrace collecteur = NouveauCollecteur(c.m_requete);
    c.Rapporter("iteration", 0, iterations, "");
    for (long t = 1; t <= iterations; ++t) {
        if (!(st = c.Annule()).Ok()) return st;
        auto suite = ada.Iterer(&collecteur);
        if (!suite.Ok()) return suite.Err();
        if (!suite.Valeur()) break;
        c.Rapporter("iteration", t, iterations, "");
    }
    collecteur.Terminer();
    if (!(st = c.Annule()).Ok()) return st;
    c.Rapporter("ecriture", 4, 4, "");
    if (!(st = EcrireTrace(c, collecteur)).Ok()) return st;
    ValeurJson j = EtatFinalDeBase(c, a.Valeur());
    j["classe_positive"] = positive;
    j["adaboost"] = ada.VersJson();
    j["iterations_demandees"] = iterations;
    j["iterations_effectuees"] = (long long)ada.Iterations().size();
    j["raison_arret"] = ada.Arret().empty() ? std::string("iterations_atteintes") : ada.Arret();
    return c.Ecrire("etat_final.json", j.Ecrire());
}

Statut OperationPerceptron(const Contexte& c) {
    const ValeurJson& p = c.m_requete.Membre("perceptron");
    ParametresPerceptron params;
    params.m_eta = Nb(p, "eta", 0.1);
    params.m_epoquesMax = (int)Ent(p, "epoques_max", 100);
    params.m_melanger = p.Contient("melanger") ? p.Membre("melanger").Booleen() : true;
    params.m_graine = (std::uint64_t)Ent(p, "graine", 1);
    if (!(params.m_eta > 0) || params.m_epoquesMax < 1) return Proto("perceptron : eta > 0 et epoques_max >= 1 requis");
    auto a = PreparerApprentissage(c);
    if (!a.Ok()) return a.Err();
    int positive = 0;
    Statut st = ControlerClassePositive(p, a.Valeur().m_jeu, positive);
    if (!st.Ok()) return st;
    CollecteurTrace collecteur = NouveauCollecteur(c.m_requete);
    c.Rapporter("apprentissage", 3, 4, "perceptron");
    auto r = EntrainerPerceptron(a.Valeur().m_x, LabelsBinaires(a.Valeur().m_labels, positive, 0), params, &collecteur);
    if (!r.Ok()) return r.Err();
    collecteur.Terminer();
    if (!(st = c.Annule()).Ok()) return st;
    c.Rapporter("ecriture", 4, 4, "");
    if (!(st = EcrireTrace(c, collecteur)).Ok()) return st;
    ValeurJson j = EtatFinalDeBase(c, a.Valeur());
    j["classe_positive"] = positive;
    j["poids"] = TableauNombres(r.Valeur().m_poids);
    j["biais"] = r.Valeur().m_biais;
    j["epoques"] = r.Valeur().m_epoques;
    j["converge"] = r.Valeur().m_converge;
    ValeurJson erreurs = ValeurJson::Tableau();
    for (int e : r.Valeur().m_erreursParEpoque) erreurs.Ajouter(e);
    j["erreurs_par_epoque"] = erreurs;
    j["raison_arret"] = r.Valeur().m_converge ? "convergence (epoque sans erreur)" : "epoques_max atteint";
    return c.Ecrire("etat_final.json", j.Ecrire());
}

Statut LireInitKmeans(const ValeurJson& p, InitKmeans& init) {
    const std::string nom = p.Contient("init") ? p.Membre("init").Texte() : "kmeans++";
    if (nom == "kmeans++") init = InitKmeans::KmeansPlusPlus;
    else if (nom == "aleatoire") init = InitKmeans::Aleatoire;
    else return Proto("kmeans.init : kmeans++ ou aleatoire");
    return Succes();
}

ValeurJson CentresVersJson(const cv::Mat& centres) {
    ValeurJson t = ValeurJson::Tableau();
    for (int i = 0; i < centres.rows; ++i) {
        ValeurJson ligne = ValeurJson::Tableau();
        for (int a = 0; a < centres.cols; ++a)
            ligne.Ajouter(centres.depth() == CV_64F ? centres.at<double>(i, a) : (double)centres.at<float>(i, a));
        t.Ajouter(ligne);
    }
    return t;
}

Statut OperationKmeans(const Contexte& c) {
    const ValeurJson& p = c.m_requete.Membre("kmeans");
    ParametresKmeans params;
    params.m_k = (int)Ent(p, "k", 3);
    params.m_graine = (std::uint64_t)Ent(p, "graine", 1);
    params.m_iterMax = (int)Ent(p, "iter_max", 100);
    Statut st = LireInitKmeans(p, params.m_init);
    if (!st.Ok()) return st;
    if (params.m_k < 1 || params.m_iterMax < 1) return Proto("kmeans : k >= 1 et iter_max >= 1 requis");
    auto a = PreparerApprentissage(c);
    if (!a.Ok()) return a.Err();
    CollecteurTrace collecteur = NouveauCollecteur(c.m_requete);
    c.Rapporter("apprentissage", 3, 4, "kmeans");
    auto r = Kmeans(a.Valeur().m_x, params, &collecteur);
    if (!r.Ok()) return r.Err();
    collecteur.Terminer();
    if (!(st = c.Annule()).Ok()) return st;
    c.Rapporter("ecriture", 4, 4, "");
    if (!(st = EcrireTrace(c, collecteur)).Ok()) return st;
    ValeurJson j = EtatFinalDeBase(c, a.Valeur());
    j["k"] = params.m_k;
    j["centres"] = CentresVersJson(r.Valeur().m_centres);
    j["iterations"] = r.Valeur().m_iterations;
    j["J_final"] = r.Valeur().m_jParPhase.empty() ? ValeurJson() : ValeurJson(r.Valeur().m_jParPhase.back());
    j["J_par_phase"] = TableauNombres(r.Valeur().m_jParPhase);
    j["raison_arret"] = r.Valeur().m_aucunChangement ? "aucun changement d'affectation" : "iter_max atteint";
    j["lignes"] = TableauEntiers(a.Valeur().m_partage.m_apprentissage);
    j["affectations"] = TableauEntiers(r.Valeur().m_affectations);
    return c.Ecrire("etat_final.json", j.Ecrire());
}

// ---- MLP et EM pas a pas : meme conventions que les autres traces (trace.json, trace.f32, etat_final.json)

// Suivi optionnel de l'exactitude sur le test du partage ("suivi_validation": "test") : affichage seulement,
// jamais utilise pour choisir un parametre ni arreter l'apprentissage.
Statut ChargerSuivi(const Contexte& c, const Apprentissage& a, cv::Mat& x, std::vector<int>& labels) {
    const std::string mode = c.m_requete.Contient("suivi_validation") ? c.m_requete.Membre("suivi_validation").Texte() : "aucun";
    if (mode == "aucun") return Succes();
    if (mode != "test") return Proto("suivi_validation : aucun ou test");
    if (a.m_partage.m_test.empty()) return Proto("suivi_validation : le test du partage est vide");
    auto xv = AppliquerNormalisation(a.m_normalisation, a.m_jeu, a.m_partage.m_test);
    if (!xv.Ok()) return xv.Err();
    x = xv.Valeur();
    for (int i : a.m_partage.m_test) labels.push_back(a.m_jeu.m_labels[(size_t)i]);
    return Succes();
}

Statut ControlerFamille(const Apprentissage& a, Famille attendue) {
    if (a.m_req.m_algorithme.m_famille == attendue) return Succes();
    return Proto(std::string("algorithme.famille doit etre \"") + NomFamille(attendue) + "\" pour cette operation");
}

// Ancien chemin OpenCV (requete : "reseau":{"moteur":"opencv"}) : une epoque par appel a ANN_MLP::train.
Statut OperationMlpOpenCv(const Contexte& c) {
    auto a = PreparerApprentissage(c);
    if (!a.Ok()) return a.Err();
    Statut st = ControlerFamille(a.Valeur(), Famille::Mlp);
    if (!st.Ok()) return st;
    cv::Mat xSuivi;
    std::vector<int> labelsSuivi;
    if (!(st = ChargerSuivi(c, a.Valeur(), xSuivi, labelsSuivi)).Ok()) return st;
    CollecteurTrace collecteur = NouveauCollecteur(c.m_requete);
    c.Rapporter("apprentissage", 3, 4, "mlp");
    const RequeteExperience& req = a.Valeur().m_req;
    auto m = EntrainerMlpPasAPas(a.Valeur().m_x, a.Valeur().m_labels, a.Valeur().m_jeu.NbClasses(), req.m_algorithme.m_mlp,
                                 xSuivi, labelsSuivi, req.m_graine, &collecteur);
    if (!m.Ok()) return m.Err();
    collecteur.Terminer();
    if (!(st = c.Annule()).Ok()) return st;
    c.Rapporter("ecriture", 4, 4, "");
    if (!(st = EcrireTrace(c, collecteur)).Ok()) return st;
    ValeurJson j = EtatFinalDeBase(c, a.Valeur());
    j["parametres_demandes"] = m.Valeur().m_parametresDemandes;
    j["parametres_effectifs"] = m.Valeur().m_parametresEffectifs;
    j["epoques_effectuees"] = m.Valeur().m_parametresEffectifs.Membre("iterations_effectuees");
    j["suivi_validation"] = xSuivi.empty() ? "aucun" : "test (affichage seulement)";
    j["inspection"] = InspecterModele(m.Valeur());
    return c.Ecrire("etat_final.json", j.Ecrire());
}

// ---------------------------------------------------------------- reseau pedagogique Decid (mlp_pas_a_pas par defaut)
// Bloc optionnel "reseau" : {"moteur":"decid"|"opencv", "taille_lot", "l2", "graine"} ; le bloc "algorithme"
// (famille mlp) fournit couches_cachees, activation, methode, eta, moment, iterations_max et epsilon.

std::vector<double> VersDoubles(const cv::Mat& x) {
    std::vector<double> v((size_t)x.rows * (size_t)x.cols);
    for (int i = 0; i < x.rows; ++i)
        for (int j = 0; j < x.cols; ++j) v[(size_t)i * (size_t)x.cols + (size_t)j] = x.at<float>(i, j);
    return v;
}

ParametresReseau ParametresReseauDepuis(const Contexte& c, const RequeteExperience& req) {
    const ParametresMlp& m = req.m_algorithme.m_mlp;
    const ValeurJson& r = c.m_requete.Membre("reseau");
    ParametresReseau p;
    p.m_couchesCachees = m.m_couchesCachees;
    p.m_activation = m.m_activation;
    p.m_methode = m.m_methode;
    p.m_eta = m.m_eta;
    p.m_moment = m.m_moment;
    p.m_epoques = m.m_iterationsMax;
    p.m_epsilon = m.m_epsilon;
    p.m_tailleLot = (int)Ent(r, "taille_lot", 16);
    p.m_l2 = Nb(r, "l2", 0.0);
    p.m_graine = (std::uint64_t)Ent(r, "graine", (long long)req.m_graine);
    return p;
}

EtatTrace EtatReseau(int iteration, const char* phase, const MesuresReseau& m, const Reseau& reseau,
                     const std::vector<double>& xSuivi, const std::vector<int>& ySuivi) {
    EtatTrace e;
    e.m_iteration = iteration;
    e.m_phase = phase;
    e.m_grandeurs = {{"epoque", (double)iteration}, {"perte_apprentissage", m.m_perte},
                     {"exactitude_apprentissage", m.m_exactitude}};
    if (!ySuivi.empty()) {
        const MesuresReseau v = reseau.Mesurer(xSuivi, ySuivi, 0.0);
        e.m_grandeurs.push_back({"exactitude_validation", v.m_exactitude});
        e.m_grandeurs.push_back({"perte_validation", v.m_perte});
    }
    e.m_vecteur = reseau.PoidsConcatenes();
    return e;
}

ValeurJson ParametresReseauJson(const ParametresReseau& p, int effectuees) {
    ValeurJson j = ValeurJson::Objet();
    ValeurJson couches = ValeurJson::Tableau();
    for (int n : p.m_couchesCachees) couches.Ajouter(n);
    j["couches_cachees"] = couches;
    j["activation"] = p.m_activation;
    j["activation_sortie"] = "SOFTMAX";
    j["methode"] = p.m_methode;
    if (p.m_methode == "RETROPROPAGATION") { j["eta"] = p.m_eta; j["moment"] = p.m_moment; j["taille_lot"] = p.m_tailleLot; }
    else j["rprop"] = "iRprop- lot complet : pas initial 0.1, x1.2 / x0.5, bornes [1e-6, 50]";
    j["l2"] = p.m_l2;
    j["graine"] = (unsigned long long)p.m_graine;
    j["epsilon"] = p.m_epsilon;
    j["iterations_demandees"] = p.m_epoques;
    j["iterations_effectuees"] = effectuees;
    j["mode"] = "pas a pas Decid : etat de l'optimiseur conserve d'une epoque a l'autre";
    return j;
}

Statut OperationMlpDecid(const Contexte& c, const Apprentissage& a) {
    const ParametresReseau p = ParametresReseauDepuis(c, a.m_req);
    const int nbEntrees = a.m_x.cols, nbClasses = a.m_jeu.NbClasses();
    Statut st = VerifierParametresReseau(p, nbEntrees, nbClasses);
    if (!st.Ok()) return st;
    cv::Mat xSuiviMat;
    std::vector<int> ySuivi;
    if (!(st = ChargerSuivi(c, a, xSuiviMat, ySuivi)).Ok()) return st;
    const std::vector<double> x = VersDoubles(a.m_x), xSuivi = VersDoubles(xSuiviMat);
    std::vector<int> tailles{nbEntrees};
    tailles.insert(tailles.end(), p.m_couchesCachees.begin(), p.m_couchesCachees.end());
    tailles.push_back(nbClasses);
    Reseau reseau(tailles, p.m_activation, p.m_graine);
    EntraineurReseau entraineur(reseau, p, x, a.m_labels);
    CollecteurTrace collecteur = NouveauCollecteur(c.m_requete);
    collecteur.Recevoir(EtatReseau(0, "initial", reseau.Mesurer(x, a.m_labels, p.m_l2), reseau, xSuivi, ySuivi));
    c.Rapporter("apprentissage", 0, p.m_epoques, "reseau decid");
    double precedente = 0;
    int effectuees = 0;
    for (int epoque = 1; epoque <= p.m_epoques; ++epoque) {
        if (!(st = c.Annule()).Ok()) return st;
        const MesuresReseau m = entraineur.Epoque();
        if (!std::isfinite(m.m_perte))
            return FaireErreur(CodeErreur::Calcul, "apprentissage_reseau",
                               "perte non finie a l'epoque " + std::to_string(epoque) + " : reduire eta");
        collecteur.Recevoir(EtatReseau(epoque, "epoque", m, reseau, xSuivi, ySuivi));
        effectuees = epoque;
        c.Rapporter("apprentissage", epoque, p.m_epoques, "");
        if (p.m_epsilon > 0 && epoque > 1 && std::fabs(precedente - m.m_perte) < p.m_epsilon) break;
        precedente = m.m_perte;
    }
    collecteur.Terminer();
    c.Rapporter("ecriture", 4, 4, "");
    if (!(st = EcrireTrace(c, collecteur)).Ok()) return st;
    if (!(st = c.Ecrire("normalisation.json", a.m_normalisation.VersJson().Ecrire())).Ok()) return st;
    std::vector<double> moyennes((size_t)nbEntrees, 0.0);
    for (int i = 0; i < a.m_x.rows; ++i)
        for (int k = 0; k < nbEntrees; ++k) moyennes[(size_t)k] += a.m_x.at<float>(i, k) / (double)a.m_x.rows;
    ValeurJson j = EtatFinalDeBase(c, a);
    j["implementation"] = "decid";
    j["parametres_effectifs"] = ParametresReseauJson(p, effectuees);
    j["epoques_effectuees"] = effectuees;
    j["suivi_validation"] = ySuivi.empty() ? "aucun" : "test (affichage seulement)";
    j["attributs"] = ListeNoms(a.m_jeu.m_nomsAttributs);
    j["classes"] = ListeNoms(a.m_jeu.m_nomsClasses);
    j["moyennes_apprentissage"] = TableauNombres(moyennes);
    j["inspection"] = reseau.Inspection();
    return c.Ecrire("etat_final.json", j.Ecrire());
}

Statut OperationMlp(const Contexte& c) {
    const ValeurJson& r = c.m_requete.Membre("reseau");
    const std::string moteur = r.Contient("moteur") ? r.Membre("moteur").Texte() : "decid";
    if (moteur == "opencv") return OperationMlpOpenCv(c);
    if (moteur != "decid") return Proto("reseau.moteur : decid ou opencv");
    auto a = PreparerApprentissage(c);
    if (!a.Ok()) return a.Err();
    Statut st = ControlerFamille(a.Valeur(), Famille::Mlp);
    if (!st.Ok()) return st;
    return OperationMlpDecid(c, a.Valeur());
}

// ---------------------------------------------------------------- activations d'un reseau Decid publie
// Requete : {"operation":"activations", "reseau_source":"<tache mlp_pas_a_pas>", "epoque": n (defaut : derniere),
//            "points": [[valeurs brutes], ...], "plan": {"attribut_x", "attribut_y", "largeur", "hauteur",
//            "domaine": {"x":[min,max], "y":[min,max]} (defaut : bornes d'apprentissage +/- 5 %)}}
// Sorties : activations.json (activations par couche de chaque point ; index des cartes) et, avec "plan",
// neurones.f32 : une carte hauteur x largeur par neurone des couches 1..L (ligne 0 = y max, centre des pixels,
// autres attributs fixes a leur moyenne d'apprentissage), dans l'ordre des couches puis des neurones.

struct ReseauPublie {
    Reseau m_reseau;
    Normalisation m_normalisation;
    std::vector<double> m_moyennes;
    int m_epoque = 0;
};

Resultat<ReseauPublie> ChargerReseauPublie(const fs::path& tache, long long epoqueDemandee) {
    Statut st = VerifierPublication(tache);
    if (!st.Ok()) return st.Err();
    const fs::path r = tache / "resultat";
    auto fin = LireJsonFichier(r / "etat_final.json");
    if (!fin.Ok()) return fin.Err();
    if (fin.Valeur().Membre("implementation").Texte() != "decid")
        return Proto("reseau_source : seule une tache mlp_pas_a_pas du reseau Decid est acceptee");
    auto trace = LireJsonFichier(r / "trace.json");
    if (!trace.Ok()) return trace.Err();
    auto flux = LireFichierBinaire(r / "trace.f32");
    if (!flux.Ok()) return flux.Err();
    auto norm = LireJsonFichier(r / "normalisation.json");
    if (!norm.Ok()) return norm.Err();
    auto normalisation = NormalisationDepuisJson(norm.Valeur());
    if (!normalisation.Ok()) return normalisation.Err();
    const ValeurJson* choisi = nullptr;
    for (const auto& e : trace.Valeur().Membre("etats").Elements())
        if (epoqueDemandee < 0 || e.Membre("iteration").Entier() == epoqueDemandee) choisi = &e;
    if (!choisi)
        return Proto("epoque " + std::to_string(epoqueDemandee) + " absente de la trace (un etat conserve sur " +
                     std::to_string(trace.Valeur().Membre("pas").Entier()) + ")");
    const ValeurJson& ins = fin.Valeur().Membre("inspection");
    std::vector<int> tailles;
    for (const auto& t : ins.Membre("tailles").Elements()) tailles.push_back((int)t.Entier());
    const auto decalage = (std::size_t)choisi->Membre("decalage").Entier();
    const auto taille = (std::size_t)choisi->Membre("taille").Entier();
    if ((decalage + taille) * sizeof(float) > flux.Valeur().size())
        return FaireErreur(CodeErreur::ModeleIncompatible, "reseau_source", "trace.f32 plus courte que l'index de trace.json");
    std::vector<float> poids(taille);
    std::memcpy(poids.data(), flux.Valeur().data() + decalage * sizeof(float), taille * sizeof(float));
    auto reseau = Reseau::DepuisPoids(tailles, ins.Membre("activation_cachee").Texte(), poids.data(), poids.size());
    if (!reseau.Ok()) return reseau.Err();
    ReseauPublie p{std::move(reseau.Valeur()), normalisation.Valeur(), {}, (int)choisi->Membre("iteration").Entier()};
    for (const auto& v : fin.Valeur().Membre("moyennes_apprentissage").Elements()) p.m_moyennes.push_back(v.Nombre());
    return p;
}

ValeurJson ActivationsJson(const std::vector<std::vector<double>>& a) {
    ValeurJson couches = ValeurJson::Tableau();
    for (const auto& couche : a) couches.Ajouter(TableauNombres(couche));
    return couches;
}

// Coordonnee brute -> espace du modele pour un seul attribut (meme formule que TransformerVecteur)
double NormaliserAttribut(const Normalisation& n, int k, double v, int d) {
    std::vector<double> brut = n.m_min.empty() ? std::vector<double>((size_t)d, 0.0) : n.m_min;
    brut[(size_t)k] = v;
    return TransformerVecteur(n, brut)[(size_t)k];
}

Statut DomainePlan(const ValeurJson& plan, const Normalisation& n, int ax, int ay, double& xmin, double& xmax,
                   double& ymin, double& ymax) {
    const ValeurJson& dom = plan.Membre("domaine");
    if (dom.Contient("x") && dom.Contient("y")) {
        if (dom.Membre("x").Elements().size() != 2 || dom.Membre("y").Elements().size() != 2)
            return Proto("plan.domaine : x et y attendus sous la forme [min, max]");
        xmin = dom.Membre("x").Elements()[0].Nombre(); xmax = dom.Membre("x").Elements()[1].Nombre();
        ymin = dom.Membre("y").Elements()[0].Nombre(); ymax = dom.Membre("y").Elements()[1].Nombre();
    } else if (n.m_methode == "minmax") {
        auto marge = [](double a0, double b0, double& a1, double& b1) { const double m = (b0 - a0) * 0.05; a1 = a0 - m; b1 = b0 + m; };
        marge(n.m_min[(size_t)ax], n.m_max[(size_t)ax], xmin, xmax);
        marge(n.m_min[(size_t)ay], n.m_max[(size_t)ay], ymin, ymax);
    } else {
        return Proto("plan : domaine explicite requis quand aucune normalisation min-max n'est publiee");
    }
    if (!(xmax > xmin) || !(ymax > ymin)) return Proto("plan : domaine vide");
    return Succes();
}

Statut EcrirePlanActivations(const Contexte& c, const ReseauPublie& pub, const ValeurJson& plan, ValeurJson& j) {
    const int d = pub.m_reseau.NbEntrees();
    const int ax = (int)Ent(plan, "attribut_x", 0), ay = (int)Ent(plan, "attribut_y", 1);
    const int l = (int)Ent(plan, "largeur", 48), h = (int)Ent(plan, "hauteur", 48);
    if (ax < 0 || ay < 0 || ax >= d || ay >= d || ax == ay) return Proto("plan : attributs hors plage ou identiques");
    if (l < 2 || h < 2 || l > 256 || h > 256) return Proto("plan : largeur et hauteur entre 2 et 256");
    double xmin = 0, xmax = 0, ymin = 0, ymax = 0;
    Statut st = DomainePlan(plan, pub.m_normalisation, ax, ay, xmin, xmax, ymin, ymax);
    if (!st.Ok()) return st;
    if ((int)pub.m_moyennes.size() != d)
        return FaireErreur(CodeErreur::ModeleIncompatible, "reseau_source", "moyennes d'apprentissage absentes");
    const std::vector<int>& tailles = pub.m_reseau.Tailles();
    std::size_t nbNeurones = 0;
    for (size_t k = 1; k < tailles.size(); ++k) nbNeurones += (size_t)tailles[k];
    const std::size_t pixels = (size_t)l * (size_t)h;
    std::vector<float> cartes(nbNeurones * pixels);
    std::vector<double> x = pub.m_moyennes;
    std::vector<std::vector<double>> a;
    for (int py = 0; py < h; ++py) {
        if (!(st = c.Annule()).Ok()) return st;
        x[(size_t)ay] = NormaliserAttribut(pub.m_normalisation, ay, ymax - (py + 0.5) * (ymax - ymin) / h, d);
        for (int px = 0; px < l; ++px) {
            x[(size_t)ax] = NormaliserAttribut(pub.m_normalisation, ax, xmin + (px + 0.5) * (xmax - xmin) / l, d);
            pub.m_reseau.Propager(x.data(), a);
            std::size_t n = 0;
            for (size_t k = 1; k < a.size(); ++k)
                for (double v : a[k]) cartes[n++ * pixels + (size_t)py * (size_t)l + (size_t)px] = (float)v;
        }
    }
    ValeurJson index = ValeurJson::Tableau();
    std::size_t n = 0;
    for (size_t k = 1; k < tailles.size(); ++k)
        for (int i = 0; i < tailles[k]; ++i, ++n) {
            ValeurJson e = ValeurJson::Objet();
            e["couche"] = (long long)k;
            e["neurone"] = i;
            e["decalage"] = (unsigned long long)(n * pixels);
            e["taille"] = (unsigned long long)pixels;
            index.Ajouter(e);
        }
    ValeurJson pj = ValeurJson::Objet();
    pj["attribut_x"] = ax; pj["attribut_y"] = ay; pj["largeur"] = l; pj["hauteur"] = h;
    pj["x"] = TableauNombres({xmin, xmax});
    pj["y"] = TableauNombres({ymin, ymax});
    pj["valeurs_fixes"] = "moyenne d'apprentissage (espace du modele)";
    pj["convention"] = "neurones.f32 : float32 little-endian ; ligne 0 = y max ; centre des pixels ; sortie = probabilites softmax";
    pj["cartes"] = index;
    j["plan"] = pj;
    return c.EcrireOctets("neurones.f32", cartes.data(), cartes.size() * sizeof(float));
}

Statut OperationActivations(const Contexte& c) {
    const std::string source = c.m_requete.Membre("reseau_source").Texte();
    if (source.empty()) return Proto("champ \"reseau_source\" (tache mlp_pas_a_pas publiee) manquant");
    auto chemin = ResoudreChemin(source, c.m_dossier);
    if (!chemin.Ok()) return chemin.Err();
    auto rp = ChargerReseauPublie(chemin.Valeur(), Ent(c.m_requete, "epoque", -1));
    if (!rp.Ok()) return rp.Err();
    const ReseauPublie& pub = rp.Valeur();
    const int d = pub.m_reseau.NbEntrees();
    ValeurJson j = EnTeteSortie(c, "decid.activations/1");
    j["reseau_source"] = source;
    j["epoque"] = pub.m_epoque;
    j["convention"] = "activations[0] = entree dans l'espace du modele ; activations[c] = f(poids_c^T [activations[c-1]; 1]) ; "
                      "derniere couche = softmax";
    ValeurJson points = ValeurJson::Tableau();
    std::vector<std::vector<double>> a;
    for (const auto& p : c.m_requete.Membre("points").Elements()) {
        std::vector<double> brut;
        for (const auto& v : p.Elements()) brut.push_back(v.Nombre());
        if ((int)brut.size() != d) return Proto("chaque point doit avoir " + std::to_string(d) + " valeurs brutes");
        const std::vector<double> x = TransformerVecteur(pub.m_normalisation, brut);
        pub.m_reseau.Propager(x.data(), a);
        ValeurJson o = ValeurJson::Objet();
        o["brut"] = TableauNombres(brut);
        o["activations"] = ActivationsJson(a);
        o["classe"] = (long long)(std::max_element(a.back().begin(), a.back().end()) - a.back().begin());
        points.Ajouter(o);
    }
    j["points"] = points;
    const ValeurJson& plan = c.m_requete.Membre("plan");
    if (!plan.EstNul()) {
        Statut st = EcrirePlanActivations(c, pub, plan, j);
        if (!st.Ok()) return st;
    }
    return c.Ecrire("activations.json", j.Ecrire());
}

// Exactitude sur les lignes de test du partage : classe = correspondance du cluster de la ligne ; une ligne dans un
// cluster non associe compte comme une erreur. Nul (avec raison) si le test est vide.
ValeurJson ExactitudeTestEm(const Apprentissage& a, const std::vector<int>& clustersTout, const std::vector<int>& correspondance) {
    ValeurJson j = ValeurJson::Objet();
    const auto& test = a.m_partage.m_test;
    j["lignes"] = (long long)test.size();
    if (test.empty()) { j["valeur"] = ValeurJson(); j["raison"] = "test vide"; return j; }
    long long justes = 0, nonAssocies = 0;
    for (int i : test) {
        const int k = clustersTout[(size_t)i];
        const int classe = k >= 0 && (size_t)k < correspondance.size() ? correspondance[(size_t)k] : -1;
        nonAssocies += classe < 0 ? 1 : 0;
        justes += classe == a.m_jeu.m_labels[(size_t)i] ? 1 : 0;
    }
    j["valeur"] = (double)justes / (double)test.size();
    j["non_associees"] = nonAssocies;
    j["regle"] = "classe = correspondance du cluster ; cluster non associe = erreur";
    return j;
}

Statut OperationEm(const Contexte& c) {
    auto a = PreparerApprentissage(c);
    if (!a.Ok()) return a.Err();
    Statut st = ControlerFamille(a.Valeur(), Famille::Em);
    if (!st.Ok()) return st;
    CollecteurTrace collecteur = NouveauCollecteur(c.m_requete);
    c.Rapporter("apprentissage", 3, 4, "em");
    const RequeteExperience& req = a.Valeur().m_req;
    auto m = EntrainerEmPasAPas(a.Valeur().m_x, a.Valeur().m_labels, a.Valeur().m_jeu.NbClasses(), req.m_algorithme.m_em,
                                req.m_graine, &collecteur);
    if (!m.Ok()) return m.Err();
    collecteur.Terminer();
    if (!(st = c.Annule()).Ok()) return st;
    auto clusters = ClustersEm(m.Valeur(), a.Valeur().m_x);
    if (!clusters.Ok()) return clusters.Err();
    // toutes les lignes du jeu (export « donnees classees par EM » de l'ancien Decid), meme normalisation
    std::vector<int> toutes((size_t)a.Valeur().m_jeu.m_nbLignes);
    for (int i = 0; i < a.Valeur().m_jeu.m_nbLignes; ++i) toutes[(size_t)i] = i;
    auto xTout = AppliquerNormalisation(a.Valeur().m_normalisation, a.Valeur().m_jeu, toutes);
    if (!xTout.Ok()) return xTout.Err();
    auto clustersTout = ClustersEm(m.Valeur(), xTout.Valeur());
    if (!clustersTout.Ok()) return clustersTout.Err();
    c.Rapporter("ecriture", 4, 4, "");
    if (!(st = EcrireTrace(c, collecteur)).Ok()) return st;
    ValeurJson j = EtatFinalDeBase(c, a.Valeur());
    j["parametres_demandes"] = m.Valeur().m_parametresDemandes;
    j["parametres_effectifs"] = m.Valeur().m_parametresEffectifs;
    j["iterations_effectuees"] = m.Valeur().m_parametresEffectifs.Membre("iterations_effectuees");
    j["correspondance_clusters"] = TableauEntiers(m.Valeur().m_correspondanceClusters);
    j["lignes"] = TableauEntiers(a.Valeur().m_partage.m_apprentissage);
    j["clusters"] = TableauEntiers(clusters.Valeur());
    j["clusters_toutes_lignes"] = TableauEntiers(clustersTout.Valeur());   // indice = ligne du jeu
    j["exactitude_test"] = ExactitudeTestEm(a.Valeur(), clustersTout.Valeur(), m.Valeur().m_correspondanceClusters);
    j["inspection"] = InspecterModele(m.Valeur());
    return c.Ecrire("etat_final.json", j.Ecrire());
}

// ---------------------------------------------------------------- experience (delegation a L1)

Statut OperationExperience(const Contexte& c) {
    auto req = RequeteDepuisJson(c.m_requete, c.m_dossier);
    if (!req.Ok()) return req.Err();
    return ExecuterExperience(req.Valeur(), c.m_staging, c.m_annulation, c.m_rapporter);
}

// ---------------------------------------------------------------- generation, regression, segmentation
// (calculs dans Generation.cpp, Regression.cpp et Segmentation.cpp ; ici, seulement l'aiguillage)

Statut OperationGenerer(const Contexte& c) {
    c.Rapporter("generation", 1, 1, "");
    return ExecuterGeneration(c.m_requete, c.m_staging, c.m_annulation);
}

Statut OperationRegression(const Contexte& c) {
    c.Rapporter("regression", 1, 1, "");
    return ExecuterRegression(c.m_requete, c.m_dossier, c.m_staging, c.m_annulation);
}

Statut OperationEchantillonner(const Contexte& c) {
    c.Rapporter("echantillonnage", 1, 1, "");
    return ExecuterEchantillonnage(c.m_requete, c.m_dossier, c.m_staging, c.m_annulation);
}

Statut OperationDecider(const Contexte& c) {
    c.Rapporter("decision", 1, 1, "application du modele publie");
    return ExecuterDecision(c.m_requete, c.m_dossier, c.m_staging, c.m_annulation);
}

Statut OperationSegmenter(const Contexte& c) {
    c.Rapporter("segmentation", 1, 1, "classification des pixels");
    return ExecuterSegmentation(c.m_requete, c.m_dossier, c.m_staging, c.m_annulation);
}

using Operation = Statut (*)(const Contexte&);
struct EntreeOperation { const char* m_nom; Operation m_fonction; };
const EntreeOperation OPERATIONS[] = {
    {"image", &OperationImage},
    {"volume", &OperationVolume},
    {"validation", &OperationValidation},
    {"balayage", &OperationBalayage},
    {"adaboost_pas_a_pas", &OperationAdaBoost},
    {"perceptron", &OperationPerceptron},
    {"kmeans", &OperationKmeans},
    {"mlp_pas_a_pas", &OperationMlp},
    {"em_pas_a_pas", &OperationEm},
    {"activations", &OperationActivations},
    {"generer", &OperationGenerer},
    {"regression", &OperationRegression},
    {"echantillonner_couleurs", &OperationEchantillonner},
    {"segmenter", &OperationSegmenter},
    {"decider", &OperationDecider},
};

Operation TrouverOperation(const std::string& nom) {
    if (nom.empty() || nom == "experience") return &OperationExperience;
    for (const auto& o : OPERATIONS) if (nom == o.m_nom) return o.m_fonction;
    return nullptr;
}

} // namespace

// ---------------------------------------------------------------- API

Resultat<ExperiencePubliee> ChargerExperiencePubliee(const fs::path& dossierTache) {
    Statut publication = VerifierPublication(dossierTache);
    if (!publication.Ok()) return publication.Err();
    const fs::path r = dossierTache / "resultat";
    ExperiencePubliee e;
    auto jeu = ChargerJeu(r / "jeu");
    if (!jeu.Ok()) return jeu.Err();
    e.m_jeu = std::move(jeu.Valeur());
    auto jp = LireJsonFichier(r / "partage.json");
    if (!jp.Ok()) return jp.Err();
    auto partage = PartageDepuisJson(jp.Valeur());
    if (!partage.Ok()) return partage.Err();
    Statut valide = ValiderPartage(partage.Valeur(), e.m_jeu.m_nbLignes);
    if (!valide.Ok()) return valide.Err();
    e.m_partage = std::move(partage.Valeur());
    auto jn = LireJsonFichier(r / "normalisation.json");
    if (!jn.Ok()) return jn.Err();
    auto norm = NormalisationDepuisJson(jn.Valeur());
    if (!norm.Ok()) return norm.Err();
    e.m_normalisation = std::move(norm.Valeur());
    auto manifeste = LireJsonFichier(r / "modele.json");
    if (!manifeste.Ok()) return manifeste.Err();
    Famille famille;
    if (!FamilleDepuisNom(manifeste.Valeur().Membre("famille").Texte(), famille))
        return FaireErreur(CodeErreur::ModeleIncompatible, "experience_source", "famille inconnue dans modele.json");
    const int nbClasses = (int)manifeste.Valeur().Membre("classes").Elements().size();
    if (nbClasses != e.m_jeu.NbClasses())
        return FaireErreur(CodeErreur::ModeleIncompatible, "experience_source", "classes du modele differentes de celles du jeu");
    auto modele = ChargerModele(famille, r / "modele.yml", nbClasses);
    if (!modele.Ok()) return modele.Err();
    e.m_modele = std::move(modele.Valeur());
    e.m_hashModele = Sha256Fichier(r / "modele.yml");
    return e;
}

Resultat<JeuDeDonnees> LireSourceRequete(const RequeteExperience& req) {
    if (req.m_typeSource == "csv") return LireCsv(req.m_source, req.m_contratCsv);
    if (req.m_typeSource == "cmc") return LireCmc(req.m_source, req.m_relocalisations);
    return LireFichiersParRole(req.m_fichiersRoles);
}

Statut ExecuterOperation(const ValeurJson& requete, const fs::path& dossierRequete, const fs::path& staging,
                         const std::atomic<bool>& annulation, const RapporteurEvenement& rapporter) {
    Contexte c{requete, dossierRequete, staging, annulation, rapporter};
    Statut st = c.Annule();
    if (!st.Ok()) return st;
    const std::string nom = requete.Membre("operation").Texte();
    Operation operation = TrouverOperation(nom);
    if (!operation)
        return Proto("operation inconnue : \"" + nom + "\" (experience, image, volume, validation, balayage, "
                     "adaboost_pas_a_pas, perceptron, kmeans, mlp_pas_a_pas, em_pas_a_pas, activations, generer, regression, "
                     "echantillonner_couleurs, segmenter, decider)");
    if (requete.Membre("protocol_version").Entier() != 1) return Proto("version de protocole non prise en charge");
    if (c.Id().empty()) return Proto("request_id manquant");
    std::error_code ec;
    fs::create_directories(staging, ec);
    if (ec) return FaireErreur(CodeErreur::EcritureRefusee, "staging", "dossier de staging non creable", CheminUtf8(staging));
    if (!(st = operation(c)).Ok()) return st;
    if (!(st = c.Annule()).Ok()) return st;
    if (nom.empty() || nom == "experience") return Succes();   // L1 ecrit deja artefacts.json
    c.Rapporter("artefacts", 1, 1, "");
    return EcrireArtefacts(staging);
}

Statut ValiderRequete(const ValeurJson& requete, const fs::path& dossierRequete) {
    const std::string nom = requete.Membre("operation").Texte();
    if (!TrouverOperation(nom))
        return Proto("operation inconnue : \"" + nom + "\" (experience, image, volume, validation, balayage, "
                     "adaboost_pas_a_pas, perceptron, kmeans, mlp_pas_a_pas, em_pas_a_pas, activations, generer, regression, "
                     "echantillonner_couleurs, segmenter, decider)");
    if (requete.Membre("protocol_version").Entier() != 1) return Proto("version de protocole non prise en charge");
    if (requete.Membre("request_id").Texte().empty()) return Proto("request_id manquant");
    if (nom == "activations") {
        const std::string source = requete.Membre("reseau_source").Texte();
        if (source.empty()) return Proto("champ \"reseau_source\" (tache mlp_pas_a_pas publiee) manquant");
        fs::path p = CheminDepuisUtf8(source);
        return VerifierPublication(p.is_relative() ? dossierRequete / p : p);
    }
    if (nom == "generer") {
        auto lu = RequeteGenerationDepuisJson(requete);
        if (!lu.Ok()) return lu.Err();
        return Succes();
    }
    if (nom == "regression") {
        auto lu = RequeteRegressionDepuisJson(requete, dossierRequete);
        if (!lu.Ok()) return lu.Err();
        return Succes();
    }
    if (nom == "echantillonner_couleurs") return ValiderEchantillonnage(requete, dossierRequete);
    if (nom == "segmenter") {
        Statut st = ValiderParametresSegmentation(requete, dossierRequete);
        if (!st.Ok()) return st;
        fs::path p = CheminDepuisUtf8(requete.Membre("experience").Texte());
        return VerifierPublication(p.is_relative() ? dossierRequete / p : p);
    }
    if (nom == "decider") {
        Statut st = ValiderDecision(requete, dossierRequete);
        if (!st.Ok()) return st;
    }
    if (nom == "image" || nom == "volume" || nom == "decider") {
        const std::string source = requete.Membre("experience").Texte();
        if (source.empty()) return Proto("champ \"experience\" (tache publiee) manquant");
        fs::path p = CheminDepuisUtf8(source);
        return VerifierPublication(p.is_relative() ? dossierRequete / p : p);
    }
    // experience, validation, balayage, pas a pas : meme lecture que l'execution (algorithme injecte si absent)
    auto lu = RequeteDepuisJson(AvecAlgorithme(requete), dossierRequete);
    if (!lu.Ok()) return lu.Err();
    return Succes();
}

Statut EcrireArtefacts(const fs::path& staging) {
    std::vector<fs::path> chemins;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(staging, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (!it->is_regular_file()) continue;
        if (it->path().filename() == "artefacts.json" && it->path().parent_path() == staging) continue;
        chemins.push_back(it->path());
    }
    if (ec) return FaireErreur(CodeErreur::Lecture, "artefacts", "staging illisible", CheminUtf8(staging));
    std::sort(chemins.begin(), chemins.end());
    ValeurJson artefacts = ValeurJson::Objet();
    artefacts["schema"] = "decid.artefacts/1";
    ValeurJson liste = ValeurJson::Tableau();
    for (const auto& chemin : chemins) {
        std::uintmax_t taille = 0;
        ValeurJson f = ValeurJson::Objet();
        f["sha256"] = Sha256Fichier(chemin, &taille);
        f["chemin"] = CheminUtf8(fs::relative(chemin, staging));
        f["octets"] = (unsigned long long)taille;
        liste.Ajouter(f);
    }
    artefacts["fichiers"] = liste;
    return EcrireFichierAtomique(staging / "artefacts.json", artefacts.Ecrire(), false);
}

} // namespace decid
