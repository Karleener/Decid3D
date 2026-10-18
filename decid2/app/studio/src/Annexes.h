// Dialogues annexes : Guide de TP (etapes faites selon la seance) et Exporter (artefacts publies par le moteur,
// recopies tels quels ; tableau des experiences en CSV).
#pragma once
#include <string>
#include <vector>

#include "Etude.h"
#include "Moteur.h"
#include "decid.h"

namespace studio {

class Annexes {
public:
    Annexes(Fenetre& fenetre, Moteur& moteur, Etude& etude);
    void DialogueOuvert(const std::string& id);

private:
    void MajGuide();
    void MajExporter();
    void ChoisirExperience(int k);

    Fenetre& m_fenetre;
    Moteur& m_moteur;
    Etude& m_etude;
};

} // namespace studio
