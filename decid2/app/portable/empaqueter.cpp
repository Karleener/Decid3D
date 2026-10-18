// Decid - outil de construction de l'executable portable (DECID_PORTABLE).
// Reunit la bibliotheque databasesCSV dans un paquet compresse (API Compression de Windows,
// LZMS), embarque ensuite dans Decid.exe comme ressource RCDATA. Ecrit aussi l'empreinte SHA-256 du paquet, qui
// sert de numero de version pour l'extraction dans %LOCALAPPDATA%\Decid\portable\.
//   decid-empaqueter <sortie.paquet> <sortie-empreinte.txt> <dossier databasesCSV>
// Format avant compression : "DECIDPQ1", u32 nombre d'entrees, puis par entree : u32 longueur du chemin, chemin
// UTF-8 relatif (separateur '/'), u64 taille, octets.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <compressapi.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Sha256.h"

namespace fs = std::filesystem;

namespace {

void AjouterU32(std::vector<char>& v, std::uint32_t x) { v.insert(v.end(), (const char*)&x, (const char*)&x + 4); }
void AjouterU64(std::vector<char>& v, std::uint64_t x) { v.insert(v.end(), (const char*)&x, (const char*)&x + 8); }

bool Lire(const fs::path& p, std::vector<char>& octets) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    octets.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

bool Ecrire(const fs::path& p, const void* donnees, std::size_t taille) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write((const char*)donnees, (std::streamsize)taille);
    return (bool)f;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 4) {
        std::fprintf(stderr, "usage : decid-empaqueter <sortie.paquet> <empreinte.txt> <databasesCSV>\n");
        return 2;
    }
    const fs::path sortie = argv[1], empreinte = argv[2], bibliotheque = argv[3];

    std::vector<std::pair<std::string, fs::path>> entrees;
    std::vector<fs::path> fichiers;
    for (const auto& e : fs::recursive_directory_iterator(bibliotheque))
        if (e.is_regular_file()) fichiers.push_back(e.path());
    std::sort(fichiers.begin(), fichiers.end());
    for (const auto& f : fichiers)
        entrees.push_back({"databasesCSV/" + fs::relative(f, bibliotheque).generic_u8string(), f});

    std::vector<char> brut{'D', 'E', 'C', 'I', 'D', 'P', 'Q', '1'};
    AjouterU32(brut, (std::uint32_t)entrees.size());
    std::vector<char> octets;
    for (const auto& [nom, chemin] : entrees) {
        if (!Lire(chemin, octets)) {
            std::fprintf(stderr, "lecture impossible : %ls\n", chemin.c_str());
            return 1;
        }
        AjouterU32(brut, (std::uint32_t)nom.size());
        brut.insert(brut.end(), nom.begin(), nom.end());
        AjouterU64(brut, octets.size());
        brut.insert(brut.end(), octets.begin(), octets.end());
    }

    COMPRESSOR_HANDLE c = nullptr;
    if (!CreateCompressor(COMPRESS_ALGORITHM_LZMS, nullptr, &c)) {
        std::fprintf(stderr, "CreateCompressor : erreur %lu\n", GetLastError());
        return 1;
    }
    SIZE_T besoin = 0;
    Compress(c, brut.data(), brut.size(), nullptr, 0, &besoin);
    std::vector<char> compresse(besoin);
    SIZE_T obtenu = 0;
    const BOOL ok = Compress(c, brut.data(), brut.size(), compresse.data(), compresse.size(), &obtenu);
    CloseCompressor(c);
    if (!ok) {
        std::fprintf(stderr, "Compress : erreur %lu\n", GetLastError());
        return 1;
    }
    compresse.resize(obtenu);

    decid::Sha256 h;
    h.Ajouter(compresse.data(), compresse.size());
    const std::string hex = h.FinirHex();
    if (!Ecrire(sortie, compresse.data(), compresse.size()) || !Ecrire(empreinte, hex.data(), hex.size())) {
        std::fprintf(stderr, "ecriture du paquet impossible\n");
        return 1;
    }
    std::printf("paquet Decid : %zu entrees, %zu -> %zu octets, empreinte %s\n", entrees.size(), brut.size(), compresse.size(), hex.c_str());
    return 0;
}
