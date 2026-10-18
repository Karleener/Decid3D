#include "CadreFenetre.h"

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

#include <chrono>
#include <cstdlib>

namespace studio {

namespace {

constexpr UINT_PTR ID_SOUS_CLASSE = 0xDEC1D;

struct Etat {
    Fenetre* m_fenetre = nullptr;
    DWORD m_dernierAppui = 0;     // pour reconnaitre un double appui sur la barre
    POINT m_dernierPoint{};
};

// epaisseur du bord de redimensionnement, selon l'echelle de l'ecran
int Bord(HWND h) {
    const UINT dpi = GetDpiForWindow(h);
    return GetSystemMetricsForDpi(SM_CYFRAME, dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
}

LRESULT CALLBACK Procedure(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR ref) {
    auto* etat = reinterpret_cast<Etat*>(ref);
    switch (m) {
    case WM_NCCALCSIZE:
        if (w) {
            // la zone client monte jusqu'en haut de la fenetre ; les autres bords restent ceux de Windows
            auto* p = reinterpret_cast<NCCALCSIZE_PARAMS*>(l);
            const LONG haut = p->rgrc[0].top;
            const LRESULT r = DefSubclassProc(h, m, w, l);
            if (r != 0) return r;
            // agrandie, la fenetre deborde de l'ecran de l'epaisseur du bord
            p->rgrc[0].top = haut + (IsZoomed(h) ? Bord(h) : 0);
            return 0;
        }
        break;
    case WM_NCHITTEST: {
        const LRESULT r = DefSubclassProc(h, m, w, l);
        if (IsZoomed(h)) return r;
        POINT pt{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
        ScreenToClient(h, &pt);
        if (pt.y >= 0 && pt.y < Bord(h)) {     // bord du haut : redimensionnement
            if (r == HTLEFT) return HTTOPLEFT;
            if (r == HTRIGHT) return HTTOPRIGHT;
            if (r == HTCLIENT) return HTTOP;
        }
        return r;
    }
    case WM_SIZE: {
        if (w == SIZE_MINIMIZED) break;
        Fenetre* f = etat->m_fenetre;
        const bool agrandie = w == SIZE_MAXIMIZED;
        slint::invoke_from_event_loop([f, agrandie] { f->global<CadreFenetre>().set_agrandie(agrandie); });
        break;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(h, Procedure, ID_SOUS_CLASSE);
        delete etat;
        break;
    }
    return DefSubclassProc(h, m, w, l);
}

} // namespace

void IntegrerBarreTitre(Fenetre& fenetre) {
    HWND h = fenetre.window().win32_hwnd();
    if (!h) {
        // winit cree la fenetre native un peu apres le demarrage de la boucle : on reessaie (au plus 5 s)
        static int essais = 0;
        if (++essais < 100)
            slint::Timer::single_shot(std::chrono::milliseconds(50), [&fenetre] { IntegrerBarreTitre(fenetre); });
        return;
    }
    auto* etat = new Etat;
    etat->m_fenetre = &fenetre;
    if (!SetWindowSubclass(h, Procedure, ID_SOUS_CLASSE, reinterpret_cast<DWORD_PTR>(etat))) {
        delete etat;
        return;
    }
    SetWindowPos(h, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    fenetre.global<CadreFenetre>().set_agrandie(IsZoomed(h) != 0);

    fenetre.global<CadreFenetre>().on_commande([&fenetre, etat](slint::SharedString c) {
        HWND h = fenetre.window().win32_hwnd();
        if (!h) return;
        const std::string_view cmd(c.data(), c.size());
        if (cmd == "reduire") ShowWindow(h, SW_MINIMIZE);
        else if (cmd == "agrandir") ShowWindow(h, IsZoomed(h) ? SW_RESTORE : SW_MAXIMIZE);
        else if (cmd == "fermer") PostMessageW(h, WM_CLOSE, 0, 0);   // passe par la confirmation de fermeture
        else if (cmd == "glisser") {
            POINT pt;
            GetCursorPos(&pt);
            const DWORD t = GetTickCount();
            const bool double_ = t - etat->m_dernierAppui <= GetDoubleClickTime()
                && std::abs(pt.x - etat->m_dernierPoint.x) <= GetSystemMetrics(SM_CXDOUBLECLK)
                && std::abs(pt.y - etat->m_dernierPoint.y) <= GetSystemMetrics(SM_CYDOUBLECLK);
            if (double_) {
                etat->m_dernierAppui = 0;
                ShowWindow(h, IsZoomed(h) ? SW_RESTORE : SW_MAXIMIZE);
                return;
            }
            etat->m_dernierAppui = t;
            etat->m_dernierPoint = pt;
            // comme une legende Windows : deplacement, ancrage aux bords (Aero Snap)
            ReleaseCapture();
            PostMessageW(h, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(pt.x, pt.y));
        }
    });
}

} // namespace studio
