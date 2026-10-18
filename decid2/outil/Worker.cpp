// Orchestrateur et worker. Un seul proprietaire du commit : l'orchestrateur.
// Visibilite d'un resultat = commit.json present ET coherent (ecrit en dernier).
#include "Worker.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "Experience.h"
#include "Operations.h"
#include "Fichiers.h"
#include "Json.h"
#include "Sha256.h"

namespace decid {

namespace {

long long MillisecondesMonotones() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// ---------- worker (enfant) ----------

std::mutex g_sortie;
long g_sequence = 0;

void Emettre(ValeurJson evenement) {
    std::lock_guard<std::mutex> verrou(g_sortie);
    evenement["seq"] = ++g_sequence;
    evenement["utc"] = HorodatageUtc();
    evenement["mono_ms"] = MillisecondesMonotones();
    std::string ligne = evenement.Ecrire(false) + "\n";
    std::fwrite(ligne.data(), 1, ligne.size(), stdout);
    std::fflush(stdout);
}

ValeurJson ErreurVersJson(const Erreur& e) {
    ValeurJson j = ValeurJson::Objet();
    j["code"] = Erreur::NomCode(e.m_code);
    j["phase"] = e.m_phase;
    j["message"] = e.m_message;
    j["fichier"] = e.m_fichier;
    j["ligne"] = e.m_ligne;
    j["colonne"] = e.m_colonne;
    j["reprise_possible"] = e.m_reprisePossible;
    return j;
}

// Crochets de test documentes (aucun effet hors tests) :
//   DECID_TEST_PAUSE=<phase>:<ms>   pause dans la phase indiquee (tests d'annulation)
//   DECID_TEST_CRASH=<phase>        arret brutal du processus dans la phase indiquee
void CrochetsDeTest(const std::string& phase) {
    if (const char* pause = std::getenv("DECID_TEST_PAUSE")) {
        std::string v(pause);
        size_t sep = v.find(':');
        if (sep != std::string::npos && v.substr(0, sep) == phase)
            std::this_thread::sleep_for(std::chrono::milliseconds(std::atol(v.c_str() + sep + 1)));
    }
    if (const char* crash = std::getenv("DECID_TEST_CRASH"))
        if (phase == crash) std::_Exit(99);
}

} // namespace

int ExecuterWorker(const fs::path& requete, const fs::path& staging, const fs::path& fichierAnnulation) {
    auto fin = [](const char* statut, const Erreur* e) {
        ValeurJson ev = ValeurJson::Objet();
        ev["type"] = "fin";
        ev["statut"] = statut;
        if (e) ev["erreur"] = ErreurVersJson(*e);
        Emettre(ev);
    };
    auto texte = LireFichierTexte(requete);
    if (!texte.Ok()) { fin("failed", &texte.Err()); return 2; }
    auto json = LireJson(texte.Valeur());
    if (!json.Ok()) { fin("failed", &json.Err()); return 2; }
    const std::string operation = json.Valeur().Membre("operation").Texte();
    const bool experience = operation.empty() || operation == "experience";
    Resultat<RequeteExperience> req = experience ? RequeteDepuisJson(json.Valeur(), requete.parent_path())
                                                 : Resultat<RequeteExperience>(RequeteExperience{});
    if (!req.Ok()) { fin("failed", &req.Err()); return 2; }

    // Surveillance du fichier d'annulation dans un thread possede (joint avant la sortie)
    std::atomic<bool> annulation{false}, arret{false};
    std::thread surveillant([&] {
        while (!arret.load()) {
            std::error_code ec;
            if (fs::exists(fichierAnnulation, ec)) { annulation.store(true); break; }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    auto rapporter = [](const std::string& phase, long i, long n, const std::string& detail) {
        ValeurJson ev = ValeurJson::Objet();
        ev["type"] = "progression";
        ev["phase"] = phase;
        ev["i"] = i;
        ev["n"] = n;                      // n = 0 : avancement indetermine
        ev["detail"] = detail;
        Emettre(ev);
        CrochetsDeTest(phase);
    };
    Statut resultat = experience ? ExecuterExperience(req.Valeur(), staging, annulation, rapporter)
                                 : ExecuterOperation(json.Valeur(), requete.parent_path(), staging, annulation, rapporter);
    arret.store(true);
    surveillant.join();

    if (resultat.Ok()) { fin("succeeded", nullptr); return 0; }
    bool annule = resultat.Err().m_code == CodeErreur::Annulation;
    fin(annule ? "cancelled" : "failed", &resultat.Err());
    return annule ? 3 : 2;
}

// ---------- orchestrateur (parent) ----------

std::string EtatTache(const fs::path& dossierTache) {
    auto texte = LireFichierTexte(dossierTache / "etat.json");
    if (texte.Ok()) {
        auto json = LireJson(texte.Valeur());
        if (json.Ok()) {
            std::string etat = json.Valeur().Membre("etat").Texte();
            if (etat != "succeeded") return etat;
            // un succes n'est visible que si le commit est present et coherent
            auto commit = LireFichierTexte(dossierTache / "commit.json");
            if (!commit.Ok()) return "interrupted";
            auto jc = LireJson(commit.Valeur());
            if (!jc.Ok()) return "interrupted";
            std::string hash = Sha256Fichier(dossierTache / "resultat" / "artefacts.json");
            return hash == jc.Valeur().Membre("sha256_artefacts").Texte() ? "succeeded" : "interrupted";
        }
    }
    return "interrupted";
}

#ifdef _WIN32

namespace {

std::wstring Guillemets(const std::wstring& s) { return L"\"" + s + L"\""; }

struct Poignee {
    HANDLE m_h = nullptr;
    ~Poignee() { if (m_h && m_h != INVALID_HANDLE_VALUE) CloseHandle(m_h); }
};

std::atomic<bool>* g_annulationConsole = nullptr;
BOOL WINAPI GestionnaireConsole(DWORD type) {
    if ((type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT) && g_annulationConsole) {
        g_annulationConsole->store(true);
        return TRUE;
    }
    return FALSE;
}

} // namespace

int Orchestrer(const fs::path& executable, const std::wstring& prefixe, const fs::path& requete, const fs::path& dossierTaches,
               double delaiArretSecondes) {
    auto echouer = [](const Erreur& e) { std::fprintf(stderr, "%s\n", e.Texte().c_str()); return 2; };
    auto texte = LireFichierTexte(requete);
    if (!texte.Ok()) return echouer(texte.Err());
    auto json = LireJson(texte.Valeur());
    if (!json.Ok()) return echouer(json.Err());
    // L'orchestrateur ne lit que l'identite de la requete ; le worker valide le reste (et echoue proprement).
    const std::string idRequete = json.Valeur().Membre("request_id").Texte();
    if (json.Valeur().Membre("protocol_version").Entier() != 1 || idRequete.empty() ||
        idRequete.find_first_of("/\\:*?\"<>|") != std::string::npos || idRequete == "." || idRequete == "..")
        return echouer(FaireErreur(CodeErreur::Protocole, "orchestration",
            "requete invalide : protocol_version 1 et request_id simple (sans separateur de chemin) requis"));

    auto validation = ValiderRequete(json.Valeur(), requete.parent_path());   // avant toute creation
    if (!validation.Ok()) return echouer(validation.Err());

    fs::path tache = dossierTaches / CheminDepuisUtf8(idRequete);
    std::error_code ec;
    if (fs::exists(tache, ec))
        return echouer(FaireErreur(CodeErreur::EcritureRefusee, "orchestration",
            "dossier de tache deja existant : une tache publiee ou interrompue n'est jamais reecrite, choisir un autre request_id",
            CheminUtf8(tache)));
    fs::create_directories(tache, ec);
    if (ec) return echouer(FaireErreur(CodeErreur::EcritureRefusee, "orchestration", "dossier de tache non creable", CheminUtf8(tache)));
    fs::path staging = tache / "staging";
    fs::path fichierAnnulation = tache / "annuler";
    fs::path journal = tache / "evenements.jsonl";

    // Copie figee de la requete
    auto copie = EcrireFichierAtomique(tache / "requete.json", texte.Valeur(), false);
    if (!copie.Ok()) return echouer(copie.Err());

    std::atomic<bool> annulationConsole{false};
    g_annulationConsole = &annulationConsole;
    SetConsoleCtrlHandler(GestionnaireConsole, TRUE);

    // Tuyau stdout du worker
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    Poignee lecture, ecriture;
    if (!CreatePipe(&lecture.m_h, &ecriture.m_h, &sa, 0))
        return echouer(FaireErreur(CodeErreur::CrashWorker, "orchestration", "creation du tuyau impossible"));
    SetHandleInformation(lecture.m_h, HANDLE_FLAG_INHERIT, 0);

    // Job Object : tout le groupe de processus meurt avec la tache
    Poignee job;
    job.m_h = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limites{};
    limites.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(job.m_h, JobObjectExtendedLimitInformation, &limites, sizeof(limites));

    std::wstring ligneCommande = Guillemets(executable.wstring()) + (prefixe.empty() ? L"" : L" " + prefixe) + L" worker --requete " +
        Guillemets(fs::absolute(requete).wstring()) + L" --staging " + Guillemets(fs::absolute(staging).wstring()) +
        L" --annulation " + Guillemets(fs::absolute(fichierAnnulation).wstring());
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = ecriture.m_h;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> tampon(ligneCommande.begin(), ligneCommande.end());
    tampon.push_back(L'\0');
    if (!CreateProcessW(nullptr, tampon.data(), nullptr, nullptr, TRUE, CREATE_SUSPENDED | CREATE_NO_WINDOW,
                        nullptr, nullptr, &si, &pi))
        return echouer(FaireErreur(CodeErreur::CrashWorker, "orchestration", "lancement du worker impossible"));
    Poignee processus, thread;
    processus.m_h = pi.hProcess;
    thread.m_h = pi.hThread;
    AssignProcessToJobObject(job.m_h, pi.hProcess);
    ResumeThread(pi.hThread);
    CloseHandle(ecriture.m_h);
    ecriture.m_h = nullptr;

    // Lecture des evenements dans un thread possede ; journal renumerote par l'orchestrateur
    std::mutex verrou;
    std::string dernierStatut;
    ValeurJson derniereErreur;
    std::thread lecteur([&] {
        FILE* f = _wfopen(journal.wstring().c_str(), L"ab");
        std::string accumule;
        char bloc[4096];
        DWORD lus = 0;
        long sequence = 0;
        while (ReadFile(lecture.m_h, bloc, sizeof(bloc), &lus, nullptr) && lus > 0) {
            accumule.append(bloc, lus);
            size_t pos;
            while ((pos = accumule.find('\n')) != std::string::npos) {
                std::string ligne = accumule.substr(0, pos);
                accumule.erase(0, pos + 1);
                auto ev = LireJson(ligne);
                if (!ev.Ok()) continue;                      // ligne non protocolaire ignoree
                ValeurJson e = ev.Valeur();
                e["seq_orchestrateur"] = ++sequence;
                std::string sortie = e.Ecrire(false) + "\n";
                if (f) { std::fwrite(sortie.data(), 1, sortie.size(), f); std::fflush(f); }
                std::printf("%s", sortie.c_str());
                std::fflush(stdout);
                if (e.Membre("type").Texte() == "fin") {
                    std::lock_guard<std::mutex> v(verrou);
                    dernierStatut = e.Membre("statut").Texte();
                    derniereErreur = e.Membre("erreur");
                }
            }
        }
        if (f) std::fclose(f);
    });

    // Attente avec surveillance de l'annulation et delai de grace
    bool annulationDemandee = false, arretForce = false;
    long long debutGrace = 0;
    for (;;) {
        DWORD r = WaitForSingleObject(pi.hProcess, 100);
        if (r == WAIT_OBJECT_0) break;
        if (!annulationDemandee && (annulationConsole.load() || fs::exists(fichierAnnulation, ec))) {
            annulationDemandee = true;
            debutGrace = MillisecondesMonotones();
            EcrireFichierAtomique(fichierAnnulation, std::string("annulation demandee\n"), true);
        }
        if (annulationDemandee && MillisecondesMonotones() - debutGrace > (long long)(delaiArretSecondes * 1000)) {
            TerminateJobObject(job.m_h, 3);   // l'appel OpenCV opaque ne s'interrompt pas : arret du groupe
            arretForce = true;
            WaitForSingleObject(pi.hProcess, INFINITE);
            break;
        }
    }
    DWORD codeSortie = 0;
    GetExitCodeProcess(pi.hProcess, &codeSortie);
    lecteur.join();
    SetConsoleCtrlHandler(GestionnaireConsole, FALSE);
    g_annulationConsole = nullptr;

    // Decision terminale unique
    std::string etat;
    ValeurJson diagnostic = ValeurJson::Objet();
    if (arretForce || annulationDemandee || dernierStatut == "cancelled") {
        etat = "cancelled";
        diagnostic["raison"] = arretForce ? "arret force apres le delai de grace" : "annulation cooperative";
    } else if (codeSortie == 0 && dernierStatut == "succeeded") {
        auto verification = VerifierStaging(staging);
        if (!verification.Ok()) {
            etat = "failed";
            diagnostic["raison"] = verification.Err().Texte();
        } else {
            // Publication : renommage du staging (meme volume), puis commit.json en dernier
            fs::path resultat = tache / "resultat";
            if (!MoveFileExW(staging.wstring().c_str(), resultat.wstring().c_str(), MOVEFILE_WRITE_THROUGH)) {
                etat = "failed";
                diagnostic["raison"] = "renommage staging -> resultat refuse (fichier verrouille ?)";
            } else {
                ValeurJson commit = ValeurJson::Objet();
                commit["schema"] = "decid.commit/1";
                commit["request_id"] = idRequete;
                commit["utc"] = HorodatageUtc();
                commit["dossier"] = "resultat";
                commit["sha256_artefacts"] = Sha256Fichier(resultat / "artefacts.json");
                auto ecrit = EcrireFichierAtomique(tache / "commit.json", commit.Ecrire(), false);
                etat = ecrit.Ok() ? "succeeded" : "failed";
                if (!ecrit.Ok()) diagnostic["raison"] = ecrit.Err().Texte();
            }
        }
    } else {
        etat = "failed";
        diagnostic["raison"] = dernierStatut.empty() ? "worker termine sans evenement de fin (crash)" : "echec du calcul";
        diagnostic["code_sortie_worker"] = (long long)codeSortie;
        if (!derniereErreur.EstNul()) diagnostic["erreur"] = derniereErreur;
    }
    ValeurJson etatJson = ValeurJson::Objet();
    etatJson["schema"] = "decid.task-state/1";
    etatJson["etat"] = etat;
    etatJson["utc"] = HorodatageUtc();
    etatJson["diagnostic"] = diagnostic;
    auto ecritEtat = EcrireFichierAtomique(tache / "etat.json", etatJson.Ecrire(), false);   // une seule fin
    if (!ecritEtat.Ok()) std::fprintf(stderr, "%s\n", ecritEtat.Err().Texte().c_str());
    ValeurJson sortie = ValeurJson::Objet();
    sortie["etat"] = etat;
    sortie["tache"] = CheminUtf8(tache);
    std::printf("%s\n", sortie.Ecrire(false).c_str());
    return etat == "succeeded" ? 0 : etat == "cancelled" ? 3 : 2;
}

#else

int Orchestrer(const fs::path&, const std::wstring&, const fs::path&, const fs::path&, double) {
    std::fprintf(stderr, "orchestrateur non disponible sur cette plateforme\n");
    return 2;
}

#endif

} // namespace decid
