#include "Trace.h"
#include "Verifier.h"

using namespace decid;

static EtatTrace Faire(int i, std::size_t taillevecteur = 0) {
    EtatTrace e;
    e.m_iteration = i;
    e.m_phase = "mise_a_jour";
    e.m_grandeurs.push_back({"J", 100.0 / (i + 1)});
    e.m_vecteur.assign(taillevecteur, 1.0f);
    return e;
}

TEST(trace_decimation_par_nombre) {
    CollecteurTrace c(64, 1u << 30);
    for (int i = 0; i < 1000; ++i) c.Recevoir(Faire(i));
    c.Terminer();
    const auto& etats = c.Etats();
    VERIFIER(etats.size() <= 65);
    VERIFIER(etats.size() >= 2);
    VERIFIER_EGAL(etats.front().m_iteration, 0);
    VERIFIER_EGAL(etats.back().m_iteration, 999);
    for (std::size_t i = 1; i < etats.size(); ++i) {
        VERIFIER(etats[i].m_iteration > etats[i - 1].m_iteration);
    }
    VERIFIER(c.Pas() > 1);
}

TEST(trace_budget_octets) {
    const std::size_t budget = 4000;
    CollecteurTrace c(100000, budget);
    for (int i = 0; i < 500; ++i) {
        c.Recevoir(Faire(i, 20));
        VERIFIER(c.Octets() <= budget);
    }
    VERIFIER_EGAL(c.Etats().front().m_iteration, 0);
}

TEST(trace_sans_depassement) {
    CollecteurTrace c(64, 1u << 30);
    for (int i = 0; i < 40; ++i) c.Recevoir(Faire(i));
    c.Terminer();
    VERIFIER_EGAL(c.Etats().size(), std::size_t(40));
    VERIFIER_EGAL(c.Pas(), 1);
}

TEST(trace_json) {
    CollecteurTrace c(8, 1u << 20);
    for (int i = 0; i < 3; ++i) c.Recevoir(Faire(i));
    c.Terminer();
    const ValeurJson j = c.VersJson();
    VERIFIER_EGAL(j.Membre("recus").Entier(), 3LL);
    VERIFIER_EGAL(j.Membre("pas").Entier(), 1LL);
    VERIFIER_EGAL(j.Membre("etats").Elements().size(), std::size_t(3));
    const auto& e = j.Membre("etats").Elements()[1];
    VERIFIER_EGAL(e.Membre("iteration").Entier(), 1LL);
    VERIFIER_EGAL(e.Membre("phase").Texte(), std::string("mise_a_jour"));
    VERIFIER_PROCHE(e.Membre("grandeurs").Membre("J").Nombre(), 50.0, 1e-12);
}

int main(int argc, char** argv) { return LancerTests(argc, argv); }
