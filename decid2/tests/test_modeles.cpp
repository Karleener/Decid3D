// Tests des modeles SVM et foret aleatoire (ACC-07) et de leur inspection.
// Donnees SYNTHETIQUES generees de facon deterministe (LCG maison) ; fichiers dans verif::DossierTemporaire.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "Fichiers.h"
#include "Modeles.h"
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

// Nuages 2D : centres donnes, n points par classe.
Corpus Nuages(const std::vector<std::pair<double, double>>& centres, int n, std::uint64_t graine) {
    Corpus c;
    c.m_nbClasses = (int)centres.size();
    c.m_x = cv::Mat((int)centres.size() * n, 2, CV_32F);
    Lcg g{graine};
    int ligne = 0;
    for (int k = 0; k < c.m_nbClasses; ++k)
        for (int i = 0; i < n; ++i, ++ligne) {
            c.m_x.at<float>(ligne, 0) = (float)(centres[(size_t)k].first + g.Bruit());
            c.m_x.at<float>(ligne, 1) = (float)(centres[(size_t)k].second + g.Bruit());
            c.m_labels.push_back(k);
        }
    return c;
}

Corpus DeuxClasses() { return Nuages({{0.0, 0.0}, {2.0, 2.0}}, 60, 7); }                 // se chevauchent un peu
Corpus TroisClasses() { return Nuages({{0.0, 0.0}, {4.0, 0.0}, {2.0, 4.0}}, 40, 11); }

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

double Precision(const std::vector<int>& predites, const std::vector<int>& vraies) {
    int bons = 0;
    for (size_t i = 0; i < vraies.size(); ++i) bons += predites[i] == vraies[i] ? 1 : 0;
    return (double)bons / (double)vraies.size();
}

void Ecrire(const fs::path& chemin, const std::string& texte) {
    std::ofstream f(chemin, std::ios::binary);
    f.write(texte.data(), (std::streamsize)texte.size());
}

Resultat<Modele> Svm(const Corpus& c, const char* noyau, double cc, double gamma, double degre = 3.0, double coef0 = 0.0) {
    ParametresSvm p;
    p.m_noyau = noyau; p.m_c = cc; p.m_gamma = gamma; p.m_degre = degre; p.m_coef0 = coef0;
    return EntrainerSvm(c.m_x, c.m_labels, c.m_nbClasses, p);
}

Resultat<Modele> Foret(const Corpus& c, int nbArbres, std::uint64_t graine) {
    ParametresForet p;
    p.m_nbArbres = nbArbres;
    return EntrainerForet(c.m_x, c.m_labels, c.m_nbClasses, p, graine);
}

// Compare la decision recalculee a la sortie brute d'OpenCV sur tout le corpus (2 classes).
void VerifierDecision(const Modele& m, const Corpus& c, const char* nom) {
    auto classes = Predire(m, c.m_x);
    if (!Reussi(classes, nom)) return;
    int ecarts = 0, signes = 0;
    for (int i = 0; i < c.m_x.rows; ++i) {
        cv::Mat ligne = c.m_x.row(i).clone();
        cv::Mat brut;
        m.m_opencv->predict(ligne, brut, cv::ml::StatModel::RAW_OUTPUT);
        double attendu = brut.at<float>(0);
        auto recalcule = DecisionSvmRecalculee(m, ligne.ptr<float>(0));
        if (!Reussi(recalcule, nom)) return;
        double v = recalcule.Valeur();
        if (!(std::fabs(v - attendu) <= std::max(1e-4, 1e-3 * std::fabs(attendu)))) ++ecarts;
        if (std::fabs(attendu) > 1e-6 && ((attendu > 0) != (classes.Valeur()[(size_t)i] == 0))) ++signes;
    }
    if (ecarts > 0) verif::Echec(__FILE__, __LINE__, std::string(nom) + " : " + std::to_string(ecarts) + " decision(s) recalculee(s) differente(s) de RAW_OUTPUT");
    if (signes > 0) verif::Echec(__FILE__, __LINE__, std::string(nom) + " : " + std::to_string(signes) + " signe(s) incoherent(s) avec la classe (positif => classe 0)");
}

} // namespace

// ---------- SVM ----------

TEST(svm_rbf_parametres_effectifs_et_precision) {
    Corpus c = DeuxClasses();
    auto m = Svm(c, "RBF", 1.0, 0.5);
    if (!Reussi(m, "EntrainerSvm RBF")) return;
    const ValeurJson& e = m.Valeur().m_parametresEffectifs;
    VERIFIER_EGAL(e.Membre("noyau").Texte(), std::string("RBF"));
    VERIFIER_PROCHE(e.Membre("c").Nombre(), 1.0, 1e-12);
    VERIFIER_PROCHE(e.Membre("gamma").Nombre(), 0.5, 1e-12);
    VERIFIER(e.Membre("nb_vecteurs_support").Entier() > 0);
    VERIFIER_EGAL(e.Membre("nb_attributs").Entier(), 2LL);
    VERIFIER_PROCHE(m.Valeur().m_parametresDemandes.Membre("gamma").Nombre(), 0.5, 1e-12);
    auto classes = Predire(m.Valeur(), c.m_x);
    if (Reussi(classes, "Predire")) {
        VERIFIER_EGAL(classes.Valeur().size(), c.m_labels.size());
        VERIFIER(Precision(classes.Valeur(), c.m_labels) > 0.85);
    }
}

TEST(svm_sauver_charger_donne_les_memes_predictions) {
    auto d = verif::DossierTemporaire("svm_aller_retour");
    Corpus c = DeuxClasses();
    auto m = Svm(c, "RBF", 1.0, 0.5);
    if (!Reussi(m, "EntrainerSvm")) return;
    VERIFIER(SauverModele(m.Valeur(), d / "svm.yml").Ok());
    auto relu = ChargerModele(Famille::Svm, d / "svm.yml", 2);
    if (!Reussi(relu, "ChargerModele")) return;
    VERIFIER_EGAL(relu.Valeur().m_nbAttributs, 2);
    auto avant = Predire(m.Valeur(), c.m_x), apres = Predire(relu.Valeur(), c.m_x);
    if (Reussi(avant, "avant") && Reussi(apres, "apres")) VERIFIER(avant.Valeur() == apres.Valeur());
    VERIFIER_PROCHE(relu.Valeur().m_parametresEffectifs.Membre("gamma").Nombre(), 0.5, 1e-12);
    VERIFIER_EGAL(relu.Valeur().m_parametresEffectifs.Membre("nb_vecteurs_support").Entier(),
                  m.Valeur().m_parametresEffectifs.Membre("nb_vecteurs_support").Entier());
    // la decision recalculee reste valable sur le modele relu
    VerifierDecision(relu.Valeur(), c, "decision (modele relu)");
}

TEST(svm_decision_recalculee_rbf_lineaire_poly) {
    Corpus c = DeuxClasses();
    auto rbf = Svm(c, "RBF", 1.0, 0.5);
    if (Reussi(rbf, "RBF")) VerifierDecision(rbf.Valeur(), c, "decision RBF");
    auto lin = Svm(c, "LINEAIRE", 1.0, 1.0);
    if (Reussi(lin, "LINEAIRE")) VerifierDecision(lin.Valeur(), c, "decision LINEAIRE");
    auto poly = Svm(c, "POLY", 1.0, 0.5, 2.0, 1.0);
    if (Reussi(poly, "POLY")) VerifierDecision(poly.Valeur(), c, "decision POLY");
    auto sig = Svm(c, "SIGMOIDE", 1.0, 0.1, 3.0, 0.0);
    if (Reussi(sig, "SIGMOIDE")) VerifierDecision(sig.Valeur(), c, "decision SIGMOIDE");
}

TEST(svm_decision_refusee_pour_une_foret) {
    Corpus c = DeuxClasses();
    auto f = Foret(c, 5, 1);
    if (!Reussi(f, "foret")) return;
    float x[2] = {0.f, 0.f};
    Refuse(DecisionSvmRecalculee(f.Valeur(), x), "decision SVM sur une foret");
}

TEST(svm_inspection_deux_et_trois_classes) {
    Corpus c2 = DeuxClasses();
    auto m2 = Svm(c2, "RBF", 1.0, 0.5);
    if (Reussi(m2, "SVM 2 classes")) {
        ValeurJson j = InspecterModele(m2.Valeur());
        VERIFIER(j.Contient("schema"));
        VERIFIER_EGAL(j.Membre("schema").Texte(), std::string("decid.inspection/1"));
        VERIFIER(j.Contient("vecteurs_support"));
        VERIFIER(j.Contient("fonctions_decision"));
        VERIFIER_EGAL(j.Membre("fonctions_decision").Elements().size(), (size_t)1);
        VERIFIER_EGAL((long long)j.Membre("vecteurs_support").Elements().size(),
                      m2.Valeur().m_parametresEffectifs.Membre("nb_vecteurs_support").Entier());
        const ValeurJson& f = j.Membre("fonctions_decision").Elements().at(0);
        VERIFIER(f.Contient("rho") && f.Contient("alpha") && f.Contient("indices_vecteurs"));
        VERIFIER_EGAL(f.Membre("alpha").Elements().size(), f.Membre("indices_vecteurs").Elements().size());
        VERIFIER_EGAL(j.Membre("parametres_effectifs").Membre("noyau").Texte(), std::string("RBF"));
        VERIFIER(!j.Ecrire().empty());     // serialisable (aucun nombre non fini)
    }
    Corpus c3 = TroisClasses();
    auto m3 = Svm(c3, "RBF", 10.0, 0.5);
    if (Reussi(m3, "SVM 3 classes")) {
        ValeurJson j = InspecterModele(m3.Valeur());
        VERIFIER_EGAL(j.Membre("fonctions_decision").Elements().size(), (size_t)3);   // une par paire
        auto classes = Predire(m3.Valeur(), c3.m_x);
        if (Reussi(classes, "Predire 3 classes")) VERIFIER(Precision(classes.Valeur(), c3.m_labels) > 0.9);
    }
}

TEST(svm_lineaire_signale_les_vecteurs_compresses) {
    Corpus c = DeuxClasses();
    auto lin = Svm(c, "LINEAIRE", 1.0, 1.0);
    if (!Reussi(lin, "LINEAIRE")) return;
    ValeurJson j = InspecterModele(lin.Valeur());
    VERIFIER(j.Membre("vecteurs_compresses_lineaire").Booleen());
    VERIFIER(j.Contient("vecteurs_support_origine"));
}

// ---------- foret ----------

TEST(foret_graine_reproductible_et_nombre_d_arbres) {
    Corpus c = DeuxClasses();
    auto a = Foret(c, 15, 42), b = Foret(c, 15, 42);
    if (!Reussi(a, "foret a") || !Reussi(b, "foret b")) return;
    VERIFIER_EGAL(a.Valeur().m_parametresEffectifs.Membre("nb_arbres").Entier(), 15LL);   // demande = effectif
    VERIFIER_EGAL(a.Valeur().m_parametresDemandes.Membre("nb_arbres").Entier(), 15LL);
    auto pa = Predire(a.Valeur(), c.m_x), pb = Predire(b.Valeur(), c.m_x);
    if (Reussi(pa, "pa") && Reussi(pb, "pb")) {
        VERIFIER(pa.Valeur() == pb.Valeur());
        VERIFIER(Precision(pa.Valeur(), c.m_labels) > 0.85);
    }
    VERIFIER_EGAL(InspecterModele(a.Valeur()).Ecrire(false), InspecterModele(b.Valeur()).Ecrire(false));
    for (int n : {1, 7, 40}) {
        auto f = Foret(c, n, 3);
        if (Reussi(f, "nombre d'arbres")) VERIFIER_EGAL(f.Valeur().m_parametresEffectifs.Membre("nb_arbres").Entier(), (long long)n);
    }
}

TEST(foret_votes_somme_et_argmax) {
    Corpus c = DeuxClasses();
    auto f = Foret(c, 15, 5);     // 15 arbres, 2 classes : pas d'egalite possible
    if (!Reussi(f, "foret")) return;
    auto classes = Predire(f.Valeur(), c.m_x);
    if (!Reussi(classes, "Predire")) return;
    int ecartsSomme = 0, ecartsArgmax = 0;
    for (int i = 0; i < c.m_x.rows; ++i) {
        auto votes = VotesForet(f.Valeur(), c.m_x.row(i).clone());
        if (!Reussi(votes, "VotesForet")) return;
        int somme = 0;
        for (int v : votes.Valeur()) somme += v;
        if (somme != 15) ++ecartsSomme;
        int meilleur = (int)(std::max_element(votes.Valeur().begin(), votes.Valeur().end()) - votes.Valeur().begin());
        if (meilleur != classes.Valeur()[(size_t)i]) ++ecartsArgmax;
    }
    VERIFIER_EGAL(ecartsSomme, 0);
    VERIFIER_EGAL(ecartsArgmax, 0);
    Corpus c3 = TroisClasses();
    auto f3 = Foret(c3, 12, 5);
    if (Reussi(f3, "foret 3 classes")) {
        auto classes3 = Predire(f3.Valeur(), c3.m_x);
        auto votes3 = VotesForet(f3.Valeur(), c3.m_x.row(0).clone());
        if (Reussi(classes3, "Predire 3") && Reussi(votes3, "votes 3")) {
            int s = 0;
            for (int v : votes3.Valeur()) s += v;
            VERIFIER_EGAL(s, 12);
            VERIFIER_EGAL(votes3.Valeur().size(), (size_t)3);
        }
    }
    auto svm = Svm(c, "RBF", 1.0, 0.5);
    if (Reussi(svm, "svm")) Refuse(VotesForet(svm.Valeur(), c.m_x.row(0).clone()), "votes sur un SVM");
}

TEST(foret_inspection_arbres_importance_et_oob) {
    Corpus c = DeuxClasses();
    auto f = Foret(c, 9, 5);
    if (!Reussi(f, "foret")) return;
    ValeurJson j = InspecterModele(f.Valeur());
    VERIFIER(j.Contient("arbres"));
    VERIFIER_EGAL(j.Membre("arbres").Elements().size(), (size_t)9);
    for (auto& arbre : j.Membre("arbres").Elements()) {
        VERIFIER(arbre.Contient("racine"));
        VERIFIER(!arbre.Membre("noeuds").Elements().empty());
    }
    VERIFIER(j.Contient("importance_variables"));
    VERIFIER_EGAL(j.Membre("importance_variables").Elements().size(), (size_t)c.m_x.cols);
    VERIFIER(j.Contient("erreur_hors_sac"));
    if (!j.Membre("erreur_hors_sac").EstNul()) {
        double oob = j.Membre("erreur_hors_sac").Nombre();
        VERIFIER(oob >= -1.0 && oob <= 100.0);
    }
    VERIFIER_EGAL(j.Membre("famille").Texte(), std::string("foret_aleatoire"));
}

// ---------- erreurs ----------

TEST(entrainement_refuse_les_entrees_invalides) {
    Corpus c = DeuxClasses();
    Corpus une = c;
    std::fill(une.m_labels.begin(), une.m_labels.end(), 0);
    auto s = Svm(une, "RBF", 1.0, 0.5);
    if (Refuse(s, "SVM une seule classe")) VERIFIER(s.Err().m_code == CodeErreur::DonneesInvalides);
    auto f = Foret(une, 5, 1);
    if (Refuse(f, "foret une seule classe")) VERIFIER(f.Err().m_code == CodeErreur::DonneesInvalides);

    Refuse(Svm(c, "RBF", 0.0, 0.5), "C nul");
    Refuse(Svm(c, "RBF", 1.0, -1.0), "gamma negatif");
    Refuse(Svm(c, "INCONNU", 1.0, 0.5), "noyau inconnu");
    Refuse(Foret(c, 0, 1), "zero arbre");

    Corpus mauvaisType = c;
    mauvaisType.m_x.convertTo(mauvaisType.m_x, CV_64F);
    Refuse(EntrainerSvm(mauvaisType.m_x, c.m_labels, 2, ParametresSvm{}), "matrice CV_64F");
    std::vector<int> courts(c.m_labels.begin(), c.m_labels.begin() + 10);
    Refuse(EntrainerSvm(c.m_x, courts, 2, ParametresSvm{}), "labels trop courts");
    std::vector<int> horsPlage = c.m_labels;
    horsPlage[0] = 5;
    Refuse(EntrainerSvm(c.m_x, horsPlage, 2, ParametresSvm{}), "label hors plage");
}

TEST(predire_refuse_une_dimension_incompatible) {
    Corpus c = DeuxClasses();
    auto svm = Svm(c, "RBF", 1.0, 0.5);
    auto foret = Foret(c, 5, 1);
    cv::Mat troisColonnes = cv::Mat::zeros(4, 3, CV_32F);
    cv::Mat unique = cv::Mat::zeros(4, 1, CV_32F);
    cv::Mat double2 = cv::Mat::zeros(4, 2, CV_64F);
    for (const Resultat<Modele>* m : {&svm, &foret}) {
        if (!Reussi(*m, "modele")) continue;
        auto r = Predire(m->Valeur(), troisColonnes);
        if (Refuse(r, "3 colonnes au lieu de 2")) VERIFIER(r.Err().m_code == CodeErreur::ModeleIncompatible);
        Refuse(Predire(m->Valeur(), unique), "1 colonne");
        Refuse(Predire(m->Valeur(), double2), "matrice CV_64F");
    }
    Modele vide;
    Refuse(Predire(vide, c.m_x), "modele vide");
}

TEST(charger_modele_corrompu_donne_une_erreur_sans_planter) {
    auto d = verif::DossierTemporaire("modele_corrompu");
    Corpus c = DeuxClasses();
    auto svm = Svm(c, "RBF", 1.0, 0.5);
    auto foret = Foret(c, 5, 1);
    if (!Reussi(svm, "svm") || !Reussi(foret, "foret")) return;
    VERIFIER(SauverModele(svm.Valeur(), d / "svm.yml").Ok());
    VERIFIER(SauverModele(foret.Valeur(), d / "foret.yml").Ok());
    for (const char* nom : {"svm.yml", "foret.yml"}) {
        Famille famille = std::string(nom) == "svm.yml" ? Famille::Svm : Famille::ForetAleatoire;
        auto texte = LireFichierTexte(d / nom);
        if (!Reussi(texte, "relecture du fichier modele")) continue;
        const std::string& t = texte.Valeur();
        Ecrire(d / "tronque.yml", t.substr(0, t.size() / 3));
        Refuse(ChargerModele(famille, d / "tronque.yml", 2), "fichier tronque (tiers)");
        Ecrire(d / "tronque2.yml", t.substr(0, t.size() - t.size() / 10));
        Refuse(ChargerModele(famille, d / "tronque2.yml", 2), "fichier tronque (dernier dixieme)");
        Ecrire(d / "vide.yml", "");
        Refuse(ChargerModele(famille, d / "vide.yml", 2), "fichier vide");
        Ecrire(d / "texte.yml", "bonjour, ceci n'est pas un modele\nligne deux\n");
        Refuse(ChargerModele(famille, d / "texte.yml", 2), "texte quelconque");
        Ecrire(d / "binaire.yml", std::string("\x00\x01\xFF\xFE{[:", 7));
        Refuse(ChargerModele(famille, d / "binaire.yml", 2), "octets quelconques");
        Refuse(ChargerModele(famille, d / "absent.yml", 2), "fichier absent");
    }
    // un modele de l'autre famille ne doit pas etre accepte comme valide
    Refuse(ChargerModele(Famille::ForetAleatoire, d / "svm.yml", 2), "fichier SVM lu comme foret");
    Refuse(ChargerModele(Famille::Svm, d / "foret.yml", 2), "fichier foret lu comme SVM");
}

TEST(sauver_modele_n_ecrase_jamais_un_fichier_existant) {
    auto d = verif::DossierTemporaire("modele_ecrasement");
    Corpus c = DeuxClasses();
    auto svm = Svm(c, "RBF", 1.0, 0.5);
    if (!Reussi(svm, "svm")) return;
    Ecrire(d / "existant.yml", "contenu a ne pas perdre");
    auto st = SauverModele(svm.Valeur(), d / "existant.yml");
    if (Refuse(st, "ecrasement")) VERIFIER(st.Err().m_code == CodeErreur::EcritureRefusee);
    auto texte = LireFichierTexte(d / "existant.yml");
    if (Reussi(texte, "relecture")) VERIFIER_EGAL(texte.Valeur(), std::string("contenu a ne pas perdre"));
    VERIFIER(SauverModele(svm.Valeur(), d / "nouveau.yml").Ok());
    Refuse(SauverModele(svm.Valeur(), d / "nouveau.yml"), "second enregistrement au meme endroit");
    VERIFIER(!fs::exists(d / "nouveau.yml.tmp"));
}

int main(int argc, char** argv) { return LancerTests(argc, argv); }
