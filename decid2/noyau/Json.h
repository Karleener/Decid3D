// JSON minimal : valeur, ecriture deterministe (cles triees), lecture stricte.
// Les nombres non finis sont interdits a l'ecriture (une metrique indefinie s'ecrit null).
#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "Erreur.h"

namespace decid {

class ValeurJson {
public:
    enum class Type { Nul, Booleen, Nombre, Entier, Texte, Tableau, Objet };

    ValeurJson() = default;                       // null
    ValeurJson(bool b);
    ValeurJson(int v);
    ValeurJson(long v);
    ValeurJson(long long v);
    ValeurJson(unsigned long long v);
    ValeurJson(unsigned v);
    ValeurJson(unsigned long v);
    ValeurJson(double v);                          // non fini -> null
    ValeurJson(const char* s);
    ValeurJson(const std::string& s);
    static ValeurJson Tableau();
    static ValeurJson Objet();

    Type GetType() const { return m_type; }
    bool EstNul() const { return m_type == Type::Nul; }

    // Acces (renvoient une valeur par defaut si le type ne correspond pas)
    bool Booleen() const;
    double Nombre() const;                         // Nombre ou Entier
    long long Entier() const;                      // Entier, ou Nombre entier exact
    const std::string& Texte() const;
    const std::vector<ValeurJson>& Elements() const;
    const std::map<std::string, ValeurJson>& Membres() const;

    // Construction
    void Ajouter(ValeurJson v);                                   // tableau
    ValeurJson& operator[](const std::string& cle);               // objet (cree si absent)
    bool Contient(const std::string& cle) const;
    const ValeurJson& Membre(const std::string& cle) const;       // null statique si absent

    std::string Ecrire(bool indente = true) const;                // cles triees, UTF-8, LF

private:
    void EcrireDans(std::string& sortie, bool indente, int niveau) const;
    Type m_type = Type::Nul;
    bool m_booleen = false;
    double m_nombre = 0.0;
    long long m_entier = 0;
    std::string m_texte;
    std::vector<ValeurJson> m_elements;
    std::map<std::string, ValeurJson> m_membres;
};

// Lecture stricte (RFC 8259) ; erreur avec ligne/colonne.
Resultat<ValeurJson> LireJson(const std::string& texte);

} // namespace decid
