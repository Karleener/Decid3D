// Commandes du moteur Decid, partagees par DecidMoteur.exe (ligne de commande) et par Decid.exe, qui se relance
// lui-meme en mode moteur (Decid.exe --moteur <commande> ...) pour chaque calcul.
//   DecidMoteur importer   --csv F [--separateur ,] [--en-tete non|oui|auto] [--cible 0] | --cmc F | --roles R.json
//                     [--sauver DOSSIER]
//   DecidMoteur exporter-csv (memes options de source) --sortie F [--avec-en-tete] [--remplacer]
//   DecidMoteur executer   --requete R.json --taches DOSSIER [--delai-arret 5]
//   DecidMoteur etat       --tache DOSSIER
//   DecidMoteur annuler    --tache DOSSIER
//   DecidMoteur inspecter  --tache DOSSIER
//   DecidMoteur worker     ... (usage interne de l'orchestrateur)
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "Donnees.h"
#include "Fichiers.h"
#include "Json.h"
#include "LigneCommande.h"
#include "Worker.h"

using namespace decid;

namespace {

std::string Utf8(const std::wstring& w) {
    return CheminUtf8(fs::path(w));
}

struct Arguments {
    std::string m_commande;
    std::map<std::string, std::wstring> m_options;    // --cle valeur
    std::map<std::string, bool> m_drapeaux;          // --cle seul
    bool A(const std::string& cle) const { return m_options.count(cle) > 0; }
    fs::path Chemin(const std::string& cle) const { return fs::path(m_options.at(cle)); }
    std::string Texte(const std::string& cle, const std::string& defaut = "") const {
        auto it = m_options.find(cle);
        return it == m_options.end() ? defaut : Utf8(it->second);
    }
};

const char* DRAPEAUX[] = {"--avec-en-tete", "--remplacer"};

Arguments Analyser(int argc, wchar_t** argv) {
    Arguments a;
    if (argc > 1) a.m_commande = Utf8(argv[1]);
    for (int i = 2; i < argc; ++i) {
        std::string cle = Utf8(argv[i]);
        bool drapeau = false;
        for (const char* d : DRAPEAUX) if (cle == d) drapeau = true;
        if (drapeau) { a.m_drapeaux[cle.substr(2)] = true; continue; }
        if (cle.rfind("--", 0) == 0 && i + 1 < argc) { a.m_options[cle.substr(2)] = argv[++i]; continue; }
        a.m_options["_inconnu"] = argv[i];
    }
    return a;
}

int Erreur_(const Erreur& e) {
    ValeurJson j = ValeurJson::Objet();
    j["erreur"] = e.Texte();
    std::fprintf(stderr, "%s\n", j.Ecrire(false).c_str());
    return 2;
}

Resultat<JeuDeDonnees> LireSource(const Arguments& a) {
    if (a.A("csv")) {
        ContratCsv c;
        std::string sep = a.Texte("separateur", ",");
        if (sep.size() != 1) return FaireErreur(CodeErreur::Protocole, "cli", "--separateur : un seul caractere");
        c.m_separateur = sep[0];
        std::string e = a.Texte("en-tete", "non");
        if (e == "oui") c.m_enTete = ModeEnTete::Oui;
        else if (e == "auto") c.m_enTete = ModeEnTete::Auto;
        else if (e == "non") c.m_enTete = ModeEnTete::Non;
        else return FaireErreur(CodeErreur::Protocole, "cli", "--en-tete : oui, non ou auto");
        c.m_colonneCible = std::stoi(a.Texte("cible", "0"));
        return LireCsv(a.Chemin("csv"), c);
    }
    if (a.A("cmc")) return LireCmc(a.Chemin("cmc"));
    if (a.A("roles")) {
        auto texte = LireFichierTexte(a.Chemin("roles"));
        if (!texte.Ok()) return texte.Err();
        auto json = LireJson(texte.Valeur());
        if (!json.Ok()) return json.Err();
        std::vector<FichierRole> fichiers;
        fs::path base = a.Chemin("roles").parent_path();
        for (auto& f : json.Valeur().Membre("fichiers").Elements()) {
            FichierRole fr;
            fr.m_chemin = CheminDepuisUtf8(f.Membre("chemin").Texte());
            if (fr.m_chemin.is_relative()) fr.m_chemin = base / fr.m_chemin;
            fr.m_classe = f.Membre("classe").Texte();
            std::string role = f.Membre("role").Texte();
            fr.m_role = role == "apprentissage" ? RoleFichier::Apprentissage : role == "test" ? RoleFichier::Test : RoleFichier::Aucun;
            fichiers.push_back(fr);
        }
        return LireFichiersParRole(fichiers);
    }
    return FaireErreur(CodeErreur::Protocole, "cli", "source manquante : --csv, --cmc ou --roles");
}

ValeurJson Resume(const JeuDeDonnees& jeu) {
    ValeurJson j = ValeurJson::Objet();
    j["lignes"] = jeu.m_nbLignes;
    j["attributs"] = jeu.m_nbAttributs;
    j["classes"] = jeu.NbClasses();
    ValeurJson comptes = ValeurJson::Objet();
    auto c = jeu.ComptesParClasse();
    for (size_t i = 0; i < c.size(); ++i) comptes[jeu.m_nomsClasses[i]] = c[i];
    j["comptes_par_classe"] = comptes;
    j["hash_canonique"] = jeu.HashCanonique();
    j["contrat"] = jeu.m_contrat;
    ValeurJson sources = ValeurJson::Tableau();
    for (auto& s : jeu.m_sources) {
        ValeurJson e = ValeurJson::Objet();
        e["chemin"] = s.m_cheminResolu;
        e["sha256"] = s.m_sha256;
        e["role"] = NomRole(s.m_role);
        sources.Ajouter(e);
    }
    j["sources"] = sources;
    ValeurJson avert = ValeurJson::Tableau();
    for (auto& w : jeu.m_avertissements) avert.Ajouter(w);
    j["avertissements"] = avert;
    return j;
}

int Aide() {
    std::printf("DecidMoteur : importer | exporter-csv | executer | etat | annuler | inspecter (voir README)\n");
    return 1;
}

} // namespace

#ifdef _WIN32
int decid::LancerMoteur(int argc, wchar_t** argv, const std::wstring& prefixe) {
    SetConsoleOutputCP(CP_UTF8);
#else
int decid::LancerMoteur(int argc, char** argv, const std::wstring& prefixe) {
#endif
    Arguments a = Analyser(argc, argv);
    if (a.m_options.count("_inconnu")) return Aide();

    if (a.m_commande == "importer") {
        auto jeu = LireSource(a);
        if (!jeu.Ok()) return Erreur_(jeu.Err());
        if (a.A("sauver")) {
            auto st = SauverJeu(jeu.Valeur(), a.Chemin("sauver"));
            if (!st.Ok()) return Erreur_(st.Err());
        }
        std::printf("%s\n", Resume(jeu.Valeur()).Ecrire().c_str());
        return 0;
    }
    if (a.m_commande == "exporter-csv") {
        if (!a.A("sortie")) return Aide();
        auto jeu = LireSource(a);
        if (!jeu.Ok()) return Erreur_(jeu.Err());
        auto st = ExporterCsv(jeu.Valeur(), a.Chemin("sortie"), a.m_drapeaux.count("avec-en-tete") > 0,
                              a.m_drapeaux.count("remplacer") > 0);
        if (!st.Ok()) return Erreur_(st.Err());
        std::printf("{\"exporte\":%d}\n", jeu.Valeur().m_nbLignes);
        return 0;
    }
    if (a.m_commande == "executer") {
        if (!a.A("requete") || !a.A("taches")) return Aide();
        double delai = std::stod(a.Texte("delai-arret", "5"));
#ifdef _WIN32
        wchar_t chemin[MAX_PATH * 4];
        GetModuleFileNameW(nullptr, chemin, (DWORD)(sizeof(chemin) / sizeof(chemin[0])));
        fs::path executable(chemin);
#else
        fs::path executable(argv[0]);
#endif
        return Orchestrer(executable, prefixe, a.Chemin("requete"), a.Chemin("taches"), delai);
    }
    if (a.m_commande == "worker") {
        if (!a.A("requete") || !a.A("staging") || !a.A("annulation")) return Aide();
        return ExecuterWorker(a.Chemin("requete"), a.Chemin("staging"), a.Chemin("annulation"));
    }
    if (a.m_commande == "etat") {
        if (!a.A("tache")) return Aide();
        std::printf("{\"etat\":\"%s\"}\n", EtatTache(a.Chemin("tache")).c_str());
        return 0;
    }
    if (a.m_commande == "annuler") {
        if (!a.A("tache")) return Aide();
        auto st = EcrireFichierAtomique(a.Chemin("tache") / "annuler", std::string("annulation demandee\n"), true);
        if (!st.Ok()) return Erreur_(st.Err());
        return 0;
    }
    if (a.m_commande == "inspecter") {
        if (!a.A("tache")) return Aide();
        if (EtatTache(a.Chemin("tache")) != "succeeded")
            return Erreur_(FaireErreur(CodeErreur::Protocole, "inspection", "tache non publiee : rien a inspecter"));
        auto texte = LireFichierTexte(a.Chemin("tache") / "resultat" / "inspection.json");
        if (!texte.Ok()) return Erreur_(texte.Err());
        std::printf("%s\n", texte.Valeur().c_str());
        return 0;
    }
    return Aide();
}
