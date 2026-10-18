#include "Moteur.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>

#include "Fichiers.h"
#include "Sha256.h"

namespace studio {

Moteur::Moteur(const fs::path& dossierTravail, app::ClientWorker& worker) : m_worker(worker) {
    std::error_code ec;
    fs::create_directories(dossierTravail, ec);
    auto ouvert = fs::exists(dossierTravail / "projet.json") ? app::Projet::Ouvrir(dossierTravail)
                                                             : app::Projet::Creer(dossierTravail, "travail");
    if (!ouvert.Ok()) {
        m_erreurInitiale = ouvert.Err().Texte();
        return;
    }
    m_projet = std::make_unique<app::Projet>(std::move(ouvert.Valeur()));
    m_abonneFin = m_worker.AbonnerFin([this](const app::FinTache& fin) {
        auto it = m_suites.find(fin.m_id);
        if (it == m_suites.end()) return;
        Suite suite = std::move(it->second);
        m_suites.erase(it);
        m_progressions.erase(fin.m_id);
        FinCalcul f;
        f.m_id = fin.m_id;
        f.m_ok = fin.m_etat == "succeeded";
        f.m_resultat = fin.m_dossier / "resultat";
        if (!f.m_ok) f.m_erreur = fin.m_raison.empty() ? "le moteur a échoué (" + fin.m_etat + ")" : fin.m_raison;
        suite(f);
    });
    m_abonneProgression = m_worker.AbonnerProgression([this](const std::string& id, const app::EvenementWorker& e) {
        auto it = m_progressions.find(id);
        if (it != m_progressions.end() && it->second) it->second(e.m_phase, e.m_i, e.m_n);
    });
}

Moteur::~Moteur() {
    if (m_abonneFin >= 0) m_worker.Desabonner(m_abonneFin);
    if (m_abonneProgression >= 0) m_worker.Desabonner(m_abonneProgression);
}

bool Moteur::Executer(decid::ValeurJson requete, Suite suite, Progression progression) {
    auto echec = [&](const std::string& message) {
        FinCalcul f;
        f.m_erreur = message;
        suite(f);
        return false;
    };
    if (!m_projet) return echec("dossier de travail indisponible : " + m_erreurInitiale);
    const std::string operation = requete.Membre("operation").Texte().empty() ? "experience" : requete.Membre("operation").Texte();
    const std::string famille = requete.Membre("algorithme").Membre("famille").Texte();
    const std::string id = m_projet->NouvelIdTache(operation, famille);
    auto chemin = m_projet->EcrireRequete(id, std::move(requete));
    if (!chemin.Ok()) return echec(chemin.Err().Texte());
    m_suites[id] = std::move(suite);
    if (progression) m_progressions[id] = std::move(progression);
    auto lance = m_worker.Lancer(chemin.Valeur(), m_projet->DossierTaches());
    if (!lance.Ok()) {
        Suite s = std::move(m_suites[id]);
        m_suites.erase(id);
        m_progressions.erase(id);
        FinCalcul f;
        f.m_id = id;
        f.m_erreur = lance.Err().Texte();
        s(f);
        return false;
    }
    return true;
}

bool Moteur::Occupe() const { return m_worker.LourdeEnCours(); }
void Moteur::AnnulerTout() { m_worker.AnnulerTout(); }
fs::path Moteur::DossierResultat(const std::string& id) const { return m_projet ? m_projet->DossierTache(id) / "resultat" : fs::path(); }
fs::path Moteur::DossierTravail() const { return m_projet ? m_projet->Dossier() : fs::path(); }

decid::Resultat<fs::path> Moteur::EcrireDonnees(const std::string& contenu, const std::string& extension) {
    if (!m_projet) {
        decid::Erreur e;
        e.m_phase = "donnees";
        e.m_message = "dossier de travail indisponible";
        return e;
    }
    const fs::path dossier = m_projet->Dossier() / "donnees";
    std::error_code ec;
    fs::create_directories(dossier, ec);
    const fs::path chemin = dossier / (decid::Sha256Texte(contenu).substr(0, 24) + "." + extension);
    if (fs::exists(chemin, ec)) return chemin;   // meme contenu, meme fichier
    auto st = decid::EcrireFichierAtomique(chemin, contenu, false);
    if (!st.Ok() && !fs::exists(chemin, ec)) return st.Err();
    return chemin;
}

// ---------- artefacts ----------
decid::Resultat<decid::ValeurJson> LireJson(const fs::path& chemin) {
    auto texte = decid::LireFichierTexte(chemin);
    if (!texte.Ok()) return texte.Err();
    return decid::LireJson(texte.Valeur());
}

template <typename T> std::vector<T> LireBinaire(const fs::path& chemin) {
    std::ifstream f(chemin, std::ios::binary | std::ios::ate);
    if (!f) return {};
    const std::streamoff taille = f.tellg();
    std::vector<T> v(taille > 0 ? (size_t)taille / sizeof(T) : 0);
    f.seekg(0);
    if (!v.empty()) f.read(reinterpret_cast<char*>(v.data()), (std::streamsize)(v.size() * sizeof(T)));
    return v;
}
template std::vector<double> LireBinaire<double>(const fs::path&);
template std::vector<float> LireBinaire<float>(const fs::path&);
template std::vector<std::int32_t> LireBinaire<std::int32_t>(const fs::path&);
template std::vector<std::int16_t> LireBinaire<std::int16_t>(const fs::path&);
template std::vector<std::uint8_t> LireBinaire<std::uint8_t>(const fs::path&);

namespace {
decid::Erreur ErreurArtefact(const std::string& message) {
    decid::Erreur e;
    e.m_phase = "artefacts";
    e.m_message = message;
    return e;
}
std::vector<std::string> Textes(const decid::ValeurJson& tableau) {
    std::vector<std::string> v;
    for (const auto& e : tableau.Elements()) v.push_back(e.Texte());
    return v;
}
std::vector<int> Entiers(const decid::ValeurJson& tableau) {
    std::vector<int> v;
    for (const auto& e : tableau.Elements()) v.push_back((int)e.Entier());
    return v;
}
} // namespace

decid::Resultat<JeuPublie> LireJeuPublie(const fs::path& resultat) {
    auto jeu = LireJson(resultat / "jeu" / "jeu.json");
    if (!jeu.Ok()) return jeu.Err();
    JeuPublie j;
    j.m_attributs = Textes(jeu.Valeur().Membre("attributs"));
    j.m_classes = Textes(jeu.Valeur().Membre("classes"));
    j.m_valeurs = LireBinaire<double>(resultat / "jeu" / "valeurs.f64");
    j.m_labels = LireBinaire<std::int32_t>(resultat / "jeu" / "labels.i32");
    if (j.m_attributs.empty() || j.m_valeurs.size() != j.m_labels.size() * j.m_attributs.size())
        return ErreurArtefact("jeu publié incohérent");
    auto partage = LireJson(resultat / "partage.json");
    if (partage.Ok()) {
        j.m_apprentissage = Entiers(partage.Valeur().Membre("apprentissage"));
        j.m_test = Entiers(partage.Valeur().Membre("test"));
    }
    return j;
}

decid::Resultat<Evaluation> LireEvaluation(const fs::path& resultat) {
    auto ev = LireJson(resultat / "evaluation.json");
    if (!ev.Ok()) return ev.Err();
    const auto& rapport = ev.Valeur().Membre("rapport");
    Evaluation e;
    const auto& ex = rapport.Membre("agregats").Membre("exactitude");
    e.m_definie = ex.Membre("status").Texte() == "defined";
    e.m_exactitude = ex.Membre("value").Nombre();
    e.m_population = ev.Valeur().Membre("population").Texte();
    const auto& conf = rapport.Membre("confusion");
    e.m_classes = Textes(conf.Membre("classes"));
    for (const auto& ligne : conf.Membre("comptes").Elements()) {
        std::vector<long long> l;
        for (const auto& v : ligne.Elements()) l.push_back(v.Entier());
        e.m_confusion.push_back(std::move(l));
    }
    const auto& td = ev.Valeur().Membre("temps_decision");
    if (td.Contient("par_echantillon_us")) e.m_tempsDecisionUs = td.Membre("par_echantillon_us").Nombre();
    return e;
}

decid::Resultat<ImageMoteur> LireImage(const fs::path& resultat) {
    auto js = LireJson(resultat / "image.json");
    if (!js.Ok()) return js.Err();
    const auto& j = js.Valeur();
    ImageMoteur im;
    im.m_largeur = (int)j.Membre("largeur").Entier();
    im.m_hauteur = (int)j.Membre("hauteur").Entier();
    const auto& r = j.Membre("requete");
    im.m_xMin = r.Membre("xmin").Nombre(); im.m_xMax = r.Membre("xmax").Nombre();
    im.m_yMin = r.Membre("ymin").Nombre(); im.m_yMax = r.Membre("ymax").Nombre();
    im.m_classes = LireBinaire<std::int16_t>(resultat / "image.i16");
    if (im.m_classes.size() != (size_t)im.m_largeur * im.m_hauteur) return ErreurArtefact("image de décision incohérente");
    const auto& score = j.Membre("score");
    if (score.Membre("statut").Texte() == "defini") {
        im.m_scores = LireBinaire<std::uint8_t>(resultat / "image_score.u8");
        if (im.m_scores.size() != im.m_classes.size()) im.m_scores.clear();
        im.m_definitionScore = score.Membre("definition").Texte();
    } else {
        im.m_definitionScore = score.Membre("raison").Texte();
    }
    return im;
}

std::vector<int> LirePredictions(const fs::path& resultat, const std::vector<std::string>& classes, int lignes) {
    std::vector<int> v((size_t)std::max(0, lignes), -1);
    auto csv = decid::LireFichierTexte(resultat / "predictions.csv");
    if (!csv.Ok()) return v;
    const std::string& t = csv.Valeur();
    size_t debut = t.find('\n');
    while (debut != std::string::npos && debut + 1 < t.size()) {
        const size_t fin = t.find('\n', debut + 1);
        std::string ligne = t.substr(debut + 1, (fin == std::string::npos ? t.size() : fin) - debut - 1);
        if (!ligne.empty() && ligne.back() == '\r') ligne.pop_back();
        const size_t p1 = ligne.find(';'), p3 = ligne.rfind(';');
        if (p1 != std::string::npos && p3 != std::string::npos && p3 > p1) {
            const int i = std::atoi(ligne.substr(0, p1).c_str());
            const auto trouve = std::find(classes.begin(), classes.end(), ligne.substr(p3 + 1));
            if (i >= 0 && i < (int)v.size() && trouve != classes.end()) v[(size_t)i] = (int)(trouve - classes.begin());
        }
        debut = fin;
    }
    return v;
}

} // namespace studio
