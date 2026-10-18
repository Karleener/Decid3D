#include <cmath>
#include <limits>
#include "Json.h"
#include "Verifier.h"

using namespace decid;

TEST(json_aller_retour) {
    ValeurJson o = ValeurJson::Objet();
    o["nom"] = "decid";
    o["n"] = 42;
    o["grand"] = 9000000000LL;
    o["ok"] = true;
    o["rien"] = ValeurJson();
    ValeurJson t = ValeurJson::Tableau();
    t.Ajouter(1);
    t.Ajouter(2.5);
    t.Ajouter("x");
    ValeurJson interne = ValeurJson::Objet();
    interne["z"] = 1;
    interne["a"] = ValeurJson::Tableau();
    t.Ajouter(interne);
    o["liste"] = t;
    for (bool ind : {true, false}) {
        const std::string s = o.Ecrire(ind);
        auto r = LireJson(s);
        VERIFIER(r.Ok());
        if (!r.Ok()) continue;
        VERIFIER_EGAL(r.Valeur().Ecrire(ind), s);
        VERIFIER_EGAL(r.Valeur().Membre("n").Entier(), 42LL);
        VERIFIER_EGAL(r.Valeur().Membre("grand").Entier(), 9000000000LL);
        VERIFIER(r.Valeur().Membre("ok").Booleen());
        VERIFIER(r.Valeur().Membre("rien").EstNul());
        VERIFIER(r.Valeur().Membre("absent").EstNul());
        VERIFIER_EGAL(r.Valeur().Membre("liste").Elements().size(), std::size_t(4));
        VERIFIER_EGAL(r.Valeur().Membre("liste").Elements()[3].Membre("z").Entier(), 1LL);
    }
}

TEST(json_ecriture_deterministe) {
    ValeurJson o = ValeurJson::Objet();
    o["b"] = 1;
    o["a"] = 2;
    VERIFIER_EGAL(o.Ecrire(false), std::string("{\"a\":2,\"b\":1}"));
    VERIFIER_EGAL(o.Ecrire(true), std::string("{\n  \"a\": 2,\n  \"b\": 1\n}"));
}

TEST(json_doubles) {
    for (double v : {0.1, 1e-300, -2.5e17, 3.141592653589793, 1.0}) {
        ValeurJson o = ValeurJson::Objet();
        o["v"] = v;
        auto r = LireJson(o.Ecrire(false));
        VERIFIER(r.Ok());
        if (r.Ok()) VERIFIER(r.Valeur().Membre("v").Nombre() == v);
    }
    auto r = LireJson("[0.1, 1e-300, 12, 1E2]");
    VERIFIER(r.Ok());
    if (r.Ok()) {
        VERIFIER(r.Valeur().Elements()[0].Nombre() == 0.1);
        VERIFIER(r.Valeur().Elements()[1].Nombre() == 1e-300);
        VERIFIER(r.Valeur().Elements()[2].GetType() == ValeurJson::Type::Entier);
        VERIFIER(r.Valeur().Elements()[3].GetType() == ValeurJson::Type::Nombre);
    }
}

TEST(json_non_fini_nul) {
    ValeurJson o = ValeurJson::Objet();
    o["nan"] = std::numeric_limits<double>::quiet_NaN();
    o["inf"] = std::numeric_limits<double>::infinity();
    VERIFIER_EGAL(o.Ecrire(false), std::string("{\"inf\":null,\"nan\":null}"));
}

TEST(json_texte) {
    const std::string source = std::string("\xC3\xA9 \xF0\x9F\x98\x80 \"q\" \\ \x01 \n\t");
    ValeurJson o = ValeurJson::Objet();
    o["t"] = source;
    const std::string s = o.Ecrire(false);
    VERIFIER(s.find("\\u0001") != std::string::npos);
    VERIFIER(s.find("\xC3\xA9") != std::string::npos);
    auto r = LireJson(s);
    VERIFIER(r.Ok());
    if (r.Ok()) VERIFIER_EGAL(r.Valeur().Membre("t").Texte(), source);

    auto u = LireJson("\"\\u00e9 \\ud83d\\ude00\"");
    VERIFIER(u.Ok());
    if (u.Ok()) VERIFIER_EGAL(u.Valeur().Texte(), std::string("\xC3\xA9 \xF0\x9F\x98\x80"));
}

TEST(json_erreurs) {
    auto a = LireJson("[1,2,]");
    VERIFIER(!a.Ok());
    auto b = LireJson("{1: 2}");
    VERIFIER(!b.Ok());
    auto c = LireJson("{\"a\":1} x");
    VERIFIER(!c.Ok());
    auto d = LireJson("\"abc");
    VERIFIER(!d.Ok());
    auto e = LireJson("{\n  \"a\": 1,\n  }");
    VERIFIER(!e.Ok());
    if (!e.Ok()) {
        VERIFIER_EGAL(e.Err().m_ligne, 3L);
        VERIFIER(e.Err().m_colonne > 0);
    }
    VERIFIER(!LireJson("").Ok());
    VERIFIER(!LireJson("01").Ok());
    VERIFIER(!LireJson("{\"a\":1,}").Ok());
    VERIFIER(LireJson("  [ ]  ").Ok());
}

TEST(bom_utf8_ignore) {
    // Fichier enregistre avec BOM (Bloc-notes, PowerShell 5) : accepte
    auto r = decid::LireJson("\xEF\xBB\xBF" "{\"a\":1}");
    VERIFIER(r.Ok());
    if (r.Ok()) VERIFIER_EGAL(r.Valeur().Membre("a").Entier(), 1LL);
    // BOM ailleurs qu'au debut : refuse
    VERIFIER(!decid::LireJson("{\"a\":\xEF\xBB\xBF" "1}").Ok());
}

int main(int argc, char** argv) { return LancerTests(argc, argv); }
