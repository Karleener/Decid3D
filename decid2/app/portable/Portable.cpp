// Decid - extraction du moteur et de la bibliotheque embarques (DECID_PORTABLE), voir Portable.h.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <compressapi.h>
#include <shlobj.h>
#include <GL/gl.h>

#include "Portable.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace studio::portable {
namespace {

std::string Utf8(const fs::path& p) {
    const auto u = p.u8string();
    return std::string(u.begin(), u.end());
}
fs::path DepuisUtf8(const std::string& s) { return fs::path(std::u8string(s.begin(), s.end())); }

struct Ressource {
    const char* m_octets = nullptr;
    std::size_t m_taille = 0;
};

Ressource Trouver(const wchar_t* nom) {
    HRSRC r = FindResourceW(nullptr, nom, MAKEINTRESOURCEW(10) /* RT_RCDATA */);
    if (!r) return {};
    HGLOBAL g = LoadResource(nullptr, r);
    if (!g) return {};
    return {(const char*)LockResource(g), (std::size_t)SizeofResource(nullptr, r)};
}

std::string LireTexte(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

bool Ecrire(const fs::path& p, const char* donnees, std::size_t taille) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(donnees, (std::streamsize)taille);
    f.close();
    return !f.fail();
}

// Chemin relatif sur : ni absolu, ni "..", ni lecteur.
bool CheminSur(const std::string& nom) {
    if (nom.empty() || nom.front() == '/' || nom.find(':') != std::string::npos || nom.find('\\') != std::string::npos) return false;
    for (const auto& partie : DepuisUtf8(nom)) if (partie == "..") return false;
    return true;
}

std::string Extraire(const Ressource& paquet, const fs::path& dossier) {
    DECOMPRESSOR_HANDLE d = nullptr;
    if (!CreateDecompressor(COMPRESS_ALGORITHM_LZMS, nullptr, &d)) return "decompresseur indisponible";
    SIZE_T taille = 0;
    Decompress(d, paquet.m_octets, paquet.m_taille, nullptr, 0, &taille);
    std::vector<char> brut(taille);
    SIZE_T obtenu = 0;
    const BOOL ok = taille > 0 && Decompress(d, paquet.m_octets, paquet.m_taille, brut.data(), brut.size(), &obtenu);
    CloseDecompressor(d);
    if (!ok || obtenu != taille) return "paquet embarque illisible";

    std::size_t pos = 0;
    auto lire = [&](void* sortie, std::size_t n) {
        if (pos + n > brut.size()) return false;
        std::memcpy(sortie, brut.data() + pos, n);
        pos += n;
        return true;
    };
    char magie[8];
    std::uint32_t nombre = 0;
    if (!lire(magie, 8) || std::memcmp(magie, "DECIDPQ1", 8) != 0 || !lire(&nombre, 4)) return "paquet embarque invalide";
    for (std::uint32_t i = 0; i < nombre; ++i) {
        std::uint32_t longueur = 0;
        std::uint64_t octets = 0;
        if (!lire(&longueur, 4) || pos + longueur > brut.size()) return "paquet embarque tronque";
        const std::string nom(brut.data() + pos, longueur);
        pos += longueur;
        if (!lire(&octets, 8) || pos + octets > brut.size() || !CheminSur(nom)) return "paquet embarque tronque";
        if (!Ecrire(dossier / DepuisUtf8(nom), brut.data() + pos, (std::size_t)octets)) return "ecriture impossible dans " + Utf8(dossier);
        pos += (std::size_t)octets;
    }
    return {};
}

// OpenGL 3 ou plus disponible ? Sans pilote graphique (machine virtuelle, bureau a distance, pilote d'affichage
// de base), Windows ne fournit que « GDI Generic » en OpenGL 1.1 et le rendu femtovg ne peut pas s'ouvrir.
bool OpenGlUtilisable() {
    WNDCLASSW classe{};
    classe.lpfnWndProc = DefWindowProcW;
    classe.hInstance = GetModuleHandleW(nullptr);
    classe.lpszClassName = L"DecidSondeOpenGL";
    RegisterClassW(&classe);
    HWND fenetre = CreateWindowExW(0, classe.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 16, 16, nullptr, nullptr, classe.hInstance, nullptr);
    bool ok = false;
    if (fenetre) {
        HDC dc = GetDC(fenetre);
        PIXELFORMATDESCRIPTOR pfd{};
        pfd.nSize = sizeof pfd;
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        const int format = ChoosePixelFormat(dc, &pfd);
        if (format && SetPixelFormat(dc, format, &pfd)) {
            if (HGLRC rc = wglCreateContext(dc)) {
                if (wglMakeCurrent(dc, rc)) {
                    const char* version = (const char*)glGetString(GL_VERSION);
                    ok = version && std::atoi(version) >= 3;
                    wglMakeCurrent(nullptr, nullptr);
                }
                wglDeleteContext(rc);
            }
        }
        ReleaseDC(fenetre, dc);
        DestroyWindow(fenetre);
    }
    UnregisterClassW(classe.lpszClassName, classe.hInstance);
    return ok;
}

// Rendu : main() choisit le rendu logiciel par defaut. Si OpenGL est demande (captures, DECID_RENDU=opengl) mais
// inutilisable sur ce poste, on revient au rendu logiciel. La sonde OpenGL ne tourne que dans ce cas.
void ChoisirRendu() {
    char backend[32] = {};
    std::size_t n = 0;
    getenv_s(&n, backend, sizeof backend, "SLINT_BACKEND");
    if (std::string(backend) == "winit-femtovg" && !OpenGlUtilisable()) _putenv_s("SLINT_BACKEND", "winit-software");
}

} // namespace

std::string Preparer() {
    ChoisirRendu();
    const Ressource paquet = Trouver(L"DECID_PAQUET");
    const Ressource empreinte = Trouver(L"DECID_EMPREINTE");
    if (!paquet.m_octets || !empreinte.m_octets || empreinte.m_taille < 16) return {};
    const std::string emp(empreinte.m_octets, empreinte.m_taille);

    PWSTR local = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local)) || !local) {
        CoTaskMemFree(local);
        return "dossier LOCALAPPDATA introuvable";
    }
    const fs::path racine = fs::path(local) / L"Decid" / L"portable";
    CoTaskMemFree(local);
    const std::wstring nom(emp.begin(), emp.begin() + 16);
    const fs::path dossier = racine / nom;
    auto complet = [&] { return LireTexte(dossier / L"empreinte.txt") == emp && fs::is_directory(dossier / L"databasesCSV"); };

    std::error_code ec;
    if (!complet()) {
        // extraction dans un dossier temporaire propre a ce processus, puis renommage : deux instances lancees
        // ensemble ne voient jamais un moteur a moitie ecrit.
        const fs::path temporaire = racine / (nom + L".tmp" + std::to_wstring(GetCurrentProcessId()));
        fs::remove_all(temporaire, ec);
        fs::create_directories(temporaire, ec);
        std::string erreur = Extraire(paquet, temporaire);
        if (erreur.empty() && !Ecrire(temporaire / L"empreinte.txt", emp.data(), emp.size())) erreur = "ecriture de l'empreinte impossible";
        if (!erreur.empty()) {
            fs::remove_all(temporaire, ec);
            return erreur;
        }
        fs::remove_all(dossier, ec);
        fs::rename(temporaire, dossier, ec);
        if (ec) fs::remove_all(temporaire, ec);   // une autre instance a fini avant nous
        if (!complet()) return "extraction des jeux d'exemple impossible dans " + Utf8(dossier);
        // anciennes versions : supprimees au mieux (un moteur encore en cours d'execution reste en place)
        for (const auto& e : fs::directory_iterator(racine, ec))
            if (e.path().filename() != dossier.filename() && e.path().filename().wstring().find(L".tmp") == std::wstring::npos)
                fs::remove_all(e.path(), ec);
    }

    wchar_t* env = nullptr;
    std::size_t taille = 0;
    const bool dejaDefinie = _wdupenv_s(&env, &taille, L"DECID_EXEMPLES") == 0 && env;
    std::free(env);
    if (!dejaDefinie) _wputenv_s(L"DECID_EXEMPLES", (dossier / L"databasesCSV").c_str());
    return {};
}

} // namespace studio::portable
