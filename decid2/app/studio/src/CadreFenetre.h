// La barre de menus de Decid tient lieu de barre de titre Windows : la legende native est retiree
// (WM_NCCALCSIZE), les bords gardent le redimensionnement et l'ancrage de Windows, la barre deplace la fenetre.
#pragma once
#include "decid.h"

namespace studio {

// A appeler une fois la fenetre creee (dans la boucle d'evenements).
void IntegrerBarreTitre(Fenetre& fenetre);

} // namespace studio
