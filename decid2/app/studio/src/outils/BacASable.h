// Outil « Bac a sable » : points poses a la souris dans [0, 1]^2, reapprentissage par le moteur 400 ms apres le dernier
// changement, image de decision et confiance. Une seule demande compte : un calcul perime est abandonne puis relance.
#pragma once
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "Moteur.h"
#include "Plan.h"
#include "decid.h"

namespace studio {

class BacASable {
public:
    BacASable(Fenetre& fenetre, Moteur& moteur);
    void Repeindre();
    void Annuler();                       // Ctrl+Z
    bool Occupe() const { return m_enCours || m_minuteur.running(); }

private:
    struct Point { double m_x, m_y; int m_classe; };
    void Poser(float x, float y, int bouton, bool maj);
    void Modifie();
    void Lancer();
    void MajCompteurs();
    void MajParametres();
    void Peindre();
    std::string Csv() const;

    Fenetre& m_fenetre;
    Moteur& m_moteur;
    std::vector<Point> m_points;
    std::map<std::string, std::string> m_valeurs;   // « famille.parametre » -> valeur saisie
    std::optional<ImageMoteur> m_image;
    int m_demande = 0;                    // numero de la derniere demande
    bool m_enCours = false;
    float m_l = 0, m_h = 0;
    slint::Timer m_redim;      // rendu differe pendant un redimensionnement
    slint::Timer m_minuteur;
};

} // namespace studio
