// Contexte : etat partage simple de l'application (projet courant, jeu charge, partage choisi, normalisation
// choisie, derniere tache par operation) avec abonnements de notification. Utilise uniquement depuis le thread
// de l'interface. Aucune dependance a Slint ni a OpenCV.
#pragma once
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ClientWorker.h"
#include "Donnees.h"
#include "Erreur.h"
#include "Json.h"
#include "Partage.h"
#include "Projet.h"

namespace app {

enum class Evenement { Projet, Jeu, Partage, Normalisation, Tache };

struct ChoixPartage {
    decid::MethodePartage m_methode = decid::MethodePartage::PremiersNParClasse;
    double m_ratio = 0.5;
    std::uint64_t m_graine = 1;
    bool m_inverse = false;
};

struct ChoixNormalisation {
    bool m_minmax = false;   // false : "aucune" ; true : min-max ajustee sur l'apprentissage seul
    double m_bas = 0.0;
    double m_haut = 1.0;
};

class Contexte {
public:
    using Abonne = std::function<void(Evenement)>;

    // --- abonnements : l'abonne est appele de maniere synchrone a chaque changement du type demande
    int Abonner(Abonne abonne);
    void Desabonner(int identifiant);

    // --- projet
    // Prend possession du projet, lit la source active si elle existe (le projet reste ouvert meme si la source
    // est illisible ; l'erreur est renvoyee). Notifie Projet puis Jeu.
    decid::Statut OuvrirProjet(std::unique_ptr<Projet> projet);
    void FermerProjet();
    bool ProjetOuvert() const { return m_projet != nullptr; }
    Projet* GetProjet() { return m_projet.get(); }
    const Projet* GetProjet() const { return m_projet.get(); }

    // --- jeu courant. Le jeu actif est conserve tant que le nouveau n'est pas valide.
    // Charge la source, l'enregistre dans le projet (si ouvert) et la rend active.
    decid::Statut ActiverSource(SourceProjet source);
    // Remplace le jeu deja lu (aucune relecture) ; efface le partage, puisque ses indices ne valent plus.
    void DefinirJeu(std::shared_ptr<const decid::JeuDeDonnees> jeu, SourceProjet source);
    std::shared_ptr<const decid::JeuDeDonnees> Jeu() const { return m_jeu; }
    const SourceProjet& Source() const { return m_source; }
    std::string HashCourt() const;               // 12 premiers caracteres du hash canonique, vide sans jeu

    // --- partage
    decid::Statut DefinirPartage(const ChoixPartage& choix);   // calcule le partage avec decid_base
    void EffacerPartage();
    const std::optional<ChoixPartage>& GetChoixPartage() const { return m_choixPartage; }
    std::shared_ptr<const decid::Partage> GetPartage() const { return m_partage; }

    // --- normalisation
    void DefinirNormalisation(const ChoixNormalisation& choix);
    const ChoixNormalisation& GetNormalisation() const { return m_normalisation; }

    // --- taches
    // Relie le contexte au client : met a jour la derniere tache de chaque operation a chaque fin de tache.
    void Brancher(ClientWorker& worker);
    void RafraichirTaches();                      // relit taches/ du projet
    std::vector<InfoTache> Taches() const { return m_taches; }
    // Derniere tache (toute issue) / derniere tache reussie d'une operation ; nullptr si aucune.
    const InfoTache* DerniereTache(const std::string& operation) const;
    const InfoTache* DerniereReussie(const std::string& operation) const;
    // Ecrit la requete dans le projet puis la lance via le worker. Renvoie l'identifiant de tache.
    decid::Resultat<std::string> LancerTache(ClientWorker& worker, const std::string& operation, const std::string& famille,
                                             decid::ValeurJson requete, bool lourde = true);
    // Message de la derniere erreur de chargement (lecture de la source a l'ouverture), vide sinon.
    const std::string& DernierMessage() const { return m_dernierMessage; }

private:
    void Notifier(Evenement e);

    std::unique_ptr<Projet> m_projet;
    std::shared_ptr<const decid::JeuDeDonnees> m_jeu;
    SourceProjet m_source;
    std::optional<ChoixPartage> m_choixPartage;
    std::shared_ptr<const decid::Partage> m_partage;
    ChoixNormalisation m_normalisation;
    std::vector<InfoTache> m_taches;
    std::string m_dernierMessage;
    std::vector<std::pair<int, Abonne>> m_abonnes;
    int m_prochain = 1;
};

} // namespace app
