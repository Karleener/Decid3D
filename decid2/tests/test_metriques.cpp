// Tests des metriques : valeurs exactes calculees a la main.
#include <string>
#include <vector>

#include "Metriques.h"
#include "Verifier.h"

using namespace decid;

namespace {

// Construit verites/predictions a partir d'une matrice (lignes = reel, colonnes = predit)
void Depuis(const std::vector<std::vector<int>>& matrice, std::vector<int>& verites, std::vector<int>& predictions) {
    for (size_t r = 0; r < matrice.size(); ++r)
        for (size_t c = 0; c < matrice[r].size(); ++c)
            for (int k = 0; k < matrice[r][c]; ++k) { verites.push_back((int)r); predictions.push_back((int)c); }
}

std::vector<std::string> Noms(int k) {
    std::vector<std::string> noms;
    for (int i = 0; i < k; ++i) noms.push_back("c" + std::to_string(i));
    return noms;
}

RapportMetriques Rapport(const std::vector<std::vector<int>>& matrice) {
    std::vector<int> v, p;
    Depuis(matrice, v, p);
    auto conf = CalculerConfusion(v, p, (int)matrice.size());
    return CalculerMetriques(conf.Valeur(), Noms((int)matrice.size()), "test");
}

} // namespace

TEST(confusion_2x2_asymetrique) {
    RapportMetriques r = Rapport({{4, 1}, {2, 3}});
    VERIFIER_EGAL(r.m_confusion.m_total, 10LL);
    VERIFIER_EGAL(r.m_confusion.Case(0, 1), 1LL);
    VERIFIER_EGAL(r.m_confusion.Case(1, 0), 2LL);
    const MetriquesClasse& c0 = r.m_classes[0];
    VERIFIER_EGAL(c0.m_tp, 4LL);
    VERIFIER_EGAL(c0.m_fp, 2LL);
    VERIFIER_EGAL(c0.m_fn, 1LL);
    VERIFIER_EGAL(c0.m_tn, 3LL);
    VERIFIER_EGAL(c0.m_support, 5LL);
    VERIFIER_PROCHE(c0.m_precision.m_valeur, 4.0 / 6.0, 1e-12);
    VERIFIER_PROCHE(c0.m_rappel.m_valeur, 0.8, 1e-12);
    VERIFIER_PROCHE(c0.m_specificite.m_valeur, 0.6, 1e-12);
    VERIFIER_PROCHE(c0.m_f1.m_valeur, 8.0 / 11.0, 1e-12);
    VERIFIER_PROCHE(c0.m_fpr.m_valeur, 0.4, 1e-12);
    VERIFIER_PROCHE(c0.m_fnr.m_valeur, 0.2, 1e-12);
    VERIFIER_PROCHE(r.m_exactitude.m_valeur, 0.7, 1e-12);
    // classe 1 : TP=3, FP=1, FN=2, TN=4
    const MetriquesClasse& c1 = r.m_classes[1];
    VERIFIER_EGAL(c1.m_fp, 1LL);
    VERIFIER_EGAL(c1.m_fn, 2LL);
    VERIFIER_EGAL(c1.m_tn, 4LL);
    VERIFIER_PROCHE(c1.m_precision.m_valeur, 0.75, 1e-12);
    // macro precision = (4/6 + 3/4)/2
    VERIFIER(r.m_precisionMacro.m_definie);
    VERIFIER_PROCHE(r.m_precisionMacro.m_valeur, (4.0 / 6.0 + 0.75) / 2.0, 1e-12);
    // ponderee rappel = (5*0.8 + 5*0.6)/10
    VERIFIER_PROCHE(r.m_rappelPondere.m_valeur, 0.7, 1e-12);
    VERIFIER(r.m_classesExcluesPonderation.empty());
}

TEST(confusion_10_classes) {
    std::vector<std::vector<int>> m(10, std::vector<int>(10, 0));
    for (int i = 0; i < 10; ++i) m[i][i] = 5;
    m[0][0] = 3; m[0][1] = 2;
    m[3][3] = 4; m[3][4] = 1;
    m[9][9] = 4; m[9][0] = 1;
    RapportMetriques r = Rapport(m);
    VERIFIER_EGAL(r.m_confusion.m_total, 50LL);
    const MetriquesClasse& c1 = r.m_classes[1];
    VERIFIER_EGAL(c1.m_tp, 5LL);
    VERIFIER_EGAL(c1.m_fp, 2LL);
    VERIFIER_EGAL(c1.m_fn, 0LL);
    VERIFIER_EGAL(c1.m_tn, 43LL);
    VERIFIER_PROCHE(c1.m_specificite.m_valeur, 43.0 / 45.0, 1e-12);
    VERIFIER_PROCHE(c1.m_precision.m_valeur, 5.0 / 7.0, 1e-12);
    const MetriquesClasse& c0 = r.m_classes[0];
    VERIFIER_EGAL(c0.m_fp, 1LL);
    VERIFIER_EGAL(c0.m_fn, 2LL);
    VERIFIER_EGAL(c0.m_tn, 50 - 3 - 1 - 2LL);
    VERIFIER_PROCHE(r.m_exactitude.m_valeur, 46.0 / 50.0, 1e-12);
    for (const auto& c : r.m_classes) VERIFIER_EGAL(c.m_tp + c.m_fp + c.m_fn + c.m_tn, 50LL);
    VERIFIER(r.m_specificiteMacro.m_definie);
}

TEST(classe_jamais_predite) {
    RapportMetriques r = Rapport({{2, 0, 0}, {0, 2, 0}, {1, 1, 0}});
    const MetriquesClasse& c2 = r.m_classes[2];
    VERIFIER(!c2.m_precision.m_definie);
    VERIFIER_EGAL(c2.m_precision.m_raison, std::string("aucune ligne predite dans la classe 2"));
    VERIFIER(c2.m_f1.m_definie);
    VERIFIER_PROCHE(c2.m_f1.m_valeur, 0.0, 0.0);
    VERIFIER(c2.m_rappel.m_definie);
    VERIFIER_PROCHE(c2.m_rappel.m_valeur, 0.0, 0.0);
    VERIFIER(!r.m_precisionMacro.m_definie);
    VERIFIER(r.m_f1Macro.m_definie);
    VERIFIER(!r.m_precisionPonderee.m_definie);
}

TEST(classe_absente_du_test) {
    RapportMetriques r = Rapport({{2, 1, 0}, {1, 2, 0}, {0, 0, 0}});
    const MetriquesClasse& c2 = r.m_classes[2];
    VERIFIER_EGAL(c2.m_support, 0LL);
    VERIFIER(!c2.m_rappel.m_definie);
    VERIFIER(!c2.m_f1.m_definie);
    VERIFIER(c2.m_specificite.m_definie);
    VERIFIER_EGAL(r.m_classesExcluesPonderation.size(), (size_t)1);
    VERIFIER_EGAL(r.m_classesExcluesPonderation[0], 2);
    VERIFIER(!r.m_rappelMacro.m_definie);
    VERIFIER(r.m_rappelPondere.m_definie);
    VERIFIER_PROCHE(r.m_rappelPondere.m_valeur, 2.0 / 3.0, 1e-12);
    VERIFIER(r.m_precisionPonderee.m_definie);
    VERIFIER_PROCHE(r.m_precisionPonderee.m_valeur, 2.0 / 3.0, 1e-12);
}

TEST(erreurs_de_confusion) {
    auto r1 = CalculerConfusion({0, 1}, {0, 2}, 2);   // prediction = K
    VERIFIER(!r1.Ok());
    VERIFIER(r1.Err().m_message.find("ligne 2") != std::string::npos);
    auto r2 = CalculerConfusion({0, -1}, {0, 1}, 2);  // verite -1
    VERIFIER(!r2.Ok());
    VERIFIER(r2.Err().m_message.find("ligne 2") != std::string::npos);
    auto r3 = CalculerConfusion({}, {}, 2);           // test vide
    VERIFIER(!r3.Ok());
    VERIFIER(r3.Err().m_message.find("test vide : evaluation impossible") != std::string::npos);
    auto r4 = CalculerConfusion({0, 1}, {0}, 2);      // tailles differentes
    VERIFIER(!r4.Ok());
}

TEST(json_sans_nan) {
    RapportMetriques r = Rapport({{2, 0, 0}, {0, 2, 0}, {1, 1, 0}});
    std::string texte = r.VersJson().Ecrire();
    VERIFIER(texte.find("nan") == std::string::npos);
    VERIFIER(texte.find("NaN") == std::string::npos);
    VERIFIER(texte.find("inf") == std::string::npos);
    VERIFIER(texte.find("\"undefined\"") != std::string::npos);
    VERIFIER(texte.find("decid.metrics/v1") != std::string::npos);
    ValeurJson j = r.VersJson();
    const ValeurJson& prec = j.Membre("classes").Elements()[2].Membre("precision");
    VERIFIER(prec.Membre("value").EstNul());
    VERIFIER_EGAL(prec.Membre("status").Texte(), std::string("undefined"));
    VERIFIER_EGAL(j.Membre("confusion").Membre("axes").Membre("lignes").Texte(), std::string("reel"));
    VERIFIER_EGAL(j.Membre("confusion").Membre("axes").Membre("colonnes").Texte(), std::string("predit"));
    const ValeurJson& f1 = j.Membre("classes").Elements()[2].Membre("f1");
    VERIFIER_EGAL(f1.Membre("status").Texte(), std::string("defined"));
}

TEST(export_csv) {
    RapportMetriques r = Rapport({{2, 0, 0}, {0, 2, 0}, {1, 1, 0}});
    std::string csv = r.VersCsvComptes();
    VERIFIER(csv.rfind("classe;support;TP;FP;FN;TN;precision;rappel;specificite;F1;FPR;FNR\n", 0) == 0);
    // classe c2 : support 2, TP 0, FP 0, FN 2, TN 4, precision vide, rappel 0, specificite 1, F1 0, FPR 0, FNR 1
    VERIFIER(csv.find("c2;2;0;0;2;4;;0;1;0;0;1\n") != std::string::npos);
    VERIFIER(csv.find("reel\\predit;c0;c1;c2") != std::string::npos);
    VERIFIER(csv.find("precision=TP/(TP+FP)") != std::string::npos);
    VERIFIER(csv.find("nan") == std::string::npos);
}

int main(int argc, char** argv) { return LancerTests(argc, argv); }
