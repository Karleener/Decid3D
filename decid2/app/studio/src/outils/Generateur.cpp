#include "Generateur.h"

#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "Donnees.h"
#include "Fichiers.h"
#include "Graphique.h"
#include "Plateforme.h"
#include "Toile.h"

namespace studio {

namespace {
slint::SharedString S(const std::string& s) { return slint::SharedString(s); }
double LireNombre(const std::string& t, double defaut) {
    std::string u = t;
    for (auto& c : u) if (c == ',') c = '.';
    char* fin = nullptr;
    const double v = std::strtod(u.c_str(), &fin);
    return fin && fin != u.c_str() && std::isfinite(v) ? v : defaut;
}
const char* MODELES[] = {"gaussiennes_2d", "gaussiennes_3d", "spirales_3d"};
const char* LIBELLES[] = {"Gaussiennes 2D", "Gaussiennes 3D", "Spirales 3D"};
} // namespace

Generateur::Generateur(Fenetre& fenetre, Moteur& moteur, Etude& etude) : m_fenetre(fenetre), m_moteur(moteur), m_etude(etude) {
    // reglages par defaut
    Classe a; a.m_nom = "A"; a.m_centre = {0.3, 0.6, 0}; a.m_ecart = {0.08, 0.05};
    Classe b; b.m_nom = "B"; b.m_centre = {0.68, 0.38, 0}; b.m_ecart = {0.06, 0.09};
    m_classes[0] = {a, b};
    Classe c3[3];
    const double centres3[3][3] = {{0.3, 0.3, 0.3}, {0.7, 0.3, 0.6}, {0.5, 0.7, 0.7}};
    for (int i = 0; i < 3; ++i) { c3[i].m_nom = std::string(1, char('A' + i)); c3[i].m_effectif = 200; c3[i].m_centre = {centres3[i][0], centres3[i][1], centres3[i][2]}; c3[i].m_ecart = {0.08, 0.08}; }
    m_classes[1] = {c3[0], c3[1], c3[2]};
    Classe s1; s1.m_nom = "A"; s1.m_effectif = 300; s1.m_centre = {0.5, 0.5, 0.2};
    Classe s2 = s1; s2.m_nom = "B"; s2.m_phase = 3.1416;
    m_classes[2] = {s1, s2};

    auto& g = m_fenetre.global<::Generateur>();
    g.on_choisir_mode([this](int i) { ChoisirMode(i); });
    g.on_changer_nom([this](int i, slint::SharedString t) {
        if (i < 0 || i >= (int)Classes().size()) return;
        std::string n(t);
        if (n.empty() || n.size() > 24 || n.find_first_of(",;\"\t") != std::string::npos) { m_fenetre.invoke_montrer_toast("Nom de classe vide ou avec un séparateur."); MajClasses(); return; }
        Classes()[(size_t)i].m_nom = n;
        Invalider();
    });
    g.on_changer_champ([this](int i, int j, slint::SharedString t) {
        if (i < 0 || i >= (int)Classes().size()) return;
        Classe& c = Classes()[(size_t)i];
        const double v = LireNombre(std::string(t), NAN);
        if (std::isnan(v)) { MajClasses(); return; }
        std::vector<double*> champs;
        if (m_mode == 0) champs = {&c.m_effectif, &c.m_centre[0], &c.m_centre[1], &c.m_ecart[0], &c.m_ecart[1]};
        else if (m_mode == 1) champs = {&c.m_effectif, &c.m_centre[0], &c.m_centre[1], &c.m_centre[2], &c.m_ecart[0]};
        else champs = {&c.m_effectif, &c.m_centre[0], &c.m_centre[1], &c.m_centre[2], &c.m_diametre, &c.m_longueur, &c.m_tours, &c.m_phase, &c.m_bruit};
        if (j < 0 || j >= (int)champs.size()) return;
        *champs[(size_t)j] = j == 0 ? std::clamp(std::round(v), 1.0, 100000.0) : v;
        if (m_mode != 2 && j >= 3 + (m_mode == 1 ? 1 : 0)) *champs[(size_t)j] = std::max(0.0, v);
        Invalider();
    });
    g.on_supprimer([this](int i) { if (Classes().size() > 1 && i >= 0 && i < (int)Classes().size()) { Classes().erase(Classes().begin() + i); m_fenetre.global<::Generateur>().set_selection(0); Invalider(); } });
    g.on_ajouter([this] {
        if (Classes().size() >= 12) return;
        Classe c = Classes().back();
        c.m_nom = NomLibre();
        if (m_mode == 0) c.m_centre = {0.5, 0.5, 0};
        if (m_mode == 2) c.m_phase = std::round(std::fmod(c.m_phase + 1.5708, 6.2832) * 1000) / 1000;
        Classes().push_back(c);
        m_fenetre.global<::Generateur>().set_selection((int)Classes().size() - 1);
        Invalider();
    });
    g.on_generer([this] { Generer(); });
    g.on_enregistrer([this] {
        if (m_csv.empty()) return;
        const std::wstring nom = std::wstring(MODELES[m_mode], MODELES[m_mode] + std::strlen(MODELES[m_mode])) + L"-graine" +
                                 std::to_wstring((long long)LireNombre(std::string(m_fenetre.global<::Generateur>().get_graine()), 7)) + L".csv";
        const auto c = app::plateforme::ChoisirFichierEnregistrement(L"Enregistrer le jeu généré", nom, L"csv", {{L"Fichier CSV", L"*.csv"}});
        if (!c) return;
        std::error_code ec;
        fs::copy_file(m_csv, *c, fs::copy_options::overwrite_existing, ec);
        m_fenetre.invoke_montrer_toast(S(ec ? "Copie impossible : " + ec.message() : "Jeu enregistré : " + decid::CheminUtf8(c->filename())));
    });
    g.on_etudier([this] {
        if (m_csv.empty()) return;
        m_etude.ImporterFichier(m_csv, {}, true);
        m_fenetre.set_ecran("etude"); m_fenetre.set_vue("etape"); m_fenetre.set_etape("donnees"); m_fenetre.set_sous_vue_donnees(0);
    });
    g.on_souris([this](int genre, float x, float y) { Souris(genre, x, y); });
    g.on_tourner([this](float dx, float dy) { m_vue3d.Tourner(dx, dy); Peindre(); });
    g.on_zoomer([this](float c) { m_vue3d.Zoomer(-c); Peindre(); });
    g.on_taille([this](float l, float h) { if (std::abs(l - m_l) < 1 && std::abs(h - m_h) < 1) return; const bool premier = m_l < 1; m_l = l; m_h = h; if (premier) Peindre(); else m_redim.start(slint::TimerMode::SingleShot, std::chrono::milliseconds(80), [this] { Peindre(); }); });
    ChoisirMode(0);
}

std::string Generateur::NomLibre() const {
    for (char c = 'A'; c <= 'Z'; ++c) {
        bool pris = false;
        for (const auto& x : m_classes[(size_t)m_mode]) pris = pris || x.m_nom == std::string(1, c);
        if (!pris) return std::string(1, c);
    }
    return "C" + std::to_string(m_classes[(size_t)m_mode].size());
}

void Generateur::ChoisirMode(int mode) {
    m_mode = std::clamp(mode, 0, 2);
    auto& g = m_fenetre.global<::Generateur>();
    g.set_mode(m_mode);
    g.set_selection(0);
    Invalider();
}

void Generateur::Invalider() {
    m_resultat = false;
    m_points.clear();
    m_classesPoints.clear();
    m_csv.clear();
    auto& g = m_fenetre.global<::Generateur>();
    g.set_resultat(false);
    g.set_erreur("");
    MajClasses();
    Peindre();
}

void Generateur::MajClasses() {
    std::vector<ClasseGen> v;
    auto f = [](double x) { return S(NombreFr(std::round(x * 1000) / 1000, 6)); };
    for (const auto& c : Classes()) {
        std::vector<ParametreSaisi> ch;
        ch.push_back({"effectif", "Points", S(std::to_string((long long)c.m_effectif))});
        ch.push_back({"cx", "Centre x", f(c.m_centre[0])});
        ch.push_back({"cy", "Centre y", f(c.m_centre[1])});
        if (m_mode >= 1) ch.push_back({"cz", "Centre z", f(c.m_centre[2])});
        if (m_mode == 0) { ch.push_back({"sx", "Écart-type x", f(c.m_ecart[0])}); ch.push_back({"sy", "Écart-type y", f(c.m_ecart[1])}); }
        if (m_mode == 1) ch.push_back({"s", "Écart-type", f(c.m_ecart[0])});
        if (m_mode == 2) {
            ch.push_back({"d", "Diamètre", f(c.m_diametre)}); ch.push_back({"l", "Longueur", f(c.m_longueur)}); ch.push_back({"t", "Tours", f(c.m_tours)});
            ch.push_back({"p", "Phase (rad)", f(c.m_phase)}); ch.push_back({"b", "Bruit", f(c.m_bruit)});
        }
        v.push_back({S(c.m_nom), std::make_shared<slint::VectorModel<ParametreSaisi>>(ch)});
    }
    m_fenetre.global<::Generateur>().set_classes(std::make_shared<slint::VectorModel<ClasseGen>>(v));
}

void Generateur::Souris(int genre, float x, float y) {
    if (m_mode != 0) return;
    x = std::clamp(x, 0.0f, 1.0f);
    y = std::clamp(y, 0.0f, 1.0f);
    auto& cl = Classes();
    if (genre == 0) {
        // un centre a moins de 10 px : on le deplace ; sinon on trace une nouvelle classe
        const double seuil = 10.0 / std::max(1.0f, std::min(m_l, m_h));
        m_geste = 0;
        for (size_t i = 0; i < cl.size(); ++i)
            if (std::hypot(cl[i].m_centre[0] - x, cl[i].m_centre[1] - y) < seuil) {
                m_geste = 1; m_classeGeste = (int)i; m_dx = cl[i].m_centre[0] - x; m_dy = cl[i].m_centre[1] - y;
                m_fenetre.global<::Generateur>().set_selection((int)i);
                break;
            }
        if (m_geste == 0) {
            if (cl.size() >= 12) { m_fenetre.invoke_montrer_toast("Au plus 12 classes."); return; }
            Classe c; c.m_nom = NomLibre(); c.m_effectif = 150; c.m_centre = {x, y, 0}; c.m_ecart = {0.02, 0.02};
            cl.push_back(c);
            m_geste = 2; m_classeGeste = (int)cl.size() - 1; m_x0 = x; m_y0 = y;
            m_fenetre.global<::Generateur>().set_selection(m_classeGeste);
        }
        m_resultat = false;
        m_fenetre.global<::Generateur>().set_resultat(false);
        Peindre();
        return;
    }
    if (m_geste == 0 || m_classeGeste < 0 || m_classeGeste >= (int)cl.size()) return;
    Classe& c = cl[(size_t)m_classeGeste];
    if (m_geste == 1) c.m_centre = {std::clamp(x + m_dx, 0.0, 1.0), std::clamp(y + m_dy, 0.0, 1.0), 0};
    else c.m_ecart = {std::max(0.005, std::abs(x - m_x0)), std::max(0.005, std::abs(y - m_y0))};
    if (genre == 2) { m_geste = 0; Invalider(); return; }
    Peindre();
}

void Generateur::Generer() {
    auto& g = m_fenetre.global<::Generateur>();
    if (m_calcul) return;
    const long long graine = (long long)LireNombre(std::string(g.get_graine()), 7);
    decid::ValeurJson r = decid::ValeurJson::Objet();
    r["operation"] = "generer";
    r["modele"] = MODELES[m_mode];
    r["graine"] = graine;
    decid::ValeurJson cls = decid::ValeurJson::Tableau();
    for (const auto& c : Classes()) {
        decid::ValeurJson o = decid::ValeurJson::Objet();
        o["nom"] = c.m_nom;
        o["effectif"] = (long long)c.m_effectif;
        decid::ValeurJson ce = decid::ValeurJson::Tableau();
        ce.Ajouter(c.m_centre[0]); ce.Ajouter(c.m_centre[1]);
        if (m_mode >= 1) ce.Ajouter(c.m_centre[2]);
        o["centre"] = ce;
        if (m_mode == 0) { decid::ValeurJson e = decid::ValeurJson::Tableau(); e.Ajouter(c.m_ecart[0]); e.Ajouter(c.m_ecart[1]); o["ecart_type"] = e; }
        if (m_mode == 1) o["ecart_type"] = c.m_ecart[0];
        if (m_mode == 2) {
            o["diametre"] = c.m_diametre; o["longueur"] = c.m_longueur; o["tours"] = c.m_tours; o["phase"] = c.m_phase; o["bruit"] = c.m_bruit;
            if (g.get_axe() == 0) { decid::ValeurJson ax = decid::ValeurJson::Tableau(); ax.Ajouter(0); ax.Ajouter(0); ax.Ajouter(1); o["axe"] = ax; }
        }
        cls.Ajouter(o);
    }
    r["classes"] = cls;
    if (m_mode >= 1 && g.get_bornes()) { decid::ValeurJson e = decid::ValeurJson::Objet(); e["min"] = 0; e["max"] = 1; r["ecretage"] = e; }
    m_calcul = true;
    g.set_calcul(true);
    g.set_erreur("");
    m_moteur.Executer(std::move(r), [this, graine](const FinCalcul& fin) {
        m_calcul = false;
        auto& g = m_fenetre.global<::Generateur>();
        g.set_calcul(false);
        if (!fin.m_ok) { g.set_erreur(S("Le moteur a refusé la génération : " + fin.m_erreur)); return; }
        auto js = LireJson(fin.m_resultat / "generation.json");
        if (!js.Ok()) { g.set_erreur(S(js.Err().Texte())); return; }
        const auto& j = js.Valeur();
        m_csv = fin.m_resultat / "generation.csv";
        decid::ContratCsv contrat;
        auto lu = decid::LireCsv(m_csv, contrat);
        m_points.clear();
        m_classesPoints.clear();
        std::string relu = "Lecture refusée.";
        if (lu.Ok()) {
            const auto& d = lu.Valeur();
            for (int l = 0; l < d.m_nbLignes; ++l) {
                std::array<double, 3> p{0, 0, 0};
                for (int a = 0; a < std::min(3, d.m_nbAttributs); ++a) p[(size_t)a] = d.m_valeurs[(size_t)l * d.m_nbAttributs + a];
                m_points.push_back(p);
                m_classesPoints.push_back(d.m_labels[l]);
            }
            std::vector<long long> n(d.m_nomsClasses.size(), 0);
            for (int l : d.m_labels) ++n[(size_t)l];
            relu = "Classes : " + std::to_string(d.m_nomsClasses.size()) + " · effectifs : ";
            for (size_t k = 0; k < n.size(); ++k) relu += (k ? " · " : "") + d.m_nomsClasses[k] + " : " + std::to_string(n[k]);
        }
        g.set_relu(S(relu));
        std::vector<Fait> faits = {{"Modèle", S(LIBELLES[m_mode])}, {"Lignes", S(std::to_string(j.Membre("nb_lignes").Entier()))}};
        std::string attrs;
        for (const auto& a : j.Membre("noms_attributs").Elements()) attrs += (attrs.empty() ? "" : ", ") + a.Texte();
        faits.push_back({"Attributs", S(attrs)});
        faits.push_back({"Graine", S(std::to_string(graine))});
        const auto& mn = j.Membre("min").Elements();
        const auto& mx = j.Membre("max").Elements();
        const auto& na = j.Membre("noms_attributs").Elements();
        for (size_t a = 0; a < na.size() && a < mn.size() && a < mx.size(); ++a)
            faits.push_back({"Étendue", S(na[a].Texte() + " ∈ [" + NombreFr(mn[a].Nombre(), 3) + " ; " + NombreFr(mx[a].Nombre(), 3) + "]")});
        const std::string h = j.Membre("sha256_csv").Texte();
        faits.push_back({"Empreinte", S(h.substr(0, 12) + "…")});
        g.set_faits(std::make_shared<slint::VectorModel<Fait>>(faits));
        m_resultat = true;
        g.set_resultat(true);
        if (m_mode >= 1) {
            VolumeMoteur boite;
            for (int k = 0; k < 3; ++k) { boite.m_min[k] = 0; boite.m_max[k] = 1; }
            for (const auto& p : m_points) for (int k = 0; k < 3; ++k) { boite.m_min[k] = std::min(boite.m_min[k], p[(size_t)k]); boite.m_max[k] = std::max(boite.m_max[k], p[(size_t)k]); }
            std::vector<Vue3D::Point> pts;
            for (size_t i = 0; i < m_points.size(); ++i) pts.push_back({m_points[i][0], m_points[i][1], m_points[i][2], m_classesPoints[i], 0.85f});
            m_vue3d.Preparer(boite, pts);
        }
        Peindre();
    });
}

void Generateur::Peindre() {
    auto& g = m_fenetre.global<::Generateur>();
    if (m_l < 8 || m_h < 8) return;
    const float e = m_fenetre.window().scale_factor();
    const int L = (int)std::lround(m_l * e), H = (int)std::lround(m_h * e);
    const bool sombre = m_fenetre.global<Theme>().get_sombre();
    if (m_mode >= 1) {
        if (!m_resultat) {
            Toile t(L, H, sombre, {0, 1, 0, 1});
            g.set_vue(t.Image());
            g.set_legende("La vue 3D affichera les points publiés par le moteur.");
            return;
        }
        g.set_vue(m_vue3d.Rendre(L, H, sombre, e));
        g.set_legende("glisser pour tourner · molette pour zoomer · points lus dans generation.csv");
        return;
    }
    Toile t(L, H, sombre, {0, 1, 0, 1});
    for (size_t i = 0; i < m_points.size(); ++i) t.Point(m_points[i][0], m_points[i][1], m_classesPoints[i], 2.6f * e, 0.75f);
    const int sel = g.get_selection();
    const auto& cl = m_classes[0];
    for (size_t i = 0; i < cl.size(); ++i) {
        const auto& c = cl[i];
        t.EllipseAxiale(c.m_centre[0], c.m_centre[1], 2 * c.m_ecart[0], 2 * c.m_ecart[1], (int)i, 0.0f, 1.0f * e);
        t.EllipseAxiale(c.m_centre[0], c.m_centre[1], c.m_ecart[0], c.m_ecart[1], (int)i, 0.0f, ((int)i == sel ? 2.0f : 1.2f) * e);
        t.Point(c.m_centre[0], c.m_centre[1], (int)i, ((int)i == sel ? 4.5f : 3.5f) * e, 1.0f);
    }
    g.set_vue(t.Image());
    g.set_legende(S(std::string("x → · y ↑ · ") + (m_resultat ? "points lus dans generation.csv" : "aucun point tiré pour l’instant") + " · ellipses : 1 et 2 écarts-types demandés"));
}

} // namespace studio
