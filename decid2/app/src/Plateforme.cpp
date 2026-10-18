#include "Plateforme.h"

#include <algorithm>
#include <cstdlib>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shobjidl.h>

namespace app::plateforme {

namespace {

// Libere un objet COM a la sortie de portee.
template <class T>
struct Com {
    T* m_p = nullptr;
    Com() = default;
    Com(const Com&) = delete;
    Com& operator=(const Com&) = delete;
    ~Com() {
        if (m_p) m_p->Release();
    }
    T** Adresse() { return &m_p; }
    T* operator->() const { return m_p; }
    explicit operator bool() const { return m_p != nullptr; }
};

// Initialise COM pour le thread de l'interface (sans effet si deja fait par la bibliotheque graphique).
void InitialiserCom() {
    static const bool fait = [] {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        return true;
    }();
    (void)fait;
}

std::optional<fs::path> Dialogue(const std::wstring& titre, bool dossier, const std::vector<FiltreFichier>& filtres) {
    InitialiserCom();
    Com<IFileOpenDialog> d;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(d.Adresse())))) return std::nullopt;
    FILEOPENDIALOGOPTIONS options = 0;
    d->GetOptions(&options);
    options |= FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST;
    if (dossier) options |= FOS_PICKFOLDERS;
    d->SetOptions(options);
    d->SetTitle(titre.c_str());
    std::vector<COMDLG_FILTERSPEC> specs;
    if (!dossier && !filtres.empty()) {
        for (const auto& f : filtres) specs.push_back({f.m_libelle.c_str(), f.m_motif.c_str()});
        d->SetFileTypes((UINT)specs.size(), specs.data());
    }
    if (FAILED(d->Show(GetActiveWindow()))) return std::nullopt;   // annulation
    Com<IShellItem> item;
    if (FAILED(d->GetResult(item.Adresse()))) return std::nullopt;
    PWSTR chemin = nullptr;
    if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &chemin)) || !chemin) return std::nullopt;
    fs::path resultat(chemin);
    CoTaskMemFree(chemin);
    return resultat;
}

} // namespace

fs::path CheminExecutable() {
    std::wstring tampon(32768, L'\0');
    const DWORD n = GetModuleFileNameW(nullptr, tampon.data(), (DWORD)tampon.size());
    tampon.resize(n);
    return fs::path(tampon);
}

fs::path DossierExecutable() { return CheminExecutable().parent_path(); }

std::optional<fs::path> ChoisirDossier(const std::wstring& titre) { return Dialogue(titre, true, {}); }

std::optional<fs::path> ChoisirFichierEnregistrement(const std::wstring& titre, const std::wstring& nomPropose,
                                                     const std::wstring& extension, const std::vector<FiltreFichier>& filtres) {
    InitialiserCom();
    Com<IFileSaveDialog> d;
    if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(d.Adresse())))) return std::nullopt;
    FILEOPENDIALOGOPTIONS options = 0;
    d->GetOptions(&options);
    d->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_OVERWRITEPROMPT);
    d->SetTitle(titre.c_str());
    d->SetFileName(nomPropose.c_str());
    d->SetDefaultExtension(extension.c_str());
    std::vector<COMDLG_FILTERSPEC> specs;
    for (const auto& f : filtres) specs.push_back({f.m_libelle.c_str(), f.m_motif.c_str()});
    if (!specs.empty()) d->SetFileTypes((UINT)specs.size(), specs.data());
    if (FAILED(d->Show(GetActiveWindow()))) return std::nullopt;   // annulation
    Com<IShellItem> item;
    if (FAILED(d->GetResult(item.Adresse()))) return std::nullopt;
    PWSTR chemin = nullptr;
    if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &chemin)) || !chemin) return std::nullopt;
    fs::path resultat(chemin);
    CoTaskMemFree(chemin);
    return resultat;
}

std::optional<fs::path> ChoisirFichier(const std::wstring& titre, const std::vector<FiltreFichier>& filtres) {
    return Dialogue(titre, false, filtres);
}

std::optional<fs::path> DossierExemples() {
    std::error_code ec;
    wchar_t* env = nullptr;
    size_t taille = 0;
    if (_wdupenv_s(&env, &taille, L"DECID_EXEMPLES") == 0 && env) {
        fs::path p(env);
        std::free(env);
        if (fs::is_directory(p, ec)) return p;
    }
    fs::path courant = DossierExecutable();
    for (int niveau = 0; niveau < 8 && !courant.empty(); ++niveau) {
        const fs::path candidat = courant / L"databasesCSV";
        if (fs::is_directory(candidat, ec)) return candidat;
        const fs::path parent = courant.parent_path();
        if (parent == courant) break;
        courant = parent;
    }
    return std::nullopt;
}

std::vector<Exemple> ListerExemples() {
    std::vector<Exemple> liste;
    const auto dossier = DossierExemples();
    if (!dossier) return liste;
    std::error_code ec;
    fs::recursive_directory_iterator it(*dossier, fs::directory_options::skip_permission_denied, ec), fin;
    for (; !ec && it != fin; it.increment(ec)) {
        if (it.depth() > 1) {
            it.disable_recursion_pending();
            continue;
        }
        if (!it->is_regular_file(ec)) continue;
        std::wstring ext = it->path().extension().wstring();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t c) { return (wchar_t)towlower(c); });
        if (ext != L".csv") continue;
        Exemple e;
        e.m_chemin = it->path();
        e.m_nom = it->path().stem().string();
        liste.push_back(std::move(e));
    }
    std::sort(liste.begin(), liste.end(), [](const Exemple& a, const Exemple& b) { return a.m_nom < b.m_nom; });
    return liste;
}

} // namespace app::plateforme
