// Tests du worker / orchestrateur via la ligne de commande DecidMoteur.exe (ACC-10 exports generiques,
// ACC-12 sans interface, ACC-29 annulation / arret force / crash).
// Usage (voir CMakeLists) : test_worker <DecidMoteur.exe> <dossier source>.
// Fixtures SYNTHETIQUES generees dans verif::DossierTemporaire ; DecidMoteur.exe lance par CreateProcessW.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <windows.h>
#include <tlhelp32.h>

#include "Fichiers.h"
#include "Json.h"
#include "Sha256.h"
#include "Verifier.h"

using namespace decid;

namespace {

fs::path g_exe;     // chemin de DecidMoteur.exe (argv[1])

// ---------- outils generaux ----------

void Ecrire(const fs::path& chemin, const std::string& texte) {
    std::ofstream f(chemin, std::ios::binary);
    f.write(texte.data(), (std::streamsize)texte.size());
}

std::string Lire(const fs::path& chemin) {
    auto t = LireFichierTexte(chemin);
    return t.Ok() ? t.Valeur() : std::string();
}

std::string Hash(const fs::path& chemin) { return Sha256Fichier(chemin); }

bool Contient(const std::string& texte, const std::string& motif) { return texte.find(motif) != std::string::npos; }

std::vector<std::string> Lister(const fs::path& dossier) {
    std::vector<std::string> noms;
    std::error_code ec;
    for (auto& e : fs::directory_iterator(dossier, ec)) noms.push_back(e.path().filename().string());
    std::sort(noms.begin(), noms.end());
    return noms;
}

ValeurJson LireJsonFichier(const fs::path& chemin) {
    auto j = LireJson(Lire(chemin));
    return j.Ok() ? j.Valeur() : ValeurJson();
}

std::string EtatFichier(const fs::path& tache) { return LireJsonFichier(tache / "etat.json").Membre("etat").Texte(); }

// Variable d'environnement positionnee le temps d'un scenario, puis retiree (heritee par decid2 et son worker).
class VarEnv {
public:
    VarEnv(const wchar_t* nom, const wchar_t* valeur) : m_nom(nom) { SetEnvironmentVariableW(nom, valeur); }
    ~VarEnv() { SetEnvironmentVariableW(m_nom, nullptr); }
    VarEnv(const VarEnv&) = delete;
    VarEnv& operator=(const VarEnv&) = delete;
private:
    const wchar_t* m_nom;
};

// ---------- lancement de DecidMoteur.exe ----------

struct Poignee {
    HANDLE m_h = nullptr;
    Poignee() = default;
    explicit Poignee(HANDLE h) : m_h(h) {}
    Poignee(const Poignee&) = delete;
    Poignee& operator=(const Poignee&) = delete;
    ~Poignee() { Fermer(); }
    void Fermer() { if (m_h && m_h != INVALID_HANDLE_VALUE) CloseHandle(m_h); m_h = nullptr; }
    bool Valide() const { return m_h && m_h != INVALID_HANDLE_VALUE; }
};

struct Processus {
    Poignee m_processus;
    DWORD m_pid = 0;
    fs::path m_sortie;      // fichier recevant stdout + stderr
    bool m_lance = false;
    DWORD Code() const { DWORD c = 0; GetExitCodeProcess(m_processus.m_h, &c); return c; }
    // Attend la fin ; au-dela du delai, tue le processus (le job du worker meurt avec lui) et renvoie false.
    bool Attendre(DWORD millisecondes) {
        if (WaitForSingleObject(m_processus.m_h, millisecondes) == WAIT_OBJECT_0) return true;
        TerminateProcess(m_processus.m_h, 250);
        WaitForSingleObject(m_processus.m_h, 5000);
        return false;
    }
    std::string Sortie() const { return Lire(m_sortie); }
};

std::wstring Guillemets(const std::wstring& s) { return L"\"" + s + L"\""; }

// Lance DecidMoteur.exe sans attendre. stdout et stderr vont dans un fichier, stdin sur NUL.
bool Lancer(const std::vector<std::wstring>& arguments, const fs::path& fichierSortie, Processus& p) {
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    Poignee sortie(CreateFileW(fichierSortie.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    Poignee entree(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!sortie.Valide() || !entree.Valide()) return false;
    std::wstring ligne = Guillemets(g_exe.wstring());
    for (auto& a : arguments) ligne += L" " + Guillemets(a);
    std::vector<wchar_t> tampon(ligne.begin(), ligne.end());
    tampon.push_back(L'\0');
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = entree.m_h;
    si.hStdOutput = sortie.m_h;
    si.hStdError = sortie.m_h;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, tampon.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        return false;
    CloseHandle(pi.hThread);
    p.m_processus.m_h = pi.hProcess;
    p.m_pid = pi.dwProcessId;
    p.m_sortie = fichierSortie;
    p.m_lance = true;
    return true;
}

struct Execution { bool m_ok = false; DWORD m_code = 0; std::string m_sortie; };

Execution Executer(const std::vector<std::wstring>& arguments, const fs::path& fichierSortie, DWORD delaiMs = 120000) {
    Execution e;
    Processus p;
    if (!Lancer(arguments, fichierSortie, p)) {
        verif::Echec(__FILE__, __LINE__, "CreateProcessW a echoue pour " + g_exe.string());
        return e;
    }
    e.m_ok = p.Attendre(delaiMs);
    if (!e.m_ok) verif::Echec(__FILE__, __LINE__, "DecidMoteur.exe n'a pas termine dans le delai");
    e.m_code = p.Code();
    e.m_sortie = p.Sortie();
    return e;
}

std::set<DWORD> PidsDecid2() {
    std::set<DWORD> pids;
    Poignee instantane(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!instantane.Valide()) return pids;
    PROCESSENTRY32W e{};
    e.dwSize = sizeof(e);
    for (BOOL ok = Process32FirstW(instantane.m_h, &e); ok; ok = Process32NextW(instantane.m_h, &e))
        if (_wcsicmp(e.szExeFile, g_exe.filename().c_str()) == 0) pids.insert(e.th32ProcessID);
    return pids;
}

// ---------- fixtures ----------

struct Fixture {
    fs::path m_racine, m_source, m_csv, m_requetes, m_taches;
};

// CSV synthetique : 2 classes (0/1), 2 attributs, ~200 lignes, sans en-tete (nuages deterministes).
std::string CsvDeuxClasses() {
    std::uint64_t e = 99;
    auto suivant = [&]() { e = e * 6364136223846793005ULL + 1442695040888963407ULL; return (double)(e >> 11) / 9007199254740992.0; };
    std::string t;
    for (int i = 0; i < 200; ++i) {
        int classe = i % 2;
        double x = classe * 2.0 + suivant() + suivant() - 1.0;
        double y = classe * 2.0 + suivant() + suivant() - 1.0;
        t += std::to_string(classe) + "," + std::to_string(x) + "," + std::to_string(y) + "\n";
    }
    return t;
}

Fixture CreerFixture(const std::string& nom) {
    Fixture f;
    f.m_racine = verif::DossierTemporaire(nom);
    f.m_source = f.m_racine / "source";
    f.m_requetes = f.m_racine / "requetes";
    f.m_taches = f.m_racine / "taches";
    fs::create_directories(f.m_source);
    fs::create_directories(f.m_requetes);
    fs::create_directories(f.m_taches);
    f.m_csv = f.m_source / "donnees.csv";
    Ecrire(f.m_csv, CsvDeuxClasses());
    return f;
}

fs::path EcrireRequete(const Fixture& f, const std::string& id, const std::string& famille = "svm") {
    ValeurJson r = ValeurJson::Objet();
    r["protocol_version"] = 1;
    r["request_id"] = id;
    ValeurJson s = ValeurJson::Objet();
    s["type"] = "csv";
    s["chemin"] = CheminUtf8(f.m_csv);
    s["en_tete"] = "non";
    r["source"] = s;
    ValeurJson p = ValeurJson::Objet();
    p["methode"] = "premiers_n_par_classe";
    p["ratio"] = 0.5;
    p["graine"] = 1;
    r["partage"] = p;
    ValeurJson n = ValeurJson::Objet();
    n["methode"] = "minmax";
    r["normalisation"] = n;
    ValeurJson a = ValeurJson::Objet();
    a["famille"] = famille;
    a["noyau"] = "RBF";
    a["c"] = 1;
    a["gamma"] = 1;
    r["algorithme"] = a;
    r["threads"] = 1;
    fs::path chemin = f.m_requetes / (id + ".json");
    Ecrire(chemin, r.Ecrire());
    return chemin;
}

std::vector<std::wstring> ArgsExecuter(const Fixture& f, const fs::path& requete, const wchar_t* delai = nullptr) {
    std::vector<std::wstring> a = {L"executer", L"--requete", requete.wstring(), L"--taches", f.m_taches.wstring()};
    if (delai) { a.push_back(L"--delai-arret"); a.push_back(delai); }
    return a;
}

std::vector<std::wstring> ArgsTache(const wchar_t* commande, const fs::path& tache) {
    return {commande, L"--tache", tache.wstring()};
}

bool Attendre(const std::function<bool()>& condition, int delaiMs) {
    auto debut = std::chrono::steady_clock::now();
    while (!condition()) {
        if (std::chrono::steady_clock::now() - debut > std::chrono::milliseconds(delaiMs)) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return true;
}

// Lance "executer" en arriere-plan, attend que le worker soit entre dans la phase d'apprentissage,
// puis cree <tache>\annuler. Renvoie la duree (ms) entre la demande et la fin du processus.
struct Annulation { bool m_termine = false; DWORD m_code = 0; long long m_dureeMs = 0; std::string m_sortie; fs::path m_tache; };

Annulation LancerPuisAnnuler(const Fixture& f, const std::string& id, const wchar_t* delai) {
    Annulation r;
    fs::path requete = EcrireRequete(f, id);
    Processus p;
    if (!Lancer(ArgsExecuter(f, requete, delai), f.m_racine / "sortie.txt", p)) {
        verif::Echec(__FILE__, __LINE__, "lancement de executer impossible");
        return r;
    }
    r.m_tache = f.m_taches / id;
    if (!Attendre([&] { return fs::exists(r.m_tache); }, 15000))
        verif::Echec(__FILE__, __LINE__, "le dossier de tache n'est jamais apparu");
    Attendre([&] { return Contient(Lire(r.m_tache / "evenements.jsonl"), "apprentissage"); }, 15000);
    auto debut = std::chrono::steady_clock::now();
    Ecrire(r.m_tache / "annuler", "annuler\n");
    r.m_termine = p.Attendre(40000);
    r.m_dureeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - debut).count();
    r.m_code = p.Code();
    r.m_sortie = p.Sortie();
    return r;
}

} // namespace

// ---------- scenario 1 : succes complet ----------

TEST(worker_succes_commit_etat_inspection_et_source_intacte) {
    Fixture f = CreerFixture("worker_succes");
    const std::string hashSource = Hash(f.m_csv);
    const auto avant = Lister(f.m_source);
    fs::path requete = EcrireRequete(f, "t-succes");
    Execution e = Executer(ArgsExecuter(f, requete), f.m_racine / "s1.txt");
    VERIFIER(e.m_ok);
    VERIFIER_EGAL((long long)e.m_code, 0LL);
    fs::path tache = f.m_taches / "t-succes";
    VERIFIER_EGAL(EtatFichier(tache), std::string("succeeded"));
    VERIFIER(fs::exists(tache / "commit.json"));
    VERIFIER(fs::exists(tache / "resultat" / "artefacts.json"));
    VERIFIER_EGAL(LireJsonFichier(tache / "commit.json").Membre("sha256_artefacts").Texte(),
                  Hash(tache / "resultat" / "artefacts.json"));
    VERIFIER(!Hash(tache / "resultat" / "artefacts.json").empty());
    VERIFIER(!fs::exists(tache / "staging"));      // le staging a ete publie, pas laisse

    Execution etat = Executer(ArgsTache(L"etat", tache), f.m_racine / "s1-etat.txt");
    VERIFIER_EGAL((long long)etat.m_code, 0LL);
    auto jetat = LireJson(etat.m_sortie);
    VERIFIER(jetat.Ok());
    if (jetat.Ok()) VERIFIER_EGAL(jetat.Valeur().Membre("etat").Texte(), std::string("succeeded"));

    Execution inspection = Executer(ArgsTache(L"inspecter", tache), f.m_racine / "s1-insp.txt");
    VERIFIER_EGAL((long long)inspection.m_code, 0LL);
    auto jinsp = LireJson(inspection.m_sortie);
    VERIFIER(jinsp.Ok());
    if (jinsp.Ok())
        VERIFIER_EGAL(jinsp.Valeur().Membre("schema").Texte(), std::string("decid.inspection/1"));

    VERIFIER_EGAL(Hash(f.m_csv), hashSource);
    VERIFIER(Lister(f.m_source) == avant);          // aucun fichier cree a cote de la source
}

// ---------- scenario 2 : meme request_id ----------

TEST(worker_meme_request_id_est_refuse_et_la_tache_reste_intacte) {
    Fixture f = CreerFixture("worker_rejeu");
    fs::path requete = EcrireRequete(f, "t-rejeu");
    Execution premier = Executer(ArgsExecuter(f, requete), f.m_racine / "s2a.txt");
    VERIFIER_EGAL((long long)premier.m_code, 0LL);
    fs::path tache = f.m_taches / "t-rejeu";
    const std::string hEtat = Hash(tache / "etat.json"), hCommit = Hash(tache / "commit.json"),
                      hArtefacts = Hash(tache / "resultat" / "artefacts.json"), hModele = Hash(tache / "resultat" / "modele.yml");
    const auto contenu = Lister(tache);

    Execution second = Executer(ArgsExecuter(f, requete), f.m_racine / "s2b.txt");
    VERIFIER(second.m_ok);
    VERIFIER(second.m_code != 0);
    VERIFIER(!second.m_sortie.empty());               // message d'erreur explicite
    VERIFIER_EGAL(Hash(tache / "etat.json"), hEtat);
    VERIFIER_EGAL(Hash(tache / "commit.json"), hCommit);
    VERIFIER_EGAL(Hash(tache / "resultat" / "artefacts.json"), hArtefacts);
    VERIFIER_EGAL(Hash(tache / "resultat" / "modele.yml"), hModele);
    VERIFIER(Lister(tache) == contenu);
    VERIFIER_EGAL(EtatFichier(tache), std::string("succeeded"));
}

// ---------- scenario 3 : annulation cooperative ----------

TEST(worker_annulation_cooperative) {
    Fixture f = CreerFixture("worker_annulation");
    VarEnv pause(L"DECID_TEST_PAUSE", L"apprentissage:3000");
    Annulation r = LancerPuisAnnuler(f, "t-annule", nullptr);
    VERIFIER(r.m_termine);
    VERIFIER_EGAL((long long)r.m_code, 3LL);
    VERIFIER_EGAL(EtatFichier(r.m_tache), std::string("cancelled"));
    VERIFIER(!fs::exists(r.m_tache / "commit.json"));
    VERIFIER(!fs::exists(r.m_tache / "resultat"));
    Execution etat = Executer(ArgsTache(L"etat", r.m_tache), f.m_racine / "s3-etat.txt");
    VERIFIER(Contient(etat.m_sortie, "cancelled"));
    VERIFIER(!Contient(etat.m_sortie, "succeeded"));
    Execution inspection = Executer(ArgsTache(L"inspecter", r.m_tache), f.m_racine / "s3-insp.txt");
    VERIFIER(inspection.m_code != 0);                       // rien a inspecter
}

// ---------- scenario 4 : arret force ----------

TEST(worker_arret_force_apres_le_delai_de_grace) {
    Fixture f = CreerFixture("worker_arret_force");
    const std::set<DWORD> avant = PidsDecid2();
    {
        VarEnv pause(L"DECID_TEST_PAUSE", L"apprentissage:20000");
        Annulation r = LancerPuisAnnuler(f, "t-force", L"1");
        VERIFIER(r.m_termine);
        VERIFIER(r.m_dureeMs < 10000);
        VERIFIER_EGAL((long long)r.m_code, 3LL);
        VERIFIER_EGAL(EtatFichier(r.m_tache), std::string("cancelled"));
        auto diag = LireJsonFichier(r.m_tache / "etat.json").Membre("diagnostic").Membre("raison").Texte();
        VERIFIER(Contient(diag, "arret force"));
        VERIFIER(!fs::exists(r.m_tache / "commit.json"));
        VERIFIER(!fs::exists(r.m_tache / "resultat"));
    }
    // aucun processus DecidMoteur.exe nouveau ne doit subsister (worker tue avec son groupe)
    bool propre = Attendre([&] {
        for (DWORD pid : PidsDecid2()) if (!avant.count(pid)) return false;
        return true;
    }, 5000);
    VERIFIER(propre);
}

// ---------- scenario 5 : crash du worker ----------

TEST(worker_crash_donne_failed_sans_commit) {
    Fixture f = CreerFixture("worker_crash");
    VarEnv crash(L"DECID_TEST_CRASH", L"sauvegarde");
    fs::path requete = EcrireRequete(f, "t-crash");
    Execution e = Executer(ArgsExecuter(f, requete), f.m_racine / "s5.txt");
    VERIFIER(e.m_ok);
    VERIFIER_EGAL((long long)e.m_code, 2LL);
    fs::path tache = f.m_taches / "t-crash";
    VERIFIER_EGAL(EtatFichier(tache), std::string("failed"));
    VERIFIER(!fs::exists(tache / "commit.json"));
    VERIFIER(!fs::exists(tache / "resultat"));
    VERIFIER(Contient(LireJsonFichier(tache / "etat.json").Membre("diagnostic").Membre("raison").Texte(), "crash"));
    Execution etat = Executer(ArgsTache(L"etat", tache), f.m_racine / "s5-etat.txt");
    VERIFIER(!Contient(etat.m_sortie, "succeeded"));
    VERIFIER(Contient(etat.m_sortie, "failed"));
}

TEST(worker_etat_n_annonce_jamais_un_succes_sans_commit_coherent) {
    Fixture f = CreerFixture("worker_commit_incoherent");
    fs::path requete = EcrireRequete(f, "t-incoherent");
    VERIFIER_EGAL((long long)Executer(ArgsExecuter(f, requete), f.m_racine / "s.txt").m_code, 0LL);
    fs::path tache = f.m_taches / "t-incoherent";
    Execution ok = Executer(ArgsTache(L"etat", tache), f.m_racine / "e1.txt");
    VERIFIER(Contient(ok.m_sortie, "succeeded"));
    // artefacts alteres apres coup : le commit n'est plus coherent
    Ecrire(tache / "resultat" / "artefacts.json", "{}");
    Execution altere = Executer(ArgsTache(L"etat", tache), f.m_racine / "e2.txt");
    VERIFIER(!Contient(altere.m_sortie, "succeeded"));
    // commit absent
    fs::remove(tache / "commit.json");
    Execution sans = Executer(ArgsTache(L"etat", tache), f.m_racine / "e3.txt");
    VERIFIER(!Contient(sans.m_sortie, "succeeded"));
}

// ---------- scenario 6 : requete invalide ----------

TEST(worker_requete_invalide_donne_le_code_2_et_un_message) {
    Fixture f = CreerFixture("worker_invalide");
    fs::path requete = EcrireRequete(f, "t-invalide", "famille_inconnue");
    Execution e = Executer(ArgsExecuter(f, requete), f.m_racine / "s6.txt");
    VERIFIER(e.m_ok);
    VERIFIER_EGAL((long long)e.m_code, 2LL);
    VERIFIER(Contient(e.m_sortie, "famille"));
    VERIFIER(!fs::exists(f.m_taches / "t-invalide"));      // aucune tache creee pour une requete invalide
    Ecrire(f.m_requetes / "casse.json", "{ pas du json");
    Execution casse = Executer(ArgsExecuter(f, f.m_requetes / "casse.json"), f.m_racine / "s6b.txt");
    VERIFIER_EGAL((long long)casse.m_code, 2LL);
    VERIFIER(!casse.m_sortie.empty());
}

// ---------- scenario 7 : exporter-csv vers la source ----------

TEST(cli_exporter_csv_vers_la_source_est_refuse) {
    Fixture f = CreerFixture("cli_export_source");
    const std::string hashSource = Hash(f.m_csv);
    const auto avant = Lister(f.m_source);
    for (bool remplacer : {false, true}) {
        std::vector<std::wstring> a = {L"exporter-csv", L"--csv", f.m_csv.wstring(), L"--sortie", f.m_csv.wstring()};
        if (remplacer) a.push_back(L"--remplacer");
        Execution e = Executer(a, f.m_racine / "s7.txt");
        VERIFIER(e.m_ok);
        VERIFIER(e.m_code != 0);
        VERIFIER(!e.m_sortie.empty());
        VERIFIER_EGAL(Hash(f.m_csv), hashSource);
        VERIFIER(Lister(f.m_source) == avant);
    }
}

TEST(cli_exporter_csv_generique_vers_un_autre_fichier) {
    Fixture f = CreerFixture("cli_export_ok");
    const std::string hashSource = Hash(f.m_csv);
    fs::path sortie = f.m_racine / "export.csv";
    std::vector<std::wstring> a = {L"exporter-csv", L"--csv", f.m_csv.wstring(), L"--sortie", sortie.wstring()};
    Execution e = Executer(a, f.m_racine / "s8.txt");
    VERIFIER_EGAL((long long)e.m_code, 0LL);
    VERIFIER(fs::exists(sortie));
    const std::string exporte = Lire(sortie);
    long lignes = (long)std::count(exporte.begin(), exporte.end(), '\n');
    VERIFIER_EGAL(lignes, 200L);
    Execution encore = Executer(a, f.m_racine / "s8b.txt");           // destination existante, sans --remplacer
    VERIFIER(encore.m_code != 0);
    VERIFIER_EGAL(Hash(f.m_csv), hashSource);
    // l'export relu par decid2 donne le meme jeu (meme hash canonique que la source)
    Execution a1 = Executer({L"importer", L"--csv", f.m_csv.wstring()}, f.m_racine / "i1.txt");
    Execution a2 = Executer({L"importer", L"--csv", sortie.wstring()}, f.m_racine / "i2.txt");
    auto j1 = LireJson(a1.m_sortie), j2 = LireJson(a2.m_sortie);
    if (j1.Ok() && j2.Ok()) {
        VERIFIER(!j1.Valeur().Membre("hash_canonique").Texte().empty());
        VERIFIER_EGAL(j1.Valeur().Membre("hash_canonique").Texte(), j2.Valeur().Membre("hash_canonique").Texte());
    } else {
        verif::Echec(__FILE__, __LINE__, "la sortie de decid2 importer n'est pas du JSON");
    }
}

// argv[1] = DecidMoteur.exe, argv[2] = dossier source (voir CMakeLists) : on decale pour le harnais.
int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) g_exe = argv[1];
    if (g_exe.empty() || !fs::exists(g_exe)) {
        std::printf("test_worker : chemin de DecidMoteur.exe absent ou invalide (argv[1])\n");
        return 1;
    }
    return LancerTests(argc - 1, argv + 1);
}
