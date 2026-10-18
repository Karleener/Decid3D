// Operation "decider" : applique le modele d'une experience publiee a une autre source CSV (parite avec
// « Decision sur fichier de donnees » de l'ancien Decid).
// Requete :
//  {"operation":"decider", "protocol_version":1, "request_id":"...",
//   "experience": "<dossier d'une tache experience publiee>" (relatif au dossier de requete),
//   "source": {"type":"csv", "chemin", "separateur":",", "en_tete":"non"|"oui"|"auto",
//              "colonne_cible": n (0 = premiere) ou -1 (fichier sans classes)}}
// Controles : meme nombre d'attributs que le modele ; si la source a un en-tete, memes noms dans le meme ordre.
// Normalisation : celle publiee par l'experience (normalisation.json), jamais reajustee sur la source.
// Classes : verites rapprochees des classes du modele PAR NOM ; une classe inconnue du modele rend l'evaluation
// non evaluable (les predictions restent publiees).
// Fichiers ecrits : predictions.csv (ligne;origine;verite;prediction, verite vide sans classes), decision.json,
// evaluation.json (temps_decision toujours ; rapport et comptes.csv si les classes sont connues).
#pragma once
#include <atomic>
#include <filesystem>
#include "Erreur.h"
#include "Json.h"

namespace decid {

Statut ExecuterDecision(const ValeurJson& requete, const std::filesystem::path& dossierRequete,
                        const std::filesystem::path& staging, const std::atomic<bool>& annulation);

// Validation sans execution : source lisible au sens du protocole, experience publiee.
Statut ValiderDecision(const ValeurJson& requete, const std::filesystem::path& dossierRequete);

} // namespace decid
