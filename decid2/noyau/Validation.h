// Validation croisee, balayage de parametres et courbes (spec v2 §6.4, §7.5 ; ACC-06, 25, 26).
// Le test final n'est jamais utilise pour choisir un parametre.
#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "Donnees.h"
#include "Erreur.h"
#include "Json.h"
#include "Metriques.h"
#include "Modeles.h"

namespace decid {

// Plis : OrdreFichier = blocs contigus dans l'ordre des lignes fournies (exercice du TD M1 2024 :
// sans melange, une CV d'ordre 2 coupe par blocs de classes) ; Melange = melange portable (splitmix64,
// voir TirageUniforme dans Partage.h) puis blocs. Stratifie = repartition par classe (option).
enum class ModePlis { OrdreFichier, Melange };
struct Pli { std::vector<int> m_apprentissage, m_validation; };   // indices de lignes du jeu, tries

struct PlanPlis {
    int m_k = 10;
    ModePlis m_mode = ModePlis::Melange;
    bool m_stratifie = false;
    std::uint64_t m_graine = 1;
    std::vector<int> m_perimetre;   // lignes concernees (ex. l'apprentissage d'un partage) ; jamais le test final
    std::vector<Pli> m_plis;
    ValeurJson VersJson() const;
};
// k dans [2, taille du perimetre] ; pas de rabattement silencieux (erreur sinon).
Resultat<PlanPlis> CreerPlis(const JeuDeDonnees& jeu, const std::vector<int>& perimetre, int k, ModePlis mode,
                             bool stratifie, std::uint64_t graine);

struct ConfigApprentissage {
    ParametresAlgorithme m_algorithme;       // famille et parametres de la famille
    std::string m_normalisation = "aucune";   // ajustee DANS chaque pli sur ses seules lignes d'apprentissage
    double m_bas = 0.0, m_haut = 1.0;
    std::filesystem::path m_fichierLim;       // "minmax_lim" : memes bornes imposees dans chaque pli
    std::uint64_t m_graine = 1;
};

using RapporteurProgression = std::function<void(const std::string& phase, long i, long n)>;

struct RapportValidation {
    PlanPlis m_plan;
    std::vector<RapportMetriques> m_parPli;       // metriques de chaque pli de validation
    Metrique m_exactitudeMoyennePlis;             // moyenne simple des exactitudes par pli
    Metrique m_exactitudeEcartTypePlis;           // ecart-type (n-1) des exactitudes par pli
    RapportMetriques m_horsPli;                   // confusion des predictions hors pli (chaque ligne une fois)
    std::vector<int> m_predictionsHorsPli;        // alignees sur m_plan.m_perimetre
    ValeurJson VersJson() const;                  // noms distincts : "moyenne_par_pli", "hors_pli"
};

Resultat<RapportValidation> ValiderCroise(const JeuDeDonnees& jeu, const PlanPlis& plan,
                                          const ConfigApprentissage& config,
                                          const std::atomic<bool>* annulation, const RapporteurProgression& rapporter);

// Valeurs logarithmiques : debut..fin inclus, 'parDecade' valeurs par decade (ex. 0.01..1000, 1 -> 6 valeurs)
std::vector<double> ValeursLogarithmiques(double debut, double fin, int parDecade);

// Balayage d un parametre par validation croisee : SVM ("gamma", "c"), foret ("nb_arbres", "profondeur_max"),
// KPPV et EM ("k"), Boost ("nb_faibles")
// sur le perimetre fourni. Chaque candidat effectivement execute est persiste avec ses metriques.
struct ResultatCandidat { double m_valeur = 0; RapportValidation m_validation; };
struct RapportBalayage {
    std::string m_parametre;
    std::vector<ResultatCandidat> m_candidats;
    int m_indiceMeilleur = -1;          // meilleure exactitude moyenne par pli ; egalite -> plus petite valeur
    bool m_meilleurEnBord = false;      // optimum au bord de la grille exploree (a signaler)
    std::string m_critere = "exactitude moyenne par pli (validation croisee, sans le test)";
    ValeurJson VersJson() const;
};
Resultat<RapportBalayage> Balayer(const JeuDeDonnees& jeu, const PlanPlis& plan, const ConfigApprentissage& base,
                                  const std::string& parametre, const std::vector<double>& valeurs,
                                  const std::atomic<bool>* annulation, const RapporteurProgression& rapporter);

// Courbe ROC standard sur scores QUALIFIES (classe positive, score croissant = plus positif) :
// points (FPR, TPR, seuil) pour chaque seuil distinct, AUC par trapezes. Refus si une seule classe presente.
struct PointRoc { double m_fpr, m_tpr, m_seuil; };
struct CourbeRoc { std::vector<PointRoc> m_points; double m_auc = 0; std::string m_origineScore; ValeurJson VersJson() const; };
Resultat<CourbeRoc> CalculerRoc(const std::vector<double>& scores, const std::vector<int>& estPositif,
                                const std::string& origineScore);

// Graphe pedagogique des TD (abscisse FNR %, ordonnee 100 - FPR %), un point par modele (ex. par valeur de
// gamma) : ce n'est PAS une ROC, et aucune AUC n'est calculee.
struct PointPedagogique { double m_valeurParametre, m_fnrPourcent, m_cent_moins_fprPourcent; };
std::vector<PointPedagogique> PointsFnrFpr(const RapportBalayage& balayage, int classePositive);

} // namespace decid
