// Regression (voir Regression.h).
// Requete :
//  {"operation":"regression", "protocol_version":1, "request_id":"...",
//   "source": meme bloc que l'experience (type csv : chemin, separateur, en_tete, colonne_cible) ; la colonne cible
//             doit etre numerique (texte converti en double, refus explicite sinon),
//   "partage": {"methode":"aleatoire"|"premiers_n", "ratio":0.7, "graine":1},
//   "normalisation": {"methode":"aucune"|"minmax"|"minmax_lim", "bas":0, "haut":1[, "fichier": ".lim"]} (entrees seulement,
//     ajustee sur l'apprentissage, ou bornes imposees par le fichier .lim),
//   "algorithme": {"famille":"svm"|"mlp"|"foret_aleatoire"|"kppv", parametres de la famille},
//   "courbe": {"attribut":0, "points":200}}
// Partage aleatoire : Fisher-Yates avec TirageUniforme (splitmix64, comme Partage.cpp), puis floor(N*ratio)
// premieres lignes du melange en apprentissage ; indices tries.
// Courbe : coupe du modele le long d'un attribut (grille reguliere entre min et max d'apprentissage), les autres
// attributs fixes a leur moyenne d'apprentissage (valeurs brutes publiees).
// Controle : le modele publie est relu depuis regression_modele.yml et doit redonner les memes predictions.
#include "Regression.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "Donnees.h"
#include "Fichiers.h"
#include "Normalisation.h"
#include "Operations.h"
#include "Partage.h"
#include "Sha256.h"

namespace decid {

namespace {

using cv::ml::StatModel;

const int COURBE_MAX = 2000;
const std::size_t POINTS_PUBLIES_MAX = 5000;

Erreur Proto(const std::string& message) { return FaireErreur(CodeErreur::Protocole, "regression", message); }
Erreur ErreurCv(const char* phase, const cv::Exception& e) {
    return FaireErreur(CodeErreur::Calcul, phase, std::string("OpenCV : ") + e.what());
}
Statut Annule(const std::atomic<bool>& annulation) {
    if (annulation.load())
        return FaireErreur(CodeErreur::Annulation, "annulation", "calcul annule a la demande, rien n'est publie");
    return Succes();
}

double Nb(const ValeurJson& j, const char* cle, double defaut) { return j.Contient(cle) ? j.Membre(cle).Nombre() : defaut; }
int Ent(const ValeurJson& j, const char* cle, int defaut) { return j.Contient(cle) ? (int)j.Membre(cle).Entier() : defaut; }
std::string Txt(const ValeurJson& j, const char* cle, const std::string& defaut) {
    return j.Contient(cle) ? j.Membre(cle).Texte() : defaut;
}

std::string TexteNombre(double x) {
    char tampon[40];
    std::snprintf(tampon, sizeof(tampon), "%.17g", x);
    return tampon;
}

ValeurJson Nombres(const std::vector<double>& v) {
    ValeurJson t = ValeurJson::Tableau();
    for (double x : v) t.Ajouter(x);
    return t;
}

// ---------------------------------------------------------------- parametres

int NoyauOpenCv(const std::string& n) {
    if (n == "LINEAIRE") return cv::ml::SVM::LINEAR;
    if (n == "POLY") return cv::ml::SVM::POLY;
    if (n == "SIGMOIDE") return cv::ml::SVM::SIGMOID;
    return cv::ml::SVM::RBF;
}

Statut LireSvm(const ValeurJson& a, ParametresRegression& p) {
    p.m_noyau = Txt(a, "noyau", p.m_noyau);
    if (p.m_noyau != "LINEAIRE" && p.m_noyau != "POLY" && p.m_noyau != "RBF" && p.m_noyau != "SIGMOIDE")
        return Proto("noyau SVM : LINEAIRE, POLY, RBF ou SIGMOIDE");
    p.m_c = Nb(a, "c", p.m_c);
    p.m_gamma = Nb(a, "gamma", p.m_gamma);
    p.m_epsilon = Nb(a, "epsilon", p.m_epsilon);
    p.m_degre = Nb(a, "degre", p.m_degre);
    p.m_coef0 = Nb(a, "coef0", p.m_coef0);
    if (!(p.m_c > 0) || !(p.m_gamma > 0) || !(p.m_epsilon >= 0) || !(p.m_degre > 0) || !std::isfinite(p.m_coef0))
        return Proto("SVM : c > 0, gamma > 0, epsilon >= 0, degre > 0 attendus");
    return Succes();
}

Statut LireMlp(const ValeurJson& a, ParametresRegression& p) {
    if (a.Contient("couches_cachees")) {
        p.m_couchesCachees.clear();
        for (auto& c : a.Membre("couches_cachees").Elements()) p.m_couchesCachees.push_back((int)c.Entier());
    }
    if (p.m_couchesCachees.empty() || p.m_couchesCachees.size() > 8) return Proto("MLP : 1 a 8 couches cachees");
    for (int n : p.m_couchesCachees) if (n < 1 || n > 1024) return Proto("MLP : 1 a 1024 neurones par couche");
    p.m_activation = Txt(a, "activation", p.m_activation);
    if (p.m_activation != "SIGMOIDE_SYM" && p.m_activation != "IDENTITE")
        return Proto("MLP en regression : activation SIGMOIDE_SYM ou IDENTITE");
    p.m_methode = Txt(a, "methode", p.m_methode);
    if (p.m_methode != "RPROP" && p.m_methode != "RETROPROPAGATION") return Proto("MLP : methode RPROP ou RETROPROPAGATION");
    p.m_eta = Nb(a, "eta", p.m_eta);
    p.m_moment = Nb(a, "moment", p.m_moment);
    p.m_iterationsMax = Ent(a, "iterations_max", p.m_iterationsMax);
    p.m_tolerance = Nb(a, "epsilon", p.m_tolerance);
    if (!(p.m_eta > 0) || !(p.m_moment >= 0) || p.m_iterationsMax < 1 || p.m_iterationsMax > 100000 || !(p.m_tolerance >= 0))
        return Proto("MLP : eta > 0, moment >= 0, 1 <= iterations_max <= 100000, epsilon >= 0");
    return Succes();
}

Statut LireForet(const ValeurJson& a, ParametresRegression& p) {
    p.m_nbArbres = Ent(a, "nb_arbres", p.m_nbArbres);
    p.m_profondeurMax = Ent(a, "profondeur_max", p.m_profondeurMax);
    p.m_minEchantillons = Ent(a, "min_echantillons", p.m_minEchantillons);
    p.m_nbVariablesActives = Ent(a, "nb_variables_actives", p.m_nbVariablesActives);
    if (p.m_nbArbres < 1 || p.m_nbArbres > 2000 || p.m_profondeurMax < 1 || p.m_profondeurMax > 64 ||
        p.m_minEchantillons < 1 || p.m_nbVariablesActives < 0)
        return Proto("foret : 1..2000 arbres, profondeur 1..64, min_echantillons >= 1");
    return Succes();
}

// ---------------------------------------------------------------- modeles OpenCV

cv::Ptr<StatModel> CreerSvm(const ParametresRegression& p) {
    auto svm = cv::ml::SVM::create();
    svm->setType(cv::ml::SVM::EPS_SVR);
    svm->setKernel(NoyauOpenCv(p.m_noyau));
    svm->setC(p.m_c);
    svm->setGamma(p.m_gamma);
    svm->setP(p.m_epsilon);
    svm->setDegree(p.m_degre);
    svm->setCoef0(p.m_coef0);
    return svm;
}

cv::Ptr<StatModel> CreerMlp(const ParametresRegression& p, int nbAttributs) {
    auto ann = cv::ml::ANN_MLP::create();
    cv::Mat couches(1, (int)p.m_couchesCachees.size() + 2, CV_32S);
    couches.at<int>(0) = nbAttributs;
    for (std::size_t i = 0; i < p.m_couchesCachees.size(); ++i) couches.at<int>((int)i + 1) = p.m_couchesCachees[i];
    couches.at<int>(couches.cols - 1) = 1;   // une seule sortie en regression (comme l'ancien Decid)
    ann->setLayerSizes(couches);
    ann->setActivationFunction(p.m_activation == "IDENTITE" ? cv::ml::ANN_MLP::IDENTITY : cv::ml::ANN_MLP::SIGMOID_SYM, 0, 0);
    if (p.m_methode == "RETROPROPAGATION") ann->setTrainMethod(cv::ml::ANN_MLP::BACKPROP, p.m_eta, p.m_moment);
    else ann->setTrainMethod(cv::ml::ANN_MLP::RPROP, 0.1, FLT_EPSILON);
    ann->setTermCriteria(cv::TermCriteria(cv::TermCriteria::MAX_ITER + cv::TermCriteria::EPS, p.m_iterationsMax, p.m_tolerance));
    return ann;
}

cv::Ptr<StatModel> CreerForet(const ParametresRegression& p) {
    auto foret = cv::ml::RTrees::create();
    foret->setMaxDepth(p.m_profondeurMax);
    foret->setMinSampleCount(p.m_minEchantillons);
    foret->setActiveVarCount(p.m_nbVariablesActives);
    foret->setRegressionAccuracy(0.0001f);   // valeur de l'ancien Decid
    foret->setCalculateVarImportance(true);
    foret->setTermCriteria(cv::TermCriteria(cv::TermCriteria::MAX_ITER, p.m_nbArbres, 0));
    return foret;
}

cv::Ptr<StatModel> CreerKppv(const ParametresRegression& p) {
    auto knn = cv::ml::KNearest::create();
    knn->setDefaultK(p.m_k);
    knn->setIsClassifier(false);             // prediction = moyenne des cibles des k voisins
    knn->setAlgorithmType(cv::ml::KNearest::BRUTE_FORCE);
    return knn;
}

cv::Ptr<StatModel> ModeleVide(FamilleRegression f) {
    switch (f) {
        case FamilleRegression::Svm: return cv::ml::SVM::create();
        case FamilleRegression::Mlp: return cv::ml::ANN_MLP::create();
        case FamilleRegression::ForetAleatoire: return cv::ml::RTrees::create();
        case FamilleRegression::Kppv: return cv::ml::KNearest::create();
    }
    return {};
}

ValeurJson ParametresEffectifs(const cv::Ptr<StatModel>& m, FamilleRegression f) {
    ValeurJson e = ValeurJson::Objet();
    e["nb_attributs"] = m->getVarCount();
    if (f == FamilleRegression::Svm) {
        auto svm = m.dynamicCast<cv::ml::SVM>();
        e["c"] = svm->getC();
        e["gamma"] = svm->getGamma();
        e["epsilon"] = svm->getP();
        e["nb_vecteurs_support"] = svm->getSupportVectors().rows;
    } else if (f == FamilleRegression::Mlp) {
        auto ann = m.dynamicCast<cv::ml::ANN_MLP>();
        ValeurJson couches = ValeurJson::Tableau();
        cv::Mat c = ann->getLayerSizes();
        for (int i = 0; i < (int)c.total(); ++i) couches.Ajouter(c.at<int>(i));
        e["couches"] = couches;
        e["mise_a_l_echelle"] = "entrees et sortie mises a l'echelle par OpenCV (flags 0)";
    } else if (f == FamilleRegression::ForetAleatoire) {
        auto foret = m.dynamicCast<cv::ml::RTrees>();
        e["nb_arbres"] = (long long)foret->getRoots().size();
        cv::Mat imp = foret->getVarImportance();
        std::vector<double> v;
        for (int i = 0; i < (int)imp.total(); ++i) v.push_back(imp.at<float>(i));
        e["importance_variables"] = Nombres(v);
    } else {
        auto knn = m.dynamicCast<cv::ml::KNearest>();
        e["k"] = knn->getDefaultK();
    }
    return e;
}

std::string TexteModele(const cv::Ptr<StatModel>& m) {
    cv::FileStorage flux(".yml", cv::FileStorage::WRITE | cv::FileStorage::MEMORY);
    flux << m->getDefaultName() << "{";
    m->write(flux);
    flux << "}";
    return flux.releaseAndGetString();
}

Resultat<cv::Ptr<StatModel>> RelireModele(const std::string& texte, FamilleRegression f) {
    try {
        cv::FileStorage flux(texte, cv::FileStorage::READ | cv::FileStorage::MEMORY);
        cv::Ptr<StatModel> m = ModeleVide(f);
        cv::FileNode racine = flux.getFirstTopLevelNode();
        if (racine.empty() || racine.name() != m->getDefaultName())
            return FaireErreur(CodeErreur::ModeleIncompatible, "verification", "modele de regression relu incoherent");
        m->read(racine);
        if (!m->isTrained())
            return FaireErreur(CodeErreur::ModeleIncompatible, "verification", "modele de regression relu non entraine");
        return m;
    } catch (const cv::Exception& e) {
        return ErreurCv("verification", e);
    }
}

// ---------------------------------------------------------------- donnees

Resultat<std::vector<double>> CiblesNumeriques(const JeuDeDonnees& jeu) {
    std::vector<double> valeurParClasse;
    for (const std::string& t : jeu.m_nomsClasses) {
        char* fin = nullptr;
        const double v = std::strtod(t.c_str(), &fin);
        if (t.empty() || fin == nullptr || *fin != '\0' || !std::isfinite(v))
            return FaireErreur(CodeErreur::DonneesInvalides, "regression",
                               "cible non numerique : \"" + t + "\" (la colonne cible doit contenir des nombres)");
        valeurParClasse.push_back(v);
    }
    std::vector<double> y;
    y.reserve((size_t)jeu.m_nbLignes);
    for (int l : jeu.m_labels) y.push_back(valeurParClasse[(size_t)l]);
    return y;
}

void Partager(const RequeteRegression& r, int n, std::vector<int>& apprentissage, std::vector<int>& test) {
    std::vector<int> ordre((size_t)n);
    for (int i = 0; i < n; ++i) ordre[(size_t)i] = i;
    if (r.m_partage == "aleatoire") {
        std::uint64_t etat = r.m_graine;
        for (std::size_t i = ordre.size(); i > 1; --i)
            std::swap(ordre[i - 1], ordre[(size_t)TirageUniforme(etat, i)]);
    }
    const std::size_t nApp = (std::size_t)std::floor(n * r.m_ratio);
    apprentissage.assign(ordre.begin(), ordre.begin() + (std::ptrdiff_t)nApp);
    test.assign(ordre.begin() + (std::ptrdiff_t)nApp, ordre.end());
    std::sort(apprentissage.begin(), apprentissage.end());
    std::sort(test.begin(), test.end());
}

std::string HashIndices(const std::vector<int>& a, const std::vector<int>& t) {
    std::string s = "regression-partage/1;a";
    for (int i : a) s += "," + std::to_string(i);
    s += ";t";
    for (int i : t) s += "," + std::to_string(i);
    return Sha256Texte(s);
}

std::vector<double> Extraire(const std::vector<double>& v, const std::vector<int>& indices) {
    std::vector<double> r;
    for (int i : indices) r.push_back(v[(size_t)i]);
    return r;
}

struct Etat {
    JeuDeDonnees m_jeu;
    std::vector<double> m_y;
    std::vector<int> m_app, m_test;
    Normalisation m_norm;
    cv::Mat m_xApp, m_xTest;
    cv::Ptr<StatModel> m_modele;
    std::vector<double> m_predApp, m_predTest;
};

Statut Preparer(const RequeteRegression& r, Etat& e) {
    auto jeu = LireSourceRequete(r.m_source);
    if (!jeu.Ok()) return jeu.Err();
    e.m_jeu = std::move(jeu.Valeur());
    auto y = CiblesNumeriques(e.m_jeu);
    if (!y.Ok()) return y.Err();
    e.m_y = std::move(y.Valeur());
    Partager(r, e.m_jeu.m_nbLignes, e.m_app, e.m_test);
    if (e.m_app.size() < 2) return FaireErreur(CodeErreur::DonneesInvalides, "partage", "moins de 2 lignes d'apprentissage");
    if (r.m_algorithme.m_famille == FamilleRegression::Kppv && (int)e.m_app.size() < r.m_algorithme.m_k)
        return FaireErreur(CodeErreur::DonneesInvalides, "partage", "k superieur au nombre de lignes d'apprentissage");
    if (r.m_attributCourbe >= e.m_jeu.m_nbAttributs) return Proto("courbe.attribut hors des attributs du jeu");
    const std::string hash = HashIndices(e.m_app, e.m_test);
    auto n = NormalisationDemandee(r.m_source.m_normalisation, r.m_source.m_fichierLim, r.m_source.m_bas,
                                   r.m_source.m_haut, e.m_jeu, e.m_app, hash);
    if (!n.Ok()) return n.Err();
    e.m_norm = n.Valeur();
    auto xa = AppliquerNormalisation(e.m_norm, e.m_jeu, e.m_app);
    if (!xa.Ok()) return xa.Err();
    e.m_xApp = xa.Valeur();
    if (e.m_test.empty()) return Succes();
    auto xt = AppliquerNormalisation(e.m_norm, e.m_jeu, e.m_test);
    if (!xt.Ok()) return xt.Err();
    e.m_xTest = xt.Valeur();
    return Succes();
}

// ---------------------------------------------------------------- sorties

struct Courbe { std::vector<double> m_x, m_y, m_fixes; };

Resultat<Courbe> CalculerCourbe(const RequeteRegression& r, const Etat& e) {
    const int d = e.m_jeu.m_nbAttributs, a = r.m_attributCourbe;
    Courbe c;
    c.m_fixes.assign((size_t)d, 0.0);
    double mini = INFINITY, maxi = -INFINITY;
    for (int l : e.m_app) {
        for (int k = 0; k < d; ++k) c.m_fixes[(size_t)k] += e.m_jeu.Valeur(l, k) / (double)e.m_app.size();
        mini = std::min(mini, e.m_jeu.Valeur(l, a));
        maxi = std::max(maxi, e.m_jeu.Valeur(l, a));
    }
    const int n = r.m_pointsCourbe;
    cv::Mat x(n, d, CV_32F);
    for (int i = 0; i < n; ++i) {
        std::vector<double> brut = c.m_fixes;
        brut[(size_t)a] = n == 1 ? mini : mini + (maxi - mini) * i / (n - 1);
        c.m_x.push_back(brut[(size_t)a]);
        std::vector<double> t = TransformerVecteur(e.m_norm, brut);
        for (int k = 0; k < d; ++k) x.at<float>(i, k) = (float)t[(size_t)k];
    }
    auto y = PredireRegression(e.m_modele, x);
    if (!y.Ok()) return y.Err();
    c.m_y = y.Valeur();
    return c;
}

void AjouterLignes(std::string& csv, const Etat& e, const std::vector<int>& indices, const std::vector<double>& pred,
                   const char* ensemble) {
    for (std::size_t i = 0; i < indices.size(); ++i) {
        const int l = indices[i];
        const double y = e.m_y[(size_t)l];
        csv += std::to_string(l) + "," + std::to_string(e.m_jeu.m_origines[(size_t)l].m_ligne) + "," + ensemble + "," +
               TexteNombre(y) + "," + TexteNombre(pred[i]) + "," + TexteNombre(y - pred[i]) + "\n";
    }
}

std::string CsvPredictions(const Etat& e) {
    std::string csv = "indice,ligne_fichier,ensemble,cible,prediction,residu\n";
    AjouterLignes(csv, e, e.m_app, e.m_predApp, "apprentissage");
    AjouterLignes(csv, e, e.m_test, e.m_predTest, "test");
    return csv;
}

void AjouterPoints(ValeurJson& points, const Etat& e, const std::vector<int>& indices, const std::vector<double>& pred,
                   int attribut, const char* ensemble) {
    for (std::size_t i = 0; i < indices.size() && points.Elements().size() < POINTS_PUBLIES_MAX; ++i) {
        ValeurJson p = ValeurJson::Objet();
        p["x"] = e.m_jeu.Valeur(indices[i], attribut);
        p["cible"] = e.m_y[(size_t)indices[i]];
        p["prediction"] = pred[i];
        p["ensemble"] = ensemble;
        points.Ajouter(p);
    }
}

ValeurJson CourbeJson(const ValeurJson& requete, const RequeteRegression& r, const Etat& e, const Courbe& c) {
    ValeurJson j = ValeurJson::Objet();
    j["schema"] = "decid.regression-courbe/1";
    j["request_id"] = requete.Membre("request_id").Texte();
    j["attribut"] = r.m_attributCourbe;
    j["nom_attribut"] = e.m_jeu.m_nomsAttributs[(size_t)r.m_attributCourbe];
    j["definition"] = "coupe du modele le long de l'attribut, grille reguliere entre min et max d'apprentissage ; "
                      "autres attributs fixes a leur moyenne d'apprentissage (valeurs brutes)";
    j["valeurs_fixes"] = Nombres(c.m_fixes);
    j["x"] = Nombres(c.m_x);
    j["y"] = Nombres(c.m_y);
    ValeurJson points = ValeurJson::Tableau();
    AjouterPoints(points, e, e.m_test, e.m_predTest, r.m_attributCourbe, "test");
    AjouterPoints(points, e, e.m_app, e.m_predApp, r.m_attributCourbe, "apprentissage");
    j["points"] = points;
    j["points_tronques"] = points.Elements().size() < e.m_app.size() + e.m_test.size();
    return j;
}

ValeurJson SourceJson(const RequeteRegression& r, const Etat& e) {
    ValeurJson s = ValeurJson::Objet();
    s["chemin"] = CheminUtf8(r.m_source.m_source);
    if (!e.m_jeu.m_sources.empty()) s["sha256"] = e.m_jeu.m_sources[0].m_sha256;
    s["hash_jeu"] = e.m_jeu.HashCanonique();
    s["nb_lignes"] = e.m_jeu.m_nbLignes;
    s["nb_attributs"] = e.m_jeu.m_nbAttributs;
    s["colonne_cible"] = r.m_source.m_contratCsv.m_colonneCible;
    ValeurJson noms = ValeurJson::Tableau();
    for (auto& n : e.m_jeu.m_nomsAttributs) noms.Ajouter(n);
    s["noms_attributs"] = noms;
    s["contrat"] = e.m_jeu.m_contrat;
    return s;
}

ValeurJson Description(const ValeurJson& requete, const RequeteRegression& r, const Etat& e) {
    ValeurJson j = ValeurJson::Objet();
    j["schema"] = "decid.regression/1";
    j["request_id"] = requete.Membre("request_id").Texte();
    j["operation"] = "regression";
    j["source"] = SourceJson(r, e);
    ValeurJson p = ValeurJson::Objet();
    p["methode"] = r.m_partage;
    p["ratio"] = r.m_ratio;
    p["graine"] = (unsigned long long)r.m_graine;
    p["generateur"] = "splitmix64+rejet+fisher_yates_decid_v1";
    p["n_apprentissage"] = (long long)e.m_app.size();
    p["n_test"] = (long long)e.m_test.size();
    p["hash"] = e.m_norm.m_hashPartage;
    j["partage"] = p;
    j["normalisation"] = e.m_norm.VersJson();
    j["algorithme"] = r.m_algorithme.VersJson();
    j["parametres_effectifs"] = ParametresEffectifs(e.m_modele, r.m_algorithme.m_famille);
    ValeurJson m = ValeurJson::Objet();
    m["apprentissage"] = CalculerMetriques(Extraire(e.m_y, e.m_app), e.m_predApp).VersJson();
    m["test"] = CalculerMetriques(Extraire(e.m_y, e.m_test), e.m_predTest).VersJson();
    m["definitions"] = "EQM = moyenne de (y - prediction)^2 ; MAE = moyenne de |y - prediction| ; "
                       "R2 = 1 - somme des residus^2 / somme des (y - moyenne de y)^2, sur l'ensemble evalue";
    j["metriques"] = m;
    ValeurJson f = ValeurJson::Tableau();
    for (const char* n : {"regression_predictions.csv", "regression_courbe.json", "regression_modele.yml"}) f.Ajouter(n);
    j["fichiers"] = f;
    j["requete_brute"] = requete;
    return j;
}

// Relecture du modele publie : memes predictions sur le test (ou l'apprentissage si le test est vide).
Statut VerifierRelecture(const std::string& texte, const RequeteRegression& r, const Etat& e) {
    auto relu = RelireModele(texte, r.m_algorithme.m_famille);
    if (!relu.Ok()) return relu.Err();
    const bool surTest = !e.m_test.empty();
    auto pred = PredireRegression(relu.Valeur(), surTest ? e.m_xTest : e.m_xApp);
    if (!pred.Ok()) return pred.Err();
    const std::vector<double>& attendu = surTest ? e.m_predTest : e.m_predApp;
    for (std::size_t i = 0; i < attendu.size(); ++i)
        if (std::fabs(pred.Valeur()[i] - attendu[i]) > 1e-6 * (1.0 + std::fabs(attendu[i])))
            return FaireErreur(CodeErreur::ModeleIncompatible, "verification", "le modele relu ne redonne pas les predictions");
    return Succes();
}

Statut Ecrire(const fs::path& staging, const char* nom, const std::string& texte) {
    return EcrireFichierAtomique(staging / nom, texte, false);
}

} // namespace

// ---------------------------------------------------------------- API

const char* NomFamilleRegression(FamilleRegression f) {
    switch (f) {
        case FamilleRegression::Svm: return "svm";
        case FamilleRegression::Mlp: return "mlp";
        case FamilleRegression::ForetAleatoire: return "foret_aleatoire";
        case FamilleRegression::Kppv: return "kppv";
    }
    return "?";
}

ValeurJson ParametresRegression::VersJson() const {
    ValeurJson j = ValeurJson::Objet();
    j["famille"] = NomFamilleRegression(m_famille);
    if (m_famille == FamilleRegression::Svm) {
        j["type"] = "EPS_SVR";
        j["noyau"] = m_noyau;
        j["c"] = m_c;
        j["gamma"] = m_gamma;
        j["epsilon"] = m_epsilon;
        j["degre"] = m_degre;
        j["coef0"] = m_coef0;
    } else if (m_famille == FamilleRegression::Mlp) {
        ValeurJson c = ValeurJson::Tableau();
        for (int n : m_couchesCachees) c.Ajouter(n);
        j["couches_cachees"] = c;
        j["activation"] = m_activation;
        j["methode"] = m_methode;
        j["eta"] = m_eta;
        j["moment"] = m_moment;
        j["iterations_max"] = m_iterationsMax;
        j["epsilon"] = m_tolerance;
    } else if (m_famille == FamilleRegression::ForetAleatoire) {
        j["nb_arbres"] = m_nbArbres;
        j["profondeur_max"] = m_profondeurMax;
        j["min_echantillons"] = m_minEchantillons;
        j["nb_variables_actives"] = m_nbVariablesActives;
    } else {
        j["k"] = m_k;
    }
    return j;
}

Resultat<ParametresRegression> ParametresRegressionDepuisJson(const ValeurJson& a) {
    ParametresRegression p;
    const std::string famille = a.Membre("famille").Texte();
    bool connue = false;
    for (auto f : {FamilleRegression::Svm, FamilleRegression::Mlp, FamilleRegression::ForetAleatoire, FamilleRegression::Kppv})
        if (famille == NomFamilleRegression(f)) { p.m_famille = f; connue = true; }
    if (!connue) return Proto("algorithme.famille : svm, mlp, foret_aleatoire ou kppv");
    Statut st = Succes();
    if (p.m_famille == FamilleRegression::Svm) st = LireSvm(a, p);
    else if (p.m_famille == FamilleRegression::Mlp) st = LireMlp(a, p);
    else if (p.m_famille == FamilleRegression::ForetAleatoire) st = LireForet(a, p);
    else {
        p.m_k = Ent(a, "k", p.m_k);
        if (p.m_k < 1 || p.m_k > 1000) st = Proto("KPPV : 1 <= k <= 1000");
    }
    if (!st.Ok()) return st.Err();
    return p;
}

ValeurJson MetriquesRegression::VersJson() const {
    ValeurJson j = ValeurJson::Objet();
    j["n"] = m_n;
    if (m_n == 0) return j;
    j["eqm"] = m_eqm;
    j["rmse"] = std::sqrt(m_eqm);
    j["mae"] = m_mae;
    j["r2"] = m_r2Defini ? ValeurJson(m_r2) : ValeurJson();
    return j;
}

MetriquesRegression CalculerMetriques(const std::vector<double>& cible, const std::vector<double>& prediction) {
    MetriquesRegression m;
    m.m_n = (long)cible.size();
    if (cible.empty()) return m;
    double moyenne = 0, ssRes = 0, ssTot = 0, abs = 0;
    for (double y : cible) moyenne += y / (double)cible.size();
    for (std::size_t i = 0; i < cible.size(); ++i) {
        const double r = cible[i] - prediction[i];
        ssRes += r * r;
        abs += std::fabs(r);
        ssTot += (cible[i] - moyenne) * (cible[i] - moyenne);
    }
    m.m_eqm = ssRes / (double)cible.size();
    m.m_mae = abs / (double)cible.size();
    m.m_r2Defini = ssTot > 0;
    if (m.m_r2Defini) m.m_r2 = 1.0 - ssRes / ssTot;
    return m;
}

Resultat<RequeteRegression> RequeteRegressionDepuisJson(const ValeurJson& j, const fs::path& dossierRequete) {
    // Le bloc source est lu par la fonction de l'experience ; partage et algorithme de classification neutres.
    ValeurJson copie = j;
    ValeurJson partageNeutre = ValeurJson::Objet();
    partageNeutre["methode"] = NomMethode(MethodePartage::PremiersN);
    copie["partage"] = partageNeutre;
    ValeurJson algoNeutre = ValeurJson::Objet();
    algoNeutre["famille"] = "svm";
    copie["algorithme"] = algoNeutre;
    auto source = RequeteDepuisJson(copie, dossierRequete);
    if (!source.Ok()) return source.Err();
    RequeteRegression r;
    r.m_source = source.Valeur();
    if (r.m_source.m_typeSource != "csv") return Proto("regression : source de type csv seulement");
    r.m_source.m_contratCsv.m_cibleNumerique = true;
    const ValeurJson& p = j.Membre("partage");
    r.m_partage = Txt(p, "methode", r.m_partage);
    if (r.m_partage != "aleatoire" && r.m_partage != "premiers_n") return Proto("partage : aleatoire ou premiers_n");
    r.m_ratio = Nb(p, "ratio", r.m_ratio);
    if (!(r.m_ratio > 0 && r.m_ratio <= 1)) return Proto("partage.ratio dans ]0, 1]");
    const long long graine = p.Contient("graine") ? p.Membre("graine").Entier() : 1;
    if (graine < 0) return Proto("partage.graine : entier positif ou nul");
    r.m_graine = (std::uint64_t)graine;
    auto algo = ParametresRegressionDepuisJson(j.Membre("algorithme"));
    if (!algo.Ok()) return algo.Err();
    r.m_algorithme = algo.Valeur();
    r.m_attributCourbe = Ent(j.Membre("courbe"), "attribut", 0);
    r.m_pointsCourbe = Ent(j.Membre("courbe"), "points", 200);
    if (r.m_attributCourbe < 0) return Proto("courbe.attribut : indice positif");
    if (r.m_pointsCourbe < 2 || r.m_pointsCourbe > COURBE_MAX) return Proto("courbe.points : 2 a 2000");
    return r;
}

Resultat<cv::Ptr<StatModel>> EntrainerRegression(const cv::Mat& x, const std::vector<double>& y,
                                                 const ParametresRegression& p, std::uint64_t graine) {
    cv::Mat reponses((int)y.size(), 1, CV_32F);
    for (std::size_t i = 0; i < y.size(); ++i) reponses.at<float>((int)i) = (float)y[i];
    try {
        cv::theRNG() = cv::RNG(graine);   // RNG local au thread qui calcule (poids du MLP, tirages de la foret)
        cv::Ptr<StatModel> m;
        switch (p.m_famille) {
            case FamilleRegression::Svm: m = CreerSvm(p); break;
            case FamilleRegression::Mlp: m = CreerMlp(p, x.cols); break;
            case FamilleRegression::ForetAleatoire: m = CreerForet(p); break;
            case FamilleRegression::Kppv: m = CreerKppv(p); break;
        }
        cv::Mat types(1, x.cols + 1, CV_8U, cv::Scalar(cv::ml::VAR_ORDERED));   // reponse ordonnee = regression
        auto donnees = cv::ml::TrainData::create(x, cv::ml::ROW_SAMPLE, reponses, cv::noArray(), cv::noArray(),
                                                 cv::noArray(), types);
        if (!m->train(donnees)) return FaireErreur(CodeErreur::Calcul, "apprentissage", "OpenCV a refuse l'apprentissage");
        return m;
    } catch (const cv::Exception& e) {
        return ErreurCv("apprentissage", e);
    }
}

Resultat<std::vector<double>> PredireRegression(const cv::Ptr<StatModel>& modele, const cv::Mat& x) {
    if (x.rows == 0) return std::vector<double>();
    if (x.cols != modele->getVarCount())
        return FaireErreur(CodeErreur::ModeleIncompatible, "prediction", "dimension incompatible avec le modele");
    cv::Mat sortie;
    try {
        modele->predict(x, sortie);
    } catch (const cv::Exception& e) {
        return ErreurCv("prediction", e);
    }
    sortie.convertTo(sortie, CV_64F);
    if (sortie.rows != x.rows) return FaireErreur(CodeErreur::Calcul, "prediction", "nombre de sorties inattendu");
    std::vector<double> y;
    for (int i = 0; i < sortie.rows; ++i) {
        const double v = sortie.at<double>(i, 0);
        if (!std::isfinite(v)) return FaireErreur(CodeErreur::Calcul, "prediction", "prediction non finie");
        y.push_back(v);
    }
    return y;
}

Statut ExecuterRegression(const ValeurJson& requete, const fs::path& dossierRequete, const fs::path& staging,
                          const std::atomic<bool>& annulation) {
    auto r = RequeteRegressionDepuisJson(requete, dossierRequete);
    if (!r.Ok()) return r.Err();
    Etat e;
    Statut st = Preparer(r.Valeur(), e);
    if (!st.Ok() || !(st = Annule(annulation)).Ok()) return st;
    auto modele = EntrainerRegression(e.m_xApp, Extraire(e.m_y, e.m_app), r.Valeur().m_algorithme, r.Valeur().m_graine);
    if (!modele.Ok()) return modele.Err();
    e.m_modele = modele.Valeur();
    if (!(st = Annule(annulation)).Ok()) return st;
    auto pa = PredireRegression(e.m_modele, e.m_xApp);
    if (!pa.Ok()) return pa.Err();
    e.m_predApp = pa.Valeur();
    auto pt = PredireRegression(e.m_modele, e.m_xTest);
    if (!pt.Ok()) return pt.Err();
    e.m_predTest = pt.Valeur();
    auto courbe = CalculerCourbe(r.Valeur(), e);
    if (!courbe.Ok()) return courbe.Err();
    std::string texteModele;
    try {
        texteModele = TexteModele(e.m_modele);
    } catch (const cv::Exception& ex) {
        return ErreurCv("sauvegarde_modele", ex);
    }
    if (!(st = VerifierRelecture(texteModele, r.Valeur(), e)).Ok()) return st;
    if (!(st = Annule(annulation)).Ok()) return st;
    if (!(st = Ecrire(staging, "regression_modele.yml", texteModele)).Ok()) return st;
    if (!(st = Ecrire(staging, "regression_predictions.csv", CsvPredictions(e))).Ok()) return st;
    if (!(st = Ecrire(staging, "regression_courbe.json", CourbeJson(requete, r.Valeur(), e, courbe.Valeur()).Ecrire())).Ok())
        return st;
    return Ecrire(staging, "regression.json", Description(requete, r.Valeur(), e).Ecrire());
}

} // namespace decid
