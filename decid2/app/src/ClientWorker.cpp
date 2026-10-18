#include "ClientWorker.h"

#include <algorithm>
#include <fstream>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "Fichiers.h"
#include "Json.h"

namespace app {

using decid::CodeErreur;
using decid::FaireErreur;
using decid::Resultat;
using decid::ValeurJson;

// ------------------------------------------------------------------ FileEvenements

void FileEvenements::Poster(std::function<void()> action) {
    {
        std::lock_guard<std::mutex> v(m_verrou);
        m_actions.push_back(std::move(action));
    }
    m_cv.notify_all();
}

size_t FileEvenements::Pomper() {
    size_t n = 0;
    for (;;) {
        std::function<void()> action;
        {
            std::lock_guard<std::mutex> v(m_verrou);
            if (m_actions.empty()) break;
            action = std::move(m_actions.front());
            m_actions.pop_front();
        }
        action();
        ++n;
    }
    return n;
}

bool FileEvenements::PomperJusqua(const std::function<bool()>& condition, std::chrono::milliseconds delai) {
    const auto fin = std::chrono::steady_clock::now() + delai;
    for (;;) {
        Pomper();
        if (condition()) return true;
        if (std::chrono::steady_clock::now() >= fin) return false;
        std::unique_lock<std::mutex> v(m_verrou);
        m_cv.wait_for(v, std::chrono::milliseconds(20), [&] { return !m_actions.empty(); });
    }
}

Dispatcheur FileEvenements::VersDispatcheur() {
    return [this](std::function<void()> action) { Poster(std::move(action)); };
}

// ------------------------------------------------------------------ ClientWorker

namespace {

struct Poignee {
    HANDLE m_h = nullptr;
    Poignee() = default;
    explicit Poignee(HANDLE h) : m_h(h) {}
    Poignee(const Poignee&) = delete;
    Poignee& operator=(const Poignee&) = delete;
    ~Poignee() { Fermer(); }
    void Fermer() {
        if (m_h && m_h != INVALID_HANDLE_VALUE) CloseHandle(m_h);
        m_h = nullptr;
    }
    HANDLE Detacher() {
        HANDLE h = m_h;
        m_h = nullptr;
        return h;
    }
};

std::wstring Guillemets(const std::wstring& s) { return L"\"" + s + L"\""; }

ValeurJson LireJsonFichier(const fs::path& chemin) {
    auto texte = decid::LireFichierTexte(chemin);
    if (!texte.Ok()) return ValeurJson();
    auto json = decid::LireJson(texte.Valeur());
    return json.Ok() ? json.Valeur() : ValeurJson();
}

} // namespace

struct ClientWorker::Noyau {
    std::atomic<bool> m_ferme{false};
    int m_prochain = 1;
    std::vector<std::pair<int, AbonneProgression>> m_progression;
    std::vector<std::pair<int, AbonneFin>> m_fin;
};

struct ClientWorker::Tache {
    std::string m_id;
    fs::path m_dossier;
    bool m_lourde = true;
    HANDLE m_processus = nullptr;
    HANDLE m_lecture = nullptr;
    std::thread m_fil;
    std::atomic<bool> m_termine{false};
    std::atomic<bool> m_annulationDemandee{false};

    Tache() = default;
    Tache(const Tache&) = delete;
    Tache& operator=(const Tache&) = delete;
    ~Tache() {
        if (m_fil.joinable()) m_fil.join();
        if (m_lecture) CloseHandle(m_lecture);
        if (m_processus) CloseHandle(m_processus);
    }
};

ClientWorker::ClientWorker(fs::path executable, Dispatcheur dispatcheur, std::wstring prefixe)
    : m_executable(std::move(executable)), m_prefixe(std::move(prefixe)), m_dispatcheur(std::move(dispatcheur)),
      m_noyau(std::make_shared<Noyau>()) {}

ClientWorker::~ClientWorker() { Arreter(); }

Resultat<std::string> ClientWorker::Lancer(const fs::path& requete, const fs::path& dossierTaches, bool lourde) {
    Recolter();
    std::error_code ec;
    if (!fs::exists(m_executable, ec))
        return FaireErreur(CodeErreur::CrashWorker, "worker", "moteur de calcul introuvable",
                           decid::CheminUtf8(m_executable));
    const ValeurJson json = LireJsonFichier(requete);
    const std::string id = json.Membre("request_id").Texte();
    if (id.empty())
        return FaireErreur(CodeErreur::Protocole, "worker", "requête illisible ou sans request_id", decid::CheminUtf8(requete));

    {
        std::lock_guard<std::mutex> v(m_verrou);
        int lourdes = 0;
        for (const auto& t : m_taches) {
            if (t->m_termine.load()) continue;
            if (t->m_id == id) return FaireErreur(CodeErreur::Protocole, "worker", "cette tâche est déjà en cours : " + id);
            if (t->m_lourde) ++lourdes;
        }
        if (lourde && lourdes >= m_maxLourdes)
            return FaireErreur(CodeErreur::Ressources, "worker", "une tâche est déjà en cours : attendez sa fin ou annulez-la");
    }

    fs::create_directories(dossierTaches, ec);

    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    Poignee lecture, ecriture, vide;
    HANDLE l = nullptr, e = nullptr;
    if (!CreatePipe(&l, &e, &sa, 0))
        return FaireErreur(CodeErreur::CrashWorker, "worker", "création du tuyau impossible");
    lecture.m_h = l;
    ecriture.m_h = e;
    SetHandleInformation(lecture.m_h, HANDLE_FLAG_INHERIT, 0);
    vide.m_h = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    // seules ces deux poignees sont heritees (pas de fuite vers d'autres processus lances en parallele)
    HANDLE heritees[2] = {ecriture.m_h, vide.m_h};
    SIZE_T tailleListe = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &tailleListe);
    std::vector<char> tamponListe(tailleListe);
    auto liste = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(tamponListe.data());
    if (!InitializeProcThreadAttributeList(liste, 1, 0, &tailleListe) ||
        !UpdateProcThreadAttribute(liste, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, heritees, sizeof(heritees), nullptr, nullptr))
        return FaireErreur(CodeErreur::CrashWorker, "worker", "préparation du lancement impossible");

    STARTUPINFOEXW si{};
    si.StartupInfo.cb = sizeof(si);
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdOutput = ecriture.m_h;
    si.StartupInfo.hStdError = ecriture.m_h;
    si.StartupInfo.hStdInput = vide.m_h;
    si.lpAttributeList = liste;

    std::wstring ligne = Guillemets(fs::absolute(m_executable).wstring()) + (m_prefixe.empty() ? L"" : L" " + m_prefixe) +
                         L" executer --requete " +
                         Guillemets(fs::absolute(requete).wstring()) + L" --taches " +
                         Guillemets(fs::absolute(dossierTaches).wstring());
    std::vector<wchar_t> commande(ligne.begin(), ligne.end());
    commande.push_back(L'\0');
    const std::wstring repertoire = fs::absolute(m_executable).parent_path().wstring();
    PROCESS_INFORMATION pi{};
    const BOOL ok = CreateProcessW(nullptr, commande.data(), nullptr, nullptr, TRUE,
                                   CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, repertoire.c_str(),
                                   &si.StartupInfo, &pi);
    DeleteProcThreadAttributeList(liste);
    if (!ok) return FaireErreur(CodeErreur::CrashWorker, "worker", "lancement du moteur de calcul impossible");
    CloseHandle(pi.hThread);
    ecriture.Fermer();   // seul le processus enfant garde l'extremite d'ecriture

    auto tache = std::make_unique<Tache>();
    tache->m_id = id;
    tache->m_dossier = dossierTaches / decid::CheminDepuisUtf8(id);
    tache->m_lourde = lourde;
    tache->m_processus = pi.hProcess;
    tache->m_lecture = lecture.Detacher();
    Tache* brute = tache.get();
    {
        std::lock_guard<std::mutex> v(m_verrou);
        m_taches.push_back(std::move(tache));
    }
    brute->m_fil = std::thread([this, brute] { Lire(*brute); });
    return id;
}

void ClientWorker::Lire(Tache& tache) {
    auto noyau = m_noyau;
    auto disp = m_dispatcheur;
    auto poster = [noyau, disp](std::function<void(Noyau&)> action) {
        disp([noyau, action = std::move(action)] {
            if (!noyau->m_ferme.load()) action(*noyau);
        });
    };

    std::string accumule, dernierMessage;
    char bloc[4096];
    DWORD lus = 0;
    while (ReadFile(tache.m_lecture, bloc, sizeof(bloc), &lus, nullptr) && lus > 0) {
        accumule.append(bloc, lus);
        size_t pos;
        while ((pos = accumule.find('\n')) != std::string::npos) {
            std::string ligne = accumule.substr(0, pos);
            accumule.erase(0, pos + 1);
            auto ev = decid::LireJson(ligne);
            if (!ev.Ok()) continue;   // ligne non protocolaire
            const ValeurJson& j = ev.Valeur();
            const std::string type = j.Membre("type").Texte();
            if (type == "progression") {
                EvenementWorker evt;
                evt.m_phase = j.Membre("phase").Texte();
                evt.m_i = (long)j.Membre("i").Entier();
                evt.m_n = (long)j.Membre("n").Entier();
                evt.m_detail = j.Membre("detail").Texte();
                const std::string id = tache.m_id;
                poster([id, evt](Noyau& n) {
                    auto copie = n.m_progression;
                    for (auto& a : copie) a.second(id, evt);
                });
            } else if (type == "fin") {
                dernierMessage = j.Membre("erreur").Membre("message").Texte();
            }
        }
    }
    WaitForSingleObject(tache.m_processus, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(tache.m_processus, &code);

    FinTache fin;
    fin.m_id = tache.m_id;
    fin.m_dossier = tache.m_dossier;
    fin.m_codeSortie = (int)code;
    const ValeurJson etat = LireJsonFichier(tache.m_dossier / "etat.json");
    fin.m_etat = etat.Membre("etat").Texte();
    if (!fin.m_etat.empty()) {
        const ValeurJson& d = etat.Membre("diagnostic");
        fin.m_raison = d.Membre("erreur").Membre("message").Texte();
        if (fin.m_raison.empty()) fin.m_raison = d.Membre("raison").Texte();
        if (fin.m_raison.empty()) fin.m_raison = dernierMessage;
    } else if (tache.m_annulationDemandee.load() || code == 3) {
        fin.m_etat = "cancelled";
        fin.m_raison = "annulée avant le démarrage du calcul";
    } else {
        fin.m_etat = "interrupted";
        fin.m_raison = dernierMessage.empty() ? "le moteur de calcul s'est arrêté sans état final (code " + std::to_string((int)code) + ")"
                                              : dernierMessage;
    }
    tache.m_termine.store(true);   // avant l'envoi : un abonne qui interroge EnCours() voit deja la tache terminee
    poster([fin](Noyau& n) {
        auto copie = n.m_fin;
        for (auto& a : copie) a.second(fin);
    });
}

void ClientWorker::Recolter() {
    std::vector<std::unique_ptr<Tache>> finies;
    {
        std::lock_guard<std::mutex> v(m_verrou);
        for (auto it = m_taches.begin(); it != m_taches.end();) {
            if ((*it)->m_termine.load()) {
                finies.push_back(std::move(*it));
                it = m_taches.erase(it);
            } else {
                ++it;
            }
        }
    }
    finies.clear();   // ~Tache joint le thread
}

void ClientWorker::Annuler(const std::string& id) {
    std::lock_guard<std::mutex> v(m_verrou);
    for (auto& t : m_taches) {
        if (t->m_id != id || t->m_termine.load()) continue;
        t->m_annulationDemandee.store(true);
        std::error_code ec;
        if (fs::exists(t->m_dossier, ec)) {
            std::ofstream f(t->m_dossier / "annuler", std::ios::binary);
            f << "annulation demandee\n";
        } else {
            TerminateProcess(t->m_processus, 3);   // orchestrateur encore en validation : rien a perdre
        }
    }
}

void ClientWorker::AnnulerTout() {
    for (const auto& id : TachesEnCours()) Annuler(id);
}

bool ClientWorker::EnCours() const { return !TachesEnCours().empty(); }

bool ClientWorker::LourdeEnCours() const {
    std::lock_guard<std::mutex> v(m_verrou);
    for (const auto& t : m_taches)
        if (!t->m_termine.load() && t->m_lourde) return true;
    return false;
}

std::vector<std::string> ClientWorker::TachesEnCours() const {
    std::lock_guard<std::mutex> v(m_verrou);
    std::vector<std::string> ids;
    for (const auto& t : m_taches)
        if (!t->m_termine.load()) ids.push_back(t->m_id);
    return ids;
}

int ClientWorker::AbonnerProgression(AbonneProgression f) {
    const int id = m_noyau->m_prochain++;
    m_noyau->m_progression.emplace_back(id, std::move(f));
    return id;
}

int ClientWorker::AbonnerFin(AbonneFin f) {
    const int id = m_noyau->m_prochain++;
    m_noyau->m_fin.emplace_back(id, std::move(f));
    return id;
}

void ClientWorker::Desabonner(int identifiant) {
    auto egal = [&](const auto& p) { return p.first == identifiant; };
    m_noyau->m_progression.erase(std::remove_if(m_noyau->m_progression.begin(), m_noyau->m_progression.end(), egal),
                                 m_noyau->m_progression.end());
    m_noyau->m_fin.erase(std::remove_if(m_noyau->m_fin.begin(), m_noyau->m_fin.end(), egal), m_noyau->m_fin.end());
}

void ClientWorker::Arreter(std::chrono::milliseconds delai) {
    m_noyau->m_ferme.store(true);   // plus aucun abonne ne sera appele
    AnnulerTout();
    const auto limite = std::chrono::steady_clock::now() + delai;
    while (EnCours() && std::chrono::steady_clock::now() < limite) std::this_thread::sleep_for(std::chrono::milliseconds(25));
    {
        std::lock_guard<std::mutex> v(m_verrou);
        for (auto& t : m_taches)
            if (!t->m_termine.load()) TerminateProcess(t->m_processus, 3);   // le Job Object tue le worker
    }
    std::vector<std::unique_ptr<Tache>> toutes;
    {
        std::lock_guard<std::mutex> v(m_verrou);
        toutes.swap(m_taches);
    }
    toutes.clear();   // ~Tache joint chaque thread
}

} // namespace app
