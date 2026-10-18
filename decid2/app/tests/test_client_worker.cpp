// ClientWorker : vraie petite experience via DecidMoteur.exe sur un CSV synthetique (evenements + etat succeeded),
// annulation (DECID_TEST_PAUSE=apprentissage:3000 -> cancelled), refus d'une seconde tache lourde, arret propre.
// Usage : test_client_worker <DecidMoteur.exe>
#include <chrono>
#include <fstream>
#include <string>

#include <windows.h>

#include "ClientWorker.h"
#include "Fichiers.h"
#include "Projet.h"
#include "Verifier.h"

using namespace app;
using namespace std::chrono_literals;

namespace {

class VarEnv {
public:
    VarEnv(const wchar_t* nom, const wchar_t* valeur) : m_nom(nom) { SetEnvironmentVariableW(nom, valeur); }
    ~VarEnv() { SetEnvironmentVariableW(m_nom, nullptr); }
    VarEnv(const VarEnv&) = delete;
    VarEnv& operator=(const VarEnv&) = delete;
private:
    const wchar_t* m_nom;
};

std::string CsvSynthetique() {
    std::uint64_t e = 99;
    auto suivant = [&]() {
        e = e * 6364136223846793005ULL + 1442695040888963407ULL;
        return (double)(e >> 11) / 9007199254740992.0;
    };
    std::string t;
    for (int i = 0; i < 200; ++i) {
        const int c = i % 2;
        t += std::to_string(c) + "," + std::to_string(c * 2.0 + suivant() + suivant() - 1.0) + "," +
             std::to_string(c * 2.0 + suivant() + suivant() - 1.0) + "\n";
    }
    return t;
}

struct Banc {
    fs::path m_racine;
    Projet m_projet;
    std::string m_id;
    fs::path m_requete;
};

decid::ValeurJson RequeteSvm() { return decid::ValeurJson::Objet(); }

Banc Preparer(const std::string& nom) {
    const fs::path racine = verif::DossierTemporaire(nom);
    {
        std::ofstream f(racine / "donnees.csv", std::ios::binary);
        f << CsvSynthetique();
    }
    auto p = Projet::Creer(racine / "projet", nom);
    Banc b{racine, std::move(p.Valeur()), "", ""};
    b.m_id = b.m_projet.NouvelIdTache("experience", "svm");
    decid::ValeurJson r = RequeteSvm();
    decid::ValeurJson s = decid::ValeurJson::Objet();
    s["type"] = "csv";
    s["chemin"] = decid::CheminUtf8(racine / "donnees.csv");
    s["en_tete"] = "non";
    r["source"] = s;
    decid::ValeurJson pa = decid::ValeurJson::Objet();
    pa["methode"] = "premiers_n_par_classe";
    pa["ratio"] = 0.5;
    pa["graine"] = 1;
    r["partage"] = pa;
    decid::ValeurJson n = decid::ValeurJson::Objet();
    n["methode"] = "minmax";
    r["normalisation"] = n;
    decid::ValeurJson a = decid::ValeurJson::Objet();
    a["famille"] = "svm";
    a["noyau"] = "RBF";
    a["c"] = 1;
    a["gamma"] = 1;
    r["algorithme"] = a;
    r["threads"] = 1;
    b.m_requete = b.m_projet.EcrireRequete(b.m_id, r).Valeur();
    return b;
}

fs::path Exe() { return fs::path(verif::Source()); }

} // namespace

TEST(experience_reelle_evenements_et_etat_succeeded) {
    Banc b = Preparer("cw_succes");
    FileEvenements file;
    ClientWorker w(Exe(), file.VersDispatcheur());
    std::vector<EvenementWorker> evenements;
    std::vector<FinTache> fins;
    w.AbonnerProgression([&](const std::string& id, const EvenementWorker& e) {
        VERIFIER_EGAL(id, b.m_id);
        evenements.push_back(e);
    });
    w.AbonnerFin([&](const FinTache& f) { fins.push_back(f); });

    auto lancee = w.Lancer(b.m_requete, b.m_projet.DossierTaches());
    VERIFIER(lancee.Ok());
    if (!lancee.Ok()) {
        std::printf("  %s\n", lancee.Err().Texte().c_str());
        return;
    }
    VERIFIER_EGAL(lancee.Valeur(), b.m_id);
    VERIFIER(file.PomperJusqua([&] { return !fins.empty(); }, 60s));
    VERIFIER_EGAL(fins.size(), (size_t)1);
    if (fins.empty()) return;
    if (fins[0].m_etat != "succeeded") std::printf("  raison : %s\n", fins[0].m_raison.c_str());
    VERIFIER_EGAL(fins[0].m_etat, std::string("succeeded"));
    VERIFIER(!evenements.empty());
    bool phaseNommee = false;
    for (const auto& e : evenements) phaseNommee = phaseNommee || !e.m_phase.empty();
    VERIFIER(phaseNommee);
    VERIFIER(fs::exists(b.m_projet.DossierTache(b.m_id) / "commit.json"));
    VERIFIER(!w.EnCours());
    // les taches listees par le projet voient l'etat
    const auto taches = b.m_projet.ListerTaches();
    VERIFIER_EGAL(taches.size(), (size_t)1);
    if (!taches.empty()) VERIFIER_EGAL(taches[0].m_etat, std::string("succeeded"));
}

TEST(annulation_donne_cancelled_et_une_seule_tache_lourde) {
    VarEnv pause(L"DECID_TEST_PAUSE", L"apprentissage:3000");
    Banc b = Preparer("cw_annulation");
    FileEvenements file;
    ClientWorker w(Exe(), file.VersDispatcheur());
    bool phaseApprentissage = false;
    std::vector<FinTache> fins;
    w.AbonnerProgression([&](const std::string&, const EvenementWorker& e) {
        if (e.m_phase == "apprentissage") phaseApprentissage = true;
    });
    w.AbonnerFin([&](const FinTache& f) { fins.push_back(f); });

    VERIFIER(w.Lancer(b.m_requete, b.m_projet.DossierTaches()).Ok());
    VERIFIER(w.EnCours());
    VERIFIER(w.LourdeEnCours());
    // une seconde tache lourde est refusee tant que la premiere tourne
    const std::string id2 = b.m_projet.NouvelIdTache("experience", "svm");
    auto texte = decid::LireFichierTexte(b.m_requete);
    VERIFIER(texte.Ok());
    if (!texte.Ok()) return;
    auto j = decid::LireJson(texte.Valeur());
    VERIFIER(j.Ok());
    if (!j.Ok()) return;
    auto req2 = b.m_projet.EcrireRequete(id2, j.Valeur());
    VERIFIER(req2.Ok());
    if (!req2.Ok()) return;
    VERIFIER(!w.Lancer(req2.Valeur(), b.m_projet.DossierTaches()).Ok());

    VERIFIER(file.PomperJusqua([&] { return phaseApprentissage; }, 60s));
    w.Annuler(b.m_id);
    VERIFIER(file.PomperJusqua([&] { return !fins.empty(); }, 60s));
    if (fins.empty()) return;
    VERIFIER_EGAL(fins[0].m_etat, std::string("cancelled"));
    VERIFIER(!w.EnCours());
}

TEST(arret_propre_avec_tache_en_cours) {
    VarEnv pause(L"DECID_TEST_PAUSE", L"apprentissage:20000");
    Banc b = Preparer("cw_arret");
    FileEvenements file;
    bool phase = false;
    {
        ClientWorker w(Exe(), file.VersDispatcheur());
        w.AbonnerProgression([&](const std::string&, const EvenementWorker& e) { phase = phase || e.m_phase == "apprentissage"; });
        VERIFIER(w.Lancer(b.m_requete, b.m_projet.DossierTaches()).Ok());
        VERIFIER(file.PomperJusqua([&] { return phase; }, 60s));
        const auto debut = std::chrono::steady_clock::now();
        w.Arreter(1500ms);   // annule, attend 1,5 s, puis arrete l'orchestrateur ; ne doit pas durer 20 s
        const auto duree = std::chrono::steady_clock::now() - debut;
        VERIFIER(duree < 15s);
        VERIFIER(!w.EnCours());
    }   // le destructeur ne bloque pas non plus
}

TEST(requete_illisible_ou_executable_absent_refuses) {
    Banc b = Preparer("cw_erreurs");
    FileEvenements file;
    {
        ClientWorker w(Exe(), file.VersDispatcheur());
        VERIFIER(!w.Lancer(b.m_racine / "absente.json", b.m_projet.DossierTaches()).Ok());
    }
    {
        ClientWorker w(b.m_racine / "pas-de-moteur.exe", file.VersDispatcheur());
        VERIFIER(!w.Lancer(b.m_requete, b.m_projet.DossierTaches()).Ok());
    }
}

int main(int argc, char** argv) { return LancerTests(argc, argv); }
