// Point d'entree des commandes du moteur (importer, executer, worker...).
#pragma once
#include <string>

namespace decid {

// prefixe : argument remis devant la commande quand le moteur se relance lui-meme pour le worker
// ("" pour DecidMoteur.exe, "--moteur" pour Decid.exe). argv[0] est ignore.
#ifdef _WIN32
int LancerMoteur(int argc, wchar_t** argv, const std::wstring& prefixe);
#else
int LancerMoteur(int argc, char** argv, const std::wstring& prefixe);
#endif

} // namespace decid
