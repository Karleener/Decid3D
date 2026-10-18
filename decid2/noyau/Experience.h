// Experience complete (tranche L1) : import -> partage -> normalisation -> apprentissage ->
// sauvegarde -> rechargement verifie -> evaluation -> inspection. Ecrit uniquement dans un dossier
// de staging fourni ; la publication (commit) est faite par l'orchestrateur, pas ici.
#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>
#include "Donnees.h"
#include "Erreur.h"
#include "Json.h"
#include "Modeles.h"
#include "Partage.h"

namespace decid {

namespace fs = std::filesystem;

struct RequeteExperience {
    int m_versionProtocole = 1;
    std::string m_idRequete;
    // Source
    std::string m_typeSource = "csv";          // "csv", "cmc", "roles"
    fs::path m_source;                          // csv ou cmc
    ContratCsv m_contratCsv;
    std::vector<FichierRole> m_fichiersRoles;   // typeSource == "roles"
    std::vector<Relocalisation> m_relocalisations;
    // Partage
    MethodePartage m_methode = MethodePartage::PremiersNParClasse;
    double m_ratio = 0.5;
    std::uint64_t m_graine = 1;
    bool m_inverse = false;
    // Pretraitement
    std::string m_normalisation = "aucune";     // "aucune", "minmax" ou "minmax_lim" (bornes du fichier .lim)
    double m_bas = 0.0, m_haut = 1.0;
    fs::path m_fichierLim;                      // minmax_lim : "normalisation.fichier" (relatif au dossier de requete)
    // Algorithme
    ParametresAlgorithme m_algorithme;          // famille et parametres (voir Modeles.h)
    int m_nbThreads = 1;                        // cv::setNumThreads dans le worker

    ValeurJson VersJson() const;
};
Resultat<RequeteExperience> RequeteDepuisJson(const ValeurJson& json, const fs::path& dossierRequete);

// Evenement de progression : phase et avancement reel (i/n) ou indetermine (n = 0)
using RapporteurEvenement = std::function<void(const std::string& phase, long i, long n, const std::string& detail)>;

// Temps de decision publie dans evaluation.json ("temps_decision") : horloge monotone autour de la seule
// prediction du lot (aucune entree-sortie) ; le lot est repete jusqu'a au moins 1 ms cumulee pour rester
// mesurable, puis le temps est rapporte par echantillon.
struct TempsDecision {
    double m_parEchantillonUs = 0.0;
    long long m_echantillons = 0;
    int m_repetitions = 0;
    std::string m_lot = "du lot de test";   // lot chronometre, repris dans "mesure"
    ValeurJson VersJson() const;   // {par_echantillon_us, echantillons, repetitions, mesure}
};
Resultat<std::vector<int>> PredireChronometre(const Modele& modele, const cv::Mat& x, TempsDecision& temps);

// Execute l'experience ; s'arrete proprement entre deux phases si annulation devient vraie.
// Ecrit dans staging : jeu/, partage.json, normalisation.json, modele.yml, modele.json (manifeste),
// inspection.json, evaluation.json, comptes.csv, predictions.csv, artefacts.json (liste + hashes).
Statut ExecuterExperience(const RequeteExperience& req, const fs::path& staging,
                          const std::atomic<bool>& annulation, const RapporteurEvenement& rapporter);

// Verification independante d'un staging (utilisee par l'orchestrateur avant commit) :
// presence, tailles et hashes de artefacts.json, rechargement du modele, schema et prediction de controle.
Statut VerifierStaging(const fs::path& staging);

} // namespace decid
