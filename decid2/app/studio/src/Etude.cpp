#include "Etude.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <chrono>
#include <cstdio>

#include "Fichiers.h"
#include "Plateforme.h"

namespace studio {

namespace {

// ---------- bibliotheque ----------
struct EntreeCatalogue { const char* m_cle; const char* m_nom; const char* m_fichier; const char* m_desc; bool m_enTete; };
const EntreeCatalogue CATALOGUE[] = {
#include "Catalogue.inc"
};
constexpr int NB_JEUX = (int)(sizeof(CATALOGUE) / sizeof(CATALOGUE[0]));

// ---------- familles et reglage principal ----------
struct ValeurFamille { const char* m_libelle; const char* m_detail; const char* m_cle; double m_nombre; };
struct FamilleDecid {
    const char* m_id; const char* m_court; const char* m_nom; const char* m_reglage; const char* m_aide;
    std::vector<ValeurFamille> m_valeurs;
    int m_defaut;
};
const std::vector<FamilleDecid>& Familles() {
    static const std::vector<FamilleDecid> f = {
        {"svm", "SVM", "Machine à vecteurs de support", "Noyau et γ",
         "Le noyau décide de la forme possible de la frontière. Avec le noyau gaussien, γ règle la portée de chaque point d’apprentissage : trop petit, la frontière reste lisse ; trop grand, elle se fragmente.",
         {{"linéaire", "une droite", "lin", 0}, {"γ 1", "très large", "gamma", 1}, {"γ 10", "large", "gamma", 10}, {"γ 30", "", "gamma", 30},
          {"γ 100", "", "gamma", 100}, {"γ 300", "", "gamma", 300}, {"γ 1000", "courte", "gamma", 1000}, {"γ 3000", "très courte", "gamma", 3000}}, 3},
        {"kppv", "k-PPV", "k plus proches voisins", "Nombre de voisins k",
         "Chaque point prend la classe majoritaire de ses k voisins d’apprentissage les plus proches. Peu de voisins : décision très locale ; beaucoup : décision lissée.",
         {{"k = 1", "très locale", "k", 1}, {"k = 3", "", "k", 3}, {"k = 5", "équilibre", "k", 5}, {"k = 9", "", "k", 9},
          {"k = 15", "", "k", 15}, {"k = 25", "lissée", "k", 25}, {"k = 51", "", "k", 51}, {"k = 101", "vote très large", "k", 101}}, 2},
        {"foret_aleatoire", "Forêt", "Forêt aléatoire", "Profondeur des arbres",
         "Chaque niveau ajoute une question « attribut ≤ seuil ? ». 50 arbres, chacun appris sur un tirage des points, votent.",
         {{"profondeur 1", "", "profondeur_max", 1}, {"profondeur 2", "4 rectangles au plus", "profondeur_max", 2}, {"profondeur 4", "16 rectangles", "profondeur_max", 4},
          {"profondeur 8", "détails fins", "profondeur_max", 8}, {"profondeur 12", "très détaillé", "profondeur_max", 12}, {"profondeur 20", "", "profondeur_max", 20}}, 3},
        {"bayes_normal", "Bayes", "Bayes normal", "Aucun réglage",
         "Moyennes et covariances de chaque classe sont estimées sur l’apprentissage ; chaque point va à la classe la plus probable.",
         {{"Bayes normal", "une gaussienne par classe", "", 0}}, 0},
        {"mlp", "MLP", "Réseau de neurones", "Nombre d’itérations",
         "Un réseau à une couche cachée de 10 neurones (sigmoïde symétrique, RPROP). Les itérations ajustent les poids sur l’apprentissage.",
         {{"10", "", "iterations_max", 10}, {"30", "", "iterations_max", 30}, {"100", "", "iterations_max", 100}, {"300", "", "iterations_max", 300},
          {"1000", "", "iterations_max", 1000}, {"3000", "", "iterations_max", 3000}}, 3},
        {"boost", "Boost", "Boosting", "Nombre de classifieurs faibles",
         "Des arbres d’une seule question sont ajoutés un à un ; chacun insiste sur les points que les précédents classaient mal (GentleBoost).",
         {{"1", "", "nb_faibles", 1}, {"5", "", "nb_faibles", 5}, {"10", "", "nb_faibles", 10}, {"25", "", "nb_faibles", 25},
          {"50", "", "nb_faibles", 50}, {"100", "", "nb_faibles", 100}, {"200", "", "nb_faibles", 200}, {"500", "", "nb_faibles", 500}}, 4},
        {"em", "EM", "EM (non supervisé)", "Nombre de clusters K",
         "EM regroupe les points en K gaussiennes sans regarder leurs classes ; chaque cluster est ensuite associé à la classe majoritaire de ses points d’apprentissage.",
         {{"K = 2", "", "k", 2}, {"K = 3", "", "k", 3}, {"K = 4", "", "k", 4}, {"K = 5", "", "k", 5}, {"K = 6", "", "k", 6},
          {"K = 8", "", "k", 8}, {"K = 10", "", "k", 10}, {"K = 15", "", "k", 15}}, 1},
    };
    return f;
}


std::string Pct(double v) {
    char b[32];
    std::snprintf(b, sizeof b, "%.1f %%", v * 100.0);
    std::string s(b);
    for (auto& c : s) if (c == '.') c = ',';
    return s;
}
std::string Entier(long long v) {
    std::string s = std::to_string(v), r;
    for (size_t i = 0; i < s.size(); ++i) {
        if (i && (s.size() - i) % 3 == 0) r += " ";   // espace fine insecable des milliers
        r += s[i];
    }
    return r;
}
std::string Nombre(double v) {
    char b[32];
    std::snprintf(b, sizeof b, "%g", v);
    std::string s(b);
    for (auto& c : s) if (c == '.') c = ',';
    return s;
}

std::optional<fs::path> CheminJeu(int i) {
    const auto racine = app::plateforme::DossierExemples();
    if (!racine || i < 0 || i >= NB_JEUX) return std::nullopt;
    return *racine / decid::CheminDepuisUtf8(CATALOGUE[i].m_fichier);
}

slint::SharedString S(const std::string& s) { return slint::SharedString(s); }



} // namespace

std::string SourceEtude::Protocole() const {
    char b[96];
    std::snprintf(b, sizeof b, "%g", m_ratio * 100.0);
    std::string r(b);
    for (auto& c : r) if (c == '.') c = ',';
    return r + " % " + (m_methode == "aleatoire_stratifie" ? "au hasard" : "premiers de chaque classe") + ", graine " + std::to_string(m_graine)
        + (m_normalisation == 1 ? ", bornes .lim" : m_normalisation == 2 ? ", sans mise à l’échelle" : "");
}
std::string SourceEtude::CleProtocole() const {
    return m_cle + "|" + m_methode + "|" + std::to_string(m_ratio) + "|" + std::to_string(m_graine) + "|" + std::to_string(m_normalisation)
        + "|" + decid::CheminUtf8(m_lim);
}

Etude::Etude(Fenetre& fenetre, Moteur& moteur) : m_fenetre(fenetre), m_moteur(moteur) {
    auto& g = m_fenetre.global<::Etude>();
    m_fenetre.set_experiences(m_modeleExperiences);
    m_fenetre.set_menu_experiences(m_menuExperiences);
    g.set_titres_experiences(m_titres);

    std::vector<Famille> fam;
    for (const auto& f : Familles()) fam.push_back({S(f.m_court), S(f.m_nom), S("moteur Decid")});
    g.set_familles(std::make_shared<slint::VectorModel<Famille>>(fam));

    g.on_choisir_jeu([this](int i) { ChoisirJeu(i); });
    g.on_choisir_partage([this](int i) { ChoisirPartage(i); });
    g.on_choisir_famille([this](int i) { ChoisirFamille(i); });
    g.on_choisir_valeur([this](int i) { m_fenetre.global<::Etude>().set_valeur(i); });
    g.on_axes_changes([this] { PeindreApercu(); });
    g.on_entrainer([this] { Entrainer(); });
    g.on_annuler_calcul([this] {
        ++*m_generationEffets;      // une serie d'effets s'arrete a l'etape suivante
        m_moteur.AnnulerTout();
        Liberer("Calcul annulé.");
    });
    g.on_voir_effets([this] { VoirEffets(); });
    g.set_vignettes(m_vignettes);
    g.set_vignettes_scores(m_scoresVignettes);
    g.on_choisir_experience([this](int k) { OuvrirExperience(k); });
    g.on_vue_plan_changee([this] { PeindrePlan(); });
    g.on_regler_en_direct([this](int v) {
        if (m_occupe || m_courante < 0) return;
        ExperienceEtude x = m_experiences[m_courante];
        const auto& f = Familles()[x.m_famille];
        x.m_valeur = std::clamp(v, 0, (int)f.m_valeurs.size() - 1);
        x.m_titre = std::string(f.m_court) + " · " + f.m_valeurs[x.m_valeur].m_libelle;
        x.m_image.clear(); x.m_volume.clear();
        Lancer(std::move(x), m_courante);
    });
    g.on_tourner_3d([this](float dx, float dy) { m_vue3d.Tourner(dx, dy); Peindre3D(); });
    g.on_zoomer_3d([this](float crans) { m_vue3d.Zoomer(-crans); Peindre3D(); });
    g.on_mode_3d_change([this] { m_vue3d.FixerDonneesSeules(m_fenetre.global<::Etude>().get_donnees_seules()); Peindre3D(); });
    g.on_changer([this](slint::SharedString quoi) { m_fenetre.set_etape(quoi == "modele" ? "modele" : "donnees"); });
    g.on_taille_apercu([this](float l, float h) {
        if (std::abs(l - m_apercuL) < 1 && std::abs(h - m_apercuH) < 1) return;
        const bool premier = m_apercuL < 1;
        m_apercuL = l; m_apercuH = h;
        if (premier) PeindreApercu();
        else m_redim.start(slint::TimerMode::SingleShot, std::chrono::milliseconds(80), [this] { PeindreApercu(); });
    });
    g.on_taille_plan([this](float l, float h) {
        if (std::abs(l - m_planL) < 1 && std::abs(h - m_planH) < 1) return;
        const bool premier = m_planL < 1;
        m_planL = l; m_planH = h;
        if (premier) PeindrePlan();
        else m_redim.start(slint::TimerMode::SingleShot, std::chrono::milliseconds(80), [this] { PeindrePlan(); });
    });
    g.on_imp_choisir([this] { ImpChoisir(); });
    g.on_imp_choisir_lim([this] { ImpChoisirLim(); });
    g.on_imp_relire([this] { ImpRelire(); });
    g.on_imp_utiliser([this] { ImpUtiliser(); });

    RemplirBibliotheque();
    ChoisirJeu(1);           // la spirale par defaut
    ChoisirFamille(0);
    MajExperiences();
}

// ================================================================ Donnees
void Etude::RemplirBibliotheque() {
    std::vector<JeuBiblio> v;
    for (int i = 0; i < NB_JEUX; ++i) v.push_back({S(CATALOGUE[i].m_nom), S(CATALOGUE[i].m_desc), S("")});
    m_fenetre.global<::Etude>().set_bibliotheque(std::make_shared<slint::VectorModel<JeuBiblio>>(v));
}

void Etude::ChoisirJeu(int i) {
    auto& g = m_fenetre.global<::Etude>();
    if (i < 0 || i >= NB_JEUX) return;
    const auto chemin = CheminJeu(i);
    if (!chemin) {
        g.set_donnees_erreur("Bibliothèque databasesCSV introuvable (variable DECID_EXEMPLES ou dossier voisin de l’application).");
        return;
    }
    SourceEtude s;
    s.m_nom = CATALOGUE[i].m_nom;
    s.m_cle = CATALOGUE[i].m_cle;
    s.m_chemin = *chemin;
    s.m_contrat.m_separateur = ',';
    s.m_contrat.m_enTete = CATALOGUE[i].m_enTete ? decid::ModeEnTete::Oui : decid::ModeEnTete::Non;
    s.m_contrat.m_colonneCible = 0;
    const int p = g.get_partage();
    s.m_methode = p == 3 ? "premiers_n_par_classe" : "aleatoire_stratifie";
    s.m_ratio = p == 1 ? 0.25 : p == 2 ? 0.75 : 0.5;
    auto lu = decid::LireCsv(s.m_chemin, s.m_contrat);
    if (!lu.Ok()) {
        g.set_jeu(i);
        g.set_donnees_erreur(S(lu.Err().Texte()));
        return;
    }
    g.set_jeu(i);
    Adopter(std::move(s), std::move(lu.Valeur()));
    g.set_jeu_desc(S(CATALOGUE[i].m_desc));
}

std::optional<SourceEtude> Etude::SourceBibliotheque(int jeu, int partage) const {
    const auto chemin = CheminJeu(jeu);
    if (!chemin) return std::nullopt;
    SourceEtude s;
    s.m_nom = CATALOGUE[jeu].m_nom;
    s.m_cle = CATALOGUE[jeu].m_cle;
    s.m_chemin = *chemin;
    s.m_contrat.m_separateur = ',';
    s.m_contrat.m_enTete = CATALOGUE[jeu].m_enTete ? decid::ModeEnTete::Oui : decid::ModeEnTete::Non;
    s.m_contrat.m_colonneCible = 0;
    s.m_methode = partage == 3 ? "premiers_n_par_classe" : "aleatoire_stratifie";
    s.m_ratio = partage == 1 ? 0.25 : partage == 2 ? 0.75 : 0.5;
    return s;
}

std::vector<std::string> Etude::NomsBibliotheque() const {
    std::vector<std::string> v;
    for (int i = 0; i < NB_JEUX; ++i) v.push_back(CATALOGUE[i].m_nom);
    return v;
}
std::string Etude::CleBibliotheque(int jeu) const { return jeu >= 0 && jeu < NB_JEUX ? CATALOGUE[jeu].m_cle : std::string(); }
std::vector<std::pair<std::string, std::string>> Etude::ExperiencesEntrainees() const {
    std::vector<std::pair<std::string, std::string>> v;
    for (const auto& x : m_experiences)
        if (x.m_module.empty() && !x.m_absente && !x.m_tache.empty()) v.push_back({x.m_titre + " · " + x.m_jeuNom + " · " + x.m_score, x.m_tache});
    return v;
}
bool Etude::ContientTache(const std::string& tache) const {
    for (const auto& x : m_experiences) if (x.m_tache == tache) return true;
    return false;
}
void Etude::AjouterExperienceModule(const std::string& module, const std::string& tache, const std::string& titre, const std::string& jeuNom,
                                    const std::string& protocoleNom, double score, const std::string& cleDonnees, const std::string& cleProtocole) {
    if (ContientTache(tache)) return;
    ExperienceEtude x;
    x.m_module = module; x.m_tache = tache; x.m_titre = titre; x.m_jeuNom = jeuNom; x.m_partageNom = protocoleNom;
    x.m_score = score >= 0 ? Pct(score) : "—";
    x.m_cleDonnees = cleDonnees; x.m_cleProtocole = cleProtocole;
    m_experiences.push_back(std::move(x));
    m_courante = (int)m_experiences.size() - 1;
    MajExperiences();
}

void Etude::OuvrirConfiguration(int jeu, int partage, int famille, int valeur) {
    auto& g = m_fenetre.global<::Etude>();
    g.set_partage(partage);
    ChoisirJeu(jeu);
    ChoisirPartage(partage);
    ChoisirFamille(famille);
    g.set_valeur(valeur);
    m_fenetre.set_ecran("etude");
    m_fenetre.set_vue("etape");
    m_fenetre.set_etape("modele");
}

void Etude::ChoisirPartage(int i) {
    auto& g = m_fenetre.global<::Etude>();
    g.set_partage(i);
    m_source.m_methode = i == 3 ? "premiers_n_par_classe" : "aleatoire_stratifie";
    m_source.m_ratio = i == 1 ? 0.25 : i == 2 ? 0.75 : 0.5;
    m_source.m_graine = 42;
    g.set_protocole(S(m_source.Protocole()));
}

void Etude::Adopter(SourceEtude source, decid::JeuDeDonnees d) {
    auto& g = m_fenetre.global<::Etude>();
    ++*m_generationEffets;
    g.set_effets_calcules(false);
    m_source = std::move(source);
    m_donnees = std::move(d);
    g.set_donnees_erreur("");
    g.set_jeu_nom(S(m_source.m_nom));
    g.set_jeu_desc(S(m_source.m_cmc ? "Projet de l’ancien Decid (.cmc)." : "Fichier importé."));
    g.set_protocole(S(m_source.Protocole()));

    std::vector<long long> comptes(m_donnees->m_nomsClasses.size(), 0);
    for (int l : m_donnees->m_labels) if (l >= 0 && l < (int)comptes.size()) ++comptes[l];
    std::string eff;
    for (size_t k = 0; k < comptes.size() && k < 6; ++k) eff += (k ? " · " : "") + Entier(comptes[k]);
    if (comptes.size() > 6) eff += " …";
    std::vector<Fait> faits = {
        {"Fichier", S(decid::CheminUtf8(m_source.m_chemin.filename()))},
        {"Première ligne", m_source.m_cmc ? "—" : m_source.m_contrat.m_enTete == decid::ModeEnTete::Oui ? "en-tête" : m_source.m_contrat.m_enTete == decid::ModeEnTete::Auto ? "auto" : "observation"},
        {"Lignes", S(Entier(m_donnees->m_nbLignes))},
        {"Attributs", S(std::to_string(m_donnees->m_nbAttributs))},
        {"Classes", S(std::to_string(comptes.size()))},
        {"Effectifs", S(eff)}};
    g.set_jeu_faits(std::make_shared<slint::VectorModel<Fait>>(faits));
    std::vector<slint::SharedString> attrs;
    for (const auto& a : m_donnees->m_nomsAttributs) attrs.push_back(S(a));
    g.set_attributs(std::make_shared<slint::VectorModel<slint::SharedString>>(attrs));
    g.set_axe_x(0);
    g.set_axe_y(m_donnees->m_nbAttributs >= 2 ? 1 : 0);
    PeindreApercu();
}

void Etude::PeindreApercu() {
    auto& g = m_fenetre.global<::Etude>();
    if (!m_donnees || m_apercuL < 8 || m_apercuH < 8) return;
    const auto& d = *m_donnees;
    const int ax = std::clamp(g.get_axe_x(), 0, std::max(0, d.m_nbAttributs - 1)), ay = std::clamp(g.get_axe_y(), 0, std::max(0, d.m_nbAttributs - 1));
    std::vector<PointPlan> pts;
    if (d.m_nbAttributs >= 2)
        for (int l = 0; l < d.m_nbLignes; ++l)
            pts.push_back({d.m_valeurs[(size_t)l * d.m_nbAttributs + ax], d.m_valeurs[(size_t)l * d.m_nbAttributs + ay], d.m_labels[l], false});
    g.set_apercu_legende(d.m_nbAttributs >= 2
        ? S(d.m_nomsAttributs[ax] + " → · " + d.m_nomsAttributs[ay] + " ↑ · valeurs du fichier, avant mise à l’échelle")
        : S("Un seul attribut : pas de plan."));
    const float echelle = m_fenetre.window().scale_factor();
    const int L = (int)std::lround(m_apercuL * echelle), H = (int)std::lround(m_apercuH * echelle);
    const float rayon = std::clamp(2.6f * echelle * std::sqrt(4000.0f / std::max<size_t>(pts.size(), 400)), 1.6f * echelle, 4.0f * echelle);
    g.set_apercu(RendrePlan(L, H, m_fenetre.global<Theme>().get_sombre(), DomaineDesPoints(pts), nullptr, pts, rayon));
}

// ---------- importer ----------
void Etude::ImpChoisir() {
    auto& g = m_fenetre.global<::Etude>();
    const bool cmc = g.get_imp_mode() == 1;
    const auto chemin = cmc ? app::plateforme::ChoisirFichier(L"Projet de l’ancien Decid", {{L"Projet Decid (.cmc)", L"*.cmc"}})
                            : app::plateforme::ChoisirFichier(L"Fichier de données", {{L"Fichiers CSV", L"*.csv;*.txt"}, {L"Tous les fichiers", L"*.*"}});
    if (!chemin) return;
    m_import = SourceEtude{};
    m_import.m_chemin = *chemin;
    m_import.m_cmc = cmc;
    m_import.m_nom = decid::CheminUtf8(chemin->stem());
    m_import.m_cle = "fichier:" + decid::CheminUtf8(*chemin);
    g.set_imp_fichier(S(decid::CheminUtf8(chemin->filename())));
    ImpRelire();
}

void Etude::ImporterFichier(const fs::path& chemin, const fs::path& lim, bool utiliser) {
    auto& g = m_fenetre.global<::Etude>();
    const bool cmc = chemin.extension() == ".cmc";
    g.set_imp_mode(cmc ? 1 : 0);
    m_import = SourceEtude{};
    m_import.m_chemin = chemin;
    m_import.m_cmc = cmc;
    m_import.m_nom = decid::CheminUtf8(chemin.stem());
    m_import.m_cle = "fichier:" + decid::CheminUtf8(chemin);
    g.set_imp_fichier(S(decid::CheminUtf8(chemin.filename())));
    if (!lim.empty()) {
        m_import.m_lim = lim;
        g.set_imp_lim(S(decid::CheminUtf8(lim.filename())));
        g.set_imp_norm(1);
    }
    ImpRelire();
    if (utiliser) ImpUtiliser();
}

void Etude::ImpChoisirLim() {
    const auto chemin = app::plateforme::ChoisirFichier(L"Bornes de mise à l’échelle", {{L"Fichier de bornes (.lim)", L"*.lim"}});
    if (!chemin) return;
    m_import.m_lim = *chemin;
    m_fenetre.global<::Etude>().set_imp_lim(S(decid::CheminUtf8(chemin->filename())));
}

void Etude::ImpRelire() {
    auto& g = m_fenetre.global<::Etude>();
    if (m_import.m_chemin.empty()) return;
    m_importLu.reset();
    g.set_imp_lu(false);
    g.set_imp_erreur("");

    // lignes brutes : les 20 premieres, telles quelles
    std::vector<slint::SharedString> brutes;
    if (auto texte = decid::LireFichierTexte(m_import.m_chemin); texte.Ok()) {
        size_t debut = 0;
        const std::string& t = texte.Valeur();
        while (debut < t.size() && brutes.size() < 20) {
            size_t fin = t.find('\n', debut);
            if (fin == std::string::npos) fin = t.size();
            std::string l = t.substr(debut, fin - debut);
            if (!l.empty() && l.back() == '\r') l.pop_back();
            brutes.push_back(S(l.size() > 400 ? l.substr(0, 400) + " …" : l));
            debut = fin + 1;
        }
    }
    g.set_imp_lignes(std::make_shared<slint::VectorModel<slint::SharedString>>(brutes));

    decid::Resultat<decid::JeuDeDonnees> lu = decid::Erreur{};
    if (m_import.m_cmc) {
        // .dat deplaces : chaque dossier cite par le .cmc et absent est cherche a cote du .cmc
        m_import.m_relocalisations.clear();
        const std::string dossierCmc = decid::CheminUtf8(m_import.m_chemin.parent_path());
        for (const auto& l : brutes) {
            std::string ligne(l.data(), l.size());
            while (!ligne.empty() && (ligne.back() == ' ' || ligne.back() == '\t')) ligne.pop_back();
            if (ligne.size() < 4 || ligne.substr(ligne.size() - 4) != ".dat") continue;
            const size_t sep = ligne.find_last_of("\\/");
            if (sep == std::string::npos) continue;
            const std::string prefixe = ligne.substr(0, sep);
            std::error_code ec;
            if (fs::exists(decid::CheminDepuisUtf8(ligne), ec)) continue;
            bool connu = false;
            for (const auto& r : m_import.m_relocalisations) connu = connu || r.m_prefixeAncien == prefixe;
            if (!connu) m_import.m_relocalisations.push_back({prefixe, dossierCmc});
        }
        lu = decid::LireCmc(m_import.m_chemin, m_import.m_relocalisations);
    } else {
        const int en = g.get_imp_en_tete(), sep = g.get_imp_separateur();
        m_import.m_contrat.m_enTete = en == 1 ? decid::ModeEnTete::Oui : en == 2 ? decid::ModeEnTete::Auto : decid::ModeEnTete::Non;
        m_import.m_contrat.m_separateur = sep == 1 ? ';' : sep == 2 ? '\t' : sep == 3 ? ' ' : ',';
        m_import.m_contrat.m_colonneCible = std::max(0, std::atoi(std::string(g.get_imp_cible()).c_str()) - 1);
        lu = decid::LireCsv(m_import.m_chemin, m_import.m_contrat);
    }
    if (!lu.Ok()) {
        g.set_imp_erreur(S(lu.Err().Texte()));
        g.set_imp_resume(std::make_shared<slint::VectorModel<Fait>>());
        return;
    }
    const auto& d = lu.Valeur();
    std::vector<long long> comptes(d.m_nomsClasses.size(), 0);
    for (int l : d.m_labels) if (l >= 0 && l < (int)comptes.size()) ++comptes[l];
    std::string classes;
    for (size_t k = 0; k < d.m_nomsClasses.size() && k < 8; ++k) classes += (k ? " · " : "") + d.m_nomsClasses[k] + " (" + Entier(comptes[k]) + ")";
    if (d.m_nomsClasses.size() > 8) classes += " …";
    std::vector<Fait> resume = {{"Lignes", S(Entier(d.m_nbLignes))}, {"Attributs", S(std::to_string(d.m_nbAttributs))},
                                {"Classes", S(classes)}, {"Contrat", S(d.m_contrat)}};
    if (!d.m_avertissements.empty()) resume.push_back({"Avertissement", S(d.m_avertissements.front())});
    g.set_imp_resume(std::make_shared<slint::VectorModel<Fait>>(resume));
    m_importLu = std::move(lu.Valeur());
    g.set_imp_lu(true);
}

void Etude::ImpUtiliser() {
    auto& g = m_fenetre.global<::Etude>();
    if (!m_importLu) return;
    if (g.get_imp_norm() == 1 && m_import.m_lim.empty()) { g.set_imp_erreur("Choisissez le fichier .lim, ou une autre mise à l’échelle."); return; }
    SourceEtude s = m_import;
    s.m_normalisation = g.get_imp_norm();
    const int r = std::clamp(g.get_imp_ratio(), 0, 6);
    s.m_ratio = (r + 1) / 8.0;
    s.m_methode = g.get_imp_methode() == 1 ? "premiers_n_par_classe" : "aleatoire_stratifie";
    s.m_graine = std::max(0LL, std::atoll(std::string(g.get_imp_graine()).c_str()));
    g.set_jeu(-1);
    Adopter(std::move(s), *m_importLu);
}

// ================================================================ Modele
void Etude::ChoisirFamille(int i) {
    const auto& F = Familles();
    if (i < 0 || i >= (int)F.size()) return;
    auto& g = m_fenetre.global<::Etude>();
    g.set_famille(i);
    g.set_reglage_nom(S(F[i].m_reglage));
    g.set_reglage_aide(S(F[i].m_aide));
    std::vector<ValeurReglage> v;
    for (const auto& x : F[i].m_valeurs) v.push_back({S(x.m_libelle), S(x.m_detail)});
    g.set_valeurs(std::make_shared<slint::VectorModel<ValeurReglage>>(v));
    g.set_valeur(F[i].m_defaut);
    ++*m_generationEffets;
    g.set_effets_calcules(false);
}

// Effet de chaque valeur : pour chaque valeur du reglage, une experience et une petite image de decision, l'une apres
// l'autre (le worker n'execute qu'une tache lourde a la fois). Les vignettes apparaissent au fil du calcul.
void Etude::VoirEffets() {
    auto& g = m_fenetre.global<::Etude>();
    if (m_occupe || !m_donnees) return;
    const int famille = g.get_famille();
    const auto& f = Familles()[famille];
    m_vignettes->clear();
    m_scoresVignettes->clear();
    for (size_t k = 0; k < f.m_valeurs.size(); ++k) { m_vignettes->push_back(slint::Image()); m_scoresVignettes->push_back("…"); }
    g.set_effets_calcules(true);
    const int na = m_donnees->m_nbAttributs;
    const int ax = std::clamp(g.get_axe_x(), 0, std::max(0, na - 1)), ay = std::clamp(g.get_axe_y(), 0, std::max(0, na - 1));
    auto generation = std::make_shared<int>(++*m_generationEffets);
    EffetSuivant(famille, 0, m_source, ax, ay, generation);
}

void Etude::EffetSuivant(int famille, int k, SourceEtude source, int axeX, int axeY, std::shared_ptr<int> generation) {
    const auto& f = Familles()[famille];
    if (*generation != *m_generationEffets) return;   // famille ou jeu change entre-temps
    if (k >= (int)f.m_valeurs.size()) { Liberer(); return; }
    Occuper("effet de " + std::string(f.m_valeurs[k].m_libelle) + " (" + std::to_string(k + 1) + "/" + std::to_string(f.m_valeurs.size()) + ")");
    m_moteur.Executer(RequeteExperience(source, famille, k), [this, famille, k, source, axeX, axeY, generation](const FinCalcul& fin) {
        if (*generation != *m_generationEffets) { Liberer(std::string(m_fenetre.global<::Etude>().get_erreur())); return; }   // garde « Calcul annulé. »
        if (!fin.m_ok) { m_scoresVignettes->set_row_data(k, "refusé"); EffetSuivant(famille, k + 1, source, axeX, axeY, generation); return; }
        auto ev = LireEvaluation(fin.m_resultat);
        m_scoresVignettes->set_row_data(k, S(ev.Ok() && ev.Valeur().m_definie ? Pct(ev.Valeur().m_exactitude) : "—"));
        decid::ValeurJson im = decid::ValeurJson::Objet();
        im["operation"] = "image";
        im["experience"] = Moteur::RefTache(fin.m_id);
        im["largeur"] = 96; im["hauteur"] = 96;
        im["attribut_x"] = axeX; im["attribut_y"] = axeY;
        im["domaine"] = "auto";
        im["valeurs_fixes"] = "moyenne_apprentissage";
        m_moteur.Executer(std::move(im), [this, famille, k, source, axeX, axeY, generation](const FinCalcul& fi) {
            if (*generation != *m_generationEffets) { Liberer(std::string(m_fenetre.global<::Etude>().get_erreur())); return; }   // garde « Calcul annulé. »
            if (fi.m_ok)
                if (auto image = LireImage(fi.m_resultat); image.Ok()) {
                    const auto& m = image.Valeur();
                    const float e = m_fenetre.window().scale_factor();
                    const int t = (int)std::lround(88 * e);
                    m_vignettes->set_row_data(k, RendrePlan(t, t, m_fenetre.global<Theme>().get_sombre(), {m.m_xMin, m.m_xMax, m.m_yMin, m.m_yMax}, &m, {}, 1.0f, nullptr, 0.6f));
                }
            EffetSuivant(famille, k + 1, source, axeX, axeY, generation);
        });
    });
}

decid::ValeurJson Etude::RequeteExperience(const SourceEtude& src, int famille, int valeur) const {
    const auto& f = Familles()[std::clamp(famille, 0, (int)Familles().size() - 1)];
    const auto& v = f.m_valeurs[std::clamp(valeur, 0, (int)f.m_valeurs.size() - 1)];

    decid::ValeurJson r = decid::ValeurJson::Objet();
    decid::ValeurJson source = decid::ValeurJson::Objet();
    source["chemin"] = decid::CheminUtf8(src.m_chemin);
    if (src.m_cmc) {
        source["type"] = "cmc";
        decid::ValeurJson rel = decid::ValeurJson::Tableau();
        for (const auto& x : src.m_relocalisations) {
            decid::ValeurJson o = decid::ValeurJson::Objet();
            o["ancien"] = x.m_prefixeAncien;
            o["nouveau"] = x.m_dossierNouveau;
            rel.Ajouter(o);
        }
        source["relocalisations"] = rel;
    } else {
        source["type"] = "csv";
        source["en_tete"] = src.m_contrat.m_enTete == decid::ModeEnTete::Oui ? "oui" : src.m_contrat.m_enTete == decid::ModeEnTete::Auto ? "auto" : "non";
        source["separateur"] = std::string(1, src.m_contrat.m_separateur);
        source["colonne_cible"] = src.m_contrat.m_colonneCible;
    }
    r["source"] = source;
    r["threads"] = std::clamp(m_fenetre.global<Preferences>().get_threads(), 1, 8);

    decid::ValeurJson a = decid::ValeurJson::Objet();
    a["famille"] = f.m_id;
    const std::string id = f.m_id;
    if (id == "svm") {
        a["noyau"] = std::string(v.m_cle) == "lin" ? "LINEAIRE" : "RBF";
        if (std::string(v.m_cle) != "lin") a["gamma"] = v.m_nombre;
        a["c"] = 10;
    } else if (id == "foret_aleatoire") {
        a["profondeur_max"] = (int)v.m_nombre; a["nb_arbres"] = 50; a["min_echantillons"] = 2;
    } else if (id == "mlp") {
        decid::ValeurJson couches = decid::ValeurJson::Tableau(); couches.Ajouter(10);
        a["couches_cachees"] = couches; a["activation"] = "SIGMOIDE_SYM"; a["methode"] = "RPROP"; a["epsilon"] = 1e-6;
        a["iterations_max"] = (int)v.m_nombre;
    } else if (id == "boost") {
        a["nb_faibles"] = (int)v.m_nombre; a["profondeur"] = 1; a["type"] = "GENTLE";
    } else if (id == "em") {
        a["k"] = (int)v.m_nombre; a["covariance"] = "DIAGONALE";
    } else if (id == "kppv") {
        a["k"] = (int)v.m_nombre;
    }
    r["algorithme"] = a;

    decid::ValeurJson p = decid::ValeurJson::Objet();
    p["methode"] = src.m_methode;
    p["ratio"] = src.m_ratio;
    p["graine"] = src.m_graine;
    r["partage"] = p;
    decid::ValeurJson n = decid::ValeurJson::Objet();
    if (src.m_normalisation == 2) n["methode"] = "aucune";
    else {
        n["methode"] = src.m_normalisation == 1 ? "minmax_lim" : "minmax";
        if (src.m_normalisation == 1) n["fichier"] = decid::CheminUtf8(src.m_lim);
        n["bas"] = 0; n["haut"] = 1;
    }
    r["normalisation"] = n;
    return r;
}

void Etude::Occuper(const std::string& phase) {
    m_occupe = true;
    auto& g = m_fenetre.global<::Etude>();
    g.set_occupe(true);
    g.set_erreur("");
    g.set_phase(S(phase));
}
void Etude::Liberer(const std::string& erreur) {
    m_occupe = false;
    auto& g = m_fenetre.global<::Etude>();
    g.set_occupe(false);
    g.set_erreur(S(erreur));
}

void Etude::Entrainer() {
    if (m_occupe) return;
    auto& g = m_fenetre.global<::Etude>();
    if (!m_donnees) { Liberer("Choisissez d’abord un jeu de données lisible."); return; }
    ExperienceEtude x;
    x.m_famille = g.get_famille();
    const auto& f = Familles()[x.m_famille];
    x.m_valeur = std::clamp(g.get_valeur(), 0, (int)f.m_valeurs.size() - 1);
    x.m_source = m_source;
    x.m_titre = std::string(f.m_court) + " · " + f.m_valeurs[x.m_valeur].m_libelle;
    x.m_jeuNom = m_source.m_nom;
    x.m_partageNom = m_source.Protocole();
    x.m_cleDonnees = m_source.m_cle;
    x.m_cleProtocole = m_source.CleProtocole();
    const int na = m_donnees->m_nbAttributs;
    x.m_axeX = std::clamp(g.get_axe_x(), 0, std::max(0, na - 1));
    x.m_axeY = std::clamp(g.get_axe_y(), 0, std::max(0, na - 1));
    x.m_axeZ = 0;
    while (x.m_axeZ < na && (x.m_axeZ == x.m_axeX || x.m_axeZ == x.m_axeY)) ++x.m_axeZ;
    Lancer(std::move(x), -1);
}

void Etude::Lancer(ExperienceEtude x, int remplacer) {
    const int na = m_donnees ? m_donnees->m_nbAttributs : 0;
    const bool plan = na >= 2 && x.m_axeX != x.m_axeY;
    const bool volume = na >= 3 && x.m_axeZ < na;
    Occuper("apprentissage");
    m_moteur.Executer(RequeteExperience(x.m_source, x.m_famille, x.m_valeur), [this, x, plan, volume, remplacer](const FinCalcul& fin) mutable {
        if (!fin.m_ok) { Liberer("Le moteur a refusé l’apprentissage : " + fin.m_erreur); return; }
        x.m_tache = fin.m_id;
        if (!plan) { Terminer(std::move(x), remplacer); return; }
        Occuper("image de décision");
        decid::ValeurJson im = decid::ValeurJson::Objet();
        im["operation"] = "image";
        im["experience"] = Moteur::RefTache(fin.m_id);
        im["largeur"] = 200; im["hauteur"] = 200;
        im["attribut_x"] = x.m_axeX; im["attribut_y"] = x.m_axeY;
        im["domaine"] = "auto";
        im["valeurs_fixes"] = "moyenne_apprentissage";
        m_moteur.Executer(std::move(im), [this, x, volume, remplacer](const FinCalcul& fi) mutable {
            if (!fi.m_ok) { Liberer("Le moteur a refusé l’image de décision : " + fi.m_erreur); return; }
            x.m_image = fi.m_id;
            if (!volume) { Terminer(std::move(x), remplacer); return; }
            Occuper("volume de décision");
            decid::ValeurJson vo = decid::ValeurJson::Objet();
            vo["operation"] = "volume";
            vo["experience"] = Moteur::RefTache(x.m_tache);
            vo["resolution"] = std::clamp(m_fenetre.global<Preferences>().get_grille3d(), 20, 120);
            decid::ValeurJson at = decid::ValeurJson::Tableau();
            at.Ajouter(x.m_axeX); at.Ajouter(x.m_axeY); at.Ajouter(x.m_axeZ);
            vo["attributs"] = at;
            vo["domaine"] = "auto";
            vo["valeurs_fixes"] = "moyenne_apprentissage";
            m_moteur.Executer(std::move(vo), [this, x, remplacer](const FinCalcul& fv) mutable {
                if (fv.m_ok) x.m_volume = fv.m_id;   // sans volume, l'experience reste valable en 2D
                Terminer(std::move(x), remplacer);
            }, [this](const std::string& phase, long, long) { m_fenetre.global<::Etude>().set_phase(S("volume de décision · " + phase)); });
        }, [this](const std::string& phase, long, long) { m_fenetre.global<::Etude>().set_phase(S("image de décision · " + phase)); });
    }, [this](const std::string& phase, long i, long n) {
        m_fenetre.global<::Etude>().set_phase(S(n > 0 ? phase + " " + std::to_string(i) + "/" + std::to_string(n) : phase));
    });
}

void Etude::Terminer(ExperienceEtude x, int remplacer) {
    auto ev = LireEvaluation(m_moteur.DossierResultat(x.m_tache));
    if (!ev.Ok()) { Liberer("Résultats illisibles : " + ev.Err().Texte()); return; }
    x.m_score = ev.Valeur().m_definie ? Pct(ev.Valeur().m_exactitude) : "—";
    int k = remplacer;
    if (k >= 0 && k < (int)m_experiences.size()) m_experiences[k] = std::move(x);
    else { m_experiences.push_back(std::move(x)); k = (int)m_experiences.size() - 1; }
    Liberer();
    OuvrirExperience(k);
}

// ================================================================ Resultats
void Etude::OuvrirExperience(int k) {
    if (k < 0 || k >= (int)m_experiences.size()) return;
    m_courante = k;
    const auto& x = m_experiences[k];
    if (!x.m_module.empty() && !x.m_absente) {
        // experience d'un outil : elle se lit (et se refait) dans cet outil
        MajExperiences();
        const auto it = m_ouvertures.find(x.m_module);
        m_fenetre.invoke_ouvrir_outil(slint::SharedString(x.m_module));
        if (it != m_ouvertures.end()) it->second(x.m_tache);
        return;
    }
    if (x.m_absente) {
        MajExperiences();
        m_fenetre.global<::Etude>().set_resultat_present(false);
        Liberer("« " + x.m_titre + " » : les résultats calculés ne sont pas dans le dossier de travail de cet ordinateur. Refaites l’expérience (Changer de modèle, puis Entraîner).");
        m_fenetre.set_ecran("etude"); m_fenetre.set_vue("etape"); m_fenetre.set_etape("resultats");
        return;
    }
    auto it = m_cache.find(x.m_tache);
    if (it == m_cache.end()) {
        auto r = std::make_shared<Resultats>();
        const auto dossier = m_moteur.DossierResultat(x.m_tache);
        auto ev = LireEvaluation(dossier);
        auto jeu = LireJeuPublie(dossier);
        if (!ev.Ok() || !jeu.Ok()) { Liberer("Résultats de l’expérience illisibles."); return; }
        r->m_evaluation = std::move(ev.Valeur());
        r->m_jeu = std::move(jeu.Valeur());
        if (!x.m_image.empty()) {
            auto im = LireImage(m_moteur.DossierResultat(x.m_image));
            if (im.Ok()) r->m_image = std::move(im.Valeur());
        }
        if (!x.m_volume.empty())
            if (auto vo = LireVolume(m_moteur.DossierResultat(x.m_volume)); vo.Ok()) r->m_volume = std::move(vo.Valeur());
        // predictions des points de test : « ligne;origine;verite;prediction » (noms de classes)
        r->m_prediction.assign((size_t)r->m_jeu.Lignes(), -1);
        if (auto csv = decid::LireFichierTexte(dossier / "predictions.csv"); csv.Ok()) {
            const std::string& t = csv.Valeur();
            size_t debut = t.find('\n');
            while (debut != std::string::npos && debut + 1 < t.size()) {
                size_t fin = t.find('\n', debut + 1);
                std::string ligne = t.substr(debut + 1, (fin == std::string::npos ? t.size() : fin) - debut - 1);
                if (!ligne.empty() && ligne.back() == '\r') ligne.pop_back();
                const size_t p1 = ligne.find(';'), p3 = ligne.rfind(';');
                if (p1 != std::string::npos && p3 != std::string::npos && p3 > p1) {
                    const int i = std::atoi(ligne.substr(0, p1).c_str());
                    const std::string predite = ligne.substr(p3 + 1);
                    const auto& cl = r->m_jeu.m_classes;
                    const auto trouve = std::find(cl.begin(), cl.end(), predite);
                    if (i >= 0 && i < (int)r->m_prediction.size() && trouve != cl.end()) r->m_prediction[i] = (int)(trouve - cl.begin());
                }
                debut = fin;
            }
        }
        // interieur publie par le moteur : vecteurs de support (SVM), gaussiennes (Bayes)
        if (auto ins = LireJson(dossier / "inspection.json"); ins.Ok()) {
            const auto& j = ins.Valeur();
            r->m_famille = j.Membre("famille").Texte();
            for (const auto& v : j.Membre("vecteurs_support").Elements()) {
                std::vector<double> u;
                for (const auto& c : v.Elements()) u.push_back(c.Nombre());
                r->m_vecteursSupport.push_back(std::move(u));
            }
            for (const auto& c : j.Membre("recalcul_decid").Membre("classes").Elements()) {
                Gaussienne ga;
                ga.m_classe = (int)c.Membre("classe").Entier();
                for (const auto& v : c.Membre("moyenne").Elements()) ga.m_moyenne.push_back(v.Nombre());
                for (const auto& l : c.Membre("covariance").Elements()) {
                    std::vector<double> u;
                    for (const auto& v : l.Elements()) u.push_back(v.Nombre());
                    ga.m_covariance.push_back(std::move(u));
                }
                r->m_gaussiennes.push_back(std::move(ga));
            }
        }
        it = m_cache.emplace(x.m_tache, r).first;
    }
    MajExperiences();
    AfficherResultats();
    m_fenetre.set_ecran("etude");
    m_fenetre.set_vue("etape");
    m_fenetre.set_etape("resultats");
}

void Etude::AfficherResultats() {
    auto& g = m_fenetre.global<::Etude>();
    if (m_courante < 0) { g.set_resultat_present(false); return; }
    const auto& x = m_experiences[m_courante];
    const auto& r = *m_cache.at(x.m_tache);
    g.set_interieur(false);
    g.set_cellule_reel(-1);
    g.set_cellule_predit(-1);
    g.set_vue_3d(false);
    g.set_vue_3d_possible(r.m_volume.has_value());
    {
        const auto& f = Familles()[x.m_famille];
        std::vector<slint::SharedString> vals;
        for (const auto& v : f.m_valeurs) vals.push_back(S(v.m_libelle));
        g.set_direct_nom(S(f.m_reglage));
        g.set_direct_valeurs(std::make_shared<slint::VectorModel<slint::SharedString>>(vals));
        g.set_direct_valeur(x.m_valeur);
    }
    if (r.m_volume) Preparer3D();
    const bool svm = !r.m_vecteursSupport.empty(), bayes = !r.m_gaussiennes.empty();
    g.set_interieur_possible(r.m_image.has_value() && (svm || bayes));
    g.set_interieur_texte(S(svm ? Entier((long long)r.m_vecteursSupport.size()) + " vecteurs de support (anneaux) : eux seuls définissent la frontière. Les points d’apprentissage sont pâles."
        : bayes ? "Une gaussienne par classe, estimée sur l’apprentissage : ellipses à 1 écart-type (trait plein) et 2 écarts-types (trait fin), dans le plan des deux axes."
        : "Ce modèle n’a pas d’intérieur à dessiner dans le plan."));
    const auto& e = r.m_evaluation;
    g.set_resultat_present(true);
    g.set_courante(m_courante);
    g.set_score(S(e.m_definie ? Pct(e.m_exactitude) : "—"));
    long long total = 0, justes = 0;
    for (size_t i = 0; i < e.m_confusion.size(); ++i)
        for (size_t j = 0; j < e.m_confusion[i].size(); ++j) { total += e.m_confusion[i][j]; if (i == j) justes += e.m_confusion[i][j]; }
    g.set_score_detail(S(Entier(justes) + " points de test bien classés sur " + Entier(total) + " · " + x.m_partageNom + " · " + x.m_jeuNom));

    std::vector<slint::SharedString> classes;
    for (const auto& c : e.m_classes) classes.push_back(S(c));
    g.set_confusion_classes(std::make_shared<slint::VectorModel<slint::SharedString>>(classes));
    std::vector<LigneConfusion> lignes;
    for (size_t i = 0; i < e.m_confusion.size() && i < 12; ++i) {
        std::vector<int> cel;
        int t = 0;
        for (long long v : e.m_confusion[i]) { cel.push_back((int)v); t += (int)v; }
        lignes.push_back({S(i < e.m_classes.size() ? e.m_classes[i] : std::to_string(i)), std::make_shared<slint::VectorModel<int>>(cel), t});
    }
    g.set_confusion(std::make_shared<slint::VectorModel<LigneConfusion>>(lignes));

    if (r.m_image) {
        g.set_confiance(S(r.m_image->m_scores.empty()
            ? "Score non défini pour ce modèle : " + r.m_image->m_definitionScore + ". Les régions gardent une teinte uniforme."
            : "Teinte soutenue : décision franche · pâle : près de la frontière. Score du moteur : " + r.m_image->m_definitionScore + "."));
    } else {
        g.set_confiance("Un seul attribut : pas d’image de décision.");
    }
    g.set_temps_decision(S(e.m_tempsDecisionUs >= 0 ? "Temps de décision : " + Nombre(std::round(e.m_tempsDecisionUs * 100) / 100) + " µs par échantillon (mesuré par le moteur)." : ""));
    const auto& a = r.m_jeu.m_attributs;
    g.set_plan_legende(S(a.size() >= 2 ? a[x.m_axeX] + " → · " + a[x.m_axeY] + " ↑ · points du jeu publié par le moteur (après mise à l’échelle)"
        + (a.size() > 2 ? ", autres attributs à leur moyenne d’apprentissage" : "") + " · régions et confiance calculées par le moteur" : ""));
    PeindrePlan();
}

void Etude::PeindrePlan() {
    if (m_courante < 0 || m_planL < 8 || m_planH < 8) return;
    const auto& x = m_experiences[m_courante];
    const auto it = m_cache.find(x.m_tache);
    if (it == m_cache.end()) return;
    const auto& r = *it->second;
    // Frontiere : points de test, erreurs devant ; case de confusion choisie : seuls ses points ressortent.
    // Interieur : points d'apprentissage pales, puis vecteurs de support ou gaussiennes.
    const auto& g = m_fenetre.global<::Etude>();
    const bool interieur = g.get_interieur();
    const int cr = g.get_cellule_reel(), cp = g.get_cellule_predit();
    std::vector<PointPlan> pts;
    const int d = r.m_jeu.Attributs();
    if (d >= 2)
        for (int l = 0; l < r.m_jeu.Lignes(); ++l) {
            PointPlan q{r.m_jeu.m_valeurs[(size_t)l * d + x.m_axeX], r.m_jeu.m_valeurs[(size_t)l * d + x.m_axeY], r.m_jeu.m_labels[l], false};
            const int pred = l < (int)r.m_prediction.size() ? r.m_prediction[l] : -1;
            if (interieur) {
                if (pred >= 0) continue;
                q.m_opacite = 0.3f;
            } else {
                if (pred < 0) continue;
                if (cr >= 0) { const bool dedans = q.m_classe == cr && pred == cp; q.m_opacite = dedans ? 1.0f : 0.08f; q.m_devant = dedans; }
                else if (pred != q.m_classe) { q.m_opacite = 1.0f; q.m_devant = true; }
                else q.m_opacite = 0.6f;
            }
            pts.push_back(q);
        }
    DecorPlan decor;
    if (interieur) {
        for (const auto& v : r.m_vecteursSupport)
            if ((int)v.size() > std::max(x.m_axeX, x.m_axeY)) decor.m_anneaux.push_back({v[x.m_axeX], v[x.m_axeY]});
        for (const auto& ga : r.m_gaussiennes) {
            const int a = x.m_axeX, b = x.m_axeY;
            if ((int)ga.m_moyenne.size() <= std::max(a, b) || (int)ga.m_covariance.size() <= std::max(a, b)) continue;
            decor.m_ellipses.push_back({ga.m_moyenne[a], ga.m_moyenne[b], ga.m_covariance[a][a], ga.m_covariance[a][b], ga.m_covariance[b][b], ga.m_classe});
        }
    }
    DomainePlan dom = DomaineDesPoints(pts);
    if (r.m_image) dom = {r.m_image->m_xMin, r.m_image->m_xMax, r.m_image->m_yMin, r.m_image->m_yMax};
    const float echelle = m_fenetre.window().scale_factor();
    const int L = (int)std::lround(m_planL * echelle), H = (int)std::lround(m_planH * echelle);
    const float rayon = std::clamp(2.6f * echelle * std::sqrt(4000.0f / std::max<size_t>(pts.size(), 400)), 1.6f * echelle, 4.0f * echelle);
    m_fenetre.global<::Etude>().set_plan(RendrePlan(L, H, m_fenetre.global<Theme>().get_sombre(), dom, r.m_image ? &*r.m_image : nullptr, pts, rayon, interieur ? &decor : nullptr));
}

// ================================================================ Projet
namespace {
decid::ValeurJson SourceVersJson(const SourceEtude& x) {
    decid::ValeurJson j = decid::ValeurJson::Objet();
    j["nom"] = x.m_nom; j["cle"] = x.m_cle; j["chemin"] = decid::CheminUtf8(x.m_chemin); j["cmc"] = x.m_cmc;
    j["separateur"] = std::string(1, x.m_contrat.m_separateur);
    j["en_tete"] = x.m_contrat.m_enTete == decid::ModeEnTete::Oui ? "oui" : x.m_contrat.m_enTete == decid::ModeEnTete::Auto ? "auto" : "non";
    j["colonne_cible"] = x.m_contrat.m_colonneCible;
    decid::ValeurJson rel = decid::ValeurJson::Tableau();
    for (const auto& r : x.m_relocalisations) {
        decid::ValeurJson o = decid::ValeurJson::Objet();
        o["ancien"] = r.m_prefixeAncien; o["nouveau"] = r.m_dossierNouveau;
        rel.Ajouter(o);
    }
    j["relocalisations"] = rel;
    j["normalisation"] = x.m_normalisation; j["lim"] = decid::CheminUtf8(x.m_lim);
    j["methode"] = x.m_methode; j["ratio"] = x.m_ratio; j["graine"] = x.m_graine;
    return j;
}
SourceEtude SourceDepuisJson(const decid::ValeurJson& j) {
    SourceEtude x;
    x.m_nom = j.Membre("nom").Texte(); x.m_cle = j.Membre("cle").Texte();
    x.m_chemin = decid::CheminDepuisUtf8(j.Membre("chemin").Texte()); x.m_cmc = j.Membre("cmc").Booleen();
    const std::string sep = j.Membre("separateur").Texte();
    x.m_contrat.m_separateur = sep.size() == 1 ? sep[0] : ',';
    const std::string en = j.Membre("en_tete").Texte();
    x.m_contrat.m_enTete = en == "oui" ? decid::ModeEnTete::Oui : en == "auto" ? decid::ModeEnTete::Auto : decid::ModeEnTete::Non;
    x.m_contrat.m_colonneCible = (int)j.Membre("colonne_cible").Entier();
    for (const auto& r : j.Membre("relocalisations").Elements()) x.m_relocalisations.push_back({r.Membre("ancien").Texte(), r.Membre("nouveau").Texte()});
    x.m_normalisation = std::clamp((int)j.Membre("normalisation").Entier(), 0, 2);
    x.m_lim = decid::CheminDepuisUtf8(j.Membre("lim").Texte());
    x.m_methode = j.Membre("methode").Texte() == "premiers_n_par_classe" ? "premiers_n_par_classe" : "aleatoire_stratifie";
    x.m_ratio = std::clamp(j.Membre("ratio").Nombre(), 0.05, 0.95);
    x.m_graine = j.Membre("graine").Entier();
    return x;
}
// identifiant de tache accepte : celui que le moteur fabrique ([a-z0-9_-]), jamais un chemin
bool IdTacheValide(const std::string& id) {
    if (id.empty() || id.size() > 120) return false;
    for (char c : id) if (!(std::isalnum((unsigned char)c) || c == '-' || c == '_' || c == 'T')) return false;
    return true;
}
} // namespace

decid::ValeurJson Etude::Sauvegarder() const {
    const auto& g = m_fenetre.global<::Etude>();
    decid::ValeurJson j = decid::ValeurJson::Objet();
    j["format"] = "decid-projet-app";
    j["version"] = 1;
    decid::ValeurJson e = decid::ValeurJson::Objet();
    e["source"] = SourceVersJson(m_source);
    e["jeu_bibliotheque"] = g.get_jeu();
    e["partage"] = g.get_partage();
    e["famille"] = g.get_famille();
    e["valeur"] = g.get_valeur();
    j["etude"] = e;
    decid::ValeurJson xs = decid::ValeurJson::Tableau();
    for (const auto& x : m_experiences) {
        decid::ValeurJson o = decid::ValeurJson::Objet();
        o["titre"] = x.m_titre; o["jeu"] = x.m_jeuNom; o["protocole"] = x.m_partageNom; o["score"] = x.m_score;
        o["cle_donnees"] = x.m_cleDonnees; o["cle_protocole"] = x.m_cleProtocole;
        o["tache"] = x.m_tache; o["image"] = x.m_image; o["volume"] = x.m_volume;
        o["axe_x"] = x.m_axeX; o["axe_y"] = x.m_axeY; o["axe_z"] = x.m_axeZ;
        o["famille"] = x.m_famille; o["valeur"] = x.m_valeur;
        o["source"] = SourceVersJson(x.m_source);
        o["module"] = x.m_module;
        xs.Ajouter(o);
    }
    j["experiences"] = xs;
    j["courante"] = m_courante;
    return j;
}

std::string Etude::Empreinte() const {
    decid::ValeurJson j = Sauvegarder();
    return j.Ecrire(false);
}

void Etude::Vider() {
    m_experiences.clear();
    m_cache.clear();
    m_courante = -1;
    MajExperiences();
    m_fenetre.global<::Etude>().set_resultat_present(false);
}

std::string Etude::Charger(const decid::ValeurJson& j) {
    if (j.Membre("format").Texte() != "decid-projet-app" || j.Membre("version").Entier() != 1)
        return "Ce fichier n’est pas un projet de cette version de Decid.";
    Vider();
    const auto& e = j.Membre("etude");
    const int jeu = (int)e.Membre("jeu_bibliotheque").Entier();
    auto& g = m_fenetre.global<::Etude>();
    g.set_partage(std::clamp((int)e.Membre("partage").Entier(), 0, 3));
    if (jeu >= 0 && jeu < NB_JEUX) ChoisirJeu(jeu);
    else {
        SourceEtude s = SourceDepuisJson(e.Membre("source"));
        decid::Resultat<decid::JeuDeDonnees> lu = s.m_cmc ? decid::LireCmc(s.m_chemin, s.m_relocalisations) : decid::LireCsv(s.m_chemin, s.m_contrat);
        if (lu.Ok()) { g.set_jeu(-1); Adopter(std::move(s), std::move(lu.Valeur())); }
    }
    ChoisirFamille(std::clamp((int)e.Membre("famille").Entier(), 0, (int)Familles().size() - 1));
    g.set_valeur((int)e.Membre("valeur").Entier());

    int ecartees = 0, absentes = 0;
    for (const auto& o : j.Membre("experiences").Elements()) {
        ExperienceEtude x;
        x.m_titre = o.Membre("titre").Texte(); x.m_jeuNom = o.Membre("jeu").Texte(); x.m_partageNom = o.Membre("protocole").Texte();
        x.m_score = o.Membre("score").Texte(); x.m_cleDonnees = o.Membre("cle_donnees").Texte(); x.m_cleProtocole = o.Membre("cle_protocole").Texte();
        x.m_tache = o.Membre("tache").Texte(); x.m_image = o.Membre("image").Texte(); x.m_volume = o.Membre("volume").Texte();
        x.m_axeX = (int)o.Membre("axe_x").Entier(); x.m_axeY = (int)o.Membre("axe_y").Entier(); x.m_axeZ = (int)o.Membre("axe_z").Entier();
        x.m_famille = std::clamp((int)o.Membre("famille").Entier(), 0, (int)Familles().size() - 1);
        x.m_valeur = (int)o.Membre("valeur").Entier();
        x.m_source = SourceDepuisJson(o.Membre("source"));
        x.m_module = o.Membre("module").Texte();
        if (!x.m_module.empty() && x.m_module != "validation" && x.m_module != "pas-a-pas") { ++ecartees; continue; }
        if (x.m_titre.empty() || !IdTacheValide(x.m_tache) || (!x.m_image.empty() && !IdTacheValide(x.m_image)) || (!x.m_volume.empty() && !IdTacheValide(x.m_volume))) {
            ++ecartees;
            continue;
        }
        std::error_code ec;
        x.m_absente = !fs::exists(m_moteur.DossierResultat(x.m_tache) / "artefacts.json", ec);
        if (x.m_absente) ++absentes;
        m_experiences.push_back(std::move(x));
    }
    m_courante = m_experiences.empty() ? -1 : std::clamp((int)j.Membre("courante").Entier(), 0, (int)m_experiences.size() - 1);
    MajExperiences();
    std::string r = std::to_string(m_experiences.size()) + (m_experiences.size() > 1 ? " expériences" : " expérience");
    if (ecartees) r += " · " + std::to_string(ecartees) + " illisible(s) écartée(s)";
    if (absentes) r += " · " + std::to_string(absentes) + " sans résultats sur cet ordinateur";
    if (m_courante >= 0) OuvrirExperience(m_courante);
    else { m_fenetre.set_ecran("etude"); m_fenetre.set_vue("etape"); m_fenetre.set_etape("donnees"); }
    return r;
}

void Etude::Repeindre() {
    PeindreApercu();
    PeindrePlan();
    Peindre3D();
}

void Etude::Preparer3D() {
    const auto& x = m_experiences[m_courante];
    const auto& r = *m_cache.at(x.m_tache);
    std::vector<Vue3D::Point> pts;
    const int d = r.m_jeu.Attributs();
    for (int l = 0; l < r.m_jeu.Lignes(); ++l) {
        const bool test = l < (int)r.m_prediction.size() && r.m_prediction[l] >= 0;
        if (!test) continue;   // les points de test, devant le volume
        pts.push_back({r.m_jeu.m_valeurs[(size_t)l * d + x.m_axeX], r.m_jeu.m_valeurs[(size_t)l * d + x.m_axeY], r.m_jeu.m_valeurs[(size_t)l * d + x.m_axeZ],
                       r.m_jeu.m_labels[l], 0.9f});
    }
    m_vue3d.Preparer(*r.m_volume, pts);
    m_fenetre.global<::Etude>().set_donnees_seules(m_fenetre.global<Preferences>().get_donnees3d());
    m_vue3d.FixerDonneesSeules(m_fenetre.global<::Etude>().get_donnees_seules());
    const auto& a = r.m_jeu.m_attributs;
    m_fenetre.global<::Etude>().set_legende_3d(S("x = " + a[x.m_axeX] + " · y = " + a[x.m_axeY] + " · z = " + a[x.m_axeZ] + " · glisser pour tourner, molette pour zoomer · volume "
        + std::to_string(r.m_volume->m_resolution) + "³ calculé par le moteur" + (r.m_volume->m_scores.empty() ? "" : ", opacité selon la confiance")));
}

void Etude::Peindre3D() {
    auto& g = m_fenetre.global<::Etude>();
    if (!g.get_vue_3d() || !m_vue3d.Pret() || m_planL < 8) { m_rotation.stop(); return; }
    // transparence de l'ancien Decid 0,1..0,99 -> alpha des voxels (0,7 -> 0,16)
    const float t = std::clamp(m_fenetre.global<Preferences>().get_transparence3d(), 0.1f, 0.99f);
    m_vue3d.FixerTransparence(0.16f * (1.0f - t) / 0.3f);
    if (m_fenetre.global<Preferences>().get_rotation3d() && !m_rotation.running())
        m_rotation.start(slint::TimerMode::Repeated, std::chrono::milliseconds(33), [this] {
            // rien a animer hors de l'ecran des resultats, ni fenetre reduite
            const bool visible = m_fenetre.get_ecran() == "etude" && m_fenetre.get_vue() == "etape" && m_fenetre.get_etape() == "resultats"
                && !m_fenetre.window().is_minimized();
            if (!visible || !m_fenetre.global<::Etude>().get_vue_3d() || !m_fenetre.global<Preferences>().get_rotation3d()) { m_rotation.stop(); return; }
            m_vue3d.Tourner(3.0f, 0.0f);
            Peindre3D();
        });
    const float echelle = m_fenetre.window().scale_factor();
    g.set_image_3d(m_vue3d.Rendre((int)std::lround(m_planL * echelle), (int)std::lround(m_planH * echelle), m_fenetre.global<Theme>().get_sombre(), echelle));
}

void Etude::MajExperiences() {
    m_modeleExperiences->clear();
    m_titres->clear();
    for (const auto& x : m_experiences) {
        Experience e{};
        e.titre = S(x.m_titre); e.jeu_nom = S(x.m_jeuNom); e.protocole_nom = S(x.m_partageNom); e.score = S(x.m_score);
        e.cle_donnees = S(x.m_cleDonnees); e.cle_protocole = S(x.m_cleProtocole);
        m_modeleExperiences->push_back(e);
        m_titres->push_back(S(x.m_titre + " · " + x.m_jeuNom + " · " + x.m_score));
    }
    m_fenetre.set_courante(m_courante);
    std::vector<EntreeMenu> menu;
    if (m_experiences.empty()) {
        EntreeMenu n{}; n.libelle = "Aucune expérience : chaque entraînement en ajoute une."; n.note = true; menu.push_back(n);
    }
    for (size_t k = 0; k < m_experiences.size(); ++k) {
        EntreeMenu m{};
        m.libelle = S(m_experiences[k].m_titre + " · " + m_experiences[k].m_jeuNom);
        m.raccourci = S(m_experiences[k].m_score);
        m.action = S("exp:" + std::to_string(k));
        m.courant = (int)k == m_courante;
        menu.push_back(m);
    }
    EntreeMenu sep{}; sep.separateur = true; menu.push_back(sep);
    EntreeMenu c{}; c.libelle = "Comparer les expériences…"; c.action = "experiences"; c.inactif = m_experiences.empty(); menu.push_back(c);
    m_menuExperiences->clear();
    for (auto& m : menu) m_menuExperiences->push_back(m);
}

} // namespace studio
