// Tests de la lecture / export / sauvegarde des donnees (ACC-01, 02, 11, 24).
// Toutes les fixtures sont SYNTHETIQUES : generees par les tests dans verif::DossierTemporaire
// (sous le dossier de build). Seul test optionnel : lecture seule des donnees optdigits de reference.
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "Donnees.h"
#include "Fichiers.h"
#include "Sha256.h"
#include "Verifier.h"

using namespace decid;

namespace {

// ---------- outils ----------

struct Lcg {
    std::uint64_t m_etat;
    double Suivant() {   // uniforme [0,1)
        m_etat = m_etat * 6364136223846793005ULL + 1442695040888963407ULL;
        return (double)(m_etat >> 11) / 9007199254740992.0;
    }
};

void Ecrire(const fs::path& chemin, const std::string& texte) {
    std::ofstream f(chemin, std::ios::binary);
    f.write(texte.data(), (std::streamsize)texte.size());
}

std::string Hash(const fs::path& chemin) { return Sha256Fichier(chemin); }

std::vector<std::string> Lister(const fs::path& dossier) {
    std::vector<std::string> noms;
    for (auto& e : fs::directory_iterator(dossier)) noms.push_back(e.path().filename().string());
    std::sort(noms.begin(), noms.end());
    return noms;
}

bool Contient(const std::string& texte, const std::string& motif) {
    return texte.find(motif) != std::string::npos;
}

bool AvertissementContient(const JeuDeDonnees& jeu, const std::string& motif) {
    for (auto& a : jeu.m_avertissements) if (Contient(a, motif)) return true;
    return false;
}

template <class T>
bool Reussi(const Resultat<T>& r, const char* contexte) {
    if (!r.Ok()) verif::Echec(__FILE__, __LINE__, std::string(contexte) + " : " + r.Err().Texte());
    return r.Ok();
}

template <class T>
bool Refuse(const Resultat<T>& r, const char* contexte) {
    if (r.Ok()) verif::Echec(__FILE__, __LINE__, std::string(contexte) + " : succes inattendu, une erreur etait attendue");
    return !r.Ok();
}

ContratCsv Contrat(char separateur, ModeEnTete mode, int cible = 0) {
    ContratCsv c;
    c.m_separateur = separateur;
    c.m_enTete = mode;
    c.m_colonneCible = cible;
    return c;
}

Resultat<JeuDeDonnees> LireTexteCsv(const fs::path& dossier, const std::string& contenu, char sep, ModeEnTete mode) {
    Ecrire(dossier / "entree.csv", contenu);
    return LireCsv(dossier / "entree.csv", Contrat(sep, mode));
}

bool MemesBits(const std::vector<double>& a, const std::vector<double>& b) {
    return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(double)) == 0;
}

std::string Reel17(double v) {
    char tampon[64];
    std::snprintf(tampon, sizeof(tampon), "%.17g", v);
    return tampon;
}

// DAT historique synthetique : nbAttributs, nbEchantillons, puis les valeurs.
void EcrireDat(const fs::path& chemin, int p, const std::vector<std::vector<double>>& lignes) {
    std::string t = std::to_string(p) + "\n" + std::to_string(lignes.size()) + "\n";
    for (auto& l : lignes) {
        for (size_t i = 0; i < l.size(); ++i) t += (i ? " " : "") + Reel17(l[i]);
        t += "\n";
    }
    Ecrire(chemin, t);
}

// Jeu synthetique de reference pour export / sauvegarde : 3 labels externes, valeurs extremes incluses.
std::string CsvSynthetique() {
    static const char* labels[] = {"-3", "7", "42"};
    Lcg g{12345};
    std::string t;
    for (int i = 0; i < 40; ++i) {
        t += labels[i % 3];
        for (int a = 0; a < 5; ++a) {
            double v = (g.Suivant() * 2.0 - 1.0) * (a % 2 ? 1e5 : 1e-5);
            if (i == 0) { const double sp[] = {0.0, -0.0, 4.9e-324, 1.7976931348623157e308, 1.0 / 3.0}; v = sp[a]; }
            t += "," + Reel17(v);
        }
        t += "\n";
    }
    return t;
}

Resultat<JeuDeDonnees> JeuSynthetique(const fs::path& dossier) {
    Ecrire(dossier / "source.csv", CsvSynthetique());
    return LireCsv(dossier / "source.csv", Contrat(',', ModeEnTete::Non));
}

} // namespace

// ---------- CSV : contrat d'en-tete ----------

TEST(csv_sans_en_tete_conserve_la_premiere_ligne) {
    auto d = verif::DossierTemporaire("csv_sans_entete");
    auto r = LireTexteCsv(d, "1,0.5,0.25\n2,0.75,1\n1,3,4\n", ',', ModeEnTete::Non);
    if (!Reussi(r, "csv sans en-tete")) return;
    const JeuDeDonnees& j = r.Valeur();
    VERIFIER_EGAL(j.m_nbLignes, 3);
    VERIFIER_EGAL(j.m_nbAttributs, 2);
    VERIFIER(j.Valeur(0, 0) == 0.5);      // 1re valeur : la 1re ligne n'est pas sautee
    VERIFIER(j.Valeur(2, 1) == 4.0);      // derniere valeur
    VERIFIER_EGAL(j.m_origines[0].m_ligne, 1L);
    VERIFIER_EGAL(j.m_nomsAttributs.size(), (size_t)2);
    VERIFIER_EGAL(j.m_nomsAttributs[0], std::string("x1"));
    VERIFIER_EGAL(j.NbClasses(), 2);
}

TEST(csv_en_tete_oui_lit_les_noms) {
    auto d = verif::DossierTemporaire("csv_entete_oui");
    auto r = LireTexteCsv(d, "classe,alpha,beta\n1,2,3\n2,4,5\n", ',', ModeEnTete::Oui);
    if (!Reussi(r, "csv en-tete oui")) return;
    VERIFIER_EGAL(r.Valeur().m_nbLignes, 2);
    VERIFIER_EGAL(r.Valeur().m_nomsAttributs.size(), (size_t)2);
    VERIFIER_EGAL(r.Valeur().m_nomsAttributs[0], std::string("alpha"));
    VERIFIER_EGAL(r.Valeur().m_nomsAttributs[1], std::string("beta"));
    VERIFIER_EGAL(r.Valeur().m_origines[0].m_ligne, 2L);
}

TEST(csv_en_tete_oui_sans_observation_est_refuse) {
    auto d = verif::DossierTemporaire("csv_entete_seul");
    Refuse(LireTexteCsv(d, "classe,a,b\n", ',', ModeEnTete::Oui), "en-tete seul");
}

TEST(csv_auto_detecte_en_tete_observation_et_ambigu) {
    auto d = verif::DossierTemporaire("csv_auto");
    auto texte = LireTexteCsv(d, "classe,a,b\n1,2,3\n2,4,5\n", ',', ModeEnTete::Auto);
    if (Reussi(texte, "auto non numerique")) {
        VERIFIER_EGAL(texte.Valeur().m_nbLignes, 2);
        VERIFIER_EGAL(texte.Valeur().m_nomsAttributs[0], std::string("a"));
    }
    auto numerique = LireTexteCsv(d, "1,0.5,0.25\n2,0.75,1\n", ',', ModeEnTete::Auto);
    if (Reussi(numerique, "auto numerique")) VERIFIER_EGAL(numerique.Valeur().m_nbLignes, 2);
    auto mixte = LireTexteCsv(d, "x,1.0,abc\n1,2,3\n", ',', ModeEnTete::Auto);
    if (Refuse(mixte, "auto mixte")) {
        VERIFIER_EGAL(mixte.Err().m_ligne, 1L);
        VERIFIER(Contient(mixte.Err().m_message, "ambigu"));
    }
}

// ---------- CSV : formats ----------

TEST(csv_separateurs_point_virgule_et_tabulation) {
    auto d = verif::DossierTemporaire("csv_separateurs");
    auto pv = LireTexteCsv(d, "a;1.5;2\nb;3;4\n", ';', ModeEnTete::Non);
    if (Reussi(pv, "separateur ;")) {
        VERIFIER_EGAL(pv.Valeur().m_nbAttributs, 2);
        VERIFIER(pv.Valeur().Valeur(1, 0) == 3.0);
    }
    auto tab = LireTexteCsv(d, "a\t1.5\t2\nb\t3\t4\n", '\t', ModeEnTete::Non);
    if (Reussi(tab, "separateur tab")) {
        VERIFIER_EGAL(tab.Valeur().m_nbAttributs, 2);
        VERIFIER(tab.Valeur().Valeur(0, 0) == 1.5);
    }
}

TEST(csv_bom_crlf_et_lignes_vides) {
    auto d = verif::DossierTemporaire("csv_bom_crlf");
    auto bom = LireTexteCsv(d, "\xEF\xBB\xBF" "1,2\n3,4\n", ',', ModeEnTete::Non);
    if (Reussi(bom, "bom")) {
        VERIFIER_EGAL(bom.Valeur().m_nbLignes, 2);
        VERIFIER_EGAL(bom.Valeur().m_nomsClasses[0], std::string("1"));   // le BOM n'entre pas dans le label
    }
    auto r = LireTexteCsv(d, "1,2\r\n\r\n3,4\r\n   \r\n", ',', ModeEnTete::Non);
    if (!Reussi(r, "crlf + lignes vides")) return;
    VERIFIER_EGAL(r.Valeur().m_nbLignes, 2);
    VERIFIER(AvertissementContient(r.Valeur(), "vide"));
    VERIFIER_EGAL(r.Valeur().m_origines[1].m_ligne, 3L);   // numero de ligne physique
}

TEST(csv_guillemets) {
    auto d = verif::DossierTemporaire("csv_guillemets");
    auto r = LireTexteCsv(d, "\"a,b\",\"1.5\",2\n\"c\"\"d\",3,4\n", ',', ModeEnTete::Non);
    if (!Reussi(r, "guillemets")) return;
    VERIFIER_EGAL(r.Valeur().m_nbLignes, 2);
    VERIFIER_EGAL(r.Valeur().m_nomsClasses[0], std::string("a,b"));
    VERIFIER_EGAL(r.Valeur().m_nomsClasses[1], std::string("c\"d"));
    VERIFIER(r.Valeur().Valeur(0, 0) == 1.5);
    Refuse(LireTexteCsv(d, "\"a,1,2\n", ',', ModeEnTete::Non), "guillemet non ferme");
}

TEST(csv_nombre_de_colonnes_different_donne_le_numero_de_ligne) {
    auto d = verif::DossierTemporaire("csv_colonnes");
    auto court = LireTexteCsv(d, "1,2,3\n\n1,2\n", ',', ModeEnTete::Non);
    if (Refuse(court, "ligne trop courte")) {
        VERIFIER_EGAL(court.Err().m_ligne, 3L);
        VERIFIER(Contient(court.Err().Texte(), "ligne 3"));
    }
    auto long_ = LireTexteCsv(d, "1,2,3\n1,2,3\n1,2,3,4\n", ',', ModeEnTete::Non);
    if (Refuse(long_, "ligne trop longue")) VERIFIER_EGAL(long_.Err().m_ligne, 3L);
    auto mauvaisSep = LireTexteCsv(d, "1;2;3\n", ',', ModeEnTete::Non);
    if (Refuse(mauvaisSep, "mauvais separateur")) VERIFIER(Contient(mauvaisSep.Err().m_message, "separateur"));
}

TEST(csv_virgule_decimale_avec_point_virgule_est_refusee) {
    auto d = verif::DossierTemporaire("csv_virgule");
    auto r = LireTexteCsv(d, "a;0,5;2\nb;1;2\n", ';', ModeEnTete::Non);
    if (!Refuse(r, "virgule decimale")) return;
    VERIFIER(Contient(r.Err().m_message, "virgule"));
    VERIFIER_EGAL(r.Err().m_ligne, 1L);
    VERIFIER_EGAL(r.Err().m_colonne, 2L);
    Refuse(LireTexteCsv(d, "a,abc,2\nb,1,2\n", ',', ModeEnTete::Non), "valeur non numerique");
    Refuse(LireTexteCsv(d, "a,nan,2\nb,1,2\n", ',', ModeEnTete::Non), "valeur nan");
}

// ---------- labels ----------

TEST(labels_numeriques_tries_et_mappes) {
    auto d = verif::DossierTemporaire("labels_num");
    auto r = LireTexteCsv(d, "42,1\n-3,2\n7,3\n-3,4\n", ',', ModeEnTete::Non);
    if (!Reussi(r, "labels -3/7/42")) return;
    const JeuDeDonnees& j = r.Valeur();
    VERIFIER_EGAL(j.NbClasses(), 3);
    VERIFIER_EGAL(j.m_nomsClasses[0], std::string("-3"));
    VERIFIER_EGAL(j.m_nomsClasses[1], std::string("7"));
    VERIFIER_EGAL(j.m_nomsClasses[2], std::string("42"));
    VERIFIER((j.m_labels == std::vector<int>{2, 0, 1, 0}));
    auto reel = LireTexteCsv(d, "1.0,1\n2,2\n", ',', ModeEnTete::Non);
    if (Reussi(reel, "label 1.0")) {
        VERIFIER_EGAL(reel.Valeur().m_nomsClasses[0], std::string("1"));
        VERIFIER_EGAL(reel.Valeur().m_nomsClasses[1], std::string("2"));
    }
}

TEST(labels_invalides_sont_refuses) {
    auto d = verif::DossierTemporaire("labels_refus");
    Refuse(LireTexteCsv(d, "1.5,1\n2,2\n", ',', ModeEnTete::Non), "label fractionnaire");
    Refuse(LireTexteCsv(d, "nan,1\n2,2\n", ',', ModeEnTete::Non), "label nan");
    Refuse(LireTexteCsv(d, "NaN,1\n2,2\n", ',', ModeEnTete::Non), "label NaN");
    Refuse(LireTexteCsv(d, "inf,1\n2,2\n", ',', ModeEnTete::Non), "label inf");
    Refuse(LireTexteCsv(d, "-inf,1\n2,2\n", ',', ModeEnTete::Non), "label -inf");
    auto vide = LireTexteCsv(d, ",1\n2,2\n", ',', ModeEnTete::Non);
    if (Refuse(vide, "label vide")) VERIFIER_EGAL(vide.Err().m_ligne, 1L);
    auto melange = LireTexteCsv(d, "1,1\nchat,2\n", ',', ModeEnTete::Non);
    if (Refuse(melange, "melange nombres / textes")) VERIFIER(Contient(melange.Err().m_message, "melang"));
}

TEST(labels_textuels_tries_par_octets) {
    auto d = verif::DossierTemporaire("labels_txt");
    auto r = LireTexteCsv(d, "chien,1\nchat,2\nchat,3\n", ',', ModeEnTete::Non);
    if (!Reussi(r, "labels texte")) return;
    VERIFIER_EGAL(r.Valeur().m_nomsClasses[0], std::string("chat"));
    VERIFIER_EGAL(r.Valeur().m_nomsClasses[1], std::string("chien"));
    VERIFIER((r.Valeur().m_labels == std::vector<int>{1, 0, 0}));
}

// ---------- hash canonique ----------

TEST(hash_canonique_independant_du_formatage_mais_pas_de_l_ordre) {
    auto d = verif::DossierTemporaire("hash_canon");
    auto a = LireTexteCsv(d, "1,0.5\n2,1\n", ',', ModeEnTete::Non);
    if (!Reussi(a, "hash a")) return;
    std::string ha = a.Valeur().HashCanonique();
    auto b = LireTexteCsv(d, "1,0.50\n2,1.0\n", ',', ModeEnTete::Non);
    if (Reussi(b, "hash b")) VERIFIER_EGAL(b.Valeur().HashCanonique(), ha);
    auto c = LireTexteCsv(d, "2,1\n1,0.5\n", ',', ModeEnTete::Non);
    if (Reussi(c, "hash c")) VERIFIER(c.Valeur().HashCanonique() != ha);
    VERIFIER_EGAL(ha.size(), (size_t)64);
}

// ---------- export -> reimport ----------

TEST(export_puis_reimport_est_exact_avec_et_sans_en_tete) {
    auto d = verif::DossierTemporaire("export_reimport");
    auto avant = JeuSynthetique(d);
    if (!Reussi(avant, "jeu synthetique")) return;
    const JeuDeDonnees& j = avant.Valeur();
    fs::create_directories(d / "sortie");
    for (bool entete : {true, false}) {
        fs::path dest = d / "sortie" / (entete ? "avec.csv" : "sans.csv");
        auto st = ExporterCsv(j, dest, entete, false);
        if (!Reussi(st, "ExporterCsv")) continue;
        auto apres = LireCsv(dest, Contrat(',', entete ? ModeEnTete::Oui : ModeEnTete::Non));
        if (!Reussi(apres, "reimport")) continue;
        const JeuDeDonnees& k = apres.Valeur();
        VERIFIER_EGAL(k.m_nbLignes, j.m_nbLignes);
        VERIFIER_EGAL(k.m_nbAttributs, j.m_nbAttributs);
        VERIFIER(k.m_labels == j.m_labels);                // meme ordre
        VERIFIER(k.m_nomsClasses == j.m_nomsClasses);      // labels externes
        VERIFIER(k.m_nomsAttributs == j.m_nomsAttributs);
        VERIFIER(MemesBits(k.m_valeurs, j.m_valeurs));     // valeurs bit a bit
        VERIFIER_EGAL(k.HashCanonique(), j.HashCanonique());
    }
}

TEST(export_refuse_la_source_et_les_destinations_existantes) {
    auto d = verif::DossierTemporaire("export_refus");
    auto lu = JeuSynthetique(d);
    if (!Reussi(lu, "jeu synthetique")) return;
    const JeuDeDonnees& j = lu.Valeur();
    const fs::path source = d / "source.csv";
    const std::string avant = Hash(source);
    for (bool remplacer : {false, true}) {
        auto st = ExporterCsv(j, source, false, remplacer);
        if (Refuse(st, "export vers la source")) VERIFIER(st.Err().m_code == CodeErreur::EcritureRefusee);
        VERIFIER_EGAL(Hash(source), avant);
    }
    auto autreOrthographe = ExporterCsv(j, d / "." / "source.csv", false, true);   // meme fichier, autre ecriture
    Refuse(autreOrthographe, "export vers la source (chemin equivalent)");
    VERIFIER_EGAL(Hash(source), avant);

    const fs::path dest = d / "dest.csv";
    VERIFIER(ExporterCsv(j, dest, false, false).Ok());
    const std::string hashDest = Hash(dest);
    auto existe = ExporterCsv(j, dest, true, false);
    if (Refuse(existe, "destination existante")) VERIFIER(existe.Err().m_code == CodeErreur::EcritureRefusee);
    VERIFIER_EGAL(Hash(dest), hashDest);
    VERIFIER(ExporterCsv(j, dest, true, true).Ok());     // avec remplacement explicite
    VERIFIER(Hash(dest) != hashDest);
    VERIFIER(!fs::exists(d / "dest.csv.tmp"));
}

TEST(import_ne_cree_aucun_fichier_dans_le_dossier_source) {
    auto d = verif::DossierTemporaire("import_lecture_seule");
    Ecrire(d / "source.csv", CsvSynthetique());
    const auto avant = Lister(d);
    const std::string hash = Hash(d / "source.csv");
    auto r = LireCsv(d / "source.csv", Contrat(',', ModeEnTete::Non));
    Reussi(r, "import");
    VERIFIER(Lister(d) == avant);
    VERIFIER_EGAL(Hash(d / "source.csv"), hash);
    EcrireDat(d / "c0.dat", 2, {{1, 2}});
    EcrireDat(d / "c1.dat", 2, {{3, 4}});
    const auto avantDat = Lister(d);
    LireFichiersParRole({{d / "c0.dat", "0", RoleFichier::Apprentissage}, {d / "c1.dat", "1", RoleFichier::Test}});
    VERIFIER(Lister(d) == avantDat);
}

// ---------- CMC ----------

namespace {
void EcrireFixtureCmc(const fs::path& d) {
    fs::create_directories(d / "donnees");
    EcrireDat(d / "donnees" / "c0a.dat", 2, {{1, 2}, {3, 4}});
    EcrireDat(d / "donnees" / "c0b.dat", 2, {{5, 6}});
    EcrireDat(d / "donnees" / "c1.dat", 2, {{7, 8}, {9, 10}, {11, 12}});
    Ecrire(d / "jeu.cmc", "2\n2\n1\ndonnees/c0a.dat\ndonnees/c0b.dat\ndonnees/c1.dat\n");
}
} // namespace

TEST(cmc_chemins_relatifs_labels_par_rang) {
    auto d = verif::DossierTemporaire("cmc_ok");
    EcrireFixtureCmc(d);
    auto r = LireCmc(d / "jeu.cmc");
    if (!Reussi(r, "cmc")) return;
    const JeuDeDonnees& j = r.Valeur();
    VERIFIER_EGAL(j.m_nbLignes, 6);
    VERIFIER_EGAL(j.m_nbAttributs, 2);
    VERIFIER_EGAL(j.NbClasses(), 2);
    VERIFIER((j.m_labels == std::vector<int>{0, 0, 0, 1, 1, 1}));
    VERIFIER_EGAL(j.m_sources.size(), (size_t)3);
    VERIFIER_EGAL(j.m_sources[2].m_cheminDeclare, std::string("donnees/c1.dat"));
    VERIFIER(j.Valeur(5, 1) == 12.0);
}

TEST(cmc_fichier_absent_ou_incoherent_ne_donne_jamais_un_jeu_partiel) {
    auto d = verif::DossierTemporaire("cmc_erreurs");
    EcrireFixtureCmc(d);
    fs::remove(d / "donnees" / "c1.dat");
    auto absent = LireCmc(d / "jeu.cmc");
    if (Refuse(absent, "DAT absent")) VERIFIER(absent.Err().m_code == CodeErreur::Lecture);

    EcrireFixtureCmc(d);
    Ecrire(d / "donnees" / "c1.dat", "2\n4\n7 8\n9 10\n11 12\n");        // 4 annonces, 3 presents
    auto moins = LireCmc(d / "jeu.cmc");
    if (Refuse(moins, "plus d'echantillons annonces")) VERIFIER(Contient(moins.Err().m_message, "echantillons"));

    Ecrire(d / "donnees" / "c1.dat", "2\n2\n7 8\n9 10\n11 12\n");        // 2 annonces, 3 presents
    auto trop = LireCmc(d / "jeu.cmc");
    if (Refuse(trop, "valeurs en trop")) VERIFIER(Contient(trop.Err().m_message, "en trop"));

    Ecrire(d / "donnees" / "c1.dat", "2\n1\n7 abc\n");
    Refuse(LireCmc(d / "jeu.cmc"), "valeur DAT non numerique");
}

TEST(cmc_relocalisation_de_prefixe) {
    auto d = verif::DossierTemporaire("cmc_reloc");
    fs::create_directories(d / "nouveau");
    EcrireDat(d / "nouveau" / "c0.dat", 2, {{1, 2}});
    EcrireDat(d / "nouveau" / "c1.dat", 2, {{3, 4}});
    const std::string ancien = "C:\\ancien\\machine\\data";
    Ecrire(d / "jeu.cmc", "2\n1\n1\n" + ancien + "\\c0.dat\n" + ancien + "\\c1.dat\n");
    auto sans = LireCmc(d / "jeu.cmc");
    Refuse(sans, "chemin historique sans relocalisation");
    auto avec = LireCmc(d / "jeu.cmc", {{ancien, CheminUtf8(d / "nouveau")}});
    if (!Reussi(avec, "relocalisation")) return;
    VERIFIER_EGAL(avec.Valeur().m_nbLignes, 2);
    VERIFIER_EGAL(avec.Valeur().m_sources[0].m_cheminDeclare, ancien + "\\c0.dat");   // le declare est conserve
    VERIFIER(Contient(avec.Valeur().m_sources[0].m_cheminResolu, "nouveau"));
}

// ---------- roles ----------

namespace {
void EcrireFixtureRoles(const fs::path& d, bool doublon) {
    EcrireDat(d / "c0.dat", 2, {{1, 2}, {3, 4}});
    EcrireDat(d / "c1.dat", 2, {{5, 6}, {7, 8}, {9, 10}});
    if (doublon) EcrireDat(d / "ctes0.dat", 2, {{1, 2}});     // numeriquement egale a une ligne d'apprentissage
    else EcrireDat(d / "ctes0.dat", 2, {{11, 12}});
    EcrireDat(d / "ctes1.dat", 2, {{13, 14}});
}
std::vector<FichierRole> FichiersRoles(const fs::path& d) {
    return {{d / "c0.dat", "0", RoleFichier::Apprentissage}, {d / "c1.dat", "1", RoleFichier::Apprentissage},
            {d / "ctes0.dat", "0", RoleFichier::Test}, {d / "ctes1.dat", "1", RoleFichier::Test}};
}
} // namespace

TEST(roles_sources_avec_role_et_origines) {
    auto d = verif::DossierTemporaire("roles_ok");
    EcrireFixtureRoles(d, false);
    auto r = LireFichiersParRole(FichiersRoles(d));
    if (!Reussi(r, "roles")) return;
    const JeuDeDonnees& j = r.Valeur();
    VERIFIER_EGAL(j.m_nbLignes, 7);
    VERIFIER_EGAL(j.m_sources.size(), (size_t)4);
    VERIFIER(j.m_sources[0].m_role == RoleFichier::Apprentissage);
    VERIFIER(j.m_sources[2].m_role == RoleFichier::Test);
    VERIFIER_EGAL(j.m_sources[3].m_classeDeclaree, std::string("1"));
    VERIFIER_EGAL(j.m_origines[0].m_ligne, 3L);     // 1re ligne de donnees d'un DAT
    VERIFIER_EGAL(j.m_origines[1].m_ligne, 4L);
    VERIFIER(!AvertissementContient(j, "numeriquement"));
}

TEST(roles_refus_du_meme_fichier_et_du_role_manquant) {
    auto d = verif::DossierTemporaire("roles_refus");
    EcrireFixtureRoles(d, false);
    auto deux = LireFichiersParRole({{d / "c0.dat", "0", RoleFichier::Apprentissage},
                                     {d / "c0.dat", "0", RoleFichier::Test}});
    if (Refuse(deux, "meme fichier deux fois")) VERIFIER(Contient(deux.Err().m_message, "deux fois"));
    auto autreOrthographe = LireFichiersParRole({{d / "c0.dat", "0", RoleFichier::Apprentissage},
                                                 {d / "." / "c0.dat", "1", RoleFichier::Test}});
    Refuse(autreOrthographe, "meme fichier, chemin equivalent");
    Refuse(LireFichiersParRole({{d / "c0.dat", "0", RoleFichier::Aucun}, {d / "c1.dat", "1", RoleFichier::Test}}), "role manquant");
    Refuse(LireFichiersParRole({}), "aucun fichier");
}

TEST(roles_lignes_egales_entre_apprentissage_et_test_sont_signalees_pas_supprimees) {
    auto d = verif::DossierTemporaire("roles_doublon");
    EcrireFixtureRoles(d, true);
    auto r = LireFichiersParRole(FichiersRoles(d));
    if (!Reussi(r, "roles avec doublon")) return;
    VERIFIER_EGAL(r.Valeur().m_nbLignes, 7);                       // aucune suppression
    VERIFIER(AvertissementContient(r.Valeur(), "numeriquement"));
}

TEST(optdigits_reference_si_present) {
    // donnees de TP originales (facultatives) : DECID_OPTDIGITS = dossier de c0.dat..c9.dat et ctes0.dat..ctes9.dat
    const char* env = std::getenv("DECID_OPTDIGITS");
    if (!env || !*env) { std::printf("  (DECID_OPTDIGITS non defini : test saute)\n"); return; }
    const fs::path dossier = env;
    std::vector<FichierRole> fichiers;
    for (int c = 0; c < 10; ++c)
        fichiers.push_back({dossier / ("c" + std::to_string(c) + ".dat"), std::to_string(c), RoleFichier::Apprentissage});
    for (int c = 0; c < 10; ++c)
        fichiers.push_back({dossier / ("ctes" + std::to_string(c) + ".dat"), std::to_string(c), RoleFichier::Test});
    std::error_code ec;
    for (auto& f : fichiers)
        if (!fs::exists(f.m_chemin, ec)) { std::printf("  (optdigits absent : test saute)\n"); return; }
    const std::string avant = Hash(dossier / "c0.dat");
    auto r = LireFichiersParRole(fichiers);
    if (Reussi(r, "optdigits")) {
        const JeuDeDonnees& j = r.Valeur();
        long apprentissage = 0, test = 0;
        for (auto& o : j.m_origines) (j.m_sources[(size_t)o.m_source].m_role == RoleFichier::Test ? test : apprentissage)++;
        VERIFIER_EGAL(apprentissage, 3824L);
        VERIFIER_EGAL(test, 1798L);
        VERIFIER_EGAL(j.m_nbAttributs, 64);
        VERIFIER_EGAL(j.NbClasses(), 10);
    }
    VERIFIER_EGAL(Hash(dossier / "c0.dat"), avant);   // lecture seule
}

// ---------- sauvegarde binaire du jeu ----------

TEST(sauver_puis_charger_jeu_est_exact) {
    auto d = verif::DossierTemporaire("jeu_aller_retour");
    EcrireFixtureRoles(d, true);
    auto lu = LireFichiersParRole(FichiersRoles(d));
    if (!Reussi(lu, "lecture roles")) return;
    const JeuDeDonnees& j = lu.Valeur();
    VERIFIER(SauverJeu(j, d / "jeu").Ok());
    auto r = ChargerJeu(d / "jeu");
    if (!Reussi(r, "ChargerJeu")) return;
    const JeuDeDonnees& k = r.Valeur();
    VERIFIER_EGAL(k.m_nbLignes, j.m_nbLignes);
    VERIFIER_EGAL(k.m_nbAttributs, j.m_nbAttributs);
    VERIFIER(MemesBits(k.m_valeurs, j.m_valeurs));
    VERIFIER(k.m_labels == j.m_labels);
    VERIFIER(k.m_nomsClasses == j.m_nomsClasses);
    VERIFIER(k.m_nomsAttributs == j.m_nomsAttributs);
    VERIFIER(k.m_avertissements == j.m_avertissements);
    VERIFIER_EGAL(k.m_contrat, j.m_contrat);
    VERIFIER_EGAL(k.HashCanonique(), j.HashCanonique());
    VERIFIER_EGAL(k.m_origines.size(), j.m_origines.size());
    for (size_t i = 0; i < j.m_origines.size() && i < k.m_origines.size(); ++i) {
        VERIFIER_EGAL(k.m_origines[i].m_source, j.m_origines[i].m_source);
        VERIFIER_EGAL(k.m_origines[i].m_ligne, j.m_origines[i].m_ligne);
    }
    VERIFIER_EGAL(k.m_sources.size(), j.m_sources.size());
    for (size_t i = 0; i < j.m_sources.size() && i < k.m_sources.size(); ++i) {
        VERIFIER_EGAL(k.m_sources[i].m_sha256, j.m_sources[i].m_sha256);
        VERIFIER_EGAL(k.m_sources[i].m_cheminResolu, j.m_sources[i].m_cheminResolu);
        VERIFIER_EGAL(k.m_sources[i].m_cheminDeclare, j.m_sources[i].m_cheminDeclare);
        VERIFIER(k.m_sources[i].m_role == j.m_sources[i].m_role);
        VERIFIER_EGAL(k.m_sources[i].m_classeDeclaree, j.m_sources[i].m_classeDeclaree);
        VERIFIER(k.m_sources[i].m_taille == j.m_sources[i].m_taille);
    }
}

TEST(charger_jeu_detecte_un_octet_altere) {
    auto d = verif::DossierTemporaire("jeu_altere");
    auto lu = JeuSynthetique(d);
    if (!Reussi(lu, "jeu synthetique")) return;
    VERIFIER(SauverJeu(lu.Valeur(), d / "jeu").Ok());
    VERIFIER(ChargerJeu(d / "jeu").Ok());
    {
        std::fstream f(d / "jeu" / "valeurs.f64", std::ios::in | std::ios::out | std::ios::binary);
        f.seekp(37);
        char c = 0;
        f.seekg(37);
        f.get(c);
        f.seekp(37);
        f.put((char)(c ^ 0x01));
    }
    Refuse(ChargerJeu(d / "jeu"), "valeurs.f64 altere");
    Refuse(ChargerJeu(d / "inexistant"), "dossier de jeu inexistant");
}

int main(int argc, char** argv) { return LancerTests(argc, argv); }
