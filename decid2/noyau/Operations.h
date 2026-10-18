// Operations L2 executees par le worker (meme protocole, staging et publication que l'experience L1).
// Champ "operation" de la requete : "experience" (defaut, L1), "image", "volume", "validation", "balayage",
// "adaboost_pas_a_pas", "perceptron", "kmeans", "mlp_pas_a_pas", "em_pas_a_pas" (les operations pas a pas
// ecrivent aussi partage.json), "generer" (Generation.h), "regression" (Regression.h), "echantillonner_couleurs" et
// "segmenter" (Segmentation.h), "decider" (Decision.h : modele publie applique a une autre source CSV).
#pragma once
#include <atomic>
#include <filesystem>
#include <string>
#include "Donnees.h"
#include "Erreur.h"
#include "Experience.h"
#include "Json.h"
#include "Modeles.h"
#include "Normalisation.h"
#include "Partage.h"

namespace decid {

namespace fs = std::filesystem;

// Resultat d'une experience publiee (dossier <tache>/resultat), relu et verifie (commit coherent exige).
struct ExperiencePubliee {
    JeuDeDonnees m_jeu;
    Partage m_partage;
    Normalisation m_normalisation;
    Modele m_modele;
    std::string m_hashModele;        // SHA-256 du fichier modele.yml
};
Resultat<ExperiencePubliee> ChargerExperiencePubliee(const fs::path& dossierTache);

// Lecture de la source d'une requete (bloc "source", meme format que l'experience)
Resultat<JeuDeDonnees> LireSourceRequete(const RequeteExperience& req);

// Execute l'operation decrite par la requete JSON brute dans le staging.
// Fichiers ecrits selon l'operation (tous listes ensuite dans artefacts.json, ecrit en dernier) :
//  image      : image.i16 (classes int16 LE, lignes de haut en bas), image.png (apercu colorise, palette sans
//               bleu), image.json (requete complete, domaine, valeurs fixes, cle de cache, hash, origine du modele)
//  volume     : volume.i16, volume.json
//  validation : plis.json, validation.json
//  balayage   : balayage.json (candidats, meilleur, bord), points_fnr_fpr.json
//  adaboost_pas_a_pas / perceptron / kmeans : trace.json (grandeurs par etat), trace.f32 (vecteurs concatenes,
//               index des decalages dans trace.json), etat_final.json ; kmeans ajoute trace_affectations.i32
//               (affectation de chaque ligne d'apprentissage a chaque etat, voir EcrireTrace)
//  decider    : predictions.csv, evaluation.json, decision.json, comptes.csv si les classes sont connues
Statut ExecuterOperation(const ValeurJson& requete, const fs::path& dossierRequete, const fs::path& staging,
                         const std::atomic<bool>& annulation, const RapporteurEvenement& rapporter);

// Validation complete SANS execution ni ecriture (appelee par l'orchestrateur avant de creer la tache) :
// operation connue, protocole, request_id, blocs lisibles ; pour image/volume, experience source publiee.
Statut ValiderRequete(const ValeurJson& requete, const fs::path& dossierRequete);

// Ecrit artefacts.json (liste, tailles, hashes) : a appeler en dernier dans le staging.
Statut EcrireArtefacts(const fs::path& staging);

} // namespace decid
