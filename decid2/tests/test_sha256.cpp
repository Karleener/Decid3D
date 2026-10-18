#include <fstream>
#include "Sha256.h"
#include "Verifier.h"

using namespace decid;

TEST(sha256_vide) {
    VERIFIER_EGAL(Sha256Texte(""), std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
}

TEST(sha256_abc) {
    VERIFIER_EGAL(Sha256Texte("abc"), std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
}

TEST(sha256_448_bits) {
    VERIFIER_EGAL(Sha256Texte("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
                  std::string("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
}

TEST(sha256_million_a) {
    Sha256 h;
    const std::string bloc(1000, 'a');
    for (int i = 0; i < 1000; ++i) h.Ajouter(bloc);
    VERIFIER_EGAL(h.FinirHex(), std::string("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
}

TEST(sha256_morceaux) {
    std::string texte;
    for (int i = 0; i < 1000; ++i) texte.push_back(static_cast<char>('a' + i % 26));
    Sha256 h;
    std::size_t pos = 0;
    std::size_t pas = 1;
    while (pos < texte.size()) {
        const std::size_t n = (pos + pas > texte.size()) ? texte.size() - pos : pas;
        h.Ajouter(texte.data() + pos, n);
        pos += n;
        pas = pas % 70 + 3;
    }
    VERIFIER_EGAL(h.FinirHex(), Sha256Texte(texte));
}

TEST(sha256_fichier) {
    const auto dossier = verif::DossierTemporaire("sha256");
    const auto chemin = dossier / "abc.txt";
    {
        std::ofstream f(chemin, std::ios::binary);
        f << "abc";
    }
    std::uintmax_t taille = 0;
    VERIFIER_EGAL(Sha256Fichier(chemin, &taille), std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    VERIFIER_EGAL(static_cast<unsigned long long>(taille), 3ULL);
    VERIFIER_EGAL(Sha256Fichier(dossier / "absent.bin"), std::string());
}

int main(int argc, char** argv) { return LancerTests(argc, argv); }
