// « Je decouvre » : six questions courtes. Chaque scenario est un pari, puis un
// resultat calcule par le moteur sur la vraie spirale (ou les spirales 3D), puis une manipulation. Les calculs sont
// lances des l'ouverture du scenario, l'un apres l'autre, et gardes en memoire pour la seance.
#pragma once
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Donnees.h"
#include "Etude.h"
#include "Moteur.h"
#include "Plan.h"
#include "Vue3D.h"
#include "decid.h"

namespace studio {

class Decouvrir {
public:
    Decouvrir(Fenetre& fenetre, Moteur& moteur, Etude& etude);
    void Repeindre();
    bool Occupe() const { return !m_file.empty() || m_enCours; }

private:
    struct Config { int m_partage = 0, m_famille = 0, m_valeur = 0; std::string m_libelle; };
    struct Resultat {
        std::string m_tache;
        double m_exactitude = 0;
        bool m_definie = false;
        JeuPublie m_jeu;
        std::vector<int> m_prediction;
        std::optional<ImageMoteur> m_image;
        std::optional<VolumeMoteur> m_volume;
    };
    struct Definition {
        std::string m_titre, m_resume;
        std::vector<std::string> m_badges;
        int m_jeu = 1;   // indice du catalogue (1 : spirale)
        std::string m_question;
        std::vector<std::pair<std::string, std::string>> m_paris;
        int m_bon = -1;
        std::string m_explication;
        Config m_principal;
        std::string m_explorerNom, m_explorerAide;
        std::vector<Config> m_explorer;
        bool m_espace = false;     // trois modeles en 3D
        bool m_reseau = false;     // renvoie vers l'outil Pas a pas du reseau
    };

    void Ouvrir(int i);
    void Fermer();
    void Lancer();
    void Manipuler();
    void Explorer(int k);
    void ChoisirAlgo3D(int k);
    void VersEtude();
    void Afficher();
    void Peindre();
    void Demander(const Config& c, bool volume);
    void Suivant();
    std::string Cle(const Config& c) const;
    const Definition& Def() const;

    Fenetre& m_fenetre;
    Moteur& m_moteur;
    Etude& m_etude;
    std::vector<Definition> m_definitions;
    std::map<int, decid::JeuDeDonnees> m_donnees;      // jeu de la bibliotheque -> lu pour l'etape du pari
    std::map<std::string, std::shared_ptr<Resultat>> m_resultats;
    std::deque<std::pair<Config, bool>> m_file;        // calculs en attente (config, avec volume)
    bool m_enCours = false;
    int m_courant = -1, m_etape = 0, m_explore = 0, m_algo3d = 0;
    float m_l = 0, m_h = 0;
    slint::Timer m_redim;      // rendu differe pendant un redimensionnement
    Vue3D m_vue3d;
    std::string m_vue3dCle;
};

} // namespace studio
