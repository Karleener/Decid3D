// Validation croisee, balayage de parametres, ROC et points pedagogiques (spec v2 §6.4, §7.5).
#include "Validation.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

#include "Normalisation.h"
#include "Partage.h"

namespace decid {
namespace {

Erreur ErreurValidation(const std::string& message) {
    return FaireErreur(CodeErreur::DonneesInvalides, "validation", message);
}

ValeurJson TableauEntiers(const std::vector<int>& v) {
    ValeurJson t = ValeurJson::Tableau();
    for (int x : v) t.Ajouter(ValeurJson(x));
    return t;
}

const char* NomMode(ModePlis m) { return m == ModePlis::OrdreFichier ? "ordre_fichier" : "melange"; }

// ---------- construction des plis ----------

Statut VerifierPerimetre(const JeuDeDonnees& jeu, const std::vector<int>& perimetre, int k) {
    if (perimetre.empty()) return ErreurValidation("le perimetre de la validation croisee est vide");
    std::vector<char> vu(static_cast<size_t>(std::max(jeu.m_nbLignes, 0)), 0);
    for (int i : perimetre) {
        if (i < 0 || i >= jeu.m_nbLignes)
            return ErreurValidation("indice de ligne hors du jeu dans le perimetre : " + std::to_string(i));
        if (vu[static_cast<size_t>(i)])
            return ErreurValidation("ligne en double dans le perimetre : " + std::to_string(i));
        vu[static_cast<size_t>(i)] = 1;
    }
    if (k < 2 || k > static_cast<int>(perimetre.size()))
        return ErreurValidation("le nombre de plis k = " + std::to_string(k) + " doit etre compris entre 2 et "
                                + std::to_string(perimetre.size()) + " (taille du perimetre)");
    return Succes();
}

void Melanger(std::vector<int>& v, std::uint64_t& etat) {
    for (size_t i = v.size(); i > 1; --i) {
        size_t j = static_cast<size_t>(TirageUniforme(etat, static_cast<std::uint64_t>(i)));
        std::swap(v[i - 1], v[j]);
    }
}

// k blocs contigus, tailles differant d'au plus 1 (les premiers sont les plus grands)
std::vector<std::vector<int>> DecouperEnBlocs(const std::vector<int>& ordre, int k) {
    std::vector<std::vector<int>> blocs;
    size_t base = ordre.size() / static_cast<size_t>(k), reste = ordre.size() % static_cast<size_t>(k), debut = 0;
    for (size_t f = 0; f < static_cast<size_t>(k); ++f) {
        size_t taille = base + (f < reste ? 1 : 0);
        blocs.emplace_back(ordre.begin() + static_cast<std::ptrdiff_t>(debut),
                           ordre.begin() + static_cast<std::ptrdiff_t>(debut + taille));
        debut += taille;
    }
    return blocs;
}

// Chaque classe est repartie separement, ses lignes distribuees tour a tour (le tourniquet continue d'une
// classe a la suivante : les effectifs des plis restent equilibres).
std::vector<std::vector<int>> RepartirParClasse(const JeuDeDonnees& jeu, const std::vector<int>& ordre, int k,
                                                bool melanger, std::uint64_t& etat) {
    std::vector<std::vector<int>> groupes(static_cast<size_t>(std::max(jeu.NbClasses(), 1)));
    for (int ligne : ordre) groupes[static_cast<size_t>(jeu.m_labels[static_cast<size_t>(ligne)])].push_back(ligne);
    std::vector<std::vector<int>> blocs(static_cast<size_t>(k));
    size_t pli = 0;
    for (auto& g : groupes) {
        if (melanger) Melanger(g, etat);
        for (int ligne : g) {
            blocs[pli].push_back(ligne);
            pli = (pli + 1) % blocs.size();
        }
    }
    return blocs;
}

std::vector<std::vector<int>> ChoisirBlocs(const JeuDeDonnees& jeu, const std::vector<int>& perimetre, int k,
                                           ModePlis mode, bool stratifie, std::uint64_t graine) {
    std::uint64_t etat = graine;
    bool melanger = mode == ModePlis::Melange;
    if (stratifie) return RepartirParClasse(jeu, perimetre, k, melanger, etat);
    std::vector<int> ordre = perimetre;
    if (melanger) Melanger(ordre, etat);
    return DecouperEnBlocs(ordre, k);
}

std::vector<Pli> PlisDepuisBlocs(const std::vector<int>& perimetre, std::vector<std::vector<int>> blocs, int nbLignes) {
    std::vector<int> trie = perimetre;
    std::sort(trie.begin(), trie.end());
    std::vector<Pli> plis;
    std::vector<char> enValidation(static_cast<size_t>(nbLignes), 0);
    for (auto& bloc : blocs) {
        std::sort(bloc.begin(), bloc.end());
        for (int i : bloc) enValidation[static_cast<size_t>(i)] = 1;
        Pli p;
        p.m_validation = bloc;
        for (int i : trie)
            if (!enValidation[static_cast<size_t>(i)]) p.m_apprentissage.push_back(i);
        for (int i : bloc) enValidation[static_cast<size_t>(i)] = 0;
        plis.push_back(std::move(p));
    }
    return plis;
}

// ---------- validation croisee ----------

Statut VerifierPlan(const JeuDeDonnees& jeu, const PlanPlis& plan) {
    int k = static_cast<int>(plan.m_plis.size());
    Statut s = VerifierPerimetre(jeu, plan.m_perimetre, k);
    if (!s.Ok()) return s;
    std::vector<char> dansPerimetre(static_cast<size_t>(jeu.m_nbLignes), 0);
    for (int i : plan.m_perimetre) dansPerimetre[static_cast<size_t>(i)] = 1;
    std::vector<int> comptes(static_cast<size_t>(jeu.m_nbLignes), 0);
    for (const Pli& p : plan.m_plis) {
        if (p.m_validation.empty() || p.m_apprentissage.empty())
            return ErreurValidation("un pli a une partie vide (apprentissage ou validation)");
        for (int i : p.m_validation) {
            if (i < 0 || i >= jeu.m_nbLignes || !dansPerimetre[static_cast<size_t>(i)])
                return ErreurValidation("une ligne de validation est hors du perimetre");
            ++comptes[static_cast<size_t>(i)];
        }
        std::vector<char> enValidation(static_cast<size_t>(jeu.m_nbLignes), 0);
        for (int i : p.m_validation) enValidation[static_cast<size_t>(i)] = 1;
        for (int i : p.m_apprentissage)
            if (i < 0 || i >= jeu.m_nbLignes || !dansPerimetre[static_cast<size_t>(i)] || enValidation[static_cast<size_t>(i)])
                return ErreurValidation("apprentissage et validation d'un pli se recouvrent, ou ligne hors perimetre");
    }
    for (int i : plan.m_perimetre)
        if (comptes[static_cast<size_t>(i)] != 1)
            return ErreurValidation("la ligne " + std::to_string(i) + " n'est pas en validation exactement une fois");
    return Succes();
}

bool Annule(const std::atomic<bool>* annulation) { return annulation != nullptr && annulation->load(); }

Erreur ErreurAnnulation() {
    return FaireErreur(CodeErreur::Annulation, "annulation", "validation croisee annulee a la demande, rien n'est publie");
}

std::vector<int> LabelsDe(const JeuDeDonnees& jeu, const std::vector<int>& lignes) {
    std::vector<int> labels;
    labels.reserve(lignes.size());
    for (int i : lignes) labels.push_back(jeu.m_labels[static_cast<size_t>(i)]);
    return labels;
}

int NbClassesDistinctes(const std::vector<int>& labels) {
    std::vector<int> u = labels;
    std::sort(u.begin(), u.end());
    return static_cast<int>(std::unique(u.begin(), u.end()) - u.begin());
}

Resultat<Normalisation> NormalisationDuPli(const JeuDeDonnees& jeu, const Pli& pli, const ConfigApprentissage& c) {
    return NormalisationDemandee(c.m_normalisation, c.m_fichierLim, c.m_bas, c.m_haut, jeu, pli.m_apprentissage, "");
}

Resultat<Modele> EntrainerPli(const cv::Mat& x, const std::vector<int>& labels, int nbClasses,
                              const ConfigApprentissage& c) {
    return Entrainer(x, labels, nbClasses, c.m_algorithme, c.m_graine);
}

Erreur AvecContextePli(const Erreur& e, size_t pli, size_t k) {
    Erreur r = e;
    r.m_message = "pli " + std::to_string(pli + 1) + "/" + std::to_string(k) + " : " + e.m_message;
    return r;
}

// Predictions de la validation d'un pli (normalisation et modele appris sur l'apprentissage du pli seul)
Resultat<std::vector<int>> PredirePli(const JeuDeDonnees& jeu, const Pli& pli, const ConfigApprentissage& c) {
    std::vector<int> labelsApp = LabelsDe(jeu, pli.m_apprentissage);
    if (NbClassesDistinctes(labelsApp) < 2)
        return ErreurValidation("l'apprentissage de ce pli ne contient qu'une seule classe : les lignes sont "
                                "ordonnees par classe (utiliser le melange ou la stratification)");
    auto norm = NormalisationDuPli(jeu, pli, c);
    if (!norm.Ok()) return norm.Err();
    auto xApp = AppliquerNormalisation(norm.Valeur(), jeu, pli.m_apprentissage);
    if (!xApp.Ok()) return xApp.Err();
    auto xVal = AppliquerNormalisation(norm.Valeur(), jeu, pli.m_validation);
    if (!xVal.Ok()) return xVal.Err();
    auto modele = EntrainerPli(xApp.Valeur(), labelsApp, jeu.NbClasses(), c);
    if (!modele.Ok()) return modele.Err();
    return Predire(modele.Valeur(), xVal.Valeur());
}

Metrique MetriqueDefinie(double valeur, const std::string& population) {
    Metrique m;
    m.m_definie = true;
    m.m_valeur = valeur;
    m.m_population = population;
    return m;
}

void CalculerStatistiquesPlis(RapportValidation& r) {
    std::vector<double> acc;
    for (const auto& m : r.m_parPli) acc.push_back(m.m_exactitude.m_valeur);
    double n = static_cast<double>(acc.size());
    double moyenne = std::accumulate(acc.begin(), acc.end(), 0.0) / n;
    double somme = 0;
    for (double a : acc) somme += (a - moyenne) * (a - moyenne);
    double ecart = n > 1 ? std::sqrt(somme / (n - 1)) : 0.0;
    r.m_exactitudeMoyennePlis = MetriqueDefinie(moyenne, "exactitude moyenne des " + std::to_string(acc.size()) + " plis de validation");
    r.m_exactitudeEcartTypePlis = MetriqueDefinie(ecart, "ecart-type (n-1) des exactitudes des plis de validation");
}

Statut CalculerHorsPli(const JeuDeDonnees& jeu, std::vector<int> vraies, RapportValidation& r) {
    auto conf = CalculerConfusion(vraies, r.m_predictionsHorsPli, jeu.NbClasses());
    if (!conf.Ok()) return conf.Err();
    r.m_horsPli = CalculerMetriques(conf.Valeur(), jeu.m_nomsClasses, "hors pli");
    return Succes();
}

Statut TraiterPli(const JeuDeDonnees& jeu, RapportValidation& r, size_t i, const ConfigApprentissage& c,
                  const std::vector<int>& position) {
    const Pli& pli = r.m_plan.m_plis[i];
    auto pred = PredirePli(jeu, pli, c);
    if (!pred.Ok()) return AvecContextePli(pred.Err(), i, r.m_plan.m_plis.size());
    std::vector<int> vraies = LabelsDe(jeu, pli.m_validation);
    auto conf = CalculerConfusion(vraies, pred.Valeur(), jeu.NbClasses());
    if (!conf.Ok()) return AvecContextePli(conf.Err(), i, r.m_plan.m_plis.size());
    r.m_parPli.push_back(CalculerMetriques(conf.Valeur(), jeu.m_nomsClasses, "validation pli " + std::to_string(i + 1)));
    for (size_t j = 0; j < pli.m_validation.size(); ++j)
        r.m_predictionsHorsPli[static_cast<size_t>(position[static_cast<size_t>(pli.m_validation[j])])] = pred.Valeur()[j];
    return Succes();
}

// ---------- balayage ----------

bool EstEntier(double v) { return std::isfinite(v) && v == std::floor(v) && std::fabs(v) < 1e9; }

// Parametre de balayage -> famille concernee et champ modifie
struct ParametreBalayage { const char* m_nom; Famille m_famille; bool m_entier; };
const ParametreBalayage PARAMETRES_BALAYAGE[] = {
    {"gamma", Famille::Svm, false}, {"c", Famille::Svm, false},
    {"nb_arbres", Famille::ForetAleatoire, true}, {"profondeur_max", Famille::ForetAleatoire, true},
    {"k", Famille::Kppv, true}, {"k", Famille::Em, true},
    {"nb_faibles", Famille::Boost, true},
};

void AffecterParametre(ParametresAlgorithme& a, const std::string& parametre, double v) {
    const int entier = static_cast<int>(v);
    switch (a.m_famille) {
    case Famille::Svm: (parametre == "gamma" ? a.m_svm.m_gamma : a.m_svm.m_c) = v; break;
    case Famille::ForetAleatoire: (parametre == "nb_arbres" ? a.m_foret.m_nbArbres : a.m_foret.m_profondeurMax) = entier; break;
    case Famille::Kppv: a.m_kppv.m_k = entier; break;
    case Famille::Em: a.m_em.m_k = entier; break;
    case Famille::Boost: a.m_boost.m_nbFaibles = entier; break;
    default: break;
    }
}

Statut AppliquerParametre(ConfigApprentissage& c, const std::string& parametre, double valeur) {
    bool connu = false;
    for (const auto& p : PARAMETRES_BALAYAGE) {
        if (parametre != p.m_nom) continue;
        connu = true;
        if (p.m_famille != c.m_algorithme.m_famille) continue;
        if (p.m_entier && !EstEntier(valeur)) return ErreurValidation("le parametre \"" + parametre + "\" doit etre un entier");
        AffecterParametre(c.m_algorithme, parametre, valeur);
        return Succes();
    }
    if (!connu) return ErreurValidation("parametre de balayage inconnu : \"" + parametre + "\"");
    return ErreurValidation(std::string("le parametre \"") + parametre + "\" ne s'applique pas a la famille "
                            + NomFamille(c.m_algorithme.m_famille));
}

void ChoisirMeilleur(RapportBalayage& r) {
    r.m_indiceMeilleur = 0;
    for (size_t i = 1; i < r.m_candidats.size(); ++i) {
        const ResultatCandidat& cand = r.m_candidats[i];
        const ResultatCandidat& best = r.m_candidats[static_cast<size_t>(r.m_indiceMeilleur)];
        double a = cand.m_validation.m_exactitudeMoyennePlis.m_valeur, b = best.m_validation.m_exactitudeMoyennePlis.m_valeur;
        bool egal = std::fabs(a - b) <= 1e-12;
        if (a > b + 1e-12 || (egal && cand.m_valeur < best.m_valeur)) r.m_indiceMeilleur = static_cast<int>(i);
    }
    r.m_meilleurEnBord = r.m_indiceMeilleur == 0 || r.m_indiceMeilleur == static_cast<int>(r.m_candidats.size()) - 1;
}

// ---------- logarithmes ----------

double Puissance10(int e) {
    double p = 1.0;
    for (int i = 0; i < std::abs(e); ++i) p *= 10.0;
    return e >= 0 ? p : 1.0 / p;
}

std::vector<double> DecadesEntieres(double debut, double fin) {
    std::vector<double> v;
    int e0 = static_cast<int>(std::lround(std::log10(debut)));
    if (std::fabs(Puissance10(e0) - debut) > 1e-9 * debut) return v;
    for (int e = e0; Puissance10(e) <= fin * (1.0 + 1e-12) && v.size() < 10000; ++e) v.push_back(Puissance10(e));
    return v;
}

}  // namespace

// ---------- plis ----------

ValeurJson PlanPlis::VersJson() const {
    ValeurJson j = ValeurJson::Objet();
    j["k"] = m_k;
    j["mode"] = NomMode(m_mode);
    j["stratifie"] = m_stratifie;
    j["graine"] = static_cast<unsigned long long>(m_graine);
    j["perimetre"] = TableauEntiers(m_perimetre);
    ValeurJson plis = ValeurJson::Tableau();
    for (const Pli& p : m_plis) {
        ValeurJson o = ValeurJson::Objet();
        o["apprentissage"] = TableauEntiers(p.m_apprentissage);
        o["validation"] = TableauEntiers(p.m_validation);
        plis.Ajouter(o);
    }
    j["plis"] = plis;
    return j;
}

Resultat<PlanPlis> CreerPlis(const JeuDeDonnees& jeu, const std::vector<int>& perimetre, int k, ModePlis mode,
                             bool stratifie, std::uint64_t graine) {
    Statut s = VerifierPerimetre(jeu, perimetre, k);
    if (!s.Ok()) return s.Err();
    if (stratifie && static_cast<int>(jeu.m_labels.size()) != jeu.m_nbLignes)
        return ErreurValidation("labels absents ou incoherents : stratification impossible");
    PlanPlis plan;
    plan.m_k = k;
    plan.m_mode = mode;
    plan.m_stratifie = stratifie;
    plan.m_graine = graine;
    plan.m_perimetre = perimetre;
    plan.m_plis = PlisDepuisBlocs(perimetre, ChoisirBlocs(jeu, perimetre, k, mode, stratifie, graine), jeu.m_nbLignes);
    return plan;
}

// ---------- validation croisee ----------

ValeurJson RapportValidation::VersJson() const {
    ValeurJson j = ValeurJson::Objet();
    j["plan"] = m_plan.VersJson();
    ValeurJson parPli = ValeurJson::Tableau();
    for (const auto& m : m_parPli) parPli.Ajouter(m.VersJson());
    j["par_pli"] = parPli;
    j["moyenne_par_pli"] = m_exactitudeMoyennePlis.VersJson();
    j["ecart_type_par_pli"] = m_exactitudeEcartTypePlis.VersJson();
    j["hors_pli"] = m_horsPli.VersJson();
    j["predictions_hors_pli"] = TableauEntiers(m_predictionsHorsPli);
    return j;
}

Resultat<RapportValidation> ValiderCroise(const JeuDeDonnees& jeu, const PlanPlis& plan,
                                          const ConfigApprentissage& config,
                                          const std::atomic<bool>* annulation, const RapporteurProgression& rapporter) {
    Statut s = VerifierPlan(jeu, plan);
    if (!s.Ok()) return s.Err();
    RapportValidation r;
    r.m_plan = plan;
    r.m_predictionsHorsPli.assign(plan.m_perimetre.size(), -1);
    std::vector<int> position(static_cast<size_t>(jeu.m_nbLignes), -1);
    for (size_t i = 0; i < plan.m_perimetre.size(); ++i) position[static_cast<size_t>(plan.m_perimetre[i])] = static_cast<int>(i);
    for (size_t i = 0; i < plan.m_plis.size(); ++i) {
        if (Annule(annulation)) return ErreurAnnulation();
        Statut st = TraiterPli(jeu, r, i, config, position);
        if (!st.Ok()) return st.Err();
        if (rapporter) rapporter("pli", static_cast<long>(i + 1), static_cast<long>(plan.m_plis.size()));
    }
    if (Annule(annulation)) return ErreurAnnulation();
    CalculerStatistiquesPlis(r);
    Statut hp = CalculerHorsPli(jeu, LabelsDe(jeu, plan.m_perimetre), r);
    if (!hp.Ok()) return hp.Err();
    return r;
}

// ---------- balayage ----------

std::vector<double> ValeursLogarithmiques(double debut, double fin, int parDecade) {
    std::vector<double> v;
    if (!(debut > 0) || !(fin >= debut) || parDecade < 1 || !std::isfinite(fin)) return v;
    if (parDecade == 1) {
        v = DecadesEntieres(debut, fin);
        if (!v.empty()) return v;
    }
    double l0 = std::log10(debut), l1 = std::log10(fin);
    long n = static_cast<long>(std::floor((l1 - l0) * parDecade + 1e-9)) + 1;
    for (long i = 0; i < n && i < 10000; ++i)
        v.push_back(std::pow(10.0, l0 + static_cast<double>(i) / static_cast<double>(parDecade)));
    return v;
}

ValeurJson RapportBalayage::VersJson() const {
    ValeurJson j = ValeurJson::Objet();
    j["parametre"] = m_parametre;
    j["critere"] = m_critere;
    j["indice_meilleur"] = m_indiceMeilleur;
    j["meilleur_en_bord"] = m_meilleurEnBord;
    ValeurJson cands = ValeurJson::Tableau();
    for (const auto& c : m_candidats) {
        ValeurJson o = ValeurJson::Objet();
        o["valeur"] = c.m_valeur;
        o["validation"] = c.m_validation.VersJson();
        cands.Ajouter(o);
    }
    j["candidats"] = cands;
    return j;
}

Resultat<RapportBalayage> Balayer(const JeuDeDonnees& jeu, const PlanPlis& plan, const ConfigApprentissage& base,
                                  const std::string& parametre, const std::vector<double>& valeurs,
                                  const std::atomic<bool>* annulation, const RapporteurProgression& rapporter) {
    if (valeurs.empty()) return ErreurValidation("aucune valeur a balayer");
    RapportBalayage r;
    r.m_parametre = parametre;
    for (size_t i = 0; i < valeurs.size(); ++i) {
        ConfigApprentissage c = base;
        Statut sp = AppliquerParametre(c, parametre, valeurs[i]);
        if (!sp.Ok()) return sp.Err();
        auto v = ValiderCroise(jeu, plan, c, annulation, rapporter);
        if (!v.Ok()) return v.Err();
        ResultatCandidat cand;
        cand.m_valeur = valeurs[i];
        cand.m_validation = std::move(v.Valeur());
        r.m_candidats.push_back(std::move(cand));
        if (rapporter) rapporter("candidat", static_cast<long>(i + 1), static_cast<long>(valeurs.size()));
    }
    ChoisirMeilleur(r);
    return r;
}

// ---------- ROC ----------

ValeurJson CourbeRoc::VersJson() const {
    ValeurJson j = ValeurJson::Objet();
    j["auc"] = m_auc;
    j["origine_score"] = m_origineScore;
    ValeurJson pts = ValeurJson::Tableau();
    for (const PointRoc& p : m_points) {
        ValeurJson o = ValeurJson::Objet();
        o["fpr"] = p.m_fpr;
        o["tpr"] = p.m_tpr;
        o["seuil"] = p.m_seuil;     // null pour le point initial (aucun exemple predit positif)
        pts.Ajouter(o);
    }
    j["points"] = pts;
    return j;
}

namespace {

Statut VerifierEntreesRoc(const std::vector<double>& scores, const std::vector<int>& estPositif) {
    if (scores.size() != estPositif.size()) return ErreurValidation("scores et classes reelles de tailles differentes");
    if (scores.empty()) return ErreurValidation("aucun score pour la courbe ROC");
    for (double s : scores)
        if (!std::isfinite(s)) return ErreurValidation("un score n'est pas un nombre fini");
    auto pos = std::count_if(estPositif.begin(), estPositif.end(), [](int p) { return p != 0; });
    if (pos == 0 || pos == static_cast<std::ptrdiff_t>(estPositif.size()))
        return ErreurValidation("la courbe ROC exige au moins un exemple positif et un negatif");
    return Succes();
}

double AireTrapezes(const std::vector<PointRoc>& pts) {
    double aire = 0;
    for (size_t i = 1; i < pts.size(); ++i) aire += (pts[i].m_fpr - pts[i - 1].m_fpr) * (pts[i].m_tpr + pts[i - 1].m_tpr) / 2.0;
    return aire;
}

}  // namespace

Resultat<CourbeRoc> CalculerRoc(const std::vector<double>& scores, const std::vector<int>& estPositif,
                                const std::string& origineScore) {
    Statut s = VerifierEntreesRoc(scores, estPositif);
    if (!s.Ok()) return s.Err();
    std::vector<size_t> ordre(scores.size());
    std::iota(ordre.begin(), ordre.end(), size_t{0});
    std::stable_sort(ordre.begin(), ordre.end(), [&](size_t a, size_t b) { return scores[a] > scores[b]; });
    double nbPos = static_cast<double>(std::count_if(estPositif.begin(), estPositif.end(), [](int p) { return p != 0; }));
    double nbNeg = static_cast<double>(scores.size()) - nbPos;
    CourbeRoc c;
    c.m_origineScore = origineScore;
    c.m_points.push_back({0.0, 0.0, std::numeric_limits<double>::infinity()});
    double tp = 0, fp = 0;
    for (size_t i = 0; i < ordre.size();) {
        double seuil = scores[ordre[i]];
        for (; i < ordre.size() && scores[ordre[i]] == seuil; ++i) (estPositif[ordre[i]] != 0 ? tp : fp) += 1.0;
        c.m_points.push_back({fp / nbNeg, tp / nbPos, seuil});
    }
    c.m_auc = AireTrapezes(c.m_points);
    return c;
}

// ---------- points pedagogiques ----------

std::vector<PointPedagogique> PointsFnrFpr(const RapportBalayage& balayage, int classePositive) {
    std::vector<PointPedagogique> points;
    for (const auto& cand : balayage.m_candidats) {
        const Confusion& c = cand.m_validation.m_horsPli.m_confusion;
        if (classePositive < 0 || classePositive >= c.m_nbClasses) continue;
        long long tp = c.Case(classePositive, classePositive), fn = 0, fp = 0, total = c.m_total;
        for (int j = 0; j < c.m_nbClasses; ++j) {
            fn += c.Case(classePositive, j);
            fp += c.Case(j, classePositive);
        }
        fn -= tp;
        fp -= tp;
        long long tn = total - tp - fn - fp;
        if (fn + tp == 0 || fp + tn == 0) continue;      // denominateur nul : point ignore, jamais invente
        double fnr = 100.0 * static_cast<double>(fn) / static_cast<double>(fn + tp);
        double fpr = 100.0 * static_cast<double>(fp) / static_cast<double>(fp + tn);
        points.push_back({cand.m_valeur, fnr, 100.0 - fpr});
    }
    return points;
}

}  // namespace decid
