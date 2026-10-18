// Acces fichiers : chemins Unicode, ecriture atomique, garde-fous sur les sources.
#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include "Erreur.h"

namespace decid {

namespace fs = std::filesystem;

Resultat<std::string> LireFichierTexte(const fs::path& chemin);
Resultat<std::vector<char>> LireFichierBinaire(const fs::path& chemin);

// Ecrit dans <chemin>.tmp, ferme et force l'ecriture disque, puis renomme sur le meme volume.
// Refuse d'ecraser un fichier existant si remplacer == false.
Statut EcrireFichierAtomique(const fs::path& chemin, const void* donnees, std::size_t taille,
                             bool remplacer);
inline Statut EcrireFichierAtomique(const fs::path& chemin, const std::string& texte, bool remplacer) {
    return EcrireFichierAtomique(chemin, texte.data(), texte.size(), remplacer);
}

// Vrai si les deux chemins designent le meme fichier (apres resolution).
bool MemeFichier(const fs::path& a, const fs::path& b);

// Refuse une destination qui coincide avec l'une des sources protegees.
Statut VerifierDestination(const fs::path& destination, const std::vector<fs::path>& sources);

// Chemin -> texte UTF-8 (pour manifestes et messages)
std::string CheminUtf8(const fs::path& chemin);
fs::path CheminDepuisUtf8(const std::string& texte);

std::string HorodatageUtc();      // "2026-10-15T21:04:05Z"

} // namespace decid
