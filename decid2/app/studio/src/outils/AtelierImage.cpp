#include "AtelierImage.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Capture.h"
#include "Fichiers.h"
#include "Plateforme.h"

namespace studio {

namespace {
slint::SharedString S(const std::string& s) { return slint::SharedString(s); }
const char* ESPACES[] = {"Lab", "HSV", "RGB", "YCrCb"};
const int FENETRES[] = {1, 3, 5, 7, 9, 11, 15, 21};
const char* ALGOS[] = {"k-PPV", "Bayes", "SVM", "Forêt"};
constexpr std::uint32_t CLASSES_SOMBRE[6] = {0xF07A62, 0x7FB2E8, 0xF2C46B, 0x6CC4B4, 0xA99BF0, 0xE58FC0};
constexpr std::uint32_t CLASSES_CLAIR[6] = {0xC2462F, 0x3672B5, 0xB8862A, 0x2F8C7E, 0x6656C2, 0xB24C8A};
slint::Image VersImage(const ImageRgba& im) {
    slint::SharedPixelBuffer<slint::Rgba8Pixel> t(im.m_largeur, im.m_hauteur);
    std::memcpy(t.begin(), im.m_pixels.data(), im.m_pixels.size());
    return slint::Image(t);
}
} // namespace

AtelierImage::AtelierImage(::Fenetre& fenetre, Moteur& moteur)
    : m_fenetre(fenetre), m_moteur(moteur), m_webcam([](std::function<void()> a) { slint::invoke_from_event_loop(std::move(a)); }) {
    auto& g = m_fenetre.global<::AtelierImage>();
    g.on_choisir_image([this] {
        const auto c = app::plateforme::ChoisirFichier(L"Image à segmenter", {{L"Images PNG ou JPEG", L"*.png;*.jpg;*.jpeg"}});
        if (c) ChargerImage(*c);
    });
    g.on_allumer([this] {
        auto& g = m_fenetre.global<::AtelierImage>();
        g.set_erreur("");
        g.set_image_presente(false);
        g.set_segmentation(false);
        const std::string e = m_webcam.Demarrer([this](const ImageRgba& im) {
            m_derniereImage = im;
            m_fenetre.global<::AtelierImage>().set_apercu(VersImage(im));
        });
        if (!e.empty()) { g.set_erreur(S(e)); return; }
        g.set_camera(true);
    });
    g.on_eteindre([this] { m_webcam.Arreter(); m_fenetre.global<::AtelierImage>().set_camera(false); });
    g.on_capturer([this] {
        auto& g = m_fenetre.global<::AtelierImage>();
        if (m_derniereImage.m_largeur == 0) { m_fenetre.invoke_montrer_toast("La caméra n’est pas encore prête."); return; }
        const fs::path dossier = m_moteur.DossierTravail() / "donnees";
        std::error_code ec;
        fs::create_directories(dossier, ec);
        const fs::path png = dossier / ("capture-webcam-" + std::to_string(++m_captures) + ".png");
        if (!app::EcrirePng(png, m_derniereImage.m_pixels.data(), m_derniereImage.m_largeur, m_derniereImage.m_hauteur)) {
            g.set_erreur("Capture impossible à enregistrer.");
            return;
        }
        ChargerImage(png);
        g.set_fichier("capture webcam");
    });
    g.on_revenir_apercu([this] {
        auto& g = m_fenetre.global<::AtelierImage>();
        g.set_image_presente(false);
        g.set_segmentation(false);
        m_image.clear();
    });
    g.on_ajouter_classe([this] {
        auto& g = m_fenetre.global<::AtelierImage>();
        std::string n(g.get_nouvelle());
        while (!n.empty() && n.back() == ' ') n.pop_back();
        if (n.empty() || n.size() > 24 || n.find_first_of(",;\"\t") != std::string::npos) { m_fenetre.invoke_montrer_toast("Nom de classe vide ou avec un séparateur."); return; }
        const auto it = std::find(m_classes.begin(), m_classes.end(), n);
        if (it != m_classes.end()) { g.set_classe((int)(it - m_classes.begin())); g.set_nouvelle(""); return; }
        if (m_classes.size() >= 6) return;
        m_classes.push_back(n);
        g.set_classe((int)m_classes.size() - 1);
        g.set_nouvelle("");
        MajZones();
    });
    g.on_zone_tracee([this](float x, float y, float l, float h) {
        const int c = std::clamp(m_fenetre.global<::AtelierImage>().get_classe(), 0, (int)m_classes.size() - 1);
        AjouterZone(m_classes[(size_t)c], (int)std::lround(x), (int)std::lround(y), (int)std::lround(l), (int)std::lround(h));
    });
    g.on_supprimer_zone([this](int k) { if (k >= 0 && k < (int)m_zones.size()) { m_zones.erase(m_zones.begin() + k); MajZones(); } });
    g.on_apprendre([this] { Apprendre(); });
    g.on_resegmenter([this] { if (!m_experience.empty()) Segmenter(m_experience, m_echantillons); });
    g.on_enregistrer_carte([this] {
        if (m_tacheSegmentation.empty()) return;
        const auto c = app::plateforme::ChoisirFichierEnregistrement(L"Enregistrer la carte des classes", L"segmentation.png", L"png", {{L"Image PNG", L"*.png"}});
        if (!c) return;
        std::error_code ec;
        fs::copy_file(m_moteur.DossierResultat(m_tacheSegmentation) / "segmentation.png", *c, fs::copy_options::overwrite_existing, ec);
        m_fenetre.invoke_montrer_toast(S(ec ? "Copie impossible : " + ec.message() : "Carte enregistrée : " + decid::CheminUtf8(c->filename())));
    });
    MajZones();
}

std::string AtelierImage::Espace() const { return ESPACES[std::clamp(m_fenetre.global<::AtelierImage>().get_espace(), 0, 3)]; }
int AtelierImage::TailleFenetre() const { return FENETRES[std::clamp(m_fenetre.global<::AtelierImage>().get_fenetre(), 0, 7)]; }

void AtelierImage::ChargerImage(const fs::path& chemin) {
    auto& g = m_fenetre.global<::AtelierImage>();
    std::error_code ec;
    if (fs::file_size(chemin, ec) > 16ull * 1024 * 1024) { g.set_erreur("Image de plus de 16 Mo refusée."); return; }
    auto image = slint::Image::load_from_path(slint::SharedString(decid::CheminUtf8(chemin)));
    const auto taille = image.size();
    if (taille.width == 0 || taille.height == 0) { g.set_erreur("Image illisible (PNG ou JPEG attendu)."); return; }
    m_image = chemin;
    m_largeur = (int)taille.width;
    m_hauteur = (int)taille.height;
    g.set_image(image);
    g.set_largeur(m_largeur);
    g.set_hauteur(m_hauteur);
    g.set_fichier(S(decid::CheminUtf8(chemin.filename())));
    g.set_image_presente(true);
    g.set_segmentation(false);
    g.set_erreur("");
    // les zones qui sortent de la nouvelle image sont retirees
    m_zones.erase(std::remove_if(m_zones.begin(), m_zones.end(), [&](const ZoneI& z) { return z.m_x + z.m_l > m_largeur || z.m_y + z.m_h > m_hauteur; }), m_zones.end());
    MajZones();
    g.set_modele_compatible(!m_experience.empty() && m_espaceModele == Espace() && m_fenetreModele == TailleFenetre());
}

void AtelierImage::AjouterZone(const std::string& classe, int x, int y, int l, int h) {
    if (l < 2 || h < 2 || m_zones.size() >= 64 || m_largeur == 0) return;
    x = std::clamp(x, 0, m_largeur - 1); y = std::clamp(y, 0, m_hauteur - 1);
    l = std::min(l, m_largeur - x); h = std::min(h, m_hauteur - y);
    m_zones.push_back({classe, x, y, l, h});
    MajZones();
}

void AtelierImage::MajZones() {
    auto& g = m_fenetre.global<::AtelierImage>();
    std::vector<slint::SharedString> cl;
    for (const auto& c : m_classes) cl.push_back(S(c));
    g.set_classes(std::make_shared<slint::VectorModel<slint::SharedString>>(cl));
    std::vector<Zone> z;
    for (const auto& x : m_zones) {
        const int i = (int)(std::find(m_classes.begin(), m_classes.end(), x.m_classe) - m_classes.begin());
        z.push_back({i, (float)x.m_x, (float)x.m_y, (float)x.m_l, (float)x.m_h, S(x.m_classe + " · " + std::to_string(x.m_l) + " × " + std::to_string(x.m_h) + " px")});
    }
    g.set_zones(std::make_shared<slint::VectorModel<Zone>>(z));
}

void AtelierImage::Lancer(decid::ValeurJson requete, const std::string& etape, std::function<void(const FinCalcul&)> suite) {
    auto& g = m_fenetre.global<::AtelierImage>();
    m_calcul = true;
    g.set_calcul(true);
    g.set_etape(S(etape));
    m_moteur.Executer(std::move(requete), [this, suite](const FinCalcul& f) {
        m_calcul = false;
        m_fenetre.global<::AtelierImage>().set_calcul(false);
        suite(f);
    });
}

void AtelierImage::Apprendre() {
    auto& g = m_fenetre.global<::AtelierImage>();
    if (m_calcul || m_image.empty()) return;
    std::vector<std::string> distinctes;
    for (const auto& z : m_zones) if (std::find(distinctes.begin(), distinctes.end(), z.m_classe) == distinctes.end()) distinctes.push_back(z.m_classe);
    if (distinctes.size() < 2) { g.set_erreur("Tracez des zones d’au moins deux classes."); return; }
    g.set_erreur("");
    const std::string espace = Espace();
    const int fenetre = TailleFenetre();
    decid::ValeurJson r = decid::ValeurJson::Objet();
    r["operation"] = "echantillonner_couleurs";
    r["image"] = decid::CheminUtf8(m_image);
    r["espace"] = espace;
    r["fenetre"] = fenetre;
    r["graine"] = 1;
    r["par_zone"] = std::clamp(std::atoi(std::string(g.get_par_zone()).c_str()), 1, 5000);
    decid::ValeurJson zs = decid::ValeurJson::Tableau();
    for (const auto& z : m_zones) {
        decid::ValeurJson o = decid::ValeurJson::Objet();
        o["classe"] = z.m_classe; o["x"] = z.m_x; o["y"] = z.m_y; o["largeur"] = z.m_l; o["hauteur"] = z.m_h;
        zs.Ajouter(o);
    }
    r["zones"] = zs;
    const int algo = std::clamp(g.get_algorithme(), 0, 3);
    Lancer(std::move(r), "Tirage des fenêtres…", [this, espace, fenetre, algo](const FinCalcul& f1) {
        auto& g = m_fenetre.global<::AtelierImage>();
        if (!f1.m_ok) { g.set_erreur(S("Échantillonnage refusé : " + f1.m_erreur)); return; }
        m_fenetresApprises = 0;
        if (auto ej = LireJson(f1.m_resultat / "echantillons.json"); ej.Ok())
            for (const auto& [k, v] : ej.Valeur().Membre("comptes").Membres()) m_fenetresApprises += v.Entier();
        decid::ValeurJson e = decid::ValeurJson::Objet();
        decid::ValeurJson s = decid::ValeurJson::Objet();
        s["type"] = "csv"; s["chemin"] = decid::CheminUtf8(f1.m_resultat / "echantillons.csv"); s["en_tete"] = "non"; s["separateur"] = ","; s["colonne_cible"] = 0;
        e["source"] = s;
        e["threads"] = 1;
        decid::ValeurJson a = decid::ValeurJson::Objet();
        if (algo == 0) { a["famille"] = "kppv"; a["k"] = 5; }
        else if (algo == 1) a["famille"] = "bayes_normal";
        else if (algo == 2) { a["famille"] = "svm"; a["noyau"] = "RBF"; a["c"] = 10; a["gamma"] = 10; }
        else { a["famille"] = "foret_aleatoire"; a["nb_arbres"] = 50; a["profondeur_max"] = 10; a["min_echantillons"] = 2; }
        e["algorithme"] = a;
        decid::ValeurJson p = decid::ValeurJson::Objet();
        p["methode"] = "aleatoire_stratifie"; p["ratio"] = 0.5; p["graine"] = 1;
        e["partage"] = p;
        decid::ValeurJson n = decid::ValeurJson::Objet();
        n["methode"] = "minmax"; n["bas"] = 0; n["haut"] = 1;
        e["normalisation"] = n;
        const std::string ech = f1.m_id;
        Lancer(std::move(e), "Apprentissage…", [this, ech, espace, fenetre, algo](const FinCalcul& f2) {
            auto& g = m_fenetre.global<::AtelierImage>();
            if (!f2.m_ok) { g.set_erreur(S("Apprentissage refusé : " + f2.m_erreur)); return; }
            auto ev = LireEvaluation(f2.m_resultat);
            m_exactitude = ev.Ok() && ev.Valeur().m_definie ? ev.Valeur().m_exactitude : -1;
            m_experience = f2.m_id; m_echantillons = ech; m_espaceModele = espace; m_fenetreModele = fenetre; m_algoModele = ALGOS[algo];
            g.set_modele(true);
            g.set_modele_compatible(true);
            Segmenter(m_experience, m_echantillons);
        });
    });
}

void AtelierImage::Segmenter(const std::string& experience, const std::string& echantillons) {
    auto& g = m_fenetre.global<::AtelierImage>();
    if (m_calcul || m_image.empty()) return;
    decid::ValeurJson r = decid::ValeurJson::Objet();
    r["operation"] = "segmenter";
    r["experience"] = Moteur::RefTache(experience);
    r["echantillons"] = Moteur::RefTache(echantillons);
    r["image"] = decid::CheminUtf8(m_image);
    r["espace"] = m_espaceModele;
    r["fenetre"] = m_fenetreModele;
    (void)g;
    Lancer(std::move(r), "Segmentation…", [this](const FinCalcul& f) {
        auto& g = m_fenetre.global<::AtelierImage>();
        if (!f.m_ok) { g.set_erreur(S("Segmentation refusée : " + f.m_erreur)); return; }
        auto sj = LireJson(f.m_resultat / "segmentation.json");
        if (!sj.Ok()) { g.set_erreur(S(sj.Err().Texte())); return; }
        m_tacheSegmentation = f.m_id;
        const auto& j = sj.Valeur();
        m_classesPixels = LireBinaire<std::int16_t>(f.m_resultat / "segmentation.i16");
        std::vector<PartClasse> parts;
        for (const auto& c : j.Membre("classes").Elements()) {
            char b[32];
            std::snprintf(b, sizeof b, "%.1f %%", c.Membre("part").Nombre() * 100);
            std::string pc(b);
            for (auto& ch : pc) if (ch == '.') ch = ',';
            parts.push_back({S(c.Membre("nom").Texte()), S(pc), (int)c.Membre("id").Entier()});
        }
        g.set_parts(std::make_shared<slint::VectorModel<PartClasse>>(parts));
        std::string canaux;
        for (const auto& c : j.Membre("canaux").Elements()) canaux += (canaux.empty() ? "" : ", ") + c.Texte();
        const int L = (int)j.Membre("largeur").Entier(), H = (int)j.Membre("hauteur").Entier();
        std::vector<Fait> faits = {
            {"Espace, fenêtre", S(j.Membre("espace").Texte() + " (" + canaux + "), " + std::to_string(j.Membre("fenetre").Entier()) + " × " + std::to_string(j.Membre("fenetre").Entier()))},
            {"Algorithme", S(m_algoModele)},
            {"Fenêtres d’apprentissage", S(std::to_string(m_fenetresApprises))},
            {"Score sur les fenêtres de test", S(m_exactitude >= 0 ? std::to_string((int)std::lround(m_exactitude * 1000) / 10) + "," + std::to_string((int)std::lround(m_exactitude * 1000) % 10) + " %" : "—")},
            {"Pixels classés", S(std::to_string(L) + " × " + std::to_string(H))}};
        g.set_faits(std::make_shared<slint::VectorModel<Fait>>(faits));
        g.set_segmentation(true);
        g.set_vue(1);
        Repeindre();
    });
}

void AtelierImage::Repeindre() {
    auto& g = m_fenetre.global<::AtelierImage>();
    if (m_classesPixels.empty() || (size_t)m_largeur * m_hauteur != m_classesPixels.size()) return;
    const bool sombre = m_fenetre.global<Theme>().get_sombre();
    slint::SharedPixelBuffer<slint::Rgba8Pixel> t(m_largeur, m_hauteur);
    auto* o = t.begin();
    for (size_t i = 0; i < m_classesPixels.size(); ++i) {
        const std::uint32_t c = (sombre ? CLASSES_SOMBRE : CLASSES_CLAIR)[((m_classesPixels[i] % 6) + 6) % 6];
        o[i] = {(std::uint8_t)((c >> 16) & 255), (std::uint8_t)((c >> 8) & 255), (std::uint8_t)(c & 255), 153};   // opacite 0,6
    }
    g.set_carte(slint::Image(t));
}

} // namespace studio
