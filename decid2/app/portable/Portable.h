// Decid - executable portable (DECID_PORTABLE) : Decid.exe contient l'interface et le moteur (relance de lui-meme
// avec --moteur pour chaque calcul). Les jeux databasesCSV y sont embarques (ressources DECID_PAQUET et
// DECID_EMPREINTE, voir empaqueter.cpp) ; au premier lancement, ou quand l'empreinte change, ils sont extraits dans
// %LOCALAPPDATA%\Decid\portable\<empreinte>\ et DECID_EXEMPLES (si elle n'est pas deja definie) les designe.
#pragma once
#include <string>

namespace studio::portable {

// Si OpenGL est demande (captures) mais inutilisable sur ce poste, revient au rendu logiciel de Slint.
// Renvoie "" si tout va bien (ou si l'executable n'embarque rien), sinon la cause de l'echec ; dans ce cas
// l'application retombe sur la recherche habituelle (databasesCSV dans un dossier parent de l'executable).
std::string Preparer();

} // namespace studio::portable
