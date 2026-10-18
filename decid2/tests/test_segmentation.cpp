// Tests de l'atelier image (Segmentation.cpp) : caracteristique couleur exacte, echantillonnage par zones,
// chaine complete echantillons -> experience publiee -> segmentation, refus explicites.
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include <opencv2/imgcodecs.hpp>

#include "Experience.h"
#include "Fichiers.h"
#include "Json.h"
#include "Operations.h"
#include "Segmentation.h"
#include "Sha256.h"
#include "Verifier.h"

using namespace decid;

namespace {

ValeurJson Json(const std::string& texte) {
    auto j = LireJson(texte);
    VERIFIER(j.Ok());
    return j.Ok() ? j.Valeur() : ValeurJson();
}

std::string Barre(const fs::path& p) {
    std::string t = CheminUtf8(p);
    for (char& c : t) if (c == '\\') c = '/';
    return t;
}

void EcrireTexte(const fs::path& chemin, const std::string& texte) {
    std::ofstream f(chemin, std::ios::binary);
    f.write(texte.data(), (std::streamsize)texte.size());
}

// 120 x 80 : moitie gauche rouge, moitie droite verte, bruit +-20 deterministe
fs::path ImageDeuxMoities(const fs::path& dossier) {
    cv::Mat image(80, 120, CV_8UC3);
    std::uint64_t etat = 7;
    auto bruit = [&etat]() { etat = etat * 6364136223846793005ULL + 1442695040888963407ULL; return (int)((etat >> 33) % 41) - 20; };
    for (int y = 0; y < 80; ++y)
        for (int x = 0; x < 120; ++x) {
            const bool gauche = x < 60;
            image.at<cv::Vec3b>(y, x) = cv::Vec3b(cv::saturate_cast<uchar>(40 + bruit()),
                                                  cv::saturate_cast<uchar>((gauche ? 50 : 190) + bruit()),
                                                  cv::saturate_cast<uchar>((gauche ? 200 : 60) + bruit()));
        }
    std::vector<uchar> png;
    cv::imencode(".png", image, png);
    const fs::path p = dossier / "moities.png";
    EcrireTexte(p, std::string(png.begin(), png.end()));
    return p;
}

Statut Lancer(const std::string& requete, const fs::path& dossier, const fs::path& staging) {
    std::atomic<bool> annulation{false};
    return ExecuterOperation(Json(requete), dossier, staging, annulation, nullptr);
}

// Publication comme l'orchestrateur : staging -> resultat, puis commit.json
void Publier(const fs::path& tache) {
    std::error_code ec;
    fs::rename(tache / "staging", tache / "resultat", ec);
    VERIFIER(!ec);
    ValeurJson commit = ValeurJson::Objet();
    commit["schema"] = "decid.commit/1";
    commit["dossier"] = "resultat";
    commit["sha256_artefacts"] = Sha256Fichier(tache / "resultat" / "artefacts.json");
    EcrireTexte(tache / "commit.json", commit.Ecrire());
}

std::string ReqEchantillons(const fs::path& image, const std::string& espace, const std::string& zones) {
    return R"({"operation":"echantillonner_couleurs","protocol_version":1,"request_id":"ech","image":")" + Barre(image) +
           R"(","espace":")" + espace + R"(","fenetre":5,"graine":3,"par_zone":150,"zones":)" + zones + "}";
}

const char* ZONES = R"([{"classe":"rouge","x":5,"y":5,"largeur":40,"hauteur":70},
                        {"classe":"vert","x":75,"y":5,"largeur":40,"hauteur":70}])";

std::string ReqSegmenter(const fs::path& image, const std::string& espace, int fenetre) {
    return R"({"operation":"segmenter","protocol_version":1,"request_id":"seg","experience":"taches/exp","echantillons":"taches/ech",
               "image":")" + Barre(image) + R"(","espace":")" + espace + R"(","fenetre":)" + std::to_string(fenetre) + "}";
}

struct Chaine { fs::path m_racine, m_image; bool m_prete = false; };

// echantillonnage (Lab) -> experience KPPV publiee sur echantillons.csv
const Chaine& Fabrique() {
    static Chaine c;
    if (c.m_prete) return c;
    c.m_prete = true;
    c.m_racine = verif::DossierTemporaire("segmentation");
    c.m_image = ImageDeuxMoities(c.m_racine);
    const fs::path ech = c.m_racine / "taches" / "ech";
    VERIFIER(Lancer(ReqEchantillons(c.m_image, "Lab", ZONES), c.m_racine, ech / "staging").Ok());
    Publier(ech);
    const std::string exp = R"({"protocol_version":1,"request_id":"exp","source":{"type":"csv","chemin":")" +
        Barre(ech / "resultat" / "echantillons.csv") + R"(","en_tete":"non","colonne_cible":0},
        "partage":{"methode":"premiers_n_par_classe","ratio":0.5},"normalisation":{"methode":"minmax"},
        "algorithme":{"famille":"kppv","k":3}})";
    auto r = RequeteDepuisJson(Json(exp), c.m_racine);
    VERIFIER(r.Ok());
    if (!r.Ok()) return c;
    std::atomic<bool> annulation{false};
    RapporteurEvenement muet = [](const std::string&, long, long, const std::string&) {};
    VERIFIER(ExecuterExperience(r.Valeur(), c.m_racine / "taches" / "exp" / "staging", annulation, muet).Ok());
    Publier(c.m_racine / "taches" / "exp");
    return c;
}

} // namespace

TEST(caracteristique_uniforme_exacte) {
    cv::Mat image(20, 30, CV_8UC3, cv::Scalar(10, 20, 30));   // B, G, R
    auto c = CaracteristiquesCouleur(image, EspaceCouleur::Rgb, 5);
    VERIFIER(c.Ok());
    if (!c.Ok()) return;
    const cv::Vec3f v = c.Valeur().at<cv::Vec3f>(0, 0);   // bord reflechi : meme valeur
    VERIFIER_PROCHE(v[0], 30 / 255.0, 1e-6);
    VERIFIER_PROCHE(v[1], 20 / 255.0, 1e-6);
    VERIFIER_PROCHE(v[2], 10 / 255.0, 1e-6);
    VERIFIER(!CaracteristiquesCouleur(image, EspaceCouleur::Lab, 4).Ok());   // fenetre paire
}

TEST(fenetre_moyenne_les_voisins) {
    cv::Mat image(9, 9, CV_8UC3, cv::Scalar(0, 0, 0));
    image.at<cv::Vec3b>(4, 4) = cv::Vec3b(0, 0, 255);   // un pixel rouge au centre
    auto c = CaracteristiquesCouleur(image, EspaceCouleur::Rgb, 3);
    VERIFIER(c.Ok());
    if (!c.Ok()) return;
    VERIFIER_PROCHE(c.Valeur().at<cv::Vec3f>(4, 4)[0], 1.0 / 9, 1e-6);
    VERIFIER_PROCHE(c.Valeur().at<cv::Vec3f>(3, 3)[0], 1.0 / 9, 1e-6);
    VERIFIER_PROCHE(c.Valeur().at<cv::Vec3f>(2, 2)[0], 0.0, 1e-6);
}

TEST(echantillonnage_publie_csv_et_comptes) {
    const Chaine& c = Fabrique();
    const fs::path r = c.m_racine / "taches" / "ech" / "resultat";
    auto texte = LireFichierTexte(r / "echantillons.json");
    VERIFIER(texte.Ok());
    if (!texte.Ok()) return;
    ValeurJson d = Json(texte.Valeur());
    VERIFIER_EGAL(d.Membre("comptes").Membre("rouge").Entier(), 150LL);
    VERIFIER_EGAL(d.Membre("comptes").Membre("vert").Entier(), 150LL);
    VERIFIER_EGAL(d.Membre("espace").Texte(), std::string("Lab"));
    VERIFIER_EGAL(d.Membre("sha256_csv").Texte(), Sha256Fichier(r / "echantillons.csv"));
    ContratCsv contrat;
    auto jeu = LireCsv(r / "echantillons.csv", contrat);
    VERIFIER(jeu.Ok());
    if (jeu.Ok()) { VERIFIER_EGAL(jeu.Valeur().m_nbLignes, 300); VERIFIER_EGAL(jeu.Valeur().m_nbAttributs, 3); }
    // meme graine : memes octets
    const fs::path bis = c.m_racine / "bis";
    VERIFIER(Lancer(ReqEchantillons(c.m_image, "Lab", ZONES), c.m_racine, bis).Ok());
    VERIFIER_EGAL(Sha256Fichier(bis / "echantillons.csv"), Sha256Fichier(r / "echantillons.csv"));
}

TEST(segmentation_separe_les_deux_moities) {
    const Chaine& c = Fabrique();
    const fs::path staging = c.m_racine / "seg";
    Statut st = Lancer(ReqSegmenter(c.m_image, "Lab", 5), c.m_racine, staging);
    VERIFIER(st.Ok());
    if (!st.Ok()) { std::printf("  %s\n", st.Err().m_message.c_str()); return; }
    auto b = LireFichierBinaire(staging / "segmentation.i16");
    VERIFIER(b.Ok());
    if (!b.Ok()) return;
    VERIFIER_EGAL(b.Valeur().size(), (size_t)(120 * 80 * 2));
    const std::int16_t* classes = reinterpret_cast<const std::int16_t*>(b.Valeur().data());
    long justes = 0;
    for (int y = 0; y < 80; ++y)
        for (int x = 0; x < 120; ++x) justes += classes[y * 120 + x] == (x < 60 ? 0 : 1);   // rouge = 0, vert = 1
    VERIFIER(justes > 120 * 80 * 97 / 100);
    ValeurJson d = Json(LireFichierTexte(staging / "segmentation.json").Valeur());
    VERIFIER(d.Membre("echantillons_verifies").Booleen());
    VERIFIER_EGAL(d.Membre("classes").Elements().size(), (size_t)2);
    VERIFIER_EGAL(d.Membre("classes").Elements()[0].Membre("nom").Texte(), std::string("rouge"));
    VERIFIER_EGAL(d.Membre("sha256_classes").Texte(), Sha256Fichier(staging / "segmentation.i16"));
    cv::Mat png = cv::imread(CheminUtf8(staging / "segmentation.png"), cv::IMREAD_UNCHANGED);
    VERIFIER_EGAL(png.cols, 120);
    VERIFIER_EGAL(png.rows, 80);
}

TEST(refus_explicites) {
    const Chaine& c = Fabrique();
    // espace different de celui de l'echantillonnage
    VERIFIER(!Lancer(ReqSegmenter(c.m_image, "HSV", 5), c.m_racine, c.m_racine / "r1").Ok());
    VERIFIER(!Lancer(ReqSegmenter(c.m_image, "Lab", 3), c.m_racine, c.m_racine / "r2").Ok());
    VERIFIER(!ValiderRequete(Json(ReqSegmenter(c.m_image, "Lab", 4)), c.m_racine).Ok());
    VERIFIER(!ValiderRequete(Json(ReqSegmenter(c.m_image, "XYZ", 5)), c.m_racine).Ok());
    VERIFIER(ValiderRequete(Json(ReqSegmenter(c.m_image, "Lab", 5)), c.m_racine).Ok());
    // fichier qui n'est pas une image
    EcrireTexte(c.m_racine / "faux.png", "pas une image");
    VERIFIER(!Lancer(ReqEchantillons(c.m_racine / "faux.png", "Lab", ZONES), c.m_racine, c.m_racine / "r3").Ok());
    // zone hors de l'image, une seule classe
    VERIFIER(!Lancer(ReqEchantillons(c.m_image, "Lab", R"([{"classe":"a","x":100,"y":0,"largeur":40,"hauteur":10},
                                                         {"classe":"b","x":0,"y":0,"largeur":4,"hauteur":4}])"),
                     c.m_racine, c.m_racine / "r4").Ok());
    VERIFIER(!ValiderRequete(Json(ReqEchantillons(c.m_image, "Lab", R"([{"classe":"a","x":0,"y":0,"largeur":4,"hauteur":4}])")),
                             c.m_racine).Ok());
    VERIFIER(!fs::exists(c.m_racine / "r1" / "segmentation.json"));
}

int main(int argc, char** argv) { return LancerTests(argc, argv); }
