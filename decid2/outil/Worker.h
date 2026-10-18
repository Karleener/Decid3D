// Execution isolee : orchestrateur (processus parent, seul proprietaire du commit) et worker (enfant).
#pragma once
#include <filesystem>
#include <string>

namespace decid {

namespace fs = std::filesystem;

// Parent : lance le worker dans un Job Object Windows, relaie les evenements, gere l'annulation
// (fichier "annuler" dans le dossier de tache, delai de grace puis arret du groupe de processus),
// verifie le staging, publie (renommage staging -> resultat, puis commit.json en dernier) et ecrit
// un etat terminal unique (etat.json). Code de retour : 0 succes, 2 echec, 3 annulation.
// prefixe : argument place avant "worker" pour relancer le meme executable (voir LigneCommande.h).
int Orchestrer(const fs::path& executable, const std::wstring& prefixe, const fs::path& requete, const fs::path& dossierTaches,
               double delaiArretSecondes);

// Enfant : execute l'experience dans le dossier de staging et ecrit des evenements JSONL sur stdout.
int ExecuterWorker(const fs::path& requete, const fs::path& staging, const fs::path& fichierAnnulation);

// Etat d'une tache : "succeeded", "failed", "cancelled" ou "interrupted" (aucun commit complet).
std::string EtatTache(const fs::path& dossierTache);

} // namespace decid
