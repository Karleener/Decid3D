// Lecture des donnees : CSV a contrat explicite, CMC/DAT historiques, fichiers par role.
// Regle : aucune ligne n'est jamais supprimee en silence, aucune ecriture pres des sources.
#include "Donnees.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <unordered_map>
#include "Fichiers.h"
#include "Json.h"
#include "Sha256.h"

namespace decid {

const char* NomRole(RoleFichier role) {
    switch (role) {
    case RoleFichier::Apprentissage: return "apprentissage";
    case RoleFichier::Test: return "test";
    default: return "aucun";
    }
}

namespace {

// ---------- outils texte ----------

std::string Nettoyer(const std::string& s) {
    size_t debut = 0, fin = s.size();
    while (debut < fin && (s[debut] == ' ' || s[debut] == '\t')) ++debut;
    while (fin > debut && (s[fin - 1] == ' ' || s[fin - 1] == '\t' || s[fin - 1] == '\r')) --fin;
    return s.substr(debut, fin - debut);
}

// Decoupe en lignes physiques (\n), retire \r final ; retire le BOM UTF-8 eventuel.
std::vector<std::string> DecouperLignes(const std::string& texte) {
    std::vector<std::string> lignes;
    size_t debut = 0;
    if (texte.size() >= 3 && (unsigned char)texte[0] == 0xEF && (unsigned char)texte[1] == 0xBB &&
        (unsigned char)texte[2] == 0xBF)
        debut = 3;
    while (debut <= texte.size()) {
        size_t fin = texte.find('\n', debut);
        if (fin == std::string::npos) fin = texte.size();
        std::string ligne = texte.substr(debut, fin - debut);
        if (!ligne.empty() && ligne.back() == '\r') ligne.pop_back();
        lignes.push_back(ligne);
        if (fin == texte.size()) break;
        debut = fin + 1;
    }
    // la derniere "ligne" apres un \n final est vide : on la retire (ce n'est pas une ligne du fichier)
    if (!lignes.empty() && lignes.back().empty() && !texte.empty() && texte.back() == '\n') lignes.pop_back();
    return lignes;
}

bool LigneVide(const std::string& ligne) {
    return Nettoyer(ligne).empty();
}

// Champs d'une ligne CSV, guillemets "..." avec "" pour un guillemet.
bool DecouperChamps(const std::string& ligne, char separateur, std::vector<std::string>& champs) {
    champs.clear();
    std::string courant;
    bool entreGuillemets = false;
    for (size_t i = 0; i < ligne.size(); ++i) {
        char c = ligne[i];
        if (entreGuillemets) {
            if (c == '"') {
                if (i + 1 < ligne.size() && ligne[i + 1] == '"') { courant += '"'; ++i; }
                else entreGuillemets = false;
            } else courant += c;
        } else if (c == '"') {
            entreGuillemets = true;
        } else if (c == separateur) {
            champs.push_back(Nettoyer(courant));
            courant.clear();
        } else courant += c;
    }
    if (entreGuillemets) return false;
    champs.push_back(Nettoyer(courant));
    return true;
}

// Nombre decimal (point decimal, locale independante). Faux si non numerique ou non fini.
bool LireNombre(const std::string& texte, double& valeur) {
    if (texte.empty()) return false;
    const char* debut = texte.data();
    const char* fin = debut + texte.size();
    if (*debut == '+') ++debut;       // from_chars refuse le '+'
    auto r = std::from_chars(debut, fin, valeur);
    return r.ec == std::errc() && r.ptr == fin && std::isfinite(valeur);
}

std::string TexteDouble(double v) {
    char tampon[64];
    auto r = std::to_chars(tampon, tampon + sizeof(tampon), v);   // representation la plus courte exacte
    return std::string(tampon, r.ptr);
}

// ---------- labels ----------

// Construit le mapping label externe -> identifiant interne.
// Labels numeriques : entiers obligatoires (fractionnaires, NaN, Inf refuses), tri numerique croissant.
// Labels textuels : tri par octets UTF-8. Melange numerique/textuel : refuse.
struct LabelBrut { std::string m_texte; long m_ligne; int m_source; };

// Cible numerique (regression) : chaque valeur doit etre un nombre fini ; texte brut garde, tri par octets.
Statut ControlerCiblesNumeriques(const std::vector<LabelBrut>& bruts, const std::vector<SourceFichier>& sources) {
    for (auto& b : bruts) {
        double v;
        if (!LireNombre(b.m_texte, v))
            return FaireErreur(CodeErreur::DonneesInvalides, "import", "cible non numerique : \"" + b.m_texte + "\"",
                               sources[b.m_source].m_cheminResolu, b.m_ligne);
    }
    return Succes();
}

Resultat<std::vector<int>> CartographierLabels(const std::vector<LabelBrut>& bruts,
                                                const std::vector<SourceFichier>& sources,
                                                std::vector<std::string>& nomsClasses, bool cibleNumerique = false) {
    bool tousNumeriques = !cibleNumerique, aucunNumerique = true;
    std::vector<long long> entiers(bruts.size());
    for (size_t i = 0; i < bruts.size(); ++i) {
        double v = 0;
        const std::string& t = bruts[i].m_texte;
        bool estNombre = !cibleNumerique && LireNombre(t, v);
        bool resembleNombre = !t.empty() && (std::isdigit((unsigned char)t[0]) || t[0] == '-' || t[0] == '+' || t[0] == '.');
        if (estNombre) {
            aucunNumerique = false;
            if (std::floor(v) != v || std::fabs(v) > 9007199254740992.0)
                return FaireErreur(CodeErreur::DonneesInvalides, "import",
                    "label numerique non entier refuse : \"" + t + "\"",
                    sources[bruts[i].m_source].m_cheminResolu, bruts[i].m_ligne);
            entiers[i] = (long long)v;
        } else {
            tousNumeriques = false;
            std::string minuscule = t;
            std::transform(minuscule.begin(), minuscule.end(), minuscule.begin(), [](unsigned char c) { return (char)std::tolower(c); });
            if (!cibleNumerique && (t.empty() || resembleNombre || minuscule == "nan" || minuscule == "inf" || minuscule == "-inf"))
                return FaireErreur(CodeErreur::DonneesInvalides, "import",
                    t.empty() ? "label manquant" : "label invalide : \"" + t + "\"",
                    sources[bruts[i].m_source].m_cheminResolu, bruts[i].m_ligne);
        }
    }
    if (!tousNumeriques && !aucunNumerique)
        return FaireErreur(CodeErreur::DonneesInvalides, "import",
            "labels melangeant nombres et textes : mapping ambigu refuse");

    std::vector<int> ids(bruts.size());
    nomsClasses.clear();
    if (tousNumeriques) {
        std::set<long long> distincts(entiers.begin(), entiers.end());
        std::map<long long, int> index;
        for (long long e : distincts) { index[e] = (int)nomsClasses.size(); nomsClasses.push_back(std::to_string(e)); }
        for (size_t i = 0; i < bruts.size(); ++i) ids[i] = index[entiers[i]];
    } else {
        std::set<std::string> distincts;
        for (auto& b : bruts) distincts.insert(b.m_texte);
        std::map<std::string, int> index;
        for (auto& n : distincts) { index[n] = (int)nomsClasses.size(); nomsClasses.push_back(n); }
        for (size_t i = 0; i < bruts.size(); ++i) ids[i] = index[bruts[i].m_texte];
    }
    return ids;
}

Resultat<SourceFichier> DecrireSource(const fs::path& chemin, const std::string& declare) {
    SourceFichier s;
    s.m_cheminDeclare = declare;
    std::error_code ec;
    fs::path absolu = fs::absolute(chemin, ec);
    s.m_cheminResolu = CheminUtf8(ec ? chemin : absolu);
    s.m_sha256 = Sha256Fichier(chemin, &s.m_taille);
    if (s.m_sha256.empty())
        return FaireErreur(CodeErreur::Lecture, "import", "fichier illisible ou absent", s.m_cheminResolu);
    return s;
}

void AjouterAvertissementDoublons(JeuDeDonnees& jeu) {
    // Lignes numeriquement egales entre roles differents : signalees, jamais supprimees
    std::unordered_map<std::string, int> vues;
    long egalites = 0;
    for (int i = 0; i < jeu.m_nbLignes; ++i) {
        std::string cle((const char*)&jeu.m_valeurs[(size_t)i * jeu.m_nbAttributs],
                        sizeof(double) * jeu.m_nbAttributs);
        cle.append((const char*)&jeu.m_labels[i], sizeof(int));
        auto it = vues.find(cle);
        if (it == vues.end()) vues[cle] = i;
        else if (jeu.m_sources[jeu.m_origines[it->second].m_source].m_role !=
                 jeu.m_sources[jeu.m_origines[i].m_source].m_role) ++egalites;
    }
    if (egalites > 0)
        jeu.m_avertissements.push_back(std::to_string(egalites) +
            " ligne(s) numeriquement egale(s) entre apprentissage et test (signalees, non supprimees)");
}

// Lit un DAT historique : nbAttributs, nbEchantillons, puis valeurs separees par des blancs.
Statut LireDat(const fs::path& chemin, int indiceSource, const std::string& label,
               JeuDeDonnees& jeu, std::vector<LabelBrut>& labels) {
    auto texte = LireFichierTexte(chemin);
    if (!texte.Ok()) return texte.Err();
    const std::string& t = texte.Valeur();
    const std::string nom = CheminUtf8(chemin);

    size_t pos = 0;
    long ligne = 1;
    auto jetonSuivant = [&](std::string& jeton, long& ligneJeton) -> bool {
        while (pos < t.size() && std::isspace((unsigned char)t[pos])) { if (t[pos] == '\n') ++ligne; ++pos; }
        if (pos >= t.size()) return false;
        size_t debut = pos;
        while (pos < t.size() && !std::isspace((unsigned char)t[pos])) ++pos;
        jeton = t.substr(debut, pos - debut);
        ligneJeton = ligne;
        return true;
    };

    std::string jeton;
    long ligneJeton = 0;
    double v;
    if (!jetonSuivant(jeton, ligneJeton) || !LireNombre(jeton, v) || v < 1 || std::floor(v) != v)
        return FaireErreur(CodeErreur::DonneesInvalides, "import", "en-tete DAT invalide (nombre d'attributs)", nom, ligneJeton);
    int nbAttributs = (int)v;
    if (!jetonSuivant(jeton, ligneJeton) || !LireNombre(jeton, v) || v < 0 || std::floor(v) != v)
        return FaireErreur(CodeErreur::DonneesInvalides, "import", "en-tete DAT invalide (nombre d'echantillons)", nom, ligneJeton);
    long nbEchantillons = (long)v;

    if (jeu.m_nbAttributs == 0) jeu.m_nbAttributs = nbAttributs;
    else if (jeu.m_nbAttributs != nbAttributs)
        return FaireErreur(CodeErreur::DonneesInvalides, "import",
            std::to_string(jeu.m_nbAttributs) + " attributs attendus, " + std::to_string(nbAttributs) + " declares", nom, 1);

    for (long e = 0; e < nbEchantillons; ++e) {
        long ligneEchantillon = 0;
        for (int a = 0; a < nbAttributs; ++a) {
            if (!jetonSuivant(jeton, ligneJeton))
                return FaireErreur(CodeErreur::DonneesInvalides, "import",
                    std::to_string(nbEchantillons) + " echantillons annonces, fichier termine a l'echantillon " +
                    std::to_string(e + 1), nom, ligne);
            if (!LireNombre(jeton, v))
                return FaireErreur(CodeErreur::DonneesInvalides, "import", "valeur non numerique ou non finie : \"" + jeton + "\"",
                                   nom, ligneJeton);
            if (a == 0) ligneEchantillon = ligneJeton;
            jeu.m_valeurs.push_back(v);
        }
        jeu.m_origines.push_back({indiceSource, ligneEchantillon});
        labels.push_back({label, ligneEchantillon, indiceSource});
        ++jeu.m_nbLignes;
    }
    if (jetonSuivant(jeton, ligneJeton))
        return FaireErreur(CodeErreur::DonneesInvalides, "import",
            "valeurs en trop apres les " + std::to_string(nbEchantillons) + " echantillons annonces", nom, ligneJeton);
    return Succes();
}

Statut FinaliserLabels(JeuDeDonnees& jeu, const std::vector<LabelBrut>& bruts, bool cibleNumerique = false) {
    if (cibleNumerique) {
        Statut st = ControlerCiblesNumeriques(bruts, jeu.m_sources);
        if (!st.Ok()) return st;
    }
    auto ids = CartographierLabels(bruts, jeu.m_sources, jeu.m_nomsClasses, cibleNumerique);
    if (!ids.Ok()) return ids.Err();
    jeu.m_labels = ids.Valeur();
    if (jeu.m_nomsAttributs.empty())
        for (int a = 0; a < jeu.m_nbAttributs; ++a) jeu.m_nomsAttributs.push_back("x" + std::to_string(a + 1));
    if (jeu.NbClasses() < 2)
        jeu.m_avertissements.push_back("une seule classe : apprentissage supervise impossible");
    return Succes();
}

} // namespace

// ---------- JeuDeDonnees ----------

std::vector<long> JeuDeDonnees::ComptesParClasse() const {
    std::vector<long> comptes(m_nomsClasses.size(), 0);
    for (int l : m_labels) if (l >= 0 && l < (int)comptes.size()) ++comptes[l];
    return comptes;
}

// canon_v1 : descripteur texte a cles triees + float64 LE (zero signe normalise) + int32 LE des labels.
// Exclut chemins et horodatage ; inclut ordre des lignes, noms et mapping.
std::string JeuDeDonnees::HashCanonique() const {
    ValeurJson d = ValeurJson::Objet();
    d["canon"] = "decid.canon_v1";
    d["tache"] = "classification";
    d["n"] = m_nbLignes;
    d["p"] = m_nbAttributs;
    ValeurJson attributs = ValeurJson::Tableau();
    for (auto& n : m_nomsAttributs) attributs.Ajouter(n);
    d["attributs"] = attributs;
    ValeurJson classes = ValeurJson::Tableau();
    for (auto& n : m_nomsClasses) classes.Ajouter(n);
    d["classes"] = classes;
    Sha256 h;
    h.Ajouter(d.Ecrire(false));
    h.Ajouter("\n", 1);
    for (double v : m_valeurs) {
        if (v == 0.0) v = 0.0;                    // -0 -> +0
        std::uint8_t octets[8];
        std::uint64_t bits;
        std::memcpy(&bits, &v, 8);
        for (int i = 0; i < 8; ++i) octets[i] = (std::uint8_t)(bits >> (8 * i));
        h.Ajouter(octets, 8);
    }
    for (int l : m_labels) {
        std::uint8_t octets[4];
        for (int i = 0; i < 4; ++i) octets[i] = (std::uint8_t)((std::uint32_t)l >> (8 * i));
        h.Ajouter(octets, 4);
    }
    return h.FinirHex();
}

// ---------- CSV ----------

Resultat<JeuDeDonnees> LireCsv(const fs::path& chemin, const ContratCsv& contrat) {
    const std::string nom = CheminUtf8(chemin);
    auto source = DecrireSource(chemin, nom);
    if (!source.Ok()) return source.Err();
    auto texte = LireFichierTexte(chemin);
    if (!texte.Ok()) return texte.Err();

    JeuDeDonnees jeu;
    jeu.m_sources.push_back(source.Valeur());
    std::vector<std::string> lignes = DecouperLignes(texte.Valeur());
    std::vector<LabelBrut> labels;
    std::vector<std::string> champs;
    long lignesVides = 0;
    int nbColonnes = -1;
    bool enTeteATraiter = contrat.m_enTete != ModeEnTete::Non;
    bool enTeteLu = false;
    const int cible = contrat.m_sansCible ? -1 : contrat.m_colonneCible;

    for (size_t i = 0; i < lignes.size(); ++i) {
        long numero = (long)i + 1;
        if (LigneVide(lignes[i])) { ++lignesVides; continue; }
        if (!DecouperChamps(lignes[i], contrat.m_separateur, champs))
            return FaireErreur(CodeErreur::DonneesInvalides, "import", "guillemet non ferme", nom, numero);
        if (nbColonnes < 0) {
            nbColonnes = (int)champs.size();
            if (nbColonnes < (contrat.m_sansCible ? 1 : 2))
                return FaireErreur(CodeErreur::DonneesInvalides, "import",
                    "une seule colonne trouvee : separateur '" + std::string(1, contrat.m_separateur) + "' incorrect ?", nom, numero);
            if (!contrat.m_sansCible && (cible < 0 || cible >= nbColonnes))
                return FaireErreur(CodeErreur::DonneesInvalides, "import", "colonne cible hors du fichier", nom, numero);
            jeu.m_nbAttributs = contrat.m_sansCible ? nbColonnes : nbColonnes - 1;
        }
        if ((int)champs.size() != nbColonnes)
            return FaireErreur(CodeErreur::DonneesInvalides, "import",
                std::to_string(nbColonnes) + " valeurs attendues, " + std::to_string(champs.size()) + " trouvees", nom, numero);

        if (enTeteATraiter) {
            enTeteATraiter = false;
            int numeriques = 0;
            double v;
            for (int c = 0; c < nbColonnes; ++c)
                if (c != cible && LireNombre(champs[c], v)) ++numeriques;
            bool premiereEstEnTete;
            if (contrat.m_enTete == ModeEnTete::Oui) premiereEstEnTete = true;
            else if (numeriques == jeu.m_nbAttributs) premiereEstEnTete = false;
            else if (numeriques == 0) premiereEstEnTete = true;
            else
                return FaireErreur(CodeErreur::DonneesInvalides, "import",
                    "premiere ligne ambigue (en-tete ou observation ?) : preciser l'option d'en-tete", nom, numero);
            if (premiereEstEnTete) {
                for (int c = 0; c < nbColonnes; ++c)
                    if (c != cible) jeu.m_nomsAttributs.push_back(champs[c]);
                enTeteLu = true;
                continue;
            }
        }

        for (int c = 0; c < nbColonnes; ++c) {
            if (c == cible) continue;
            double v;
            if (!LireNombre(champs[c], v)) {
                std::string message = "valeur non numerique ou non finie : \"" + champs[c] + "\"";
                if (champs[c].find(',') != std::string::npos) message += " (virgule decimale non acceptee)";
                return FaireErreur(CodeErreur::DonneesInvalides, "import", message, nom, numero, c + 1);
            }
            jeu.m_valeurs.push_back(v);
        }
        labels.push_back({cible < 0 ? std::string("?") : champs[(size_t)cible], numero, 0});
        jeu.m_origines.push_back({0, numero});
        ++jeu.m_nbLignes;
    }
    if (jeu.m_nbLignes == 0)
        return FaireErreur(CodeErreur::DonneesInvalides, "import", "aucune observation lue", nom);

    auto fin = FinaliserLabels(jeu, labels, contrat.m_cibleNumerique);
    if (!fin.Ok()) return fin.Err();
    jeu.m_contrat = "csv separateur='" + std::string(1, contrat.m_separateur) + "' en_tete=" +
                    (enTeteLu ? "oui" : "non") + " (demande: " +
                    (contrat.m_enTete == ModeEnTete::Oui ? "oui" : contrat.m_enTete == ModeEnTete::Non ? "non" : "auto") +
                    ") colonne_cible=" + (cible < 0 ? std::string("aucune") : std::to_string(cible)) +
                    " decimal='.' encodage=utf-8" +
                    (contrat.m_cibleNumerique ? " cible=numerique" : "");
    if (lignesVides > 0)
        jeu.m_avertissements.push_back(std::to_string(lignesVides) + " ligne(s) vide(s) ignoree(s)");
    return jeu;
}

// ---------- CMC ----------

Resultat<JeuDeDonnees> LireCmc(const fs::path& chemin, const std::vector<Relocalisation>& relocalisations) {
    const std::string nom = CheminUtf8(chemin);
    auto texte = LireFichierTexte(chemin);
    if (!texte.Ok()) return texte.Err();
    std::vector<std::string> lignes = DecouperLignes(texte.Valeur());
    size_t i = 0;
    auto ligneSuivante = [&](std::string& sortie) -> bool {
        while (i < lignes.size() && LigneVide(lignes[i])) ++i;
        if (i >= lignes.size()) return false;
        sortie = Nettoyer(lignes[i++]);
        return true;
    };
    std::string l;
    double v;
    if (!ligneSuivante(l) || !LireNombre(l, v) || v < 1 || std::floor(v) != v)
        return FaireErreur(CodeErreur::DonneesInvalides, "import", "CMC : nombre de classes invalide", nom, (long)i);
    int nbClasses = (int)v;
    std::vector<int> fichiersParClasse;
    for (int c = 0; c < nbClasses; ++c) {
        if (!ligneSuivante(l) || !LireNombre(l, v) || v < 1 || std::floor(v) != v)
            return FaireErreur(CodeErreur::DonneesInvalides, "import",
                "CMC : nombre de fichiers de la classe " + std::to_string(c) + " invalide", nom, (long)i);
        fichiersParClasse.push_back((int)v);
    }

    JeuDeDonnees jeu;
    std::vector<LabelBrut> labels;
    fs::path dossierCmc = chemin.parent_path();
    for (int c = 0; c < nbClasses; ++c) {
        for (int f = 0; f < fichiersParClasse[c]; ++f) {
            if (!ligneSuivante(l))
                return FaireErreur(CodeErreur::DonneesInvalides, "import", "CMC : chemin de fichier manquant", nom, (long)i);
            std::string declare = l;
            std::string resolu = declare;
            for (auto& r : relocalisations)
                if (resolu.rfind(r.m_prefixeAncien, 0) == 0) { resolu = r.m_dossierNouveau + resolu.substr(r.m_prefixeAncien.size()); break; }
            fs::path p = CheminDepuisUtf8(resolu);
            if (p.is_relative()) p = dossierCmc / p;
            auto source = DecrireSource(p, declare);
            if (!source.Ok()) return source.Err();      // un fichier manquant ne donne jamais un jeu partiel
            jeu.m_sources.push_back(source.Valeur());
            auto st = LireDat(p, (int)jeu.m_sources.size() - 1, std::to_string(c), jeu, labels);
            if (!st.Ok()) return st.Err();
        }
    }
    if (ligneSuivante(l))
        return FaireErreur(CodeErreur::DonneesInvalides, "import", "CMC : lignes en trop apres les chemins declares", nom, (long)i);
    auto fin = FinaliserLabels(jeu, labels);
    if (!fin.Ok()) return fin.Err();
    jeu.m_contrat = "cmc " + std::to_string(nbClasses) + " classes, label = rang de la classe dans le CMC";
    return jeu;
}

// ---------- fichiers par role ----------

Resultat<JeuDeDonnees> LireFichiersParRole(const std::vector<FichierRole>& fichiers) {
    if (fichiers.empty())
        return FaireErreur(CodeErreur::DonneesInvalides, "import", "aucun fichier fourni");
    JeuDeDonnees jeu;
    std::vector<LabelBrut> labels;
    std::set<std::string> dejaLus;
    for (auto& f : fichiers) {
        if (f.m_role == RoleFichier::Aucun)
            return FaireErreur(CodeErreur::DonneesInvalides, "import", "role manquant (apprentissage ou test)", CheminUtf8(f.m_chemin));
        auto source = DecrireSource(f.m_chemin, CheminUtf8(f.m_chemin));
        if (!source.Ok()) return source.Err();
        if (!dejaLus.insert(source.Valeur().m_cheminResolu).second)
            return FaireErreur(CodeErreur::DonneesInvalides, "import",
                "meme fichier source fourni deux fois (doublon d'origine refuse)", source.Valeur().m_cheminResolu);
        SourceFichier s = source.Valeur();
        s.m_role = f.m_role;
        s.m_classeDeclaree = f.m_classe;
        jeu.m_sources.push_back(s);
        auto st = LireDat(f.m_chemin, (int)jeu.m_sources.size() - 1, f.m_classe, jeu, labels);
        if (!st.Ok()) return st.Err();
    }
    auto fin = FinaliserLabels(jeu, labels);
    if (!fin.Ok()) return fin.Err();
    jeu.m_contrat = "fichiers DAT par role, label = classe declaree par fichier";
    AjouterAvertissementDoublons(jeu);
    return jeu;
}

// ---------- export CSV ----------

Statut ExporterCsv(const JeuDeDonnees& jeu, const fs::path& destination, bool avecEnTete, bool remplacer) {
    std::vector<fs::path> sources;
    for (auto& s : jeu.m_sources) sources.push_back(CheminDepuisUtf8(s.m_cheminResolu));
    auto verif = VerifierDestination(destination, sources);
    if (!verif.Ok()) return verif;
    std::string sortie;
    if (avecEnTete) {
        sortie += "classe";
        for (auto& n : jeu.m_nomsAttributs) sortie += "," + n;
        sortie += "\n";
    }
    for (int l = 0; l < jeu.m_nbLignes; ++l) {
        sortie += jeu.m_nomsClasses[jeu.m_labels[l]];
        for (int a = 0; a < jeu.m_nbAttributs; ++a) sortie += "," + TexteDouble(jeu.Valeur(l, a));
        sortie += "\n";
    }
    return EcrireFichierAtomique(destination, sortie, remplacer);
}

// ---------- sauvegarde binaire ----------

Statut SauverJeu(const JeuDeDonnees& jeu, const fs::path& dossier) {
    std::error_code ec;
    fs::create_directories(dossier, ec);
    if (ec) return FaireErreur(CodeErreur::EcritureRefusee, "sauvegarde", "dossier non creable", CheminUtf8(dossier));

    std::vector<std::int64_t> origines;
    for (auto& o : jeu.m_origines) { origines.push_back(o.m_source); origines.push_back(o.m_ligne); }
    std::vector<std::int32_t> labels(jeu.m_labels.begin(), jeu.m_labels.end());

    struct Bloc { const char* m_nom; const void* m_donnees; size_t m_taille; };
    Bloc blocs[] = {
        {"valeurs.f64", jeu.m_valeurs.data(), jeu.m_valeurs.size() * sizeof(double)},
        {"labels.i32", labels.data(), labels.size() * sizeof(std::int32_t)},
        {"origines.i64", origines.data(), origines.size() * sizeof(std::int64_t)},
    };
    ValeurJson d = ValeurJson::Objet();
    d["schema"] = "decid.dataset/1";
    d["hash_canonique"] = jeu.HashCanonique();
    d["n"] = jeu.m_nbLignes;
    d["p"] = jeu.m_nbAttributs;
    d["contrat"] = jeu.m_contrat;
    ValeurJson classes = ValeurJson::Tableau(), attributs = ValeurJson::Tableau(), avert = ValeurJson::Tableau();
    for (auto& n : jeu.m_nomsClasses) classes.Ajouter(n);
    for (auto& n : jeu.m_nomsAttributs) attributs.Ajouter(n);
    for (auto& a : jeu.m_avertissements) avert.Ajouter(a);
    d["classes"] = classes;
    d["attributs"] = attributs;
    d["avertissements"] = avert;
    ValeurJson sources = ValeurJson::Tableau();
    for (auto& s : jeu.m_sources) {
        ValeurJson j = ValeurJson::Objet();
        j["chemin_declare"] = s.m_cheminDeclare;
        j["chemin_resolu"] = s.m_cheminResolu;
        j["sha256"] = s.m_sha256;
        j["taille"] = (unsigned long long)s.m_taille;
        j["role"] = NomRole(s.m_role);
        j["classe_declaree"] = s.m_classeDeclaree;
        sources.Ajouter(j);
    }
    d["sources"] = sources;
    ValeurJson fichiers = ValeurJson::Objet();
    for (auto& b : blocs) {
        auto st = EcrireFichierAtomique(dossier / b.m_nom, b.m_donnees, b.m_taille, true);
        if (!st.Ok()) return st;
        Sha256 h;
        h.Ajouter(b.m_donnees, b.m_taille);
        ValeurJson f = ValeurJson::Objet();
        f["sha256"] = h.FinirHex();
        f["octets"] = (unsigned long long)b.m_taille;
        fichiers[b.m_nom] = f;
    }
    d["fichiers"] = fichiers;
    return EcrireFichierAtomique(dossier / "jeu.json", d.Ecrire(), true);
}

Resultat<JeuDeDonnees> ChargerJeu(const fs::path& dossier) {
    auto texte = LireFichierTexte(dossier / "jeu.json");
    if (!texte.Ok()) return texte.Err();
    auto json = LireJson(texte.Valeur());
    if (!json.Ok()) return json.Err();
    const ValeurJson& d = json.Valeur();
    if (d.Membre("schema").Texte() != "decid.dataset/1")
        return FaireErreur(CodeErreur::Protocole, "chargement", "schema de jeu inconnu", CheminUtf8(dossier));
    JeuDeDonnees jeu;
    jeu.m_nbLignes = (int)d.Membre("n").Entier();
    jeu.m_nbAttributs = (int)d.Membre("p").Entier();
    jeu.m_contrat = d.Membre("contrat").Texte();
    for (auto& v : d.Membre("classes").Elements()) jeu.m_nomsClasses.push_back(v.Texte());
    for (auto& v : d.Membre("attributs").Elements()) jeu.m_nomsAttributs.push_back(v.Texte());
    for (auto& v : d.Membre("avertissements").Elements()) jeu.m_avertissements.push_back(v.Texte());
    for (auto& v : d.Membre("sources").Elements()) {
        SourceFichier s;
        s.m_cheminDeclare = v.Membre("chemin_declare").Texte();
        s.m_cheminResolu = v.Membre("chemin_resolu").Texte();
        s.m_sha256 = v.Membre("sha256").Texte();
        s.m_taille = (std::uintmax_t)v.Membre("taille").Entier();
        std::string role = v.Membre("role").Texte();
        s.m_role = role == "apprentissage" ? RoleFichier::Apprentissage : role == "test" ? RoleFichier::Test : RoleFichier::Aucun;
        s.m_classeDeclaree = v.Membre("classe_declaree").Texte();
        jeu.m_sources.push_back(s);
    }
    auto lireBloc = [&](const char* nom, size_t attendu) -> Resultat<std::vector<char>> {
        auto b = LireFichierBinaire(dossier / nom);
        if (!b.Ok()) return b.Err();
        const ValeurJson& f = d.Membre("fichiers").Membre(nom);
        Sha256 h;
        h.Ajouter(b.Valeur().data(), b.Valeur().size());
        if (b.Valeur().size() != attendu || h.FinirHex() != f.Membre("sha256").Texte())
            return FaireErreur(CodeErreur::ModeleIncompatible, "chargement", std::string("bloc corrompu : ") + nom, CheminUtf8(dossier));
        return b;
    };
    size_t n = (size_t)jeu.m_nbLignes, p = (size_t)jeu.m_nbAttributs;
    auto valeurs = lireBloc("valeurs.f64", n * p * sizeof(double));
    if (!valeurs.Ok()) return valeurs.Err();
    auto labels = lireBloc("labels.i32", n * sizeof(std::int32_t));
    if (!labels.Ok()) return labels.Err();
    auto origines = lireBloc("origines.i64", n * 2 * sizeof(std::int64_t));
    if (!origines.Ok()) return origines.Err();
    jeu.m_valeurs.resize(n * p);
    std::memcpy(jeu.m_valeurs.data(), valeurs.Valeur().data(), n * p * sizeof(double));
    std::vector<std::int32_t> l32(n);
    std::memcpy(l32.data(), labels.Valeur().data(), n * sizeof(std::int32_t));
    std::vector<std::int64_t> o64(n * 2);
    std::memcpy(o64.data(), origines.Valeur().data(), n * 2 * sizeof(std::int64_t));
    for (size_t i = 0; i < n; ++i) {
        if (l32[i] < 0 || l32[i] >= (int)jeu.m_nomsClasses.size())
            return FaireErreur(CodeErreur::ModeleIncompatible, "chargement", "label interne hors plage", CheminUtf8(dossier));
        jeu.m_labels.push_back(l32[i]);
        jeu.m_origines.push_back({(int)o64[2 * i], (long)o64[2 * i + 1]});
    }
    if (jeu.HashCanonique() != d.Membre("hash_canonique").Texte())
        return FaireErreur(CodeErreur::ModeleIncompatible, "chargement", "hash canonique different", CheminUtf8(dossier));
    return jeu;
}

} // namespace decid
