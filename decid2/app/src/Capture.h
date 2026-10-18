// Capture d'ecran de la fenetre (option --capture) : lecture du framebuffer OpenGL apres un rendu, puis PNG.
#pragma once
#include <cstdint>
#include <filesystem>

namespace app {

// PNG non compresse (blocs "stored" zlib) ; rgba = largeur x hauteur x 4 octets, ligne 0 en haut.
bool EcrirePng(const std::filesystem::path& chemin, const std::uint8_t* rgba, int largeur, int hauteur);

// Lit le framebuffer courant (a appeler pendant le rappel AfterRendering) et l'ecrit en PNG.
bool CapturerFramebuffer(const std::filesystem::path& chemin, int largeur, int hauteur);

} // namespace app
