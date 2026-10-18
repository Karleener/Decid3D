// Tests de la validation croisee, du balayage, de la ROC et des points FNR/FPR (ACC-06, 25, 26).
// Donnees SYNTHETIQUES deterministes generees ici (LCG maison) ; aucun fichier.
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "Normalisation.h"
#include "Partage.h"
#include "Validation.h"
#include "Verifier.h"

using namespace decid;

namespace {

struct Lcg {
    std::uint64_t m_etat;
    double Suivant() {
        m_etat = m_etat * 6364136223846793005ULL + 1442695040888963407ULL;
        return (double)(m_etat >> 11) / 9007199254740992.0;
    }
    double Bruit() { return Suivant() + Suivant() + Suivant() - 1.5; }
};

template <class T>
bool Reussi(const Resultat<T>& r, const char* contexte) {
    if (!r.Ok()) verif::Echec(__FILE__, __LINE__, std::string(contexte) + " : " + r.Err().Texte());
    return r.Ok();
}

template <class T>
bool Refuse(const Resultat<T>& r, const char* contexte) {
    if (r.Ok()) verif::Echec(__FILE__, __LINE__, std::string(contexte) + " : succes inattendu, une erreur etait attendue");
    return !r.Ok();
}

// Nuages 2D : n points par classe. entrelace = lignes alternant les classes ; sinon triees par classe.
JeuDeDonnees Nuages(const std::vector<std::pair<double, double>>& centres, int n, std::uint64_t graine, bool entrelace) {
    JeuDeDonnees j;
    j.m_nbAttributs = 2;
    j.m_nomsAttributs = {"x1", "x2"};
    Lcg g{graine};
    int k = (int)centres.size();
    for (int c = 0; c < k; ++c) j.m_nomsClasses.push_back("c" + std::to_string(c));
    std::vector<std::vector<std::pair<double, double>>> pts((size_t)k);
    for (int c = 0; c < k; ++c)
        for (int i = 0; i < n; ++i) pts[(size_t)c].push_back({centres[(size_t)c].first + g.Bruit(), centres[(size_t)c].second + g.Bruit()});
    auto ajouter = [&](int c, int i) {
        j.m_valeurs.push_back(pts[(size_t)c][(size_t)i].first);
        j.m_valeurs.push_back(pts[(size_t)c][(size_t)i].second);
        j.m_labels.push_back(c);
    };
    if (entrelace) { for (int i = 0; i < n; ++i) for (int c = 0; c < k; ++c) ajouter(c, i); }
    else           { for (int c = 0; c < k; ++c) for (int i = 0; i < n; ++i) ajouter(c, i); }
    j.m_nbLignes = (int)j.m_labels.size();
    return j;
}

JeuDeDonnees DeuxNuages(bool entrelace) { return Nuages({{0.0, 0.0}, {3.0, 3.0}}, 40, 5, entrelace); }

std::vector<int> Toutes(const JeuDeDonnees& j) {
    std::vector<int> v((size_t)j.m_nbLignes);
    for (int i = 0; i < j.m_nbLignes; ++i) v[(size_t)i] = i;
    return v;
}

ConfigApprentissage ConfigSvm() {
    ConfigApprentissage c;
    c.m_algorithme.m_famille = Famille::Svm;
    c.m_algorithme.m_svm.m_c = 1.0;
    c.m_algorithme.m_svm.m_gamma = 0.5;
    return c;
}

bool Contient(const std::string& texte, const std::string& morceau) { return texte.find(morceau) != std::string::npos; }

std::vector<int> JsonVersEntiers(const ValeurJson& t) {
    std::vector<int> v;
    for (const auto& e : t.Elements()) v.push_back((int)e.Entier());
    return v;
}

bool MemesPlis(const PlanPlis& a, const PlanPlis& b) {
    if (a.m_plis.size() != b.m_plis.size()) return false;
    for (size_t i = 0; i < a.m_plis.size(); ++i)
        if (a.m_plis[i].m_validation != b.m_plis[i].m_validation || a.m_plis[i].m_apprentissage != b.m_plis[i].m_apprentissage) return false;
    return true;
}

} // namespace

// ---------- ACC-06 : plis ----------

TEST(acc06_chaque_ligne_du_perimetre_en_validation_une_fois) {
    JeuDeDonnees jeu = DeuxNuages(true);
    std::vector<int> perimetre;
    for (int i = 0; i < jeu.m_nbLignes; ++i) if (i % 5 != 0) perimetre.push_back(i);   // 64 lignes sur 80
    for (ModePlis mode : {ModePlis::OrdreFichier, ModePlis::Melange}) {
        for (bool strat : {false, true}) {
            auto r = CreerPlis(jeu, perimetre, 5, mode, strat, 3);
            if (!Reussi(r, "CreerPlis")) continue;
            const PlanPlis& plan = r.Valeur();
            VERIFIER_EGAL(plan.m_plis.size(), (size_t)5);
            std::vector<int> compte((size_t)jeu.m_nbLignes, 0);
            size_t mini = 1000, maxi = 0;
            for (const Pli& p : plan.m_plis) {
                VERIFIER(std::is_sorted(p.m_validation.begin(), p.m_validation.end()));
                VERIFIER(std::is_sorted(p.m_apprentissage.begin(), p.m_apprentissage.end()));
                for (int i : p.m_validation) ++compte[(size_t)i];
                std::vector<int> inter;
                std::set_intersection(p.m_apprentissage.begin(), p.m_apprentissage.end(), p.m_validation.begin(),
                                      p.m_validation.end(), std::back_inserter(inter));
                VERIFIER(inter.empty());
                VERIFIER_EGAL(p.m_apprentissage.size() + p.m_validation.size(), perimetre.size());
                mini = std::min(mini, p.m_validation.size());
                maxi = std::max(maxi, p.m_validation.size());
            }
            VERIFIER(maxi - mini <= 1);
            std::vector<char> dans((size_t)jeu.m_nbLignes, 0);
            for (int i : perimetre) dans[(size_t)i] = 1;
            for (int i = 0; i < jeu.m_nbLignes; ++i) VERIFIER_EGAL(compte[(size_t)i], dans[(size_t)i] ? 1 : 0);
        }
    }
}

TEST(acc06_plis_identiques_pour_meme_graine_et_json_avec_indices) {
    JeuDeDonnees jeu = DeuxNuages(true);
    auto a = CreerPlis(jeu, Toutes(jeu), 4, ModePlis::Melange, false, 17);
    auto b = CreerPlis(jeu, Toutes(jeu), 4, ModePlis::Melange, false, 17);
    auto c = CreerPlis(jeu, Toutes(jeu), 4, ModePlis::Melange, false, 18);
    if (!Reussi(a, "a") || !Reussi(b, "b") || !Reussi(c, "c")) return;
    VERIFIER(MemesPlis(a.Valeur(), b.Valeur()));
    VERIFIER(!MemesPlis(a.Valeur(), c.Valeur()));
    auto relu = LireJson(a.Valeur().VersJson().Ecrire());
    if (!Reussi(relu, "relecture JSON")) return;
    const ValeurJson& plis = relu.Valeur().Membre("plis");
    VERIFIER_EGAL(plis.Elements().size(), (size_t)4);
    for (size_t i = 0; i < 4 && i < plis.Elements().size(); ++i) {
        VERIFIER(JsonVersEntiers(plis.Elements()[i].Membre("validation")) == a.Valeur().m_plis[i].m_validation);
        VERIFIER(JsonVersEntiers(plis.Elements()[i].Membre("apprentissage")) == a.Valeur().m_plis[i].m_apprentissage);
    }
    VERIFIER_EGAL(relu.Valeur().Membre("k").Entier(), 4LL);
    VERIFIER_EGAL(relu.Valeur().Membre("graine").Entier(), 17LL);
}

TEST(ordre_fichier_garde_l_ordre_du_perimetre_en_blocs_contigus) {
    JeuDeDonnees jeu = DeuxNuages(true);
    std::vector<int> perimetre = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0};     // ordre fourni : decroissant
    auto r = CreerPlis(jeu, perimetre, 3, ModePlis::OrdreFichier, false, 1);
    if (!Reussi(r, "CreerPlis")) return;
    const auto& p = r.Valeur().m_plis;
    VERIFIER(p[0].m_validation == (std::vector<int>{6, 7, 8, 9}));     // 4, 3, 3 : les premiers plus grands
    VERIFIER(p[1].m_validation == (std::vector<int>{3, 4, 5}));
    VERIFIER(p[2].m_validation == (std::vector<int>{0, 1, 2}));
    VERIFIER(r.Valeur().m_perimetre == perimetre);
}

TEST(plis_refusent_k_et_perimetre_invalides) {
    JeuDeDonnees jeu = DeuxNuages(true);
    std::vector<int> tout = Toutes(jeu);
    Refuse(CreerPlis(jeu, tout, 1, ModePlis::Melange, false, 1), "k = 1");
    Refuse(CreerPlis(jeu, tout, 0, ModePlis::Melange, false, 1), "k = 0");
    Refuse(CreerPlis(jeu, tout, -3, ModePlis::Melange, false, 1), "k negatif");
    Refuse(CreerPlis(jeu, tout, jeu.m_nbLignes + 1, ModePlis::Melange, false, 1), "k > N");
    VERIFIER(CreerPlis(jeu, tout, jeu.m_nbLignes, ModePlis::Melange, false, 1).Ok());   // k = N (leave-one-out)
    Refuse(CreerPlis(jeu, {1, 2, 3}, 4, ModePlis::OrdreFichier, false, 1), "k > taille du perimetre");
    Refuse(CreerPlis(jeu, {}, 2, ModePlis::Melange, false, 1), "perimetre vide");
    Refuse(CreerPlis(jeu, {1, 2, 2, 3}, 2, ModePlis::Melange, false, 1), "doublon");
    Refuse(CreerPlis(jeu, {1, 2, jeu.m_nbLignes}, 2, ModePlis::Melange, false, 1), "indice hors du jeu");
    Refuse(CreerPlis(jeu, {1, -2, 3}, 2, ModePlis::Melange, false, 1), "indice negatif");
}

TEST(stratifie_repartit_chaque_classe_proportionnellement) {
    JeuDeDonnees jeu = Nuages({{0, 0}, {3, 3}, {0, 4}}, 23, 9, false);   // 3 classes de 23, triees par classe
    for (ModePlis mode : {ModePlis::OrdreFichier, ModePlis::Melange}) {
        auto r = CreerPlis(jeu, Toutes(jeu), 5, mode, true, 4);
        if (!Reussi(r, "stratifie")) continue;
        std::vector<std::vector<long>> comptes;
        size_t mini = 1000, maxi = 0;
        for (const Pli& p : r.Valeur().m_plis) {
            comptes.push_back(ComptesClasses(jeu, p.m_validation));
            mini = std::min(mini, p.m_validation.size());
            maxi = std::max(maxi, p.m_validation.size());
        }
        VERIFIER(maxi - mini <= 1);
        for (size_t c = 0; c < 3; ++c) {
            long lo = 1000, hi = 0;
            for (const auto& v : comptes) { lo = std::min(lo, v[c]); hi = std::max(hi, v[c]); }
            VERIFIER(hi - lo <= 1);       // 23 / 5 : 4 ou 5 lignes de chaque classe par pli
            VERIFIER(lo >= 4);
        }
    }
}

// ---------- ValiderCroise ----------

TEST(acc06_le_scaler_du_pli_ne_voit_pas_sa_validation) {
    JeuDeDonnees jeu = DeuxNuages(true);
    jeu.m_valeurs[0] = 1000.0;          // la ligne 0 (classe 0) est une valeur aberrante sur x1
    auto plan = CreerPlis(jeu, Toutes(jeu), 5, ModePlis::Melange, false, 6);
    if (!Reussi(plan, "CreerPlis")) return;
    ConfigApprentissage config = ConfigSvm();
    config.m_normalisation = "minmax";
    auto rapport = ValiderCroise(jeu, plan.Valeur(), config, nullptr, {});
    if (!Reussi(rapport, "ValiderCroise")) return;
    VERIFIER_EGAL(rapport.Valeur().m_parPli.size(), (size_t)5);
    for (size_t f = 0; f < 5; ++f) {
        const Pli& pli = plan.Valeur().m_plis[f];
        auto norm = AjusterMinMax(jeu, pli.m_apprentissage, 0.0, 1.0, "");
        if (!Reussi(norm, "AjusterMinMax")) return;
        bool aberrantEnValidation = std::find(pli.m_validation.begin(), pli.m_validation.end(), 0) != pli.m_validation.end();
        if (aberrantEnValidation) VERIFIER(norm.Valeur().m_max[0] < 100.0);      // le scaler du pli ignore la ligne 0
        else VERIFIER_EGAL(norm.Valeur().m_max[0], 1000.0);
        // reproduction independante du pli : meme confusion que le rapport
        std::vector<int> la, lv;
        for (int i : pli.m_apprentissage) la.push_back(jeu.m_labels[(size_t)i]);
        for (int i : pli.m_validation) lv.push_back(jeu.m_labels[(size_t)i]);
        auto xa = AppliquerNormalisation(norm.Valeur(), jeu, pli.m_apprentissage);
        auto xv = AppliquerNormalisation(norm.Valeur(), jeu, pli.m_validation);
        if (!Reussi(xa, "xa") || !Reussi(xv, "xv")) return;
        auto m = EntrainerSvm(xa.Valeur(), la, 2, config.m_algorithme.m_svm);
        if (!Reussi(m, "EntrainerSvm")) return;
        auto pred = Predire(m.Valeur(), xv.Valeur());
        if (!Reussi(pred, "Predire")) return;
        auto conf = CalculerConfusion(lv, pred.Valeur(), 2);
        if (!Reussi(conf, "CalculerConfusion")) return;
        VERIFIER(conf.Valeur().m_comptes == rapport.Valeur().m_parPli[f].m_confusion.m_comptes);
    }
}

TEST(validation_croisee_svm_statistiques_et_hors_pli) {
    JeuDeDonnees jeu = DeuxNuages(true);
    std::vector<int> perimetre;
    for (int i = 0; i < jeu.m_nbLignes; ++i) if (i % 4 != 3) perimetre.push_back(i);
    auto plan = CreerPlis(jeu, perimetre, 5, ModePlis::Melange, false, 2);
    if (!Reussi(plan, "CreerPlis")) return;
    std::vector<long> phases;
    long dernier = 0, total = 0;
    RapporteurProgression rapporter = [&](const std::string& phase, long i, long n) {
        if (phase == "pli") { phases.push_back(i); dernier = i; total = n; }
    };
    auto r = ValiderCroise(jeu, plan.Valeur(), ConfigSvm(), nullptr, rapporter);
    if (!Reussi(r, "ValiderCroise")) return;
    const RapportValidation& v = r.Valeur();
    VERIFIER_EGAL(phases.size(), (size_t)5);
    VERIFIER_EGAL(dernier, 5L);
    VERIFIER_EGAL(total, 5L);
    VERIFIER_EGAL(v.m_parPli.size(), (size_t)5);
    double somme = 0;
    for (const auto& m : v.m_parPli) somme += m.m_exactitude.m_valeur;
    double moyenne = somme / 5.0, s2 = 0;
    for (const auto& m : v.m_parPli) s2 += (m.m_exactitude.m_valeur - moyenne) * (m.m_exactitude.m_valeur - moyenne);
    VERIFIER(v.m_exactitudeMoyennePlis.m_definie);
    VERIFIER_PROCHE(v.m_exactitudeMoyennePlis.m_valeur, moyenne, 1e-12);
    VERIFIER_PROCHE(v.m_exactitudeEcartTypePlis.m_valeur, std::sqrt(s2 / 4.0), 1e-12);
    VERIFIER(moyenne > 0.85);
    VERIFIER_EGAL(v.m_predictionsHorsPli.size(), perimetre.size());
    VERIFIER_EGAL(v.m_horsPli.m_confusion.m_total, (long long)perimetre.size());
    VERIFIER_EGAL(v.m_horsPli.m_exactitude.m_population, std::string("hors pli"));
    VERIFIER_EGAL(v.m_parPli[2].m_exactitude.m_population, std::string("validation pli 3"));
    long long bons = 0;
    for (size_t i = 0; i < perimetre.size(); ++i) {
        VERIFIER(v.m_predictionsHorsPli[i] >= 0 && v.m_predictionsHorsPli[i] < 2);
        bons += v.m_predictionsHorsPli[i] == jeu.m_labels[(size_t)perimetre[i]] ? 1 : 0;
    }
    VERIFIER_PROCHE(v.m_horsPli.m_exactitude.m_valeur, (double)bons / (double)perimetre.size(), 1e-12);
    // JSON : noms explicites distincts
    auto j = LireJson(v.VersJson().Ecrire());
    if (Reussi(j, "JSON du rapport")) {
        VERIFIER(j.Valeur().Contient("moyenne_par_pli"));
        VERIFIER(j.Valeur().Contient("ecart_type_par_pli"));
        VERIFIER(j.Valeur().Contient("hors_pli"));
        VERIFIER(j.Valeur().Membre("plan").Contient("plis"));
        VERIFIER_EGAL(j.Valeur().Membre("par_pli").Elements().size(), (size_t)5);
    }
}

TEST(validation_croisee_foret_reproductible) {
    JeuDeDonnees jeu = DeuxNuages(true);
    auto plan = CreerPlis(jeu, Toutes(jeu), 4, ModePlis::Melange, true, 8);
    if (!Reussi(plan, "CreerPlis")) return;
    ConfigApprentissage c;
    c.m_algorithme.m_famille = Famille::ForetAleatoire;
    c.m_algorithme.m_foret.m_nbArbres = 11;
    c.m_graine = 42;
    auto a = ValiderCroise(jeu, plan.Valeur(), c, nullptr, {});
    auto b = ValiderCroise(jeu, plan.Valeur(), c, nullptr, {});
    if (!Reussi(a, "foret a") || !Reussi(b, "foret b")) return;
    VERIFIER(a.Valeur().m_predictionsHorsPli == b.Valeur().m_predictionsHorsPli);
    VERIFIER(a.Valeur().m_exactitudeMoyennePlis.m_valeur > 0.8);
    VERIFIER_EGAL(a.Valeur().VersJson().Ecrire(false), b.Valeur().VersJson().Ecrire(false));
}

// Lecon du TD M1 2024 : sur un jeu TRIE PAR CLASSE, une validation croisee d'ordre 2 en ordre fichier
// coupe par blocs de classes ; l'apprentissage d'un pli ne contient alors qu'UNE classe et rien ne peut
// etre appris. Il faut melanger (ou stratifier) avant de decouper. L'erreur doit etre explicite.
TEST(exercice_du_td_jeu_trie_par_classe_ordre_fichier_echoue_melange_reussit) {
    JeuDeDonnees jeu = DeuxNuages(false);            // classe 0 (40 lignes) puis classe 1 (40 lignes)
    auto ordre = CreerPlis(jeu, Toutes(jeu), 2, ModePlis::OrdreFichier, false, 1);
    if (!Reussi(ordre, "plan ordre fichier")) return;
    auto echec = ValiderCroise(jeu, ordre.Valeur(), ConfigSvm(), nullptr, {});
    if (Refuse(echec, "ordre fichier sur jeu trie par classe")) {
        VERIFIER(Contient(echec.Err().m_message, "une seule classe"));
        VERIFIER(Contient(echec.Err().m_message, "pli 1"));
    }
    auto melange = CreerPlis(jeu, Toutes(jeu), 2, ModePlis::Melange, false, 1);
    if (!Reussi(melange, "plan melange")) return;
    auto ok = ValiderCroise(jeu, melange.Valeur(), ConfigSvm(), nullptr, {});
    if (Reussi(ok, "melange")) VERIFIER(ok.Valeur().m_exactitudeMoyennePlis.m_valeur > 0.85);
    auto strat = CreerPlis(jeu, Toutes(jeu), 2, ModePlis::OrdreFichier, true, 1);
    if (Reussi(strat, "plan stratifie")) VERIFIER(ValiderCroise(jeu, strat.Valeur(), ConfigSvm(), nullptr, {}).Ok());
}

TEST(validation_croisee_annulation_et_plan_incoherent) {
    JeuDeDonnees jeu = DeuxNuages(true);
    auto plan = CreerPlis(jeu, Toutes(jeu), 3, ModePlis::Melange, false, 1);
    if (!Reussi(plan, "CreerPlis")) return;
    std::atomic<bool> stop(true);
    auto r = ValiderCroise(jeu, plan.Valeur(), ConfigSvm(), &stop, {});
    if (Refuse(r, "annulation")) VERIFIER(r.Err().m_code == CodeErreur::Annulation);
    PlanPlis casse = plan.Valeur();
    casse.m_plis[0].m_validation.pop_back();      // une ligne n'est plus jamais en validation
    Refuse(ValiderCroise(jeu, casse, ConfigSvm(), nullptr, {}), "plan incoherent");
    ConfigApprentissage mauvaise = ConfigSvm();
    mauvaise.m_normalisation = "zscore";
    Refuse(ValiderCroise(jeu, plan.Valeur(), mauvaise, nullptr, {}), "normalisation inconnue");
}

// ---------- valeurs logarithmiques ----------

TEST(valeurs_logarithmiques_exactes) {
    std::vector<double> v = ValeursLogarithmiques(0.01, 1000.0, 1);
    VERIFIER_EGAL(v.size(), (size_t)6);
    if (v.size() == 6) {
        VERIFIER(v[0] == 0.01); VERIFIER(v[1] == 0.1); VERIFIER(v[2] == 1.0);
        VERIFIER(v[3] == 10.0); VERIFIER(v[4] == 100.0); VERIFIER(v[5] == 1000.0);
    }
    std::vector<double> d = ValeursLogarithmiques(1.0, 100.0, 2);
    VERIFIER_EGAL(d.size(), (size_t)5);
    if (d.size() == 5) { VERIFIER_PROCHE(d[1], std::sqrt(10.0), 1e-12); VERIFIER_PROCHE(d[4], 100.0, 1e-9); }
    VERIFIER_EGAL(ValeursLogarithmiques(5.0, 5.0, 1).size(), (size_t)1);
    VERIFIER(ValeursLogarithmiques(0.0, 10.0, 1).empty());
    VERIFIER(ValeursLogarithmiques(-1.0, 10.0, 1).empty());
    VERIFIER(ValeursLogarithmiques(10.0, 1.0, 1).empty());
    VERIFIER(ValeursLogarithmiques(1.0, 10.0, 0).empty());
}

// ---------- balayage ----------

TEST(balayage_gamma_cinq_candidats_meme_plan) {
    JeuDeDonnees jeu = DeuxNuages(true);
    auto plan = CreerPlis(jeu, Toutes(jeu), 4, ModePlis::Melange, false, 3);
    if (!Reussi(plan, "CreerPlis")) return;
    std::vector<double> valeurs = ValeursLogarithmiques(0.01, 100.0, 1);
    VERIFIER_EGAL(valeurs.size(), (size_t)5);
    auto r = Balayer(jeu, plan.Valeur(), ConfigSvm(), "gamma", valeurs, nullptr, {});
    if (!Reussi(r, "Balayer gamma")) return;
    const RapportBalayage& b = r.Valeur();
    VERIFIER_EGAL(b.m_candidats.size(), (size_t)5);
    VERIFIER(b.m_indiceMeilleur >= 0 && b.m_indiceMeilleur < 5);
    if (b.m_indiceMeilleur < 0 || b.m_indiceMeilleur >= 5) return;
    double meilleure = b.m_candidats[(size_t)b.m_indiceMeilleur].m_validation.m_exactitudeMoyennePlis.m_valeur;
    for (size_t i = 0; i < b.m_candidats.size(); ++i) {
        VERIFIER(MemesPlis(b.m_candidats[i].m_validation.m_plan, plan.Valeur()));
        VERIFIER_PROCHE(b.m_candidats[i].m_valeur, valeurs[i], 0.0);
        VERIFIER_EGAL(b.m_candidats[i].m_validation.m_parPli.size(), (size_t)4);
        VERIFIER(b.m_candidats[i].m_validation.m_exactitudeMoyennePlis.m_valeur <= meilleure + 1e-12);
    }
    VERIFIER_EGAL(b.m_meilleurEnBord, b.m_indiceMeilleur == 0 || b.m_indiceMeilleur == 4);
    auto j = LireJson(b.VersJson().Ecrire());
    if (Reussi(j, "JSON du balayage")) {
        VERIFIER_EGAL(j.Valeur().Membre("candidats").Elements().size(), (size_t)5);
        VERIFIER_EGAL(j.Valeur().Membre("parametre").Texte(), std::string("gamma"));
        VERIFIER_EGAL(j.Valeur().Membre("indice_meilleur").Entier(), (long long)b.m_indiceMeilleur);
    }
}

TEST(balayage_egalite_plus_petite_valeur_et_bord) {
    // Nuages tres separes : toutes les valeurs donnent la meme exactitude (1.0) -> egalite -> plus petite valeur.
    JeuDeDonnees jeu = Nuages({{0.0, 0.0}, {12.0, 12.0}}, 30, 4, true);
    auto plan = CreerPlis(jeu, Toutes(jeu), 5, ModePlis::Melange, false, 2);
    if (!Reussi(plan, "CreerPlis")) return;
    ConfigApprentissage cfg = ConfigSvm();
    cfg.m_normalisation = "minmax";
    auto r = Balayer(jeu, plan.Valeur(), cfg, "gamma", {1.0, 0.1, 10.0}, nullptr, {});     // grille non triee
    if (!Reussi(r, "Balayer")) return;
    const RapportBalayage& b = r.Valeur();
    bool toutesEgales = true;
    for (const auto& c : b.m_candidats) toutesEgales = toutesEgales && c.m_validation.m_exactitudeMoyennePlis.m_valeur == 1.0;
    VERIFIER(toutesEgales);
    VERIFIER_EGAL(b.m_indiceMeilleur, 1);                 // gamma = 0.1, la plus petite valeur
    VERIFIER(!b.m_meilleurEnBord);                        // au milieu de la grille
    auto bord = Balayer(jeu, plan.Valeur(), cfg, "gamma", {0.1, 1.0, 10.0}, nullptr, {});
    if (Reussi(bord, "Balayer grille triee")) {
        VERIFIER_EGAL(bord.Valeur().m_indiceMeilleur, 0);
        VERIFIER(bord.Valeur().m_meilleurEnBord);         // optimum au bord : a signaler
    }
}

TEST(balayage_parametres_par_famille) {
    JeuDeDonnees jeu = DeuxNuages(true);
    auto plan = CreerPlis(jeu, Toutes(jeu), 3, ModePlis::Melange, false, 3);
    if (!Reussi(plan, "CreerPlis")) return;
    Refuse(Balayer(jeu, plan.Valeur(), ConfigSvm(), "nb_arbres", {5, 10}, nullptr, {}), "nb_arbres sur un SVM");
    Refuse(Balayer(jeu, plan.Valeur(), ConfigSvm(), "profondeur_max", {3}, nullptr, {}), "profondeur_max sur un SVM");
    Refuse(Balayer(jeu, plan.Valeur(), ConfigSvm(), "inconnu", {1}, nullptr, {}), "parametre inconnu");
    Refuse(Balayer(jeu, plan.Valeur(), ConfigSvm(), "gamma", {}, nullptr, {}), "aucune valeur");
    VERIFIER(Balayer(jeu, plan.Valeur(), ConfigSvm(), "c", {0.1, 1.0}, nullptr, {}).Ok());
    ConfigApprentissage foret;
    foret.m_algorithme.m_famille = Famille::ForetAleatoire;
    foret.m_graine = 5;
    Refuse(Balayer(jeu, plan.Valeur(), foret, "gamma", {1.0}, nullptr, {}), "gamma sur une foret");
    Refuse(Balayer(jeu, plan.Valeur(), foret, "nb_arbres", {2.5}, nullptr, {}), "nb_arbres non entier");
    auto r = Balayer(jeu, plan.Valeur(), foret, "nb_arbres", {3, 9}, nullptr, {});
    if (Reussi(r, "nb_arbres foret")) VERIFIER_EGAL(r.Valeur().m_candidats.size(), (size_t)2);
    auto p = Balayer(jeu, plan.Valeur(), foret, "profondeur_max", {2, 6}, nullptr, {});
    if (Reussi(p, "profondeur_max foret")) VERIFIER_EGAL(p.Valeur().m_candidats.size(), (size_t)2);
}

// ---------- ROC ----------

TEST(roc_cas_parfait_inverse_et_ties) {
    auto parfait = CalculerRoc({0.9, 0.8, 0.3, 0.1}, {1, 1, 0, 0}, "score de decision SVM");
    if (Reussi(parfait, "parfait")) {
        VERIFIER_PROCHE(parfait.Valeur().m_auc, 1.0, 1e-12);
        VERIFIER_EGAL(parfait.Valeur().m_origineScore, std::string("score de decision SVM"));
        const auto& pts = parfait.Valeur().m_points;
        VERIFIER(pts.front().m_fpr == 0.0 && pts.front().m_tpr == 0.0);
        VERIFIER(pts.back().m_fpr == 1.0 && pts.back().m_tpr == 1.0);
    }
    auto inverse = CalculerRoc({0.1, 0.2, 0.8, 0.9}, {1, 1, 0, 0}, "inverse");
    if (Reussi(inverse, "inverse")) VERIFIER_PROCHE(inverse.Valeur().m_auc, 0.0, 1e-12);
    // main : tri decroissant 0.9(P) 0.7(N) 0.6(P) 0.4(N) -> (0,0)(0,.5)(.5,.5)(.5,1)(1,1) ; AUC = .25 + .5 = .75
    auto aleatoire = CalculerRoc({0.9, 0.7, 0.6, 0.4}, {1, 0, 1, 0}, "construit");
    if (Reussi(aleatoire, "aleatoire")) {
        VERIFIER_PROCHE(aleatoire.Valeur().m_auc, 0.75, 1e-12);
        VERIFIER_EGAL(aleatoire.Valeur().m_points.size(), (size_t)5);
        const auto& p = aleatoire.Valeur().m_points;
        if (p.size() == 5) {
            VERIFIER_PROCHE(p[1].m_fpr, 0.0, 1e-12); VERIFIER_PROCHE(p[1].m_tpr, 0.5, 1e-12);
            VERIFIER_PROCHE(p[2].m_fpr, 0.5, 1e-12); VERIFIER_PROCHE(p[2].m_tpr, 0.5, 1e-12);
            VERIFIER_PROCHE(p[3].m_fpr, 0.5, 1e-12); VERIFIER_PROCHE(p[3].m_tpr, 1.0, 1e-12);
            VERIFIER_PROCHE(p[2].m_seuil, 0.7, 0.0);
        }
    }
    auto egaux = CalculerRoc({0.5, 0.5}, {1, 0}, "scores egaux");     // un seul seuil : diagonale
    if (Reussi(egaux, "scores egaux")) {
        VERIFIER_PROCHE(egaux.Valeur().m_auc, 0.5, 1e-12);
        VERIFIER_EGAL(egaux.Valeur().m_points.size(), (size_t)2);
    }
    auto j = LireJson(aleatoire.Ok() ? aleatoire.Valeur().VersJson().Ecrire() : std::string("{}"));
    if (Reussi(j, "JSON ROC")) VERIFIER_EGAL(j.Valeur().Membre("points").Elements().size(), (size_t)5);
}

TEST(roc_refuse_les_entrees_invalides) {
    Refuse(CalculerRoc({0.1, 0.2, 0.3}, {1, 1, 1}, "x"), "une seule classe (positifs)");
    Refuse(CalculerRoc({0.1, 0.2, 0.3}, {0, 0, 0}, "x"), "une seule classe (negatifs)");
    Refuse(CalculerRoc({0.1, 0.2}, {1}, "x"), "tailles differentes");
    Refuse(CalculerRoc({}, {}, "x"), "vide");
    Refuse(CalculerRoc({0.1, std::numeric_limits<double>::quiet_NaN()}, {1, 0}, "x"), "score NaN");
}

// ---------- points pedagogiques FNR / 100 - FPR ----------

TEST(points_fnr_fpr_valeurs_a_la_main) {
    auto candidat = [](double valeur, std::vector<long long> comptes) {
        ResultatCandidat c;
        c.m_valeur = valeur;
        Confusion& k = c.m_validation.m_horsPli.m_confusion;
        k.m_nbClasses = 2;
        k.m_comptes = comptes;
        k.m_total = comptes[0] + comptes[1] + comptes[2] + comptes[3];
        return c;
    };
    RapportBalayage b;
    // reel 0 : 40 predits 0, 10 predits 1 ; reel 1 : 5 predits 0, 45 predits 1
    b.m_candidats.push_back(candidat(0.1, {40, 10, 5, 45}));
    b.m_candidats.push_back(candidat(1.0, {50, 0, 0, 0}));        // aucun reel de classe 1 : denominateur nul
    b.m_candidats.push_back(candidat(10.0, {30, 20, 10, 40}));
    auto p1 = PointsFnrFpr(b, 1);
    VERIFIER_EGAL(p1.size(), (size_t)2);                // le candidat sans classe positive est ignore
    if (p1.size() == 2) {
        // classe 1 positive : TP = 45, FN = 5, FP = 10, TN = 40 -> FNR 10 %, FPR 20 % -> 80
        VERIFIER_PROCHE(p1[0].m_valeurParametre, 0.1, 0.0);
        VERIFIER_PROCHE(p1[0].m_fnrPourcent, 10.0, 1e-12);
        VERIFIER_PROCHE(p1[0].m_cent_moins_fprPourcent, 80.0, 1e-12);
        // TP = 40, FN = 10, FP = 20, TN = 30 -> FNR 20 %, FPR 40 % -> 60
        VERIFIER_PROCHE(p1[1].m_valeurParametre, 10.0, 0.0);
        VERIFIER_PROCHE(p1[1].m_fnrPourcent, 20.0, 1e-12);
        VERIFIER_PROCHE(p1[1].m_cent_moins_fprPourcent, 60.0, 1e-12);
    }
    auto p0 = PointsFnrFpr(b, 0);
    VERIFIER_EGAL(p0.size(), (size_t)2);                // classe 0 positive : FP + TN = 0 pour le candidat 1.0
    if (p0.size() == 2) {
        // TP = 40, FN = 10, FP = 5, TN = 45 -> FNR 20 %, FPR 10 % -> 90
        VERIFIER_PROCHE(p0[0].m_fnrPourcent, 20.0, 1e-12);
        VERIFIER_PROCHE(p0[0].m_cent_moins_fprPourcent, 90.0, 1e-12);
    }
    VERIFIER(PointsFnrFpr(b, 2).empty());               // classe inexistante
}

int main(int argc, char** argv) { return LancerTests(argc, argv); }
