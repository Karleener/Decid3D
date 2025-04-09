// DonneeFloat.h: interface for the CDonneeFloatMod class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <opencv2/core.hpp>
#include <opencv2/ml.hpp>

class CDonneeFloatMod  
{
public:
    CDonneeFloatMod();
    CDonneeFloatMod(const std::string& nomFichier, int proportion, bool inverse = false, bool visuTest = false);
    virtual ~CDonneeFloatMod();

    // Fonctions de chargement de données
    int LireDonnee(const std::string& nomFichier, int proportion, bool inverse = false, bool redistribAleat = false, bool firstHalf = false);
    int LireDonneeCSV(const std::string& nomFichier, int proportion, bool inverse = false, bool redistribAleat = false, bool firstHalf = false);
    int LireDonneeCSVRegression(const std::string& nomFichier, int proportion, bool inverse = false, bool redistribAleat = false, bool firstHalf = false);
  
    // Fonctions de sauvegarde
    void SaveToCSV(const std::string& fileName);
    void SauveMinMax(const std::string& nomFichier);
    void LitMinMax(const std::string& nomFichier);
    
    // Fonctions de manipulation de données
    void Repartition(int proportion, bool inverse, bool firstHalf, const std::string& nomFichier);
    void CreerFichiersCMCEtDat(const std::string& nomFichier);
    
    // Fonctions de normalisation
    void Normalise(float binf = 0.1f, float bsup = 0.9f);
    void NormaliseSimple();
    void NormaliseVisu();
    
    // Fonctions de redistribution
    void RedistribAleat();
    void RedistribCV(int cv, int iter);
    void RedistribAppTest(int Na);
    
    // Fonctions d'initialisation de mémoire
    void InitMem(int nbpar, int nbcl, int nbmes);
    void InitMemTab(int nbpar, int nbcl, const std::vector<int>& nbmes, const std::vector<int>& nbmestest = {});
    
    // Fonctions utilitaires
    void Clear();
    bool isNumber(const std::string& str);
    char DetectSeparator(const std::string& line);
    
    // Convertir vers/depuis les formats OpenCV
    cv::Ptr<cv::ml::TrainData> ToOpenCVTrainData() const;
    void FromOpenCVTrainData(const cv::Ptr<cv::ml::TrainData>& trainData);
    
    // Accesseurs
    cv::Mat GetTrainingData() const { return m_trainingData; }
    cv::Mat GetTrainingLabels() const { return m_trainingLabels; }
    cv::Mat GetTestData() const { return m_testData; }
    cv::Mat GetTestLabels() const { return m_testLabels; }
    void CreateDataFromMat();
    
public:
    // Matrices OpenCV pour stocker les données
    cv::Mat m_trainingData;    // Données d'apprentissage
    cv::Mat m_trainingLabels;  // Étiquettes d'apprentissage
    cv::Mat m_testData;        // Données de test
    cv::Mat m_testLabels;      // Étiquettes de test
    cv::Mat m_allData;         // Toutes les données
    cv::Mat m_allLabels;       // Toutes les étiquettes
    cv::Mat m_VisuData;       // Les données visualisées
    
    std::vector<float> m_mins; // Valeurs minimales pour normalisation
    std::vector<float> m_maxs; // Valeurs maximales pour normalisation
    
    // Statistiques sur les données
    std::vector<int> m_nbMesAppParClasse;      // Nombre de mesures par classe
    std::vector<int> m_nbMesOrigParClasse;  // Nombre original de mesures par classe
    std::vector<int> m_nbMesTestParClasse;  // Nombre de mesures de test par classe

    
    int m_nbMesTotalApp;    // Nombre total de mesures d'apprentissage
    int m_nbMesTotal;       // Nombre total de mesures
    int m_nbMesTotalTest;   // Nombre total de mesures de test
    int m_nbParametres;     // Nombre de paramètres (variables)
    int m_nbClasses;        // Nombre de classes

    cv::Ptr<cv::ml::TrainData> m_pData;
    
    bool m_visuTest;        // Flag pour visualisation des tests
    int m_erreur;           // Code d'erreur
    
    // Méthodes internes privées
    //void PreprocessData();
    void SplitData(int proportion, bool inverse, bool firstHalf);
};
