// Client du moteur : lance `Decid.exe --moteur executer --requete R --taches T` (ou DecidMoteur.exe executer ...) (CreateProcessW, sans console), lit les
// evenements JSONL (stdout) dans un thread POSSEDE et JOINT (jamais detache), et les relaie dans la boucle
// de l'interface par un Dispatcheur (en production : slint::invoke_from_event_loop). L'etat terminal d'une tache
// est lu dans <taches>/<id>/etat.json (et non deduit du seul code de sortie).
// Aucune dependance a Slint ni a OpenCV : testable sans interface avec FileEvenements.
//
// Threads : Lancer, Annuler, Abonner*, Arreter sont appeles depuis le thread de l'interface ; les abonnes sont
// toujours executes dans ce thread (via le Dispatcheur).
#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Erreur.h"

namespace app {

namespace fs = std::filesystem;

struct EvenementWorker {
    std::string m_phase;    // phase reelle du worker
    long m_i = 0;           // avancement
    long m_n = 0;           // total ; 0 = indetermine (jamais de pourcentage invente)
    std::string m_detail;
};

struct FinTache {
    std::string m_id;
    std::string m_etat;         // "succeeded" | "failed" | "cancelled" | "interrupted"
    std::string m_raison;       // diagnostic.raison de etat.json, ou cause locale
    int m_codeSortie = 0;
    fs::path m_dossier;         // <taches>/<id>
};

using Dispatcheur = std::function<void(std::function<void()>)>;

// File d'evenements pour les usages sans boucle d'interface (tests) : le Dispatcheur y poste, le thread
// appelant "pompe".
class FileEvenements {
public:
    void Poster(std::function<void()> action);
    // Execute les actions en attente ; renvoie leur nombre.
    size_t Pomper();
    // Pompe jusqu'a ce que `condition` soit vraie ou que le delai expire ; renvoie condition().
    bool PomperJusqua(const std::function<bool()>& condition, std::chrono::milliseconds delai);
    Dispatcheur VersDispatcheur();
private:
    std::mutex m_verrou;
    std::condition_variable m_cv;
    std::deque<std::function<void()>> m_actions;
};

class ClientWorker {
public:
    using AbonneProgression = std::function<void(const std::string& id, const EvenementWorker&)>;
    using AbonneFin = std::function<void(const FinTache&)>;

    // executable : Decid.exe (avec le prefixe --moteur) ou DecidMoteur.exe.
    // prefixe : argument place avant la commande ("--moteur" quand le moteur est Decid.exe lui-meme)
    ClientWorker(fs::path executable, Dispatcheur dispatcheur, std::wstring prefixe = L"");
    ~ClientWorker();                      // Arreter()
    ClientWorker(const ClientWorker&) = delete;
    ClientWorker& operator=(const ClientWorker&) = delete;

    // Nombre maximal de taches lourdes simultanees (1 par defaut).
    void FixerMaxLourdes(int n) { m_maxLourdes = n < 1 ? 1 : n; }

    // Lance la tache decrite par le fichier requete (request_id = identifiant de tache = nom du dossier de tache).
    // Echoue si une tache lourde tourne deja, si l'executable est absent ou si la requete est illisible.
    decid::Resultat<std::string> Lancer(const fs::path& requete, const fs::path& dossierTaches, bool lourde = true);

    // Annulation cooperative : cree <tache>/annuler (le worker la voit en <= 100 ms). Si le dossier de tache
    // n'existe pas encore (validation en cours), le processus d'orchestration est arrete.
    void Annuler(const std::string& id);
    void AnnulerTout();

    bool EnCours() const;                 // au moins une tache non terminee
    bool LourdeEnCours() const;
    std::vector<std::string> TachesEnCours() const;

    // Abonnements (thread de l'interface). Renvoie un identifiant pour Desabonner.
    int AbonnerProgression(AbonneProgression f);
    int AbonnerFin(AbonneFin f);
    void Desabonner(int identifiant);

    // Annule tout, attend jusqu'a `delai` la fin des taches, puis arrete les orchestrateurs restants (leur Job
    // Object tue le worker) et joint tous les threads. Appelee par le destructeur ; ne declenche plus d'abonne.
    void Arreter(std::chrono::milliseconds delai = std::chrono::milliseconds(12000));

private:
    struct Tache;
    struct Noyau;     // abonnes + drapeau de fermeture, partage avec les actions postees

    void Lire(Tache& tache);
    void Recolter();                      // joint les threads termines

    fs::path m_executable;
    std::wstring m_prefixe;
    Dispatcheur m_dispatcheur;
    std::shared_ptr<Noyau> m_noyau;
    mutable std::mutex m_verrou;
    std::vector<std::unique_ptr<Tache>> m_taches;
    int m_maxLourdes = 1;
};

} // namespace app
