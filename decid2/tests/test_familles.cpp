// Tests des familles L5 : KPPV, Bayes normal, MLP, EM, Boost un-contre-tous (ACC-07 : sauvegarde puis
// rechargement => predictions identiques), plus branchement dans l'experience et les operations pas a pas.
// Donnees SYNTHETIQUES deterministes (LCG maison) ; fichiers dans verif::DossierTemporaire.
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include "Experience.h"
#include "Fichiers.h"
#include "Modeles.h"
#include "Operations.h"
#include "Trace.h"
#include "Sha256.h"
#include "Verifier.h"

using namespace decid;

namespace {

struct Lcg {
    std::uint64_t m_etat;
    double Suivant() {
        m_etat = m_etat * 6364136223846793005ULL + 1442695040888963407ULL;
        return (double)(m_etat >> 11) / 9007199254740992.0;
    }
    double Bruit() { return Suivant() + Suivant() + Suivant() - 1.5; }   // ~ cloche, ecart-type 0.5
};

struct Corpus {
    cv::Mat m_x;                 // CV_32F
    std::vector<int> m_labels;
    int m_nbClasses = 0;
};

// Nuages 2D : centres donnes, n points par classe (effectifs pouvant varier), bruit = echelle * Bruit()
Corpus Nuages(const std::vector<std::pair<double, double>>& centres, const std::vector<int>& effectifs,
              std::uint64_t graine, double echelle = 1.0) {
    Corpus c;
    c.m_nbClasses = (int)centres.size();
    int total = 0;
    for (int n : effectifs) total += n;
    c.m_x = cv::Mat(total, 2, CV_32F);
    Lcg g{graine};
    int ligne = 0;
    for (int k = 0; k < c.m_nbClasses; ++k)
        for (int i = 0; i < effectifs[(size_t)k]; ++i, ++ligne) {
            c.m_x.at<float>(ligne, 0) = (float)(centres[(size_t)k].first + echelle * g.Bruit());
            c.m_x.at<float>(ligne, 1) = (float)(centres[(size_t)k].second + echelle * g.Bruit());
            c.m_labels.push_back(k);
        }
    return c;
}

Corpus Trois(std::uint64_t graine, int n = 40) {
    return Nuages({{0.0, 0.0}, {5.0, 0.0}, {2.5, 5.0}}, {n, n, n}, graine);
}
Corpus Quatre(std::uint64_t graine) {
    return Nuages({{0.0, 0.0}, {6.0, 0.0}, {0.0, 6.0}, {6.0, 6.0}}, {30, 30, 30, 30}, graine, 0.8);
}

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

double Exactitude(const std::vector<int>& predites, const std::vector<int>& vraies) {
    int bons = 0;
    for (size_t i = 0; i < vraies.size(); ++i) bons += predites[i] == vraies[i] ? 1 : 0;
    return (double)bons / (double)vraies.size();
}

ParametresAlgorithme Algo(Famille f) {
    ParametresAlgorithme p;
    p.m_famille = f;
    return p;
}

Resultat<Modele> Apprendre(const Corpus& c, const ParametresAlgorithme& p, std::uint64_t graine = 1) {
    return Entrainer(c.m_x, c.m_labels, c.m_nbClasses, p, graine);
}

// Exactitude sur un jeu de test independant (autre graine)
double ExactiteTest(const Modele& m, const Corpus& test) {
    auto pred = Predire(m, test.m_x);
    if (!Reussi(pred, "Predire (test)")) return 0.0;
    return Exactitude(pred.Valeur(), test.m_labels);
}

// ACC-07 : sauvegarde, rechargement, predictions identiques ; renvoie le modele recharge
bool Recharger(const Modele& m, const Corpus& c, const std::string& nom, Modele& recharge) {
    auto d = verif::DossierTemporaire(nom);
    if (!Reussi(SauverModele(m, d / "modele.yml"), "SauverModele")) return false;
    auto r = ChargerModele(m.m_famille, d / "modele.yml", c.m_nbClasses);
    if (!Reussi(r, "ChargerModele")) return false;
    recharge = r.Valeur();
    auto avant = Predire(m, c.m_x), apres = Predire(recharge, c.m_x);
    if (!Reussi(avant, "Predire avant") || !Reussi(apres, "Predire apres")) return false;
    VERIFIER(avant.Valeur() == apres.Valeur());
    VERIFIER_EGAL(recharge.m_nbAttributs, m.m_nbAttributs);
    return true;
}

bool InspectionNonVide(const Modele& m) {
    ValeurJson j = InspecterModele(m);
    VERIFIER_EGAL(j.Membre("famille").Texte(), std::string(NomFamille(m.m_famille)));
    VERIFIER(j.Contient("parametres_effectifs") && !j.Membre("parametres_effectifs").Membres().empty());
    VERIFIER(j.Ecrire(false).size() > 200);
    return true;
}

} // namespace

// ---------- parametres JSON ----------

TEST(parametres_json_aller_retour_pour_toutes_les_familles) {
    const char* blocs[] = {
        "{\"famille\":\"svm\",\"noyau\":\"RBF\",\"c\":2,\"gamma\":0.5}",
        "{\"famille\":\"foret_aleatoire\",\"nb_arbres\":7}",
        "{\"famille\":\"kppv\",\"k\":9}",
        "{\"famille\":\"bayes_normal\"}",
        "{\"famille\":\"mlp\",\"couches_cachees\":[6,4],\"activation\":\"RELU\",\"methode\":\"RPROP\",\"iterations_max\":50}",
        "{\"famille\":\"em\",\"k\":4,\"covariance\":\"GENERIQUE\",\"iterations_max\":20,\"epsilon\":0.001}",
        "{\"famille\":\"boost\",\"type\":\"REEL\",\"nb_faibles\":12,\"profondeur\":2}"};
    for (const char* texte : blocs) {
        auto j = LireJson(texte);
        if (!Reussi(j, texte)) continue;
        auto p = ParametresDepuisJson(j.Valeur());
        if (!Reussi(p, texte)) continue;
        ValeurJson retour = p.Valeur().VersJson();
        auto p2 = ParametresDepuisJson(retour);
        if (Reussi(p2, "relecture")) VERIFIER_EGAL(p2.Valeur().VersJson().Ecrire(false), retour.Ecrire(false));
        VERIFIER_EGAL(retour.Membre("famille").Texte(), j.Valeur().Membre("famille").Texte());
    }
    auto em = ParametresDepuisJson(LireJson(blocs[5]).Valeur());
    if (Reussi(em, "em")) { VERIFIER_EGAL(em.Valeur().m_em.m_k, 4); VERIFIER_EGAL(em.Valeur().m_em.m_covariance, std::string("GENERIQUE")); }
    auto mlp = ParametresDepuisJson(LireJson(blocs[4]).Valeur());
    if (Reussi(mlp, "mlp")) VERIFIER_EGAL(mlp.Valeur().m_mlp.m_couchesCachees.size(), (size_t)2);
}

TEST(parametres_json_refus_explicites) {
    Refuse(ParametresDepuisJson(LireJson("{\"famille\":\"inconnue\"}").Valeur()), "famille inconnue");
    Refuse(ParametresDepuisJson(LireJson("{\"famille\":\"kppv\",\"k\":\"trois\"}").Valeur()), "k texte");
    Refuse(ParametresDepuisJson(LireJson("{\"famille\":\"kppv\",\"k\":2.5}").Valeur()), "k non entier");
    Refuse(ParametresDepuisJson(LireJson("{\"famille\":\"mlp\",\"couches_cachees\":[4,\"a\"]}").Valeur()), "couche texte");
    Refuse(ParametresDepuisJson(LireJson("{\"famille\":\"em\",\"covariance\":3}").Valeur()), "covariance nombre");
    Famille f;
    VERIFIER(FamilleDepuisNom("rf", f) && f == Famille::ForetAleatoire);
    VERIFIER(FamilleDepuisNom("bayes_normal", f) && f == Famille::BayesNormal);
    VERIFIER(!FamilleDepuisNom("naif", f));
    for (Famille fam : {Famille::Svm, Famille::ForetAleatoire, Famille::Kppv, Famille::BayesNormal, Famille::Mlp, Famille::Em,
                        Famille::Boost}) {
        Famille retour;
        VERIFIER(FamilleDepuisNom(NomFamille(fam), retour) && retour == fam);
    }
}

// ---------- KPPV ----------

TEST(kppv_apprentissage_rechargement_et_parametres_effectifs) {
    Corpus app = Trois(3), test = Trois(99, 20);
    ParametresAlgorithme p = Algo(Famille::Kppv);
    p.m_kppv.m_k = 5;
    auto m = Apprendre(app, p);
    if (!Reussi(m, "kppv")) return;
    VERIFIER(ExactiteTest(m.Valeur(), test) > 0.95);
    VERIFIER_EGAL(m.Valeur().m_parametresEffectifs.Membre("k").Entier(), 5LL);
    VERIFIER_EGAL(m.Valeur().m_parametresEffectifs.Membre("nb_echantillons").Entier(), 120LL);
    InspectionNonVide(m.Valeur());
    VERIFIER_EGAL(InspecterModele(m.Valeur()).Membre("k").Entier(), 5LL);
    Modele relu;
    if (!Recharger(m.Valeur(), test, "familles_kppv", relu)) return;
    VERIFIER_EGAL(relu.m_parametresEffectifs.Membre("k").Entier(), 5LL);
    VERIFIER_EGAL(relu.m_parametresEffectifs.Membre("nb_echantillons").Entier(), 120LL);
}

TEST(kppv_k_hors_bornes_refuse_sans_rabattement) {
    Corpus app = Trois(3, 10);
    ParametresAlgorithme p = Algo(Famille::Kppv);
    p.m_kppv.m_k = 0;
    Refuse(Apprendre(app, p), "k = 0");
    p.m_kppv.m_k = 31;     // N = 30 : OpenCV rabattrait en silence
    Refuse(Apprendre(app, p), "k > N");
    p.m_kppv.m_k = 30;
    VERIFIER(Apprendre(app, p).Ok());
    p.m_kppv.m_k = 1;
    VERIFIER(Apprendre(app, p).Ok());
}

TEST(kppv_voisins_coherents_avec_findNearest) {
    Corpus app = Trois(5), test = Trois(77, 8);
    ParametresAlgorithme p = Algo(Famille::Kppv);
    p.m_kppv.m_k = 7;
    auto m = Apprendre(app, p);
    if (!Reussi(m, "kppv")) return;
    auto pred = Predire(m.Valeur(), test.m_x);
    if (!Reussi(pred, "Predire")) return;
    auto knn = m.Valeur().m_opencv.dynamicCast<cv::ml::KNearest>();
    for (int i = 0; i < test.m_x.rows; ++i) {
        auto v = VoisinsKppvPour(m.Valeur(), test.m_x.ptr<float>(i));
        if (!Reussi(v, "VoisinsKppvPour")) return;
        const VoisinsKppv& r = v.Valeur();
        VERIFIER_EGAL(r.m_indices.size(), (size_t)7);
        std::set<int> uniques(r.m_indices.begin(), r.m_indices.end());
        VERIFIER_EGAL(uniques.size(), (size_t)7);
        int somme = 0;
        for (int c : r.m_vote) somme += c;
        VERIFIER_EGAL(somme, 7);
        for (size_t j = 0; j < r.m_indices.size(); ++j) {
            VERIFIER_EGAL(r.m_classes[j], app.m_labels[(size_t)r.m_indices[j]]);                     // classe de l'echantillon embarque
            if (j > 0) VERIFIER(r.m_distances[j] >= r.m_distances[j - 1]);
            double d2 = 0;
            for (int a = 0; a < 2; ++a) {
                double e = (double)test.m_x.at<float>(i, a) - (double)app.m_x.at<float>(r.m_indices[j], a);
                d2 += e * e;
            }
            VERIFIER_PROCHE(std::sqrt(d2), r.m_distances[j], 1e-4);
        }
        VERIFIER_EGAL(r.m_decision, pred.Valeur()[(size_t)i]);
        cv::Mat res, nr, dist;
        knn->findNearest(test.m_x.row(i), 7, res, nr, dist);
        VERIFIER_EGAL((int)std::lround(res.at<float>(0)), r.m_decision);
        for (size_t j = 0; j < r.m_indices.size(); ++j)
            VERIFIER_PROCHE(dist.at<float>(0, (int)j), r.m_distances[j] * r.m_distances[j], 1e-3);
    }
}

TEST(kppv_regles_d_egalite_plus_petit_indice_et_plus_petit_id) {
    // Deux voisins a distance egale (indices 0 et 1) de classes 1 et 0 : vote 1-1 => classe 0 (plus petit id)
    cv::Mat x = (cv::Mat_<float>(4, 2) << 0, 0, 1, 0, 10, 10, 11, 10);
    std::vector<int> labels{1, 0, 1, 0};
    ParametresKppv p;
    p.m_k = 2;
    auto m = EntrainerKppv(x, labels, 2, p);
    if (!Reussi(m, "kppv egalite")) return;
    float requete[2] = {0.5f, 0.0f};
    auto v = VoisinsKppvPour(m.Valeur(), requete);
    if (!Reussi(v, "voisins")) return;
    VERIFIER_EGAL(v.Valeur().m_indices.size(), (size_t)2);
    VERIFIER_EGAL(v.Valeur().m_indices[0], 0);
    VERIFIER_EGAL(v.Valeur().m_indices[1], 1);
    VERIFIER_EGAL(v.Valeur().m_vote[0], 1);
    VERIFIER_EGAL(v.Valeur().m_vote[1], 1);
    VERIFIER_EGAL(v.Valeur().m_decision, 0);
    cv::Mat q(1, 2, CV_32F, requete);
    auto pred = Predire(m.Valeur(), q);
    if (Reussi(pred, "Predire egalite")) VERIFIER_EGAL(pred.Valeur()[0], 0);
    // Trois voisins a distance egale : l'indice le plus petit est retenu quand k < nombre d'egalites
    cv::Mat x3 = (cv::Mat_<float>(5, 1) << 1, -1, 1, 50, 60);
    std::vector<int> l3{0, 1, 0, 1, 1};
    p.m_k = 1;
    auto m3 = EntrainerKppv(x3, l3, 2, p);
    if (!Reussi(m3, "kppv egalite distance")) return;
    float zero[1] = {0.0f};
    auto v3 = VoisinsKppvPour(m3.Valeur(), zero);
    if (Reussi(v3, "voisins 3")) VERIFIER_EGAL(v3.Valeur().m_indices[0], 0);
}

// ---------- Bayes normal ----------

namespace {

double LogVraisemblance(const ValeurJson& classe, const float* x, int p) {
    cv::Mat mu(p, 1, CV_64F), s(p, p, CV_64F), xv(p, 1, CV_64F);
    for (int a = 0; a < p; ++a) {
        mu.at<double>(a) = classe.Membre("moyenne").Elements()[(size_t)a].Nombre();
        xv.at<double>(a) = x[a];
        for (int b = 0; b < p; ++b)
            s.at<double>(a, b) = classe.Membre("covariance").Elements()[(size_t)a].Elements()[(size_t)b].Nombre();
    }
    cv::Mat inverse;
    cv::invert(s, inverse, cv::DECOMP_SVD);
    cv::Mat d = xv - mu;
    return -0.5 * (std::log(cv::determinant(s)) + d.dot(inverse * d));
}

} // namespace

TEST(bayes_decision_opencv_egale_argmax_des_log_vraisemblances_recalculees) {
    // Classes tres desequilibrees (120 contre 15) et chevauchantes : sans a priori, la decision ne depend pas des effectifs
    Corpus app = Nuages({{0.0, 0.0}, {1.2, 1.0}}, {120, 15}, 21);
    auto m = Apprendre(app, Algo(Famille::BayesNormal));
    if (!Reussi(m, "bayes")) return;
    const ValeurJson recalcul = InspecterModele(m.Valeur()).Membre("recalcul_decid");
    VERIFIER_EGAL(recalcul.Membre("classes").Elements().size(), (size_t)2);
    VERIFIER_EGAL(recalcul.Membre("source").Texte(), std::string("recalcul sur les donnees d'apprentissage"));
    // moyennes recalculees = moyennes reelles de chaque classe
    for (int c = 0; c < 2; ++c) {
        double somme[2] = {0, 0};
        int n = 0;
        for (int i = 0; i < app.m_x.rows; ++i)
            if (app.m_labels[(size_t)i] == c) { somme[0] += app.m_x.at<float>(i, 0); somme[1] += app.m_x.at<float>(i, 1); ++n; }
        const ValeurJson& cl = recalcul.Membre("classes").Elements()[(size_t)c];
        VERIFIER_EGAL((int)cl.Membre("effectif").Entier(), n);
        VERIFIER_PROCHE(cl.Membre("moyenne").Elements()[0].Nombre(), somme[0] / n, 1e-9);
        VERIFIER_PROCHE(cl.Membre("moyenne").Elements()[1].Nombre(), somme[1] / n, 1e-9);
    }
    Corpus extra = Nuages({{0.0, 0.0}, {1.2, 1.0}}, {40, 40}, 55);
    int desaccords = 0, differentsDeLaFrequence = 0;
    for (const Corpus* c : {&app, &extra}) {
        auto pred = Predire(m.Valeur(), c->m_x);
        if (!Reussi(pred, "Predire")) return;
        for (int i = 0; i < c->m_x.rows; ++i) {
            const float* x = c->m_x.ptr<float>(i);
            double l0 = LogVraisemblance(recalcul.Membre("classes").Elements()[0], x, 2);
            double l1 = LogVraisemblance(recalcul.Membre("classes").Elements()[1], x, 2);
            int attendu = l1 > l0 ? 1 : 0;
            if (std::fabs(l1 - l0) > 1e-6 && attendu != pred.Valeur()[(size_t)i]) ++desaccords;
            // avec a priori = frequences (120/135 contre 15/135), la decision serait plus souvent 0
            double lp0 = l0 + std::log(120.0 / 135.0), lp1 = l1 + std::log(15.0 / 135.0);
            if ((lp1 > lp0 ? 1 : 0) != attendu) ++differentsDeLaFrequence;
        }
    }
    VERIFIER_EGAL(desaccords, 0);
    VERIFIER(differentsDeLaFrequence > 0);    // preuve que l'a priori de frequence changerait des decisions : OpenCV n'en met pas
}

TEST(bayes_apprentissage_rechargement_inspection) {
    Corpus app = Trois(8), test = Trois(31, 20);
    auto m = Apprendre(app, Algo(Famille::BayesNormal));
    if (!Reussi(m, "bayes")) return;
    VERIFIER(ExactiteTest(m.Valeur(), test) > 0.95);
    VERIFIER_EGAL(m.Valeur().m_parametresEffectifs.Membre("nb_classes_apprises").Entier(), 3LL);
    InspectionNonVide(m.Valeur());
    Modele relu;
    if (!Recharger(m.Valeur(), test, "familles_bayes", relu)) return;
    // apres rechargement, l'inspection reconstruit les memes moyennes depuis l'etat OpenCV du fichier
    const ValeurJson avant = InspecterModele(m.Valeur()).Membre("recalcul_decid"), apres = InspecterModele(relu).Membre("recalcul_decid");
    VERIFIER_EGAL(apres.Membre("classes").Elements().size(), (size_t)3);
    for (size_t c = 0; c < 3 && c < apres.Membre("classes").Elements().size(); ++c)
        for (size_t a = 0; a < 2; ++a)
            VERIFIER_PROCHE(avant.Membre("classes").Elements()[c].Membre("moyenne").Elements()[a].Nombre(),
                            apres.Membre("classes").Elements()[c].Membre("moyenne").Elements()[a].Nombre(), 1e-9);
}

// ---------- MLP ----------

namespace {

ParametresMlp MlpRapide() {
    ParametresMlp p;
    p.m_couchesCachees = {8};
    p.m_methode = "RETROPROPAGATION";
    p.m_eta = 0.1;
    p.m_moment = 0.1;
    p.m_iterationsMax = 300;
    p.m_epsilon = 1e-6;
    return p;
}

} // namespace

TEST(mlp_retropropagation_rechargement_et_parametres_effectifs) {
    Corpus app = Trois(4), test = Trois(88, 20);
    ParametresAlgorithme p = Algo(Famille::Mlp);
    p.m_mlp = MlpRapide();
    auto m = Apprendre(app, p);
    if (!Reussi(m, "mlp")) return;
    VERIFIER(ExactiteTest(m.Valeur(), test) > 0.9);
    const ValeurJson& e = m.Valeur().m_parametresEffectifs;
    VERIFIER_EGAL(e.Membre("couches").Elements().size(), (size_t)3);
    VERIFIER_EGAL(e.Membre("couches").Elements()[0].Entier(), 2LL);
    VERIFIER_EGAL(e.Membre("couches").Elements()[1].Entier(), 8LL);
    VERIFIER_EGAL(e.Membre("couches").Elements()[2].Entier(), 3LL);
    VERIFIER_EGAL(e.Membre("activation").Texte(), std::string("SIGMOIDE_SYM"));
    VERIFIER_EGAL(e.Membre("methode").Texte(), std::string("RETROPROPAGATION"));
    VERIFIER_PROCHE(e.Membre("eta").Nombre(), 0.1, 1e-12);
    VERIFIER_EGAL(e.Membre("critere_arret").Membre("max_iterations").Entier(), 300LL);
    VERIFIER_EGAL(e.Membre("iterations_demandees").Entier(), 300LL);
    VERIFIER(e.Membre("iterations_effectuees").EstNul());       // non exposee par OpenCV : enregistree a part, jamais confondue
    InspectionNonVide(m.Valeur());
    VERIFIER_EGAL(InspecterModele(m.Valeur()).Membre("couches").Elements().size(), (size_t)2);
    Modele relu;
    if (!Recharger(m.Valeur(), test, "familles_mlp", relu)) return;
    VERIFIER_EGAL(relu.m_parametresEffectifs.Membre("couches").Elements().size(), (size_t)3);
    VERIFIER_EGAL(relu.m_parametresEffectifs.Membre("critere_arret").Membre("max_iterations").Entier(), 300LL);
}

TEST(mlp_rprop_et_activations) {
    Corpus app = Trois(4), test = Trois(88, 20);
    for (const char* activation : {"SIGMOIDE_SYM", "RELU", "IDENTITE"}) {
        ParametresAlgorithme p = Algo(Famille::Mlp);
        p.m_mlp = MlpRapide();
        p.m_mlp.m_methode = "RPROP";
        p.m_mlp.m_activation = activation;
        p.m_mlp.m_iterationsMax = 100;
        auto m = Apprendre(app, p);
        if (!Reussi(m, activation)) continue;
        VERIFIER_EGAL(m.Valeur().m_parametresEffectifs.Membre("activation").Texte(), std::string(activation));
        VERIFIER_EGAL(m.Valeur().m_parametresEffectifs.Membre("methode").Texte(), std::string("RPROP"));
        double acc = ExactiteTest(m.Valeur(), test);
        std::printf("  %s (RPROP) : exactitude test = %.3f\n", activation, acc);
        // RELU est aussi appliquee a la couche de sortie par OpenCV : des sorties peuvent rester mortes (0) et des
        // classes n'etre jamais predites ; seules les activations bornees ou lineaires sont exigees precises.
        if (std::string(activation) != "RELU" && !(acc > 0.85))
            verif::Echec(__FILE__, __LINE__, std::string(activation) + " : exactitude trop faible " + std::to_string(acc));
        Modele relu;
        Recharger(m.Valeur(), test, std::string("familles_mlp_") + activation, relu);
    }
}

TEST(mlp_parametres_hors_bornes_refuses) {
    Corpus app = Trois(4, 10);
    ParametresAlgorithme p = Algo(Famille::Mlp);
    p.m_mlp = MlpRapide();
    p.m_mlp.m_eta = 5.0;
    Refuse(Apprendre(app, p), "eta > 1");
    p.m_mlp = MlpRapide();
    p.m_mlp.m_couchesCachees = {1};
    Refuse(Apprendre(app, p), "couche cachee d'un neurone");
    p.m_mlp = MlpRapide();
    p.m_mlp.m_activation = "TANH";
    Refuse(Apprendre(app, p), "activation inconnue");
    p.m_mlp = MlpRapide();
    p.m_mlp.m_iterationsMax = 0;
    Refuse(Apprendre(app, p), "0 iteration");
}

TEST(mlp_pas_a_pas_un_etat_par_epoque_poids_changent_perte_baisse) {
    Corpus app = Trois(4), val = Trois(61, 15);
    ParametresMlp p = MlpRapide();
    p.m_iterationsMax = 40;
    p.m_epsilon = 1e-12;
    CollecteurTrace trace(1000, 64u << 20);
    auto m = EntrainerMlpPasAPas(app.m_x, app.m_labels, 3, p, val.m_x, val.m_labels, 1, &trace);
    if (!Reussi(m, "mlp pas a pas")) return;
    trace.Terminer();
    const auto& etats = trace.Etats();
    VERIFIER_EGAL(etats.size(), (size_t)40);                                  // nombre d'etats = epoques
    VERIFIER_EGAL(m.Valeur().m_parametresEffectifs.Membre("iterations_effectuees").Entier(), 40LL);
    if (etats.size() != 40) return;
    VERIFIER(!etats.front().m_vecteur.empty());
    VERIFIER_EGAL(etats.front().m_vecteur.size(), etats.back().m_vecteur.size());
    double ecart = 0;
    for (size_t i = 0; i < etats.front().m_vecteur.size(); ++i)
        ecart += std::fabs(etats.back().m_vecteur[i] - etats.front().m_vecteur[i]);
    VERIFIER(ecart > 1e-3);                                                   // les poids ont vraiment change
    auto grandeur = [](const EtatTrace& e, const char* nom) {
        for (const auto& g : e.m_grandeurs) if (g.first == nom) return g.second;
        return std::nan("");
    };
    double debut = 0, fin = 0;
    for (size_t i = 0; i < 3; ++i) { debut += grandeur(etats[i], "perte_apprentissage") / 3; fin += grandeur(etats[37 + i], "perte_apprentissage") / 3; }
    VERIFIER(fin < debut);                                                    // la perte baisse globalement
    VERIFIER(std::isfinite(grandeur(etats.back(), "exactitude_validation")));
    VERIFIER(grandeur(etats.back(), "exactitude_apprentissage") > 0.8);
    for (size_t i = 0; i < etats.size(); ++i) VERIFIER_EGAL((int)grandeur(etats[i], "epoque"), (int)i + 1);
    // modele final utilisable, sauvegardable
    Corpus test = Trois(88, 20);
    VERIFIER(ExactiteTest(m.Valeur(), test) > 0.85);
    Modele relu;
    Recharger(m.Valeur(), test, "familles_mlp_pas_a_pas", relu);
    // sans validation : la grandeur n'est pas emise
    CollecteurTrace sans(1000, 64u << 20);
    p.m_iterationsMax = 5;
    auto m2 = EntrainerMlpPasAPas(app.m_x, app.m_labels, 3, p, cv::Mat(), {}, 1, &sans);
    if (Reussi(m2, "mlp sans validation")) {
        VERIFIER_EGAL(sans.Etats().size(), (size_t)5);
        VERIFIER(std::isnan(grandeur(sans.Etats().back(), "exactitude_validation")));
    }
}

// ---------- EM ----------

TEST(em_k_refuses_hors_de_2_a_min_20_n_jamais_rabattus) {
    Corpus app = Quatre(6);
    ParametresAlgorithme p = Algo(Famille::Em);
    for (int k : {0, 1, 21, 100}) {
        p.m_em.m_k = k;
        auto r = Apprendre(app, p);
        if (Refuse(r, ("K = " + std::to_string(k)).c_str()))
            VERIFIER(r.Err().m_message.find("K = " + std::to_string(k)) != std::string::npos);
    }
    // K > N : jeu de 6 lignes
    Corpus petit = Nuages({{0.0, 0.0}, {5.0, 5.0}}, {3, 3}, 2);
    p.m_em.m_k = 7;
    auto r = Apprendre(petit, p);
    if (Refuse(r, "K > N")) VERIFIER(r.Err().m_message.find("min(20, N) = 6") != std::string::npos);
    p.m_em.m_k = 2;
    VERIFIER(Apprendre(petit, p).Ok());
}

TEST(em_k4_accepte_relu_apres_apprentissage_et_rechargement) {
    Corpus app = Quatre(6), test = Quatre(41);
    ParametresAlgorithme p = Algo(Famille::Em);
    p.m_em.m_k = 4;
    p.m_em.m_covariance = "DIAGONALE";
    auto m = Apprendre(app, p, 3);
    if (!Reussi(m, "em K=4")) return;
    auto em = m.Valeur().m_opencv.dynamicCast<cv::ml::EM>();
    VERIFIER_EGAL(em->getClustersNumber(), 4);                                // K relu sur le modele
    VERIFIER_EGAL(m.Valeur().m_parametresEffectifs.Membre("k").Entier(), 4LL);
    VERIFIER_EGAL(m.Valeur().m_correspondanceClusters.size(), (size_t)4);
    VERIFIER(ExactiteTest(m.Valeur(), test) > 0.95);
    InspectionNonVide(m.Valeur());
    VERIFIER_EGAL(InspecterModele(m.Valeur()).Membre("moyennes").Elements().size(), (size_t)4);
    Modele relu;
    if (!Recharger(m.Valeur(), test, "familles_em", relu)) return;
    VERIFIER_EGAL(relu.m_opencv.dynamicCast<cv::ml::EM>()->getClustersNumber(), 4);
    VERIFIER(relu.m_correspondanceClusters == m.Valeur().m_correspondanceClusters);
    VERIFIER_EGAL(relu.m_parametresEffectifs.Membre("k").Entier(), 4LL);
    for (const char* cov : {"SPHERIQUE", "GENERIQUE"}) {
        p.m_em.m_covariance = cov;
        auto autre = Apprendre(app, p, 3);
        if (Reussi(autre, cov)) VERIFIER_EGAL(autre.Valeur().m_parametresEffectifs.Membre("covariance").Texte(), std::string(cov));
    }
}

TEST(em_cluster_non_associe_donne_une_erreur_avec_le_nombre_de_lignes) {
    Corpus app = Quatre(6);
    ParametresAlgorithme p = Algo(Famille::Em);
    p.m_em.m_k = 4;
    auto m = Apprendre(app, p, 3);
    if (!Reussi(m, "em")) return;
    Modele casse = m.Valeur();
    auto clusters = ClustersEm(casse, app.m_x);
    if (!Reussi(clusters, "ClustersEm")) return;
    const int cible = clusters.Valeur()[0];
    long attendues = 0;
    for (int k : clusters.Valeur()) attendues += k == cible ? 1 : 0;
    casse.m_correspondanceClusters[(size_t)cible] = -1;
    auto r = Predire(casse, app.m_x);
    if (Refuse(r, "cluster non associe")) {
        VERIFIER(r.Err().m_message.find(std::to_string(attendues) + " ligne(s)") != std::string::npos);
        VERIFIER(r.Err().m_message.find("non associe") != std::string::npos);
    }
}

TEST(em_correspondance_majoritaire_egalite_plus_petit_id_cluster_vide_moins_un) {
    // cluster 0 : classes 2,2,1 -> 2 ; cluster 1 : classes 1,0 (egalite) -> 0 ; cluster 2 : aucune ligne -> -1
    std::vector<int> clusters{0, 0, 0, 1, 1};
    std::vector<int> labels{2, 2, 1, 1, 0};
    std::vector<int> c = CorrespondanceClusters(clusters, labels, 3, 3);
    VERIFIER_EGAL(c.size(), (size_t)3);
    VERIFIER_EGAL(c[0], 2);
    VERIFIER_EGAL(c[1], 0);
    VERIFIER_EGAL(c[2], -1);
}

TEST(em_pas_a_pas_vraisemblance_non_decroissante_et_un_etat_par_iteration) {
    Corpus app = Quatre(6);
    ParametresEm p;
    p.m_k = 4;
    p.m_covariance = "DIAGONALE";
    p.m_iterationsMax = 30;
    p.m_epsilon = 1e-9;
    CollecteurTrace trace(1000, 64u << 20);
    auto m = EntrainerEmPasAPas(app.m_x, app.m_labels, 4, p, 3, &trace);
    if (!Reussi(m, "em pas a pas")) return;
    trace.Terminer();
    const auto& etats = trace.Etats();
    const long long effectuees = m.Valeur().m_parametresEffectifs.Membre("iterations_effectuees").Entier();
    VERIFIER(effectuees >= 2 && effectuees <= 30);
    VERIFIER_EGAL((long long)etats.size(), effectuees);
    double precedente = -1e300;
    for (const EtatTrace& e : etats) {
        double l = e.m_grandeurs[1].second;
        VERIFIER(l >= precedente - 1e-6 * std::fabs(precedente) - 1e-9);      // non decroissante (tolerance numerique)
        precedente = l;
        VERIFIER_EGAL(e.m_vecteur.size(), (size_t)(4 * 2 + 4 * 2 + 4));       // moyennes, covariances diagonales, poids
    }
    VERIFIER(etats.back().m_grandeurs[1].second > etats.front().m_grandeurs[1].second);
    VERIFIER_EGAL(m.Valeur().m_opencv.dynamicCast<cv::ml::EM>()->getClustersNumber(), 4);
    VERIFIER(ExactiteTest(m.Valeur(), Quatre(41)) > 0.95);
    Refuse(EntrainerEmPasAPas(app.m_x, app.m_labels, 4, ParametresEm{21, "DIAGONALE", 10, 1e-6}, 1, nullptr), "K = 21 pas a pas");
}

// ---------- Boost un-contre-tous ----------

TEST(boost_signe_du_score_qualifie_sur_toutes_les_lignes_pour_chaque_type) {
    Corpus app = Trois(9), test = Trois(23, 20);
    for (const char* type : {"DISCRETE", "REEL", "LOGIT", "GENTLE"}) {
        ParametresAlgorithme p = Algo(Famille::Boost);
        p.m_boost.m_type = type;
        p.m_boost.m_nbFaibles = 30;
        auto m = Apprendre(app, p);
        if (!Reussi(m, type)) continue;
        VERIFIER(ExactiteTest(m.Valeur(), test) > 0.9);
        for (const Corpus* c : {&app, &test}) {
            auto scores = ScoresBoost(m.Valeur(), c->m_x);
            if (!Reussi(scores, type)) continue;
            long incoherences = 0, brutes = 0;
            for (int cl = 0; cl < 3; ++cl) {
                cv::Mat etiquette, indice;
                m.Valeur().m_sousModeles[(size_t)cl]->predict(c->m_x, etiquette);                          // sans RAW_OUTPUT : label
                m.Valeur().m_sousModeles[(size_t)cl]->predict(c->m_x, indice, cv::ml::StatModel::RAW_OUTPUT);  // RAW_OUTPUT seul : indice 0/1
                for (int i = 0; i < c->m_x.rows; ++i) {
                    bool positif = scores.Valeur().at<float>(i, cl) > 0.0f;
                    if (positif != ((int)std::lround(etiquette.at<float>(i)) == 1)) ++incoherences;
                    if (positif != ((int)std::lround(indice.at<float>(i)) == 1)) ++brutes;
                }
            }
            VERIFIER_EGAL(incoherences, 0L);   // score > 0 <=> classe positive pour predict sans RAW_OUTPUT
            VERIFIER_EGAL(brutes, 0L);         // et RAW_OUTPUT seul ne rend que l'indice, pas le score
        }
        InspectionNonVide(m.Valeur());
        VERIFIER_EGAL(m.Valeur().m_parametresEffectifs.Membre("type").Texte(), std::string(type));
    }
}

TEST(boost_decision_argmax_egalite_plus_petit_id_et_un_seul_yaml) {
    Corpus app = Trois(9), test = Trois(23, 20);
    ParametresAlgorithme p = Algo(Famille::Boost);
    p.m_boost.m_nbFaibles = 25;
    auto m = Apprendre(app, p);
    if (!Reussi(m, "boost")) return;
    auto scores = ScoresBoost(m.Valeur(), test.m_x);
    auto pred = Predire(m.Valeur(), test.m_x);
    if (!Reussi(scores, "scores") || !Reussi(pred, "pred")) return;
    for (int i = 0; i < test.m_x.rows; ++i) {
        int attendu = 0;
        for (int c = 1; c < 3; ++c) if (scores.Valeur().at<float>(i, c) > scores.Valeur().at<float>(i, attendu)) attendu = c;
        VERIFIER_EGAL(pred.Valeur()[(size_t)i], attendu);
    }
    VERIFIER_EGAL(m.Valeur().m_parametresEffectifs.Membre("nb_faibles_max").Entier(), 25LL);
    Modele relu;
    if (!Recharger(m.Valeur(), test, "familles_boost", relu)) return;
    const fs::path d = fs::current_path() / "tmp_tests" / "familles_boost";   // cree par Recharger
    auto texte = LireFichierTexte(d / "modele.yml");
    if (Reussi(texte, "lecture yaml")) {
        VERIFIER(texte.Valeur().find("decid_boost_ova") != std::string::npos);
        for (int c = 0; c < 3; ++c) VERIFIER(texte.Valeur().find("classe_" + std::to_string(c)) != std::string::npos);
    }
    auto s2 = ScoresBoost(relu, test.m_x);
    if (Reussi(s2, "scores relus"))
        for (int i = 0; i < test.m_x.rows; ++i)
            for (int c = 0; c < 3; ++c) VERIFIER_PROCHE(s2.Valeur().at<float>(i, c), scores.Valeur().at<float>(i, c), 1e-4);
    Refuse(ChargerModele(Famille::Boost, d / "modele.yml", 4), "nombre de classes different");
    Refuse(ChargerModele(Famille::Svm, d / "modele.yml", 3), "boost lu comme svm");
}

TEST(boost_classe_absente_et_parametres_refuses) {
    Corpus app = Trois(9, 10);
    ParametresAlgorithme p = Algo(Famille::Boost);
    Corpus avecTrou = app;       // la classe 2 n'a aucune ligne mais nbClasses vaut 3
    avecTrou.m_x = app.m_x.rowRange(0, 20).clone();
    avecTrou.m_labels.resize(20);
    Refuse(Apprendre(avecTrou, p), "classe 2 absente");
    p.m_boost.m_type = "RADIAL";
    Refuse(Apprendre(app, p), "type inconnu");
    p.m_boost.m_type = "DISCRETE";
    p.m_boost.m_nbFaibles = 0;
    Refuse(Apprendre(app, p), "nb_faibles = 0");
}

// ---------- toutes les familles : Entrainer, ACC-07, inspection ----------

TEST(entrainer_point_d_entree_unique_pour_les_sept_familles) {
    Corpus app = Trois(14), test = Trois(15, 15);
    ParametresAlgorithme parametres[7];
    parametres[0] = Algo(Famille::Svm);
    parametres[0].m_svm.m_gamma = 0.5;
    parametres[1] = Algo(Famille::ForetAleatoire);
    parametres[1].m_foret.m_nbArbres = 15;
    parametres[2] = Algo(Famille::Kppv);
    parametres[3] = Algo(Famille::BayesNormal);
    parametres[4] = Algo(Famille::Mlp);
    parametres[4].m_mlp = MlpRapide();
    parametres[5] = Algo(Famille::Em);
    parametres[5].m_em.m_k = 3;
    parametres[6] = Algo(Famille::Boost);
    parametres[6].m_boost.m_nbFaibles = 20;
    for (const ParametresAlgorithme& p : parametres) {
        const char* nom = NomFamille(p.m_famille);
        auto m = Apprendre(app, p, 5);
        if (!Reussi(m, nom)) continue;
        VERIFIER(m.Valeur().m_famille == p.m_famille);
        VERIFIER_EGAL(m.Valeur().m_nbAttributs, 2);
        VERIFIER(!m.Valeur().m_parametresDemandes.Membres().empty() || p.m_famille == Famille::BayesNormal);
        VERIFIER(!m.Valeur().m_parametresEffectifs.Membres().empty());
        double acc = ExactiteTest(m.Valeur(), test);
        if (!(acc > 0.9)) verif::Echec(__FILE__, __LINE__, std::string(nom) + " : exactitude trop faible " + std::to_string(acc));
        InspectionNonVide(m.Valeur());
        Modele relu;
        Recharger(m.Valeur(), test, std::string("familles_tous_") + nom, relu);
        VERIFIER(relu.m_famille == p.m_famille);
        auto dim = Predire(m.Valeur(), cv::Mat::zeros(2, 5, CV_32F));
        Refuse(dim, (std::string(nom) + " dimension incompatible").c_str());
    }
}

// ---------- experience complete et operations pas a pas ----------

namespace {

std::string CsvTroisClasses() {
    const double centres[3][3] = {{0.2, 0.2, 0.2}, {0.8, 0.2, 0.5}, {0.5, 0.8, 0.8}};
    Lcg g{12345};
    std::string csv = "Classe,x1,x2,x3\n";
    char tampon[128];
    for (int c = 0; c < 3; ++c)
        for (int i = 0; i < 40; ++i) {
            std::snprintf(tampon, sizeof(tampon), "%d,%.6f,%.6f,%.6f\n", c, centres[c][0] + (g.Suivant() - 0.5) * 0.3,
                          centres[c][1] + (g.Suivant() - 0.5) * 0.3, centres[c][2] + (g.Suivant() - 0.5) * 0.3);
            csv += tampon;
        }
    return csv;
}

fs::path EcrireCsv(const std::string& dossier) {
    auto d = verif::DossierTemporaire(dossier);
    std::ofstream f(d / "donnees.csv", std::ios::binary);
    const std::string csv = CsvTroisClasses();
    f.write(csv.data(), (std::streamsize)csv.size());
    return d / "donnees.csv";
}

ValeurJson Requete(const std::string& id, const std::string& operation, const fs::path& csv, const std::string& reste) {
    std::string t = "{\"protocol_version\":1,\"request_id\":\"" + id + "\",";
    if (!operation.empty()) t += "\"operation\":\"" + operation + "\",";
    t += "\"source\":{\"type\":\"csv\",\"chemin\":\"" + csv.generic_string() + "\",\"en_tete\":\"oui\"},"
         "\"partage\":{\"methode\":\"aleatoire_stratifie\",\"ratio\":0.6,\"graine\":7},"
         "\"normalisation\":{\"methode\":\"minmax\",\"bas\":0,\"haut\":1}";
    if (!reste.empty()) t += "," + reste;
    auto j = LireJson(t + "}");
    VERIFIER(j.Ok());
    return j.Ok() ? j.Valeur() : ValeurJson();
}

ValeurJson LireJ(const fs::path& chemin) {
    auto t = LireFichierTexte(chemin);
    VERIFIER(t.Ok());
    if (!t.Ok()) return ValeurJson();
    auto j = LireJson(t.Valeur());
    VERIFIER(j.Ok());
    return j.Ok() ? j.Valeur() : ValeurJson();
}

Statut LancerOperation(const ValeurJson& requete, const fs::path& racine, const std::string& nom, fs::path& staging) {
    staging = racine / "st" / nom;
    std::error_code ec;
    fs::remove_all(staging, ec);
    std::atomic<bool> annulation{false};
    RapporteurEvenement rapporteur = [](const std::string&, long, long, const std::string&) {};
    return ExecuterOperation(requete, racine, staging, annulation, rapporteur);
}

bool ArtefactListe(const fs::path& staging, const std::string& chemin) {
    const ValeurJson artefacts = LireJ(staging / "artefacts.json");
    for (const auto& f : artefacts.Membre("fichiers").Elements())
        if (f.Membre("chemin").Texte() == chemin) return true;
    return false;
}

// Publie un staging deja renomme en <tache>/resultat : commit.json coherent (comme l'orchestrateur)
void EcrireCommitPourTest(const fs::path& tache) {
    ValeurJson commit = ValeurJson::Objet();
    commit["schema"] = "decid.commit/1";
    commit["request_id"] = CheminUtf8(tache.filename());
    commit["dossier"] = "resultat";
    commit["sha256_artefacts"] = Sha256Fichier(tache / "resultat" / "artefacts.json");
    const std::string texte = commit.Ecrire();
    std::ofstream f(tache / "commit.json", std::ios::binary);
    f.write(texte.data(), (std::streamsize)texte.size());
}

struct FamilleExperience { const char* m_nom; const char* m_algorithme; };

} // namespace

TEST(experience_complete_pour_chaque_nouvelle_famille) {
    const fs::path csv = EcrireCsv("familles_csv_exp");
    const FamilleExperience familles[] = {
        {"kppv", "{\"famille\":\"kppv\",\"k\":5}"},
        {"bayes", "{\"famille\":\"bayes_normal\"}"},
        {"mlp", "{\"famille\":\"mlp\",\"couches_cachees\":[8],\"iterations_max\":300,\"eta\":0.1,\"moment\":0.1}"},
        {"em", "{\"famille\":\"em\",\"k\":3,\"covariance\":\"DIAGONALE\",\"iterations_max\":50}"},
        {"boost", "{\"famille\":\"boost\",\"type\":\"DISCRETE\",\"nb_faibles\":30,\"profondeur\":1}"}};
    for (const auto& f : familles) {
        ValeurJson j = Requete(std::string("exp-") + f.m_nom, "", csv, std::string("\"algorithme\":") + f.m_algorithme);
        auto req = RequeteDepuisJson(j, csv.parent_path());
        if (!Reussi(req, f.m_nom)) continue;
        // la requete se relit depuis son propre JSON (aller-retour des parametres)
        auto relue = RequeteDepuisJson(req.Valeur().VersJson(), csv.parent_path());
        if (Reussi(relue, "relecture requete")) VERIFIER_EGAL(relue.Valeur().VersJson().Ecrire(false), req.Valeur().VersJson().Ecrire(false));
        auto staging = verif::DossierTemporaire(std::string("familles_exp_") + f.m_nom) / "staging";
        std::atomic<bool> annulation{false};
        Statut st = ExecuterExperience(req.Valeur(), staging, annulation, [](const std::string&, long, long, const std::string&) {});
        if (!Reussi(st, f.m_nom)) continue;
        Statut v = VerifierStaging(staging);
        if (!Reussi(v, (std::string("VerifierStaging ") + f.m_nom).c_str())) continue;
        ValeurJson evaluation = LireJ(staging / "evaluation.json");
        double exactitude = evaluation.Membre("rapport").Membre("agregats").Membre("exactitude").Membre("value").Nombre();
        std::printf("  %s : exactitude test = %.4f\n", f.m_nom, exactitude);
        VERIFIER(exactitude > 0.9);
        ValeurJson manifeste = LireJ(staging / "modele.json");
        VERIFIER_EGAL(manifeste.Membre("famille").Texte(), std::string(NomFamille(req.Valeur().m_algorithme.m_famille)));
        VERIFIER(!manifeste.Membre("parametres_effectifs").Membres().empty());
        VERIFIER(manifeste.Contient("algorithme"));
        if (std::string(f.m_nom) == "em") {
            VERIFIER_EGAL(manifeste.Membre("correspondance_clusters").Elements().size(), (size_t)3);
            VERIFIER_EGAL(manifeste.Membre("parametres_effectifs").Membre("k").Entier(), 3LL);
        }
        VERIFIER(!LireJ(staging / "inspection.json").Membres().empty());
    }
}

TEST(operations_pas_a_pas_ecrivent_partage_json_et_traces_mlp_em) {
    const fs::path csv = EcrireCsv("familles_csv_ops");
    const fs::path racine = csv.parent_path();
    fs::path staging;
    const std::string trace = "\"trace\":{\"max_etats\":2000,\"max_octets\":67108864}";
    // partage.json dans le staging des operations existantes
    struct Ancienne { const char* m_op; const char* m_reste; };
    const Ancienne anciennes[] = {
        {"adaboost_pas_a_pas", "\"adaboost\":{\"faible\":\"seuil\",\"iterations\":5,\"classe_positive\":0}"},
        {"perceptron", "\"perceptron\":{\"eta\":0.1,\"epoques_max\":5,\"classe_positive\":0}"},
        {"kmeans", "\"kmeans\":{\"k\":3,\"graine\":1,\"iter_max\":10}"}};
    for (const auto& a : anciennes) {
        Statut st = LancerOperation(Requete(std::string("p-") + a.m_op, a.m_op, csv, std::string(a.m_reste) + "," + trace), racine, a.m_op, staging);
        if (!Reussi(st, a.m_op)) continue;
        VERIFIER(fs::exists(staging / "partage.json"));
        VERIFIER(ArtefactListe(staging, "partage.json"));
        VERIFIER(VerifierStaging(staging).Ok());
        auto pa = PartageDepuisJson(LireJ(staging / "partage.json"));
        if (Reussi(pa, "partage relu")) VERIFIER_EGAL(pa.Valeur().m_apprentissage.size(), (size_t)72);   // 0.6 x 120
    }
    // MLP pas a pas
    Statut st = LancerOperation(Requete("mlp-ops", "mlp_pas_a_pas", csv,
        "\"algorithme\":{\"famille\":\"mlp\",\"couches_cachees\":[6],\"iterations_max\":15,\"epsilon\":1e-12,\"eta\":0.1,\"moment\":0.1}," + trace +
        ",\"suivi_validation\":\"test\""), racine, "mlp", staging);
    if (Reussi(st, "mlp_pas_a_pas")) {
        VERIFIER(VerifierStaging(staging).Ok());
        VERIFIER(ArtefactListe(staging, "partage.json"));
        ValeurJson t = LireJ(staging / "trace.json");
        // reseau Decid (defaut) : etat initial (epoque 0) + 15 epoques
        VERIFIER_EGAL(t.Membre("etats").Elements().size(), (size_t)16);
        VERIFIER_EGAL(t.Membre("etats").Elements().front().Membre("iteration").Entier(), 0LL);
        VERIFIER(t.Membre("etats").Elements().back().Membre("grandeurs").Contient("exactitude_validation"));
        ValeurJson fin = LireJ(staging / "etat_final.json");
        VERIFIER_EGAL(fin.Membre("epoques_effectuees").Entier(), 15LL);
        VERIFIER_EGAL(fin.Membre("implementation").Texte(), std::string("decid"));
        VERIFIER(fin.Membre("inspection").Contient("couches"));
        VERIFIER(ArtefactListe(staging, "normalisation.json"));
    }
    // ancien chemin OpenCV, toujours disponible
    st = LancerOperation(Requete("mlp-opencv", "mlp_pas_a_pas", csv,
        "\"algorithme\":{\"famille\":\"mlp\",\"couches_cachees\":[6],\"iterations_max\":5,\"epsilon\":1e-12},\"reseau\":{\"moteur\":\"opencv\"}," + trace),
        racine, "mlp_opencv", staging);
    if (Reussi(st, "mlp_pas_a_pas moteur opencv")) {
        VERIFIER_EGAL(LireJ(staging / "trace.json").Membre("etats").Elements().size(), (size_t)5);
        VERIFIER(!LireJ(staging / "etat_final.json").Contient("implementation"));
    }
    Refuse(LancerOperation(Requete("mlp-moteur", "mlp_pas_a_pas", csv, "\"reseau\":{\"moteur\":\"autre\"}"), racine, "mlp_moteur", staging),
           "moteur inconnu");
    // activations d'un reseau Decid publie : points et cartes du plan
    {
        const fs::path taches = racine / "taches_reseau";
        std::error_code ec;
        fs::remove_all(taches, ec);
        fs::create_directories(taches / "mlp-source", ec);
        fs::path stagingSource;
        Statut s1 = LancerOperation(Requete("mlp-source", "mlp_pas_a_pas", csv,
            "\"algorithme\":{\"famille\":\"mlp\",\"couches_cachees\":[4,3],\"activation\":\"TANH\",\"methode\":\"RPROP\",\"iterations_max\":20,\"epsilon\":1e-12}," + trace),
            racine, "mlp_source", stagingSource);
        if (Reussi(s1, "source activations")) {
            fs::rename(stagingSource, taches / "mlp-source" / "resultat", ec);
            EcrireCommitPourTest(taches / "mlp-source");
            Statut s2 = LancerOperation(Requete("act", "activations", "",
                "\"reseau_source\":\"taches_reseau/mlp-source\",\"epoque\":7,\"points\":[[0.5,0.5,0.5]],"
                "\"plan\":{\"attribut_x\":0,\"attribut_y\":1,\"largeur\":10,\"hauteur\":8}"), racine, "act", staging);
            if (Reussi(s2, "activations")) {
                ValeurJson j = LireJ(staging / "activations.json");
                VERIFIER_EGAL(j.Membre("epoque").Entier(), 7LL);
                const auto& couches = j.Membre("points").Elements().at(0).Membre("activations").Elements();
                VERIFIER_EGAL(couches.size(), (size_t)4);                       // entree, 4, 3, sortie
                VERIFIER_EGAL(couches.back().Elements().size(), (size_t)3);
                double somme = 0;
                for (const auto& v : couches.back().Elements()) somme += v.Nombre();
                VERIFIER(std::fabs(somme - 1.0) < 1e-9);                       // softmax
                VERIFIER_EGAL(fs::file_size(staging / "neurones.f32"), (std::uintmax_t)((4 + 3 + 3) * 10 * 8 * 4));
                VERIFIER_EGAL(j.Membre("plan").Membre("cartes").Elements().size(), (size_t)10);
            }
            Refuse(LancerOperation(Requete("act2", "activations", "",
                "\"reseau_source\":\"taches_reseau/mlp-source\",\"epoque\":999"), racine, "act2", staging), "epoque absente");
        }
    }
    // EM pas a pas, sans bloc algorithme : la famille par defaut est em ; avec un autre algorithme : refus
    st = LancerOperation(Requete("em-ops", "em_pas_a_pas", csv, trace), racine, "em", staging);
    Reussi(st, "em_pas_a_pas sans bloc algorithme (K par defaut = 2)");
    st = LancerOperation(Requete("em-ops2", "em_pas_a_pas", csv,
        "\"algorithme\":{\"famille\":\"em\",\"k\":3,\"iterations_max\":25}," + trace), racine, "em2", staging);
    if (Reussi(st, "em_pas_a_pas")) {
        VERIFIER(VerifierStaging(staging).Ok());
        ValeurJson t = LireJ(staging / "trace.json");
        ValeurJson fin = LireJ(staging / "etat_final.json");
        VERIFIER_EGAL((long long)t.Membre("etats").Elements().size(), fin.Membre("iterations_effectuees").Entier());
        VERIFIER_EGAL(fin.Membre("correspondance_clusters").Elements().size(), (size_t)3);
        VERIFIER_EGAL(fin.Membre("clusters").Elements().size(), (size_t)72);
        const auto& tout = fin.Membre("clusters_toutes_lignes").Elements();
        const auto& lignes = fin.Membre("lignes").Elements();
        VERIFIER(tout.size() > (size_t)72);
        bool coherent = lignes.size() == 72;
        for (size_t i = 0; coherent && i < lignes.size(); ++i)
            coherent = tout[(size_t)lignes[i].Entier()].Entier() == fin.Membre("clusters").Elements()[i].Entier();
        VERIFIER(coherent);
        const ValeurJson& et = fin.Membre("exactitude_test");
        VERIFIER(et.Membre("lignes").Entier() == (long long)tout.size() - 72);
        VERIFIER(et.Membre("valeur").Nombre() >= 0 && et.Membre("valeur").Nombre() <= 1);
    }
    st = LancerOperation(Requete("mlp-mauvais", "mlp_pas_a_pas", csv, "\"algorithme\":{\"famille\":\"svm\"}"), racine, "mlp_mauvais", staging);
    Refuse(st, "mlp_pas_a_pas avec une famille svm");
    VERIFIER(ValiderRequete(Requete("v1", "em_pas_a_pas", csv, ""), racine).Ok());
    VERIFIER(ValiderRequete(Requete("v2", "mlp_pas_a_pas", csv, ""), racine).Ok());
}

int main(int argc, char** argv) { return LancerTests(argc, argv); }
