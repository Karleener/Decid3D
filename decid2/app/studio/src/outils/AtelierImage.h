// Outil « Atelier image » : image fixe ou capture webcam (Media Foundation), zones etiquetees, puis trois taches du
// moteur : echantillonner_couleurs, experience sur les fenetres tirees, segmenter l'image entiere.
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "Moteur.h"
#include "Webcam.h"
#include "decid.h"

namespace studio {

class AtelierImage {
public:
    AtelierImage(::Fenetre& fenetre, Moteur& moteur);
    ~AtelierImage() { m_webcam.Arreter(); }
    void ChargerImage(const fs::path& chemin);   // aussi utilise pour la verification sans boite de dialogue
    void AjouterZone(const std::string& classe, int x, int y, int l, int h);
    void Apprendre();
    bool Occupe() const { return m_calcul; }
    void Repeindre();

private:
    struct ZoneI { std::string m_classe; int m_x, m_y, m_l, m_h; };
    void MajZones();
    void Segmenter(const std::string& experience, const std::string& echantillons);
    void Lancer(decid::ValeurJson requete, const std::string& etape, std::function<void(const FinCalcul&)> suite);
    std::string Espace() const;
    int TailleFenetre() const;

    ::Fenetre& m_fenetre;
    Moteur& m_moteur;
    Webcam m_webcam;
    ImageRgba m_derniereImage;          // derniere image de la camera
    fs::path m_image;                   // image envoyee au moteur
    int m_largeur = 0, m_hauteur = 0;
    std::vector<std::string> m_classes{"fond", "objet"};
    std::vector<ZoneI> m_zones;
    bool m_calcul = false;
    // modele appris
    std::string m_experience, m_echantillons, m_espaceModele;
    int m_fenetreModele = 0;
    double m_exactitude = -1;
    long long m_fenetresApprises = 0;
    std::string m_algoModele;
    // segmentation
    std::string m_tacheSegmentation;
    std::vector<std::int16_t> m_classesPixels;
    std::vector<int> m_idsClasses;
    int m_captures = 0;
};

} // namespace studio
