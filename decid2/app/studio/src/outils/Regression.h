// Outil « Regression » : lecture brute du fichier (noms de colonnes), operation regression du moteur, puis coupe du
// modele et nuage predit contre reel dessines depuis regression_courbe.json. Jusqu'a 8 essais compares.
#pragma once
#include <map>
#include <string>
#include <vector>

#include "Etude.h"
#include "Moteur.h"
#include "decid.h"

namespace studio {

class Regression {
public:
    Regression(Fenetre& fenetre, Moteur& moteur, Etude& etude);
    void Lire();
    void Apprendre();
    bool Occupe() const { return m_calcul; }
    void Repeindre() { Peindre(); }

private:
    struct PointR { double m_x, m_cible, m_prediction; bool m_test; };
    struct EssaiR {
        std::string m_titre, m_tache, m_nomAttribut, m_nomCible;
        decid::ValeurJson m_mesures;
        std::vector<double> m_cx, m_cy;
        std::vector<PointR> m_points;
        int m_nTest = 0, m_nApp = 0;
    };
    void ChoisirFamille(int i);
    void MajParametres();
    void MajEntrees();
    void Afficher();
    void Peindre();

    Fenetre& m_fenetre;
    Moteur& m_moteur;
    Etude& m_etude;
    fs::path m_fichierLocal;
    fs::path m_chemin;                       // fichier lu
    std::vector<std::string> m_colonnes;
    std::map<std::string, std::string> m_valeurs;
    std::vector<EssaiR> m_essais;
    int m_courant = -1;
    bool m_calcul = false;
    float m_l = 0, m_h = 0;
    slint::Timer m_redim;      // rendu differe pendant un redimensionnement
};

} // namespace studio
