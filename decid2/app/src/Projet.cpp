#include "Projet.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <mutex>
#include <random>

#include "Fichiers.h"

namespace app {

using decid::CodeErreur;
using decid::Erreur;
using decid::FaireErreur;
using decid::Resultat;
using decid::Statut;
using decid::ValeurJson;

namespace {

const char* NomEnTete(decid::ModeEnTete m) {
    switch (m) {
    case decid::ModeEnTete::Oui: return "oui";
    case decid::ModeEnTete::Auto: return "auto";
    default: return "non";
    }
}

decid::ModeEnTete EnTeteDepuisNom(const std::string& nom) {
    if (nom == "oui") return decid::ModeEnTete::Oui;
    if (nom == "auto") return decid::ModeEnTete::Auto;
    return decid::ModeEnTete::Non;
}

const char* NomRoleJson(decid::RoleFichier r) {
    switch (r) {
    case decid::RoleFichier::Apprentissage: return "apprentissage";
    case decid::RoleFichier::Test: return "test";
    default: return "aucun";
    }
}

decid::RoleFichier RoleDepuisNom(const std::string& nom) {
    if (nom == "apprentissage") return decid::RoleFichier::Apprentissage;
    if (nom == "test") return decid::RoleFichier::Test;
    return decid::RoleFichier::Aucun;
}

ValeurJson LireJsonFichier(const fs::path& chemin) {
    auto texte = decid::LireFichierTexte(chemin);
    if (!texte.Ok()) return ValeurJson();
    auto json = decid::LireJson(texte.Valeur());
    return json.Ok() ? json.Valeur() : ValeurJson();
}

std::string HorodatageCompact() {
    std::string t = decid::HorodatageUtc();   // 2026-10-15T21:04:05Z
    std::string r;
    for (char c : t)
        if (c != '-' && c != ':' && c != 'Z') r += c;
    return r;                                  // 20261015T210405
}

std::string HexAleatoire(int chiffres) {
    static std::mutex verrou;
    static std::mt19937_64 generateur{std::random_device{}()};
    std::lock_guard<std::mutex> v(verrou);
    std::string r;
    const char* hex = "0123456789abcdef";
    for (int i = 0; i < chiffres; ++i) r += hex[generateur() & 15];
    return r;
}

std::int64_t HorodatageFichier(const fs::path& chemin) {
    std::error_code ec;
    auto t = fs::last_write_time(chemin, ec);
    return ec ? 0 : (std::int64_t)t.time_since_epoch().count();
}

} // namespace

std::string Assainir(const std::string& texte) {
    std::string r;
    for (unsigned char c : texte) {
        if (std::isalnum(c) && c < 128) r += (char)std::tolower(c);
        else if (c == '-' || c == '_') r += (char)c;
        else if (c == ' ' || c == '.') r += '_';
    }
    return r.empty() ? "x" : r;
}

// ------------------------------------------------------------------ SourceProjet

ValeurJson SourceProjet::VersJson() const {
    ValeurJson j = ValeurJson::Objet();
    j["type"] = m_type;
    if (m_type == "roles") {
        ValeurJson liste = ValeurJson::Tableau();
        for (const auto& f : m_fichiers) {
            ValeurJson e = ValeurJson::Objet();
            e["chemin"] = decid::CheminUtf8(f.m_chemin);
            e["classe"] = f.m_classe;
            e["role"] = NomRoleJson(f.m_role);
            liste.Ajouter(e);
        }
        j["fichiers"] = liste;
    } else {
        j["chemin"] = m_chemin;
    }
    ValeurJson c = ValeurJson::Objet();
    c["separateur"] = std::string(1, m_contrat.m_separateur);
    c["en_tete"] = NomEnTete(m_contrat.m_enTete);
    c["colonne_cible"] = m_contrat.m_colonneCible;
    j["contrat"] = c;
    j["hash_canonique"] = m_hashCanonique;
    j["lignes"] = m_lignes;
    j["attributs"] = m_attributs;
    j["classes"] = m_classes;
    return j;
}

Resultat<SourceProjet> SourceProjet::DepuisJson(const ValeurJson& j) {
    if (j.GetType() != ValeurJson::Type::Objet)
        return FaireErreur(CodeErreur::DonneesInvalides, "projet", "source de projet invalide");
    SourceProjet s;
    s.m_type = j.Membre("type").Texte().empty() ? "csv" : j.Membre("type").Texte();
    s.m_chemin = j.Membre("chemin").Texte();
    for (const auto& f : j.Membre("fichiers").Elements()) {
        decid::FichierRole fr;
        fr.m_chemin = decid::CheminDepuisUtf8(f.Membre("chemin").Texte());
        fr.m_classe = f.Membre("classe").Texte();
        fr.m_role = RoleDepuisNom(f.Membre("role").Texte());
        s.m_fichiers.push_back(fr);
    }
    const ValeurJson& c = j.Membre("contrat");
    const std::string sep = c.Membre("separateur").Texte();
    s.m_contrat.m_separateur = sep.empty() ? ',' : sep[0];
    s.m_contrat.m_enTete = EnTeteDepuisNom(c.Membre("en_tete").Texte());
    s.m_contrat.m_colonneCible = (int)c.Membre("colonne_cible").Entier();
    s.m_hashCanonique = j.Membre("hash_canonique").Texte();
    s.m_lignes = (int)j.Membre("lignes").Entier();
    s.m_attributs = (int)j.Membre("attributs").Entier();
    s.m_classes = (int)j.Membre("classes").Entier();
    if (s.m_type == "roles" ? s.m_fichiers.empty() : s.m_chemin.empty())
        return FaireErreur(CodeErreur::DonneesInvalides, "projet", "source de projet sans fichier");
    return s;
}

std::string SourceProjet::Description() const {
    if (m_type == "roles") return std::to_string(m_fichiers.size()) + " fichiers par rôle";
    return decid::CheminDepuisUtf8(m_chemin).filename().string();
}

Resultat<decid::JeuDeDonnees> ChargerSource(const SourceProjet& source) {
    if (source.m_type == "cmc") return decid::LireCmc(decid::CheminDepuisUtf8(source.m_chemin));
    if (source.m_type == "roles") return decid::LireFichiersParRole(source.m_fichiers);
    return decid::LireCsv(decid::CheminDepuisUtf8(source.m_chemin), source.m_contrat);
}

void RenseignerSource(SourceProjet& source, const decid::JeuDeDonnees& jeu) {
    source.m_hashCanonique = jeu.HashCanonique();
    source.m_lignes = jeu.m_nbLignes;
    source.m_attributs = jeu.m_nbAttributs;
    source.m_classes = jeu.NbClasses();
}

// ------------------------------------------------------------------ Projet

Resultat<Projet> Projet::Creer(const fs::path& dossier, const std::string& nom) {
    std::error_code ec;
    if (fs::exists(dossier / "projet.json", ec))
        return FaireErreur(CodeErreur::EcritureRefusee, "projet",
                           "ce dossier contient déjà un projet : ouvrez-le plutôt que de le recréer", decid::CheminUtf8(dossier));
    fs::create_directories(dossier / "requetes", ec);
    if (ec) return FaireErreur(CodeErreur::EcritureRefusee, "projet", "dossier de projet non créable", decid::CheminUtf8(dossier));
    fs::create_directories(dossier / "taches", ec);
    if (ec) return FaireErreur(CodeErreur::EcritureRefusee, "projet", "dossier de tâches non créable", decid::CheminUtf8(dossier));
    Projet p;
    p.m_dossier = fs::absolute(dossier);
    p.m_nom = nom.empty() ? p.m_dossier.filename().string() : nom;
    p.m_creeUtc = decid::HorodatageUtc();
    auto st = p.Sauver();
    if (!st.Ok()) return st.Err();
    return p;
}

Resultat<Projet> Projet::Ouvrir(const fs::path& dossier) {
    const fs::path fichier = dossier / "projet.json";
    auto texte = decid::LireFichierTexte(fichier);
    if (!texte.Ok()) return texte.Err();
    auto json = decid::LireJson(texte.Valeur());
    if (!json.Ok()) return json.Err();
    const ValeurJson& j = json.Valeur();
    if (j.Membre("schema").Texte() != "decid.projet/1")
        return FaireErreur(CodeErreur::DonneesInvalides, "projet", "format de projet inconnu (schéma decid.projet/1 attendu)",
                           decid::CheminUtf8(fichier));
    Projet p;
    p.m_dossier = fs::absolute(dossier);
    p.m_nom = j.Membre("nom").Texte();
    p.m_creeUtc = j.Membre("cree_utc").Texte();
    for (const auto& s : j.Membre("sources").Elements()) {
        auto source = SourceProjet::DepuisJson(s);
        if (!source.Ok()) return source.Err();
        p.m_sources.push_back(source.Valeur());
    }
    std::error_code ec;
    fs::create_directories(p.DossierRequetes(), ec);
    fs::create_directories(p.DossierTaches(), ec);
    return p;
}

Statut Projet::Sauver() const {
    ValeurJson j = ValeurJson::Objet();
    j["schema"] = "decid.projet/1";
    j["nom"] = m_nom;
    j["cree_utc"] = m_creeUtc;
    ValeurJson sources = ValeurJson::Tableau();
    for (const auto& s : m_sources) sources.Ajouter(s.VersJson());
    j["sources"] = sources;
    return decid::EcrireFichierAtomique(m_dossier / "projet.json", j.Ecrire() + "\n", true);
}

Statut Projet::AjouterSource(const SourceProjet& source) {
    // une source identique (meme type, meme chemin) est remplacee puis replacee en derniere position
    m_sources.erase(std::remove_if(m_sources.begin(), m_sources.end(),
                                   [&](const SourceProjet& s) {
                                       return s.m_type == source.m_type && s.m_chemin == source.m_chemin &&
                                              (source.m_type != "roles" || s.VersJson().Ecrire() == source.VersJson().Ecrire());
                                   }),
                    m_sources.end());
    m_sources.push_back(source);
    return Sauver();
}

std::string Projet::NouvelIdTache(const std::string& operation, const std::string& famille) {
    const std::string base = Assainir(operation) + "-" + Assainir(famille.empty() ? "sans" : famille) + "-" + HorodatageCompact() + "-";
    std::error_code ec;
    for (int essai = 0;; ++essai) {
        const std::string id = base + HexAleatoire(essai < 40 ? 4 : 8);
        if (m_idsDelivres.count(id)) continue;
        if (fs::exists(DossierRequetes() / (id + ".json"), ec) || fs::exists(DossierTache(id), ec)) continue;
        m_idsDelivres.insert(id);
        return id;
    }
}

Resultat<fs::path> Projet::EcrireRequete(const std::string& id, ValeurJson requete) const {
    if (requete.GetType() != ValeurJson::Type::Objet)
        return FaireErreur(CodeErreur::Protocole, "requete", "une requête est un objet JSON");
    if (id.empty() || id.find_first_of("/\\:") != std::string::npos)
        return FaireErreur(CodeErreur::Protocole, "requete", "identifiant de tâche invalide : " + id);
    requete["protocol_version"] = 1;
    requete["request_id"] = id;
    const fs::path chemin = DossierRequetes() / (id + ".json");
    auto st = decid::EcrireFichierAtomique(chemin, requete.Ecrire() + "\n", false);
    if (!st.Ok()) return st.Err();
    return chemin;
}

std::vector<InfoTache> Projet::ListerTaches() const {
    std::vector<InfoTache> liste;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(DossierTaches(), ec)) {
        if (!e.is_directory(ec)) continue;
        InfoTache t;
        t.m_id = e.path().filename().string();
        t.m_dossier = e.path();
        const ValeurJson requete = LireJsonFichier(e.path() / "requete.json");
        t.m_operation = requete.Membre("operation").Texte().empty() ? "experience" : requete.Membre("operation").Texte();
        t.m_famille = requete.Membre("algorithme").Membre("famille").Texte();
        const ValeurJson etat = LireJsonFichier(e.path() / "etat.json");
        t.m_etat = etat.Membre("etat").Texte().empty() ? "running" : etat.Membre("etat").Texte();
        const fs::path requeteProjet = DossierRequetes() / (t.m_id + ".json");
        t.m_ordre = fs::exists(requeteProjet, ec) ? HorodatageFichier(requeteProjet) : HorodatageFichier(e.path());
        liste.push_back(std::move(t));
    }
    std::sort(liste.begin(), liste.end(), [](const InfoTache& a, const InfoTache& b) {
        return a.m_ordre != b.m_ordre ? a.m_ordre > b.m_ordre : a.m_id > b.m_id;
    });
    return liste;
}

} // namespace app
