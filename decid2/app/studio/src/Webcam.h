// Webcam par Media Foundation (sans OpenCV dans l'interface) : premiere camera video, images RGB32 lues dans un thread
// POSSEDE et JOINT, relayees dans la boucle de l'interface. Aucune image n'est envoyee ailleurs qu'au moteur local.
#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace studio {

struct ImageRgba {
    int m_largeur = 0, m_hauteur = 0;
    std::vector<std::uint8_t> m_pixels;   // RGBA8, ligne 0 en haut
};

class Webcam {
public:
    using Dispatcheur = std::function<void(std::function<void()>)>;
    using Abonne = std::function<void(const ImageRgba&)>;
    explicit Webcam(Dispatcheur d) : m_dispatcheur(std::move(d)) {}
    ~Webcam() { Arreter(); }
    Webcam(const Webcam&) = delete;
    Webcam& operator=(const Webcam&) = delete;

    // Demarre la premiere camera ; renvoie un message d'erreur (vide si succes). Les images arrivent dans « abonne ».
    std::string Demarrer(Abonne abonne);
    void Arreter();
    bool Active() const { return m_actif; }

private:
    Dispatcheur m_dispatcheur;
    std::thread m_thread;
    std::atomic<bool> m_continuer{false};
    std::atomic<bool> m_actif{false};
};

} // namespace studio
