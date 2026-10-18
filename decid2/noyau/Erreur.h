// Erreur structuree et resultat de fonction (aucune exception ne traverse le noyau).
#pragma once
#include <string>
#include <utility>
#include <variant>

namespace decid {

// Classes d'erreur minimales (spec v2 §8.3)
enum class CodeErreur {
    Lecture,            // fichier illisible, absent
    DonneesInvalides,   // valeur, label, dimension incoherents
    ModeleIncompatible, // modele corrompu ou schema different
    Ressources,         // memoire, taille au-dela du budget
    Calcul,             // echec OpenCV
    Annulation,
    CrashWorker,
    EcritureRefusee,
    Protocole
};

struct Erreur {
    CodeErreur m_code = CodeErreur::DonneesInvalides;
    std::string m_phase;     // "import", "partage", "normalisation", ...
    std::string m_message;   // message utilisateur en francais
    std::string m_fichier;   // fichier concerne, vide sinon
    long m_ligne = 0;        // 1-based, 0 = sans objet
    long m_colonne = 0;      // 1-based, 0 = sans objet
    bool m_reprisePossible = true;

    std::string Texte() const;        // message complet lisible
    static const char* NomCode(CodeErreur code);
};

// Resultat<T> : soit une valeur, soit une erreur.
template <class T>
class Resultat {
public:
    Resultat(T valeur) : m_contenu(std::move(valeur)) {}
    Resultat(Erreur erreur) : m_contenu(std::move(erreur)) {}
    bool Ok() const { return m_contenu.index() == 0; }
    T& Valeur() { return std::get<0>(m_contenu); }
    const T& Valeur() const { return std::get<0>(m_contenu); }
    const Erreur& Err() const { return std::get<1>(m_contenu); }
private:
    std::variant<T, Erreur> m_contenu;
};

struct Rien {};
using Statut = Resultat<Rien>;
inline Statut Succes() { return Statut(Rien{}); }

// Raccourci de construction
Erreur FaireErreur(CodeErreur code, const std::string& phase, const std::string& message,
                   const std::string& fichier = "", long ligne = 0, long colonne = 0);

} // namespace decid
