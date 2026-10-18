// Confusion et metriques : une metrique indefinie vaut null avec une raison, jamais NaN.
#include "Metriques.h"

#include <charconv>

namespace decid {

namespace {

const char* const kVersionDefinition = "decid.metrics/v1";

Metrique Definie(double valeur, const std::string& population) {
    Metrique m;
    m.m_definie = true;
    m.m_valeur = valeur;
    m.m_population = population;
    return m;
}

Metrique Indefinie(const std::string& raison, const std::string& population) {
    Metrique m;
    m.m_definie = false;
    m.m_raison = raison;
    m.m_population = population;
    return m;
}

Metrique Quotient(double numerateur, double denominateur, const std::string& raison,
                  const std::string& population) {
    if (denominateur <= 0.0) return Indefinie(raison, population);
    return Definie(numerateur / denominateur, population);
}

std::string PopulationClasse(const std::string& population, int c) {
    return population + ", classe " + std::to_string(c) + " positive";
}

MetriquesClasse CalculerClasse(const Confusion& conf, int c, const std::string& population) {
    const int k = conf.m_nbClasses;
    MetriquesClasse m;
    m.m_tp = conf.Case(c, c);
    long long colonne = 0, ligne = 0;
    for (int i = 0; i < k; ++i) {
        colonne += conf.Case(i, c);
        ligne += conf.Case(c, i);
    }
    m.m_fp = colonne - m.m_tp;
    m.m_fn = ligne - m.m_tp;
    m.m_tn = conf.m_total - m.m_tp - m.m_fp - m.m_fn;
    m.m_support = ligne;
    const std::string pop = PopulationClasse(population, c);
    const std::string cl = std::to_string(c);
    m.m_precision = Quotient((double)m.m_tp, (double)(m.m_tp + m.m_fp),
                             "aucune ligne predite dans la classe " + cl, pop);
    m.m_rappel = Quotient((double)m.m_tp, (double)(m.m_tp + m.m_fn),
                          "aucune ligne reelle dans la classe " + cl, pop);
    m.m_specificite = Quotient((double)m.m_tn, (double)(m.m_tn + m.m_fp),
                               "aucune ligne negative pour la classe " + cl, pop);
    m.m_fpr = Quotient((double)m.m_fp, (double)(m.m_fp + m.m_tn),
                       "aucune ligne negative pour la classe " + cl, pop);
    m.m_fnr = Quotient((double)m.m_fn, (double)(m.m_fn + m.m_tp),
                       "aucune ligne reelle dans la classe " + cl, pop);
    m.m_f1 = Quotient(2.0 * (double)m.m_tp, (double)(2 * m.m_tp + m.m_fp + m.m_fn),
                      "classe " + cl + " absente du test et jamais predite", pop);
    return m;
}

std::string ListeClasses(const std::vector<int>& classes) {
    std::string s;
    for (size_t i = 0; i < classes.size(); ++i) s += (i ? ", " : "") + std::to_string(classes[i]);
    return s;
}

using Selecteur = const Metrique& (*)(const MetriquesClasse&);

// Moyenne macro stricte sur toutes les classes declarees
Metrique MoyenneMacro(const std::vector<MetriquesClasse>& classes, Selecteur choisir, const std::string& nom,
                      const std::string& population) {
    std::vector<int> indefinies;
    double somme = 0.0;
    for (size_t c = 0; c < classes.size(); ++c) {
        const Metrique& m = choisir(classes[c]);
        if (m.m_definie) somme += m.m_valeur; else indefinies.push_back((int)c);
    }
    const std::string pop = population + ", macro sur toutes les classes";
    if (classes.empty()) return Indefinie("aucune classe declaree", pop);
    if (!indefinies.empty())
        return Indefinie(nom + " indefinie pour les classes " + ListeClasses(indefinies), pop);
    return Definie(somme / (double)classes.size(), pop);
}

// Moyenne ponderee par le support sur les classes de support > 0
Metrique MoyennePonderee(const std::vector<MetriquesClasse>& classes, Selecteur choisir, const std::string& nom,
                         const std::string& population) {
    std::vector<int> indefinies;
    double somme = 0.0;
    long long total = 0;
    for (size_t c = 0; c < classes.size(); ++c) {
        if (classes[c].m_support <= 0) continue;
        const Metrique& m = choisir(classes[c]);
        total += classes[c].m_support;
        if (m.m_definie) somme += m.m_valeur * (double)classes[c].m_support; else indefinies.push_back((int)c);
    }
    const std::string pop = population + ", ponderee par le support";
    if (total <= 0) return Indefinie("aucune ligne reelle", pop);
    if (!indefinies.empty())
        return Indefinie(nom + " indefinie pour les classes " + ListeClasses(indefinies), pop);
    return Definie(somme / (double)total, pop);
}

const Metrique& SelPrecision(const MetriquesClasse& m) { return m.m_precision; }
const Metrique& SelRappel(const MetriquesClasse& m) { return m.m_rappel; }
const Metrique& SelF1(const MetriquesClasse& m) { return m.m_f1; }
const Metrique& SelSpecificite(const MetriquesClasse& m) { return m.m_specificite; }

ValeurJson MetriquesClasseVersJson(const MetriquesClasse& m, int id, const std::string& nom) {
    ValeurJson j = ValeurJson::Objet();
    j["id"] = ValeurJson(id);
    j["nom"] = ValeurJson(nom);
    j["support"] = ValeurJson(m.m_support);
    j["tp"] = ValeurJson(m.m_tp);
    j["fp"] = ValeurJson(m.m_fp);
    j["fn"] = ValeurJson(m.m_fn);
    j["tn"] = ValeurJson(m.m_tn);
    j["precision"] = m.m_precision.VersJson();
    j["rappel"] = m.m_rappel.VersJson();
    j["specificite"] = m.m_specificite.VersJson();
    j["f1"] = m.m_f1.VersJson();
    j["fpr"] = m.m_fpr.VersJson();
    j["fnr"] = m.m_fnr.VersJson();
    return j;
}

std::string NombreTexte(double v) {
    char tampon[64];
    auto r = std::to_chars(tampon, tampon + sizeof(tampon), v);
    return std::string(tampon, r.ptr);
}

std::string CelluleTexte(const std::string& texte) {
    if (texte.find_first_of(";\"\r\n") == std::string::npos) return texte;
    std::string s = "\"";
    for (char ch : texte) { if (ch == '"') s += '"'; s += ch; }
    return s + "\"";
}

std::string CelluleMetrique(const Metrique& m) { return m.m_definie ? NombreTexte(m.m_valeur) : std::string(); }

Erreur ErreurMetriques(const std::string& message) {
    return FaireErreur(CodeErreur::DonneesInvalides, "metriques", message);
}

} // namespace

ValeurJson Metrique::VersJson() const {
    ValeurJson j = ValeurJson::Objet();
    j["value"] = m_definie ? ValeurJson(m_valeur) : ValeurJson();
    j["status"] = ValeurJson(m_definie ? "defined" : "undefined");
    j["reason"] = m_definie ? ValeurJson() : ValeurJson(m_raison);
    j["definition_version"] = ValeurJson(kVersionDefinition);
    j["population"] = ValeurJson(m_population);
    return j;
}

Resultat<Confusion> CalculerConfusion(const std::vector<int>& verites, const std::vector<int>& predictions,
                                      int nbClasses) {
    if (verites.size() != predictions.size())
        return ErreurMetriques("verites et predictions de tailles differentes");
    if (verites.empty()) return ErreurMetriques("test vide : evaluation impossible");
    if (nbClasses <= 0) return ErreurMetriques("nombre de classes invalide");
    for (size_t i = 0; i < verites.size(); ++i) {
        if (verites[i] < 0 || verites[i] >= nbClasses)
            return ErreurMetriques("ligne " + std::to_string(i + 1) + " : classe reelle " +
                                   std::to_string(verites[i]) + " hors de [0," + std::to_string(nbClasses) + ")");
        if (predictions[i] < 0 || predictions[i] >= nbClasses)
            return ErreurMetriques("ligne " + std::to_string(i + 1) + " : classe predite " +
                                   std::to_string(predictions[i]) + " hors de [0," + std::to_string(nbClasses) + ")");
    }
    Confusion conf;
    conf.m_nbClasses = nbClasses;
    conf.m_comptes.assign((size_t)nbClasses * nbClasses, 0);
    for (size_t i = 0; i < verites.size(); ++i) ++conf.m_comptes[(size_t)verites[i] * nbClasses + predictions[i]];
    conf.m_total = (long long)verites.size();
    return conf;
}

RapportMetriques CalculerMetriques(const Confusion& confusion, const std::vector<std::string>& nomsClasses,
                                   const std::string& population) {
    RapportMetriques r;
    r.m_confusion = confusion;
    r.m_nomsClasses = nomsClasses;
    for (int c = 0; c < confusion.m_nbClasses; ++c) r.m_classes.push_back(CalculerClasse(confusion, c, population));
    for (int c = 0; c < confusion.m_nbClasses; ++c)
        if (r.m_classes[c].m_support == 0) r.m_classesExcluesPonderation.push_back(c);

    long long bonnes = 0;
    for (const auto& m : r.m_classes) bonnes += m.m_tp;
    r.m_exactitude = Quotient((double)bonnes, (double)confusion.m_total, "test vide : evaluation impossible",
                              population);
    r.m_precisionMacro = MoyenneMacro(r.m_classes, SelPrecision, "precision", population);
    r.m_rappelMacro = MoyenneMacro(r.m_classes, SelRappel, "rappel", population);
    r.m_f1Macro = MoyenneMacro(r.m_classes, SelF1, "F1", population);
    r.m_specificiteMacro = MoyenneMacro(r.m_classes, SelSpecificite, "specificite", population);
    r.m_precisionPonderee = MoyennePonderee(r.m_classes, SelPrecision, "precision", population);
    r.m_rappelPondere = MoyennePonderee(r.m_classes, SelRappel, "rappel", population);
    r.m_f1Pondere = MoyennePonderee(r.m_classes, SelF1, "F1", population);
    return r;
}

ValeurJson RapportMetriques::VersJson() const {
    ValeurJson j = ValeurJson::Objet();
    j["definition_version"] = ValeurJson(m_definition);

    ValeurJson conf = ValeurJson::Objet();
    ValeurJson axes = ValeurJson::Objet();
    axes["lignes"] = ValeurJson("reel");
    axes["colonnes"] = ValeurJson("predit");
    conf["axes"] = axes;
    ValeurJson noms = ValeurJson::Tableau();
    for (const auto& n : m_nomsClasses) noms.Ajouter(ValeurJson(n));
    conf["classes"] = noms;
    ValeurJson comptes = ValeurJson::Tableau();
    for (int r = 0; r < m_confusion.m_nbClasses; ++r) {
        ValeurJson ligne = ValeurJson::Tableau();
        for (int c = 0; c < m_confusion.m_nbClasses; ++c) ligne.Ajouter(ValeurJson(m_confusion.Case(r, c)));
        comptes.Ajouter(ligne);
    }
    conf["comptes"] = comptes;
    conf["total"] = ValeurJson(m_confusion.m_total);
    j["confusion"] = conf;

    ValeurJson classes = ValeurJson::Tableau();
    for (size_t c = 0; c < m_classes.size(); ++c)
        classes.Ajouter(MetriquesClasseVersJson(m_classes[c], (int)c, c < m_nomsClasses.size() ? m_nomsClasses[c] : ""));
    j["classes"] = classes;

    ValeurJson agr = ValeurJson::Objet();
    agr["exactitude"] = m_exactitude.VersJson();
    agr["precision_macro"] = m_precisionMacro.VersJson();
    agr["rappel_macro"] = m_rappelMacro.VersJson();
    agr["f1_macro"] = m_f1Macro.VersJson();
    agr["specificite_macro"] = m_specificiteMacro.VersJson();
    agr["precision_ponderee"] = m_precisionPonderee.VersJson();
    agr["rappel_pondere"] = m_rappelPondere.VersJson();
    agr["f1_pondere"] = m_f1Pondere.VersJson();
    j["agregats"] = agr;

    ValeurJson exclues = ValeurJson::Tableau();
    for (int c : m_classesExcluesPonderation) exclues.Ajouter(ValeurJson(c));
    j["classes_exclues_ponderation"] = exclues;
    return j;
}

std::string RapportMetriques::VersCsvComptes() const {
    std::string s = "classe;support;TP;FP;FN;TN;precision;rappel;specificite;F1;FPR;FNR\n";
    for (size_t c = 0; c < m_classes.size(); ++c) {
        const MetriquesClasse& m = m_classes[c];
        s += CelluleTexte(c < m_nomsClasses.size() ? m_nomsClasses[c] : std::to_string(c));
        s += ";" + std::to_string(m.m_support) + ";" + std::to_string(m.m_tp) + ";" + std::to_string(m.m_fp);
        s += ";" + std::to_string(m.m_fn) + ";" + std::to_string(m.m_tn);
        s += ";" + CelluleMetrique(m.m_precision) + ";" + CelluleMetrique(m.m_rappel);
        s += ";" + CelluleMetrique(m.m_specificite) + ";" + CelluleMetrique(m.m_f1);
        s += ";" + CelluleMetrique(m.m_fpr) + ";" + CelluleMetrique(m.m_fnr) + "\n";
    }
    s += "\nmatrice de confusion (lignes = reel, colonnes = predit)\nreel\\predit";
    for (int c = 0; c < m_confusion.m_nbClasses; ++c)
        s += ";" + CelluleTexte(c < (int)m_nomsClasses.size() ? m_nomsClasses[c] : std::to_string(c));
    s += "\n";
    for (int r = 0; r < m_confusion.m_nbClasses; ++r) {
        s += CelluleTexte(r < (int)m_nomsClasses.size() ? m_nomsClasses[r] : std::to_string(r));
        for (int c = 0; c < m_confusion.m_nbClasses; ++c) s += ";" + std::to_string(m_confusion.Case(r, c));
        s += "\n";
    }
    s += "\ndefinitions (" + m_definition + ") : precision=TP/(TP+FP) ; rappel=TP/(TP+FN) ; "
         "specificite=TN/(TN+FP) ; F1=2TP/(2TP+FP+FN) ; FPR=FP/(FP+TN) ; FNR=FN/(FN+TP) ; "
         "cellule vide = indefini (denominateur nul)\n";
    return s;
}

} // namespace decid
