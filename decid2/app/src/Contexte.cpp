#include "Contexte.h"

#include <algorithm>

namespace app {

using decid::Resultat;
using decid::Statut;
using decid::ValeurJson;

int Contexte::Abonner(Abonne abonne) {
    const int id = m_prochain++;
    m_abonnes.emplace_back(id, std::move(abonne));
    return id;
}

void Contexte::Desabonner(int identifiant) {
    m_abonnes.erase(std::remove_if(m_abonnes.begin(), m_abonnes.end(), [&](const auto& p) { return p.first == identifiant; }),
                    m_abonnes.end());
}

void Contexte::Notifier(Evenement e) {
    auto copie = m_abonnes;   // un abonne peut se desabonner pendant la notification
    for (auto& a : copie) a.second(e);
}

Statut Contexte::OuvrirProjet(std::unique_ptr<Projet> projet) {
    m_projet = std::move(projet);
    m_jeu.reset();
    m_source = SourceProjet();
    m_choixPartage.reset();
    m_partage.reset();
    m_dernierMessage.clear();
    RafraichirTaches();
    Notifier(Evenement::Projet);
    Statut resultat = decid::Succes();
    if (m_projet && !m_projet->Sources().empty()) {
        SourceProjet active = m_projet->Sources().back();
        auto jeu = ChargerSource(active);
        if (jeu.Ok()) {
            RenseignerSource(active, jeu.Valeur());
            m_jeu = std::make_shared<const decid::JeuDeDonnees>(std::move(jeu.Valeur()));
            m_source = std::move(active);
        } else {
            m_dernierMessage = jeu.Err().Texte();
            resultat = jeu.Err();
        }
    }
    Notifier(Evenement::Jeu);
    Notifier(Evenement::Partage);
    return resultat;
}

void Contexte::FermerProjet() {
    m_projet.reset();
    m_jeu.reset();
    m_source = SourceProjet();
    m_choixPartage.reset();
    m_partage.reset();
    m_taches.clear();
    Notifier(Evenement::Projet);
    Notifier(Evenement::Jeu);
    Notifier(Evenement::Partage);
    Notifier(Evenement::Tache);
}

Statut Contexte::ActiverSource(SourceProjet source) {
    auto jeu = ChargerSource(source);
    if (!jeu.Ok()) return jeu.Err();   // le jeu actif est conserve
    RenseignerSource(source, jeu.Valeur());
    auto partage = std::make_shared<const decid::JeuDeDonnees>(std::move(jeu.Valeur()));
    if (m_projet) {
        auto st = m_projet->AjouterSource(source);
        if (!st.Ok()) return st;
    }
    DefinirJeu(std::move(partage), std::move(source));
    return decid::Succes();
}

void Contexte::DefinirJeu(std::shared_ptr<const decid::JeuDeDonnees> jeu, SourceProjet source) {
    m_jeu = std::move(jeu);
    m_source = std::move(source);
    m_choixPartage.reset();
    m_partage.reset();
    Notifier(Evenement::Jeu);
    Notifier(Evenement::Partage);
}

std::string Contexte::HashCourt() const {
    if (!m_jeu) return std::string();
    return m_source.m_hashCanonique.substr(0, 12);
}

Statut Contexte::DefinirPartage(const ChoixPartage& choix) {
    if (!m_jeu) return decid::FaireErreur(decid::CodeErreur::DonneesInvalides, "partage", "aucun jeu de données chargé");
    auto partage = decid::CreerPartage(*m_jeu, choix.m_methode, choix.m_ratio, choix.m_graine, choix.m_inverse);
    if (!partage.Ok()) return partage.Err();
    m_partage = std::make_shared<const decid::Partage>(std::move(partage.Valeur()));
    m_choixPartage = choix;
    Notifier(Evenement::Partage);
    return decid::Succes();
}

void Contexte::EffacerPartage() {
    m_choixPartage.reset();
    m_partage.reset();
    Notifier(Evenement::Partage);
}

void Contexte::DefinirNormalisation(const ChoixNormalisation& choix) {
    m_normalisation = choix;
    Notifier(Evenement::Normalisation);
}

void Contexte::Brancher(ClientWorker& worker) {
    worker.AbonnerFin([this](const FinTache&) {
        RafraichirTaches();
        Notifier(Evenement::Tache);
    });
}

void Contexte::RafraichirTaches() {
    m_taches = m_projet ? m_projet->ListerTaches() : std::vector<InfoTache>();
}

const InfoTache* Contexte::DerniereTache(const std::string& operation) const {
    for (const auto& t : m_taches)   // deja triees de la plus recente a la plus ancienne
        if (t.m_operation == operation) return &t;
    return nullptr;
}

const InfoTache* Contexte::DerniereReussie(const std::string& operation) const {
    for (const auto& t : m_taches)
        if (t.m_operation == operation && t.m_etat == "succeeded") return &t;
    return nullptr;
}

Resultat<std::string> Contexte::LancerTache(ClientWorker& worker, const std::string& operation, const std::string& famille,
                                            ValeurJson requete, bool lourde) {
    if (!m_projet) return decid::FaireErreur(decid::CodeErreur::Protocole, "tache", "aucun projet ouvert");
    if (operation != "experience") requete["operation"] = operation;
    const std::string id = m_projet->NouvelIdTache(operation, famille);
    auto chemin = m_projet->EcrireRequete(id, std::move(requete));
    if (!chemin.Ok()) return chemin.Err();
    auto lancee = worker.Lancer(chemin.Valeur(), m_projet->DossierTaches(), lourde);
    if (!lancee.Ok()) return lancee.Err();
    RafraichirTaches();
    Notifier(Evenement::Tache);
    return lancee.Valeur();
}

} // namespace app
