#include "Erreur.h"

namespace decid {

const char* Erreur::NomCode(CodeErreur code) {
    switch (code) {
    case CodeErreur::Lecture: return "lecture";
    case CodeErreur::DonneesInvalides: return "donnees_invalides";
    case CodeErreur::ModeleIncompatible: return "modele_incompatible";
    case CodeErreur::Ressources: return "ressources";
    case CodeErreur::Calcul: return "calcul";
    case CodeErreur::Annulation: return "annulation";
    case CodeErreur::CrashWorker: return "crash_worker";
    case CodeErreur::EcritureRefusee: return "ecriture_refusee";
    case CodeErreur::Protocole: return "protocole";
    }
    return "inconnu";
}

std::string Erreur::Texte() const {
    std::string t = "[" + std::string(NomCode(m_code)) + "/" + m_phase + "] " + m_message;
    if (!m_fichier.empty()) {
        t += " (" + m_fichier;
        if (m_ligne > 0) t += ", ligne " + std::to_string(m_ligne);
        if (m_colonne > 0) t += ", colonne " + std::to_string(m_colonne);
        t += ")";
    }
    return t;
}

Erreur FaireErreur(CodeErreur code, const std::string& phase, const std::string& message,
                   const std::string& fichier, long ligne, long colonne) {
    Erreur e;
    e.m_code = code;
    e.m_phase = phase;
    e.m_message = message;
    e.m_fichier = fichier;
    e.m_ligne = ligne;
    e.m_colonne = colonne;
    return e;
}

} // namespace decid
