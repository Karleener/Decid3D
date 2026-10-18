// Tests des operations L2 (Operations.cpp), en processus, sur donnees synthetiques generees dans
// verif::DossierTemporaire : une experience publiee factice (staging renomme en resultat + commit.json)
// sert de source aux operations image et volume.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include <opencv2/imgcodecs.hpp>

#include "Fichiers.h"
#include "Json.h"
#include "Modeles.h"
#include "Normalisation.h"
#include "Operations.h"
#include "Partage.h"
#include "Sha256.h"
#include "Vue.h"
#include "Verifier.h"

using namespace decid;

namespace {

// ---------- outils ----------

void EcrireTexte(const fs::path& chemin, const std::string& texte) {
    std::ofstream f(chemin, std::ios::binary);
    f.write(texte.data(), (std::streamsize)texte.size());
}

ValeurJson Json(const std::string& texte) {
    auto j = LireJson(texte);
    VERIFIER(j.Ok());
    return j.Ok() ? j.Valeur() : ValeurJson();
}

ValeurJson LireJ(const fs::path& chemin) {
    auto t = LireFichierTexte(chemin);
    VERIFIER(t.Ok());
    return t.Ok() ? Json(t.Valeur()) : ValeurJson();
}

std::vector<char> Octets(const fs::path& chemin) {
    auto b = LireFichierBinaire(chemin);
    VERIFIER(b.Ok());
    return b.Ok() ? b.Valeur() : std::vector<char>();
}

// Generateur deterministe simple pour les donnees synthetiques
struct Alea {
    std::uint64_t m_etat = 12345;
    double Suivant() {                       // uniforme [0,1)
        m_etat = m_etat * 6364136223846793005ULL + 1442695040888963407ULL;
        return (double)(m_etat >> 11) / 9007199254740992.0;
    }
};

// 3 classes x 40 lignes, 3 attributs, centres separes, bruit +-0.3 (classes chevauchantes), tri par classe
std::string CsvTroisClasses() {
    const double centres[3][3] = {{0.2, 0.2, 0.2}, {0.8, 0.2, 0.5}, {0.5, 0.8, 0.8}};
    Alea alea;
    std::string csv = "Classe,x1,x2,x3\n";
    char tampon[128];
    for (int c = 0; c < 3; ++c)
        for (int i = 0; i < 40; ++i) {
            std::snprintf(tampon, sizeof(tampon), "%d,%.6f,%.6f,%.6f\n", c,
                          centres[c][0] + (alea.Suivant() - 0.5) * 0.6, centres[c][1] + (alea.Suivant() - 0.5) * 0.6,
                          centres[c][2] + (alea.Suivant() - 0.5) * 0.6);
            csv += tampon;
        }
    return csv;
}

// Table de verite du ET, repetee 10 fois (40 lignes)
std::string CsvEt() {
    std::string csv = "Classe,x1,x2\n";
    for (int r = 0; r < 10; ++r) csv += "0,0,0\n0,0,1\n0,1,0\n1,1,1\n";
    return csv;
}

struct Fixture {
    fs::path m_racine, m_donnees, m_donneesEt, m_tache;
};

std::string SourceJson(const fs::path& csv) {
    return "\"source\":{\"type\":\"csv\",\"chemin\":\"" + csv.generic_string() + "\",\"en_tete\":\"oui\"}";
}

ValeurJson Requete(const std::string& id, const std::string& operation, const fs::path& csv,
                   const std::string& reste, const std::string& normalisation = "{\"methode\":\"aucune\"}",
                   const std::string& partage = "{\"methode\":\"premiers_n_par_classe\",\"ratio\":0.5,\"graine\":1}") {
    std::string t = "{\"protocol_version\":1,\"request_id\":\"" + id + "\",\"operation\":\"" + operation + "\"," +
                    SourceJson(csv) + ",\"partage\":" + partage + ",\"normalisation\":" + normalisation;
    if (!reste.empty()) t += "," + reste;
    return Json(t + "}");
}

RapporteurEvenement g_rapporteur = [](const std::string&, long, long, const std::string&) {};

// Publie une experience factice : staging, VerifierStaging, renommage en resultat, commit.json
const Fixture& Fabrique() {
    static Fixture f;
    static bool pret = false;
    if (pret) return f;
    pret = true;
    f.m_racine = verif::DossierTemporaire("operations");
    f.m_donnees = f.m_racine / "trois-classes.csv";
    f.m_donneesEt = f.m_racine / "et.csv";
    EcrireTexte(f.m_donnees, CsvTroisClasses());
    EcrireTexte(f.m_donneesEt, CsvEt());
    f.m_tache = f.m_racine / "taches" / "exp";
    fs::create_directories(f.m_tache);
    ValeurJson req = Requete("exp", "experience", f.m_donnees,
                             "\"algorithme\":{\"famille\":\"svm\",\"noyau\":\"RBF\",\"c\":10,\"gamma\":2},\"threads\":1",
                             "{\"methode\":\"minmax\",\"bas\":0,\"haut\":1}");
    auto r = RequeteDepuisJson(req, f.m_racine);
    VERIFIER(r.Ok());
    if (!r.Ok()) return f;
    std::atomic<bool> annulation{false};
    auto st = ExecuterExperience(r.Valeur(), f.m_tache / "staging", annulation, g_rapporteur);
    VERIFIER(st.Ok());
    VERIFIER(VerifierStaging(f.m_tache / "staging").Ok());
    std::error_code ec;
    fs::rename(f.m_tache / "staging", f.m_tache / "resultat", ec);
    VERIFIER(!ec);
    ValeurJson commit = ValeurJson::Objet();
    commit["schema"] = "decid.commit/1";
    commit["request_id"] = "exp";
    commit["dossier"] = "resultat";
    commit["sha256_artefacts"] = Sha256Fichier(f.m_tache / "resultat" / "artefacts.json");
    EcrireTexte(f.m_tache / "commit.json", commit.Ecrire());
    return f;
}

// Lance une operation dans un staging propre ; renvoie le statut et laisse le staging a l'appelant
Statut Lancer(const ValeurJson& requete, const std::string& nom, fs::path& staging, bool annule = false) {
    const Fixture& f = Fabrique();
    staging = f.m_racine / "st" / nom;
    std::error_code ec;
    fs::remove_all(staging, ec);
    std::atomic<bool> annulation{annule};
    return ExecuterOperation(requete, f.m_racine, staging, annulation, g_rapporteur);
}

// Lance, exige le succes et la verification independante du staging
bool Reussir(const ValeurJson& requete, const std::string& nom, fs::path& staging) {
    Statut st = Lancer(requete, nom, staging);
    if (!st.Ok()) { std::printf("  erreur inattendue : %s\n", st.Err().Texte().c_str()); VERIFIER(st.Ok()); return false; }
    Statut v = VerifierStaging(staging);
    VERIFIER(v.Ok());
    VERIFIER(fs::exists(staging / "artefacts.json"));
    return true;
}

ValeurJson ReqImage(const std::string& id, const std::string& reste) {
    return Json("{\"protocol_version\":1,\"request_id\":\"" + id + "\",\"operation\":\"image\"," + reste + "}");
}

std::vector<std::int16_t> LireI16(const fs::path& chemin) {
    std::vector<char> b = Octets(chemin);
    std::vector<std::int16_t> v(b.size() / 2);
    if (!b.empty()) std::memcpy(v.data(), b.data(), v.size() * 2);
    return v;
}

std::vector<float> LireF32(const fs::path& chemin) {
    std::vector<char> b = Octets(chemin);
    std::vector<float> v(b.size() / 4);
    if (!b.empty()) std::memcpy(v.data(), b.data(), v.size() * 4);
    return v;
}

int ClasseAttendue(const ExperiencePubliee& e, const std::vector<double>& brut) {
    std::vector<double> t = TransformerVecteur(e.m_normalisation, brut);
    cv::Mat x(1, (int)t.size(), CV_32F);
    for (size_t a = 0; a < t.size(); ++a) x.at<float>(0, (int)a) = (float)t[a];
    auto p = Predire(e.m_modele, x);
    VERIFIER(p.Ok());
    return p.Ok() ? p.Valeur()[0] : -1;
}

} // namespace

// ---------- chargement d'une experience publiee ----------

TEST(experience_publiee_chargee) {
    const Fixture& f = Fabrique();
    auto e = ChargerExperiencePubliee(f.m_tache);
    VERIFIER(e.Ok());
    if (!e.Ok()) return;
    VERIFIER_EGAL(e.Valeur().m_jeu.m_nbLignes, 120);
    VERIFIER_EGAL(e.Valeur().m_modele.m_nbAttributs, 3);
    VERIFIER_EGAL(e.Valeur().m_hashModele, Sha256Fichier(f.m_tache / "resultat" / "modele.yml"));
}

// ---------- image ----------

TEST(image_classe_d_un_pixel_png_et_cle_cache) {
    const Fixture& f = Fabrique();
    auto e = ChargerExperiencePubliee(f.m_tache);
    VERIFIER(e.Ok());
    if (!e.Ok()) return;
    ValeurJson req = ReqImage("img1", "\"experience\":\"taches/exp\",\"largeur\":40,\"hauteur\":30,"
                                          "\"attribut_x\":0,\"attribut_y\":1,\"domaine\":\"auto\","
                                          "\"valeurs_fixes\":\"moyenne_apprentissage\"");
    fs::path staging;
    if (!Reussir(req, "image", staging)) return;
    ValeurJson d = LireJ(staging / "image.json");
    VERIFIER(!d.Membre("cle_cache").Texte().empty());
    VERIFIER_EGAL(d.Membre("hash_modele_source").Texte(), e.Valeur().m_hashModele);
    VERIFIER_EGAL(d.Membre("valeurs_fixes_mode").Texte(), std::string("moyenne_apprentissage"));
    std::vector<std::int16_t> classes = LireI16(staging / "image.i16");
    VERIFIER_EGAL((int)classes.size(), 40 * 30);
    // domaine auto = min/max brut du jeu +- 5 %
    const JeuDeDonnees& jeu = e.Valeur().m_jeu;
    double mini = jeu.Valeur(0, 0), maxi = mini;
    for (int i = 0; i < jeu.m_nbLignes; ++i) { mini = std::min(mini, jeu.Valeur(i, 0)); maxi = std::max(maxi, jeu.Valeur(i, 0)); }
    VERIFIER_PROCHE(d.Membre("domaine").Membre("x").Elements()[0].Nombre(), mini - 0.05 * (maxi - mini), 1e-9);
    VERIFIER_PROCHE(d.Membre("domaine").Membre("x").Elements()[1].Nombre(), maxi + 0.05 * (maxi - mini), 1e-9);
    // classe d'un pixel = prediction du modele sur le meme vecteur brut
    RequeteImage ri;
    ri.m_largeur = 40; ri.m_hauteur = 30;
    ri.m_xmin = d.Membre("domaine").Membre("x").Elements()[0].Nombre(); ri.m_xmax = d.Membre("domaine").Membre("x").Elements()[1].Nombre();
    ri.m_ymin = d.Membre("domaine").Membre("y").Elements()[0].Nombre(); ri.m_ymax = d.Membre("domaine").Membre("y").Elements()[1].Nombre();
    std::vector<double> fixes;
    for (const auto& v : d.Membre("valeurs_fixes").Elements()) fixes.push_back(v.Nombre());
    VERIFIER_EGAL((int)fixes.size(), 3);
    const int pixels[4][2] = {{0, 0}, {39, 0}, {20, 15}, {5, 29}};
    for (const auto& px : pixels) {
        double x, y;
        CentrePixel(ri, px[0], px[1], x, y);
        std::vector<double> brut = fixes;
        brut[0] = x; brut[1] = y;
        VERIFIER_EGAL((int)classes[(size_t)px[1] * 40 + (size_t)px[0]], ClasseAttendue(e.Valeur(), brut));
    }
    // PNG lisible
    std::vector<char> png = Octets(staging / "image.png");
    cv::Mat m = cv::imdecode(cv::Mat(1, (int)png.size(), CV_8U, png.data()), cv::IMREAD_UNCHANGED);
    VERIFIER(!m.empty());
    VERIFIER_EGAL(m.cols, 40);
    VERIFIER_EGAL(m.rows, 30);
}

TEST(image_domaine_explicite_et_valeurs_fixes) {
    ValeurJson req = ReqImage("img2", "\"experience\":\"taches/exp\",\"largeur\":8,\"hauteur\":8,"
                                          "\"domaine\":[0,1,0,1],\"valeurs_fixes\":[0.5,0.5,0.25]");
    fs::path staging;
    if (!Reussir(req, "image_explicite", staging)) return;
    ValeurJson d = LireJ(staging / "image.json");
    VERIFIER_EGAL(d.Membre("domaine").Membre("mode").Texte(), std::string("explicite"));
    VERIFIER_PROCHE(d.Membre("valeurs_fixes").Elements()[2].Nombre(), 0.25, 1e-12);
    // meme requete, meme cle de cache et meme image
    fs::path staging2;
    if (!Reussir(ReqImage("img3", "\"experience\":\"taches/exp\",\"largeur\":8,\"hauteur\":8,"
                                      "\"domaine\":[0,1,0,1],\"valeurs_fixes\":[0.5,0.5,0.25]"), "image_explicite2", staging2)) return;
    VERIFIER_EGAL(LireJ(staging2 / "image.json").Membre("cle_cache").Texte(), d.Membre("cle_cache").Texte());
    VERIFIER_EGAL(LireJ(staging2 / "image.json").Membre("hash_image").Texte(), d.Membre("hash_image").Texte());
}

// ---------- volume ----------

TEST(volume_dix_au_cube) {
    const Fixture& f = Fabrique();
    auto e = ChargerExperiencePubliee(f.m_tache);
    VERIFIER(e.Ok());
    if (!e.Ok()) return;
    ValeurJson req = Json("{\"protocol_version\":1,\"request_id\":\"vol1\",\"operation\":\"volume\","
                          "\"experience\":\"taches/exp\",\"resolution\":10,\"attributs\":[0,1,2],\"domaine\":"
                          "[[0,1],[0,1],[0,1]],\"valeurs_fixes\":[0.5,0.5,0.5]}");
    fs::path staging;
    if (!Reussir(req, "volume", staging)) return;
    std::vector<std::int16_t> classes = LireI16(staging / "volume.i16");
    VERIFIER_EGAL((int)classes.size(), 1000);
    VERIFIER(fs::exists(staging / "volume.json"));
    // voxel (x=3, y=7, z=1) : ordre z, puis y, puis x
    std::vector<double> brut = {0.35, 0.75, 0.15};
    VERIFIER_EGAL((int)classes[(size_t)(1 * 100 + 7 * 10 + 3)], ClasseAttendue(e.Valeur(), brut));
    VERIFIER_EGAL(LireJ(staging / "volume.json").Membre("resolution").Entier(), 10LL);
}

// ---------- validation ----------

TEST(validation_k5_chaque_ligne_une_fois) {
    const Fixture& f = Fabrique();
    ValeurJson req = Requete("val1", "validation", f.m_donnees,
                             "\"algorithme\":{\"famille\":\"svm\",\"noyau\":\"RBF\",\"c\":10,\"gamma\":2},"
                             "\"validation\":{\"k\":5,\"mode\":\"melange\",\"stratifie\":false,\"graine\":1,"
                             "\"perimetre\":\"apprentissage\"}");
    fs::path staging;
    if (!Reussir(req, "validation", staging)) return;
    ValeurJson plis = LireJ(staging / "plis.json");
    VERIFIER_EGAL((int)plis.Membre("plis").Elements().size(), 5);
    auto jeu = LireCsv(f.m_donnees, ContratCsv{',', ModeEnTete::Oui, 0});
    VERIFIER(jeu.Ok());
    if (!jeu.Ok()) return;
    auto partage = CreerPartage(jeu.Valeur(), MethodePartage::PremiersNParClasse, 0.5, 1, false);
    VERIFIER(partage.Ok());
    if (!partage.Ok()) return;
    std::multiset<int> vues;
    for (const auto& pli : plis.Membre("plis").Elements())
        for (const auto& v : pli.Membre("validation").Elements()) vues.insert((int)v.Entier());
    const std::set<int> attendues(partage.Valeur().m_apprentissage.begin(), partage.Valeur().m_apprentissage.end());
    VERIFIER_EGAL(vues.size(), attendues.size());
    for (int i : attendues) VERIFIER_EGAL((int)vues.count(i), 1);
    for (int i : partage.Valeur().m_test) VERIFIER_EGAL((int)vues.count(i), 0);   // le test final n'entre jamais
    ValeurJson v = LireJ(staging / "validation.json");
    VERIFIER_EGAL(v.Membre("perimetre").Texte(), std::string("apprentissage"));
    VERIFIER_EGAL((int)v.Membre("predictions_hors_pli").Elements().size(), (int)attendues.size());
}

// ---------- balayage ----------

TEST(balayage_trois_valeurs_meilleur_indice) {
    const Fixture& f = Fabrique();
    ValeurJson req = Requete("bal1", "balayage", f.m_donnees,
                             "\"algorithme\":{\"famille\":\"svm\",\"noyau\":\"RBF\",\"c\":10,\"gamma\":1},"
                             "\"validation\":{\"k\":5,\"mode\":\"melange\",\"graine\":1},"
                             "\"balayage\":{\"parametre\":\"gamma\",\"valeurs\":[0.1,1,10]},\"classe_positive\":0");
    fs::path staging;
    if (!Reussir(req, "balayage", staging)) return;
    ValeurJson b = LireJ(staging / "balayage.json");
    const auto& cands = b.Membre("candidats").Elements();
    VERIFIER_EGAL((int)cands.size(), 3);
    if (cands.size() != 3) return;
    int meilleur = 0;
    for (int i = 1; i < 3; ++i)
        if (cands[(size_t)i].Membre("validation").Membre("moyenne_par_pli").Membre("value").Nombre() >
            cands[(size_t)meilleur].Membre("validation").Membre("moyenne_par_pli").Membre("value").Nombre() + 1e-12)
            meilleur = i;
    VERIFIER_EGAL((int)b.Membre("indice_meilleur").Entier(), meilleur);
    VERIFIER_PROCHE(cands[1].Membre("valeur").Nombre(), 1.0, 1e-12);
    ValeurJson p = LireJ(staging / "points_fnr_fpr.json");
    VERIFIER_EGAL((int)p.Membre("points").Elements().size(), 3);
    VERIFIER(p.Membre("avertissement").Texte().find("ce n'est pas une courbe ROC") != std::string::npos);
    VERIFIER(!p.Contient("auc"));
}

TEST(balayage_logarithmique) {
    const Fixture& f = Fabrique();
    ValeurJson req = Requete("bal2", "balayage", f.m_donnees,
                             "\"algorithme\":{\"famille\":\"svm\",\"noyau\":\"RBF\",\"c\":10,\"gamma\":1},"
                             "\"validation\":{\"k\":3},"
                             "\"balayage\":{\"parametre\":\"gamma\",\"log\":{\"debut\":0.01,\"fin\":1,\"par_decade\":1}}");
    fs::path staging;
    if (!Reussir(req, "balayage_log", staging)) return;
    VERIFIER_EGAL((int)LireJ(staging / "balayage.json").Membre("candidats").Elements().size(), 3);
}

// ---------- operations pas a pas ----------

TEST(adaboost_un_etat_par_iteration_poids_normalises) {
    const Fixture& f = Fabrique();
    ValeurJson req = Requete("ada1", "adaboost_pas_a_pas", f.m_donnees,
                             "\"adaboost\":{\"faible\":\"seuil\",\"iterations\":10,\"classe_positive\":0},"
                             "\"trace\":{\"max_etats\":2000,\"max_octets\":67108864}");
    fs::path staging;
    if (!Reussir(req, "adaboost", staging)) return;
    VERIFIER(fs::exists(staging / "partage.json"));     // le partage utilise est persiste avec la trace
    ValeurJson fin = LireJ(staging / "etat_final.json");
    VERIFIER_EGAL((int)fin.Membre("iterations_effectuees").Entier(), 10);
    VERIFIER_EGAL(fin.Membre("raison_arret").Texte(), std::string("iterations_atteintes"));
    ValeurJson trace = LireJ(staging / "trace.json");
    VERIFIER_EGAL(trace.Membre("schema").Texte(), std::string("decid.trace/1"));
    const auto& etats = trace.Membre("etats").Elements();
    VERIFIER_EGAL((int)etats.size(), 10);
    std::vector<float> flux = LireF32(staging / "trace.f32");
    const int n = 60;     // lignes d'apprentissage : 20 par classe
    for (const auto& et : etats) {
        VERIFIER_EGAL((int)et.Membre("taille").Entier(), n);
        const size_t debut = (size_t)et.Membre("decalage").Entier();
        VERIFIER(debut + (size_t)n <= flux.size());
        if (debut + (size_t)n > flux.size()) return;
        double somme = 0;
        for (int i = 0; i < n; ++i) somme += flux[debut + (size_t)i];
        VERIFIER_PROCHE(somme, 1.0, 1e-4);
    }
}

TEST(perceptron_converge_sur_le_et) {
    const Fixture& f = Fabrique();
    ValeurJson req = Requete("per1", "perceptron", f.m_donneesEt,
                             "\"perceptron\":{\"eta\":0.1,\"epoques_max\":100,\"melanger\":false,\"graine\":1,"
                             "\"classe_positive\":1}",
                             "{\"methode\":\"aucune\"}",
                             "{\"methode\":\"premiers_n\",\"ratio\":0.75,\"graine\":1}");
    fs::path staging;
    if (!Reussir(req, "perceptron", staging)) return;
    ValeurJson fin = LireJ(staging / "etat_final.json");
    VERIFIER(fin.Membre("converge").Booleen());
    VERIFIER_EGAL((int)fin.Membre("poids").Elements().size(), 2);
    VERIFIER(!LireJ(staging / "trace.json").Membre("etats").Elements().empty());
    VERIFIER(fs::file_size(staging / "trace.f32") > 0);
}

TEST(kmeans_j_decroissant) {
    const Fixture& f = Fabrique();
    ValeurJson req = Requete("km1", "kmeans", f.m_donnees,
                             "\"kmeans\":{\"k\":3,\"init\":\"kmeans++\",\"graine\":1,\"iter_max\":100}");
    fs::path staging;
    if (!Reussir(req, "kmeans", staging)) return;
    ValeurJson trace = LireJ(staging / "trace.json");
    double precedent = 1e300;
    int nbJ = 0;
    for (const auto& et : trace.Membre("etats").Elements()) {
        if (!et.Membre("grandeurs").Contient("J")) continue;
        const double j = et.Membre("grandeurs").Membre("J").Nombre();
        VERIFIER(j <= precedent * (1 + 1e-9) + 1e-12);
        precedent = j;
        ++nbJ;
    }
    VERIFIER(nbJ >= 2);
    ValeurJson fin = LireJ(staging / "etat_final.json");
    VERIFIER_EGAL((int)fin.Membre("centres").Elements().size(), 3);
    VERIFIER_EGAL((int)fin.Membre("affectations").Elements().size(), 60);
}

// ---------- erreurs ----------

TEST(operation_inconnue_est_une_erreur) {
    fs::path staging;
    Statut st = Lancer(Json("{\"protocol_version\":1,\"request_id\":\"x\",\"operation\":\"roc\"}"), "inconnue", staging);
    VERIFIER(!st.Ok());
    if (!st.Ok()) VERIFIER(st.Err().m_code == CodeErreur::Protocole);
    VERIFIER(!fs::exists(staging / "artefacts.json"));
}

TEST(experience_non_publiee_ou_alteree_est_refusee) {
    const Fixture& f = Fabrique();
    const fs::path sansCommit = f.m_racine / "taches" / "sans-commit";
    const fs::path altere = f.m_racine / "taches" / "altere";
    std::error_code ec;
    for (const auto& dest : {sansCommit, altere}) {
        fs::remove_all(dest, ec);
        fs::copy(f.m_tache, dest, fs::copy_options::recursive, ec);
        VERIFIER(!ec);
    }
    fs::remove(sansCommit / "commit.json");
    ValeurJson commit = LireJ(altere / "commit.json");
    commit["sha256_artefacts"] = std::string(64, '0');
    fs::remove(altere / "commit.json");
    EcrireTexte(altere / "commit.json", commit.Ecrire());
    VERIFIER(!ChargerExperiencePubliee(sansCommit).Ok());
    VERIFIER(!ChargerExperiencePubliee(altere).Ok());
    fs::path staging;
    const std::string corps = "\"largeur\":4,\"hauteur\":4,\"domaine\":\"auto\"";
    VERIFIER(!Lancer(ReqImage("nc", "\"experience\":\"taches/sans-commit\"," + corps), "sans_commit", staging).Ok());
    VERIFIER(!fs::exists(staging / "artefacts.json"));
    VERIFIER(!Lancer(ReqImage("alt", "\"experience\":\"taches/altere\"," + corps), "altere", staging).Ok());
    VERIFIER(!fs::exists(staging / "artefacts.json"));
    // artefact altere apres commit : refuse aussi
    const fs::path corrompu = f.m_racine / "taches" / "corrompu";
    fs::remove_all(corrompu, ec);
    fs::copy(f.m_tache, corrompu, fs::copy_options::recursive, ec);
    EcrireTexte(corrompu / "resultat" / "evaluation.json", "{}");
    VERIFIER(!ChargerExperiencePubliee(corrompu).Ok());
}

TEST(annulation_sans_artefacts) {
    const Fixture& f = Fabrique();
    const std::vector<ValeurJson> requetes = {
        ReqImage("a1", "\"experience\":\"taches/exp\",\"largeur\":8,\"hauteur\":8"),
        Requete("a2", "validation", f.m_donnees, "\"algorithme\":{\"famille\":\"svm\"},\"validation\":{\"k\":3}"),
        Requete("a3", "kmeans", f.m_donnees, "\"kmeans\":{\"k\":3}"),
        Requete("a4", "adaboost_pas_a_pas", f.m_donnees, "\"adaboost\":{\"iterations\":5}"),
    };
    int n = 0;
    for (const auto& r : requetes) {
        fs::path staging;
        Statut st = Lancer(r, "annule" + std::to_string(n++), staging, true);
        VERIFIER(!st.Ok());
        if (!st.Ok()) VERIFIER(st.Err().m_code == CodeErreur::Annulation);
        VERIFIER(!fs::exists(staging / "artefacts.json"));
    }
}

// ---------- k-means : affectations publiees dans la trace ----------

namespace {

std::vector<std::int32_t> LireI32(const fs::path& chemin) {
    std::vector<char> b = Octets(chemin);
    std::vector<std::int32_t> v(b.size() / 4);
    if (!b.empty()) std::memcpy(v.data(), b.data(), v.size() * 4);
    return v;
}

} // namespace

TEST(kmeans_affectations_par_etat) {
    const Fixture& f = Fabrique();
    ValeurJson req = Requete("km2", "kmeans", f.m_donnees, "\"kmeans\":{\"k\":3,\"graine\":2}");
    fs::path staging;
    if (!Reussir(req, "kmeans_aff", staging)) return;
    ValeurJson trace = LireJ(staging / "trace.json");
    VERIFIER_EGAL(trace.Membre("schema").Texte(), std::string("decid.trace/1"));
    VERIFIER(trace.Contient("fichier_affectations"));
    const std::vector<std::int32_t> aff = LireI32(staging / "trace_affectations.i32");
    const auto& etats = trace.Membre("etats").Elements();
    VERIFIER(!etats.empty());
    VERIFIER_EGAL(aff.size(), etats.size() * 60);
    for (const auto& et : etats) {
        VERIFIER_EGAL(et.Membre("taille_affectations").Entier(), 60LL);
        VERIFIER_EGAL(et.Membre("taille").Entier(), 9LL);              // centres inchanges : k x p
    }
    for (std::int32_t a : aff) VERIFIER(a >= 0 && a < 3);
    // dernier etat = affectations finales
    ValeurJson fin = LireJ(staging / "etat_final.json");
    const long long dernier = etats.back().Membre("decalage_affectations").Entier();
    const auto& finales = fin.Membre("affectations").Elements();
    bool egales = finales.size() == 60;
    for (size_t i = 0; egales && i < finales.size(); ++i) egales = finales[i].Entier() == aff[(size_t)dernier + i];
    VERIFIER(egales);
    // les operations sans affectations gardent exactement l'ancien format
    ValeurJson ada = Requete("ada-fmt", "adaboost_pas_a_pas", f.m_donnees, "\"adaboost\":{\"iterations\":2}");
    fs::path st2;
    if (!Reussir(ada, "ada_format", st2)) return;
    VERIFIER(!LireJ(st2 / "trace.json").Contient("fichier_affectations"));
    VERIFIER(!fs::exists(st2 / "trace_affectations.i32"));
}

// ---------- temps de decision et normalisation imposee par un fichier .lim ----------

TEST(evaluation_publie_le_temps_de_decision) {
    const Fixture& f = Fabrique();
    ValeurJson ev = LireJ(f.m_tache / "resultat" / "evaluation.json");
    const ValeurJson& t = ev.Membre("temps_decision");
    VERIFIER(t.Contient("par_echantillon_us") && t.Contient("echantillons") && t.Contient("mesure"));
    VERIFIER(t.Membre("par_echantillon_us").Nombre() > 0);
    VERIFIER_EGAL(t.Membre("echantillons").Entier(), 60LL);
    VERIFIER(!t.Membre("mesure").Texte().empty());
}

namespace {

std::string NombreExact(double v) {
    char tampon[64];
    std::snprintf(tampon, sizeof(tampon), "%.17g", v);
    return tampon;
}

} // namespace

TEST(experience_minmax_lim_meme_hash_que_minmax) {
    const Fixture& f = Fabrique();
    ValeurJson norm = LireJ(f.m_tache / "resultat" / "normalisation.json");
    std::string lim = "3\r\n";
    for (size_t a = 0; a < 3; ++a)
        lim += NombreExact(norm.Membre("min").Elements()[a].Nombre()) + " " +
               NombreExact(norm.Membre("max").Elements()[a].Nombre()) + "\r\n";
    EcrireTexte(f.m_racine / "bornes.lim", lim);
    ValeurJson req = Requete("exp-lim", "experience", f.m_donnees,
                             "\"algorithme\":{\"famille\":\"svm\",\"noyau\":\"RBF\",\"c\":10,\"gamma\":2},\"threads\":1",
                             "{\"methode\":\"minmax_lim\",\"fichier\":\"bornes.lim\",\"bas\":0,\"haut\":1}");
    VERIFIER(ValiderRequete(req, f.m_racine).Ok());
    fs::path staging;
    if (!Reussir(req, "exp_lim", staging)) return;
    ValeurJson publie = LireJ(staging / "normalisation.json");
    VERIFIER_EGAL(publie.Membre("methode").Texte(), std::string("minmax"));
    VERIFIER_EGAL(publie.Membre("hash").Texte(), norm.Membre("hash").Texte());
    VERIFIER_EGAL(LireJ(staging / "requete.json").Membre("normalisation").Membre("methode").Texte(),
                  std::string("minmax_lim"));
    // bornes differentes : hash different ; fichier invalide : refus avec numero de ligne
    EcrireTexte(f.m_racine / "large.lim", "3\n-1 2\n-1 2\n-1 2\n");
    ValeurJson large = Requete("exp-lim2", "experience", f.m_donnees, "\"algorithme\":{\"famille\":\"svm\"}",
                               "{\"methode\":\"minmax_lim\",\"fichier\":\"large.lim\"}");
    fs::path st2;
    if (Reussir(large, "exp_lim_large", st2))
        VERIFIER(LireJ(st2 / "normalisation.json").Membre("hash").Texte() != norm.Membre("hash").Texte());
    EcrireTexte(f.m_racine / "faux.lim", "3\n0 1\n0 1\n");
    ValeurJson faux = Requete("exp-lim3", "experience", f.m_donnees, "\"algorithme\":{\"famille\":\"svm\"}",
                              "{\"methode\":\"minmax_lim\",\"fichier\":\"faux.lim\"}");
    fs::path st3;
    Statut st = Lancer(faux, "exp_lim_faux", st3);
    VERIFIER(!st.Ok());
    if (!st.Ok()) VERIFIER_EGAL(st.Err().m_ligne, 3L);
    VERIFIER(!fs::exists(st3 / "artefacts.json"));
    ValeurJson sansFichier = Requete("exp-lim4", "experience", f.m_donnees, "\"algorithme\":{\"famille\":\"svm\"}",
                                     "{\"methode\":\"minmax_lim\"}");
    VERIFIER(!ValiderRequete(sansFichier, f.m_racine).Ok());
    // validation croisee : memes bornes imposees dans chaque pli
    ValeurJson val = Requete("val-lim", "validation", f.m_donnees, "\"validation\":{\"k\":3}",
                             "{\"methode\":\"minmax_lim\",\"fichier\":\"bornes.lim\"}");
    fs::path st5;
    VERIFIER(Reussir(val, "val_lim", st5));
}

// ---------- decider : modele publie applique a une autre source ----------

namespace {

ValeurJson ReqDecider(const std::string& id, const fs::path& csv, int cible, const std::string& enTete = "oui") {
    return Json("{\"protocol_version\":1,\"request_id\":\"" + id + "\",\"operation\":\"decider\","
                "\"experience\":\"taches/exp\",\"source\":{\"type\":\"csv\",\"chemin\":\"" + csv.generic_string() +
                "\",\"en_tete\":\"" + enTete + "\",\"colonne_cible\":" + std::to_string(cible) + "}}");
}

// Le CSV des trois classes, sans la colonne de classe
std::string SansClasses(const std::string& csv) {
    std::string sortie, ligne;
    for (char ch : csv) {
        if (ch != '\n') { ligne += ch; continue; }
        sortie += ligne.substr(ligne.find(',') + 1) + "\n";
        ligne.clear();
    }
    return sortie;
}

std::vector<std::string> Lignes(const std::string& texte) {
    std::vector<std::string> lignes;
    std::string l;
    for (char ch : texte) { if (ch == '\n') { lignes.push_back(l); l.clear(); } else l += ch; }
    return lignes;
}

} // namespace

TEST(decider_avec_classes_predictions_et_confusion) {
    const Fixture& f = Fabrique();
    ValeurJson req = ReqDecider("dec1", f.m_donnees, 0);
    VERIFIER(ValiderRequete(req, f.m_racine).Ok());
    fs::path staging;
    if (!Reussir(req, "decider", staging)) return;
    ValeurJson ev = LireJ(staging / "evaluation.json");
    VERIFIER_EGAL(ev.Membre("statut").Texte(), std::string("evalue"));
    VERIFIER(ev.Membre("temps_decision").Membre("par_echantillon_us").Nombre() > 0);
    VERIFIER_EGAL(ev.Membre("temps_decision").Membre("echantillons").Entier(), 120LL);
    VERIFIER(fs::exists(staging / "comptes.csv"));
    ValeurJson d = LireJ(staging / "decision.json");
    VERIFIER_EGAL(d.Membre("schema").Texte(), std::string("decid.decision/1"));
    VERIFIER_EGAL(d.Membre("lignes").Entier(), 120LL);
    auto e = ChargerExperiencePubliee(f.m_tache);
    VERIFIER(e.Ok());
    if (!e.Ok()) return;
    VERIFIER_EGAL(d.Membre("hash_normalisation").Texte(), e.Valeur().m_normalisation.Hash());
    // chaque prediction = celle du modele publie sur la ligne brute normalisee
    auto t = LireFichierTexte(staging / "predictions.csv");
    VERIFIER(t.Ok());
    if (!t.Ok()) return;
    const std::vector<std::string> lignes = Lignes(t.Valeur());
    VERIFIER_EGAL(lignes.size(), (size_t)121);
    VERIFIER_EGAL(lignes[0], std::string("ligne;origine;verite;prediction"));
    const JeuDeDonnees& jeu = e.Valeur().m_jeu;
    for (int i = 0; i < 120; i += 17) {
        std::vector<double> brut;
        for (int a = 0; a < 3; ++a) brut.push_back(jeu.Valeur(i, a));
        const std::string attendu = jeu.m_nomsClasses[(size_t)ClasseAttendue(e.Valeur(), brut)];
        VERIFIER_EGAL(lignes[(size_t)i + 1].substr(lignes[(size_t)i + 1].rfind(';') + 1), attendu);
    }
    long long total = 0;
    for (const auto& c : d.Membre("comptes_predits").Elements()) total += c.Membre("lignes").Entier();
    VERIFIER_EGAL(total, 120LL);
}

TEST(decider_sans_classes) {
    const Fixture& f = Fabrique();
    auto csv = LireFichierTexte(f.m_donnees);
    VERIFIER(csv.Ok());
    if (!csv.Ok()) return;
    EcrireTexte(f.m_racine / "sans-classes.csv", SansClasses(csv.Valeur()));
    fs::path staging;
    if (!Reussir(ReqDecider("dec2", f.m_racine / "sans-classes.csv", -1), "decider_sans", staging)) return;
    ValeurJson ev = LireJ(staging / "evaluation.json");
    VERIFIER_EGAL(ev.Membre("statut").Texte(), std::string("sans_classes"));
    VERIFIER(ev.Membre("temps_decision").Membre("par_echantillon_us").Nombre() > 0);
    VERIFIER(!fs::exists(staging / "comptes.csv"));
    VERIFIER(!LireJ(staging / "decision.json").Membre("avec_classes").Booleen());
    VERIFIER(!Lancer(ReqDecider("dec2b", f.m_racine / "sans-classes.csv", -2), "decider_cible", staging).Ok());
}

TEST(decider_refuse_des_attributs_differents) {
    const Fixture& f = Fabrique();
    EcrireTexte(f.m_racine / "deux-attributs.csv", "Classe,x1,x2\n0,0.1,0.2\n1,0.5,0.6\n");
    EcrireTexte(f.m_racine / "autres-noms.csv", "Classe,a,b,c\n0,0.1,0.2,0.3\n1,0.5,0.6,0.7\n");
    fs::path staging;
    Statut st = Lancer(ReqDecider("dec3", f.m_racine / "deux-attributs.csv", 0), "decider_dim", staging);
    VERIFIER(!st.Ok());
    if (!st.Ok()) {
        VERIFIER(st.Err().m_code == CodeErreur::ModeleIncompatible);
        VERIFIER(st.Err().m_message.find("2 attribut") != std::string::npos);
    }
    VERIFIER(!fs::exists(staging / "artefacts.json"));
    st = Lancer(ReqDecider("dec4", f.m_racine / "autres-noms.csv", 0), "decider_noms", staging);
    VERIFIER(!st.Ok());
    VERIFIER(!fs::exists(staging / "artefacts.json"));
    // sans en-tete, seuls le nombre et l'ordre comptent
    EcrireTexte(f.m_racine / "sans-entete.csv", "0,0.1,0.2,0.3\n1,0.5,0.6,0.7\n");
    VERIFIER(Reussir(ReqDecider("dec5", f.m_racine / "sans-entete.csv", 0, "non"), "decider_sans_entete", staging));
}

TEST(decider_classe_inconnue_non_evaluable) {
    const Fixture& f = Fabrique();
    EcrireTexte(f.m_racine / "classe-9.csv", "Classe,x1,x2,x3\n0,0.2,0.2,0.2\n9,0.8,0.2,0.5\n");
    fs::path staging;
    if (!Reussir(ReqDecider("dec6", f.m_racine / "classe-9.csv", 0), "decider_inconnue", staging)) return;
    ValeurJson ev = LireJ(staging / "evaluation.json");
    VERIFIER_EGAL(ev.Membre("statut").Texte(), std::string("non_evaluable"));
    VERIFIER(ev.Membre("raison").Texte().find("9") != std::string::npos);
    VERIFIER(fs::exists(staging / "predictions.csv"));
    // experience absente : refus avant toute ecriture
    ValeurJson sans = Json("{\"protocol_version\":1,\"request_id\":\"dec7\",\"operation\":\"decider\","
                           "\"experience\":\"taches/absente\",\"source\":{\"type\":\"csv\",\"chemin\":\"x.csv\"}}");
    VERIFIER(!ValiderRequete(sans, f.m_racine).Ok());
}

int main(int argc, char** argv) { return LancerTests(argc, argv); }
