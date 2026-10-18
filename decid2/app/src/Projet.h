// Projet Decid 2 : un dossier choisi par l'utilisateur (dossier de travail de l'application).
//   <projet>/projet.json        {schema:"decid.projet/1", nom, cree_utc, sources:[...]}
//   <projet>/requetes/<id>.json requetes envoyees au worker (copie figee)
//   <projet>/taches/<id>/       taches du worker (resultat/, commit.json, etat.json, evenements.jsonl)
// Une source n'est jamais copiee ni modifiee. Aucune dependance a Slint ni a OpenCV.
//
// Format d'une source dans projet.json :
//   {"type":"csv","chemin":"...","contrat":{"separateur":",","en_tete":"non|oui|auto","colonne_cible":0},
//    "hash_canonique":"...","lignes":N,"attributs":P,"classes":K}
//   {"type":"cmc","chemin":"...", ...}
//   {"type":"roles","fichiers":[{"chemin":"...","classe":"...","role":"apprentissage|test|aucun"}], ...}
// La derniere source de la liste est la source active.
#pragma once
#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "Donnees.h"
#include "Erreur.h"
#include "Json.h"

namespace app {

namespace fs = std::filesystem;

struct SourceProjet {
    std::string m_type = "csv";                  // "csv" | "cmc" | "roles"
    std::string m_chemin;                        // csv / cmc
    decid::ContratCsv m_contrat;                 // csv
    std::vector<decid::FichierRole> m_fichiers;  // roles
    std::string m_hashCanonique;
    int m_lignes = 0;
    int m_attributs = 0;
    int m_classes = 0;

    decid::ValeurJson VersJson() const;
    static decid::Resultat<SourceProjet> DepuisJson(const decid::ValeurJson& json);
    std::string Description() const;             // texte court pour l'interface (nom du fichier)
};

// Lit la source sur disque (LireCsv / LireCmc / LireFichiersParRole).
decid::Resultat<decid::JeuDeDonnees> ChargerSource(const SourceProjet& source);
// Renseigne hash, lignes, attributs, classes depuis un jeu charge.
void RenseignerSource(SourceProjet& source, const decid::JeuDeDonnees& jeu);

// Etat d'une tache. m_etat : "succeeded", "failed", "cancelled", "interrupted" (lus dans etat.json)
// ou "running" (pas encore d'etat terminal).
struct InfoTache {
    std::string m_id;
    std::string m_operation;   // "experience" par defaut
    std::string m_famille;     // algorithme.famille de la requete, vide sinon
    std::string m_etat;
    fs::path m_dossier;        // <projet>/taches/<id>
    std::int64_t m_ordre = 0;  // horodatage de la requete (plus grand = plus recent)
};

class Projet {
public:
    // Cree le dossier (et requetes/, taches/) et projet.json. Refuse un dossier qui contient deja un projet.json.
    static decid::Resultat<Projet> Creer(const fs::path& dossier, const std::string& nom);
    static decid::Resultat<Projet> Ouvrir(const fs::path& dossier);

    const fs::path& Dossier() const { return m_dossier; }
    const std::string& Nom() const { return m_nom; }
    const std::string& CreeUtc() const { return m_creeUtc; }
    const std::vector<SourceProjet>& Sources() const { return m_sources; }
    fs::path DossierRequetes() const { return m_dossier / "requetes"; }
    fs::path DossierTaches() const { return m_dossier / "taches"; }
    fs::path DossierTache(const std::string& id) const { return DossierTaches() / id; }

    // Ajoute (ou remplace si meme type et meme chemin) la source, la rend active, ecrit projet.json.
    decid::Statut AjouterSource(const SourceProjet& source);
    decid::Statut Sauver() const;

    // <operation>-<famille>-<horodatage compact>-<4 hex> ; jamais deux fois le meme identifiant.
    std::string NouvelIdTache(const std::string& operation, const std::string& famille);
    // Fixe request_id et protocol_version, ecrit requetes/<id>.json (atomique, jamais ecrase) et renvoie son chemin.
    decid::Resultat<fs::path> EcrireRequete(const std::string& id, decid::ValeurJson requete) const;
    // Taches (un dossier de taches/ par tache) triees de la plus recente a la plus ancienne.
    std::vector<InfoTache> ListerTaches() const;

private:
    fs::path m_dossier;
    std::string m_nom;
    std::string m_creeUtc;
    std::vector<SourceProjet> m_sources;
    std::set<std::string> m_idsDelivres;
};

// Identifiant simple (sans separateur de chemin) accepte par le worker : [a-z0-9_-].
std::string Assainir(const std::string& texte);

} // namespace app
