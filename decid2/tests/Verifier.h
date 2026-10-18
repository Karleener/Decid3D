// Petit harnais de tests (aucune dependance externe). Chaque test_xxx.cpp :
//   TEST(nom) { VERIFIER(cond); VERIFIER_EGAL(a, b); }
//   int main(int argc, char** argv) { return LancerTests(argc, argv); }
// argv[1] = dossier source decid2 (pour les fixtures), voir DossierSource().
#pragma once
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace verif {

struct CasTest { const char* m_nom; std::function<void()> m_corps; };
inline std::vector<CasTest>& Registre() { static std::vector<CasTest> r; return r; }
inline int& Echecs() { static int e = 0; return e; }
inline std::filesystem::path& Source() { static std::filesystem::path p; return p; }

struct Enregistreur {
    Enregistreur(const char* nom, std::function<void()> corps) { Registre().push_back({nom, std::move(corps)}); }
};

inline void Echec(const char* fichier, int ligne, const std::string& message) {
    std::printf("  ECHEC %s:%d : %s\n", fichier, ligne, message.c_str());
    ++Echecs();
}

// Dossier temporaire propre au test, sous le dossier de build (jamais a cote des sources)
inline std::filesystem::path DossierTemporaire(const std::string& nom) {
    auto p = std::filesystem::current_path() / "tmp_tests" / nom;
    std::error_code ec;
    std::filesystem::remove_all(p, ec);
    std::filesystem::create_directories(p);
    return p;
}

} // namespace verif

#define TEST(nom) \
    static void nom(); \
    static verif::Enregistreur enregistreur_##nom(#nom, nom); \
    static void nom()

#define VERIFIER(cond) \
    do { if (!(cond)) verif::Echec(__FILE__, __LINE__, "condition fausse : " #cond); } while (0)

#define VERIFIER_EGAL(a, b) \
    do { auto va_ = (a); auto vb_ = (b); if (!(va_ == vb_)) { std::ostringstream o_; \
         o_ << #a " != " #b " (" << va_ << " / " << vb_ << ")"; verif::Echec(__FILE__, __LINE__, o_.str()); } } while (0)

#define VERIFIER_PROCHE(a, b, tol) \
    do { double va_ = (a), vb_ = (b); if (!(std::fabs(va_ - vb_) <= (tol))) { std::ostringstream o_; \
         o_ << #a " ~ " #b " (" << va_ << " / " << vb_ << ")"; verif::Echec(__FILE__, __LINE__, o_.str()); } } while (0)

inline int LancerTests(int argc, char** argv) {
    if (argc > 1) verif::Source() = argv[1];
    int echecsAvant;
    for (auto& t : verif::Registre()) {
        echecsAvant = verif::Echecs();
        std::printf("[ %s ]\n", t.m_nom);
        t.m_corps();
        std::printf("  %s\n", verif::Echecs() == echecsAvant ? "ok" : "ECHEC");
    }
    std::printf("%zu tests, %d verification(s) en echec\n", verif::Registre().size(), verif::Echecs());
    return verif::Echecs() == 0 ? 0 : 1;
}
