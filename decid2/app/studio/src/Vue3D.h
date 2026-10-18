// Vue 3D du volume de decision publie par le moteur : voxels a la frontiere entre
// classes, translucides et plus opaques la ou le moteur est sur de lui, points du jeu, cube de repere. Rendu
// logiciel (peintre, du plus loin au plus proche) dans un tampon RGBA8 ; refait seulement sur interaction.
#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>

#include <slint.h>

#include "Erreur.h"

namespace studio {

struct VolumeMoteur {                      // operation volume
    int m_resolution = 0;
    double m_min[3] = {0, 0, 0}, m_max[3] = {1, 1, 1};
    std::vector<std::int16_t> m_classes;   // z, puis y, puis x (x varie le plus vite)
    std::vector<std::uint8_t> m_scores;    // vide si non defini
};

// Lit resultat/volume.json, volume.i16 et (si defini) volume_score.u8
decid::Resultat<VolumeMoteur> LireVolume(const std::filesystem::path& resultat);

class Vue3D {
public:
    // points : coordonnees dans le domaine du volume ; classe ; opacite
    struct Point { double m_x, m_y, m_z; int m_classe; float m_opacite; };

    void Preparer(const VolumeMoteur& volume, const std::vector<Point>& points);
    void Tourner(float dx, float dy);                // pixels de glisser
    void Zoomer(float crans);                         // molette
    void FixerDonneesSeules(bool v) { m_donneesSeules = v; }
    void FixerTransparence(float t) { m_transparence = t; }   // alpha de base des voxels
    slint::Image Rendre(int largeur, int hauteur, bool sombre, float echelleEcran) const;
    bool Pret() const { return m_pret; }

private:
    struct Voxel { float m_x, m_y, m_z; int m_classe; float m_score; };   // score < 0 : non defini
    struct PointN { float m_x, m_y, m_z; int m_classe; float m_opacite; };
    std::vector<Voxel> m_voxels;
    std::vector<PointN> m_points;
    float m_taille = 0.05f;
    float m_yaw = -0.7f, m_pitch = 0.45f, m_zoom = 1.0f;
    float m_transparence = 0.16f;
    bool m_donneesSeules = false;
    bool m_pret = false;
};

} // namespace studio
