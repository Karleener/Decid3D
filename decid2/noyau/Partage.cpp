// Partage apprentissage/test : implementation deterministe et portable.
#include "Partage.h"

#include <algorithm>
#include <charconv>
#include <cmath>

#include "Sha256.h"

namespace decid {

namespace {

Erreur ErreurPartage(const std::string& message) {
    return FaireErreur(CodeErreur::DonneesInvalides, "partage", message);
}

std::string NombreTexte(double v) {
    char tampon[64];
    auto r = std::to_chars(tampon, tampon + sizeof(tampon), v);
    return std::string(tampon, r.ptr);
}

bool TexteVersDouble(const std::string& texte, double& v) {
    auto r = std::from_chars(texte.data(), texte.data() + texte.size(), v);
    return r.ec == std::errc() && r.ptr == texte.data() + texte.size();
}

bool TexteVersU64(const std::string& texte, std::uint64_t& v) {
    auto r = std::from_chars(texte.data(), texte.data() + texte.size(), v);
    return r.ec == std::errc() && r.ptr == texte.data() + texte.size();
}

std::uint64_t SplitMix64(std::uint64_t& etat) {
    etat += 0x9E3779B97F4A7C15ULL;
    std::uint64_t z = etat;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

ValeurJson TableauEntiers(const std::vector<int>& v) {
    ValeurJson t = ValeurJson::Tableau();
    for (int x : v) t.Ajouter(ValeurJson(x));
    return t;
}

bool LireEntiers(const ValeurJson& j, std::vector<int>& sortie) {
    if (j.GetType() != ValeurJson::Type::Tableau) return false;
    for (const auto& e : j.Elements()) {
        if (e.GetType() != ValeurJson::Type::Entier && e.GetType() != ValeurJson::Type::Nombre) return false;
        sortie.push_back(static_cast<int>(e.Entier()));
    }
    return true;
}

void EcrireIndices(std::string& s, const char* nom, const std::vector<int>& v) {
    s += nom;
    s += '=';
    for (int x : v) { s += std::to_string(x); s += ','; }
    s += ';';
}

size_t NbApprentissage(size_t n, double ratio) {
    return static_cast<size_t>(std::floor(static_cast<double>(n) * ratio));
}

std::vector<std::vector<int>> IndicesParClasse(const JeuDeDonnees& jeu) {
    std::vector<std::vector<int>> groupes(jeu.NbClasses());
    for (int i = 0; i < jeu.m_nbLignes; ++i) groupes[jeu.m_labels[i]].push_back(i);
    return groupes;
}

// Les k premieres lignes du groupe vont en apprentissage, le reste en test
void AjouterPremiers(const std::vector<int>& lignes, double ratio, Partage& p) {
    size_t k = NbApprentissage(lignes.size(), ratio);
    for (size_t i = 0; i < lignes.size(); ++i) (i < k ? p.m_apprentissage : p.m_test).push_back(lignes[i]);
}

void PartagerParClasseAleatoire(const JeuDeDonnees& jeu, double ratio, std::uint64_t graine, Partage& p) {
    std::uint64_t etat = graine;
    auto groupes = IndicesParClasse(jeu);
    for (auto& g : groupes) {
        for (size_t i = g.size(); i > 1; --i) {
            size_t j = static_cast<size_t>(TirageUniforme(etat, i));
            std::swap(g[i - 1], g[j]);
        }
        AjouterPremiers(g, ratio, p);
    }
}

Statut PartagerParRoles(const JeuDeDonnees& jeu, Partage& p) {
    for (int i = 0; i < jeu.m_nbLignes; ++i) {
        int s = (i < (int)jeu.m_origines.size()) ? jeu.m_origines[i].m_source : -1;
        if (s < 0 || s >= (int)jeu.m_sources.size())
            return ErreurPartage("ligne " + std::to_string(i + 1) + " : source inconnue");
        RoleFichier role = jeu.m_sources[s].m_role;
        if (role == RoleFichier::Aucun)
            return ErreurPartage("ligne " + std::to_string(i + 1) +
                                 " : le fichier source n'a pas de role (apprentissage ou test)");
        (role == RoleFichier::Apprentissage ? p.m_apprentissage : p.m_test).push_back(i);
    }
    return Succes();
}

void CalculerDiagnostics(const JeuDeDonnees& jeu, Partage& p) {
    auto app = ComptesClasses(jeu, p.m_apprentissage);
    auto test = ComptesClasses(jeu, p.m_test);
    for (int c = 0; c < jeu.NbClasses(); ++c) {
        if (app[c] == 0) p.m_diagnostics.push_back("classe " + jeu.m_nomsClasses[c] + " : 0 ligne en apprentissage");
        if (test[c] == 0) p.m_diagnostics.push_back("classe " + jeu.m_nomsClasses[c] + " : 0 ligne en test");
    }
    if (p.m_test.empty())
        p.m_diagnostics.push_back("test vide : evaluation impossible, apprentissage seul autorise");
}

} // namespace

const char* NomMethode(MethodePartage m) {
    switch (m) {
        case MethodePartage::PremiersN: return "premiers_n";
        case MethodePartage::PremiersNParClasse: return "premiers_n_par_classe";
        case MethodePartage::AleatoireStratifie: return "aleatoire_stratifie";
        case MethodePartage::ParRoles: return "par_roles";
    }
    return "inconnue";
}

bool MethodeDepuisNom(const std::string& nom, MethodePartage& m) {
    const MethodePartage toutes[] = {MethodePartage::PremiersN, MethodePartage::PremiersNParClasse,
                                     MethodePartage::AleatoireStratifie, MethodePartage::ParRoles};
    for (auto cand : toutes) {
        if (nom == NomMethode(cand)) { m = cand; return true; }
    }
    return false;
}

std::uint64_t TirageUniforme(std::uint64_t& etat, std::uint64_t borne) {
    if (borne == 0) return 0;
    const std::uint64_t seuil = (0 - borne) % borne; // (2^64 - borne) % borne
    std::uint64_t r = SplitMix64(etat);
    while (r < seuil) r = SplitMix64(etat);
    return r % borne;
}

std::vector<long> ComptesClasses(const JeuDeDonnees& jeu, const std::vector<int>& indices) {
    std::vector<long> comptes(jeu.NbClasses(), 0);
    for (int i : indices) {
        if (i < 0 || i >= (int)jeu.m_labels.size()) continue;
        int c = jeu.m_labels[i];
        if (c >= 0 && c < (int)comptes.size()) ++comptes[c];
    }
    return comptes;
}

std::string Partage::Hash() const {
    std::string s = "decid.partage/v1;methode=";
    s += NomMethode(m_methode);
    s += ";ratio=" + NombreTexte(m_ratio);
    s += ";graine=" + std::to_string(m_graine);
    s += ";inverse=" + std::string(m_inverse ? "1" : "0");
    s += ";hashJeu=" + m_hashJeu + ";";
    EcrireIndices(s, "apprentissage", m_apprentissage);
    EcrireIndices(s, "test", m_test);
    return Sha256Texte(s);
}

ValeurJson Partage::VersJson() const {
    ValeurJson j = ValeurJson::Objet();
    j["methode"] = ValeurJson(NomMethode(m_methode));
    j["ratio"] = ValeurJson(m_ratio);
    j["ratio_texte"] = ValeurJson(NombreTexte(m_ratio));
    j["graine"] = ValeurJson(std::to_string(m_graine));
    j["inverse"] = ValeurJson(m_inverse);
    j["generateur"] = ValeurJson(m_generateur);
    j["hash_jeu"] = ValeurJson(m_hashJeu);
    j["nb_lignes"] = ValeurJson(static_cast<long long>(m_apprentissage.size() + m_test.size()));
    j["apprentissage"] = TableauEntiers(m_apprentissage);
    j["test"] = TableauEntiers(m_test);
    ValeurJson diag = ValeurJson::Tableau();
    for (const auto& d : m_diagnostics) diag.Ajouter(ValeurJson(d));
    j["diagnostics"] = diag;
    j["hash"] = ValeurJson(Hash());
    return j;
}

Statut ValiderPartage(const Partage& partage, int nbLignes) {
    std::vector<char> vu(nbLignes > 0 ? nbLignes : 0, 0);
    const std::vector<int>* ensembles[2] = {&partage.m_apprentissage, &partage.m_test};
    for (const auto* ens : ensembles) {
        for (int i : *ens) {
            if (i < 0 || i >= nbLignes)
                return ErreurPartage("indice " + std::to_string(i) + " hors de [0," + std::to_string(nbLignes) + ")");
            if (vu[i])
                return ErreurPartage("indice " + std::to_string(i) +
                                     " present plusieurs fois (doublon ou ensembles non disjoints)");
            vu[i] = 1;
        }
    }
    if (partage.m_apprentissage.size() + partage.m_test.size() != static_cast<size_t>(nbLignes))
        return ErreurPartage("l'union de l'apprentissage et du test ne couvre pas toutes les lignes");
    if (partage.m_apprentissage.empty()) return ErreurPartage("apprentissage vide");
    return Succes();
}

Resultat<Partage> CreerPartage(const JeuDeDonnees& jeu, MethodePartage methode, double ratio,
                               std::uint64_t graine, bool inverse) {
    if (jeu.m_nbLignes <= 0) return ErreurPartage("jeu de donnees vide");
    if (methode != MethodePartage::ParRoles && !(ratio > 0.0 && ratio <= 1.0))
        return ErreurPartage("ratio d'apprentissage hors de ]0,1] : " + NombreTexte(ratio));

    Partage p;
    p.m_methode = methode;
    p.m_ratio = ratio;
    p.m_graine = graine;
    p.m_inverse = inverse;
    p.m_generateur = "splitmix64+rejet+fisher_yates_decid_v1";
    p.m_hashJeu = jeu.HashCanonique();

    if (methode == MethodePartage::PremiersN) {
        std::vector<int> toutes(jeu.m_nbLignes);
        for (int i = 0; i < jeu.m_nbLignes; ++i) toutes[i] = i;
        AjouterPremiers(toutes, ratio, p);
    } else if (methode == MethodePartage::PremiersNParClasse) {
        for (const auto& g : IndicesParClasse(jeu)) AjouterPremiers(g, ratio, p);
    } else if (methode == MethodePartage::AleatoireStratifie) {
        PartagerParClasseAleatoire(jeu, ratio, graine, p);
    } else {
        Statut s = PartagerParRoles(jeu, p);
        if (!s.Ok()) return s.Err();
    }

    std::sort(p.m_apprentissage.begin(), p.m_apprentissage.end());
    std::sort(p.m_test.begin(), p.m_test.end());
    if (inverse) std::swap(p.m_apprentissage, p.m_test);

    Statut valide = ValiderPartage(p, jeu.m_nbLignes);
    if (!valide.Ok()) return valide.Err();
    CalculerDiagnostics(jeu, p);
    return p;
}

Resultat<Partage> PartageDepuisJson(const ValeurJson& json) {
    if (json.GetType() != ValeurJson::Type::Objet) return ErreurPartage("partage JSON : objet attendu");
    Partage p;
    if (!MethodeDepuisNom(json.Membre("methode").Texte(), p.m_methode))
        return ErreurPartage("partage JSON : methode inconnue");
    const std::string ratioTexte = json.Membre("ratio_texte").Texte();
    if (ratioTexte.empty() || !TexteVersDouble(ratioTexte, p.m_ratio)) p.m_ratio = json.Membre("ratio").Nombre();
    if (!TexteVersU64(json.Membre("graine").Texte(), p.m_graine))
        return ErreurPartage("partage JSON : graine invalide");
    p.m_inverse = json.Membre("inverse").Booleen();
    p.m_generateur = json.Membre("generateur").Texte();
    p.m_hashJeu = json.Membre("hash_jeu").Texte();
    if (!LireEntiers(json.Membre("apprentissage"), p.m_apprentissage) ||
        !LireEntiers(json.Membre("test"), p.m_test))
        return ErreurPartage("partage JSON : indices invalides");
    for (const auto& d : json.Membre("diagnostics").Elements()) p.m_diagnostics.push_back(d.Texte());

    Statut valide = ValiderPartage(p, static_cast<int>(p.m_apprentissage.size() + p.m_test.size()));
    if (!valide.Ok()) return valide.Err();
    if (json.Membre("hash").Texte() != p.Hash())
        return ErreurPartage("partage JSON : le hash relu ne correspond pas au contenu");
    return p;
}

} // namespace decid
