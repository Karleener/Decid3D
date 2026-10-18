// Decid - application. Fenetre Slint, rendu logiciel par defaut (OpenGL/femtovg pour les captures) ;
// aucun calcul scientifique ici : chaque calcul passe par Decid.exe --moteur (ClientWorker). Options :
//   --capture <fichier.png>     affiche l'ecran, capture la fenetre, quitte (verification visuelle)
//   --ecran <nom>               ecran affiche au depart (accueil, decouvrir, etude)
//   --theme sombre|clair        --lecture normale|facile   (pour la capture ; sinon les preferences enregistrees)
//   --vue etape|outil|experiences   --etape <nom>   --outil <id>   --dialogue <id>   --menu <indice>   (captures)
//   --jeu <i> --famille <i> --valeur <i> --entrainer   lance un vrai apprentissage avant la capture (verification)
//   --importer <fichier .csv|.cmc> [--lim <fichier.lim>] [--utiliser]   lit un fichier comme l'ecran Importer
//   --interieur | --cellule <reel> <predit>   etat du plan des resultats a la capture
//   --scenario <i> --pari <k> --etape-scenario <0|1|2>   deroule un scenario de « Je decouvre » avant la capture
//   --annuler <ms>   annule le calcul en cours apres ce delai (verification du bouton Annuler)
//   --largeur <px>   largeur de la fenetre pour la capture (avec --hauteur)
//   --enregistrer <fichier.decid>   enregistre le projet avant la capture ; --ouvrir <fichier.decid> l'ouvre au depart
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>

#include <chrono>
#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <memory>
#include <string>
#include <vector>

#include "Capture.h"
#include "ClientWorker.h"
#include "LigneCommande.h"
#include "Annexes.h"
#include "CadreFenetre.h"
#include "Decouvrir.h"
#include "outils/BacASable.h"
#include "outils/PasAPas.h"
#include "outils/AtelierImage.h"
#include "outils/Decision.h"
#include "outils/Generateur.h"
#include "outils/Regression.h"
#include "outils/Validation.h"
#include "Etude.h"
#include "Moteur.h"
#include "Fichiers.h"
#include "Plateforme.h"
#include <slint.h>

#include "decid.h"
// --- executable portable (DECID_PORTABLE) : debut
#ifdef DECID_PORTABLE
#include "Portable.h"
#endif
// --- executable portable : fin

namespace fs = std::filesystem;

namespace {

struct Options {
    fs::path m_fichierCapture;
    std::string m_ecran;
    std::optional<bool> m_sombre;
    std::optional<bool> m_facile;
    std::string m_vue, m_etape, m_outil, m_dialogue;
    int m_menu = -1;
    bool m_entrainer = false;
    int m_jeu = -1, m_famille = -1, m_valeur = -1;
    fs::path m_importer, m_lim;
    bool m_utiliser = false, m_interieur = false, m_vue3d = false, m_effets = false;
    int m_annuler = -1;      // ms : annule le calcul en cours apres ce delai (verification du bouton Annuler)
    int m_celluleReel = -1, m_cellulePredit = -1, m_direct = -1;
    int m_scenario = -1, m_pari = -1, m_etapeScenario = 0;
    fs::path m_enregistrer, m_ouvrir;
    bool m_bacExemple = false;
    std::string m_valider;   // v : validation, b : balayage (verification)
    int m_hauteur = 0;       // hauteur de la fenetre pour la capture (pages longues)
    int m_largeur = 0;       // largeur de la fenetre pour la capture (verifier une grande fenetre)
    int m_pas = -1, m_pasEtape = -1;   // pas a pas : algorithme et etape a montrer
    bool m_regression = false;
    int m_generer = -1;
    bool m_decider = false;
    fs::path m_atelier;   // image : deux zones de test, apprentissage et segmentation
};

Options Analyser(int argc, wchar_t** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::wstring a = argv[i];
        const bool suivant = i + 1 < argc;
        if (a == L"--capture" && suivant) o.m_fichierCapture = argv[++i];
        else if (a == L"--ecran" && suivant) o.m_ecran = decid::CheminUtf8(fs::path(argv[++i]));
        else if (a == L"--theme" && suivant) o.m_sombre = std::wstring(argv[++i]) != L"clair";
        else if (a == L"--lecture" && suivant) o.m_facile = std::wstring(argv[++i]) == L"facile";
        else if (a == L"--vue" && suivant) o.m_vue = decid::CheminUtf8(fs::path(argv[++i]));
        else if (a == L"--etape" && suivant) o.m_etape = decid::CheminUtf8(fs::path(argv[++i]));
        else if (a == L"--outil" && suivant) o.m_outil = decid::CheminUtf8(fs::path(argv[++i]));
        else if (a == L"--dialogue" && suivant) o.m_dialogue = decid::CheminUtf8(fs::path(argv[++i]));
        else if (a == L"--menu" && suivant) o.m_menu = _wtoi(argv[++i]);
        else if (a == L"--entrainer") o.m_entrainer = true;
        else if (a == L"--importer" && suivant) o.m_importer = argv[++i];
        else if (a == L"--lim" && suivant) o.m_lim = argv[++i];
        else if (a == L"--utiliser") o.m_utiliser = true;
        else if (a == L"--interieur") o.m_interieur = true;
        else if (a == L"--vue3d") o.m_vue3d = true;
        else if (a == L"--effets") o.m_effets = true;
        else if (a == L"--annuler" && suivant) o.m_annuler = _wtoi(argv[++i]);
        else if (a == L"--scenario" && suivant) o.m_scenario = _wtoi(argv[++i]);
        else if (a == L"--enregistrer" && suivant) o.m_enregistrer = argv[++i];
        else if (a == L"--ouvrir" && suivant) o.m_ouvrir = argv[++i];
        else if (a == L"--bac-exemple") o.m_bacExemple = true;
        else if (a == L"--hauteur" && suivant) o.m_hauteur = _wtoi(argv[++i]);
        else if (a == L"--largeur" && suivant) o.m_largeur = _wtoi(argv[++i]);
        else if (a == L"--pas" && suivant) o.m_pas = _wtoi(argv[++i]);
        else if (a == L"--regression") o.m_regression = true;
        else if (a == L"--generer" && suivant) o.m_generer = _wtoi(argv[++i]);
        else if (a == L"--decider") o.m_decider = true;
        else if (a == L"--atelier" && suivant) o.m_atelier = argv[++i];
        else if (a == L"--pas-etape" && suivant) o.m_pasEtape = _wtoi(argv[++i]);
        else if (a == L"--valider" && suivant) o.m_valider = decid::CheminUtf8(fs::path(argv[++i]));
        else if (a == L"--pari" && suivant) o.m_pari = _wtoi(argv[++i]);
        else if (a == L"--etape-scenario" && suivant) o.m_etapeScenario = _wtoi(argv[++i]);
        else if (a == L"--direct" && suivant) o.m_direct = _wtoi(argv[++i]);
        else if (a == L"--cellule" && i + 2 < argc) { o.m_celluleReel = _wtoi(argv[++i]); o.m_cellulePredit = _wtoi(argv[++i]); }
        else if (a == L"--jeu" && suivant) o.m_jeu = _wtoi(argv[++i]);
        else if (a == L"--famille" && suivant) o.m_famille = _wtoi(argv[++i]);
        else if (a == L"--valeur" && suivant) o.m_valeur = _wtoi(argv[++i]);
    }
    return o;
}

// Preferences persistantes (equivalent de Decid.ini de l'ancien logiciel) : %APPDATA%\Decid\preferences.ini
class FichierPreferences {
public:
    FichierPreferences() {
        PWSTR dossier = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &dossier)))
            m_chemin = fs::path(dossier) / L"Decid" / L"preferences.ini";
        CoTaskMemFree(dossier);
    }
    void Lire(bool& sombre, bool& facile, Fenetre& fenetre) const {
        if (m_chemin.empty()) return;
        std::ifstream f(m_chemin);
        std::string ligne;
        auto& p = fenetre.global<::Preferences>();
        while (std::getline(f, ligne)) {
            const size_t eg = ligne.find('=');
            if (eg == std::string::npos) continue;
            const std::string cle = ligne.substr(0, eg), v = ligne.substr(eg + 1);
            if (cle == "theme") sombre = v != "clair";
            else if (cle == "lecture") facile = v == "facile";
            else if (cle == "grille3d") p.set_grille3d(std::clamp(std::atoi(v.c_str()), 20, 120));
            else if (cle == "transparence3d") p.set_transparence3d(std::clamp((float)std::atof(v.c_str()), 0.1f, 0.99f));
            else if (cle == "threads") p.set_threads(std::clamp(std::atoi(v.c_str()), 1, 8));
            else if (cle == "rotation3d") p.set_rotation3d(v == "1");
            else if (cle == "donnees3d") p.set_donnees3d(v == "1");
        }
    }
    void Ecrire(bool sombre, bool facile, Fenetre& fenetre) const {
        if (m_chemin.empty()) return;
        std::error_code ec;
        fs::create_directories(m_chemin.parent_path(), ec);
        const auto& p = fenetre.global<::Preferences>();
        std::ofstream f(m_chemin, std::ios::trunc);
        f << "theme=" << (sombre ? "sombre" : "clair") << "\nlecture=" << (facile ? "facile" : "normale") << "\ngrille3d=" << p.get_grille3d()
          << "\ntransparence3d=" << p.get_transparence3d() << "\nthreads=" << p.get_threads() << "\nrotation3d=" << (p.get_rotation3d() ? 1 : 0)
          << "\ndonnees3d=" << (p.get_donnees3d() ? 1 : 0) << "\n";
    }
private:
    fs::path m_chemin;
};

// Projet ouvert ou enregistre : chemin du fichier .decid et empreinte du dernier etat enregistre
struct ProjetOuvert {
    fs::path m_chemin;
    std::string m_empreinte;
    std::function<void()> m_apresConfirmation;   // action en attente d'une confirmation
};

fs::path DossierTravail() {
    PWSTR dossier = nullptr;
    fs::path p;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &dossier))) p = fs::path(dossier) / L"Decid" / L"travail";
    CoTaskMemFree(dossier);
    return p.empty() ? app::plateforme::DossierExecutable() / L"travail" : p;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    // mode moteur : Decid.exe --moteur <commande> ... (lance par l'interface pour chaque calcul, sans fenetre)
    if (argc >= 2 && std::wstring(argv[1]) == L"--moteur") return decid::LancerMoteur(argc - 1, argv + 1, L"--moteur");
    // Rendu logiciel par defaut : ~35 Mo au repos au lieu de ~110 Mo avec un contexte OpenGL (memoire du pilote
    // graphique), et aucune carte graphique requise (machine virtuelle, bureau a distance). Les images du plan et de la 3D
    // sont de toute facon calculees sur le processeur. OpenGL pour les captures (lecture du framebuffer) ou sur demande
    // (DECID_RENDU=opengl) ; un SLINT_BACKEND deja defini est respecte.
    {
        size_t defini = 0;
        getenv_s(&defini, nullptr, 0, "SLINT_BACKEND");
        if (defini == 0) {
            char rendu[16] = {};
            size_t n = 0;
            getenv_s(&n, rendu, sizeof rendu, "DECID_RENDU");
            bool opengl = std::string(rendu) == "opengl";
            for (int i = 1; i < argc; ++i) opengl = opengl || std::wstring(argv[i]) == L"--capture";
            _putenv_s("SLINT_BACKEND", opengl ? "winit-femtovg" : "winit-software");
        }
    }
    // --- executable portable (DECID_PORTABLE) : debut -- extrait moteur et bibliotheque embarques (app/portable)
    std::string erreurPortable;   // extraction impossible (disque plein, dossier en lecture seule...) : montree a l'ouverture
#ifdef DECID_PORTABLE
    erreurPortable = studio::portable::Preparer();
#endif
    // --- executable portable : fin
    const Options options = Analyser(argc, argv);
    const bool capture = !options.m_fichierCapture.empty();

    auto fenetre = Fenetre::create();
    if (options.m_hauteur > 0 || options.m_largeur > 0)
        fenetre->window().set_size(slint::LogicalSize({options.m_largeur > 0 ? (float)options.m_largeur : 1440.0f, options.m_hauteur > 0 ? (float)options.m_hauteur : 900.0f}));
    const Theme& theme = fenetre->global<Theme>();

    FichierPreferences preferences;
    bool sombre = true, facile = false;
    if (!capture) preferences.Lire(sombre, facile, *fenetre);
    theme.set_sombre(options.m_sombre.value_or(sombre));
    theme.set_facile(options.m_facile.value_or(facile));
    if (!options.m_ecran.empty()) fenetre->set_ecran(slint::SharedString(options.m_ecran));
    if (!options.m_vue.empty()) fenetre->set_vue(slint::SharedString(options.m_vue));
    if (!options.m_etape.empty()) fenetre->set_etape(slint::SharedString(options.m_etape));
    if (!options.m_outil.empty()) fenetre->set_outil(slint::SharedString(options.m_outil));
    if (!options.m_dialogue.empty()) fenetre->set_dialogue(slint::SharedString(options.m_dialogue));
    if (options.m_menu >= 0) fenetre->set_menu_ouvert(options.m_menu);

    // moteur : Decid.exe lui-meme en mode moteur (processus separe), dossier de travail dans %LOCALAPPDATA%\Decid\travail
    wchar_t soiMeme[MAX_PATH * 4];
    GetModuleFileNameW(nullptr, soiMeme, (DWORD)(sizeof(soiMeme) / sizeof(soiMeme[0])));
    app::ClientWorker worker(fs::path(soiMeme), [](std::function<void()> action) {
        slint::invoke_from_event_loop(std::move(action));
    }, L"--moteur");
    studio::Moteur moteur(DossierTravail(), worker);
    studio::Etude etude(*fenetre, moteur);
    studio::Decouvrir decouvrir(*fenetre, moteur, etude);
    studio::BacASable bacASable(*fenetre, moteur);
    studio::Validation validation(*fenetre, moteur, etude);
    etude.FixerOuvertureModule("validation", [&](const std::string& t) { validation.MontrerTache(t); });
    studio::PasAPas pasAPas(*fenetre, moteur, etude);
    etude.FixerOuvertureModule("pas-a-pas", [&](const std::string& t) { pasAPas.MontrerTache(t); });
    studio::Regression regression(*fenetre, moteur, etude);
    studio::Generateur generateur(*fenetre, moteur, etude);
    studio::Decision decision(*fenetre, moteur, etude);
    studio::AtelierImage atelier(*fenetre, moteur);
    studio::Annexes annexes(*fenetre, moteur, etude);
    fenetre->on_dialogue_ouvert([&](slint::SharedString id) { annexes.DialogueOuvert(std::string(id)); });
    fenetre->global<::Preferences>().on_changees([&] { if (!capture) preferences.Ecrire(theme.get_sombre(), theme.get_facile(), *fenetre); etude.Repeindre(); });
    fenetre->on_outil_ouvert([&](slint::SharedString id) { if (id == "decision") decision.Actualiser(); });
    if (options.m_jeu >= 0) fenetre->global<Etude>().invoke_choisir_jeu(options.m_jeu);
    if (options.m_famille >= 0) fenetre->global<Etude>().invoke_choisir_famille(options.m_famille);
    if (options.m_valeur >= 0) fenetre->global<Etude>().invoke_choisir_valeur(options.m_valeur);
    if (!options.m_importer.empty()) {
        etude.ImporterFichier(options.m_importer, options.m_lim, options.m_utiliser);
        if (!options.m_utiliser) fenetre->set_sous_vue_donnees(1);
    }
    if (options.m_entrainer) etude.Entrainer();
    if (options.m_effets) fenetre->global<Etude>().invoke_voir_effets();
    if (options.m_annuler >= 0)
        slint::Timer::single_shot(std::chrono::milliseconds(options.m_annuler), [&] { fenetre->global<Etude>().invoke_annuler_calcul(); });
    if (options.m_scenario >= 0) {
        fenetre->set_ecran("decouvrir");
        fenetre->global<Decouvrir>().invoke_ouvrir(options.m_scenario);
        if (options.m_pari >= 0) fenetre->global<Decouvrir>().set_pari(options.m_pari);
    }
    bool fermetureConfirmee = false;

    // ---------- projet .decid
    ProjetOuvert projet;
    projet.m_empreinte = etude.Empreinte();
    const std::vector<app::FiltreFichier> filtresProjet = {{L"Projet Decid", L"*.decid"}};
    auto modifie = [&] { return etude.AExperiences() && etude.Empreinte() != projet.m_empreinte; };
    auto majTitre = [&] {
        const std::string nom = projet.m_chemin.empty() ? (etude.AExperiences() ? "Sans titre" : "") : decid::CheminUtf8(projet.m_chemin.stem());
        fenetre->set_titre_projet(slint::SharedString(nom.empty() ? nom : nom + (modifie() ? " •" : "")));
    };
    // si des modifications seraient perdues, demander d'abord
    auto apresConfirmation = [&](const std::string& question, std::function<void()> action) {
        if (!modifie()) { action(); return; }
        projet.m_apresConfirmation = std::move(action);
        fenetre->set_confirmation(slint::SharedString(question));
    };
    fenetre->on_confirmer([&](bool oui) {
        fenetre->set_confirmation("");
        auto action = std::move(projet.m_apresConfirmation);
        projet.m_apresConfirmation = nullptr;
        if (oui && action) action();
    });
    auto enregistrer = [&](bool sous) {
        if (!etude.AExperiences()) { fenetre->invoke_montrer_toast("Rien à enregistrer : entraînez d’abord au moins un modèle."); return; }
        fs::path chemin = projet.m_chemin;
        if (chemin.empty() || sous) {
            const auto choisi = app::plateforme::ChoisirFichierEnregistrement(L"Enregistrer le projet",
                chemin.empty() ? L"projet.decid" : chemin.filename().wstring(), L"decid", filtresProjet);
            if (!choisi) return;
            chemin = *choisi;
        }
        const std::string texte = etude.Sauvegarder().Ecrire(true);
        auto ecrit = decid::EcrireFichierAtomique(chemin, texte, true);
        if (!ecrit.Ok()) { fenetre->invoke_montrer_toast(slint::SharedString("Enregistrement impossible : " + ecrit.Err().Texte())); return; }
        projet.m_chemin = chemin;
        projet.m_empreinte = etude.Empreinte();
        majTitre();
        fenetre->invoke_montrer_toast(slint::SharedString("Projet enregistré : " + decid::CheminUtf8(chemin.filename())));
    };
    auto ouvrir = [&](const fs::path& chemin) {
        auto texte = decid::LireFichierTexte(chemin);
        if (!texte.Ok() || texte.Valeur().size() > 5 * 1024 * 1024) { fenetre->invoke_montrer_toast("Ouverture impossible : fichier illisible ou trop volumineux."); return; }
        auto json = decid::LireJson(texte.Valeur());
        if (!json.Ok()) { fenetre->invoke_montrer_toast("Ouverture impossible : ce fichier n’est pas un projet Decid."); return; }
        const std::string compteRendu = etude.Charger(json.Valeur());
        if (compteRendu.rfind("Ce fichier", 0) == 0) { fenetre->invoke_montrer_toast(slint::SharedString(compteRendu)); return; }
        projet.m_chemin = chemin;
        projet.m_empreinte = etude.Empreinte();
        majTitre();
        fenetre->invoke_montrer_toast(slint::SharedString("Projet ouvert : " + decid::CheminUtf8(chemin.filename()) + " · " + compteRendu));
    };
    fenetre->on_ouvrir_projet([&] {
        apresConfirmation("Le projet en cours n’est pas enregistré. L’abandonner pour en ouvrir un autre ?", [&] {
            if (const auto chemin = app::plateforme::ChoisirFichier(L"Ouvrir un projet Decid", filtresProjet)) ouvrir(*chemin);
        });
    });
    fenetre->window().on_close_requested([&] {
        if (!capture && !fermetureConfirmee && modifie()) {
            apresConfirmation("Le projet n’est pas enregistré. Quitter Decid quand même ?", [&] {
                fermetureConfirmee = true;
                worker.Arreter();
                fenetre->hide();
            });
            return slint::CloseRequestResponse::KeepWindowShown;
        }
        worker.Arreter();
        return slint::CloseRequestResponse::HideWindow;
    });
    if (!options.m_ouvrir.empty()) ouvrir(options.m_ouvrir);
    if (!options.m_dialogue.empty()) annexes.DialogueOuvert(options.m_dialogue);
    if (options.m_bacExemple) fenetre->global<BacASable>().invoke_exemple();
    if (options.m_valider.find('v') != std::string::npos) validation.Lancer(false);
    if (options.m_pas >= 0) { fenetre->global<PasAPas>().invoke_choisir_algorithme(options.m_pas); pasAPas.Calculer(); }
    if (options.m_regression) { regression.Lire(); regression.Apprendre(); }
    if (options.m_generer >= 0) { generateur.ChoisirMode(options.m_generer); generateur.Generer(); }
    if (!options.m_atelier.empty()) {
        atelier.ChargerImage(options.m_atelier);
        atelier.AjouterZone("fond", 4, 4, 50, 60);
        atelier.AjouterZone("objet", 190, 30, 80, 20);
        atelier.Apprendre();
    }

    // actions que le Slint confie au C++
    fenetre->on_commande([&](slint::SharedString action) {
        const std::string a(action);
        if (a == "titre") { majTitre(); return; }   // l'etude a change (voir empreinte-etude dans decid.slint)
        if (a == "nouveau") {
            apresConfirmation("Le projet en cours n’est pas enregistré. L’abandonner pour en commencer un nouveau ?", [&] {
                etude.Vider();
                projet = ProjetOuvert{};
                projet.m_empreinte = etude.Empreinte();
                fenetre->set_ecran("etude"); fenetre->set_vue("etape"); fenetre->set_etape("donnees");
                majTitre();
            });
            return;
        }
        if (a == "ouvrir") { fenetre->invoke_ouvrir_projet(); return; }
        if (a == "enregistrer" || a == "enregistrer-sous") { enregistrer(a == "enregistrer-sous"); return; }
        if (a.rfind("exp:", 0) == 0) { etude.OuvrirExperience(std::atoi(a.c_str() + 4)); return; }
        if (a == "annuler") {
            if (fenetre->get_vue() == "outil" && fenetre->get_outil() == "bac-a-sable") bacASable.Annuler();
            else fenetre->invoke_montrer_toast("Ctrl+Z annule le dernier point du bac à sable.");
            return;
        }
        if (a == "plein-ecran") { fenetre->window().set_fullscreen(!fenetre->window().is_fullscreen()); return; }
        if (a == "rafraichir") { fenetre->window().request_redraw(); return; }
        fenetre->invoke_montrer_toast("Commande indisponible.");   // action de menu sans traitement (ne devrait pas arriver)
    });

    fenetre->on_preferences_changees([&] {
        if (!capture) preferences.Ecrire(theme.get_sombre(), theme.get_facile(), *fenetre);
        etude.Repeindre();   // les plans sont des images : les refaire aux couleurs du theme
        decouvrir.Repeindre();
        bacASable.Repeindre();
        pasAPas.Repeindre();
        regression.Repeindre();
        generateur.Repeindre();
        atelier.Repeindre();
    });

    // capture : laisser la mise en page se stabiliser, puis lire le framebuffer apres le rendu suivant
    bool captureDemandee = false, premierRendu = false, directLance = false;
    int etapeScenario = 0;
    bool balayageLance = false, pasEtapeFaite = false, decisionLancee = false;
    slint::Timer minuteur;
    auto err = fenetre->window().set_rendering_notifier([&](slint::RenderingState etat, slint::GraphicsAPI) {
        if (etat != slint::RenderingState::AfterRendering || !capture) return;
        if (captureDemandee) {
            captureDemandee = false;
            const auto taille = fenetre->window().size();
            const bool ok = app::CapturerFramebuffer(options.m_fichierCapture, (int)taille.width, (int)taille.height);
            std::printf("{\"evenement\":\"capture\",\"largeur\":%d,\"hauteur\":%d,\"ok\":%s}\n", (int)taille.width,
                        (int)taille.height, ok ? "true" : "false");
            std::fflush(stdout);
            slint::invoke_from_event_loop([] { slint::quit_event_loop(); });
        }
        if (!premierRendu) {
            premierRendu = true;
            slint::invoke_from_event_loop([&] {
                // avec --entrainer, attendre la fin du vrai calcul du moteur avant de capturer
                minuteur.start(slint::TimerMode::Repeated, std::chrono::milliseconds(700), [&] {
                    if ((options.m_entrainer || options.m_effets) && etude.Occupe()) return;
                    if (options.m_bacExemple && bacASable.Occupe()) return;
                    if (!options.m_valider.empty() && validation.Occupe()) return;
                    if (options.m_regression && regression.Occupe()) return;
                    if (options.m_generer >= 0 && generateur.Occupe()) return;
                    if (!options.m_atelier.empty() && atelier.Occupe()) return;
                    if (options.m_decider) {
                        if (!decisionLancee) { decisionLancee = true; fenetre->invoke_ouvrir_outil("decision"); decision.Appliquer(); return; }
                        if (decision.Occupe()) return;
                    }
                    if (options.m_pas >= 0) {
                        if (pasAPas.Occupe()) return;
                        if (!pasEtapeFaite) { pasEtapeFaite = true; auto& gp = fenetre->global<PasAPas>(); gp.invoke_aller_etape(options.m_pasEtape < 0 ? gp.get_nb_etapes() - 1 : options.m_pasEtape); return; }
                    }
                    if (options.m_valider.find('b') != std::string::npos && !balayageLance) { balayageLance = true; validation.Lancer(true); return; }
                    if (options.m_scenario >= 0) {
                        if (decouvrir.Occupe()) return;
                        if (etapeScenario < options.m_etapeScenario) {
                            ++etapeScenario;
                            if (etapeScenario == 1) fenetre->global<Decouvrir>().invoke_lancer();
                            else fenetre->global<Decouvrir>().invoke_manipuler();
                            return;
                        }
                    }
                    if (options.m_direct >= 0 && !directLance) { directLance = true; fenetre->global<Etude>().invoke_regler_en_direct(options.m_direct); return; }
                    minuteur.stop();
                    if (!options.m_enregistrer.empty()) { projet.m_chemin = options.m_enregistrer; enregistrer(false); }
                    auto& g = fenetre->global<Etude>();
                    if (options.m_interieur) g.set_interieur(true);
                    if (options.m_celluleReel >= 0) { g.set_cellule_reel(options.m_celluleReel); g.set_cellule_predit(options.m_cellulePredit); }
                    if (options.m_interieur || options.m_celluleReel >= 0) g.invoke_vue_plan_changee();
                    if (options.m_vue3d) { g.set_vue_3d(true); g.invoke_mode_3d_change(); }
                    captureDemandee = true;
                    fenetre->window().request_redraw();
                });
            });
        }
    });
    if (err && capture) {   // le notifier ne sert qu'a la capture : le rendu logiciel (sans OpenGL) n'en a pas
        std::fprintf(stderr, "{\"erreur\":\"rendering_notifier indisponible\"}\n");
        return 3;
    }

    // la fenetre native n'existe qu'une fois la boucle lancee
    slint::Timer::single_shot(std::chrono::milliseconds(0), [&] { studio::IntegrerBarreTitre(*fenetre); });
    if (!erreurPortable.empty())
        slint::Timer::single_shot(std::chrono::milliseconds(300), [&] {
            fenetre->invoke_montrer_toast(slint::SharedString("Moteur de calcul indisponible : " + erreurPortable));
        });
    fenetre->run();
    worker.Arreter();
    return 0;
}
