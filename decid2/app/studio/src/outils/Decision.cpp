#include "Decision.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "Fichiers.h"
#include "Graphique.h"
#include "Plateforme.h"

namespace studio {

namespace {
slint::SharedString S(const std::string& s) { return slint::SharedString(s); }
std::string Pct(double v) {
    char b[32];
    std::snprintf(b, sizeof b, "%.1f %%", v * 100.0);
    std::string s(b);
    for (auto& c : s) if (c == '.') c = ',';
    return s;
}
} // namespace

Decision::Decision(Fenetre& fenetre, Moteur& moteur, Etude& etude) : m_fenetre(fenetre), m_moteur(moteur), m_etude(etude) {
    auto& g = m_fenetre.global<::Decision>();
    std::vector<slint::SharedString> jeux;
    for (const auto& n : m_etude.NomsBibliotheque()) jeux.push_back(S(n));
    g.set_jeux(std::make_shared<slint::VectorModel<slint::SharedString>>(jeux));
    g.set_jeu(1);
    g.on_choisir_modele([this](int i) { ChoisirModele(i); });
    g.on_choisir_fichier([this] {
        const auto c = app::plateforme::ChoisirFichier(L"Fichier à décider", {{L"Fichiers CSV", L"*.csv;*.txt"}, {L"Tous les fichiers", L"*.*"}});
        if (!c) return;
        m_fichier = *c;
        m_fenetre.global<::Decision>().set_fichier(S(decid::CheminUtf8(c->filename())));
    });
    g.on_appliquer([this] { Appliquer(); });
    g.on_vers_importer([this] { m_fenetre.set_ecran("etude"); m_fenetre.set_vue("etape"); m_fenetre.set_etape("donnees"); });
    g.on_enregistrer([this] {
        if (m_tacheDecision.empty()) return;
        const auto c = app::plateforme::ChoisirFichierEnregistrement(L"Enregistrer les prédictions", L"predictions.csv", L"csv", {{L"Fichier CSV", L"*.csv"}});
        if (!c) return;
        std::error_code ec;
        fs::copy_file(m_moteur.DossierResultat(m_tacheDecision) / "predictions.csv", *c, fs::copy_options::overwrite_existing, ec);
        m_fenetre.invoke_montrer_toast(S(ec ? "Copie impossible : " + ec.message() : "Prédictions enregistrées : " + decid::CheminUtf8(c->filename())));
    });
}

void Decision::Actualiser() {
    auto& g = m_fenetre.global<::Decision>();
    m_modeles = m_etude.ExperiencesEntrainees();
    std::vector<slint::SharedString> v;
    for (const auto& [t, id] : m_modeles) v.push_back(S(t));
    g.set_modeles(std::make_shared<slint::VectorModel<slint::SharedString>>(v));
    ChoisirModele(m_modeles.empty() ? 0 : (int)m_modeles.size() - 1);
}

void Decision::ChoisirModele(int i) {
    auto& g = m_fenetre.global<::Decision>();
    g.set_modele(i);
    std::vector<Fait> faits;
    if (i >= 0 && i < (int)m_modeles.size()) {
        const auto dossier = m_moteur.DossierResultat(m_modeles[(size_t)i].second);
        auto m = LireJson(dossier / "modele.json");
        auto n = LireJson(dossier / "normalisation.json");
        if (m.Ok()) {
            std::string attrs, classes;
            for (const auto& a : m.Valeur().Membre("attributs").Elements()) attrs += (attrs.empty() ? "" : ", ") + a.Texte();
            for (const auto& c : m.Valeur().Membre("classes").Elements()) classes += (classes.empty() ? "" : ", ") + c.Texte();
            faits.push_back({"Algorithme", S(m.Valeur().Membre("famille").Texte())});
            faits.push_back({"Attributs attendus", S(std::to_string(m.Valeur().Membre("nb_attributs").Entier()) + " (" + attrs + ")")});
            faits.push_back({"Classes", S(classes)});
        } else faits.push_back({"Modèle", "publié illisible"});
        if (n.Ok()) {
            const auto& j = n.Valeur();
            faits.push_back({"Mise à l’échelle", S(j.Membre("methode").Texte() == "minmax" || j.Membre("methode").Texte() == "minmax_lim"
                ? "min-max vers [" + NombreFr(j.Membre("bas").Nombre(), 4) + " ; " + NombreFr(j.Membre("haut").Nombre(), 4) + "]" : "aucune")});
        }
    }
    g.set_modele_faits(std::make_shared<slint::VectorModel<Fait>>(faits));
}

void Decision::Appliquer() {
    auto& g = m_fenetre.global<::Decision>();
    if (m_calcul || m_modeles.empty()) return;
    const int i = std::clamp(g.get_modele(), 0, (int)m_modeles.size() - 1);
    fs::path chemin;
    std::string nom;
    if (g.get_mode() == 0) {
        const auto s = m_etude.SourceBibliotheque(g.get_jeu(), 0);
        if (!s) return;
        chemin = s->m_chemin;
        nom = m_etude.NomsBibliotheque()[(size_t)g.get_jeu()];
    } else {
        if (m_fichier.empty()) { g.set_erreur("Choisissez d’abord le fichier à décider."); return; }
        chemin = m_fichier;
        nom = decid::CheminUtf8(m_fichier.filename());
    }
    decid::ValeurJson r = decid::ValeurJson::Objet();
    r["operation"] = "decider";
    r["experience"] = Moteur::RefTache(m_modeles[(size_t)i].second);
    decid::ValeurJson s = decid::ValeurJson::Objet();
    s["type"] = "csv"; s["chemin"] = decid::CheminUtf8(chemin);
    s["en_tete"] = g.get_en_tete() == 1 ? "oui" : g.get_en_tete() == 2 ? "auto" : "non";
    s["separateur"] = std::string(1, g.get_separateur() == 1 ? ';' : g.get_separateur() == 2 ? '\t' : ',');
    s["colonne_cible"] = g.get_colonne() - 1;   // -1 : fichier sans classes
    r["source"] = s;
    m_calcul = true;
    g.set_calcul(true);
    g.set_erreur("");
    g.set_resultat(false);
    const std::string titre = m_modeles[(size_t)i].first;
    m_moteur.Executer(std::move(r), [this, nom, titre](const FinCalcul& fin) {
        m_calcul = false;
        auto& g = m_fenetre.global<::Decision>();
        g.set_calcul(false);
        if (!fin.m_ok) { g.set_erreur(S("Le moteur refuse : " + fin.m_erreur)); return; }
        m_tacheDecision = fin.m_id;
        auto ev = LireJson(fin.m_resultat / "evaluation.json");
        auto de = LireJson(fin.m_resultat / "decision.json");
        if (!de.Ok()) { g.set_erreur(S(de.Err().Texte())); return; }
        const auto& d = de.Valeur();
        const long long lignes = d.Membre("lignes").Entier();
        g.set_resume(S(titre + " appliqué à " + nom + " · " + std::to_string(lignes) + " lignes · " + d.Membre("famille").Texte()));
        const bool evaluee = ev.Ok() && ev.Valeur().Membre("statut").Texte() == "evalue";
        g.set_evaluee(evaluee);
        g.set_score_titre(evaluee ? "Score sur ce fichier" : "Score");
        if (evaluee) {
            const auto& rap = ev.Valeur().Membre("rapport");
            g.set_score(S(Pct(rap.Membre("agregats").Membre("exactitude").Membre("value").Nombre())));
            long long justes = 0, k = 0;
            for (const auto& l : rap.Membre("confusion").Membre("comptes").Elements()) { long long c = 0; for (const auto& v : l.Elements()) { if (c == k) justes += v.Entier(); ++c; } ++k; }
            g.set_score_note(S(std::to_string(justes) + " bien classés sur " + std::to_string(rap.Membre("confusion").Membre("total").Entier())));
            std::vector<slint::SharedString> cl;
            for (const auto& c : rap.Membre("confusion").Membre("classes").Elements()) cl.push_back(S(c.Texte()));
            std::vector<LigneConfusion> conf;
            size_t r2 = 0;
            for (const auto& l : rap.Membre("confusion").Membre("comptes").Elements()) {
                std::vector<int> c;
                int t = 0;
                for (const auto& v : l.Elements()) { c.push_back((int)v.Entier()); t += (int)v.Entier(); }
                conf.push_back({r2 < cl.size() ? cl[r2] : S(std::to_string(r2)), std::make_shared<slint::VectorModel<int>>(c), t});
                ++r2;
            }
            g.set_confusion(std::make_shared<slint::VectorModel<LigneConfusion>>(conf));
        } else {
            g.set_score("—");
            g.set_score_note(S(ev.Ok() ? ev.Valeur().Membre("raison").Texte() : "fichier sans classes : rien à compter"));
        }
        const auto& td = ev.Ok() ? ev.Valeur().Membre("temps_decision") : decid::ValeurJson();
        g.set_temps(S(td.Contient("par_echantillon_us") ? NombreFr(td.Membre("par_echantillon_us").Nombre(), 4) + " µs" : "—"));
        g.set_temps_note(S(td.Contient("echantillons") ? "par échantillon, sur " + std::to_string(td.Membre("echantillons").Entier()) + " échantillons. Mesure du moteur : " + td.Membre("mesure").Texte() + "." : ""));
        const long long hp = d.Membre("valeurs_hors_plage_normalisation").Entier();
        g.set_hors_plage(S(hp ? std::to_string(hp) + " valeurs sortent des bornes de la mise à l’échelle apprise : le modèle décide hors de la zone qu’il a vue." : ""));
        std::vector<ClassePredite> p;
        for (const auto& c : d.Membre("comptes_predits").Elements()) {
            const long long n = c.Membre("lignes").Entier();
            const double part = lignes > 0 ? (double)n / lignes : 0;
            char b[32];
            std::snprintf(b, sizeof b, "%.1f", part * 100);
            std::string pc(b);
            for (auto& ch : pc) if (ch == '.') ch = ',';
            p.push_back({S(c.Membre("classe").Texte()), (int)n, (float)part, S(std::to_string(n) + " · " + pc + " %")});
        }
        g.set_predites(std::make_shared<slint::VectorModel<ClassePredite>>(p));
        // 60 premieres lignes de predictions.csv (« ligne;origine;verite;prediction »)
        std::vector<LigneDecidee> lv;
        if (auto csv = decid::LireFichierTexte(fin.m_resultat / "predictions.csv"); csv.Ok()) {
            const std::string& t = csv.Valeur();
            size_t debut = t.find('\n');
            while (debut != std::string::npos && debut + 1 < t.size() && lv.size() < 60) {
                const size_t f = t.find('\n', debut + 1);
                std::string l = t.substr(debut + 1, (f == std::string::npos ? t.size() : f) - debut - 1);
                if (!l.empty() && l.back() == '\r') l.pop_back();
                std::vector<std::string> c;
                size_t a = 0;
                while (true) { const size_t b = l.find(';', a); c.push_back(l.substr(a, b == std::string::npos ? std::string::npos : b - a)); if (b == std::string::npos) break; a = b + 1; }
                if (c.size() >= 4) {
                    const size_t dp = c[1].find(':');
                    lv.push_back({S(dp == std::string::npos ? c[1] : c[1].substr(dp + 1)), S(c[2].empty() ? "inconnue" : c[2]), S(c[3]), !c[2].empty() && c[2] != c[3]});
                }
                debut = f;
            }
        }
        g.set_lignes(std::make_shared<slint::VectorModel<LigneDecidee>>(lv));
        g.set_lignes_note(S(std::to_string(lv.size()) + " premières sur " + std::to_string(lignes)));
        g.set_resultat(true);
    });
}

} // namespace studio
