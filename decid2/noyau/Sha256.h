// SHA-256 (FIPS 180-4), implementation locale sans dependance.
#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace decid {

class Sha256 {
public:
    Sha256();
    void Ajouter(const void* donnees, std::size_t taille);
    void Ajouter(const std::string& texte) { Ajouter(texte.data(), texte.size()); }
    std::string FinirHex();          // 64 caracteres hexadecimaux minuscules ; l'objet ne doit plus servir ensuite
private:
    void TraiterBloc(const std::uint8_t* bloc);
    std::uint32_t m_etat[8];
    std::uint8_t m_tampon[64];
    std::size_t m_tailleTampon = 0;
    std::uint64_t m_nbOctets = 0;
};

std::string Sha256Texte(const std::string& texte);
// Hash d'un fichier lu par blocs ; renvoie "" si le fichier est illisible.
std::string Sha256Fichier(const std::filesystem::path& chemin, std::uintmax_t* taille = nullptr);

} // namespace decid
