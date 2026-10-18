#include "Capture.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <GL/gl.h>

namespace app {

namespace {

std::uint32_t TableCrc(std::uint32_t n) {
    std::uint32_t c = n;
    for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
    return c;
}

std::uint32_t Crc32(const std::uint8_t* b, size_t n) {
    static const std::vector<std::uint32_t> table = [] {
        std::vector<std::uint32_t> t(256);
        for (std::uint32_t i = 0; i < 256; ++i) t[i] = TableCrc(i);
        return t;
    }();
    std::uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i) c = table[(c ^ b[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

void Be32(std::vector<std::uint8_t>& v, std::uint32_t x) {
    v.push_back((std::uint8_t)(x >> 24));
    v.push_back((std::uint8_t)(x >> 16));
    v.push_back((std::uint8_t)(x >> 8));
    v.push_back((std::uint8_t)x);
}

void Bloc(std::vector<std::uint8_t>& sortie, const char* type, const std::vector<std::uint8_t>& donnees) {
    Be32(sortie, (std::uint32_t)donnees.size());
    std::vector<std::uint8_t> td(type, type + 4);
    td.insert(td.end(), donnees.begin(), donnees.end());
    sortie.insert(sortie.end(), td.begin(), td.end());
    Be32(sortie, Crc32(td.data(), td.size()));
}

} // namespace

bool EcrirePng(const std::filesystem::path& chemin, const std::uint8_t* rgba, int w, int h) {
    std::vector<std::uint8_t> brut;
    brut.reserve((size_t)h * ((size_t)w * 3 + 1));
    for (int y = 0; y < h; ++y) {
        brut.push_back(0);
        for (int x = 0; x < w; ++x) {
            const std::uint8_t* s = rgba + ((size_t)y * w + x) * 4;
            brut.push_back(s[0]);
            brut.push_back(s[1]);
            brut.push_back(s[2]);
        }
    }
    std::vector<std::uint8_t> z{0x78, 0x01};
    std::uint32_t a = 1, b = 0;
    for (std::uint8_t c : brut) {
        a = (a + c) % 65521;
        b = (b + a) % 65521;
    }
    size_t pos = 0;
    do {
        const size_t n = std::min<size_t>(65535, brut.size() - pos);
        const bool dernier = pos + n >= brut.size();
        z.push_back(dernier ? 1 : 0);
        z.push_back((std::uint8_t)(n & 0xFF));
        z.push_back((std::uint8_t)(n >> 8));
        z.push_back((std::uint8_t)(~n & 0xFF));
        z.push_back((std::uint8_t)((~n >> 8) & 0xFF));
        z.insert(z.end(), brut.begin() + (std::ptrdiff_t)pos, brut.begin() + (std::ptrdiff_t)(pos + n));
        pos += n;
    } while (pos < brut.size());
    Be32(z, (b << 16) | a);

    std::vector<std::uint8_t> sortie{0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<std::uint8_t> ihdr;
    Be32(ihdr, (std::uint32_t)w);
    Be32(ihdr, (std::uint32_t)h);
    const std::uint8_t entete[5] = {8, 2, 0, 0, 0};
    ihdr.insert(ihdr.end(), entete, entete + 5);
    Bloc(sortie, "IHDR", ihdr);
    Bloc(sortie, "IDAT", z);
    Bloc(sortie, "IEND", {});
    std::error_code ec;
    std::filesystem::create_directories(chemin.parent_path(), ec);
    std::ofstream f(chemin, std::ios::binary);
    f.write(reinterpret_cast<const char*>(sortie.data()), (std::streamsize)sortie.size());
    return (bool)f;
}

bool CapturerFramebuffer(const std::filesystem::path& chemin, int w, int h) {
    if (w <= 0 || h <= 0) return false;
    std::vector<std::uint8_t> px((size_t)w * h * 4), retourne((size_t)w * h * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    for (int y = 0; y < h; ++y) std::memcpy(&retourne[(size_t)y * w * 4], &px[(size_t)(h - 1 - y) * w * 4], (size_t)w * 4);
    return EcrirePng(chemin, retourne.data(), w, h);
}

} // namespace app
