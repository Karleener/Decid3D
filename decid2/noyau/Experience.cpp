// Tranche L1 complete : import -> partage -> normalisation -> apprentissage -> sauvegarde ->
// rechargement verifie -> evaluation -> inspection. Ecrit uniquement dans le dossier de staging.
#include "Experience.h"

#include <chrono>

#include <opencv2/core/utility.hpp>
#include "Fichiers.h"
#include "Metriques.h"
#include "Normalisation.h"
#include "Sha256.h"

namespace decid {

namespace {

const char* VERSION_DECID = "decid2 2.0.0";
const int TAILLE_CONTROLE = 50;

fs::path Resoudre(const std::string& texte, const fs::path& dossierRequete) {
    fs::path p = CheminDepuisUtf8(texte);
    return p.is_relative() ? dossierRequete / p : p;
}

ModeEnTete ModeDepuisTexte(const std::string& t, bool& ok) {
    ok = true;
    if (t == "non" || t.empty()) return ModeEnTete::Non;
    if (t == "oui") return ModeEnTete::Oui;
    if (t == "auto") return ModeEnTete::Auto;
    ok = false;
    return ModeEnTete::Non;
}

Statut Annule(const std::atomic<bool>& annulation) {
    if (annulation.load())
        return FaireErreur(CodeErreur::Annulation, "annulation", "calcul annule a la demande, rien n'est publie");
    return Succes();
}

std::string IdLigne(const JeuDeDonnees& jeu, int i) {
    const auto& o = jeu.m_origines[(size_t)i];
    return std::to_string(o.m_source) + ":" + std::to_string(o.m_ligne);
}

} // namespace

ValeurJson RequeteExperience::VersJson() const {
    ValeurJson j = ValeurJson::Objet();
    j["protocol_version"] = m_versionProtocole;
    j["request_id"] = m_idRequete;
    ValeurJson s = ValeurJson::Objet();
    s["type"] = m_typeSource;
    if (m_typeSource == "roles") {
        ValeurJson liste = ValeurJson::Tableau();
        for (auto& f : m_fichiersRoles) {
            ValeurJson e = ValeurJson::Objet();
            e["chemin"] = CheminUtf8(f.m_chemin);
            e["classe"] = f.m_classe;
            e["role"] = NomRole(f.m_role);
            liste.Ajouter(e);
        }
        s["fichiers"] = liste;
    } else {
        s["chemin"] = CheminUtf8(m_source);
    }
    if (m_typeSource == "csv") {
        s["separateur"] = std::string(1, m_contratCsv.m_separateur);
        s["en_tete"] = m_contratCsv.m_enTete == ModeEnTete::Oui ? "oui" : m_contratCsv.m_enTete == ModeEnTete::Auto ? "auto" : "non";
        s["colonne_cible"] = m_contratCsv.m_colonneCible;
    }
    j["source"] = s;
    ValeurJson p = ValeurJson::Objet();
    p["methode"] = NomMethode(m_methode);
    p["ratio"] = m_ratio;
    p["graine"] = (unsigned long long)m_graine;
    p["inverse"] = m_inverse;
    j["partage"] = p;
    ValeurJson n = ValeurJson::Objet();
    n["methode"] = m_normalisation;
    n["bas"] = m_bas;
    n["haut"] = m_haut;
    if (m_normalisation == "minmax_lim") n["fichier"] = CheminUtf8(m_fichierLim);
    j["normalisation"] = n;
    j["algorithme"] = m_algorithme.VersJson();
    j["threads"] = m_nbThreads;
    return j;
}

Resultat<RequeteExperience> RequeteDepuisJson(const ValeurJson& j, const fs::path& dossierRequete) {
    auto erreur = [](const std::string& m) { return FaireErreur(CodeErreur::Protocole, "requete", m); };
    RequeteExperience r;
    r.m_versionProtocole = (int)j.Membre("protocol_version").Entier();
    if (r.m_versionProtocole != 1) return erreur("version de protocole non prise en charge");
    r.m_idRequete = j.Membre("request_id").Texte();
    if (r.m_idRequete.empty()) return erreur("request_id manquant");

    const ValeurJson& s = j.Membre("source");
    r.m_typeSource = s.Membre("type").Texte();
    if (r.m_typeSource == "csv" || r.m_typeSource == "cmc") {
        if (s.Membre("chemin").Texte().empty()) return erreur("chemin de source manquant");
        r.m_source = Resoudre(s.Membre("chemin").Texte(), dossierRequete);
    }
    if (r.m_typeSource == "csv") {
        std::string sep = s.Contient("separateur") ? s.Membre("separateur").Texte() : ",";
        if (sep.size() != 1) return erreur("separateur CSV : un seul caractere attendu");
        r.m_contratCsv.m_separateur = sep[0];
        bool ok;
        r.m_contratCsv.m_enTete = ModeDepuisTexte(s.Membre("en_tete").Texte(), ok);
        if (!ok) return erreur("en_tete : oui, non ou auto");
        r.m_contratCsv.m_colonneCible = (int)s.Membre("colonne_cible").Entier();
    } else if (r.m_typeSource == "cmc") {
        for (auto& rel : s.Membre("relocalisations").Elements())
            r.m_relocalisations.push_back({rel.Membre("ancien").Texte(), rel.Membre("nouveau").Texte()});
    } else if (r.m_typeSource == "roles") {
        for (auto& f : s.Membre("fichiers").Elements()) {
            FichierRole fr;
            fr.m_chemin = Resoudre(f.Membre("chemin").Texte(), dossierRequete);
            fr.m_classe = f.Membre("classe").Texte();
            std::string role = f.Membre("role").Texte();
            fr.m_role = role == "apprentissage" ? RoleFichier::Apprentissage : role == "test" ? RoleFichier::Test : RoleFichier::Aucun;
            if (fr.m_role == RoleFichier::Aucun || fr.m_classe.empty()) return erreur("fichier par role : classe et role requis");
            r.m_fichiersRoles.push_back(fr);
        }
    } else {
        return erreur("type de source inconnu (csv, cmc, roles)");
    }

    const ValeurJson& p = j.Membre("partage");
    if (r.m_typeSource == "roles") r.m_methode = MethodePartage::ParRoles;
    else if (!MethodeDepuisNom(p.Membre("methode").Texte(), r.m_methode)) return erreur("methode de partage inconnue");
    if (p.Contient("ratio")) r.m_ratio = p.Membre("ratio").Nombre();
    r.m_graine = (std::uint64_t)p.Membre("graine").Entier();
    r.m_inverse = p.Membre("inverse").Booleen();

    const ValeurJson& n = j.Membre("normalisation");
    r.m_normalisation = n.Contient("methode") ? n.Membre("methode").Texte() : "aucune";
    if (r.m_normalisation != "aucune" && r.m_normalisation != "minmax" && r.m_normalisation != "minmax_lim")
        return erreur("normalisation : aucune, minmax ou minmax_lim");
    if (n.Contient("bas")) r.m_bas = n.Membre("bas").Nombre();
    if (n.Contient("haut")) r.m_haut = n.Membre("haut").Nombre();
    if (!(r.m_haut > r.m_bas)) return erreur("normalisation : haut doit etre > bas");
    if (r.m_normalisation == "minmax_lim") {
        if (n.Membre("fichier").Texte().empty()) return erreur("normalisation minmax_lim : champ \"fichier\" (.lim) manquant");
        r.m_fichierLim = Resoudre(n.Membre("fichier").Texte(), dossierRequete);
    }

    auto algorithme = ParametresDepuisJson(j.Membre("algorithme"));
    if (!algorithme.Ok()) return algorithme.Err();
    r.m_algorithme = algorithme.Valeur();
    if (j.Contient("threads")) r.m_nbThreads = (int)j.Membre("threads").Entier();
    if (r.m_nbThreads < 1 || r.m_nbThreads > 64) return erreur("threads : entre 1 et 64");
    return r;
}

ValeurJson TempsDecision::VersJson() const {
    ValeurJson j = ValeurJson::Objet();
    j["par_echantillon_us"] = m_parEchantillonUs;
    j["echantillons"] = m_echantillons;
    j["repetitions"] = m_repetitions;
    j["mesure"] = "horloge monotone (steady_clock) autour de la seule prediction " + m_lot + ", sans entrees-sorties ; "
                  "lot repete jusqu'a 1 ms cumulee au moins, temps total divise par (echantillons x repetitions)";
    return j;
}

Resultat<std::vector<int>> PredireChronometre(const Modele& modele, const cv::Mat& x, TempsDecision& temps) {
    using Horloge = std::chrono::steady_clock;
    const auto debut = Horloge::now();
    auto predictions = Predire(modele, x);
    if (!predictions.Ok()) return predictions;
    int repetitions = 1;
    auto ecoule = Horloge::now() - debut;
    while (ecoule < std::chrono::milliseconds(1) && repetitions < 100000) {
        auto encore = Predire(modele, x);
        if (!encore.Ok()) return encore;
        ++repetitions;
        ecoule = Horloge::now() - debut;
    }
    const double totalUs = std::chrono::duration<double, std::micro>(ecoule).count();
    temps.m_echantillons = x.rows;
    temps.m_repetitions = repetitions;
    temps.m_parEchantillonUs = x.rows > 0 ? totalUs / ((double)x.rows * repetitions) : 0.0;
    return predictions;
}

Statut ExecuterExperience(const RequeteExperience& req, const fs::path& staging,
                          const std::atomic<bool>& annulation, const RapporteurEvenement& rapporter) {
    const long NB_PHASES = 8;
    std::error_code ec;
    fs::create_directories(staging, ec);
    if (ec) return FaireErreur(CodeErreur::EcritureRefusee, "staging", "dossier de staging non creable", CheminUtf8(staging));
    auto ecrire = [&](const char* nom, const std::string& texte) { return EcrireFichierAtomique(staging / nom, texte, false); };

    // 1. import (lecture seule des sources)
    rapporter("import", 1, NB_PHASES, "");
    Resultat<JeuDeDonnees> lu = req.m_typeSource == "csv" ? LireCsv(req.m_source, req.m_contratCsv)
                              : req.m_typeSource == "cmc" ? LireCmc(req.m_source, req.m_relocalisations)
                              : LireFichiersParRole(req.m_fichiersRoles);
    if (!lu.Ok()) return lu.Err();
    const JeuDeDonnees& jeu = lu.Valeur();
    auto st = SauverJeu(jeu, staging / "jeu");
    if (!st.Ok()) return st;
    if (!(st = Annule(annulation)).Ok()) return st;

    // 2. partage par indices
    rapporter("partage", 2, NB_PHASES, "");
    auto partage = CreerPartage(jeu, req.m_methode, req.m_ratio, req.m_graine, req.m_inverse);
    if (!partage.Ok()) return partage.Err();
    const Partage& pa = partage.Valeur();
    if (!(st = ecrire("partage.json", pa.VersJson().Ecrire())).Ok()) return st;
    if (!(st = Annule(annulation)).Ok()) return st;

    // 3. normalisation ajustee sur l'apprentissage seul
    rapporter("normalisation", 3, NB_PHASES, req.m_normalisation);
    Resultat<Normalisation> norm = NormalisationDemandee(req.m_normalisation, req.m_fichierLim, req.m_bas, req.m_haut,
                                                         jeu, pa.m_apprentissage, pa.Hash());
    if (!norm.Ok()) return norm.Err();
    if (!(st = ecrire("normalisation.json", norm.Valeur().VersJson().Ecrire())).Ok()) return st;
    auto xApp = AppliquerNormalisation(norm.Valeur(), jeu, pa.m_apprentissage);
    if (!xApp.Ok()) return xApp.Err();
    std::vector<int> yApp;
    for (int i : pa.m_apprentissage) yApp.push_back(jeu.m_labels[(size_t)i]);
    if (!(st = Annule(annulation)).Ok()) return st;

    // 4. apprentissage (RNG et threads fixes dans ce thread, celui qui calcule)
    rapporter("apprentissage", 4, NB_PHASES, NomFamille(req.m_algorithme.m_famille));
    cv::setNumThreads(req.m_nbThreads);
    cv::theRNG() = cv::RNG(req.m_graine);
    Resultat<Modele> modele = Entrainer(xApp.Valeur(), yApp, jeu.NbClasses(), req.m_algorithme, req.m_graine);
    if (!modele.Ok()) return modele.Err();
    if (!(st = Annule(annulation)).Ok()) return st;

    // 5. sauvegarde + manifeste du modele
    rapporter("sauvegarde", 5, NB_PHASES, "");
    if (!(st = SauverModele(modele.Valeur(), staging / "modele.yml")).Ok()) return st;
    ValeurJson manifeste = ValeurJson::Objet();
    manifeste["schema"] = "decid.model-manifest/1";
    manifeste["decid_version"] = VERSION_DECID;
    manifeste["opencv_version"] = CV_VERSION;
    manifeste["famille"] = NomFamille(req.m_algorithme.m_famille);
    manifeste["fichier_modele"] = "modele.yml";
    manifeste["nb_attributs"] = jeu.m_nbAttributs;
    ValeurJson attributs = ValeurJson::Tableau(), classes = ValeurJson::Tableau();
    for (auto& a : jeu.m_nomsAttributs) attributs.Ajouter(a);
    for (auto& c : jeu.m_nomsClasses) classes.Ajouter(c);
    manifeste["attributs"] = attributs;
    manifeste["classes"] = classes;
    manifeste["hash_jeu"] = jeu.HashCanonique();
    manifeste["hash_partage"] = pa.Hash();
    manifeste["hash_normalisation"] = norm.Valeur().Hash();
    manifeste["parametres_demandes"] = modele.Valeur().m_parametresDemandes;
    manifeste["parametres_effectifs"] = modele.Valeur().m_parametresEffectifs;
    manifeste["algorithme"] = req.m_algorithme.VersJson();
    if (req.m_algorithme.m_famille == Famille::Em) {   // cluster -> classe (-1 = non associe), appris sur l apprentissage
        ValeurJson correspondance = ValeurJson::Tableau();
        for (int c : modele.Valeur().m_correspondanceClusters) correspondance.Ajouter(c);
        manifeste["correspondance_clusters"] = correspondance;
    }
    manifeste["graine"] = (unsigned long long)req.m_graine;
    manifeste["threads"] = req.m_nbThreads;
    manifeste["provenance"] = "complete";
    manifeste["profil"] = "corrected_v1";
    if (!(st = ecrire("modele.json", manifeste.Ecrire())).Ok()) return st;

    // 6. rechargement et controle : predictions identiques sur un echantillon d'apprentissage
    rapporter("rechargement", 6, NB_PHASES, "");
    auto recharge = ChargerModele(req.m_algorithme.m_famille, staging / "modele.yml", jeu.NbClasses());
    if (!recharge.Ok()) return recharge.Err();
    int nbControle = std::min<int>(TAILLE_CONTROLE, xApp.Valeur().rows);
    cv::Mat xControle = xApp.Valeur().rowRange(0, nbControle);
    auto avant = Predire(modele.Valeur(), xControle);
    auto apres = Predire(recharge.Valeur(), xControle);
    if (!avant.Ok()) return avant.Err();
    if (!apres.Ok()) return apres.Err();
    if (avant.Valeur() != apres.Valeur())
        return FaireErreur(CodeErreur::ModeleIncompatible, "rechargement", "predictions differentes apres rechargement");
    ValeurJson controle = ValeurJson::Objet();
    ValeurJson lignes = ValeurJson::Tableau(), predites = ValeurJson::Tableau();
    for (int i = 0; i < nbControle; ++i) { lignes.Ajouter(pa.m_apprentissage[(size_t)i]); predites.Ajouter(avant.Valeur()[(size_t)i]); }
    controle["lignes"] = lignes;
    controle["predictions"] = predites;
    if (!(st = ecrire("controle.json", controle.Ecrire())).Ok()) return st;
    if (!(st = Annule(annulation)).Ok()) return st;

    // 7. evaluation sur le test (interdite si test vide)
    rapporter("evaluation", 7, NB_PHASES, "");
    ValeurJson evaluation = ValeurJson::Objet();
    evaluation["schema"] = "decid.evaluation/1";
    evaluation["population"] = "test";
    if (pa.m_test.empty()) {
        evaluation["statut"] = "non_evaluable";
        evaluation["raison"] = "test vide : evaluation impossible, apprentissage seul autorise";
    } else {
        long horsPlage = 0;
        auto xTest = AppliquerNormalisation(norm.Valeur(), jeu, pa.m_test, &horsPlage);
        if (!xTest.Ok()) return xTest.Err();
        TempsDecision temps;
        auto predictions = PredireChronometre(modele.Valeur(), xTest.Valeur(), temps);
        if (!predictions.Ok()) return predictions.Err();
        std::vector<int> verites;
        for (int i : pa.m_test) verites.push_back(jeu.m_labels[(size_t)i]);
        auto confusion = CalculerConfusion(verites, predictions.Valeur(), jeu.NbClasses());
        if (!confusion.Ok()) return confusion.Err();
        RapportMetriques rapport = CalculerMetriques(confusion.Valeur(), jeu.m_nomsClasses, "test");
        evaluation["statut"] = "evalue";
        evaluation["valeurs_hors_plage_normalisation"] = horsPlage;
        evaluation["rapport"] = rapport.VersJson();
        evaluation["temps_decision"] = temps.VersJson();
        if (!(st = ecrire("comptes.csv", rapport.VersCsvComptes())).Ok()) return st;
        std::string csv = "ligne;origine;verite;prediction\n";
        for (size_t k = 0; k < pa.m_test.size(); ++k) {
            int i = pa.m_test[k];
            csv += std::to_string(i) + ";" + IdLigne(jeu, i) + ";" + jeu.m_nomsClasses[(size_t)verites[k]] + ";" +
                   jeu.m_nomsClasses[(size_t)predictions.Valeur()[k]] + "\n";
        }
        if (!(st = ecrire("predictions.csv", csv)).Ok()) return st;
    }
    if (!(st = ecrire("evaluation.json", evaluation.Ecrire())).Ok()) return st;
    if (!(st = Annule(annulation)).Ok()) return st;

    // 8. inspection de l'etat interne, puis liste des artefacts (ecrite en dernier)
    rapporter("inspection", 8, NB_PHASES, "");
    if (!(st = ecrire("inspection.json", InspecterModele(modele.Valeur()).Ecrire(false))).Ok()) return st;
    if (!(st = ecrire("requete.json", req.VersJson().Ecrire())).Ok()) return st;
    ValeurJson artefacts = ValeurJson::Objet();
    artefacts["schema"] = "decid.artefacts/1";
    ValeurJson liste = ValeurJson::Tableau();
    for (auto& entree : fs::recursive_directory_iterator(staging)) {
        if (!entree.is_regular_file()) continue;
        std::uintmax_t taille = 0;
        std::string hash = Sha256Fichier(entree.path(), &taille);
        ValeurJson f = ValeurJson::Objet();
        f["chemin"] = CheminUtf8(fs::relative(entree.path(), staging));
        f["sha256"] = hash;
        f["octets"] = (unsigned long long)taille;
        liste.Ajouter(f);
    }
    artefacts["fichiers"] = liste;
    return ecrire("artefacts.json", artefacts.Ecrire());
}

Statut VerifierStaging(const fs::path& staging) {
    auto texte = LireFichierTexte(staging / "artefacts.json");
    if (!texte.Ok()) return texte.Err();
    auto liste = LireJson(texte.Valeur());
    if (!liste.Ok()) return liste.Err();
    for (auto& f : liste.Valeur().Membre("fichiers").Elements()) {
        fs::path p = staging / CheminDepuisUtf8(f.Membre("chemin").Texte());
        std::uintmax_t taille = 0;
        if (Sha256Fichier(p, &taille) != f.Membre("sha256").Texte() || (long long)taille != f.Membre("octets").Entier())
            return FaireErreur(CodeErreur::ModeleIncompatible, "verification", "artefact absent ou altere", CheminUtf8(p));
    }
    std::error_code ec;
    if (!fs::exists(staging / "modele.json", ec)) return Succes();   // operation sans modele (image, trace...)
    auto texteManifeste = LireFichierTexte(staging / "modele.json");
    if (!texteManifeste.Ok()) return texteManifeste.Err();
    auto manifeste = LireJson(texteManifeste.Valeur());
    if (!manifeste.Ok()) return manifeste.Err();
    Famille famille;
    if (!FamilleDepuisNom(manifeste.Valeur().Membre("famille").Texte(), famille))
        return FaireErreur(CodeErreur::ModeleIncompatible, "verification", "famille inconnue dans le manifeste");
    int nbClasses = (int)manifeste.Valeur().Membre("classes").Elements().size();
    auto modele = ChargerModele(famille, staging / "modele.yml", nbClasses);
    if (!modele.Ok()) return modele.Err();
    if (modele.Valeur().m_nbAttributs != (int)manifeste.Valeur().Membre("nb_attributs").Entier())
        return FaireErreur(CodeErreur::ModeleIncompatible, "verification", "nombre d'attributs du modele different du manifeste");
    // predictions de controle rejouees sur le modele relu
    auto jeu = ChargerJeu(staging / "jeu");
    if (!jeu.Ok()) return jeu.Err();
    auto texteNorm = LireFichierTexte(staging / "normalisation.json");
    if (!texteNorm.Ok()) return texteNorm.Err();
    auto jsonNorm = LireJson(texteNorm.Valeur());
    if (!jsonNorm.Ok()) return jsonNorm.Err();
    auto norm = NormalisationDepuisJson(jsonNorm.Valeur());
    if (!norm.Ok()) return norm.Err();
    auto texteControle = LireFichierTexte(staging / "controle.json");
    if (!texteControle.Ok()) return texteControle.Err();
    auto controle = LireJson(texteControle.Valeur());
    if (!controle.Ok()) return controle.Err();
    std::vector<int> lignes, attendues;
    for (auto& v : controle.Valeur().Membre("lignes").Elements()) lignes.push_back((int)v.Entier());
    for (auto& v : controle.Valeur().Membre("predictions").Elements()) attendues.push_back((int)v.Entier());
    for (int l : lignes)
        if (l < 0 || l >= jeu.Valeur().m_nbLignes)
            return FaireErreur(CodeErreur::ModeleIncompatible, "verification", "ligne de controle hors du jeu");
    auto x = AppliquerNormalisation(norm.Valeur(), jeu.Valeur(), lignes);
    if (!x.Ok()) return x.Err();
    auto obtenues = Predire(modele.Valeur(), x.Valeur());
    if (!obtenues.Ok()) return obtenues.Err();
    if (obtenues.Valeur() != attendues)
        return FaireErreur(CodeErreur::ModeleIncompatible, "verification", "predictions de controle differentes apres relecture");
    return Succes();
}

} // namespace decid
