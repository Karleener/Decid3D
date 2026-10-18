#include "Fichiers.h"

#include <ctime>
#include <fstream>
#include <iterator>

#ifdef _WIN32
#include <windows.h>
#endif

namespace decid {

namespace {

Erreur ErreurLecture(const fs::path& chemin, const std::string& detail) {
    return FaireErreur(CodeErreur::Lecture, "fichiers", "Lecture impossible : " + detail, CheminUtf8(chemin));
}

Erreur ErreurEcriture(const fs::path& chemin, const std::string& detail) {
    return FaireErreur(CodeErreur::EcritureRefusee, "fichiers", "Ecriture refusee : " + detail, CheminUtf8(chemin));
}

bool LireTout(const fs::path& chemin, std::vector<char>& contenu) {
    std::ifstream f(chemin, std::ios::binary);
    if (!f) return false;
    contenu.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return !f.bad();
}

#ifdef _WIN32
// Ecrit puis force sur disque ; renvoie 0 si tout va bien, sinon le code d'erreur Windows.
unsigned long EcrireEtFermer(const fs::path& tmp, const char* donnees, std::size_t taille) {
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return GetLastError();
    unsigned long code = 0;
    std::size_t reste = taille;
    while (reste > 0 && code == 0) {
        const DWORD lot = reste > (1u << 24) ? (1u << 24) : static_cast<DWORD>(reste);
        DWORD ecrit = 0;
        if (!WriteFile(h, donnees, lot, &ecrit, nullptr) || ecrit == 0) {
            code = GetLastError();
            if (code == 0) code = ERROR_WRITE_FAULT;
        } else {
            donnees += ecrit;
            reste -= ecrit;
        }
    }
    if (code == 0 && !FlushFileBuffers(h)) code = GetLastError();
    if (!CloseHandle(h) && code == 0) code = GetLastError();
    return code;
}

unsigned long Renommer(const fs::path& tmp, const fs::path& chemin, bool remplacer) {
    const DWORD options = MOVEFILE_WRITE_THROUGH | (remplacer ? MOVEFILE_REPLACE_EXISTING : 0);
    return MoveFileExW(tmp.c_str(), chemin.c_str(), options) ? 0 : GetLastError();
}
#else
unsigned long EcrireEtFermer(const fs::path& tmp, const char* donnees, std::size_t taille) {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (!f) return 1;
    f.write(donnees, static_cast<std::streamsize>(taille));
    f.flush();
    return f.good() ? 0 : 1;
}

unsigned long Renommer(const fs::path& tmp, const fs::path& chemin, bool) {
    std::error_code ec;
    fs::rename(tmp, chemin, ec);
    return ec ? 1 : 0;
}
#endif

} // namespace

Resultat<std::string> LireFichierTexte(const fs::path& chemin) {
    std::vector<char> contenu;
    if (!LireTout(chemin, contenu)) return ErreurLecture(chemin, "fichier absent ou illisible");
    return std::string(contenu.begin(), contenu.end());
}

Resultat<std::vector<char>> LireFichierBinaire(const fs::path& chemin) {
    std::vector<char> contenu;
    if (!LireTout(chemin, contenu)) return ErreurLecture(chemin, "fichier absent ou illisible");
    return contenu;
}

Statut EcrireFichierAtomique(const fs::path& chemin, const void* donnees, std::size_t taille, bool remplacer) {
    std::error_code ec;
    if (!remplacer && fs::exists(chemin, ec)) {
        return ErreurEcriture(chemin, "le fichier existe deja");
    }
    fs::path tmp = chemin;
    tmp += ".tmp";
    const unsigned long codeEcriture = EcrireEtFermer(tmp, static_cast<const char*>(donnees), taille);
    if (codeEcriture != 0) {
        fs::remove(tmp, ec);
        return ErreurEcriture(chemin, "ecriture du fichier temporaire impossible (code " + std::to_string(codeEcriture) + ")");
    }
    const unsigned long codeRenommage = Renommer(tmp, chemin, remplacer);
    if (codeRenommage != 0) {
        fs::remove(tmp, ec);
        return ErreurEcriture(chemin, "renommage impossible (code " + std::to_string(codeRenommage) + ")");
    }
    return Succes();
}

bool MemeFichier(const fs::path& a, const fs::path& b) {
    std::error_code ec;
    if (fs::exists(a, ec) && fs::exists(b, ec)) {
        const bool egal = fs::equivalent(a, b, ec);
        if (!ec) return egal;
    }
    const fs::path ca = fs::weakly_canonical(a, ec);
    if (ec) return false;
    const fs::path cb = fs::weakly_canonical(b, ec);
    if (ec) return false;
    return ca == cb;
}

Statut VerifierDestination(const fs::path& destination, const std::vector<fs::path>& sources) {
    for (const auto& s : sources) {
        if (MemeFichier(destination, s)) {
            return ErreurEcriture(destination, "la destination est un fichier source protege");
        }
    }
    return Succes();
}

std::string CheminUtf8(const fs::path& chemin) { return chemin.u8string(); }

fs::path CheminDepuisUtf8(const std::string& texte) { return fs::u8path(texte); }

std::string HorodatageUtc() {
    const std::time_t maintenant = std::time(nullptr);
    std::tm t{};
#ifdef _WIN32
    gmtime_s(&t, &maintenant);
#else
    gmtime_r(&maintenant, &t);
#endif
    char tampon[32];
    std::strftime(tampon, sizeof(tampon), "%Y-%m-%dT%H:%M:%SZ", &t);
    return tampon;
}

} // namespace decid
