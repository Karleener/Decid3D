// Outil « Generer » : classes reglees a la main (ou tracees a la souris en 2D), tirage par le moteur (operation generer),
// relecture de generation.csv, puis « Etudier ce jeu » qui l'ouvre dans l'etude.
#pragma once
#include <array>
#include <string>
#include <vector>

#include "Etude.h"
#include "Moteur.h"
#include "Vue3D.h"
#include "decid.h"

namespace studio {

class Generateur {
public:
    Generateur(Fenetre& fenetre, Moteur& moteur, Etude& etude);
    void Generer();
    bool Occupe() const { return m_calcul; }
    void Repeindre() { Peindre(); }
    void ChoisirMode(int mode);

private:
    struct Classe {
        std::string m_nom;
        double m_effectif = 150;
        std::array<double, 3> m_centre{0.5, 0.5, 0.5};
        std::array<double, 2> m_ecart{0.05, 0.05};   // 2D : x et y ; 3D : [0]
        double m_diametre = 0.4, m_longueur = 0.6, m_tours = 3, m_phase = 0, m_bruit = 0.01;
    };
    std::vector<Classe>& Classes() { return m_classes[(size_t)m_mode]; }
    void MajClasses();
    void Souris(int genre, float x, float y);
    void Invalider();
    void Peindre();
    std::string NomLibre() const;

    Fenetre& m_fenetre;
    Moteur& m_moteur;
    Etude& m_etude;
    int m_mode = 0;
    std::array<std::vector<Classe>, 3> m_classes;
    bool m_calcul = false;
    // resultat : points publies par le moteur
    bool m_resultat = false;
    fs::path m_csv;
    std::vector<std::array<double, 3>> m_points;
    std::vector<int> m_classesPoints;
    Vue3D m_vue3d;
    // geste de souris (2D)
    int m_geste = 0;           // 0 aucun, 1 deplacer un centre, 2 tracer une nouvelle classe
    int m_classeGeste = -1;
    double m_x0 = 0, m_y0 = 0, m_dx = 0, m_dy = 0;
    float m_l = 0, m_h = 0;
    slint::Timer m_redim;      // rendu differe pendant un redimensionnement
};

} // namespace studio
