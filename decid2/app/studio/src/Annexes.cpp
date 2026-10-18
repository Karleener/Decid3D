#include "Annexes.h"

#include <algorithm>
#include <cstdio>

#include "Fichiers.h"
#include "Plateforme.h"

namespace studio {

namespace {
slint::SharedString S(const std::string& s) { return slint::SharedString(s); }
std::string Taille(long long o) {
    char b[32];
    if (o < 1024) std::snprintf(b, sizeof b, "%lld o", o);
    else if (o < 1024 * 1024) std::snprintf(b, sizeof b, "%.1f ko", o / 1024.0);
    else std::snprintf(b, sizeof b, "%.1f Mo", o / 1048576.0);
    std::string s(b);
    for (auto& c : s) if (c == '.') c = ',';
    return s;
}
struct Principal { const char* m_nom; const char* m_libelle; const char* m_description; };
const Principal PRINCIPAUX[] = {
    {"evaluation.json", "Évaluation", "score sur le test, matrice de confusion, résultats par classe"},
    {"predictions.csv", "Prédictions", "classe réelle et classe prédite de chaque point de test"},
    {"image.png", "Image de décision", "régions du plan coloriées par le moteur"},
    {"volume.json", "Volume 3D", "description de la grille de décision 3D"},
    {"volume.i16", "Volume 3D (données)", "classe de chaque voxel, entiers 16 bits"},
};
std::string Csv(const std::string& v) {
    if (v.find_first_of("\",;\n") == std::string::npos) return v;
    std::string r = "\"";
    for (char c : v) { if (c == '"') r += '"'; r += c; }
    return r + "\"";
}
} // namespace

Annexes::Annexes(Fenetre& fenetre, Moteur& moteur, Etude& etude) : m_fenetre(fenetre), m_moteur(moteur), m_etude(etude) {
    auto& e = m_fenetre.global<::Exporter>();
    e.on_choisir([this](int k) { ChoisirExperience(k); });
    e.on_enregistrer([this](slint::SharedString tache, slint::SharedString nom) {
        const std::string t(tache), n(nom);
        if (t.empty() || n.find("..") != std::string::npos) return;
        const fs::path source = m_moteur.DossierResultat(t) / decid::CheminDepuisUtf8(n);
        std::string propose = n;
        for (auto& c : propose) if (c == '/' || c == '\\') c = '-';
        const auto c = app::plateforme::ChoisirFichierEnregistrement(L"Enregistrer un artefact du moteur", decid::CheminDepuisUtf8(propose).wstring(),
                                                                   decid::CheminDepuisUtf8(propose).extension().wstring().substr(std::min<size_t>(1, decid::CheminDepuisUtf8(propose).extension().wstring().size())), {});
        if (!c) return;
        std::error_code ec;
        fs::copy_file(source, *c, fs::copy_options::overwrite_existing, ec);
        m_fenetre.invoke_montrer_toast(S(ec ? "Copie impossible : " + ec.message() : "Enregistré : " + decid::CheminUtf8(c->filename())));
    });
    e.on_tableau([this] {
        std::string csv = "experience,donnees,partage,exactitude_test\n";
        for (const auto& x : m_etude.Experiences()) csv += Csv(x.m_titre) + "," + Csv(x.m_jeuNom) + "," + Csv(x.m_partageNom) + "," + Csv(x.m_score == "—" ? "" : x.m_score) + "\n";
        const auto c = app::plateforme::ChoisirFichierEnregistrement(L"Tableau des expériences", L"decid-experiences.csv", L"csv", {{L"Fichier CSV", L"*.csv"}});
        if (!c) return;
        auto st = decid::EcrireFichierAtomique(*c, csv, true);
        m_fenetre.invoke_montrer_toast(S(st.Ok() ? "Tableau enregistré : " + decid::CheminUtf8(c->filename()) : "Enregistrement impossible : " + st.Err().Texte()));
    });
}

void Annexes::DialogueOuvert(const std::string& id) {
    if (id == "guide") MajGuide();
    if (id == "exporter") MajExporter();
}

void Annexes::MajGuide() {
    const auto& X = m_etude.Experiences();
    const bool une = !X.empty();
    bool comparables = false;
    for (size_t i = 0; i < X.size(); ++i)
        for (size_t j = i + 1; j < X.size(); ++j) comparables = comparables || (X[i].m_cleProtocole == X[j].m_cleProtocole && X[i].m_cleDonnees == X[j].m_cleDonnees);
    std::vector<bool> f = {une, une, une, comparables, false};
    m_fenetre.global<::Guide>().set_faites(std::make_shared<slint::VectorModel<bool>>(f));
}

void Annexes::MajExporter() {
    auto& e = m_fenetre.global<::Exporter>();
    std::vector<slint::SharedString> v;
    for (const auto& x : m_etude.Experiences()) v.push_back(S(x.m_titre + " · " + x.m_jeuNom + " · " + x.m_score));
    e.set_experiences(std::make_shared<slint::VectorModel<slint::SharedString>>(v));
    const int k = m_etude.Courante() >= 0 ? m_etude.Courante() : (int)v.size() - 1;
    ChoisirExperience(std::max(0, k));
}

void Annexes::ChoisirExperience(int k) {
    auto& e = m_fenetre.global<::Exporter>();
    const auto& X = m_etude.Experiences();
    e.set_experience(k);
    std::vector<Artefact> principaux, tous;
    if (k >= 0 && k < (int)X.size()) {
        const auto& x = X[(size_t)k];
        for (const std::string& t : {x.m_tache, x.m_image, x.m_volume}) {
            if (t.empty()) continue;
            auto a = LireJson(m_moteur.DossierResultat(t) / "artefacts.json");
            if (!a.Ok()) continue;
            for (const auto& f : a.Valeur().Membre("fichiers").Elements()) {
                std::string chemin = f.Membre("chemin").Texte();
                for (auto& c : chemin) if (c == '\\') c = '/';
                const std::string taille = Taille(f.Membre("octets").Entier());
                tous.push_back({S(chemin), "", "", S(taille), S(t)});
                for (const auto& p : PRINCIPAUX)
                    if (chemin == p.m_nom) principaux.push_back({S(chemin), S(p.m_libelle), S(p.m_description), S(taille), S(t)});
            }
        }
        e.set_message(S(tous.empty() ? "Les fichiers publiés par le moteur pour cette expérience ne sont pas sur cet ordinateur." : ""));
    }
    e.set_principaux(std::make_shared<slint::VectorModel<Artefact>>(principaux));
    e.set_tous(std::make_shared<slint::VectorModel<Artefact>>(tous));
}

} // namespace studio
