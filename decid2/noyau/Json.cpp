#include "Json.h"

#include <charconv>
#include <cmath>
#include <limits>

namespace decid {

// ---------------------------------------------------------------- construction

ValeurJson::ValeurJson(bool b) : m_type(Type::Booleen), m_booleen(b) {}
ValeurJson::ValeurJson(int v) : m_type(Type::Entier), m_entier(v) {}
ValeurJson::ValeurJson(long v) : m_type(Type::Entier), m_entier(v) {}
ValeurJson::ValeurJson(long long v) : m_type(Type::Entier), m_entier(v) {}
ValeurJson::ValeurJson(unsigned v) : m_type(Type::Entier), m_entier(static_cast<long long>(v)) {}
ValeurJson::ValeurJson(unsigned long v) : ValeurJson(static_cast<unsigned long long>(v)) {}

ValeurJson::ValeurJson(unsigned long long v) {
    if (v <= static_cast<unsigned long long>(std::numeric_limits<long long>::max())) {
        m_type = Type::Entier;
        m_entier = static_cast<long long>(v);
    } else {
        m_type = Type::Nombre;
        m_nombre = static_cast<double>(v);
    }
}

ValeurJson::ValeurJson(double v) {
    if (std::isfinite(v)) {
        m_type = Type::Nombre;
        m_nombre = v;
    }
}

ValeurJson::ValeurJson(const char* s) : m_type(Type::Texte), m_texte(s ? s : "") {}
ValeurJson::ValeurJson(const std::string& s) : m_type(Type::Texte), m_texte(s) {}

ValeurJson ValeurJson::Tableau() {
    ValeurJson v;
    v.m_type = Type::Tableau;
    return v;
}

ValeurJson ValeurJson::Objet() {
    ValeurJson v;
    v.m_type = Type::Objet;
    return v;
}

// ---------------------------------------------------------------- acces

bool ValeurJson::Booleen() const { return m_type == Type::Booleen && m_booleen; }

double ValeurJson::Nombre() const {
    if (m_type == Type::Nombre) return m_nombre;
    if (m_type == Type::Entier) return static_cast<double>(m_entier);
    return 0.0;
}

long long ValeurJson::Entier() const {
    if (m_type == Type::Entier) return m_entier;
    if (m_type == Type::Nombre && std::floor(m_nombre) == m_nombre && std::fabs(m_nombre) < 9.0e18) {
        return static_cast<long long>(m_nombre);
    }
    return 0;
}

const std::string& ValeurJson::Texte() const {
    static const std::string vide;
    return m_type == Type::Texte ? m_texte : vide;
}

const std::vector<ValeurJson>& ValeurJson::Elements() const {
    static const std::vector<ValeurJson> vide;
    return m_type == Type::Tableau ? m_elements : vide;
}

const std::map<std::string, ValeurJson>& ValeurJson::Membres() const {
    static const std::map<std::string, ValeurJson> vide;
    return m_type == Type::Objet ? m_membres : vide;
}

void ValeurJson::Ajouter(ValeurJson v) {
    if (m_type != Type::Tableau) {
        *this = Tableau();
    }
    m_elements.push_back(std::move(v));
}

ValeurJson& ValeurJson::operator[](const std::string& cle) {
    if (m_type != Type::Objet) {
        *this = Objet();
    }
    return m_membres[cle];
}

bool ValeurJson::Contient(const std::string& cle) const {
    return m_type == Type::Objet && m_membres.count(cle) > 0;
}

const ValeurJson& ValeurJson::Membre(const std::string& cle) const {
    static const ValeurJson nul;
    if (m_type != Type::Objet) return nul;
    const auto it = m_membres.find(cle);
    return it == m_membres.end() ? nul : it->second;
}

// ---------------------------------------------------------------- ecriture

namespace {

void EcrireTexte(std::string& sortie, const std::string& texte) {
    static const char hex[] = "0123456789abcdef";
    sortie.push_back('"');
    for (const char c : texte) {
        const unsigned char u = static_cast<unsigned char>(c);
        switch (c) {
        case '"': sortie += "\\\""; break;
        case '\\': sortie += "\\\\"; break;
        case '\n': sortie += "\\n"; break;
        case '\r': sortie += "\\r"; break;
        case '\t': sortie += "\\t"; break;
        case '\b': sortie += "\\b"; break;
        case '\f': sortie += "\\f"; break;
        default:
            if (u < 0x20) {
                sortie += "\\u00";
                sortie.push_back(hex[u >> 4]);
                sortie.push_back(hex[u & 0xF]);
            } else {
                sortie.push_back(c);
            }
        }
    }
    sortie.push_back('"');
}

void EcrireDouble(std::string& sortie, double v) {
    char tampon[64];
    const auto r = std::to_chars(tampon, tampon + sizeof(tampon), v);
    std::string s(tampon, r.ptr);
    if (s.find_first_of(".eEn") == std::string::npos) s += ".0";
    sortie += s;
}

void Indenter(std::string& sortie, int niveau) { sortie.append(static_cast<std::size_t>(niveau) * 2, ' '); }

} // namespace

void ValeurJson::EcrireDans(std::string& sortie, bool indente, int niveau) const {
    switch (m_type) {
    case Type::Nul: sortie += "null"; break;
    case Type::Booleen: sortie += m_booleen ? "true" : "false"; break;
    case Type::Entier: sortie += std::to_string(m_entier); break;
    case Type::Nombre: EcrireDouble(sortie, m_nombre); break;
    case Type::Texte: EcrireTexte(sortie, m_texte); break;
    case Type::Tableau: {
        if (m_elements.empty()) { sortie += "[]"; break; }
        sortie.push_back('[');
        bool premier = true;
        for (const auto& e : m_elements) {
            if (!premier) sortie.push_back(',');
            premier = false;
            if (indente) { sortie.push_back('\n'); Indenter(sortie, niveau + 1); }
            e.EcrireDans(sortie, indente, niveau + 1);
        }
        if (indente) { sortie.push_back('\n'); Indenter(sortie, niveau); }
        sortie.push_back(']');
        break;
    }
    case Type::Objet: {
        if (m_membres.empty()) { sortie += "{}"; break; }
        sortie.push_back('{');
        bool premier = true;
        for (const auto& kv : m_membres) {
            if (!premier) sortie.push_back(',');
            premier = false;
            if (indente) { sortie.push_back('\n'); Indenter(sortie, niveau + 1); }
            EcrireTexte(sortie, kv.first);
            sortie += indente ? ": " : ":";
            kv.second.EcrireDans(sortie, indente, niveau + 1);
        }
        if (indente) { sortie.push_back('\n'); Indenter(sortie, niveau); }
        sortie.push_back('}');
        break;
    }
    }
}

std::string ValeurJson::Ecrire(bool indente) const {
    std::string sortie;
    EcrireDans(sortie, indente, 0);
    return sortie;
}

// ---------------------------------------------------------------- lecture

namespace {

class Lecteur {
public:
    explicit Lecteur(const std::string& texte) : m_texte(texte) {}

    Resultat<ValeurJson> Lire() {
        SauterEspaces();
        ValeurJson v;
        if (!LireValeur(v, 0)) return m_erreur;
        SauterEspaces();
        if (m_pos < m_texte.size()) {
            Echec("texte inattendu apres la valeur JSON");
            return m_erreur;
        }
        return v;
    }

private:
    static constexpr int kProfondeurMax = 512;

    bool Echec(const std::string& message) { return EchecA(m_pos, message); }

    bool EchecA(std::size_t pos, const std::string& message) {
        long ligne = 1, colonne = 1;
        for (std::size_t i = 0; i < pos && i < m_texte.size(); ++i) {
            if (m_texte[i] == '\n') { ++ligne; colonne = 1; } else { ++colonne; }
        }
        m_erreur = FaireErreur(CodeErreur::DonneesInvalides, "json", "JSON invalide : " + message, "", ligne, colonne);
        return false;
    }

    bool Fin() const { return m_pos >= m_texte.size(); }
    char Courant() const { return m_texte[m_pos]; }

    void SauterEspaces() {
        while (!Fin() && (Courant() == ' ' || Courant() == '\t' || Courant() == '\n' || Courant() == '\r')) ++m_pos;
    }

    bool LireMot(const char* mot, ValeurJson valeur, ValeurJson& sortie) {
        const std::string m(mot);
        if (m_texte.compare(m_pos, m.size(), m) != 0) return Echec("valeur inconnue");
        m_pos += m.size();
        sortie = std::move(valeur);
        return true;
    }

    bool LireValeur(ValeurJson& sortie, int profondeur) {
        if (profondeur > kProfondeurMax) return Echec("imbrication trop profonde");
        if (Fin()) return Echec("valeur attendue, fin du texte");
        const char c = Courant();
        if (c == '{') return LireObjet(sortie, profondeur);
        if (c == '[') return LireTableau(sortie, profondeur);
        if (c == '"') {
            std::string s;
            if (!LireChaine(s)) return false;
            sortie = ValeurJson(s);
            return true;
        }
        if (c == 't') return LireMot("true", ValeurJson(true), sortie);
        if (c == 'f') return LireMot("false", ValeurJson(false), sortie);
        if (c == 'n') return LireMot("null", ValeurJson(), sortie);
        if (c == '-' || (c >= '0' && c <= '9')) return LireNombre(sortie);
        return Echec("caractere inattendu");
    }

    bool LireObjet(ValeurJson& sortie, int profondeur) {
        ++m_pos;
        ValeurJson obj = ValeurJson::Objet();
        SauterEspaces();
        if (!Fin() && Courant() == '}') { ++m_pos; sortie = std::move(obj); return true; }
        for (;;) {
            SauterEspaces();
            if (Fin() || Courant() != '"') return Echec("cle de type texte attendue");
            std::string cle;
            if (!LireChaine(cle)) return false;
            SauterEspaces();
            if (Fin() || Courant() != ':') return Echec("':' attendu apres la cle");
            ++m_pos;
            SauterEspaces();
            ValeurJson v;
            if (!LireValeur(v, profondeur + 1)) return false;
            obj[cle] = std::move(v);
            SauterEspaces();
            if (Fin()) return Echec("objet non termine");
            if (Courant() == ',') { ++m_pos; continue; }
            if (Courant() == '}') { ++m_pos; break; }
            return Echec("',' ou '}' attendu");
        }
        sortie = std::move(obj);
        return true;
    }

    bool LireTableau(ValeurJson& sortie, int profondeur) {
        ++m_pos;
        ValeurJson tab = ValeurJson::Tableau();
        SauterEspaces();
        if (!Fin() && Courant() == ']') { ++m_pos; sortie = std::move(tab); return true; }
        for (;;) {
            SauterEspaces();
            ValeurJson v;
            if (!LireValeur(v, profondeur + 1)) return false;
            tab.Ajouter(std::move(v));
            SauterEspaces();
            if (Fin()) return Echec("tableau non termine");
            if (Courant() == ',') { ++m_pos; continue; }
            if (Courant() == ']') { ++m_pos; break; }
            return Echec("',' ou ']' attendu");
        }
        sortie = std::move(tab);
        return true;
    }

    bool LireHex4(unsigned& valeur) {
        if (m_pos + 4 > m_texte.size()) return Echec("sequence \\u incomplete");
        valeur = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = m_texte[m_pos + static_cast<std::size_t>(i)];
            unsigned d;
            if (c >= '0' && c <= '9') d = static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') d = static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') d = static_cast<unsigned>(c - 'A' + 10);
            else return Echec("chiffre hexadecimal attendu");
            valeur = valeur * 16 + d;
        }
        m_pos += 4;
        return true;
    }

    static void AjouterUtf8(std::string& s, unsigned cp) {
        if (cp < 0x80) {
            s.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            s.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            s.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            s.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            s.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            s.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            s.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            s.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            s.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            s.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    bool LireEchappement(std::string& sortie) {
        if (Fin()) return Echec("chaine non terminee");
        const char c = Courant();
        ++m_pos;
        switch (c) {
        case '"': sortie.push_back('"'); return true;
        case '\\': sortie.push_back('\\'); return true;
        case '/': sortie.push_back('/'); return true;
        case 'b': sortie.push_back('\b'); return true;
        case 'f': sortie.push_back('\f'); return true;
        case 'n': sortie.push_back('\n'); return true;
        case 'r': sortie.push_back('\r'); return true;
        case 't': sortie.push_back('\t'); return true;
        case 'u': break;
        default: return EchecA(m_pos - 1, "echappement inconnu");
        }
        unsigned cp = 0;
        if (!LireHex4(cp)) return false;
        if (cp >= 0xDC00 && cp <= 0xDFFF) return Echec("substitut bas isole");
        if (cp >= 0xD800 && cp <= 0xDBFF) {
            if (m_pos + 2 > m_texte.size() || m_texte[m_pos] != '\\' || m_texte[m_pos + 1] != 'u') {
                return Echec("substitut haut sans suite");
            }
            m_pos += 2;
            unsigned bas = 0;
            if (!LireHex4(bas)) return false;
            if (bas < 0xDC00 || bas > 0xDFFF) return Echec("substitut bas attendu");
            cp = 0x10000 + ((cp - 0xD800) << 10) + (bas - 0xDC00);
        }
        AjouterUtf8(sortie, cp);
        return true;
    }

    bool LireChaine(std::string& sortie) {
        const std::size_t debut = m_pos;
        ++m_pos;
        sortie.clear();
        for (;;) {
            if (Fin()) return EchecA(debut, "chaine non terminee");
            const char c = Courant();
            if (c == '"') { ++m_pos; return true; }
            if (static_cast<unsigned char>(c) < 0x20) return Echec("caractere de controle dans une chaine");
            if (c == '\\') {
                ++m_pos;
                if (!LireEchappement(sortie)) return false;
                continue;
            }
            sortie.push_back(c);
            ++m_pos;
        }
    }

    bool Chiffres() {
        const std::size_t debut = m_pos;
        while (!Fin() && Courant() >= '0' && Courant() <= '9') ++m_pos;
        return m_pos > debut;
    }

    bool LireNombre(ValeurJson& sortie) {
        const std::size_t debut = m_pos;
        bool flottant = false;
        if (Courant() == '-') ++m_pos;
        if (Fin()) return Echec("nombre incomplet");
        if (Courant() == '0') {
            ++m_pos;
        } else if (!Chiffres()) {
            return Echec("chiffre attendu");
        }
        if (!Fin() && Courant() == '.') {
            flottant = true;
            ++m_pos;
            if (!Chiffres()) return Echec("chiffre attendu apres '.'");
        }
        if (!Fin() && (Courant() == 'e' || Courant() == 'E')) {
            flottant = true;
            ++m_pos;
            if (!Fin() && (Courant() == '+' || Courant() == '-')) ++m_pos;
            if (!Chiffres()) return Echec("chiffre attendu dans l'exposant");
        }
        const char* b = m_texte.data() + debut;
        const char* e = m_texte.data() + m_pos;
        if (!flottant) {
            long long n = 0;
            const auto r = std::from_chars(b, e, n);
            if (r.ec == std::errc() && r.ptr == e) { sortie = ValeurJson(n); return true; }
        }
        double d = 0.0;
        const auto r = std::from_chars(b, e, d);
        if (r.ec != std::errc() || r.ptr != e || !std::isfinite(d)) return EchecA(debut, "nombre hors limites");
        sortie = ValeurJson(d);
        return true;
    }

    const std::string& m_texte;
    std::size_t m_pos = 0;
    Erreur m_erreur;
};

} // namespace

Resultat<ValeurJson> LireJson(const std::string& texte) {
    // BOM UTF-8 initial ignore (RFC 8259 §8.1 : permis a la lecture ; Bloc-notes et PowerShell 5 l'ecrivent)
    if (texte.size() >= 3 && (unsigned char)texte[0] == 0xEF && (unsigned char)texte[1] == 0xBB &&
        (unsigned char)texte[2] == 0xBF) {
        const std::string sansBom = texte.substr(3);   // vit jusqu'a la fin de la lecture (Lecteur garde une reference)
        Lecteur lecteur(sansBom);
        return lecteur.Lire();
    }
    Lecteur lecteur(texte);
    return lecteur.Lire();
}

} // namespace decid
