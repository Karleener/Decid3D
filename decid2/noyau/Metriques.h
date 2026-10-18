// Confusion (lignes = classes reelles, colonnes = classes predites, comptes entiers) et metriques.
// Une metrique indefinie vaut null avec une raison : jamais NaN.
#pragma once
#include <string>
#include <vector>
#include "Erreur.h"
#include "Json.h"

namespace decid {

struct Metrique {
    bool m_definie = false;
    double m_valeur = 0.0;
    std::string m_raison;          // si non definie : cause en francais
    std::string m_population;      // ex. "test, classe 2 positive"
    ValeurJson VersJson() const;   // {value, status, reason, definition_version, population}
};

struct MetriquesClasse {
    long long m_tp = 0, m_fp = 0, m_fn = 0, m_tn = 0;
    long long m_support = 0;       // nombre de lignes reellement de cette classe
    Metrique m_precision, m_rappel, m_specificite, m_f1, m_fpr, m_fnr;
};

struct Confusion {
    int m_nbClasses = 0;
    std::vector<long long> m_comptes; // m_nbClasses x m_nbClasses, [reel * K + predit]
    long long m_total = 0;
    long long Case(int reel, int predit) const { return m_comptes[(size_t)reel * m_nbClasses + predit]; }
};

struct RapportMetriques {
    std::string m_definition = "decid.metrics/v1";
    Confusion m_confusion;
    std::vector<std::string> m_nomsClasses;
    std::vector<MetriquesClasse> m_classes;
    Metrique m_exactitude;                         // accuracy
    Metrique m_precisionMacro, m_rappelMacro, m_f1Macro, m_specificiteMacro; // macro stricte
    Metrique m_precisionPonderee, m_rappelPondere, m_f1Pondere;             // ponderee par le support
    std::vector<int> m_classesExcluesPonderation;  // support nul
    ValeurJson VersJson() const;
    std::string VersCsvComptes() const;            // export tableur : comptes et formules definies
};

// Valide verite et predictions dans [0,K) AVANT toute indexation ; tailles egales ; N > 0.
Resultat<Confusion> CalculerConfusion(const std::vector<int>& verites, const std::vector<int>& predictions,
                                      int nbClasses);
RapportMetriques CalculerMetriques(const Confusion& confusion, const std::vector<std::string>& nomsClasses,
                                   const std::string& population);

} // namespace decid
