// Jeu de donnees immuable : lecture CSV (contrat explicite), CMC/DAT, fichiers par role.
// Aucune ecriture n'est jamais faite a cote des sources.
#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include "Erreur.h"

namespace decid {

namespace fs = std::filesystem;

enum class RoleFichier { Aucun, Apprentissage, Test };
const char* NomRole(RoleFichier role);

struct SourceFichier {
    std::string m_cheminDeclare;   // tel qu'ecrit (CMC) ou fourni
    std::string m_cheminResolu;    // chemin absolu effectivement lu (UTF-8)
    std::string m_sha256;          // octets sources
    std::uintmax_t m_taille = 0;
    RoleFichier m_role = RoleFichier::Aucun;
    std::string m_classeDeclaree;  // label externe impose par le CMC / le role, vide sinon
};

struct OrigineLigne {
    int m_source = 0;              // indice dans m_sources
    long m_ligne = 0;              // numero de ligne physique 1-based dans le fichier
};

// Contrat CSV : aucune ligne n'est sautee implicitement (README officiel : CSV sans en-tete).
enum class ModeEnTete { Non, Oui, Auto };
struct ContratCsv {
    char m_separateur = ',';
    ModeEnTete m_enTete = ModeEnTete::Non;
    int m_colonneCible = 0;         // colonne du label (0 = premiere, format Decid)
    bool m_cibleNumerique = false;  // regression : cible reelle quelconque (finie), gardee en texte brut par valeur
    bool m_sansCible = false;       // decision : aucune colonne de label (toutes les colonnes sont des attributs,
                                    // label "?" unique ; m_colonneCible est alors ignore)
};

struct JeuDeDonnees {
    int m_nbLignes = 0;
    int m_nbAttributs = 0;
    std::vector<double> m_valeurs;            // m_nbLignes x m_nbAttributs, ordre lignes, canonique float64
    std::vector<int> m_labels;                // identifiants internes 0..K-1
    std::vector<std::string> m_nomsClasses;   // label externe de chaque identifiant interne
    std::vector<std::string> m_nomsAttributs; // "x1".. si pas d'en-tete
    std::vector<OrigineLigne> m_origines;     // une par ligne
    std::vector<SourceFichier> m_sources;
    std::string m_contrat;                    // description du contrat de lecture effectivement applique
    std::vector<std::string> m_avertissements;// ex. lignes vides ignorees, lignes numeriquement egales

    int NbClasses() const { return (int)m_nomsClasses.size(); }
    double Valeur(int ligne, int attribut) const { return m_valeurs[(size_t)ligne * m_nbAttributs + attribut]; }
    // SHA-256 de la representation canonique canon_v1 (independante du formatage texte et du chemin)
    std::string HashCanonique() const;
    // Comptes par classe
    std::vector<long> ComptesParClasse() const;
};

// Lecture CSV ; Auto ne decide que si la premiere ligne est sans ambiguite, sinon erreur explicite.
Resultat<JeuDeDonnees> LireCsv(const fs::path& chemin, const ContratCsv& contrat);

// Lecture CMC historique : K, nb fichiers par classe, chemins des DAT (relatifs resolus depuis le CMC).
// Table de relocalisation optionnelle : prefixe absolu historique -> dossier existant.
struct Relocalisation { std::string m_prefixeAncien; std::string m_dossierNouveau; };
Resultat<JeuDeDonnees> LireCmc(const fs::path& chemin, const std::vector<Relocalisation>& relocalisations = {});

// Fichiers DAT par classe et par role (ex. optdigits : cN.dat apprentissage, ctesN.dat test).
struct FichierRole { fs::path m_chemin; std::string m_classe; RoleFichier m_role = RoleFichier::Aucun; };
Resultat<JeuDeDonnees> LireFichiersParRole(const std::vector<FichierRole>& fichiers);

// Export CSV sans perte : en-tete versionne optionnel, 17 chiffres significatifs (retour exact float64).
// Refuse d'ecrire sur l'une des sources du jeu ; ecriture atomique.
Statut ExporterCsv(const JeuDeDonnees& jeu, const fs::path& destination, bool avecEnTete, bool remplacer);

// Sauvegarde / relecture binaire du jeu (jeu.json + valeurs.f64 + labels.i32 + origines.i64)
Statut SauverJeu(const JeuDeDonnees& jeu, const fs::path& dossier);
Resultat<JeuDeDonnees> ChargerJeu(const fs::path& dossier);

} // namespace decid
