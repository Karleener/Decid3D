// Operation "decider" (voir Decision.h) : modele et normalisation relus depuis l'experience publiee, source lue
// avec son propre contrat CSV, dimensions controlees avant toute prediction.
#include "Decision.h"

#include <algorithm>
#include "Experience.h"
#include "Fichiers.h"
#include "Metriques.h"
#include "Normalisation.h"
#include "Operations.h"

namespace decid {

namespace {

Erreur Proto(const std::string& message) { return FaireErreur(CodeErreur::Protocole, "requete", message); }

Erreur Incompatible(const std::string& message) {
    return FaireErreur(CodeErreur::ModeleIncompatible, "decision", message);
}

Statut Annule(const std::atomic<bool>& annulation) {
    if (annulation.load())
        return FaireErreur(CodeErreur::Annulation, "annulation", "calcul annule a la demande, rien n'est publie");
    return Succes();
}

fs::path Resoudre(const std::string& texte, const fs::path& dossierRequete) {
    fs::path p = CheminDepuisUtf8(texte);
    return p.is_relative() ? dossierRequete / p : p;
}

struct SourceDecision {
    fs::path m_chemin;
    ContratCsv m_contrat;
};

Resultat<SourceDecision> LireSource(const ValeurJson& requete, const fs::path& dossierRequete) {
    const ValeurJson& s = requete.Membre("source");
    if (s.Membre("type").Texte() != "csv") return Proto("decider : source de type csv attendue");
    if (s.Membre("chemin").Texte().empty()) return Proto("chemin de source manquant");
    SourceDecision d;
    d.m_chemin = Resoudre(s.Membre("chemin").Texte(), dossierRequete);
    const std::string sep = s.Contient("separateur") ? s.Membre("separateur").Texte() : ",";
    if (sep.size() != 1) return Proto("separateur CSV : un seul caractere attendu");
    d.m_contrat.m_separateur = sep[0];
    const std::string enTete = s.Membre("en_tete").Texte();
    if (enTete == "oui") d.m_contrat.m_enTete = ModeEnTete::Oui;
    else if (enTete == "auto") d.m_contrat.m_enTete = ModeEnTete::Auto;
    else if (!enTete.empty() && enTete != "non") return Proto("en_tete : oui, non ou auto");
    const long long cible = s.Membre("colonne_cible").Entier();
    if (cible < -1) return Proto("colonne_cible : indice >= 0, ou -1 pour un fichier sans classes");
    d.m_contrat.m_sansCible = (cible == -1);
    d.m_contrat.m_colonneCible = (int)cible;
    return d;
}

bool EnTeteLu(const JeuDeDonnees& jeu) { return jeu.m_contrat.find("en_tete=oui") != std::string::npos; }

std::string Liste(const std::vector<std::string>& noms) {
    std::string t;
    for (const auto& n : noms) t += (t.empty() ? "" : ", ") + n;
    return t;
}

// Meme controle des dimensions que les operations sur experience publiee, avec un message explicite.
Statut ControlerAttributs(const JeuDeDonnees& modele, const JeuDeDonnees& source) {
    if (source.m_nbAttributs != modele.m_nbAttributs)
        return Incompatible("la source a " + std::to_string(source.m_nbAttributs) + " attribut(s), le modele en attend " +
                            std::to_string(modele.m_nbAttributs) + " (" + Liste(modele.m_nomsAttributs) + ")");
    if (EnTeteLu(source) && source.m_nomsAttributs != modele.m_nomsAttributs)
        return Incompatible("attributs differents de ceux du modele : source (" + Liste(source.m_nomsAttributs) +
                            "), modele (" + Liste(modele.m_nomsAttributs) + ")");
    return Succes();
}

// Verites de la source exprimees en classes du modele (par nom) ; inconnues recoit les noms absents du modele.
std::vector<int> VeritesModele(const JeuDeDonnees& source, const std::vector<std::string>& classesModele,
                               std::vector<std::string>& inconnues) {
    std::vector<int> correspondance;
    for (const auto& nom : source.m_nomsClasses) {
        auto it = std::find(classesModele.begin(), classesModele.end(), nom);
        correspondance.push_back(it == classesModele.end() ? -1 : (int)(it - classesModele.begin()));
        if (it == classesModele.end()) inconnues.push_back(nom);
    }
    std::vector<int> verites;
    for (int l : source.m_labels) verites.push_back(correspondance[(size_t)l]);
    return verites;
}

std::string IdLigne(const JeuDeDonnees& jeu, int i) {
    const auto& o = jeu.m_origines[(size_t)i];
    return std::to_string(o.m_source) + ":" + std::to_string(o.m_ligne);
}

std::string CsvPredictions(const JeuDeDonnees& source, bool avecClasses, const std::vector<int>& predictions,
                           const std::vector<std::string>& classesModele) {
    std::string csv = "ligne;origine;verite;prediction\n";
    for (int i = 0; i < source.m_nbLignes; ++i) {
        const std::string verite = avecClasses ? source.m_nomsClasses[(size_t)source.m_labels[(size_t)i]] : "";
        csv += std::to_string(i) + ";" + IdLigne(source, i) + ";" + verite + ";" +
               classesModele[(size_t)predictions[(size_t)i]] + "\n";
    }
    return csv;
}

ValeurJson ComptesPredits(const std::vector<int>& predictions, const std::vector<std::string>& classesModele) {
    std::vector<long long> n(classesModele.size(), 0);
    for (int p : predictions) ++n[(size_t)p];
    ValeurJson t = ValeurJson::Tableau();
    for (size_t c = 0; c < n.size(); ++c) {
        ValeurJson o = ValeurJson::Objet();
        o["classe"] = classesModele[c];
        o["lignes"] = n[c];
        t.Ajouter(o);
    }
    return t;
}

struct Evaluation {
    ValeurJson m_json = ValeurJson::Objet();
    std::string m_comptesCsv;               // vide si non evalue
};

Resultat<Evaluation> Evaluer(const JeuDeDonnees& source, bool avecClasses, const std::vector<int>& predictions,
                             const std::vector<std::string>& classesModele, const TempsDecision& temps) {
    Evaluation e;
    e.m_json["schema"] = "decid.evaluation/1";
    e.m_json["population"] = "decision";
    e.m_json["temps_decision"] = temps.VersJson();
    if (!avecClasses) {
        e.m_json["statut"] = "sans_classes";
        e.m_json["raison"] = "la source n'a pas de colonne de classes : predictions seules";
        return e;
    }
    std::vector<std::string> inconnues;
    std::vector<int> verites = VeritesModele(source, classesModele, inconnues);
    if (!inconnues.empty()) {
        e.m_json["statut"] = "non_evaluable";
        e.m_json["raison"] = "classe(s) de la source absente(s) du modele : " + Liste(inconnues);
        return e;
    }
    auto confusion = CalculerConfusion(verites, predictions, (int)classesModele.size());
    if (!confusion.Ok()) return confusion.Err();
    RapportMetriques rapport = CalculerMetriques(confusion.Valeur(), classesModele, "decision");
    e.m_json["statut"] = "evalue";
    e.m_json["rapport"] = rapport.VersJson();
    e.m_comptesCsv = rapport.VersCsvComptes();
    return e;
}

std::vector<int> ToutesLesLignes(int n) {
    std::vector<int> lignes((size_t)n);
    for (int i = 0; i < n; ++i) lignes[(size_t)i] = i;
    return lignes;
}

ValeurJson DescriptionDecision(const ValeurJson& requete, const ExperiencePubliee& e, const JeuDeDonnees& source,
                               bool avecClasses, long horsPlage, const std::vector<int>& predictions) {
    ValeurJson j = ValeurJson::Objet();
    j["schema"] = "decid.decision/1";
    j["request_id"] = requete.Membre("request_id").Texte();
    j["requete"] = requete;
    j["famille"] = NomFamille(e.m_modele.m_famille);
    j["hash_modele"] = e.m_hashModele;
    j["hash_normalisation"] = e.m_normalisation.Hash();
    j["hash_jeu_modele"] = e.m_jeu.HashCanonique();
    j["hash_source"] = source.HashCanonique();
    j["contrat_source"] = source.m_contrat;
    j["lignes"] = source.m_nbLignes;
    ValeurJson attributs = ValeurJson::Tableau(), classes = ValeurJson::Tableau();
    for (const auto& a : e.m_jeu.m_nomsAttributs) attributs.Ajouter(a);
    for (const auto& c : e.m_jeu.m_nomsClasses) classes.Ajouter(c);
    j["attributs"] = attributs;
    j["classes_modele"] = classes;
    j["avec_classes"] = avecClasses;
    j["valeurs_hors_plage_normalisation"] = (long long)horsPlage;
    j["comptes_predits"] = ComptesPredits(predictions, e.m_jeu.m_nomsClasses);
    return j;
}

Statut Ecrire(const fs::path& staging, const char* nom, const std::string& texte) {
    return EcrireFichierAtomique(staging / nom, texte, false);
}

} // namespace

Statut ValiderDecision(const ValeurJson& requete, const fs::path& dossierRequete) {
    auto source = LireSource(requete, dossierRequete);
    if (!source.Ok()) return source.Err();
    return Succes();
}

Statut ExecuterDecision(const ValeurJson& requete, const fs::path& dossierRequete, const fs::path& staging,
                        const std::atomic<bool>& annulation) {
    const std::string texteExperience = requete.Membre("experience").Texte();
    if (texteExperience.empty()) return Proto("champ \"experience\" (tache publiee) manquant");
    auto source = LireSource(requete, dossierRequete);
    if (!source.Ok()) return source.Err();
    auto e = ChargerExperiencePubliee(Resoudre(texteExperience, dossierRequete));
    if (!e.Ok()) return e.Err();
    auto jeu = LireCsv(source.Valeur().m_chemin, source.Valeur().m_contrat);
    if (!jeu.Ok()) return jeu.Err();
    const JeuDeDonnees& src = jeu.Valeur();
    Statut st = ControlerAttributs(e.Valeur().m_jeu, src);
    if (!st.Ok()) return st;
    if (!(st = Annule(annulation)).Ok()) return st;

    long horsPlage = 0;
    auto x = AppliquerNormalisation(e.Valeur().m_normalisation, src, ToutesLesLignes(src.m_nbLignes), &horsPlage);
    if (!x.Ok()) return x.Err();
    TempsDecision temps;
    temps.m_lot = "des lignes du fichier decide";
    auto predictions = PredireChronometre(e.Valeur().m_modele, x.Valeur(), temps);
    if (!predictions.Ok()) return predictions.Err();
    if (!(st = Annule(annulation)).Ok()) return st;

    const bool avecClasses = !source.Valeur().m_contrat.m_sansCible;
    const auto& classesModele = e.Valeur().m_jeu.m_nomsClasses;
    auto evaluation = Evaluer(src, avecClasses, predictions.Valeur(), classesModele, temps);
    if (!evaluation.Ok()) return evaluation.Err();
    if (!(st = Ecrire(staging, "predictions.csv", CsvPredictions(src, avecClasses, predictions.Valeur(), classesModele))).Ok())
        return st;
    if (!evaluation.Valeur().m_comptesCsv.empty() &&
        !(st = Ecrire(staging, "comptes.csv", evaluation.Valeur().m_comptesCsv)).Ok())
        return st;
    if (!(st = Ecrire(staging, "evaluation.json", evaluation.Valeur().m_json.Ecrire())).Ok()) return st;
    ValeurJson description = DescriptionDecision(requete, e.Valeur(), src, avecClasses, horsPlage, predictions.Valeur());
    return Ecrire(staging, "decision.json", description.Ecrire());
}

} // namespace decid
