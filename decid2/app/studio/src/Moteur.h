// Acces au moteur pour l'application Decid : un dossier de travail (requetes/ + taches/, format Projet) et le
// client du worker. Chaque calcul est une requete JSON executee par `Decid.exe --moteur executer` ; la suite est appelee
// dans le thread de l'interface quand la tache est terminee. L'interface ne calcule rien elle-meme.
#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ClientWorker.h"
#include "Json.h"
#include "Projet.h"

namespace studio {
namespace fs = std::filesystem;

struct FinCalcul {
    bool m_ok = false;
    std::string m_id;          // identifiant de la tache (= nom de son dossier)
    fs::path m_resultat;       // <taches>/<id>/resultat
    std::string m_erreur;      // message lisible si echec
};

class Moteur {
public:
    using Suite = std::function<void(const FinCalcul&)>;
    using Progression = std::function<void(const std::string& phase, long i, long n)>;

    // dossierTravail : cree au besoin (format Projet) ; worker : client deja construit.
    Moteur(const fs::path& dossierTravail, app::ClientWorker& worker);
    ~Moteur();

    bool Pret() const { return m_projet != nullptr; }
    const std::string& ErreurInitiale() const { return m_erreurInitiale; }

    // Lance une requete (operation "experience" si absente). Renvoie faux et appelle suite tout de suite si le
    // lancement echoue (tache lourde deja en cours, worker absent...).
    bool Executer(decid::ValeurJson requete, Suite suite, Progression progression = {});
    bool Occupe() const;
    void AnnulerTout();

    // Reference a une tache dans une requete ("../taches/<id>", relative au dossier des requetes)
    static std::string RefTache(const std::string& id) { return "../taches/" + id; }
    fs::path DossierResultat(const std::string& id) const;
    fs::path DossierTravail() const;
    // Donnees creees par l'interface (points du bac a sable, image de l'atelier...) : ecrites une fois dans
    // <travail>/donnees/<empreinte>.<extension>, jamais modifiees ensuite ; renvoie leur chemin absolu.
    decid::Resultat<fs::path> EcrireDonnees(const std::string& contenu, const std::string& extension);

private:
    app::ClientWorker& m_worker;
    std::unique_ptr<app::Projet> m_projet;
    std::string m_erreurInitiale;
    std::map<std::string, Suite> m_suites;
    std::map<std::string, Progression> m_progressions;
    int m_abonneFin = -1;
    int m_abonneProgression = -1;
};

// ---------- Lecture des artefacts publies ----------
decid::Resultat<decid::ValeurJson> LireJson(const fs::path& chemin);
template <typename T> std::vector<T> LireBinaire(const fs::path& chemin);   // little-endian, taille multiple de T

struct JeuPublie {                 // resultat/jeu : jeu canonique APRES normalisation
    std::vector<std::string> m_attributs;
    std::vector<std::string> m_classes;
    std::vector<double> m_valeurs; // lignes x attributs
    std::vector<std::int32_t> m_labels;
    std::vector<int> m_apprentissage, m_test;   // indices (partage.json)
    int Attributs() const { return (int)m_attributs.size(); }
    int Lignes() const { return (int)m_labels.size(); }
};
decid::Resultat<JeuPublie> LireJeuPublie(const fs::path& resultat);

struct Evaluation {
    double m_exactitude = 0.0;
    bool m_definie = false;
    std::string m_population;                    // "test" ou "apprentissage"
    std::vector<std::string> m_classes;
    std::vector<std::vector<long long>> m_confusion;   // [reel][predit]
    double m_tempsDecisionUs = -1.0;             // par echantillon, -1 si absent
};
decid::Resultat<Evaluation> LireEvaluation(const fs::path& resultat);

struct ImageMoteur {               // operation image : classes et (si defini) confiance par pixel
    int m_largeur = 0, m_hauteur = 0;
    double m_xMin = 0, m_xMax = 1, m_yMin = 0, m_yMax = 1;
    std::vector<std::int16_t> m_classes;         // ligne 0 = y max
    std::vector<std::uint8_t> m_scores;          // vide si le score n'est pas defini
    std::string m_definitionScore;               // ou la raison de son absence
};
decid::Resultat<ImageMoteur> LireImage(const fs::path& resultat);

// predictions.csv (« ligne;origine;verite;prediction ») : classe predite par ligne du jeu, -1 hors test
std::vector<int> LirePredictions(const fs::path& resultat, const std::vector<std::string>& classes, int lignes);

} // namespace studio
