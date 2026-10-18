// Moteur explicable : AdaBoost, perceptron de Rosenblatt, k-means de Lloyd (implementations pedagogiques Decid).
// Chaque etat emis vers un ObservateurTrace est un etat reellement calcule ; sans observateur, aucun etat n'est construit.
#include "Explicable.h"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "Partage.h"

namespace decid {

namespace {

constexpr double kTolEgalite = 1e-12;     // amelioration minimale pour changer de meilleur faible (egalites stables)
constexpr double kAlphaParfait = 10.0;    // alpha fixe si le faible est parfait (eps = 0) a la 1re iteration

Erreur ErreurDonnees(const std::string& phase, const std::string& message) {
    return FaireErreur(CodeErreur::DonneesInvalides, phase, message);
}

// ---------- AdaBoost : recherche du meilleur seuil sur une valeur scalaire par echantillon ----------

struct Coupe { double m_eps = 2.0; double m_seuil = 0.0; int m_sens = 1; };

// h(x) = sens si v > seuil, -sens sinon. Seuils candidats : sous le minimum, puis milieux entre valeurs distinctes
// consecutives. Calcul incremental apres tri : O(n log n). Egalite : plus petit seuil, puis sens +1.
class RechercheSeuil {
public:
    RechercheSeuil(const std::vector<double>& v, const std::vector<int>& y, const std::vector<double>& d)
        : m_v(v), m_y(y), m_d(d), m_ordre(v.size()) {
        std::iota(m_ordre.begin(), m_ordre.end(), 0);
        std::sort(m_ordre.begin(), m_ordre.end(), [&](int a, int b) { return v[a] < v[b] || (v[a] == v[b] && a < b); });
        for (double w : d) m_total += w;
    }
    Coupe Meilleure() {
        // Seuil sous le minimum : tout le monde predit +sens ; pour sens=+1 l'erreur est le poids des y=-1.
        double errPlus = 0;
        for (size_t i = 0; i < m_y.size(); ++i) if (m_y[i] < 0) errPlus += m_d[i];
        Essayer(m_v[m_ordre.front()] - 1.0, errPlus);
        size_t i = 0;
        while (i < m_ordre.size()) {
            size_t j = i;
            while (j < m_ordre.size() && m_v[m_ordre[j]] == m_v[m_ordre[i]]) {
                int k = m_ordre[j];
                errPlus += (m_y[k] > 0) ? m_d[k] : -m_d[k];   // k passe sous le seuil : predit -1 pour sens=+1
                ++j;
            }
            if (j < m_ordre.size()) Essayer(Milieu(m_v[m_ordre[i]], m_v[m_ordre[j]]), errPlus);
            i = j;
        }
        return m_meilleure;
    }
private:
    static double Milieu(double a, double b) {
        double m = 0.5 * (a + b);
        return (m > a && m < b) ? m : a;       // a : v > a separe bien a (dessous) de b (dessus)
    }
    void Essayer(double seuil, double errPlus) {
        double ePlus = std::max(0.0, errPlus), eMoins = std::max(0.0, m_total - errPlus);
        if (ePlus < m_meilleure.m_eps - kTolEgalite) m_meilleure = {ePlus, seuil, +1};
        if (eMoins < m_meilleure.m_eps - kTolEgalite) m_meilleure = {eMoins, seuil, -1};
    }
    const std::vector<double>& m_v;
    const std::vector<int>& m_y;
    const std::vector<double>& m_d;
    std::vector<int> m_ordre;
    double m_total = 0;
    Coupe m_meilleure;
};

double Projeter(const std::vector<double>& w, const float* x) {
    double s = 0;
    for (size_t j = 0; j < w.size(); ++j) s += w[j] * (double)x[j];
    return s;
}

// Direction de Fisher ponderee : w = (Sw + 1e-6 I)^-1 (m+ - m-), puis normalisation (norme 1).
// [idee, non precisee par le cours] moyennes m+/m- et covariance intra-classe Sw ponderees par d_i.
Resultat<std::vector<double>> DirectionFisher(const cv::Mat& x, const std::vector<int>& y, const std::vector<double>& d) {
    const int n = x.rows, p = x.cols;
    cv::Mat moy = cv::Mat::zeros(2, p, CV_64F);          // ligne 0 : classe -1, ligne 1 : classe +1
    double poids[2] = {0, 0};
    for (int i = 0; i < n; ++i) {
        int c = y[i] > 0 ? 1 : 0;
        poids[c] += d[i];
        for (int j = 0; j < p; ++j) moy.at<double>(c, j) += d[i] * (double)x.at<float>(i, j);
    }
    if (poids[0] <= 0 || poids[1] <= 0)
        return ErreurDonnees("adaboost", "Hyperplan : une classe a un poids total nul");
    moy.row(0) /= poids[0];
    moy.row(1) /= poids[1];
    cv::Mat sw = cv::Mat::zeros(p, p, CV_64F);
    std::vector<double> ec(p);
    for (int i = 0; i < n; ++i) {
        int c = y[i] > 0 ? 1 : 0;
        for (int j = 0; j < p; ++j) ec[j] = (double)x.at<float>(i, j) - moy.at<double>(c, j);
        for (int a = 0; a < p; ++a)
            for (int b = 0; b < p; ++b) sw.at<double>(a, b) += d[i] * ec[a] * ec[b];
    }
    for (int a = 0; a < p; ++a) sw.at<double>(a, a) += 1e-6;
    cv::Mat diff = (moy.row(1) - moy.row(0)).t(), w;
    if (!cv::solve(sw, diff, w, cv::DECOMP_LU))
        return FaireErreur(CodeErreur::Calcul, "adaboost", "Hyperplan : systeme de Fisher singulier");
    double norme = std::sqrt(w.dot(w));
    std::vector<double> dir(p, 0.0);
    if (norme < 1e-300) { dir[0] = 1.0; return dir; }    // moyennes confondues : direction arbitraire (premier axe)
    for (int j = 0; j < p; ++j) dir[j] = w.at<double>(j) / norme;
    return dir;
}

} // namespace

// ---------- Faible ----------

const char* NomFaible(TypeFaible t) { return t == TypeFaible::Seuil ? "seuil" : "hyperplan"; }

int Faible::Decider(const float* x, int nbAttributs) const {
    double v = 0;
    if (m_type == TypeFaible::Seuil) v = (m_variable >= 0 && m_variable < nbAttributs) ? (double)x[m_variable] : 0.0;
    else v = Projeter(m_direction, x);
    return v > m_seuil ? m_sens : -m_sens;
}

ValeurJson Faible::VersJson() const {
    ValeurJson o = ValeurJson::Objet();
    o["type"] = NomFaible(m_type);
    o["variable"] = m_variable;
    ValeurJson dir = ValeurJson::Tableau();
    for (double c : m_direction) dir.Ajouter(c);
    o["direction"] = dir;
    o["seuil"] = m_seuil;
    o["sens"] = m_sens;
    return o;
}

// ---------- AdaBoostPasAPas ----------

Statut AdaBoostPasAPas::Initialiser(const cv::Mat& x, const std::vector<int>& y, TypeFaible type) {
    if (x.empty() || x.type() != CV_32F) return ErreurDonnees("adaboost", "x doit etre une matrice CV_32F non vide");
    if ((int)y.size() != x.rows) return ErreurDonnees("adaboost", "nombre de labels different du nombre de lignes");
    int pos = 0, neg = 0;
    for (int v : y) {
        if (v == 1) ++pos; else if (v == -1) ++neg;
        else return ErreurDonnees("adaboost", "les labels doivent valoir -1 ou +1");
    }
    if (pos == 0 || neg == 0) return ErreurDonnees("adaboost", "il faut au moins un exemple de chaque classe");
    m_x = x.clone();
    m_y = y;
    m_type = type;
    Reinitialiser();
    return Succes();
}

void AdaBoostPasAPas::Reinitialiser() {
    const size_t n = m_y.size();
    m_poids.assign(n, n ? 1.0 / (double)n : 0.0);      // d_i = 1/n
    m_scores.assign(n, 0.0);
    m_iterations.clear();
    m_sommeGamma2 = 0;
    m_arret.clear();
}

namespace {

struct Candidat { Faible m_faible; double m_eps = 2.0; };

Faible FaibleSeuil(const cv::Mat& x, const std::vector<int>& y, const std::vector<double>& d) {
    Coupe meilleure;
    int variable = 0;
    std::vector<double> col((size_t)x.rows);
    for (int j = 0; j < x.cols; ++j) {
        for (int i = 0; i < x.rows; ++i) col[(size_t)i] = (double)x.at<float>(i, j);
        Coupe c = RechercheSeuil(col, y, d).Meilleure();
        if (c.m_eps < meilleure.m_eps - kTolEgalite) { meilleure = c; variable = j; }   // egalite : plus petite variable
    }
    Faible f;
    f.m_type = TypeFaible::Seuil; f.m_variable = variable; f.m_seuil = meilleure.m_seuil; f.m_sens = meilleure.m_sens;
    return f;
}

Resultat<Faible> FaibleHyperplan(const cv::Mat& x, const std::vector<int>& y, const std::vector<double>& d) {
    auto dir = DirectionFisher(x, y, d);
    if (!dir.Ok()) return dir.Err();
    std::vector<double> proj((size_t)x.rows);
    for (int i = 0; i < x.rows; ++i) proj[(size_t)i] = Projeter(dir.Valeur(), x.ptr<float>(i));
    Coupe c = RechercheSeuil(proj, y, d).Meilleure();
    Faible f;
    f.m_type = TypeFaible::Hyperplan; f.m_direction = dir.Valeur(); f.m_seuil = c.m_seuil; f.m_sens = c.m_sens;
    return f;
}

} // namespace

namespace {

void EmettreIteration(const IterationAdaBoost& it, const std::vector<double>& poids, ObservateurTrace& observateur) {
    EtatTrace e;
    e.m_iteration = it.m_t;
    e.m_phase = "iteration";
    e.m_grandeurs = {{"t", (double)it.m_t}, {"epsilon", it.m_epsilon}, {"alpha", it.m_alpha}, {"Z", it.m_z},
                     {"erreur_apprentissage", it.m_erreurApprentissage}, {"borne", it.m_borne}};
    e.m_vecteur.reserve(poids.size());
    for (double d : poids) e.m_vecteur.push_back((float)d);
    observateur.Recevoir(e);
}

} // namespace

Resultat<bool> AdaBoostPasAPas::Iterer(ObservateurTrace* observateur) {
    if (m_x.empty()) return ErreurDonnees("adaboost", "Initialiser doit etre appele avant Iterer");
    if (!m_arret.empty()) return false;
    Resultat<Faible> choix = (m_type == TypeFaible::Seuil) ? Resultat<Faible>(FaibleSeuil(m_x, m_y, m_poids))
                                                           : FaibleHyperplan(m_x, m_y, m_poids);
    if (!choix.Ok()) return choix.Err();
    const Faible& h = choix.Valeur();
    std::vector<int> pred((size_t)m_x.rows);
    double eps = 0;                                    // eps = somme d_i [y_i != h(x_i)], recalcule directement
    for (int i = 0; i < m_x.rows; ++i) {
        pred[(size_t)i] = h.Decider(m_x.ptr<float>(i), m_x.cols);
        if (pred[(size_t)i] != m_y[(size_t)i]) eps += m_poids[(size_t)i];
    }
    // Applique l'iteration : d_i <- d_i exp(-alpha y_i h(x_i)) / Z, scores cumules, erreur du fort, borne.
    auto appliquer = [&](double alpha) {
        const size_t n = m_poids.size();
        double z = 0;
        for (size_t i = 0; i < n; ++i) {
            m_poids[i] *= std::exp(-alpha * (double)(m_y[i] * pred[i]));
            z += m_poids[i];
        }
        for (double& d : m_poids) d /= z;
        int erreurs = 0;
        for (size_t i = 0; i < n; ++i) {
            m_scores[i] += alpha * (double)pred[i];
            int fort = m_scores[i] >= 0 ? 1 : -1;      // signe(score), 0 -> +1
            if (fort != m_y[i]) ++erreurs;
        }
        m_sommeGamma2 += (0.5 - eps) * (0.5 - eps);
        IterationAdaBoost it;
        it.m_t = (int)m_iterations.size() + 1;
        it.m_faible = h; it.m_epsilon = eps; it.m_alpha = alpha; it.m_z = z;
        it.m_erreurApprentissage = (double)erreurs / (double)n;
        it.m_borne = std::exp(-2.0 * m_sommeGamma2);
        m_iterations.push_back(it);
        if (observateur) EmettreIteration(it, m_poids, *observateur);
    };
    if (eps == 0) {
        m_arret = "epsilon = 0 : faible parfait";
        if (!m_iterations.empty()) return false;
        appliquer(kAlphaParfait);   // 1re iteration : alpha fixe (documente)
        return false;
    }
    if (eps >= 0.5 - kTolEgalite) { m_arret = "epsilon >= 1/2 : plus de faible utile"; return false; }
    appliquer(0.5 * std::log((1.0 - eps) / eps));
    return true;
}

double AdaBoostPasAPas::Score(const float* x) const {
    double s = 0;
    for (const IterationAdaBoost& it : m_iterations) s += it.m_alpha * (double)it.m_faible.Decider(x, m_x.cols);
    return s;
}

int AdaBoostPasAPas::Decider(const float* x) const { return Score(x) >= 0 ? 1 : -1; }

ValeurJson AdaBoostPasAPas::VersJson() const {
    ValeurJson o = ValeurJson::Objet();
    o["type"] = NomFaible(m_type);
    o["arret"] = m_arret;
    ValeurJson its = ValeurJson::Tableau();
    for (const IterationAdaBoost& it : m_iterations) {
        ValeurJson j = ValeurJson::Objet();
        j["t"] = it.m_t; j["faible"] = it.m_faible.VersJson(); j["epsilon"] = it.m_epsilon; j["alpha"] = it.m_alpha;
        j["Z"] = it.m_z; j["erreur_apprentissage"] = it.m_erreurApprentissage; j["borne"] = it.m_borne;
        its.Ajouter(j);
    }
    o["iterations"] = its;
    return o;
}

// ---------- AdaBoostMulticlasse ----------

Statut AdaBoostMulticlasse::Entrainer(const cv::Mat& x, const std::vector<int>& labels, int nbClasses, TypeFaible type,
                                      int nbIterations) {
    if (nbClasses < 2) return ErreurDonnees("adaboost", "il faut au moins deux classes");
    if (nbIterations < 1) return ErreurDonnees("adaboost", "nbIterations doit etre >= 1");
    for (int l : labels)
        if (l < 0 || l >= nbClasses) return ErreurDonnees("adaboost", "label hors de [0, nbClasses)");
    m_parClasse.assign((size_t)nbClasses, AdaBoostPasAPas());
    for (int c = 0; c < nbClasses; ++c) {              // un contre tous
        std::vector<int> y(labels.size());
        for (size_t i = 0; i < labels.size(); ++i) y[i] = labels[i] == c ? 1 : -1;
        Statut s = m_parClasse[(size_t)c].Initialiser(x, y, type);
        if (!s.Ok()) return s;
        for (int t = 0; t < nbIterations; ++t) {
            Resultat<bool> r = m_parClasse[(size_t)c].Iterer(nullptr);
            if (!r.Ok()) return r.Err();
            if (!r.Valeur()) break;
        }
    }
    return Succes();
}

int AdaBoostMulticlasse::Decider(const float* x) const {
    int meilleur = 0;
    double meilleurScore = 0;
    for (size_t c = 0; c < m_parClasse.size(); ++c) {
        double s = m_parClasse[c].Score(x);
        if (c == 0 || s > meilleurScore) { meilleur = (int)c; meilleurScore = s; }   // egalite : plus petit id
    }
    return meilleur;
}

// ---------- Perceptron ----------

namespace {

int Heaviside(double a) { return a > 0 ? 1 : 0; }      // convention : H(0) = 0

Statut ValiderPerceptron(const cv::Mat& x, const std::vector<int>& y, const ParametresPerceptron& p) {
    if (x.empty() || x.type() != CV_32F) return ErreurDonnees("perceptron", "x doit etre une matrice CV_32F non vide");
    if ((int)y.size() != x.rows) return ErreurDonnees("perceptron", "nombre de labels different du nombre de lignes");
    for (int v : y) if (v != 0 && v != 1) return ErreurDonnees("perceptron", "les labels doivent valoir 0 ou 1");
    if (!(p.m_eta > 0)) return ErreurDonnees("perceptron", "eta doit etre > 0");
    if (p.m_epoquesMax < 1) return ErreurDonnees("perceptron", "epoquesMax doit etre >= 1");
    if (!p.m_poidsInitiaux.empty() && (int)p.m_poidsInitiaux.size() != x.cols)
        return ErreurDonnees("perceptron", "poids initiaux : taille differente du nombre d'attributs");
    return Succes();
}

std::vector<float> PoidsEtBiais(const ResultatPerceptron& r) {
    std::vector<float> v;
    v.reserve(r.m_poids.size() + 1);
    for (double w : r.m_poids) v.push_back((float)w);
    v.push_back((float)r.m_biais);
    return v;
}

// Fisher-Yates avec splitmix64 : i de n-1 a 1, j uniforme dans [0, i].
void Melanger(std::vector<int>& ordre, std::uint64_t& etat) {
    for (size_t i = ordre.size(); i > 1; --i)
        std::swap(ordre[i - 1], ordre[(size_t)TirageUniforme(etat, i)]);
}

// Presente un exemple : y^ = H(w.x + b) ; w += eta (y - y^) x ; b += eta (y - y^). Renvoie l'erreur (y - y^ != 0).
bool Presenter(const float* x, int y, double eta, ResultatPerceptron& r, int& yChapeau) {
    double a = r.m_biais;
    for (size_t j = 0; j < r.m_poids.size(); ++j) a += r.m_poids[j] * (double)x[j];
    yChapeau = Heaviside(a);
    const double delta = eta * (double)(y - yChapeau);
    for (size_t j = 0; j < r.m_poids.size(); ++j) r.m_poids[j] += delta * (double)x[j];
    r.m_biais += delta;
    return y != yChapeau;
}

} // namespace

Resultat<ResultatPerceptron> EntrainerPerceptron(const cv::Mat& x, const std::vector<int>& y01,
                                                 const ParametresPerceptron& p, ObservateurTrace* observateur) {
    Statut v = ValiderPerceptron(x, y01, p);
    if (!v.Ok()) return v.Err();
    ResultatPerceptron r;
    r.m_poids = p.m_poidsInitiaux.empty() ? std::vector<double>((size_t)x.cols, 0.0) : p.m_poidsInitiaux;
    r.m_biais = p.m_biaisInitial;
    std::vector<int> ordre((size_t)x.rows);
    std::iota(ordre.begin(), ordre.end(), 0);
    std::uint64_t etat = p.m_graine;
    int presentes = 0;
    for (int epoque = 1; epoque <= p.m_epoquesMax; ++epoque) {
        if (p.m_melanger) Melanger(ordre, etat);
        int erreurs = 0;
        for (int i : ordre) {
            int yc = 0;
            bool faux = Presenter(x.ptr<float>(i), y01[(size_t)i], p.m_eta, r, yc);
            erreurs += faux ? 1 : 0;
            ++presentes;
            if (observateur) {
                EtatTrace e;
                e.m_iteration = presentes; e.m_phase = "exemple";
                e.m_grandeurs = {{"epoque", (double)epoque}, {"indice", (double)i}, {"y", (double)y01[(size_t)i]},
                                 {"y_chapeau", (double)yc}, {"erreur", faux ? 1.0 : 0.0}};
                e.m_vecteur = PoidsEtBiais(r);
                observateur->Recevoir(e);
            }
        }
        r.m_erreursParEpoque.push_back(erreurs);
        r.m_epoques = epoque;
        if (observateur) {
            EtatTrace e;
            e.m_iteration = epoque; e.m_phase = "epoque";
            e.m_grandeurs = {{"erreurs", (double)erreurs}};
            e.m_vecteur = PoidsEtBiais(r);
            observateur->Recevoir(e);
        }
        if (erreurs == 0) { r.m_converge = true; break; }
    }
    return r;
}

// ---------- K-means ----------

namespace {

double Dist2(const float* a, const float* b, int p) {
    double s = 0;
    for (int j = 0; j < p; ++j) { double d = (double)a[j] - (double)b[j]; s += d * d; }
    return s;
}

Statut ValiderKmeans(const cv::Mat& x, const ParametresKmeans& p) {
    if (x.empty() || x.type() != CV_32F) return ErreurDonnees("kmeans", "x doit etre une matrice CV_32F non vide");
    if (p.m_k < 1 || p.m_k > x.rows) return ErreurDonnees("kmeans", "k doit verifier 1 <= k <= n");
    if (p.m_iterMax < 1) return ErreurDonnees("kmeans", "iterMax doit etre >= 1");
    if (p.m_init == InitKmeans::Fournie &&
        (p.m_centresInitiaux.type() != CV_32F || p.m_centresInitiaux.rows != p.m_k || p.m_centresInitiaux.cols != x.cols))
        return ErreurDonnees("kmeans", "centres initiaux fournis : matrice CV_32F k x p attendue");
    return Succes();
}

cv::Mat CentresAleatoires(const cv::Mat& x, int k, std::uint64_t& etat) {
    std::vector<int> idx((size_t)x.rows);
    std::iota(idx.begin(), idx.end(), 0);
    cv::Mat c(k, x.cols, CV_32F);
    for (int i = 0; i < k; ++i) {                       // k lignes distinctes : Fisher-Yates partiel
        size_t j = (size_t)i + (size_t)TirageUniforme(etat, idx.size() - (size_t)i);
        std::swap(idx[(size_t)i], idx[j]);
        x.row(idx[(size_t)i]).copyTo(c.row(i));
    }
    return c;
}

// Tirage ~ D(x)^2. Discretisation : u = (r + 1/2) / 2^32 avec r entier uniforme dans [0, 2^32) (TirageUniforme),
// puis premier point dont la somme cumulee des D^2 depasse u * total (un point de D^2 = 0 n'est jamais tire).
int TirerProportionnel(const std::vector<double>& d2, std::uint64_t& etat) {
    const std::uint64_t echelle = 1ULL << 32;
    double total = 0;
    for (double v : d2) total += v;
    if (!(total > 0)) return (int)TirageUniforme(etat, d2.size());   // tous les points sont des centres : uniforme
    const double cible = ((double)TirageUniforme(etat, echelle) + 0.5) / (double)echelle * total;
    double cumul = 0;
    int dernierPositif = 0;
    for (size_t i = 0; i < d2.size(); ++i) {
        if (d2[i] > 0) dernierPositif = (int)i;
        cumul += d2[i];
        if (cumul > cible && d2[i] > 0) return (int)i;
    }
    return dernierPositif;                              // securite contre les arrondis
}

cv::Mat CentresPlusPlus(const cv::Mat& x, int k, std::uint64_t& etat) {
    cv::Mat c(k, x.cols, CV_32F);
    std::vector<double> d2((size_t)x.rows, 0.0);
    int choisi = (int)TirageUniforme(etat, (std::uint64_t)x.rows);
    for (int m = 0; m < k; ++m) {
        if (m > 0) choisi = TirerProportionnel(d2, etat);
        x.row(choisi).copyTo(c.row(m));
        for (int i = 0; i < x.rows; ++i) {
            double d = Dist2(x.ptr<float>(i), c.ptr<float>(m), x.cols);
            d2[(size_t)i] = (m == 0) ? d : std::min(d2[(size_t)i], d);
        }
    }
    return c;
}

// Affectation au centre le plus proche (egalite : plus petit indice). Renvoie le nombre de changements.
int Affecter(const cv::Mat& x, const cv::Mat& centres, std::vector<int>& aff) {
    int changements = 0;
    for (int i = 0; i < x.rows; ++i) {
        int meilleur = 0;
        double dm = Dist2(x.ptr<float>(i), centres.ptr<float>(0), x.cols);
        for (int c = 1; c < centres.rows; ++c) {
            double d = Dist2(x.ptr<float>(i), centres.ptr<float>(c), x.cols);
            if (d < dm) { dm = d; meilleur = c; }
        }
        if (aff[(size_t)i] != meilleur) { ++changements; aff[(size_t)i] = meilleur; }
    }
    return changements;
}

double CritereJ(const cv::Mat& x, const cv::Mat& centres, const std::vector<int>& aff) {
    double j = 0;
    for (int i = 0; i < x.rows; ++i) j += Dist2(x.ptr<float>(i), centres.ptr<float>(aff[(size_t)i]), x.cols);
    return j;
}

// Barycentres (sommes en double, centres arrondis en float) ; un centre sans point reste en place.
void MettreAJour(const cv::Mat& x, const std::vector<int>& aff, cv::Mat& centres) {
    const int k = centres.rows, p = x.cols;
    cv::Mat somme = cv::Mat::zeros(k, p, CV_64F);
    std::vector<int> compte((size_t)k, 0);
    for (int i = 0; i < x.rows; ++i) {
        ++compte[(size_t)aff[(size_t)i]];
        for (int j = 0; j < p; ++j) somme.at<double>(aff[(size_t)i], j) += (double)x.at<float>(i, j);
    }
    for (int c = 0; c < k; ++c) {
        if (compte[(size_t)c] == 0) continue;
        for (int j = 0; j < p; ++j) centres.at<float>(c, j) = (float)(somme.at<double>(c, j) / compte[(size_t)c]);
    }
}

void EmettreKmeans(ObservateurTrace* obs, int iteration, const char* phase, double j, int changements, const cv::Mat& centres,
                   const std::vector<int>& affectations) {
    if (!obs) return;
    EtatTrace e;
    e.m_iteration = iteration; e.m_phase = phase;
    e.m_grandeurs = {{"J", j}, {"changements", (double)changements}};
    e.m_vecteur.assign(centres.ptr<float>(0), centres.ptr<float>(0) + (size_t)centres.rows * (size_t)centres.cols);
    e.m_affectations = affectations;
    obs->Recevoir(e);
}

cv::Mat CentresInitiaux(const cv::Mat& x, const ParametresKmeans& p) {
    std::uint64_t etat = p.m_graine;
    if (p.m_init == InitKmeans::Fournie) return p.m_centresInitiaux.clone();
    if (p.m_init == InitKmeans::Aleatoire) return CentresAleatoires(x, p.m_k, etat);
    return CentresPlusPlus(x, p.m_k, etat);
}

} // namespace

Resultat<ResultatKmeans> Kmeans(const cv::Mat& x, const ParametresKmeans& p, ObservateurTrace* observateur) {
    Statut v = ValiderKmeans(x, p);
    if (!v.Ok()) return v.Err();
    ResultatKmeans r;
    r.m_centres = CentresInitiaux(x, p);
    r.m_affectations.assign((size_t)x.rows, -1);
    for (int it = 1; it <= p.m_iterMax; ++it) {
        r.m_iterations = it;
        int ch = Affecter(x, r.m_centres, r.m_affectations);
        double j = CritereJ(x, r.m_centres, r.m_affectations);
        r.m_jParPhase.push_back(j);
        EmettreKmeans(observateur, it, "affectation", j, ch, r.m_centres, r.m_affectations);
        if (it >= 2 && ch == 0) { r.m_aucunChangement = true; break; }
        MettreAJour(x, r.m_affectations, r.m_centres);
        j = CritereJ(x, r.m_centres, r.m_affectations);
        r.m_jParPhase.push_back(j);
        EmettreKmeans(observateur, it, "mise_a_jour", j, ch, r.m_centres, r.m_affectations);
    }
    return r;
}

} // namespace decid
