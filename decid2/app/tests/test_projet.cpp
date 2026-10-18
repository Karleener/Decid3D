// Projet : creation, reouverture, identifiants uniques, ecriture des requetes, liste des taches, contexte.
#include <fstream>
#include <set>

#include "Contexte.h"
#include "Fichiers.h"
#include "Projet.h"
#include "Verifier.h"

using namespace app;

namespace {

void Ecrire(const fs::path& chemin, const std::string& texte) {
    std::ofstream f(chemin, std::ios::binary);
    f << texte;
}

std::string CsvDeuxClasses() {
    std::string t;
    for (int i = 0; i < 40; ++i) {
        const int c = i % 2;
        t += std::to_string(c) + "," + std::to_string(c * 2 + (i % 7) * 0.1) + "," + std::to_string(c * 2 + (i % 5) * 0.1) + "\n";
    }
    return t;
}

} // namespace

TEST(creation_et_reouverture) {
    const fs::path dossier = verif::DossierTemporaire("projet_creation") / "mon projet";
    auto p = Projet::Creer(dossier, "Mon projet");
    VERIFIER(p.Ok());
    if (!p.Ok()) return;
    VERIFIER(fs::exists(dossier / "projet.json"));
    VERIFIER(fs::is_directory(dossier / "requetes"));
    VERIFIER(fs::is_directory(dossier / "taches"));
    VERIFIER_EGAL(p.Valeur().Nom(), std::string("Mon projet"));
    // un deuxieme Creer sur le meme dossier est refuse
    VERIFIER(!Projet::Creer(dossier, "autre").Ok());

    SourceProjet s;
    s.m_chemin = "D:/donnees/x.csv";
    s.m_contrat.m_separateur = ';';
    s.m_contrat.m_enTete = decid::ModeEnTete::Oui;
    s.m_contrat.m_colonneCible = 2;
    s.m_hashCanonique = "abc";
    s.m_lignes = 10;
    s.m_attributs = 3;
    s.m_classes = 2;
    VERIFIER(p.Valeur().AjouterSource(s).Ok());

    auto q = Projet::Ouvrir(dossier);
    VERIFIER(q.Ok());
    if (!q.Ok()) return;
    VERIFIER_EGAL(q.Valeur().Nom(), std::string("Mon projet"));
    VERIFIER_EGAL(q.Valeur().Sources().size(), (size_t)1);
    if (q.Valeur().Sources().empty()) return;
    const SourceProjet& r = q.Valeur().Sources().front();
    VERIFIER_EGAL(r.m_chemin, std::string("D:/donnees/x.csv"));
    VERIFIER_EGAL(r.m_contrat.m_separateur, ';');
    VERIFIER(r.m_contrat.m_enTete == decid::ModeEnTete::Oui);
    VERIFIER_EGAL(r.m_contrat.m_colonneCible, 2);
    VERIFIER_EGAL(r.m_lignes, 10);
    // un dossier sans projet.json ne s'ouvre pas
    VERIFIER(!Projet::Ouvrir(verif::DossierTemporaire("projet_vide")).Ok());
}

TEST(identifiants_de_tache_uniques_et_bien_formes) {
    auto p = Projet::Creer(verif::DossierTemporaire("projet_ids"), "ids");
    VERIFIER(p.Ok());
    if (!p.Ok()) return;
    std::set<std::string> vus;
    for (int i = 0; i < 3000; ++i) {
        const std::string id = p.Valeur().NouvelIdTache("experience", "svm");
        VERIFIER(vus.insert(id).second);
        if (i == 0) {
            // <operation>-<famille>-<horodatage compact>-<4 hex>
            VERIFIER(id.rfind("experience-svm-", 0) == 0);
            VERIFIER_EGAL(id.size(), std::string("experience-svm-20261015T210405-abcd").size());
            VERIFIER_EGAL(id[id.size() - 5], '-');
        }
    }
    VERIFIER(p.Valeur().NouvelIdTache("validation", "").find("validation-sans-") == 0);
}

TEST(ecriture_de_requete) {
    auto p = Projet::Creer(verif::DossierTemporaire("projet_requete"), "req");
    VERIFIER(p.Ok());
    if (!p.Ok()) return;
    const std::string id = p.Valeur().NouvelIdTache("experience", "kppv");
    decid::ValeurJson r = decid::ValeurJson::Objet();
    r["algorithme"] = decid::ValeurJson::Objet();
    r["algorithme"]["famille"] = "kppv";
    auto chemin = p.Valeur().EcrireRequete(id, r);
    VERIFIER(chemin.Ok());
    if (!chemin.Ok()) return;
    VERIFIER(fs::exists(chemin.Valeur()));
    VERIFIER_EGAL(chemin.Valeur().filename().string(), id + ".json");
    auto texte = decid::LireFichierTexte(chemin.Valeur());
    VERIFIER(texte.Ok());
    if (!texte.Ok()) return;
    auto json = decid::LireJson(texte.Valeur());
    VERIFIER(json.Ok());
    if (!json.Ok()) return;
    VERIFIER_EGAL(json.Valeur().Membre("request_id").Texte(), id);
    VERIFIER_EGAL(json.Valeur().Membre("protocol_version").Entier(), 1LL);
    VERIFIER_EGAL(json.Valeur().Membre("algorithme").Membre("famille").Texte(), std::string("kppv"));
    // une requete figee n'est jamais ecrasee
    VERIFIER(!p.Valeur().EcrireRequete(id, r).Ok());
    // identifiant avec separateur refuse
    VERIFIER(!p.Valeur().EcrireRequete("../hors", r).Ok());
}

TEST(liste_des_taches_avec_etat_et_operation) {
    auto p = Projet::Creer(verif::DossierTemporaire("projet_taches"), "taches");
    VERIFIER(p.Ok());
    if (!p.Ok()) return;
    const fs::path t1 = p.Valeur().DossierTache("experience-svm-a");
    const fs::path t2 = p.Valeur().DossierTache("image-sans-b");
    const fs::path t3 = p.Valeur().DossierTache("experience-kppv-c");
    for (const auto& t : {t1, t2, t3}) fs::create_directories(t);
    Ecrire(t1 / "requete.json", "{\"algorithme\":{\"famille\":\"svm\"},\"request_id\":\"experience-svm-a\"}");
    Ecrire(t1 / "etat.json", "{\"etat\":\"succeeded\",\"schema\":\"decid.task-state/1\"}");
    Ecrire(t2 / "requete.json", "{\"operation\":\"image\",\"request_id\":\"image-sans-b\"}");
    Ecrire(t2 / "etat.json", "{\"etat\":\"failed\"}");
    Ecrire(t3 / "requete.json", "{\"algorithme\":{\"famille\":\"kppv\"}}");   // pas d'etat : en cours
    const auto liste = p.Valeur().ListerTaches();
    VERIFIER_EGAL(liste.size(), (size_t)3);
    for (const auto& t : liste) {
        if (t.m_id == "experience-svm-a") {
            VERIFIER_EGAL(t.m_operation, std::string("experience"));
            VERIFIER_EGAL(t.m_famille, std::string("svm"));
            VERIFIER_EGAL(t.m_etat, std::string("succeeded"));
        } else if (t.m_id == "image-sans-b") {
            VERIFIER_EGAL(t.m_operation, std::string("image"));
            VERIFIER_EGAL(t.m_etat, std::string("failed"));
        } else {
            VERIFIER_EGAL(t.m_id, std::string("experience-kppv-c"));
            VERIFIER_EGAL(t.m_etat, std::string("running"));
        }
    }
}

TEST(contexte_jeu_partage_notifications) {
    const fs::path racine = verif::DossierTemporaire("projet_contexte");
    const fs::path csv = racine / "donnees.csv";
    Ecrire(csv, CsvDeuxClasses());
    auto p = Projet::Creer(racine / "p", "ctx");
    VERIFIER(p.Ok());
    if (!p.Ok()) return;

    Contexte ctx;
    std::vector<Evenement> recus;
    ctx.Abonner([&](Evenement e) { recus.push_back(e); });
    VERIFIER(ctx.OuvrirProjet(std::make_unique<Projet>(std::move(p.Valeur()))).Ok());
    VERIFIER(ctx.ProjetOuvert());
    VERIFIER(!ctx.Jeu());

    SourceProjet s;
    s.m_chemin = decid::CheminUtf8(csv);
    VERIFIER(ctx.ActiverSource(s).Ok());
    VERIFIER(ctx.Jeu() != nullptr);
    if (!ctx.Jeu()) return;
    VERIFIER_EGAL(ctx.Jeu()->m_nbLignes, 40);
    VERIFIER_EGAL(ctx.HashCourt().size(), (size_t)12);
    VERIFIER_EGAL(ctx.Source().m_lignes, 40);

    ChoixPartage choix;
    choix.m_methode = decid::MethodePartage::AleatoireStratifie;
    choix.m_graine = 7;
    VERIFIER(ctx.DefinirPartage(choix).Ok());
    VERIFIER(ctx.GetPartage() != nullptr);
    if (!ctx.GetPartage()) return;
    VERIFIER_EGAL(ctx.GetPartage()->m_apprentissage.size() + ctx.GetPartage()->m_test.size(), (size_t)40);

    // une source illisible est refusee et le jeu actif est conserve
    SourceProjet mauvaise;
    mauvaise.m_chemin = decid::CheminUtf8(racine / "absent.csv");
    VERIFIER(!ctx.ActiverSource(mauvaise).Ok());
    VERIFIER(ctx.Jeu() != nullptr);
    VERIFIER(ctx.GetPartage() != nullptr);

    bool jeu = false, partage = false;
    for (Evenement e : recus) {
        jeu = jeu || e == Evenement::Jeu;
        partage = partage || e == Evenement::Partage;
    }
    VERIFIER(jeu && partage);

    // reouverture : la source active est relue
    Contexte ctx2;
    auto q = Projet::Ouvrir(racine / "p");
    VERIFIER(q.Ok());
    if (!q.Ok()) return;
    VERIFIER(ctx2.OuvrirProjet(std::make_unique<Projet>(std::move(q.Valeur()))).Ok());
    VERIFIER(ctx2.Jeu() != nullptr);
    VERIFIER_EGAL(ctx2.HashCourt(), ctx.HashCourt());
}

int main(int argc, char** argv) { return LancerTests(argc, argv); }
