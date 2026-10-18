// Services de la plateforme Windows : dialogues de fichiers et de dossiers (IFileOpenDialog, jamais MessageBox),
// chemins de l'executable, localisation des exemples de bases CSV.
#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace app {

namespace fs = std::filesystem;

struct FiltreFichier {
    std::wstring m_libelle;   // ex. L"Fichiers CSV"
    std::wstring m_motif;     // ex. L"*.csv;*.txt"
};

namespace plateforme {

fs::path CheminExecutable();                 // chemin absolu de l'exe de l'application
fs::path DossierExecutable();                // dossier de l'exe (le worker est lance depuis ce dossier)

// Dialogues modaux par rapport a la fenetre active du thread. Renvoie nullopt si l'utilisateur annule.
std::optional<fs::path> ChoisirDossier(const std::wstring& titre);
std::optional<fs::path> ChoisirFichier(const std::wstring& titre, const std::vector<FiltreFichier>& filtres);
// Boite « Enregistrer sous » : nomPropose sans dossier ; extension (sans point) ajoutee si l'utilisateur l'omet
std::optional<fs::path> ChoisirFichierEnregistrement(const std::wstring& titre, const std::wstring& nomPropose,
                                                     const std::wstring& extension, const std::vector<FiltreFichier>& filtres);

// Dossier d'exemples "databasesCSV" : variable DECID_EXEMPLES, sinon recherche en remontant depuis l'exe.
std::optional<fs::path> DossierExemples();
struct Exemple {
    std::string m_nom;       // ex. "iris"
    fs::path m_chemin;       // fichier CSV
};
std::vector<Exemple> ListerExemples();       // fichiers *.csv, deux niveaux de sous-dossiers au plus

} // namespace plateforme
} // namespace app
