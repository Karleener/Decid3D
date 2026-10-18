// Tests de l'operation "regression" (Regression.cpp) : metriques exactes, quatre familles sur une relation connue,
// fichiers publies (predictions, courbe, modele relu), determinisme, refus explicites.
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "Fichiers.h"
#include "Json.h"
#include "Operations.h"
#include "Regression.h"
#include "Verifier.h"

using namespace decid;

namespace {

ValeurJson Json(const std::string& texte) {
    auto j = LireJson(texte);
    VERIFIER(j.Ok());
    return j.Ok() ? j.Valeur() : ValeurJson();
}

void EcrireTexte(const fs::path& chemin, const std::string& texte) {
    std::ofstream f(chemin, std::ios::binary);
    f.write(texte.data(), (std::streamsize)texte.size());
}

// y = 2 x1 + sin(3 x2) + bruit +-0.02, x dans [0,1] ; cible en premiere colonne (format Decid)
fs::path CsvConnu(const fs::path& dossier) {
    std::string csv;
    std::uint64_t etat = 99;
    auto alea = [&etat]() { etat = etat * 6364136223846793005ULL + 1442695040888963407ULL; return (double)(etat >> 11) / 9007199254740992.0; };
    char tampon[128];
    for (int i = 0; i < 400; ++i) {
        const double x1 = alea(), x2 = alea();
        std::snprintf(tampon, sizeof(tampon), "%.9f,%.9f,%.9f\n", 2 * x1 + std::sin(3 * x2) + (alea() - 0.5) * 0.04, x1, x2);
        csv += tampon;
    }
    const fs::path p = dossier / "connu.csv";
    EcrireTexte(p, csv);
    return p;
}

std::string Requete(const fs::path& csv, const std::string& algorithme, const std::string& id) {
    std::string chemin = CheminUtf8(csv);
    for (char& c : chemin) if (c == '\\') c = '/';
    return R"({"operation":"regression","protocol_version":1,"request_id":")" + id +
           R"(","source":{"type":"csv","chemin":")" + chemin +
           R"(","en_tete":"non","colonne_cible":0},"partage":{"methode":"aleatoire","ratio":0.7,"graine":5},
              "normalisation":{"methode":"minmax"},"algorithme":)" + algorithme + R"(,"courbe":{"attribut":0,"points":50}})";
}

ValeurJson Executer(const std::string& requete, const fs::path& staging) {
    std::atomic<bool> annulation{false};
    Statut st = ExecuterOperation(Json(requete), staging.parent_path(), staging, annulation, nullptr);
    VERIFIER(st.Ok());
    if (!st.Ok()) { std::printf("  %s\n", st.Err().m_message.c_str()); return ValeurJson(); }
    auto t = LireFichierTexte(staging / "regression.json");
    VERIFIER(t.Ok());
    return t.Ok() ? Json(t.Valeur()) : ValeurJson();
}

int CompterLignes(const fs::path& p) {
    auto t = LireFichierTexte(p);
    if (!t.Ok()) return -1;
    int n = 0;
    for (char c : t.Valeur()) n += c == '\n';
    return n;
}

} // namespace

TEST(metriques_exactes) {
    MetriquesRegression m = CalculerMetriques({1, 2, 3}, {1, 2, 4});
    VERIFIER_EGAL(m.m_n, 3L);
    VERIFIER_PROCHE(m.m_eqm, 1.0 / 3, 1e-15);
    VERIFIER_PROCHE(m.m_mae, 1.0 / 3, 1e-15);
    VERIFIER(m.m_r2Defini);
    VERIFIER_PROCHE(m.m_r2, 0.5, 1e-15);    // SSres = 1, SStot = 2
    MetriquesRegression constante = CalculerMetriques({2, 2}, {1, 3});
    VERIFIER(!constante.m_r2Defini);
    VERIFIER(constante.VersJson().Membre("r2").EstNul());
    VERIFIER_EGAL(CalculerMetriques({}, {}).m_n, 0L);
}

TEST(quatre_familles_apprennent_la_relation) {
    const fs::path dossier = verif::DossierTemporaire("regression_familles");
    const fs::path csv = CsvConnu(dossier);
    const char* algos[] = {
        R"({"famille":"svm","noyau":"RBF","c":10,"gamma":2,"epsilon":0.01})",
        R"({"famille":"mlp","couches_cachees":[12],"iterations_max":500})",
        R"({"famille":"foret_aleatoire","nb_arbres":60,"profondeur_max":12})",
        R"({"famille":"kppv","k":5})",
    };
    int i = 0;
    for (const char* a : algos) {
        ValeurJson d = Executer(Requete(csv, a, "r" + std::to_string(i)), dossier / ("s" + std::to_string(i)));
        const ValeurJson& test = d.Membre("metriques").Membre("test");
        VERIFIER_EGAL(test.Membre("n").Entier(), 120LL);
        VERIFIER(test.Membre("r2").Nombre() > 0.9);
        VERIFIER(test.Membre("mae").Nombre() < 0.15);
        VERIFIER_PROCHE(test.Membre("rmse").Nombre(), std::sqrt(test.Membre("eqm").Nombre()), 1e-12);
        ++i;
    }
}

TEST(fichiers_publies_et_relecture) {
    const fs::path dossier = verif::DossierTemporaire("regression_fichiers");
    const fs::path csv = CsvConnu(dossier);
    const fs::path staging = dossier / "staging";
    ValeurJson d = Executer(Requete(csv, R"({"famille":"foret_aleatoire","nb_arbres":30})", "rf"), staging);
    VERIFIER_EGAL(d.Membre("schema").Texte(), std::string("decid.regression/1"));
    VERIFIER_EGAL(d.Membre("partage").Membre("n_apprentissage").Entier(), 280LL);
    VERIFIER_EGAL(d.Membre("parametres_effectifs").Membre("nb_arbres").Entier(), 30LL);
    VERIFIER_EGAL(CompterLignes(staging / "regression_predictions.csv"), 401);   // en-tete + 400
    auto tc = LireFichierTexte(staging / "regression_courbe.json");
    VERIFIER(tc.Ok());
    if (tc.Ok()) {
        ValeurJson c = Json(tc.Valeur());
        VERIFIER_EGAL(c.Membre("x").Elements().size(), (size_t)50);
        VERIFIER_EGAL(c.Membre("y").Elements().size(), (size_t)50);
        VERIFIER_EGAL(c.Membre("points").Elements().size(), (size_t)400);
        // la coupe le long de x1 doit croitre (pente 2) entre ses extremites
        VERIFIER(c.Membre("y").Elements().back().Nombre() - c.Membre("y").Elements().front().Nombre() > 1.0);
    }
    VERIFIER(fs::exists(staging / "regression_modele.yml"));
    VERIFIER(fs::exists(staging / "artefacts.json"));
    VERIFIER(!fs::exists(staging / "modele.json"));   // pas confondu avec une experience de classification
}

TEST(meme_requete_memes_metriques) {
    const fs::path dossier = verif::DossierTemporaire("regression_determinisme");
    const fs::path csv = CsvConnu(dossier);
    const std::string algo = R"({"famille":"mlp","couches_cachees":[6],"iterations_max":200})";
    ValeurJson a = Executer(Requete(csv, algo, "m1"), dossier / "a");
    ValeurJson b = Executer(Requete(csv, algo, "m2"), dossier / "b");
    VERIFIER_EGAL(a.Membre("metriques").Ecrire(), b.Membre("metriques").Ecrire());
    VERIFIER_EGAL(a.Membre("partage").Membre("hash").Texte(), b.Membre("partage").Membre("hash").Texte());
}

TEST(cible_non_numerique_refusee) {
    const fs::path dossier = verif::DossierTemporaire("regression_cible");
    EcrireTexte(dossier / "classes.csv", "a,0.1\nb,0.2\na,0.3\n");
    std::atomic<bool> annulation{false};
    Statut st = ExecuterOperation(Json(Requete(dossier / "classes.csv", R"({"famille":"kppv","k":1})", "c")), dossier,
                                  dossier / "staging", annulation, nullptr);
    VERIFIER(!st.Ok());
    VERIFIER(!fs::exists(dossier / "staging" / "regression.json"));
}

TEST(parametres_invalides_refuses) {
    const fs::path csv = fs::current_path() / "absent.csv";
    const char* algos[] = {
        R"({"famille":"em"})", R"({"famille":"svm","c":0})", R"({"famille":"mlp","activation":"RELU"})",
        R"({"famille":"mlp","couches_cachees":[]})", R"({"famille":"kppv","k":0})", R"({"famille":"foret_aleatoire","nb_arbres":0})",
    };
    for (const char* a : algos) VERIFIER(!ValiderRequete(Json(Requete(csv, a, "x")), fs::current_path()).Ok());
    VERIFIER(ValiderRequete(Json(Requete(csv, R"({"famille":"kppv","k":3})", "x")), fs::current_path()).Ok());
    std::string ratio = Requete(csv, R"({"famille":"kppv"})", "x");
    ratio.replace(ratio.find("0.7"), 3, "0.0");
    VERIFIER(!ValiderRequete(Json(ratio), fs::current_path()).Ok());
}

int main(int argc, char** argv) { return LancerTests(argc, argv); }
