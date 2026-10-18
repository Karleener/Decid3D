// Tests de l'operation "generer" (Generation.cpp) : determinisme par graine, statistiques des gaussiennes,
// geometrie des spirales, refus des requetes invalides, fichiers publies et relecture par LireCsv.
#include <cmath>
#include <string>
#include <vector>

#include "Donnees.h"
#include "Fichiers.h"
#include "Generation.h"
#include "Json.h"
#include "Operations.h"
#include "Sha256.h"
#include "Verifier.h"

using namespace decid;

namespace {

ValeurJson Json(const std::string& texte) {
    auto j = LireJson(texte);
    VERIFIER(j.Ok());
    return j.Ok() ? j.Valeur() : ValeurJson();
}

const char* REQUETE_2D =
    R"({"operation":"generer","protocol_version":1,"request_id":"g2d","modele":"gaussiennes_2d","graine":7,
        "classes":[{"nom":"A","effectif":4000,"centre":[0.25,0.5],"ecart_type":[0.05,0.1]},
                   {"nom":"B","effectif":2000,"centre":[0.75,0.5],"ecart_type":0.02}]})";

RequeteGeneration Lire(const std::string& texte) {
    auto r = RequeteGenerationDepuisJson(Json(texte));
    VERIFIER(r.Ok());
    return r.Ok() ? r.Valeur() : RequeteGeneration();
}

JeuGenere GenererOk(const RequeteGeneration& r) {
    auto jeu = Generer(r, nullptr);
    VERIFIER(jeu.Ok());
    return jeu.Ok() ? jeu.Valeur() : JeuGenere();
}

void Moments(const JeuGenere& jeu, int classe, int axe, double& moyenne, double& ecart) {
    double s = 0, s2 = 0;
    long n = 0;
    for (std::size_t l = 0; l < jeu.m_labels.size(); ++l) {
        if (jeu.m_labels[l] != classe) continue;
        const double x = jeu.m_valeurs[l * (size_t)jeu.m_dimension + (size_t)axe];
        s += x; s2 += x * x; ++n;
    }
    moyenne = s / n;
    ecart = std::sqrt(s2 / n - moyenne * moyenne);
}

} // namespace

TEST(gaussiennes_2d_moments_conformes) {
    JeuGenere jeu = GenererOk(Lire(REQUETE_2D));
    VERIFIER_EGAL(jeu.m_labels.size(), (size_t)6000);
    VERIFIER_EGAL(jeu.m_dimension, 2);
    double m, s;
    Moments(jeu, 0, 0, m, s);
    VERIFIER_PROCHE(m, 0.25, 0.005);
    VERIFIER_PROCHE(s, 0.05, 0.003);
    Moments(jeu, 0, 1, m, s);
    VERIFIER_PROCHE(s, 0.1, 0.005);
    Moments(jeu, 1, 0, m, s);
    VERIFIER_PROCHE(m, 0.75, 0.002);
    VERIFIER_PROCHE(s, 0.02, 0.002);   // ecart-type scalaire = isotrope
}

TEST(meme_graine_memes_valeurs_autre_graine_differente) {
    RequeteGeneration r = Lire(REQUETE_2D);
    JeuGenere a = GenererOk(r), b = GenererOk(r);
    VERIFIER(a.m_valeurs == b.m_valeurs);
    r.m_graine = 8;
    JeuGenere c = GenererOk(r);
    VERIFIER(a.m_valeurs != c.m_valeurs);
}

TEST(gaussiennes_3d_ecretage) {
    JeuGenere jeu = GenererOk(Lire(
        R"({"modele":"gaussiennes_3d","graine":3,"ecretage":{"min":0,"max":1},
            "classes":[{"nom":"c0","effectif":500,"centre":[0.05,0.5,0.95],"ecart_type":0.2}]})"));
    VERIFIER_EGAL(jeu.m_dimension, 3);
    bool dansBornes = true, touche0 = false, touche1 = false;
    for (double x : jeu.m_valeurs) {
        dansBornes = dansBornes && x >= 0 && x <= 1;
        touche0 = touche0 || x == 0.0;
        touche1 = touche1 || x == 1.0;
    }
    VERIFIER(dansBornes);
    VERIFIER(touche0 && touche1);
}

TEST(spirale_sans_bruit_sur_le_ressort) {
    // axe z, diametre 0.4 : distance a l'axe = 0.2 et hauteur = i/n * longueur
    JeuGenere jeu = GenererOk(Lire(
        R"({"modele":"spirales_3d","graine":1,"classes":[{"nom":"s","effectif":100,"centre":[0.5,0.5,0.1],
            "diametre":0.4,"longueur":0.8,"tours":2,"phase":0,"bruit":0,"axe":[0,0,2]}]})"));
    VERIFIER_EGAL(jeu.m_labels.size(), (size_t)100);
    for (int i = 0; i < 100; ++i) {
        const double x = jeu.m_valeurs[(size_t)i * 3] - 0.5, y = jeu.m_valeurs[(size_t)i * 3 + 1] - 0.5;
        VERIFIER_PROCHE(std::sqrt(x * x + y * y), 0.2, 1e-12);
        VERIFIER_PROCHE(jeu.m_valeurs[(size_t)i * 3 + 2], 0.1 + 0.8 * i / 100.0, 1e-12);
    }
    VERIFIER_PROCHE(jeu.m_axes[0][2], 1.0, 1e-15);   // axe normalise
}

TEST(spirale_axe_tire_avec_la_graine) {
    const char* texte = R"({"modele":"spirales_3d","graine":11,"classes":[{"nom":"a","effectif":50,"centre":[0,0,0]},
                                                                         {"nom":"b","effectif":50,"centre":[0,0,0],"phase":3.14159}]})";
    JeuGenere a = GenererOk(Lire(texte)), b = GenererOk(Lire(texte));
    VERIFIER(a.m_axes == b.m_axes);
    VERIFIER_EGAL(a.m_axes.size(), (size_t)2);
    const auto& w = a.m_axes[0];
    VERIFIER_PROCHE(w[0] * w[0] + w[1] * w[1] + w[2] * w[2], 1.0, 1e-12);
}

TEST(requetes_invalides_refusees) {
    const char* invalides[] = {
        R"({"modele":"gaussiennes_4d","classes":[{"nom":"a","centre":[0,0],"ecart_type":1}]})",
        R"({"modele":"gaussiennes_2d","classes":[]})",
        R"({"modele":"gaussiennes_2d","classes":[{"nom":"a","centre":[0,0,0],"ecart_type":1}]})",
        R"({"modele":"gaussiennes_2d","classes":[{"nom":"a","centre":[0,0],"ecart_type":-1}]})",
        R"({"modele":"gaussiennes_2d","classes":[{"nom":"a,b","centre":[0,0],"ecart_type":1}]})",
        R"({"modele":"gaussiennes_2d","classes":[{"nom":"a","centre":[0,0],"ecart_type":1},{"nom":"a","centre":[1,1],"ecart_type":1}]})",
        R"({"modele":"gaussiennes_2d","classes":[{"nom":"a","effectif":0,"centre":[0,0],"ecart_type":1}]})",
        R"({"modele":"spirales_3d","classes":[{"nom":"a","centre":[0,0,0],"axe":[0,0,0]}]})",
        R"({"modele":"gaussiennes_2d","ecretage":{"min":1,"max":0},"classes":[{"nom":"a","centre":[0,0],"ecart_type":1}]})",
    };
    for (const char* t : invalides) VERIFIER(!RequeteGenerationDepuisJson(Json(t)).Ok());
}

TEST(operation_publie_csv_relisible_et_description) {
    const fs::path staging = verif::DossierTemporaire("generation") / "staging";
    std::atomic<bool> annulation{false};
    Statut st = ExecuterOperation(Json(REQUETE_2D), staging.parent_path(), staging, annulation, nullptr);
    VERIFIER(st.Ok());
    auto texte = LireFichierTexte(staging / "generation.json");
    VERIFIER(texte.Ok());
    if (!texte.Ok()) return;
    ValeurJson d = Json(texte.Valeur());
    VERIFIER_EGAL(d.Membre("schema").Texte(), std::string("decid.generation/1"));
    VERIFIER_EGAL(d.Membre("nb_lignes").Entier(), 6000LL);
    VERIFIER_EGAL(d.Membre("sha256_csv").Texte(), Sha256Fichier(staging / "generation.csv"));
    VERIFIER(fs::exists(staging / "artefacts.json"));
    ContratCsv contrat;
    auto jeu = LireCsv(staging / "generation.csv", contrat);
    VERIFIER(jeu.Ok());
    if (!jeu.Ok()) return;
    VERIFIER_EGAL(jeu.Valeur().m_nbLignes, 6000);
    VERIFIER_EGAL(jeu.Valeur().NbClasses(), 2);
    // relecture exacte (17 chiffres) : premiere valeur identique au tirage en memoire
    JeuGenere memoire = GenererOk(Lire(REQUETE_2D));
    VERIFIER_EGAL(jeu.Valeur().Valeur(0, 0), memoire.m_valeurs[0]);
    VERIFIER_EGAL(jeu.Valeur().Valeur(5999, 1), memoire.m_valeurs[5999 * 2 + 1]);
}

TEST(validation_orchestrateur) {
    VERIFIER(ValiderRequete(Json(REQUETE_2D), fs::current_path()).Ok());
    VERIFIER(!ValiderRequete(Json(R"({"operation":"generer","protocol_version":1,"request_id":"x","modele":"?"})"),
                             fs::current_path()).Ok());
}

int main(int argc, char** argv) { return LancerTests(argc, argv); }
