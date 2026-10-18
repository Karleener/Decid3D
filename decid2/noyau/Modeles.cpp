// Familles de classifieurs : cv::ml direct, parametres effectifs relus, sauvegarde en memoire
// (chemins Unicode geres par Decid, format YAML OpenCV identique aux anciens fichiers).
//
// Regles OpenCV 4.10 verifiees dans les sources (modules/ml/src) et documentees ici :
//  - KPPV (knearest.cpp) : distance euclidienne CARREE en float ; k est rabattu en silence a N par OpenCV
//    (Decid le refuse) ; vote = classe la plus frequente parmi les k reponses triees par valeur croissante,
//    la premiere classe de comptage strictement maximal l'emporte => egalite -> plus petit id de classe ;
//    a distance egale, le voisin deja insere (indice d'apprentissage le plus petit) est conserve.
//  - Bayes (nbayes.cpp) : une gaussienne a covariance pleine par classe (estimateur sans biais n-1, valeurs
//    propres plancher FLT_EPSILON), AUCUN a priori : decision = argmin(ln det + Mahalanobis^2) = argmax de la
//    log-vraisemblance gaussienne, donc a priori egaux quelle que soit la frequence des classes.
//  - MLP (ann_mlp.cpp) : le RNG est interne et fixe (graine sans effet) ; entrees standardisees par OpenCV ;
//    sorties mises a l'echelle de l'activation (sigmoide symetrique : [min,max] des cibles -> +-0.95) ;
//    RELU/IDENTITE ont une echelle degeneree (min = max = 0) => NO_OUTPUT_SCALE obligatoire ;
//    train() ne renvoie qu'un booleen : le nombre d'iterations effectuees n'est pas expose.
//  - EM (em.cpp) : trainEM = kmeans (theRNG) puis boucle E/M ; trainM(1 iteration) = M puis E ;
//    predict2 donne le cluster (premier maximum).
//  - Boost (boost.cpp) : predict(RAW_OUTPUT) seul renvoie l'INDICE de classe (0/1), pas le score ;
//    le score brut exige PREDICT_SUM | RAW_OUTPUT ; sans RAW_OUTPUT : label[val > 0].
#include "Modeles.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <opencv2/core/utility.hpp>
#include "Fichiers.h"
#include "Inspection.h"
#include "Trace.h"

namespace decid {

using cv::ml::StatModel;
using ModelePtr = cv::Ptr<cv::ml::StatModel>;

const char* NomFamille(Famille f) {
    switch (f) {
    case Famille::Svm: return "svm";
    case Famille::ForetAleatoire: return "foret_aleatoire";
    case Famille::Kppv: return "kppv";
    case Famille::BayesNormal: return "bayes_normal";
    case Famille::Mlp: return "mlp";
    case Famille::Em: return "em";
    case Famille::Boost: return "boost";
    }
    return "inconnue";
}

bool FamilleDepuisNom(const std::string& nom, Famille& f) {
    static const std::pair<const char*, Famille> TABLE[] = {
        {"svm", Famille::Svm}, {"foret_aleatoire", Famille::ForetAleatoire}, {"rf", Famille::ForetAleatoire},
        {"kppv", Famille::Kppv}, {"bayes_normal", Famille::BayesNormal}, {"mlp", Famille::Mlp},
        {"em", Famille::Em}, {"boost", Famille::Boost}};
    for (const auto& e : TABLE)
        if (nom == e.first) { f = e.second; return true; }
    return false;
}

// ====================================================================== parametres <-> JSON

namespace {

// Lecture typee d'un bloc "algorithme" : la premiere erreur est conservee, aucune valeur n'est convertie en silence.
class LecteurParametres {
public:
    explicit LecteurParametres(const ValeurJson& bloc) : m_bloc(bloc) {}
    void Entier(const char* cle, int& v) {
        double d = 0;
        if (!Nombre(cle, d)) return;
        if (d != std::floor(d) || std::fabs(d) > 2e9) return Refuser(cle, "entier attendu");
        v = (int)d;
    }
    void Reel(const char* cle, double& v) {
        double d = 0;
        if (Nombre(cle, d)) v = d;
    }
    void Texte(const char* cle, std::string& v) {
        if (!Present(cle)) return;
        if (m_bloc.Membre(cle).GetType() != ValeurJson::Type::Texte) return Refuser(cle, "texte attendu");
        v = m_bloc.Membre(cle).Texte();
    }
    void Couches(const char* cle, std::vector<int>& v) {
        if (!Present(cle)) return;
        const ValeurJson& t = m_bloc.Membre(cle);
        if (t.GetType() != ValeurJson::Type::Tableau) return Refuser(cle, "tableau d'entiers attendu");
        std::vector<int> lu;
        for (const auto& e : t.Elements()) {
            double d = e.Nombre();
            bool nombre = e.GetType() == ValeurJson::Type::Entier || e.GetType() == ValeurJson::Type::Nombre;
            if (!nombre || d != std::floor(d) || std::fabs(d) > 2e9) return Refuser(cle, "tableau d'entiers attendu");
            lu.push_back((int)d);
        }
        v = lu;
    }
    Statut Fin() const {
        if (m_erreur.empty()) return Succes();
        return FaireErreur(CodeErreur::Protocole, "requete", m_erreur);
    }
private:
    bool Present(const char* cle) const { return m_erreur.empty() && m_bloc.Contient(cle); }
    bool Nombre(const char* cle, double& d) {
        if (!Present(cle)) return false;
        const ValeurJson& j = m_bloc.Membre(cle);
        if (j.GetType() != ValeurJson::Type::Nombre && j.GetType() != ValeurJson::Type::Entier) {
            Refuser(cle, "nombre attendu");
            return false;
        }
        d = j.Nombre();
        return true;
    }
    void Refuser(const char* cle, const char* attendu) { m_erreur = std::string("algorithme.") + cle + " : " + attendu; }
    const ValeurJson& m_bloc;
    std::string m_erreur;
};

void LireSvm(LecteurParametres& l, ParametresSvm& p) {
    l.Texte("noyau", p.m_noyau); l.Reel("c", p.m_c); l.Reel("gamma", p.m_gamma);
    l.Reel("degre", p.m_degre); l.Reel("coef0", p.m_coef0);
}
void LireForet(LecteurParametres& l, ParametresForet& p) {
    l.Entier("nb_arbres", p.m_nbArbres); l.Entier("profondeur_max", p.m_profondeurMax);
    l.Entier("min_echantillons", p.m_minEchantillons); l.Entier("nb_variables_actives", p.m_nbVariablesActives);
}
void LireMlp(LecteurParametres& l, ParametresMlp& p) {
    l.Couches("couches_cachees", p.m_couchesCachees); l.Texte("activation", p.m_activation);
    l.Texte("methode", p.m_methode); l.Reel("eta", p.m_eta); l.Reel("moment", p.m_moment);
    l.Entier("iterations_max", p.m_iterationsMax); l.Reel("epsilon", p.m_epsilon);
}
void LireEm(LecteurParametres& l, ParametresEm& p) {
    l.Entier("k", p.m_k); l.Texte("covariance", p.m_covariance);
    l.Entier("iterations_max", p.m_iterationsMax); l.Reel("epsilon", p.m_epsilon);
}
void LireBoost(LecteurParametres& l, ParametresBoost& p) {
    l.Texte("type", p.m_type); l.Entier("nb_faibles", p.m_nbFaibles); l.Entier("profondeur", p.m_profondeur);
}

} // namespace

Resultat<ParametresAlgorithme> ParametresDepuisJson(const ValeurJson& a) {
    ParametresAlgorithme p;
    if (!FamilleDepuisNom(a.Membre("famille").Texte(), p.m_famille))
        return FaireErreur(CodeErreur::Protocole, "requete",
            "famille d'algorithme inconnue (svm, foret_aleatoire, kppv, bayes_normal, mlp, em, boost)");
    LecteurParametres l(a);
    switch (p.m_famille) {
    case Famille::Svm: LireSvm(l, p.m_svm); break;
    case Famille::ForetAleatoire: LireForet(l, p.m_foret); break;
    case Famille::Kppv: l.Entier("k", p.m_kppv.m_k); break;
    case Famille::BayesNormal: break;
    case Famille::Mlp: LireMlp(l, p.m_mlp); break;
    case Famille::Em: LireEm(l, p.m_em); break;
    case Famille::Boost: LireBoost(l, p.m_boost); break;
    }
    Statut st = l.Fin();
    if (!st.Ok()) return st.Err();
    return p;
}

ValeurJson ParametresAlgorithme::VersJson() const {
    ValeurJson a = ValeurJson::Objet();
    a["famille"] = NomFamille(m_famille);
    switch (m_famille) {
    case Famille::Svm:
        a["noyau"] = m_svm.m_noyau; a["c"] = m_svm.m_c; a["gamma"] = m_svm.m_gamma;
        a["degre"] = m_svm.m_degre; a["coef0"] = m_svm.m_coef0;
        break;
    case Famille::ForetAleatoire:
        a["nb_arbres"] = m_foret.m_nbArbres; a["profondeur_max"] = m_foret.m_profondeurMax;
        a["min_echantillons"] = m_foret.m_minEchantillons; a["nb_variables_actives"] = m_foret.m_nbVariablesActives;
        break;
    case Famille::Kppv: a["k"] = m_kppv.m_k; break;
    case Famille::BayesNormal: break;
    case Famille::Mlp: {
        ValeurJson couches = ValeurJson::Tableau();
        for (int n : m_mlp.m_couchesCachees) couches.Ajouter(n);
        a["couches_cachees"] = couches; a["activation"] = m_mlp.m_activation; a["methode"] = m_mlp.m_methode;
        a["eta"] = m_mlp.m_eta; a["moment"] = m_mlp.m_moment;
        a["iterations_max"] = m_mlp.m_iterationsMax; a["epsilon"] = m_mlp.m_epsilon;
        break;
    }
    case Famille::Em:
        a["k"] = m_em.m_k; a["covariance"] = m_em.m_covariance;
        a["iterations_max"] = m_em.m_iterationsMax; a["epsilon"] = m_em.m_epsilon;
        break;
    case Famille::Boost:
        a["type"] = m_boost.m_type; a["nb_faibles"] = m_boost.m_nbFaibles; a["profondeur"] = m_boost.m_profondeur;
        break;
    }
    return a;
}

// ====================================================================== outils communs

namespace {

Erreur ErreurCv(const char* phase, const cv::Exception& e) {
    return FaireErreur(CodeErreur::Calcul, phase, std::string("OpenCV : ") + e.what());
}

cv::Mat LabelsOpenCv(const std::vector<int>& labels) {
    cv::Mat m((int)labels.size(), 1, CV_32S);
    for (size_t i = 0; i < labels.size(); ++i) m.at<int>((int)i) = labels[i];
    return m;
}

Statut VerifierEntrees(const cv::Mat& x, const std::vector<int>& labels, int nbClasses, const char* phase) {
    if (x.empty() || x.type() != CV_32F || x.rows != (int)labels.size())
        return FaireErreur(CodeErreur::DonneesInvalides, phase, "matrice d'apprentissage vide ou incoherente");
    std::vector<char> presente((size_t)std::max(nbClasses, 0), 0);
    for (int l : labels) {
        if (l < 0 || l >= nbClasses)
            return FaireErreur(CodeErreur::DonneesInvalides, phase, "label interne hors plage avant apprentissage");
        presente[(size_t)l] = 1;
    }
    int nbPresentes = 0;
    for (char p : presente) nbPresentes += p;
    if (nbPresentes < 2)
        return FaireErreur(CodeErreur::DonneesInvalides, phase, "au moins deux classes sont necessaires en apprentissage");
    return Succes();
}

ValeurJson CritereVersJson(const cv::TermCriteria& t) {
    ValeurJson j = ValeurJson::Objet();
    j["type"] = t.type;
    j["max_iterations"] = t.maxCount;
    j["epsilon"] = t.epsilon;
    return j;
}

ValeurJson VecteurVersJson(const std::vector<double>& v) {
    ValeurJson t = ValeurJson::Tableau();
    for (double d : v) t.Ajouter(d);
    return t;
}

ValeurJson EntiersVersJson(const std::vector<int>& v) {
    ValeurJson t = ValeurJson::Tableau();
    for (int d : v) t.Ajouter(d);
    return t;
}

Modele ModeleDeBase(Famille f, const cv::Mat& x, int nbClasses, const ValeurJson& demandes) {
    Modele m;
    m.m_famille = f;
    m.m_nbAttributs = x.cols;
    m.m_nbClasses = nbClasses;
    m.m_parametresDemandes = demandes;
    return m;
}

cv::Ptr<cv::ml::TrainData> DonneesClassification(const cv::Mat& x, const std::vector<int>& labels) {
    return cv::ml::TrainData::create(x, cv::ml::ROW_SAMPLE, LabelsOpenCv(labels));
}

// Ecriture d'un modele OpenCV dans un noeud nomme du flux
void EcrireNoeud(cv::FileStorage& flux, const ModelePtr& modele, const std::string& nom) {
    flux << nom << "{";
    modele->write(flux);
    flux << "}";
}

// YAML OpenCV (en memoire) d'un modele isole : sert a relire des etats qu'OpenCV n'expose pas par getters
std::string YamlDe(const ModelePtr& modele) {
    cv::FileStorage flux(".yml", cv::FileStorage::WRITE | cv::FileStorage::MEMORY);
    EcrireNoeud(flux, modele, modele->getDefaultName());
    return flux.releaseAndGetString();
}

double ValeurSortie(const cv::Mat& sorties, int i) {
    return sorties.type() == CV_32S ? (double)sorties.at<int>(i) : (double)sorties.at<float>(i);
}

// Plus grand element d'une ligne (premier en cas d'egalite) ; -1 si une valeur n'est pas finie
int ArgMaxLigne(const float* v, int n) {
    int meilleur = 0;
    for (int c = 0; c < n; ++c) {
        if (!std::isfinite(v[c])) return -1;
        if (v[c] > v[meilleur]) meilleur = c;
    }
    return meilleur;
}

} // namespace

// ====================================================================== SVM

namespace {

int NoyauOpenCv(const std::string& nom) {
    if (nom == "LINEAIRE") return cv::ml::SVM::LINEAR;
    if (nom == "POLY") return cv::ml::SVM::POLY;
    if (nom == "RBF") return cv::ml::SVM::RBF;
    if (nom == "SIGMOIDE") return cv::ml::SVM::SIGMOID;
    return -1;
}

const char* NomNoyau(int type) {
    switch (type) {
    case cv::ml::SVM::LINEAR: return "LINEAIRE";
    case cv::ml::SVM::POLY: return "POLY";
    case cv::ml::SVM::RBF: return "RBF";
    case cv::ml::SVM::SIGMOID: return "SIGMOIDE";
    default: return "AUTRE";
    }
}

void RelireParametresSvm(Modele& m) {
    auto svm = m.m_opencv.dynamicCast<cv::ml::SVM>();
    ValeurJson e = ValeurJson::Objet();
    e["noyau"] = NomNoyau(svm->getKernelType());
    e["c"] = svm->getC();
    e["gamma"] = svm->getGamma();
    e["degre"] = svm->getDegree();
    e["coef0"] = svm->getCoef0();
    e["nb_vecteurs_support"] = svm->getSupportVectors().rows;
    e["critere_arret"] = CritereVersJson(svm->getTermCriteria());
    e["nb_attributs"] = svm->getVarCount();
    m.m_parametresEffectifs = e;
}

void RelireParametresForet(Modele& m) {
    auto rf = m.m_opencv.dynamicCast<cv::ml::RTrees>();
    ValeurJson e = ValeurJson::Objet();
    e["nb_arbres"] = (long long)rf->getRoots().size();
    e["profondeur_max"] = rf->getMaxDepth();
    e["min_echantillons"] = rf->getMinSampleCount();
    e["nb_variables_actives"] = rf->getActiveVarCount();
    e["critere_arret"] = CritereVersJson(rf->getTermCriteria());
    e["nb_attributs"] = rf->getVarCount();
    m.m_parametresEffectifs = e;
}

} // namespace

Resultat<Modele> EntrainerSvm(const cv::Mat& x, const std::vector<int>& labels, int nbClasses,
                              const ParametresSvm& p) {
    auto verif = VerifierEntrees(x, labels, nbClasses, "apprentissage_svm");
    if (!verif.Ok()) return verif.Err();
    int noyau = NoyauOpenCv(p.m_noyau);
    if (noyau < 0) return FaireErreur(CodeErreur::DonneesInvalides, "apprentissage_svm", "noyau inconnu : " + p.m_noyau);
    if (!(p.m_c > 0) || !(p.m_gamma > 0) || !std::isfinite(p.m_c) || !std::isfinite(p.m_gamma))
        return FaireErreur(CodeErreur::DonneesInvalides, "apprentissage_svm", "C et gamma doivent etre finis et > 0");
    if (noyau == cv::ml::SVM::POLY && !(p.m_degre > 0))
        return FaireErreur(CodeErreur::DonneesInvalides, "apprentissage_svm", "degre du noyau polynomial doit etre > 0");

    ValeurJson d = ValeurJson::Objet();
    d["noyau"] = p.m_noyau; d["c"] = p.m_c; d["gamma"] = p.m_gamma; d["degre"] = p.m_degre; d["coef0"] = p.m_coef0;
    Modele m = ModeleDeBase(Famille::Svm, x, nbClasses, d);
    try {
        auto svm = cv::ml::SVM::create();
        svm->setType(cv::ml::SVM::C_SVC);
        svm->setKernel(noyau);
        svm->setC(p.m_c);
        if (noyau != cv::ml::SVM::LINEAR) svm->setGamma(p.m_gamma);
        if (noyau == cv::ml::SVM::POLY) svm->setDegree(p.m_degre);
        if (noyau == cv::ml::SVM::POLY || noyau == cv::ml::SVM::SIGMOID) svm->setCoef0(p.m_coef0);
        if (!svm->train(x, cv::ml::ROW_SAMPLE, LabelsOpenCv(labels)))
            return FaireErreur(CodeErreur::Calcul, "apprentissage_svm", "OpenCV a refuse l'apprentissage SVM");
        m.m_opencv = svm;
    } catch (const cv::Exception& e) {
        return ErreurCv("apprentissage_svm", e);
    }
    RelireParametresSvm(m);
    return m;
}

Resultat<Modele> EntrainerForet(const cv::Mat& x, const std::vector<int>& labels, int nbClasses,
                                const ParametresForet& p, std::uint64_t graine) {
    auto verif = VerifierEntrees(x, labels, nbClasses, "apprentissage_foret");
    if (!verif.Ok()) return verif.Err();
    if (p.m_nbArbres < 1 || p.m_nbArbres > 10000 || p.m_profondeurMax < 1 || p.m_profondeurMax > 64 ||
        p.m_minEchantillons < 1 || p.m_nbVariablesActives < 0 || p.m_nbVariablesActives > x.cols)
        return FaireErreur(CodeErreur::DonneesInvalides, "apprentissage_foret", "parametres de foret hors bornes");

    ValeurJson d = ValeurJson::Objet();
    d["nb_arbres"] = p.m_nbArbres; d["profondeur_max"] = p.m_profondeurMax;
    d["min_echantillons"] = p.m_minEchantillons; d["nb_variables_actives"] = p.m_nbVariablesActives;
    d["graine"] = (unsigned long long)graine;
    Modele m = ModeleDeBase(Famille::ForetAleatoire, x, nbClasses, d);
    try {
        cv::theRNG() = cv::RNG(graine);   // RNG local au thread : celui qui calcule
        auto rf = cv::ml::RTrees::create();
        rf->setMaxDepth(p.m_profondeurMax);
        rf->setMinSampleCount(p.m_minEchantillons);
        rf->setActiveVarCount(p.m_nbVariablesActives);
        rf->setCalculateVarImportance(true);
        rf->setUseSurrogates(false);
        rf->setTermCriteria(cv::TermCriteria(cv::TermCriteria::MAX_ITER, p.m_nbArbres, 0.0));
        if (!rf->train(DonneesClassification(x, labels)))
            return FaireErreur(CodeErreur::Calcul, "apprentissage_foret", "OpenCV a refuse l'apprentissage RF");
        m.m_opencv = rf;
    } catch (const cv::Exception& e) {
        return ErreurCv("apprentissage_foret", e);
    }
    RelireParametresForet(m);
    return m;
}

// ====================================================================== KPPV

namespace {

// Echantillons et reponses embarques dans le modele KPPV (OpenCV n'a pas de getter : lus dans son YAML)
Statut ExtraireKppv(const ModelePtr& knn, cv::Mat& echantillons, cv::Mat& reponses) {
    try {
        cv::FileStorage flux(YamlDe(knn), cv::FileStorage::READ | cv::FileStorage::MEMORY);
        cv::FileNode noeud = flux.getFirstTopLevelNode();
        noeud["samples"] >> echantillons;
        noeud["responses"] >> reponses;
    } catch (const cv::Exception& e) {
        return ErreurCv("inspection_kppv", e);
    }
    if (echantillons.empty() || echantillons.type() != CV_32F || reponses.total() != (size_t)echantillons.rows)
        return FaireErreur(CodeErreur::ModeleIncompatible, "inspection_kppv", "echantillons embarques illisibles");
    return Succes();
}

void RelireParametresKppv(Modele& m, int nbEchantillons) {
    auto knn = m.m_opencv.dynamicCast<cv::ml::KNearest>();
    ValeurJson e = ValeurJson::Objet();
    e["k"] = knn->getDefaultK();
    e["nb_echantillons"] = nbEchantillons;
    e["nb_attributs"] = knn->getVarCount();
    e["algorithme"] = knn->getAlgorithmType() == cv::ml::KNearest::KDTREE ? "KDTREE" : "FORCE_BRUTE";
    e["distance"] = "euclidienne (carree en float dans OpenCV)";
    e["regle_vote"] = "classe la plus frequente parmi les k voisins ; egalite -> plus petit id de classe";
    e["regle_egalite_distance"] = "voisin d'indice d'apprentissage le plus petit conserve";
    m.m_parametresEffectifs = e;
}

} // namespace

Resultat<Modele> EntrainerKppv(const cv::Mat& x, const std::vector<int>& labels, int nbClasses,
                               const ParametresKppv& p) {
    auto verif = VerifierEntrees(x, labels, nbClasses, "apprentissage_kppv");
    if (!verif.Ok()) return verif.Err();
    if (p.m_k < 1 || p.m_k > x.rows)
        return FaireErreur(CodeErreur::DonneesInvalides, "apprentissage_kppv",
            "k = " + std::to_string(p.m_k) + " refuse : k doit etre entre 1 et le nombre d'echantillons d'apprentissage ("
            + std::to_string(x.rows) + "), OpenCV rabattrait k en silence");
    ValeurJson d = ValeurJson::Objet();
    d["k"] = p.m_k;
    Modele m = ModeleDeBase(Famille::Kppv, x, nbClasses, d);
    try {
        auto knn = cv::ml::KNearest::create();
        knn->setAlgorithmType(cv::ml::KNearest::BRUTE_FORCE);
        knn->setIsClassifier(true);
        knn->setDefaultK(p.m_k);
        if (!knn->train(x, cv::ml::ROW_SAMPLE, LabelsOpenCv(labels)))
            return FaireErreur(CodeErreur::Calcul, "apprentissage_kppv", "OpenCV a refuse l'apprentissage KPPV");
        m.m_opencv = knn;
    } catch (const cv::Exception& e) {
        return ErreurCv("apprentissage_kppv", e);
    }
    RelireParametresKppv(m, x.rows);
    return m;
}

namespace {

// Meme arithmetique float et meme ordre de sommation que BruteForceImpl::findNearestCore
float DistanceCarreeOpenCv(const float* u, const float* v, int d) {
    float s = 0;
    int i = 0;
    for (; i <= d - 4; i += 4) {
        float t0 = u[i] - v[i], t1 = u[i + 1] - v[i + 1];
        float t2 = u[i + 2] - v[i + 2], t3 = u[i + 3] - v[i + 3];
        s += t0 * t0 + t1 * t1 + t2 * t2 + t3 * t3;
    }
    for (; i < d; i++) {
        float t0 = u[i] - v[i];
        s += t0 * t0;
    }
    return s;
}

// Plus grand comptage ; egalite -> plus petit id (ordre croissant, comparaison stricte)
int DecisionParVote(const std::vector<int>& vote) {
    int meilleur = 0;
    for (size_t c = 1; c < vote.size(); ++c)
        if (vote[c] > vote[(size_t)meilleur]) meilleur = (int)c;
    return meilleur;
}

bool DistancesCoherentes(float a, float b) {
    return std::fabs((double)a - (double)b) <= 1e-5 * std::max(1.0, std::fabs((double)b));
}

Statut ControlerCoherenceKppv(const VoisinsKppv& v, const std::vector<float>& carrees, const cv::Mat& reponsesCv,
                              const cv::Mat& distancesCv, float decisionCv) {
    const std::string phase = "inspection_kppv";
    std::vector<int> classesRetrouvees = v.m_classes, cvClasses;
    for (size_t i = 0; i < carrees.size(); ++i) {
        if (!DistancesCoherentes(carrees[i], distancesCv.at<float>(0, (int)i)))
            return FaireErreur(CodeErreur::Calcul, phase, "distance recalculee incoherente avec findNearest au voisin "
                               + std::to_string(i + 1));
        cvClasses.push_back((int)std::lround(reponsesCv.at<float>(0, (int)i)));
    }
    std::sort(classesRetrouvees.begin(), classesRetrouvees.end());
    std::sort(cvClasses.begin(), cvClasses.end());
    if (classesRetrouvees != cvClasses)
        return FaireErreur(CodeErreur::Calcul, phase, "classes des voisins retrouves differentes de celles d'OpenCV");
    if (v.m_decision != (int)std::lround(decisionCv))
        return FaireErreur(CodeErreur::Calcul, phase, "vote recalcule different de la decision d'OpenCV");
    return Succes();
}

} // namespace

Resultat<VoisinsKppv> VoisinsKppvPour(const Modele& m, const float* echantillon) {
    if (m.m_famille != Famille::Kppv || !m.m_opencv)
        return FaireErreur(CodeErreur::ModeleIncompatible, "inspection_kppv", "modele non KPPV");
    cv::Mat apprentissage, reponses;
    Statut st = ExtraireKppv(m.m_opencv, apprentissage, reponses);
    if (!st.Ok()) return st.Err();
    auto knn = m.m_opencv.dynamicCast<cv::ml::KNearest>();
    const int k = knn->getDefaultK(), n = apprentissage.rows, d = apprentissage.cols;
    cv::Mat ligne(1, d, CV_32F);
    std::copy(echantillon, echantillon + d, ligne.ptr<float>());

    std::vector<std::pair<float, int>> distances((size_t)n);
    for (int j = 0; j < n; ++j) distances[(size_t)j] = {DistanceCarreeOpenCv(ligne.ptr<float>(), apprentissage.ptr<float>(j), d), j};
    std::sort(distances.begin(), distances.end());          // (distance, indice) : egalite -> plus petit indice
    VoisinsKppv v;
    std::vector<float> carrees;
    v.m_vote.assign((size_t)m.m_nbClasses, 0);
    for (int i = 0; i < k; ++i) {
        const auto& dj = distances[(size_t)i];
        int classe = (int)std::lround(reponses.at<float>(dj.second));
        if (classe < 0 || classe >= m.m_nbClasses)
            return FaireErreur(CodeErreur::ModeleIncompatible, "inspection_kppv", "classe embarquee hors plage");
        v.m_indices.push_back(dj.second);
        v.m_distances.push_back(std::sqrt((double)dj.first));
        v.m_classes.push_back(classe);
        carrees.push_back(dj.first);
        ++v.m_vote[(size_t)classe];
    }
    v.m_decision = DecisionParVote(v.m_vote);
    try {
        cv::Mat resultat, reponsesCv, distancesCv;
        float decisionCv = knn->findNearest(ligne, k, resultat, reponsesCv, distancesCv);
        st = ControlerCoherenceKppv(v, carrees, reponsesCv, distancesCv, decisionCv);
        if (!st.Ok()) return st.Err();
    } catch (const cv::Exception& e) {
        return ErreurCv("inspection_kppv", e);
    }
    return v;
}

// ====================================================================== Bayes normal

namespace {

struct StatsClasse { int m_classe = 0, m_effectif = 0; std::vector<double> m_moyenne, m_covariance; };

StatsClasse StatsDeClasse(const cv::Mat& x, const std::vector<int>& labels, int classe) {
    StatsClasse s;
    s.m_classe = classe;
    const int p = x.cols;
    s.m_moyenne.assign((size_t)p, 0.0);
    for (int i = 0; i < x.rows; ++i) {
        if (labels[(size_t)i] != classe) continue;
        ++s.m_effectif;
        for (int a = 0; a < p; ++a) s.m_moyenne[(size_t)a] += x.at<float>(i, a);
    }
    if (s.m_effectif == 0) return s;
    for (double& v : s.m_moyenne) v /= s.m_effectif;
    s.m_covariance.assign((size_t)p * (size_t)p, 0.0);
    for (int i = 0; i < x.rows; ++i) {
        if (labels[(size_t)i] != classe) continue;
        for (int a = 0; a < p; ++a)
            for (int b = 0; b < p; ++b)
                s.m_covariance[(size_t)a * (size_t)p + (size_t)b] +=
                    (x.at<float>(i, a) - s.m_moyenne[(size_t)a]) * (x.at<float>(i, b) - s.m_moyenne[(size_t)b]);
    }
    const double diviseur = s.m_effectif > 1 ? s.m_effectif - 1 : 1;   // meme convention qu'OpenCV (n = 1 : nulle)
    for (double& v : s.m_covariance) v /= diviseur;
    return s;
}

ValeurJson CovarianceVersJson(const std::vector<double>& cov, int p) {
    ValeurJson lignes = ValeurJson::Tableau();
    for (int a = 0; a < p; ++a) {
        ValeurJson ligne = ValeurJson::Tableau();
        for (int b = 0; b < p; ++b) ligne.Ajouter(cov[(size_t)a * (size_t)p + (size_t)b]);
        lignes.Ajouter(ligne);
    }
    return lignes;
}

ValeurJson RecalculVersJson(const std::vector<StatsClasse>& stats, int p, const char* source) {
    ValeurJson j = ValeurJson::Objet();
    j["source"] = source;
    j["recalcul_decid"] = true;
    j["note"] = "moyennes et covariances recalculees par Decid (OpenCV ne les expose pas) ; covariance sans biais (n-1)";
    j["a_priori"] = "egaux : OpenCV n'utilise aucun a priori de classe";
    j["regle_decision"] = "argmin_c [ ln det(S_c) + (x-m_c)' S_c^-1 (x-m_c) ] = argmax de la log-vraisemblance gaussienne";
    ValeurJson classes = ValeurJson::Tableau();
    for (const StatsClasse& s : stats) {
        ValeurJson c = ValeurJson::Objet();
        c["classe"] = s.m_classe;
        c["effectif"] = s.m_effectif;
        c["moyenne"] = VecteurVersJson(s.m_moyenne);
        c["covariance"] = CovarianceVersJson(s.m_covariance, p);
        classes.Ajouter(c);
    }
    j["classes"] = classes;
    return j;
}

ValeurJson RecalculerGaussiennes(const cv::Mat& x, const std::vector<int>& labels, int nbClasses) {
    std::vector<StatsClasse> stats;
    for (int c = 0; c < nbClasses; ++c) {
        StatsClasse s = StatsDeClasse(x, labels, c);
        if (s.m_effectif > 0) stats.push_back(std::move(s));
    }
    return RecalculVersJson(stats, x.cols, "recalcul sur les donnees d'apprentissage");
}

std::vector<cv::Mat> LireSequenceMatrices(const cv::FileNode& noeud) {
    std::vector<cv::Mat> v;
    for (auto it = noeud.begin(); it != noeud.end(); ++it) {
        cv::Mat a;
        (*it) >> a;
        v.push_back(a);
    }
    return v;
}

// Rechargement : meme formule qu'OpenCV a partir de count, sum, productsum et avg sauvegardes dans le fichier
ValeurJson RecalculDepuisEtatOpenCv(const cv::FileNode& racine, int& nbClassesApprises) {
    cv::Mat etiquettes;
    racine["cls_labels"] >> etiquettes;
    std::vector<cv::Mat> compte = LireSequenceMatrices(racine["count"]), somme = LireSequenceMatrices(racine["sum"]);
    std::vector<cv::Mat> produits = LireSequenceMatrices(racine["productsum"]), moyennes = LireSequenceMatrices(racine["avg"]);
    nbClassesApprises = (int)etiquettes.total();
    std::vector<StatsClasse> stats;
    int p = 0;
    for (int c = 0; c < nbClassesApprises && (size_t)c < moyennes.size(); ++c) {
        StatsClasse s;
        s.m_classe = etiquettes.at<int>(c);
        s.m_effectif = compte[(size_t)c].at<int>(0);
        p = moyennes[(size_t)c].cols;
        for (int a = 0; a < p; ++a) s.m_moyenne.push_back(moyennes[(size_t)c].at<double>(a));
        const double diviseur = s.m_effectif > 1 ? s.m_effectif - 1 : 1;
        for (int a = 0; a < p; ++a)
            for (int b = 0; b < p; ++b) {
                double ma = s.m_moyenne[(size_t)a], mb = s.m_moyenne[(size_t)b];
                double v = produits[(size_t)c].at<double>(a, b) - ma * somme[(size_t)c].at<double>(b)
                           - mb * somme[(size_t)c].at<double>(a) + ma * mb * s.m_effectif;
                s.m_covariance.push_back(v / diviseur);
            }
        stats.push_back(std::move(s));
    }
    return RecalculVersJson(stats, p, "reconstruit depuis l'etat OpenCV du fichier modele");
}

void RelireParametresBayes(Modele& m, int nbClassesApprises) {
    ValeurJson e = ValeurJson::Objet();
    e["nb_classes_apprises"] = nbClassesApprises;
    e["nb_attributs"] = m.m_opencv->getVarCount();
    e["covariance"] = "pleine, une par classe (estimateur sans biais n-1, valeurs propres >= FLT_EPSILON)";
    e["a_priori"] = "egaux (aucun a priori de classe dans OpenCV)";
    m.m_parametresEffectifs = e;
}

} // namespace

Resultat<Modele> EntrainerBayes(const cv::Mat& x, const std::vector<int>& labels, int nbClasses) {
    auto verif = VerifierEntrees(x, labels, nbClasses, "apprentissage_bayes");
    if (!verif.Ok()) return verif.Err();
    Modele m = ModeleDeBase(Famille::BayesNormal, x, nbClasses, ValeurJson::Objet());
    try {
        auto bayes = cv::ml::NormalBayesClassifier::create();
        if (!bayes->train(DonneesClassification(x, labels)))
            return FaireErreur(CodeErreur::Calcul, "apprentissage_bayes", "OpenCV a refuse l'apprentissage Bayes");
        m.m_opencv = bayes;
    } catch (const cv::Exception& e) {
        return ErreurCv("apprentissage_bayes", e);
    }
    m.m_recalcul = RecalculerGaussiennes(x, labels, nbClasses);
    RelireParametresBayes(m, (int)m.m_recalcul.Membre("classes").Elements().size());
    return m;
}

// ====================================================================== MLP

namespace {

int ActivationOpenCv(const std::string& nom) {
    if (nom == "SIGMOIDE_SYM") return cv::ml::ANN_MLP::SIGMOID_SYM;
    if (nom == "RELU") return cv::ml::ANN_MLP::RELU;
    if (nom == "IDENTITE") return cv::ml::ANN_MLP::IDENTITY;
    return -1;
}

Statut VerifierMlp(const ParametresMlp& p, const char* phase) {
    auto refus = [&](const std::string& message) { return FaireErreur(CodeErreur::DonneesInvalides, phase, message); };
    if (ActivationOpenCv(p.m_activation) < 0) return refus("activation inconnue : " + p.m_activation + " (SIGMOIDE_SYM, RELU, IDENTITE)");
    const bool retro = p.m_methode == "RETROPROPAGATION";
    if (!retro && p.m_methode != "RPROP") return refus("methode inconnue : " + p.m_methode + " (RETROPROPAGATION, RPROP)");
    if (p.m_couchesCachees.size() > 8) return refus("au plus 8 couches cachees");
    for (int n : p.m_couchesCachees)
        if (n < 2 || n > 4096) return refus("chaque couche cachee doit avoir entre 2 et 4096 neurones (contrainte OpenCV)");
    if (retro && !(p.m_eta >= 1e-3 && p.m_eta <= 1.0))
        return refus("eta doit etre dans [0.001, 1] (OpenCV borne eta en silence)");
    if (retro && !(p.m_moment >= 0.0 && p.m_moment <= 1.0)) return refus("moment doit etre dans [0, 1]");
    if (p.m_iterationsMax < 1 || p.m_iterationsMax > 100000) return refus("iterations_max entre 1 et 100000");
    if (!(p.m_epsilon > 0) || !std::isfinite(p.m_epsilon)) return refus("epsilon doit etre fini et > 0");
    return Succes();
}

// Cibles one-hot 0/1. Sigmoide symetrique : OpenCV remappe [min,max] des cibles vers +-0.95 puis inverse a la
// prediction (sorties relues dans l'echelle 0/1). RELU et IDENTITE : cibles 0/1 directes, sans mise a l'echelle.
cv::Mat CiblesMlp(const std::vector<int>& labels, int nbClasses) {
    cv::Mat cibles = cv::Mat::zeros((int)labels.size(), nbClasses, CV_32F);
    for (size_t i = 0; i < labels.size(); ++i) cibles.at<float>((int)i, labels[i]) = 1.0f;
    return cibles;
}

int IndicateursSortie(const ParametresMlp& p) {
    return p.m_activation == "SIGMOIDE_SYM" ? 0 : (int)cv::ml::ANN_MLP::NO_OUTPUT_SCALE;
}

cv::Ptr<cv::ml::ANN_MLP> CreerReseau(const ParametresMlp& p, int nbAttributs, int nbClasses) {
    std::vector<int> couches{nbAttributs};
    couches.insert(couches.end(), p.m_couchesCachees.begin(), p.m_couchesCachees.end());
    couches.push_back(nbClasses);
    auto ann = cv::ml::ANN_MLP::create();
    ann->setLayerSizes(cv::Mat(couches, true));
    ann->setActivationFunction(ActivationOpenCv(p.m_activation), 0, 0);
    if (p.m_methode == "RETROPROPAGATION") ann->setTrainMethod(cv::ml::ANN_MLP::BACKPROP, p.m_eta, p.m_moment);
    else ann->setTrainMethod(cv::ml::ANN_MLP::RPROP, 0.1, FLT_EPSILON);
    ann->setTermCriteria(cv::TermCriteria(cv::TermCriteria::COUNT + cv::TermCriteria::EPS, p.m_iterationsMax, p.m_epsilon));
    return ann;
}

std::string ActivationRelue(const ModelePtr& ann) {
    cv::FileStorage flux(YamlDe(ann), cv::FileStorage::READ | cv::FileStorage::MEMORY);
    std::string nom = (std::string)flux.getFirstTopLevelNode()["activation_function"];
    if (nom == "SIGMOID_SYM") return "SIGMOIDE_SYM";
    if (nom == "IDENTITY") return "IDENTITE";
    return nom.empty() ? "inconnue" : nom;
}

void RelireParametresMlp(Modele& m) {
    auto ann = m.m_opencv.dynamicCast<cv::ml::ANN_MLP>();
    ValeurJson e = ValeurJson::Objet();
    cv::Mat tailles = ann->getLayerSizes();
    ValeurJson couches = ValeurJson::Tableau();
    for (int i = 0; i < (int)tailles.total(); ++i) couches.Ajouter(tailles.at<int>(i));
    e["couches"] = couches;
    e["activation"] = ActivationRelue(m.m_opencv);
    if (ann->getTrainMethod() == cv::ml::ANN_MLP::BACKPROP) {
        e["methode"] = "RETROPROPAGATION";
        e["eta"] = ann->getBackpropWeightScale();
        e["moment"] = ann->getBackpropMomentumScale();
    } else {
        e["methode"] = "RPROP";
        e["rprop_dw0"] = ann->getRpropDW0(); e["rprop_dw_plus"] = ann->getRpropDWPlus();
        e["rprop_dw_moins"] = ann->getRpropDWMinus(); e["rprop_dw_min"] = ann->getRpropDWMin();
        e["rprop_dw_max"] = ann->getRpropDWMax();
    }
    e["critere_arret"] = CritereVersJson(ann->getTermCriteria());
    e["nb_attributs"] = ann->getVarCount();
    e["graine"] = "sans effet : le RNG interne d'OpenCV est fixe";
    m.m_parametresEffectifs = e;
}

Statut EntrainerReseau(Modele& m, const cv::Mat& x, const std::vector<int>& labels, const ParametresMlp& p) {
    auto ann = CreerReseau(p, x.cols, m.m_nbClasses);
    auto donnees = cv::ml::TrainData::create(x, cv::ml::ROW_SAMPLE, CiblesMlp(labels, m.m_nbClasses));
    if (!ann->train(donnees, IndicateursSortie(p)))
        return FaireErreur(CodeErreur::Calcul, "apprentissage_mlp", "OpenCV a refuse l'apprentissage du MLP");
    m.m_opencv = ann;
    return Succes();
}

ValeurJson DemandesDe(const ParametresMlp& p) {
    ParametresAlgorithme a;
    a.m_famille = Famille::Mlp;
    a.m_mlp = p;
    return a.VersJson();
}

} // namespace

Resultat<Modele> EntrainerMlp(const cv::Mat& x, const std::vector<int>& labels, int nbClasses, const ParametresMlp& p) {
    auto verif = VerifierEntrees(x, labels, nbClasses, "apprentissage_mlp");
    if (!verif.Ok()) return verif.Err();
    verif = VerifierMlp(p, "apprentissage_mlp");
    if (!verif.Ok()) return verif.Err();
    Modele m = ModeleDeBase(Famille::Mlp, x, nbClasses, DemandesDe(p));
    try {
        Statut st = EntrainerReseau(m, x, labels, p);
        if (!st.Ok()) return st.Err();
    } catch (const cv::Exception& e) {
        return ErreurCv("apprentissage_mlp", e);
    }
    RelireParametresMlp(m);
    m.m_parametresEffectifs["iterations_demandees"] = p.m_iterationsMax;
    m.m_parametresEffectifs["iterations_effectuees"] = ValeurJson();   // null : train() ne renvoie qu'un booleen
    m.m_parametresEffectifs["iterations_effectuees_note"] =
        "non exposees par cv::ml::ANN_MLP::train (booleen) ; connues exactement en mode pas a pas";
    return m;
}

namespace {

struct MesuresMlp { double m_perte = 0, m_exactitude = 0; };

Resultat<MesuresMlp> MesurerMlp(const ModelePtr& ann, const cv::Mat& x, const std::vector<int>& labels, int nbClasses) {
    cv::Mat sorties;
    ann->predict(x, sorties);
    MesuresMlp r;
    int bons = 0;
    for (int i = 0; i < x.rows; ++i) {
        const float* s = sorties.ptr<float>(i);
        int classe = ArgMaxLigne(s, nbClasses);
        if (classe < 0) return FaireErreur(CodeErreur::Calcul, "apprentissage_mlp", "sortie non finie du reseau (divergence)");
        bons += classe == labels[(size_t)i] ? 1 : 0;
        for (int c = 0; c < nbClasses; ++c) {
            double ecart = (c == labels[(size_t)i] ? 1.0 : 0.0) - s[c];
            r.m_perte += 0.5 * ecart * ecart;
        }
    }
    r.m_perte /= std::max(1, x.rows);
    r.m_exactitude = (double)bons / std::max(1, x.rows);
    return r;
}

std::vector<float> PoidsConcatenes(const cv::Ptr<cv::ml::ANN_MLP>& ann) {
    std::vector<float> v;
    const int nbCouches = (int)ann->getLayerSizes().total();
    for (int i = 1; i < nbCouches; ++i) {
        cv::Mat w = ann->getWeights(i);
        for (int r = 0; r < w.rows; ++r)
            for (int c = 0; c < w.cols; ++c) v.push_back((float)w.at<double>(r, c));
    }
    return v;
}

EtatTrace EtatEpoque(int epoque, const MesuresMlp& app, const Resultat<MesuresMlp>* val,
                     const cv::Ptr<cv::ml::ANN_MLP>& ann) {
    EtatTrace e;
    e.m_iteration = epoque;
    e.m_phase = "epoque";
    e.m_grandeurs = {{"epoque", (double)epoque}, {"perte_apprentissage", app.m_perte},
                     {"exactitude_apprentissage", app.m_exactitude}};
    if (val) e.m_grandeurs.push_back({"exactitude_validation", val->Valeur().m_exactitude});
    e.m_vecteur = PoidsConcatenes(ann);
    return e;
}

Statut VerifierValidationMlp(const cv::Mat& xv, const std::vector<int>& lv, int colonnes, int nbClasses) {
    if (xv.empty()) return Succes();
    if (xv.type() != CV_32F || xv.cols != colonnes || xv.rows != (int)lv.size())
        return FaireErreur(CodeErreur::DonneesInvalides, "apprentissage_mlp", "jeu de validation incoherent");
    for (int l : lv)
        if (l < 0 || l >= nbClasses)
            return FaireErreur(CodeErreur::DonneesInvalides, "apprentissage_mlp", "label de validation hors plage");
    return Succes();
}

// Une epoque par appel : la premiere SANS UPDATE_WEIGHTS (initialise poids et echelles), les suivantes AVEC.
// Chaque appel repart d'un moment nul (retropropagation) ou d'un pas RPROP initial (dw0) : seule l'etat des poids
// est conserve d'une epoque a l'autre.
Statut BoucleEpoques(Modele& m, const cv::Mat& x, const std::vector<int>& labels, const ParametresMlp& p,
                     const cv::Mat& xv, const std::vector<int>& lv, ObservateurTrace* observateur, int& effectuees) {
    auto ann = CreerReseau(p, x.cols, m.m_nbClasses);
    ann->setTermCriteria(cv::TermCriteria(cv::TermCriteria::COUNT, 1, 0.0));
    auto donnees = cv::ml::TrainData::create(x, cv::ml::ROW_SAMPLE, CiblesMlp(labels, m.m_nbClasses));
    double precedente = 0;
    for (int epoque = 1; epoque <= p.m_iterationsMax; ++epoque) {
        int indicateurs = IndicateursSortie(p) | (epoque > 1 ? (int)cv::ml::ANN_MLP::UPDATE_WEIGHTS : 0);
        if (!ann->train(donnees, indicateurs))
            return FaireErreur(CodeErreur::Calcul, "apprentissage_mlp", "OpenCV a refuse l'epoque " + std::to_string(epoque));
        auto app = MesurerMlp(ann, x, labels, m.m_nbClasses);
        if (!app.Ok()) return app.Err();
        Resultat<MesuresMlp> val = MesuresMlp{};
        if (!xv.empty()) val = MesurerMlp(ann, xv, lv, m.m_nbClasses);
        if (!val.Ok()) return val.Err();
        effectuees = epoque;
        if (observateur) observateur->Recevoir(EtatEpoque(epoque, app.Valeur(), xv.empty() ? nullptr : &val, ann));
        if (epoque > 1 && std::fabs(precedente - app.Valeur().m_perte) < p.m_epsilon) break;
        precedente = app.Valeur().m_perte;
    }
    ann->setTermCriteria(cv::TermCriteria(cv::TermCriteria::COUNT + cv::TermCriteria::EPS, p.m_iterationsMax, p.m_epsilon));
    m.m_opencv = ann;
    return Succes();
}

} // namespace

Resultat<Modele> EntrainerMlpPasAPas(const cv::Mat& x, const std::vector<int>& labels, int nbClasses,
                                     const ParametresMlp& p, const cv::Mat& xValidation,
                                     const std::vector<int>& labelsValidation, std::uint64_t graine,
                                     ObservateurTrace* observateur) {
    (void)graine;   // le RNG de l'ANN_MLP est interne a OpenCV (fixe) : la graine n'a aucun effet
    auto verif = VerifierEntrees(x, labels, nbClasses, "apprentissage_mlp");
    if (!verif.Ok()) return verif.Err();
    if (!(verif = VerifierMlp(p, "apprentissage_mlp")).Ok()) return verif.Err();
    if (!(verif = VerifierValidationMlp(xValidation, labelsValidation, x.cols, nbClasses)).Ok()) return verif.Err();
    Modele m = ModeleDeBase(Famille::Mlp, x, nbClasses, DemandesDe(p));
    int effectuees = 0;
    try {
        Statut st = BoucleEpoques(m, x, labels, p, xValidation, labelsValidation, observateur, effectuees);
        if (!st.Ok()) return st.Err();
    } catch (const cv::Exception& e) {
        return ErreurCv("apprentissage_mlp", e);
    }
    RelireParametresMlp(m);
    m.m_parametresEffectifs["iterations_demandees"] = p.m_iterationsMax;
    m.m_parametresEffectifs["iterations_effectuees"] = effectuees;
    m.m_parametresEffectifs["mode"] = "pas a pas : une epoque par appel a train (UPDATE_WEIGHTS apres la premiere)";
    return m;
}

// ====================================================================== EM

namespace {

int CovarianceOpenCv(const std::string& nom) {
    if (nom == "SPHERIQUE") return cv::ml::EM::COV_MAT_SPHERICAL;
    if (nom == "DIAGONALE") return cv::ml::EM::COV_MAT_DIAGONAL;
    if (nom == "GENERIQUE") return cv::ml::EM::COV_MAT_GENERIC;
    return -1;
}

const char* NomCovariance(int type) {
    switch (type) {
    case cv::ml::EM::COV_MAT_SPHERICAL: return "SPHERIQUE";
    case cv::ml::EM::COV_MAT_DIAGONAL: return "DIAGONALE";
    case cv::ml::EM::COV_MAT_GENERIC: return "GENERIQUE";
    default: return "AUTRE";
    }
}

// K dans [2, min(20, N)] : JAMAIS de rabattement (bug historique : K demande 21 devenait silencieusement autre chose)
Statut VerifierEm(const ParametresEm& p, int nbLignes, const char* phase) {
    auto refus = [&](const std::string& message) { return FaireErreur(CodeErreur::DonneesInvalides, phase, message); };
    const int kMax = std::min(20, nbLignes);
    if (p.m_k < 2 || p.m_k > kMax)
        return refus("K = " + std::to_string(p.m_k) + " refuse : K doit etre entre 2 et min(20, N) = " +
                     std::to_string(kMax) + " (aucun rabattement automatique)");
    if (CovarianceOpenCv(p.m_covariance) < 0) return refus("covariance inconnue : " + p.m_covariance + " (SPHERIQUE, DIAGONALE, GENERIQUE)");
    if (p.m_iterationsMax < 1 || p.m_iterationsMax > 100000) return refus("iterations_max entre 1 et 100000");
    if (!(p.m_epsilon >= 0) || !std::isfinite(p.m_epsilon)) return refus("epsilon doit etre fini et >= 0");
    return Succes();
}

cv::Ptr<cv::ml::EM> CreerEm(const ParametresEm& p) {
    auto em = cv::ml::EM::create();
    em->setClustersNumber(p.m_k);
    em->setCovarianceMatrixType(CovarianceOpenCv(p.m_covariance));
    em->setTermCriteria(cv::TermCriteria(cv::TermCriteria::COUNT + cv::TermCriteria::EPS, p.m_iterationsMax, p.m_epsilon));
    return em;
}

std::vector<int> ClustersDe(const cv::Ptr<cv::ml::EM>& em, const cv::Mat& x) {
    std::vector<int> clusters((size_t)x.rows);
    for (int i = 0; i < x.rows; ++i) {
        cv::Mat ligne;
        x.row(i).convertTo(ligne, CV_64F);
        cv::Vec2d r = em->predict2(ligne, cv::noArray());
        clusters[(size_t)i] = (int)std::lround(r[1]);
    }
    return clusters;
}

void RelireParametresEm(Modele& m) {
    auto em = m.m_opencv.dynamicCast<cv::ml::EM>();
    ValeurJson e = ValeurJson::Objet();
    e["k"] = em->getClustersNumber();
    e["covariance"] = NomCovariance(em->getCovarianceMatrixType());
    e["critere_arret"] = CritereVersJson(em->getTermCriteria());
    e["nb_attributs"] = em->getVarCount();
    e["correspondance_clusters"] = EntiersVersJson(m.m_correspondanceClusters);
    int nonAssocies = 0;
    for (int c : m.m_correspondanceClusters) nonAssocies += c < 0 ? 1 : 0;
    e["nb_clusters_non_associes"] = nonAssocies;
    e["regle_correspondance"] = "classe majoritaire des lignes d'apprentissage du cluster ; egalite -> plus petit id ; vide -> -1";
    m.m_parametresEffectifs = e;
}

void FinaliserEm(Modele& m, const cv::Mat& x, const std::vector<int>& labels) {
    auto em = m.m_opencv.dynamicCast<cv::ml::EM>();
    m.m_correspondanceClusters = CorrespondanceClusters(ClustersDe(em, x), labels, em->getClustersNumber(), m.m_nbClasses);
    RelireParametresEm(m);
}

} // namespace

std::vector<int> CorrespondanceClusters(const std::vector<int>& clusters, const std::vector<int>& labels,
                                        int nbClusters, int nbClasses) {
    std::vector<std::vector<int>> comptes((size_t)std::max(nbClusters, 0), std::vector<int>((size_t)std::max(nbClasses, 0), 0));
    for (size_t i = 0; i < clusters.size() && i < labels.size(); ++i) {
        int k = clusters[i], c = labels[i];
        if (k >= 0 && k < nbClusters && c >= 0 && c < nbClasses) ++comptes[(size_t)k][(size_t)c];
    }
    std::vector<int> correspondance((size_t)std::max(nbClusters, 0), -1);
    for (int k = 0; k < nbClusters; ++k) {
        int meilleur = 0;
        for (int c = 1; c < nbClasses; ++c)
            if (comptes[(size_t)k][(size_t)c] > comptes[(size_t)k][(size_t)meilleur]) meilleur = c;
        if (nbClasses > 0 && comptes[(size_t)k][(size_t)meilleur] > 0) correspondance[(size_t)k] = meilleur;
    }
    return correspondance;
}

Resultat<Modele> EntrainerEm(const cv::Mat& x, const std::vector<int>& labels, int nbClasses, const ParametresEm& p,
                             std::uint64_t graine) {
    auto verif = VerifierEntrees(x, labels, nbClasses, "apprentissage_em");
    if (!verif.Ok()) return verif.Err();
    if (!(verif = VerifierEm(p, x.rows, "apprentissage_em")).Ok()) return verif.Err();
    ParametresAlgorithme a;
    a.m_famille = Famille::Em;
    a.m_em = p;
    ValeurJson d = a.VersJson();
    d["graine"] = (unsigned long long)graine;
    Modele m = ModeleDeBase(Famille::Em, x, nbClasses, d);
    try {
        cv::theRNG() = cv::RNG(graine);   // kmeans d'initialisation d'OpenCV
        auto em = CreerEm(p);
        if (!em->trainEM(x, cv::noArray(), cv::noArray(), cv::noArray()))
            return FaireErreur(CodeErreur::Calcul, "apprentissage_em", "OpenCV a refuse l'apprentissage EM (vraisemblance degeneree)");
        m.m_opencv = em;
        FinaliserEm(m, x, labels);
    } catch (const cv::Exception& e) {
        return ErreurCv("apprentissage_em", e);
    }
    m.m_parametresEffectifs["iterations_demandees"] = p.m_iterationsMax;
    m.m_parametresEffectifs["iterations_effectuees"] = ValeurJson();   // null : non exposees hors pas a pas
    return m;
}

namespace {

std::vector<float> EtatEm(const cv::Ptr<cv::ml::EM>& em) {
    std::vector<float> v;
    cv::Mat moyennes = em->getMeans(), poids = em->getWeights();
    std::vector<cv::Mat> covs;
    em->getCovs(covs);
    for (int r = 0; r < moyennes.rows; ++r)
        for (int c = 0; c < moyennes.cols; ++c) v.push_back((float)moyennes.at<double>(r, c));
    for (const cv::Mat& cov : covs)
        for (int j = 0; j < cov.rows; ++j) v.push_back((float)cov.at<double>(j, j));
    for (int k = 0; k < (int)poids.total(); ++k) v.push_back((float)poids.at<double>(k));
    return v;
}

void EmettreEm(ObservateurTrace* observateur, int iteration, const char* phase, double logVraisemblance,
               const cv::Ptr<cv::ml::EM>& em) {
    if (!observateur) return;
    EtatTrace e;
    e.m_iteration = iteration;
    e.m_phase = phase;
    e.m_grandeurs = {{"iteration", (double)iteration}, {"log_vraisemblance", logVraisemblance}};
    e.m_vecteur = EtatEm(em);
    observateur->Recevoir(e);
}

// Arret d'OpenCV (doTrain) : vraisemblance qui baisse, ou gain relatif < epsilon
bool ArretEm(double actuelle, double precedente, double epsilon) {
    double delta = actuelle - precedente;
    return delta < -DBL_EPSILON || delta < epsilon * std::fabs(actuelle);
}

// Iteration 0 : trainEM(1 iteration) = kmeans + un pas E. Iteration t >= 1 : trainM(1 iteration) = pas M puis pas E
// sur les responsabilites precedentes (equivalent exact de la boucle interne d'OpenCV, un pas a la fois).
Statut BoucleEm(Modele& m, const cv::Mat& x, const ParametresEm& p, ObservateurTrace* observateur, int& effectuees) {
    auto em = CreerEm(p);
    em->setTermCriteria(cv::TermCriteria(cv::TermCriteria::COUNT, 1, 0.0));
    cv::Mat logVrais, etiquettes, probas;
    if (!em->trainEM(x, logVrais, etiquettes, probas))
        return FaireErreur(CodeErreur::Calcul, "apprentissage_em", "OpenCV a refuse l'initialisation EM");
    double actuelle = cv::sum(logVrais)[0];
    effectuees = 1;
    EmettreEm(observateur, 0, "initialisation", actuelle, em);
    for (int t = 1; t < p.m_iterationsMax; ++t) {
        cv::Mat suivantes;
        double precedente = actuelle;
        if (!em->trainM(x, probas, logVrais, etiquettes, suivantes))
            return FaireErreur(CodeErreur::Calcul, "apprentissage_em", "OpenCV a refuse l'iteration EM " + std::to_string(t));
        probas = suivantes;
        actuelle = cv::sum(logVrais)[0];
        effectuees = t + 1;
        EmettreEm(observateur, t, "iteration", actuelle, em);
        if (ArretEm(actuelle, precedente, p.m_epsilon)) break;
    }
    em->setTermCriteria(cv::TermCriteria(cv::TermCriteria::COUNT + cv::TermCriteria::EPS, p.m_iterationsMax, p.m_epsilon));
    m.m_opencv = em;
    return Succes();
}

} // namespace

Resultat<Modele> EntrainerEmPasAPas(const cv::Mat& x, const std::vector<int>& labels, int nbClasses,
                                    const ParametresEm& p, std::uint64_t graine, ObservateurTrace* observateur) {
    auto verif = VerifierEntrees(x, labels, nbClasses, "apprentissage_em");
    if (!verif.Ok()) return verif.Err();
    if (!(verif = VerifierEm(p, x.rows, "apprentissage_em")).Ok()) return verif.Err();
    ParametresAlgorithme a;
    a.m_famille = Famille::Em;
    a.m_em = p;
    ValeurJson d = a.VersJson();
    d["graine"] = (unsigned long long)graine;
    Modele m = ModeleDeBase(Famille::Em, x, nbClasses, d);
    int effectuees = 0;
    try {
        cv::theRNG() = cv::RNG(graine);
        Statut st = BoucleEm(m, x, p, observateur, effectuees);
        if (!st.Ok()) return st.Err();
        FinaliserEm(m, x, labels);
    } catch (const cv::Exception& e) {
        return ErreurCv("apprentissage_em", e);
    }
    m.m_parametresEffectifs["iterations_demandees"] = p.m_iterationsMax;
    m.m_parametresEffectifs["iterations_effectuees"] = effectuees;
    m.m_parametresEffectifs["mode"] = "pas a pas : trainEM (1 iteration) puis trainM (1 iteration) repete";
    return m;
}

Resultat<std::vector<int>> ClustersEm(const Modele& m, const cv::Mat& x) {
    if (m.m_famille != Famille::Em || !m.m_opencv)
        return FaireErreur(CodeErreur::ModeleIncompatible, "prediction", "modele non EM");
    if (x.type() != CV_32F || x.cols != m.m_nbAttributs)
        return FaireErreur(CodeErreur::ModeleIncompatible, "prediction",
            "le modele attend " + std::to_string(m.m_nbAttributs) + " attributs, " + std::to_string(x.cols) + " fournis");
    try {
        return ClustersDe(m.m_opencv.dynamicCast<cv::ml::EM>(), x);
    } catch (const cv::Exception& e) {
        return ErreurCv("prediction", e);
    }
}

// ====================================================================== Boost un-contre-tous

namespace {

int TypeBoostOpenCv(const std::string& nom) {
    if (nom == "DISCRETE") return cv::ml::Boost::DISCRETE;
    if (nom == "REEL") return cv::ml::Boost::REAL;
    if (nom == "LOGIT") return cv::ml::Boost::LOGIT;
    if (nom == "GENTLE") return cv::ml::Boost::GENTLE;
    return -1;
}

const char* NomTypeBoost(int type) {
    switch (type) {
    case cv::ml::Boost::DISCRETE: return "DISCRETE";
    case cv::ml::Boost::REAL: return "REEL";
    case cv::ml::Boost::LOGIT: return "LOGIT";
    case cv::ml::Boost::GENTLE: return "GENTLE";
    default: return "AUTRE";
    }
}

Statut VerifierBoost(const ParametresBoost& p, const char* phase) {
    auto refus = [&](const std::string& message) { return FaireErreur(CodeErreur::DonneesInvalides, phase, message); };
    if (TypeBoostOpenCv(p.m_type) < 0) return refus("type de boosting inconnu : " + p.m_type + " (DISCRETE, REEL, LOGIT, GENTLE)");
    if (p.m_nbFaibles < 1 || p.m_nbFaibles > 5000) return refus("nb_faibles entre 1 et 5000");
    if (p.m_profondeur < 1 || p.m_profondeur > 16) return refus("profondeur entre 1 et 16");
    return Succes();
}

// Labels binaires du classifieur de la classe c : classe c -> 1 (positif), toutes les autres -> 0
Resultat<std::vector<int>> LabelsUnContreTous(const std::vector<int>& labels, int c) {
    std::vector<int> y;
    int positifs = 0;
    for (int l : labels) { y.push_back(l == c ? 1 : 0); positifs += l == c ? 1 : 0; }
    if (positifs == 0 || positifs == (int)labels.size())
        return FaireErreur(CodeErreur::DonneesInvalides, "apprentissage_boost",
            positifs == 0 ? "la classe " + std::to_string(c) + " est absente de l'apprentissage : un-contre-tous impossible"
                          : "toutes les lignes sont de la classe " + std::to_string(c));
    return y;
}

cv::Ptr<cv::ml::Boost> CreerBoost(const ParametresBoost& p) {
    auto boost = cv::ml::Boost::create();
    boost->setBoostType(TypeBoostOpenCv(p.m_type));
    boost->setWeakCount(p.m_nbFaibles);
    boost->setMaxDepth(p.m_profondeur);
    boost->setCVFolds(0);
    boost->setUseSurrogates(false);
    return boost;
}

void RelireParametresBoost(Modele& m) {
    ValeurJson e = ValeurJson::Objet();
    auto premier = m.m_sousModeles.front().dynamicCast<cv::ml::Boost>();
    size_t mini = 0, maxi = 0;
    for (size_t i = 0; i < m.m_sousModeles.size(); ++i) {
        size_t n = m.m_sousModeles[i].dynamicCast<cv::ml::Boost>()->getRoots().size();
        mini = i == 0 ? n : std::min(mini, n);
        maxi = std::max(maxi, n);
    }
    e["type"] = NomTypeBoost(premier->getBoostType());
    e["nb_faibles_min"] = (long long)mini;
    e["nb_faibles_max"] = (long long)maxi;
    e["profondeur"] = premier->getMaxDepth();
    e["min_echantillons"] = premier->getMinSampleCount();
    e["taux_elagage_poids"] = premier->getWeightTrimRate();
    e["nb_classifieurs"] = (long long)m.m_sousModeles.size();
    e["nb_attributs"] = m.m_nbAttributs;
    e["etiquettes"] = "classifieur c : classe c -> 1 (positif), autres -> 0";
    e["score"] = "predict(PREDICT_SUM | RAW_OUTPUT) ; > 0 <=> classe positive ; decision = score maximal, egalite -> plus petit id";
    e["regle_decision"] = "ova-score-max/1";
    m.m_parametresEffectifs = e;
}

} // namespace

Resultat<Modele> EntrainerBoost(const cv::Mat& x, const std::vector<int>& labels, int nbClasses, const ParametresBoost& p) {
    auto verif = VerifierEntrees(x, labels, nbClasses, "apprentissage_boost");
    if (!verif.Ok()) return verif.Err();
    if (!(verif = VerifierBoost(p, "apprentissage_boost")).Ok()) return verif.Err();
    ParametresAlgorithme a;
    a.m_famille = Famille::Boost;
    a.m_boost = p;
    Modele m = ModeleDeBase(Famille::Boost, x, nbClasses, a.VersJson());
    try {
        for (int c = 0; c < nbClasses; ++c) {
            auto y = LabelsUnContreTous(labels, c);
            if (!y.Ok()) return y.Err();
            auto boost = CreerBoost(p);
            if (!boost->train(DonneesClassification(x, y.Valeur())))
                return FaireErreur(CodeErreur::Calcul, "apprentissage_boost",
                                   "OpenCV a refuse l'apprentissage du classifieur de la classe " + std::to_string(c));
            m.m_sousModeles.push_back(boost);
        }
    } catch (const cv::Exception& e) {
        return ErreurCv("apprentissage_boost", e);
    }
    RelireParametresBoost(m);
    return m;
}

Resultat<cv::Mat> ScoresBoost(const Modele& m, const cv::Mat& x) {
    if (m.m_famille != Famille::Boost || (int)m.m_sousModeles.size() != m.m_nbClasses || m.m_sousModeles.empty())
        return FaireErreur(CodeErreur::ModeleIncompatible, "prediction", "modele Boost vide ou incoherent");
    if (x.type() != CV_32F || x.cols != m.m_nbAttributs)
        return FaireErreur(CodeErreur::ModeleIncompatible, "prediction",
            "le modele attend " + std::to_string(m.m_nbAttributs) + " attributs, " + std::to_string(x.cols) + " fournis");
    cv::Mat scores(x.rows, m.m_nbClasses, CV_32F);
    try {
        for (int c = 0; c < m.m_nbClasses; ++c) {
            cv::Mat colonne;
            m.m_sousModeles[(size_t)c]->predict(x, colonne, cv::ml::DTrees::PREDICT_SUM | StatModel::RAW_OUTPUT);
            colonne.copyTo(scores.col(c));
        }
    } catch (const cv::Exception& e) {
        return ErreurCv("prediction", e);
    }
    return scores;
}

// ====================================================================== prediction

namespace {

bool ModeleAbsent(const Modele& m) {
    if (m.m_famille == Famille::Boost) {
        if (m.m_sousModeles.empty()) return true;
        for (const auto& s : m.m_sousModeles) if (!s || s->empty()) return true;
        return false;
    }
    return !m.m_opencv || m.m_opencv->empty();
}

Resultat<std::vector<int>> ClassesDepuisSorties(const Modele& m, const cv::Mat& sorties, int nbLignes) {
    std::vector<int> classes((size_t)nbLignes);
    for (int i = 0; i < nbLignes; ++i) {
        double v = ValeurSortie(sorties, i);
        int c = (int)std::lround(v);
        if (!std::isfinite(v) || (double)c != v || c < 0 || c >= m.m_nbClasses)
            return FaireErreur(CodeErreur::Calcul, "prediction",
                "sortie du modele hors des classes connues a la ligne " + std::to_string(i + 1));
        classes[(size_t)i] = c;
    }
    return classes;
}

Resultat<std::vector<int>> ClassesParArgMax(const cv::Mat& scores, int nbClasses) {
    std::vector<int> classes((size_t)scores.rows);
    for (int i = 0; i < scores.rows; ++i) {
        int c = ArgMaxLigne(scores.ptr<float>(i), nbClasses);   // egalite -> plus petit id
        if (c < 0)
            return FaireErreur(CodeErreur::Calcul, "prediction", "score non fini a la ligne " + std::to_string(i + 1));
        classes[(size_t)i] = c;
    }
    return classes;
}

Resultat<std::vector<int>> PredireMlp(const Modele& m, const cv::Mat& x) {
    cv::Mat sorties;
    m.m_opencv->predict(x, sorties);
    return ClassesParArgMax(sorties, m.m_nbClasses);
}

Resultat<std::vector<int>> PredireEm(const Modele& m, const cv::Mat& x) {
    auto clusters = ClustersEm(m, x);
    if (!clusters.Ok()) return clusters.Err();
    std::vector<int> classes;
    long nonAssocies = 0;
    for (int k : clusters.Valeur()) {
        int c = k >= 0 && (size_t)k < m.m_correspondanceClusters.size() ? m.m_correspondanceClusters[(size_t)k] : -1;
        nonAssocies += c < 0 ? 1 : 0;
        classes.push_back(c);
    }
    if (nonAssocies > 0)
        return FaireErreur(CodeErreur::Calcul, "prediction", std::to_string(nonAssocies) +
            " ligne(s) tombent dans un cluster EM non associe a une classe (aucun echantillon d'apprentissage dans ce cluster)");
    return classes;
}

Resultat<std::vector<int>> PredireBoost(const Modele& m, const cv::Mat& x) {
    auto scores = ScoresBoost(m, x);
    if (!scores.Ok()) return scores.Err();
    return ClassesParArgMax(scores.Valeur(), m.m_nbClasses);
}

} // namespace

Resultat<std::vector<int>> Predire(const Modele& m, const cv::Mat& x) {
    if (ModeleAbsent(m)) return FaireErreur(CodeErreur::ModeleIncompatible, "prediction", "modele vide");
    if (x.type() != CV_32F || x.cols != m.m_nbAttributs)
        return FaireErreur(CodeErreur::ModeleIncompatible, "prediction",
            "le modele attend " + std::to_string(m.m_nbAttributs) + " attributs, " + std::to_string(x.cols) + " fournis");
    try {
        if (m.m_famille == Famille::Mlp) return PredireMlp(m, x);
        if (m.m_famille == Famille::Em) return PredireEm(m, x);
        if (m.m_famille == Famille::Boost) return PredireBoost(m, x);
        cv::Mat sorties;
        m.m_opencv->predict(x, sorties);
        return ClassesDepuisSorties(m, sorties, x.rows);
    } catch (const cv::Exception& e) {
        return ErreurCv("prediction", e);
    }
}

// Point d'entree unique
Resultat<Modele> Entrainer(const cv::Mat& x, const std::vector<int>& labels, int nbClasses,
                           const ParametresAlgorithme& p, std::uint64_t graine) {
    cv::theRNG() = cv::RNG(graine);   // RNG local au thread qui calcule
    switch (p.m_famille) {
    case Famille::Svm: return EntrainerSvm(x, labels, nbClasses, p.m_svm);
    case Famille::ForetAleatoire: return EntrainerForet(x, labels, nbClasses, p.m_foret, graine);
    case Famille::Kppv: return EntrainerKppv(x, labels, nbClasses, p.m_kppv);
    case Famille::BayesNormal: return EntrainerBayes(x, labels, nbClasses);
    case Famille::Mlp: return EntrainerMlp(x, labels, nbClasses, p.m_mlp);
    case Famille::Em: return EntrainerEm(x, labels, nbClasses, p.m_em, graine);
    case Famille::Boost: return EntrainerBoost(x, labels, nbClasses, p.m_boost);
    }
    return FaireErreur(CodeErreur::Protocole, "apprentissage", "famille inconnue");
}

// ====================================================================== sauvegarde / chargement

namespace {

void EcrireBoost(cv::FileStorage& flux, const Modele& m) {
    flux << "decid_boost_ova" << "{" << "nb_classes" << (int)m.m_sousModeles.size() << "regle" << "ova-score-max/1" << "}";
    for (size_t i = 0; i < m.m_sousModeles.size(); ++i)
        EcrireNoeud(flux, m.m_sousModeles[i], "classe_" + std::to_string(i));
}

std::string TexteModele(const Modele& m) {
    cv::FileStorage flux(".yml", cv::FileStorage::WRITE | cv::FileStorage::MEMORY);
    if (m.m_famille == Famille::Boost) {
        EcrireBoost(flux, m);
    } else {
        EcrireNoeud(flux, m.m_opencv, m.m_opencv->getDefaultName());
        if (m.m_famille == Famille::Em) cv::write(flux, "decid_correspondance", m.m_correspondanceClusters);
    }
    return flux.releaseAndGetString();
}

cv::Ptr<StatModel> ModeleVideDeFamille(Famille f) {
    switch (f) {
    case Famille::Svm: return cv::ml::SVM::create();
    case Famille::ForetAleatoire: return cv::ml::RTrees::create();
    case Famille::Kppv: return cv::ml::KNearest::create();
    case Famille::BayesNormal: return cv::ml::NormalBayesClassifier::create();
    case Famille::Mlp: return cv::ml::ANN_MLP::create();
    case Famille::Em: return cv::ml::EM::create();
    case Famille::Boost: return cv::ml::Boost::create();
    }
    return cv::Ptr<StatModel>();
}

Statut Incompatible(const std::string& message, const fs::path& fichier) {
    return FaireErreur(CodeErreur::ModeleIncompatible, "chargement_modele", message, CheminUtf8(fichier));
}

Statut ChargerBoost(Modele& m, const cv::FileStorage& flux, const fs::path& fichier) {
    cv::FileNode entete = flux["decid_boost_ova"];
    if (entete.empty()) return Incompatible("noeud decid_boost_ova absent", fichier);
    if ((int)entete["nb_classes"] != m.m_nbClasses)
        return Incompatible("nombre de classes du fichier Boost different de celui attendu", fichier);
    for (int c = 0; c < m.m_nbClasses; ++c) {
        cv::FileNode noeud = flux["classe_" + std::to_string(c)];
        if (noeud.empty()) return Incompatible("classifieur de la classe " + std::to_string(c) + " absent", fichier);
        auto boost = cv::ml::Boost::create();
        boost->read(noeud);
        if (!boost->isTrained()) return Incompatible("classifieur de la classe " + std::to_string(c) + " vide", fichier);
        m.m_sousModeles.push_back(boost);
    }
    m.m_nbAttributs = m.m_sousModeles.front()->getVarCount();
    RelireParametresBoost(m);
    return Succes();
}

Statut ChargerEm(Modele& m, const cv::FileStorage& flux, const fs::path& fichier) {
    cv::FileNode correspondance = flux["decid_correspondance"];
    if (correspondance.empty()) return Incompatible("correspondance cluster -> classe absente", fichier);
    correspondance >> m.m_correspondanceClusters;
    const int k = m.m_opencv.dynamicCast<cv::ml::EM>()->getClustersNumber();
    if ((int)m.m_correspondanceClusters.size() != k)
        return Incompatible("correspondance de taille differente du nombre de clusters", fichier);
    for (int c : m.m_correspondanceClusters)
        if (c < -1 || c >= m.m_nbClasses) return Incompatible("correspondance : classe hors plage", fichier);
    return Succes();
}

Statut ChargerUnModele(Modele& m, const cv::FileStorage& flux, const fs::path& fichier) {
    cv::Ptr<StatModel> modele = ModeleVideDeFamille(m.m_famille);
    cv::FileNode racine = flux.getFirstTopLevelNode();
    if (racine.empty() || racine.name() != modele->getDefaultName())
        return Incompatible("fichier modele vide, corrompu ou d'une autre famille", fichier);
    modele->read(racine);
    m.m_opencv = modele;
    if (!m.m_opencv->isTrained()) return Incompatible("fichier modele vide ou corrompu", fichier);
    m.m_nbAttributs = m.m_opencv->getVarCount();
    switch (m.m_famille) {
    case Famille::Svm: RelireParametresSvm(m); break;
    case Famille::ForetAleatoire: RelireParametresForet(m); break;
    case Famille::Kppv: RelireParametresKppv(m, (int)racine["samples"]["rows"]); break;
    case Famille::BayesNormal: {
        int apprises = 0;
        m.m_recalcul = RecalculDepuisEtatOpenCv(racine, apprises);
        RelireParametresBayes(m, apprises);
        break;
    }
    case Famille::Mlp: RelireParametresMlp(m); break;
    case Famille::Em: {
        Statut st = ChargerEm(m, flux, fichier);
        if (!st.Ok()) return st;
        RelireParametresEm(m);
        break;
    }
    case Famille::Boost: break;
    }
    return Succes();
}

} // namespace

Statut SauverModele(const Modele& m, const fs::path& fichier) {
    if (ModeleAbsent(m)) return FaireErreur(CodeErreur::ModeleIncompatible, "sauvegarde_modele", "modele vide");
    std::string texte;
    try {
        texte = TexteModele(m);
    } catch (const cv::Exception& e) {
        return ErreurCv("sauvegarde_modele", e);
    }
    return EcrireFichierAtomique(fichier, texte, false);
}

Resultat<Modele> ChargerModele(Famille famille, const fs::path& fichier, int nbClasses) {
    auto texte = LireFichierTexte(fichier);
    if (!texte.Ok()) return texte.Err();
    Modele m;
    m.m_famille = famille;
    m.m_nbClasses = nbClasses;
    try {
        cv::FileStorage flux(texte.Valeur(), cv::FileStorage::READ | cv::FileStorage::MEMORY);
        if (!flux.isOpened()) return Incompatible("fichier modele illisible", fichier).Err();
        Statut st = famille == Famille::Boost ? ChargerBoost(m, flux, fichier) : ChargerUnModele(m, flux, fichier);
        if (!st.Ok()) return st.Err();
    } catch (const cv::Exception& e) {
        return FaireErreur(CodeErreur::ModeleIncompatible, "chargement_modele", std::string("OpenCV : ") + e.what(),
                           CheminUtf8(fichier));
    }
    return m;
}

// ====================================================================== inspection

namespace {

// Arbres d'un RTrees ou d'un Boost : parcours depuis chaque racine, indices de noeuds de l'objet OpenCV
ValeurJson NoeudsArbre(const cv::ml::DTrees& arbres, int racine, bool avecClasse) {
    const auto& noeuds = arbres.getNodes();
    const auto& coupures = arbres.getSplits();
    ValeurJson liste = ValeurJson::Tableau();
    std::vector<int> pile{racine};
    while (!pile.empty()) {
        int ni = pile.back(); pile.pop_back();
        const auto& n = noeuds[(size_t)ni];
        ValeurJson jn = ValeurJson::Objet();
        jn["id"] = ni;
        if (avecClasse) jn["classe"] = (int)std::lround(n.value);
        else jn["valeur"] = n.value;
        jn["gauche"] = n.left;
        jn["droite"] = n.right;
        if (n.split >= 0) {
            const auto& s = coupures[(size_t)n.split];
            jn["variable"] = s.varIdx;
            jn["seuil"] = (double)s.c;
            jn["inverse"] = s.inversed;
            jn["qualite"] = (double)s.quality;
        }
        liste.Ajouter(jn);
        if (n.right >= 0) pile.push_back(n.right);
        if (n.left >= 0) pile.push_back(n.left);
    }
    return liste;
}

ValeurJson ArbresVersJson(const cv::ml::DTrees& arbres, bool avecClasse, size_t maxArbres, bool& tronque) {
    ValeurJson resultat = ValeurJson::Tableau();
    const auto& racines = arbres.getRoots();
    tronque = racines.size() > maxArbres;
    for (size_t i = 0; i < racines.size() && i < maxArbres; ++i) {
        ValeurJson arbre = ValeurJson::Objet();
        arbre["racine"] = racines[i];
        arbre["noeuds"] = NoeudsArbre(arbres, racines[i], avecClasse);
        resultat.Ajouter(arbre);
    }
    return resultat;
}

void InspecterSvm(const Modele& m, ValeurJson& j) {
    auto svm = m.m_opencv.dynamicCast<cv::ml::SVM>();
    cv::Mat vs = svm->getSupportVectors();
    j["vecteurs_support"] = MatriceVersJson(vs);
    bool compresses = svm->getKernelType() == cv::ml::SVM::LINEAR;
    j["vecteurs_compresses_lineaire"] = compresses;   // en lineaire, OpenCV remplace les VS par w
    if (compresses) j["vecteurs_support_origine"] = MatriceVersJson(svm->getUncompressedSupportVectors());
    ValeurJson fonctions = ValeurJson::Tableau();
    // nombre de fonctions de decision : une par paire de classes presentes (un-contre-un)
    int nbFonctions = 0;
    try {
        for (;; ++nbFonctions) {
            cv::Mat alpha, indices;
            double rho = svm->getDecisionFunction(nbFonctions, alpha, indices);
            ValeurJson f = ValeurJson::Objet();
            f["rho"] = rho;
            ValeurJson a = ValeurJson::Tableau(), idx = ValeurJson::Tableau();
            for (int i = 0; i < alpha.cols * alpha.rows; ++i) a.Ajouter(alpha.at<double>(i));
            for (int i = 0; i < indices.cols * indices.rows; ++i) idx.Ajouter(indices.at<int>(i));
            f["alpha"] = a;
            f["indices_vecteurs"] = idx;
            fonctions.Ajouter(f);
        }
    } catch (const cv::Exception&) {
        // fin des fonctions de decision (OpenCV leve une assertion au-dela du dernier indice)
    }
    j["fonctions_decision"] = fonctions;
    j["convention"] = "f(x) = somme alpha_k K(vs_k, x) - rho ; f > 0 => premiere classe de la paire";
}

void InspecterForet(const Modele& m, ValeurJson& j) {
    auto rf = m.m_opencv.dynamicCast<cv::ml::RTrees>();
    bool tronque = false;
    j["arbres"] = ArbresVersJson(*rf, true, rf->getRoots().size(), tronque);
    j["regle_coupure"] = "x[variable] <= seuil => gauche (sens inverse si inverse = vrai)";
    j["erreur_hors_sac"] = rf->getOOBError();
    cv::Mat importance = rf->getVarImportance();
    ValeurJson imp = ValeurJson::Tableau();
    for (int i = 0; i < (int)importance.total(); ++i) imp.Ajouter((double)importance.at<float>(i));
    j["importance_variables"] = imp;
}

void InspecterKppv(const Modele& m, ValeurJson& j) {
    j["k"] = m.m_parametresEffectifs.Membre("k");
    j["nb_echantillons"] = m.m_parametresEffectifs.Membre("nb_echantillons");
    j["note"] = "les echantillons embarques ne sont pas recopies ici ; voisins d'un point : VoisinsKppvPour";
}

void InspecterBayes(const Modele& m, ValeurJson& j) {
    j["recalcul_decid"] = m.m_recalcul;
    j["note"] = "etat interne d'OpenCV non expose : moyennes et covariances recalculees par Decid, a priori egaux";
}

void InspecterMlp(const Modele& m, ValeurJson& j) {
    auto ann = m.m_opencv.dynamicCast<cv::ml::ANN_MLP>();
    const int nbCouches = (int)ann->getLayerSizes().total();
    ValeurJson couches = ValeurJson::Tableau();
    for (int i = 1; i < nbCouches; ++i) {
        cv::Mat w = ann->getWeights(i);
        ValeurJson c = ValeurJson::Objet();
        c["couche"] = i;
        c["entrees"] = w.rows - 1;
        c["sorties"] = w.cols;
        c["poids"] = MatriceVersJson(w, 200000);   // (entrees + 1) x sorties ; derniere ligne = biais
        couches.Ajouter(c);
    }
    j["couches"] = couches;
    j["echelle_entree"] = MatriceVersJson(ann->getWeights(0).reshape(1, ann->getWeights(0).cols / 2), 100000);
    j["echelle_sortie"] = MatriceVersJson(ann->getWeights(nbCouches).reshape(1, ann->getWeights(nbCouches).cols / 2), 100000);
    j["convention"] = "x_entree' = a*x + b (lignes [a, b] de echelle_entree) ; sortie couche = f(poids^T [entrees; 1])";
}

void InspecterEm(const Modele& m, ValeurJson& j) {
    auto em = m.m_opencv.dynamicCast<cv::ml::EM>();
    j["k"] = em->getClustersNumber();
    j["moyennes"] = MatriceVersJson(em->getMeans());
    std::vector<cv::Mat> covs;
    em->getCovs(covs);
    ValeurJson liste = ValeurJson::Tableau();
    for (const cv::Mat& c : covs) liste.Ajouter(MatriceVersJson(c, 200000));
    j["covariances"] = liste;
    j["poids"] = MatriceVersJson(em->getWeights());
    j["correspondance_clusters"] = EntiersVersJson(m.m_correspondanceClusters);
    j["note"] = "correspondance cluster -> classe : classe majoritaire de l'apprentissage ; -1 = cluster non associe";
}

void InspecterBoost(const Modele& m, ValeurJson& j) {
    const size_t MAX_FAIBLES = 100;
    ValeurJson classes = ValeurJson::Tableau();
    for (size_t c = 0; c < m.m_sousModeles.size(); ++c) {
        auto boost = m.m_sousModeles[c].dynamicCast<cv::ml::Boost>();
        bool tronque = false;
        ValeurJson jc = ValeurJson::Objet();
        jc["classe"] = (int)c;
        jc["nb_faibles"] = (long long)boost->getRoots().size();
        jc["faibles"] = ArbresVersJson(*boost, false, MAX_FAIBLES, tronque);
        jc["faibles_tronques"] = tronque;
        classes.Ajouter(jc);
    }
    j["classes"] = classes;
    j["convention"] = "score(x) = somme des valeurs de feuilles atteintes (deja ponderees) ; > 0 <=> classe positive";
}

} // namespace

ValeurJson InspecterModele(const Modele& m) {
    ValeurJson j = ValeurJson::Objet();
    j["schema"] = "decid.inspection/1";
    j["famille"] = NomFamille(m.m_famille);
    j["espace"] = "espace du modele (apres pretraitement)";
    j["parametres_effectifs"] = m.m_parametresEffectifs;
    switch (m.m_famille) {
    case Famille::Svm: InspecterSvm(m, j); break;
    case Famille::ForetAleatoire: InspecterForet(m, j); break;
    case Famille::Kppv: InspecterKppv(m, j); break;
    case Famille::BayesNormal: InspecterBayes(m, j); break;
    case Famille::Mlp: InspecterMlp(m, j); break;
    case Famille::Em: InspecterEm(m, j); break;
    case Famille::Boost: InspecterBoost(m, j); break;
    }
    return j;
}

Resultat<double> DecisionSvmRecalculee(const Modele& m, const float* x) {
    if (m.m_famille != Famille::Svm)
        return FaireErreur(CodeErreur::ModeleIncompatible, "inspection", "modele non SVM");
    auto svm = m.m_opencv.dynamicCast<cv::ml::SVM>();
    cv::Mat vs = svm->getSupportVectors();
    cv::Mat alpha, indices;
    double rho = svm->getDecisionFunction(0, alpha, indices);
    int type = svm->getKernelType();
    double gamma = svm->getGamma(), coef0 = svm->getCoef0(), degre = svm->getDegree();
    double somme = -rho;
    for (int k = 0; k < (int)alpha.total(); ++k) {
        const float* v = vs.ptr<float>(indices.at<int>(k));
        double produit = 0.0, distance = 0.0;
        for (int a = 0; a < vs.cols; ++a) {
            produit += (double)v[a] * x[a];
            double d = (double)v[a] - x[a];
            distance += d * d;
        }
        double noyau;
        switch (type) {
        case cv::ml::SVM::LINEAR: noyau = produit; break;
        case cv::ml::SVM::POLY: noyau = std::pow(gamma * produit + coef0, degre); break;
        case cv::ml::SVM::RBF: noyau = std::exp(-gamma * distance); break;
        case cv::ml::SVM::SIGMOID: noyau = std::tanh(gamma * produit + coef0); break;
        default: return FaireErreur(CodeErreur::ModeleIncompatible, "inspection", "noyau non pris en charge");
        }
        somme += alpha.at<double>(k) * noyau;
    }
    return somme;
}

Resultat<std::vector<int>> VotesForet(const Modele& m, const cv::Mat& echantillon) {
    if (m.m_famille != Famille::ForetAleatoire)
        return FaireErreur(CodeErreur::ModeleIncompatible, "inspection", "modele non foret");
    auto rf = m.m_opencv.dynamicCast<cv::ml::RTrees>();
    cv::Mat votes;
    try {
        rf->getVotes(echantillon, votes, 0);
    } catch (const cv::Exception& e) {
        return ErreurCv("inspection", e);
    }
    // ligne 0 : etiquettes des classes ; ligne 1 : votes du premier echantillon
    std::vector<int> comptes((size_t)m.m_nbClasses, 0);
    for (int c = 0; c < votes.cols; ++c) {
        int etiquette = votes.at<int>(0, c);
        if (etiquette >= 0 && etiquette < m.m_nbClasses) comptes[(size_t)etiquette] = votes.at<int>(1, c);
    }
    return comptes;
}

} // namespace decid
