#include "Webcam.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

#include <chrono>
#include <memory>

namespace studio {

namespace {
template <typename T> struct Com {
    T* p = nullptr;
    ~Com() { if (p) p->Release(); }
    T** operator&() { return &p; }
    T* operator->() { return p; }
};
} // namespace

std::string Webcam::Demarrer(Abonne abonne) {
    if (m_actif) return "";
    Arreter();
    // Media Foundation est charge a la demande (/DELAYLOAD dans l'executable portable) : absent des editions
    // Windows « N » sans Media Feature Pack, on le signale au lieu de laisser l'appel echouer.
    for (const wchar_t* dll : {L"mfplat.dll", L"mf.dll", L"mfreadwrite.dll"})
        if (!LoadLibraryExW(dll, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32))
            return "Media Foundation est absent de ce Windows (édition N : installer le Media Feature Pack).";
    // L'ouverture se fait dans le thread de lecture (COM et Media Foundation y sont initialises) ; le resultat de
    // l'ouverture est attendu ici pour pouvoir signaler une absence de camera tout de suite.
    auto etat = std::make_shared<std::atomic<int>>(0);   // 0 en cours, 1 ouverte, 2 echec
    auto message = std::make_shared<std::string>();
    m_continuer = true;
    m_thread = std::thread([this, abonne, etat, message] {
        const HRESULT hc = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const bool mf = SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_LITE));
        Com<IMFAttributes> attr;
        IMFActivate** appareils = nullptr;
        UINT32 nb = 0;
        Com<IMFMediaSource> source;
        Com<IMFSourceReader> lecteur;
        UINT32 L = 0, H = 0;
        LONG pas = 0;
        auto echec = [&](const char* m) { *message = m; etat->store(2); };
        if (!mf) echec("Media Foundation indisponible sur ce système.");
        else if (FAILED(MFCreateAttributes(&attr, 1)) || FAILED(attr->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID)) ||
                 FAILED(MFEnumDeviceSources(attr.p, &appareils, &nb)) || nb == 0)
            echec("Aucune caméra trouvée. Utilisez une image fixe.");
        else if (FAILED(appareils[0]->ActivateObject(IID_PPV_ARGS(&source))))
            echec("Accès à la caméra refusé. Autorisez-le dans les paramètres de confidentialité de Windows, ou utilisez une image fixe.");
        else {
            Com<IMFAttributes> opts;
            MFCreateAttributes(&opts, 1);
            opts->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
            Com<IMFMediaType> type;
            if (FAILED(MFCreateSourceReaderFromMediaSource(source.p, opts.p, &lecteur)) || FAILED(MFCreateMediaType(&type)) ||
                FAILED(type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video)) || FAILED(type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32)) ||
                FAILED(lecteur->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, type.p)))
                echec("La caméra ne fournit pas d’image utilisable (RGB32).");
            else {
                Com<IMFMediaType> actuel;
                lecteur->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &actuel);
                MFGetAttributeSize(actuel.p, MF_MT_FRAME_SIZE, &L, &H);
                UINT32 s = 0;
                pas = SUCCEEDED(actuel->GetUINT32(MF_MT_DEFAULT_STRIDE, &s)) ? (LONG)s : (LONG)L * 4;
                etat->store(1);
            }
        }
        for (UINT32 i = 0; i < nb; ++i) appareils[i]->Release();
        CoTaskMemFree(appareils);
        if (etat->load() == 1) {
            m_actif = true;
            while (m_continuer) {
                DWORD flux = 0, drapeaux = 0;
                LONGLONG t = 0;
                Com<IMFSample> echantillon;
                if (FAILED(lecteur->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &flux, &drapeaux, &t, &echantillon))) break;
                if (drapeaux & MF_SOURCE_READERF_ENDOFSTREAM) break;
                if (!echantillon.p) continue;
                Com<IMFMediaBuffer> tampon;
                if (FAILED(echantillon->ConvertToContiguousBuffer(&tampon))) continue;
                BYTE* octets = nullptr;
                DWORD taille = 0;
                if (FAILED(tampon->Lock(&octets, nullptr, &taille))) continue;
                ImageRgba im;
                im.m_largeur = (int)L;
                im.m_hauteur = (int)H;
                im.m_pixels.resize((size_t)L * H * 4);
                const LONG pasAbs = pas < 0 ? -pas : pas;
                if ((DWORD)pasAbs * H <= taille)
                    for (UINT32 y = 0; y < H; ++y) {
                        const BYTE* ligne = octets + (size_t)(pas < 0 ? (H - 1 - y) : y) * pasAbs;   // pas negatif : image de bas en haut
                        for (UINT32 x = 0; x < L; ++x) {
                            std::uint8_t* d = &im.m_pixels[((size_t)y * L + x) * 4];
                            d[0] = ligne[x * 4 + 2]; d[1] = ligne[x * 4 + 1]; d[2] = ligne[x * 4 + 0]; d[3] = 255;   // BGRA -> RGBA
                        }
                    }
                tampon->Unlock();
                auto partage = std::make_shared<ImageRgba>(std::move(im));
                m_dispatcheur([abonne, partage] { abonne(*partage); });
            }
            m_actif = false;
        }
        if (source.p) source->Shutdown();
        if (mf) MFShutdown();
        if (SUCCEEDED(hc)) CoUninitialize();
    });
    while (etat->load() == 0) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if (etat->load() == 2) {
        Arreter();
        return *message;
    }
    return "";
}

void Webcam::Arreter() {
    m_continuer = false;
    if (m_thread.joinable()) m_thread.join();
    m_actif = false;
}

} // namespace studio
