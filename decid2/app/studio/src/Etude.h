// Controleur de l'etude : jeu courant (bibliotheque ou fichier importe) et son apercu (Donnees), famille et
// reglage (Modele), entrainement par le moteur puis resultats (plan de decision, score, confusion, confiance), et la
// liste des experiences de la seance. Le Slint ne fait qu'afficher ; tout calcul scientifique est une tache du
// worker (studio::Moteur). Les fichiers de l'utilisateur ne sont jamais modifies.
#pragma once
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <slint.h>

#include "Donnees.h"
#include "Moteur.h"
#include "Plan.h"
#include "Vue3D.h"
#include "decid.h"

namespace studio {

// Jeu courant : ou le lire, comment, et le protocole qui l'accompagne (partage, mise a l'echelle)
struct SourceEtude {
    std::string m_nom;                         // affiche
    std::string m_cle;                         // identite des donnees pour la comparaison des experiences
    fs::path m_chemin;
    bool m_cmc = false;
    decid::ContratCsv m_contrat;
    std::vector<decid::Relocalisation> m_relocalisations;   // .cmc : dossiers des .dat deplaces
    int m_normalisation = 0;                   // 0 min-max, 1 bornes d'un .lim, 2 aucune
    fs::path m_lim;
    std::string m_methode = "aleatoire_stratifie";
    double m_ratio = 0.5;
    long long m_graine = 42;
    std::string Protocole() const;             // « 50 % au hasard, graine 42 »
    std::string CleProtocole() const;
};

struct ExperienceEtude {
    std::string m_titre, m_jeuNom, m_partageNom, m_score;
    std::string m_cleDonnees, m_cleProtocole;
    std::string m_tache, m_image, m_volume;    // taches du moteur (experience, image et volume de decision)
    int m_axeX = 0, m_axeY = 1, m_axeZ = 2;
    int m_famille = 0, m_valeur = 0;
    SourceEtude m_source;                      // pour refaire l'experience (reglage en direct)
    bool m_absente = false;                    // projet rouvert : resultats introuvables dans le dossier de travail
    std::string m_module;                      // experience produite par un outil (« validation », « pas-a-pas »…), vide sinon
};

class Etude {
public:
    Etude(Fenetre& fenetre, Moteur& moteur);

    void Entrainer();
    void OuvrirExperience(int k);
    void Repeindre();                          // theme change : refaire les images
    bool Occupe() const { return m_occupe; }
    // Services pour les autres ecrans (Je decouvre, outils)
    std::optional<SourceEtude> SourceBibliotheque(int jeu, int partage) const;   // partage : 0..3 comme les pilules
    decid::ValeurJson Requete(const SourceEtude& source, int famille, int valeur) const { return RequeteExperience(source, famille, valeur); }
    void OuvrirConfiguration(int jeu, int partage, int famille, int valeur);       // va a l'etape Modele, reglee ainsi
    std::vector<std::string> NomsBibliotheque() const;
    std::string CleBibliotheque(int jeu) const;
    bool ContientTache(const std::string& tache) const;
    // experiences entrainees par l'etude (titre, tache), pour « Appliquer un modele »
    std::vector<std::pair<std::string, std::string>> ExperiencesEntrainees() const;
    // experience produite par un outil : rouverte dans cet outil (voir FixerOuvertureModule)
    void AjouterExperienceModule(const std::string& module, const std::string& tache, const std::string& titre, const std::string& jeuNom,
                                 const std::string& protocoleNom, double score, const std::string& cleDonnees, const std::string& cleProtocole);
    void FixerOuvertureModule(const std::string& module, std::function<void(const std::string& tache)> ouvrir) { m_ouvertures[module] = std::move(ouvrir); }
    // Projet .decid : reglages de l'etude et experiences ; les resultats restent dans le dossier de travail du moteur
    decid::ValeurJson Sauvegarder() const;
    std::string Charger(const decid::ValeurJson& projet);   // renvoie un compte rendu (experiences ecartees, absentes)
    void Vider();
    std::string Empreinte() const;                            // change des que le projet change
    bool AExperiences() const { return !m_experiences.empty(); }
    const std::vector<ExperienceEtude>& Experiences() const { return m_experiences; }
    int Courante() const { return m_courante; }
    // Verification sans boite de dialogue (option --importer) : lit le fichier comme le bouton « Choisir »
    void ImporterFichier(const fs::path& chemin, const fs::path& lim, bool utiliser);

private:
    struct Gaussienne { int m_classe = 0; std::vector<double> m_moyenne; std::vector<std::vector<double>> m_covariance; };
    struct Resultats {                         // artefacts relus d'une experience
        Evaluation m_evaluation;
        JeuPublie m_jeu;
        std::optional<ImageMoteur> m_image;
        std::vector<int> m_prediction;         // par ligne du jeu : classe predite (points de test), -1 sinon
        std::string m_famille;
        std::vector<std::vector<double>> m_vecteursSupport;   // SVM (espace du modele)
        std::vector<Gaussienne> m_gaussiennes;                // Bayes normal
        std::optional<VolumeMoteur> m_volume;
    };

    void RemplirBibliotheque();
    void ChoisirJeu(int i);
    void ChoisirPartage(int i);
    void Adopter(SourceEtude source, decid::JeuDeDonnees donnees);   // devient le jeu courant
    void ChoisirFamille(int i);
    void VoirEffets();
    void EffetSuivant(int famille, int k, SourceEtude source, int axeX, int axeY, std::shared_ptr<int> generation);
    void PeindreApercu();
    void PeindrePlan();
    void Preparer3D();
    void Peindre3D();
    void AfficherResultats();
    void MajExperiences();
    void Occuper(const std::string& phase);
    void Liberer(const std::string& erreur = {});
    decid::ValeurJson RequeteExperience(const SourceEtude& source, int famille, int valeur) const;
    void Lancer(ExperienceEtude x, int remplacer);   // remplacer : indice de l'experience remplacee, -1 pour ajouter
    void Terminer(ExperienceEtude x, int remplacer);

    // importer
    void ImpChoisir();
    void ImpChoisirLim();
    void ImpRelire();
    void ImpUtiliser();

    Fenetre& m_fenetre;
    Moteur& m_moteur;

    SourceEtude m_source;
    std::optional<decid::JeuDeDonnees> m_donnees;   // jeu courant, lu pour l'apercu
    float m_apercuL = 0, m_apercuH = 0, m_planL = 0, m_planH = 0;
    Vue3D m_vue3d;
    slint::Timer m_redim;      // rendu differe pendant un redimensionnement
    slint::Timer m_rotation;   // rotation automatique (preference de l'ancien Decid)

    SourceEtude m_import;                            // fichier en cours d'import (pas encore le jeu courant)
    std::optional<decid::JeuDeDonnees> m_importLu;

    std::vector<ExperienceEtude> m_experiences;
    int m_courante = -1;
    std::map<std::string, std::shared_ptr<Resultats>> m_cache;   // par tache d'experience
    bool m_occupe = false;
    std::map<std::string, std::function<void(const std::string&)>> m_ouvertures;
    std::shared_ptr<int> m_generationEffets = std::make_shared<int>(0);   // invalide un calcul d'effets en cours
    std::shared_ptr<slint::VectorModel<slint::Image>> m_vignettes = std::make_shared<slint::VectorModel<slint::Image>>();
    std::shared_ptr<slint::VectorModel<slint::SharedString>> m_scoresVignettes = std::make_shared<slint::VectorModel<slint::SharedString>>();

    std::shared_ptr<slint::VectorModel<Experience>> m_modeleExperiences = std::make_shared<slint::VectorModel<Experience>>();
    std::shared_ptr<slint::VectorModel<EntreeMenu>> m_menuExperiences = std::make_shared<slint::VectorModel<EntreeMenu>>();
    std::shared_ptr<slint::VectorModel<slint::SharedString>> m_titres = std::make_shared<slint::VectorModel<slint::SharedString>>();
};

} // namespace studio
