#include "Sha256.h"

#include <cstring>
#include <fstream>
#include <vector>

namespace decid {

namespace {

const std::uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

std::uint32_t Rot(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

} // namespace

Sha256::Sha256() {
    const std::uint32_t init[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::memcpy(m_etat, init, sizeof(m_etat));
    std::memset(m_tampon, 0, sizeof(m_tampon));
}

void Sha256::TraiterBloc(const std::uint8_t* bloc) {
    std::uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (std::uint32_t(bloc[4 * i]) << 24) | (std::uint32_t(bloc[4 * i + 1]) << 16) |
               (std::uint32_t(bloc[4 * i + 2]) << 8) | std::uint32_t(bloc[4 * i + 3]);
    }
    for (int i = 16; i < 64; ++i) {
        const std::uint32_t s0 = Rot(w[i - 15], 7) ^ Rot(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const std::uint32_t s1 = Rot(w[i - 2], 17) ^ Rot(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    std::uint32_t a = m_etat[0], b = m_etat[1], c = m_etat[2], d = m_etat[3];
    std::uint32_t e = m_etat[4], f = m_etat[5], g = m_etat[6], h = m_etat[7];
    for (int i = 0; i < 64; ++i) {
        const std::uint32_t S1 = Rot(e, 6) ^ Rot(e, 11) ^ Rot(e, 25);
        const std::uint32_t ch = (e & f) ^ (~e & g);
        const std::uint32_t t1 = h + S1 + ch + K[i] + w[i];
        const std::uint32_t S0 = Rot(a, 2) ^ Rot(a, 13) ^ Rot(a, 22);
        const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t t2 = S0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    m_etat[0] += a; m_etat[1] += b; m_etat[2] += c; m_etat[3] += d;
    m_etat[4] += e; m_etat[5] += f; m_etat[6] += g; m_etat[7] += h;
}

void Sha256::Ajouter(const void* donnees, std::size_t taille) {
    const std::uint8_t* p = static_cast<const std::uint8_t*>(donnees);
    m_nbOctets += taille;
    while (taille > 0) {
        if (m_tailleTampon == 0 && taille >= 64) {
            TraiterBloc(p);
            p += 64;
            taille -= 64;
            continue;
        }
        const std::size_t n = (64 - m_tailleTampon < taille) ? 64 - m_tailleTampon : taille;
        std::memcpy(m_tampon + m_tailleTampon, p, n);
        m_tailleTampon += n;
        p += n;
        taille -= n;
        if (m_tailleTampon == 64) {
            TraiterBloc(m_tampon);
            m_tailleTampon = 0;
        }
    }
}

std::string Sha256::FinirHex() {
    const std::uint64_t bits = m_nbOctets * 8;
    const std::uint8_t un = 0x80;
    const std::uint8_t zero = 0;
    Ajouter(&un, 1);
    while (m_tailleTampon != 56) Ajouter(&zero, 1);
    std::uint8_t longueur[8];
    for (int i = 0; i < 8; ++i) longueur[i] = static_cast<std::uint8_t>(bits >> (56 - 8 * i));
    Ajouter(longueur, 8);

    static const char hex[] = "0123456789abcdef";
    std::string sortie;
    sortie.reserve(64);
    for (int i = 0; i < 8; ++i) {
        for (int j = 28; j >= 0; j -= 4) sortie.push_back(hex[(m_etat[i] >> j) & 0xF]);
    }
    return sortie;
}

std::string Sha256Texte(const std::string& texte) {
    Sha256 h;
    h.Ajouter(texte);
    return h.FinirHex();
}

std::string Sha256Fichier(const std::filesystem::path& chemin, std::uintmax_t* taille) {
    std::ifstream f(chemin, std::ios::binary);
    if (!f) return std::string();
    Sha256 h;
    std::vector<char> tampon(1 << 16);
    std::uintmax_t total = 0;
    while (f) {
        f.read(tampon.data(), static_cast<std::streamsize>(tampon.size()));
        const std::streamsize lu = f.gcount();
        if (lu > 0) {
            h.Ajouter(tampon.data(), static_cast<std::size_t>(lu));
            total += static_cast<std::uintmax_t>(lu);
        }
    }
    if (f.bad()) return std::string();
    if (taille) *taille = total;
    return h.FinirHex();
}

} // namespace decid
