// Bayes et estimation de densite : implementation pedagogique (cours chap. 2, TD Bayes et Parzen).
#include "Densites.h"

#include <algorithm>
#include <cmath>

namespace decid {

namespace {

const double PI = 3.14159265358979323846;

Erreur Invalide(const std::string& phase, const std::string& message) {
    return FaireErreur(CodeErreur::DonneesInvalides, phase, message);
}

// Densite gaussienne : f(x) = 1/(sigma sqrt(2 pi)) exp(-(x-mu)^2 / (2 sigma^2))
double Gauss(double x, double moyenne, double sigma) {
    const double u = (x - moyenne) / sigma;
    return std::exp(-0.5 * u * u) / (sigma * std::sqrt(2.0 * PI));
}

// Integrale par la methode des trapezes sur une grille croissante
double Trapezes(const std::vector<double>& x, const std::vector<double>& y) {
    double somme = 0;
    for (size_t i = 1; i < x.size() && i < y.size(); ++i) somme += 0.5 * (y[i - 1] + y[i]) * (x[i] - x[i - 1]);
    return somme;
}

ValeurJson TableauJson(const std::vector<double>& v) {
    ValeurJson t = ValeurJson::Tableau();
    for (double d : v) t.Ajouter(ValeurJson(d));
    return t;
}

ValeurJson MatriceJson(const std::vector<std::vector<double>>& m) {
    ValeurJson t = ValeurJson::Tableau();
    for (const auto& ligne : m) t.Ajouter(TableauJson(ligne));
    return t;
}

// Risque R(j|x) = somme_i cout[i][j] P(C_i|x)
double Risque(const std::vector<std::vector<double>>& couts, const std::vector<double>& posteriori, size_t j) {
    double r = 0;
    for (size_t i = 0; i < posteriori.size(); ++i) r += couts[i][j] * posteriori[i];
    return r;
}

} // namespace

std::vector<double> Grille(double debut, double fin, int n) {
    std::vector<double> g;
    if (n < 2 || !(debut < fin) || !std::isfinite(debut) || !std::isfinite(fin)) return g;   // grille invalide : vide
    g.resize((size_t)n);
    const double pas = (fin - debut) / (n - 1);
    for (int i = 0; i < n; ++i) g[(size_t)i] = debut + pas * i;
    g[0] = debut;
    g[(size_t)n - 1] = fin;   // bornes exactes
    return g;
}

ValeurJson ModeleBayes1D::VersJson() const {
    ValeurJson o = ValeurJson::Objet();
    o["moyennes"] = TableauJson(m_moyennes);
    o["ecartsTypes"] = TableauJson(m_ecartsTypes);
    o["aPriori"] = TableauJson(m_aPriori);
    o["couts"] = MatriceJson(m_couts);
    return o;
}

Resultat<ModeleBayes1D> AjusterBayes1D(const std::vector<double>& valeurs, const std::vector<int>& classes, int nbClasses,
                                       const std::vector<double>& aPrioriImposes,
                                       const std::vector<std::vector<double>>& couts) {
    const std::string phase = "bayes";
    if (nbClasses < 2) return Invalide(phase, "Il faut au moins 2 classes.");
    if (valeurs.size() != classes.size()) return Invalide(phase, "Valeurs et classes de tailles differentes.");
    const size_t K = (size_t)nbClasses;

    std::vector<std::vector<double>> parClasse(K);
    for (size_t i = 0; i < valeurs.size(); ++i) {
        if (classes[i] < 0 || classes[i] >= nbClasses) return Invalide(phase, "Classe hors de [0, nbClasses[.");
        if (!std::isfinite(valeurs[i])) return Invalide(phase, "Valeur non finie.");
        parClasse[(size_t)classes[i]].push_back(valeurs[i]);
    }

    ModeleBayes1D m;
    for (size_t k = 0; k < K; ++k) {
        const std::vector<double>& v = parClasse[k];
        if (v.size() < 2) return Invalide(phase, "La classe " + std::to_string(k) + " a moins de 2 echantillons : ecart-type inestimable.");
        double moy = 0;
        for (double x : v) moy += x;
        moy /= (double)v.size();
        double ss = 0;                                   // sigma^2 = (1/(n-1)) somme (x - moyenne)^2
        for (double x : v) ss += (x - moy) * (x - moy);
        const double sigma = std::sqrt(ss / (double)(v.size() - 1));
        if (!(sigma > 0)) return Invalide(phase, "La classe " + std::to_string(k) + " a un ecart-type nul (echantillons identiques).");
        m.m_moyennes.push_back(moy);
        m.m_ecartsTypes.push_back(sigma);
    }

    if (aPrioriImposes.empty()) {                        // Pr_k = N_k / N
        for (size_t k = 0; k < K; ++k) m.m_aPriori.push_back((double)parClasse[k].size() / (double)valeurs.size());
    } else {
        if (aPrioriImposes.size() != K) return Invalide(phase, "Nombre d'a priori different du nombre de classes.");
        double somme = 0;
        for (double p : aPrioriImposes) {
            if (!(p > 0) || !std::isfinite(p)) return Invalide(phase, "Les a priori doivent etre strictement positifs.");
            somme += p;
        }
        if (std::fabs(somme - 1.0) > 1e-9) return Invalide(phase, "La somme des a priori doit valoir 1.");
        m.m_aPriori = aPrioriImposes;
    }

    if (couts.empty()) {                                 // cout 0/1
        m.m_couts.assign(K, std::vector<double>(K, 1.0));
        for (size_t k = 0; k < K; ++k) m.m_couts[k][k] = 0.0;
    } else {
        if (couts.size() != K) return Invalide(phase, "La matrice de couts doit etre K x K.");
        for (size_t i = 0; i < K; ++i) {
            if (couts[i].size() != K) return Invalide(phase, "La matrice de couts doit etre K x K.");
            for (size_t j = 0; j < K; ++j) {
                if (!std::isfinite(couts[i][j]) || couts[i][j] < 0) return Invalide(phase, "Les couts doivent etre finis et >= 0.");
                if (i == j && couts[i][j] != 0) return Invalide(phase, "La diagonale de la matrice de couts doit etre nulle.");
            }
        }
        m.m_couts = couts;
    }
    return m;
}

ValeurJson CourbesBayes1D::VersJson() const {
    ValeurJson o = ValeurJson::Objet();
    o["x"] = TableauJson(m_x);
    o["densites"] = MatriceJson(m_densites);
    o["posteriori"] = MatriceJson(m_posteriori);
    ValeurJson d = ValeurJson::Tableau();
    for (int c : m_decision) d.Ajouter(ValeurJson(c));
    o["decision"] = d;
    o["seuils"] = TableauJson(m_seuils);
    o["erreurBayes"] = ValeurJson(m_erreurBayes);
    return o;
}

// Remarque : un modele incoherent ou une grille vide donne des courbes vides (pas de Resultat dans le contrat).
CourbesBayes1D CalculerCourbesBayes1D(const ModeleBayes1D& modele, const std::vector<double>& grille) {
    CourbesBayes1D c;
    const size_t K = modele.m_moyennes.size();
    if (K == 0 || modele.m_ecartsTypes.size() != K || modele.m_aPriori.size() != K || modele.m_couts.size() != K) return c;
    for (const auto& l : modele.m_couts) if (l.size() != K) return c;
    for (double s : modele.m_ecartsTypes) if (!(s > 0)) return c;

    const size_t n = grille.size();
    c.m_x = grille;
    c.m_densites.assign(K, std::vector<double>(n, 0.0));
    c.m_posteriori.assign(K, std::vector<double>(n, 0.0));
    c.m_decision.assign(n, 0);
    std::vector<std::vector<double>> risques(K, std::vector<double>(n, 0.0));   // R(j|x)
    std::vector<double> post(K);

    for (size_t i = 0; i < n; ++i) {
        // P(C_k|x) = Pr_k f_k(x) / somme_i Pr_i f_i(x)
        double evidence = 0;
        for (size_t k = 0; k < K; ++k) {
            c.m_densites[k][i] = Gauss(grille[i], modele.m_moyennes[k], modele.m_ecartsTypes[k]);
            post[k] = modele.m_aPriori[k] * c.m_densites[k][i];
            evidence += post[k];
        }
        for (size_t k = 0; k < K; ++k) {
            // Si toutes les densites sont nulles en x, l'a posteriori est l'a priori (convention)
            post[k] = evidence > 0 ? post[k] / evidence : modele.m_aPriori[k];
            c.m_posteriori[k][i] = post[k];
        }
        // Decision de risque minimal ; egalite (a 1e-12 pres) -> plus petit identifiant
        size_t meilleur = 0;
        double rMin = Risque(modele.m_couts, post, 0);
        risques[0][i] = rMin;
        for (size_t j = 1; j < K; ++j) {
            risques[j][i] = Risque(modele.m_couts, post, j);
            if (risques[j][i] < rMin - 1e-12) { rMin = risques[j][i]; meilleur = j; }
        }
        c.m_decision[i] = (int)meilleur;
    }

    // Seuils : la decision change entre i-1 et i (classes a puis b). On interpole lineairement
    // d(x) = R(a|x) - R(b|x), negatif ou nul en i-1, positif ou nul en i, jusqu'a son annulation.
    for (size_t i = 1; i < n; ++i) {
        if (c.m_decision[i] == c.m_decision[i - 1]) continue;
        const size_t a = (size_t)c.m_decision[i - 1], b = (size_t)c.m_decision[i];
        const double d0 = risques[a][i - 1] - risques[b][i - 1];
        const double d1 = risques[a][i] - risques[b][i];
        const double t = (d1 - d0) != 0 ? std::min(1.0, std::max(0.0, d0 / (d0 - d1))) : 0.5;
        c.m_seuils.push_back(grille[i - 1] + t * (grille[i] - grille[i - 1]));
    }

    // Erreur de la regle choisie : integrale (trapezes) de somme_k Pr_k f_k(x) [decision(x) != k].
    // Pour des couts 0/1 c'est l'erreur de Bayes (surface sous min_k Pr_k f_k) ; pour des couts
    // quelconques, c'est la probabilite d'erreur de la regle de risque minimal (et non le risque).
    std::vector<double> integrande(n, 0.0);
    for (size_t i = 0; i < n; ++i)
        for (size_t k = 0; k < K; ++k)
            if ((int)k != c.m_decision[i]) integrande[i] += modele.m_aPriori[k] * c.m_densites[k][i];
    c.m_erreurBayes = Trapezes(grille, integrande);
    return c;
}

Resultat<EstimationParzen1D> Parzen1D(const std::vector<double>& echantillons, double h, const std::string& noyau,
                                      const std::vector<double>& grille, bool avecContributions) {
    const std::string phase = "parzen";
    if (!(h > 0) || !std::isfinite(h)) return Invalide(phase, "La largeur de fenetre h doit etre strictement positive.");
    const bool rectangle = noyau == "RECTANGLE";
    if (!rectangle && noyau != "GAUSSIEN") return Invalide(phase, "Noyau inconnu : \"" + noyau + "\" (RECTANGLE ou GAUSSIEN).");
    if (echantillons.empty()) return Invalide(phase, "Il faut au moins 1 echantillon.");

    EstimationParzen1D e;
    const size_t N = echantillons.size(), n = grille.size();
    e.m_x = grille;
    e.m_densite.assign(n, 0.0);
    if (avecContributions) e.m_contributions.assign(N, std::vector<double>(n, 0.0));
    for (size_t j = 0; j < N; ++j) {
        for (size_t i = 0; i < n; ++i) {
            const double u = (grille[i] - echantillons[j]) / h;
            // phi rectangle : 1 si |u| <= 1/2 ; phi gaussien : exp(-u^2/2)/sqrt(2 pi)
            const double phi = rectangle ? (std::fabs(u) <= 0.5 ? 1.0 : 0.0) : std::exp(-0.5 * u * u) / std::sqrt(2.0 * PI);
            const double part = phi / (h * (double)N);     // contribution (1/N)(1/h) phi
            e.m_densite[i] += part;
            if (avecContributions) e.m_contributions[j][i] = part;
        }
    }
    e.m_integrale = Trapezes(grille, e.m_densite);
    return e;
}

Resultat<std::vector<double>> DensiteKppv1D(const std::vector<double>& echantillons, int k, const std::vector<double>& grille) {
    const std::string phase = "kppv";
    const size_t N = echantillons.size();
    if (N == 0 || k < 1 || (size_t)k > N) return Invalide(phase, "k doit verifier 1 <= k <= N (N = nombre d'echantillons).");
    std::vector<double> dens(grille.size(), 0.0), dist(N);
    for (size_t i = 0; i < grille.size(); ++i) {
        for (size_t j = 0; j < N; ++j) dist[j] = std::fabs(grille[i] - echantillons[j]);
        std::nth_element(dist.begin(), dist.begin() + (k - 1), dist.end());
        const double dk = dist[(size_t)k - 1];            // distance au k-ieme voisin
        if (!(dk > 0)) return Invalide(phase, "Distance au k-ieme voisin nulle en x = " + std::to_string(grille[i]) + " : densite non definie.");
        dens[i] = (double)k / ((double)N * 2.0 * dk);      // p_N(x) = k / (N V), V = 2 d_k
    }
    return dens;
}

} // namespace decid
