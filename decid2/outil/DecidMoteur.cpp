// DecidMoteur.exe : le moteur de Decid en ligne de commande (tests, scripts). Decid.exe contient le meme moteur.
#include "LigneCommande.h"

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) { return decid::LancerMoteur(argc, argv, L""); }
#else
int main(int argc, char** argv) { return decid::LancerMoteur(argc, argv, L""); }
#endif
