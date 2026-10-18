// Outil « Pas a pas » : AdaBoost, perceptron, k-moyennes, EM, reseau Decid. Le moteur publie une trace (un etat par
// etape : grandeurs + vecteur float32 + affectations) ; l'ecran la rejoue sans rien recalculer. Pour le reseau, les
// regions de chaque epoque viennent de l'operation « activations » du moteur.
#pragma once
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Etude.h"
#include "Moteur.h"
#include "Toile.h"
#include "decid.h"

namespace studio {

class PasAPas {
public:
    PasAPas(Fenetre& fenetre, Moteur& moteur, Etude& etude);
    void Calculer();
    void UneEtape();          // F5
    void Reinitialiser();     // F6
    bool Occupe() const { return m_calcul || m_activationsEnCours; }
    void Repeindre() { Afficher(); }
    void MontrerTache(const std::string& tache);

private:
    struct Etat {
        std::string m_phase;
        long m_iteration = 0;
        std::map<std::string, double> m_g;
        std::vector<float> m_v;
        std::vector<int> m_aff;
        bool m_depart = false;
    };
    struct Trace {
        std::string m_id;
        int m_algo = 0;
        std::vector<Etat> m_etats;
        decid::ValeurJson m_final;
        std::vector<int> m_apprentissage;      // indices des lignes du jeu (partage.json)
        int m_axeX = 0, m_axeY = 1, m_classePositive = 0;
        std::string m_jeuNom, m_reglage;
        double m_ratio = 0.5;
        long long m_graine = 1;
        long m_pas = 1, m_recus = 0;
    };

    void ChoisirAlgorithme(int i);
    void ChoisirJeu(int i);
    void MajParametres();
    double Param(const std::string& cle, double defaut) const;
    void Afficher();
    void Peindre();
    void Courbe();
    void TexteEtape();
    void DemanderActivations(long epoque);
    void Ajouter();
    void ExporterEm();
    std::pair<std::string, double> ScoreFinal(std::string& population) const;

    Fenetre& m_fenetre;
    Moteur& m_moteur;
    Etude& m_etude;
    std::optional<decid::JeuDeDonnees> m_jeu;
    std::map<std::string, std::string> m_valeurs;            // « algo.parametre » -> valeur saisie
    std::shared_ptr<Trace> m_trace;
    std::map<std::string, std::shared_ptr<Trace>> m_traces;  // par tache (experiences rouvertes)
    DomainePlan m_dom;
    float m_l = 0, m_h = 0;
    slint::Timer m_redim;      // rendu differe pendant un redimensionnement
    bool m_calcul = false;
    slint::Timer m_lecture;
    // reseau : regions par epoque (image construite depuis les cartes de sortie du moteur)
    std::map<long, ImageMoteur> m_regions;
    bool m_activationsEnCours = false;
    long m_epoqueVoulue = -1;
};

} // namespace studio
