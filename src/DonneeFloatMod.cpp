// Implémentation de DonneeFloat.cpp
#include "stdafx.h"
#include "DonneeFloatMod.h"
#include <random>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <limits>
#include <string>
#include <opencv2/core/utils/logger.hpp>

std::vector<float> unique(const cv::Mat& input, bool sort = false)
{
    if (input.channels() > 1 || input.type() != CV_32F)
    {
        std::cerr << "unique !!! Only works with CV_32F 1-channel Mat" << std::endl;
        return std::vector<float>();
    }

    std::vector<float> out;
    for (int y = 0; y < input.rows; ++y)
    {
        const float* row_ptr = input.ptr<float>(y);
        for (int x = 0; x < input.cols; ++x)
        {
            float value = row_ptr[x];
            if (std::find(out.begin(), out.end(), value) == out.end())
                out.push_back(value);
        }
    }

    if (sort)
        std::sort(out.begin(), out.end());

    return out;
}

CDonneeFloatMod::CDonneeFloatMod() : 
    m_nbMesTotalApp(0),
    m_nbMesTotal(0),
    m_nbMesTotalTest(0),
    m_nbParametres(0),
    m_nbClasses(0),
    m_visuTest(false),
    m_erreur(0)
{
}

CDonneeFloatMod::CDonneeFloatMod(const std::string& nomFichier, int proportion, bool inverse, bool visuTest) :
    m_nbMesTotalApp(0),
    m_nbMesTotal(0),
    m_nbMesTotalTest(0),
    m_nbParametres(0),
    m_nbClasses(0),
    m_visuTest(visuTest),
    m_erreur(0)
{
    LireDonneeCSV(nomFichier, proportion, inverse);
}

CDonneeFloatMod::~CDonneeFloatMod()
{
    Clear();
}

void CDonneeFloatMod::Clear()
{
    m_trainingData.release();
    m_trainingLabels.release();
    m_testData.release();
    m_testLabels.release();
    m_allData.release();
    m_allLabels.release();
    
    m_mins.clear();
    m_maxs.clear();
    m_nbMesAppParClasse.clear();
    m_nbMesOrigParClasse.clear();
    m_nbMesTestParClasse.clear();
   // m_kmaxParClasse.clear();
    
    m_nbMesTotalApp = 0;
    m_nbMesTotal = 0;
    m_nbMesTotalTest = 0;
    m_nbParametres = 0;
    m_nbClasses = 0;
}

int CDonneeFloatMod::LireDonnee(const std::string& nomFichier, int proportion, bool inverse, bool redistribAleat, bool firstHalf)
{
 
        std::ifstream fichierCMC(nomFichier);
        if (!fichierCMC.is_open()) {
           // MessageBox(NULL, "Impossible d'ouvrir le fichier CMC.", "Erreur", MB_OK);
            return -1;
        }

        // Lire le nombre de classes
        fichierCMC >> m_nbClasses;
        m_nbMesOrigParClasse.resize(m_nbClasses, 0);
        std::vector <int> nbFichiersParClasse;
        for (int c = 0; c < m_nbClasses; c++) {
            int nbFichiers;
            fichierCMC >> nbFichiers;
            nbFichiersParClasse.push_back(nbFichiers);
        }
        // Lire le nombre de fichiers par classe et stocker les noms des fichiers
        std::vector<std::vector<std::string>> fichiersDat(m_nbClasses);
        fichierCMC.ignore();
        for (int c = 0; c < m_nbClasses; c++) 
        {
            fichiersDat[c].resize(nbFichiersParClasse[c]);
            for (int f = 0; f < nbFichiersParClasse[c]; f++) 
            {
                std::getline(fichierCMC, fichiersDat[c][f]);
            }
        }
        fichierCMC.close();

        // Vecteurs temporaires pour stocker les données avant conversion en Mat
        std::vector<std::vector<float>> allData;
        std::vector<int> allLabels;

        // Lecture des fichiers .dat pour chaque classe
        for (int classLabel = 0; classLabel < m_nbClasses; ++classLabel) 
        {
            for (const std::string& fichier : fichiersDat[classLabel]) 
            {
                std::ifstream datFile(fichier);
                if (!datFile.is_open()) {
                    //MessageBox(NULL, "Impossible d'ouvrir un fichier .dat.", "Erreur", MB_OK);
                    return -1;
                }

                // Lire le nombre de paramètres et le nombre d'échantillons
                int nbParametres, nbEchantillons;
                datFile >> nbParametres >> nbEchantillons;
                m_nbMesOrigParClasse[classLabel] += nbEchantillons; // Accumuler le nombre total d'échantillons

                // Lire les données
                for (int i = 0; i < nbEchantillons; i++) 
                {
                    std::vector<float> sample(nbParametres);
                    for (int j = 0; j < nbParametres; j++)
                    {
                        datFile >> sample[j];
                    }
                    allData.push_back(sample);
                    allLabels.push_back(classLabel);
                }
                datFile.close();
            }
        }

        // Convertir les vecteurs en cv::Mat
        int totalSamples = allData.size();
        if (totalSamples == 0) {
           // MessageBox(NULL, "Aucune donnée chargée.", "Erreur", MB_OK);
            return -1;
        }

        m_nbMesTotal = totalSamples;
        m_nbParametres = allData[0].size();
        m_allData = cv::Mat(totalSamples, m_nbParametres, CV_32F);
        m_allLabels = cv::Mat(totalSamples, 1, CV_32F);

        for (int i = 0; i < totalSamples; i++) {
            for (int j = 0; j < m_nbParametres; j++) {
                m_allData.at<float>(i, j) = allData[i][j];
            }
            m_allLabels.at<float>(i, 0) = (float)allLabels[i];
        }

       // MessageBox(NULL, "Lecture des fichiers CMC et DAT terminée.", "Information", MB_OK);
    
        m_pData = cv::ml::TrainData::create(m_allData,cv::ml::ROW_SAMPLE, m_allLabels);
        if (redistribAleat)
        {
            m_pData->shuffleTrainTest();
        }

        float ratio = float(proportion) / 100.0;
        m_pData->setTrainTestSplitRatio(ratio, true);
        m_trainingData = m_pData->getTrainSamples();
        m_trainingLabels = m_pData->getTrainResponses();
        m_testData = m_pData->getTestSamples();
        m_testLabels = m_pData->getTestResponses();

        m_nbMesAppParClasse.resize(m_nbClasses, 0);
        m_nbMesOrigParClasse.resize(m_nbClasses, 0);
        m_nbMesTestParClasse.resize(m_nbClasses, 0);
        std::fill(m_nbMesAppParClasse.begin(), m_nbMesAppParClasse.end(), 0);
        std::fill(m_nbMesOrigParClasse.begin(), m_nbMesOrigParClasse.end(), 0);
        std::fill(m_nbMesTestParClasse.begin(), m_nbMesTestParClasse.end(), 0);

        m_VisuData = m_trainingData.clone(); // visu

        for (int i = 0; i < m_trainingData.cols; i++) {
            cv::normalize(m_trainingData.col(i), m_VisuData.col(i), 0, 1, cv::NORM_MINMAX);
        }

        for (int i = 0; i < m_trainingLabels.rows; i++)
        {
            float label = m_trainingLabels.at<float>(i, 0);
            m_nbMesAppParClasse[int(label)]++;
            m_nbMesOrigParClasse[int(label)]++;
        }
        for (int i = 0; i < m_testLabels.rows; i++)
        {
            float label = m_testLabels.at<float>(i, 0);
            m_nbMesTestParClasse[int(label)]++;
            m_nbMesOrigParClasse[int(label)]++;
        }

        m_nbMesTotal = m_allLabels.rows;
        m_nbMesTotalApp = m_trainingData.rows;
        m_nbMesTotalTest = m_testData.rows;

        // Calculer min et max pour chaque paramètre
        m_mins.resize(m_nbParametres, std::numeric_limits<float>::max());
        m_maxs.resize(m_nbParametres, std::numeric_limits<float>::lowest());

        for (int i = 0; i < m_nbMesTotalApp; i++)
        {
            for (int j = 0; j < m_nbParametres; j++)
            {
                float val = m_trainingData.at<float>(i, j);
                m_mins[j] = std::min(m_mins[j], val);
                m_maxs[j] = std::max(m_maxs[j], val);
            }
        }

        return 1;
}

char CDonneeFloatMod::DetectSeparator(const std::string& line)
{
    // Détecte automatiquement le séparateur dans un fichier CSV
    std::map<char, int> separatorCount;
    
    for (char c : {',', ';', '\t', ' '}) {
        separatorCount[c] = std::count(line.begin(), line.end(), c);
    }
    
    // Trouver le séparateur le plus fréquent
    return std::max_element(
        separatorCount.begin(), 
        separatorCount.end(),
        [](const auto& p1, const auto& p2) { return p1.second < p2.second; }
    )->first;
}

bool CDonneeFloatMod::isNumber(const std::string& str)
{
    if (str.empty()) return false;
    
    std::istringstream iss(str);
    float f;
    iss >> std::noskipws >> f;
    
    return iss.eof() && !iss.fail();
}

int CDonneeFloatMod::LireDonneeCSV(const std::string& nomFichier, int proportion, bool inverse, bool redistribAleat, bool firstHalf)
{
    std::ifstream file(nomFichier);
    if (!file.is_open())
    {
        return 0;
    }
    std::string line;
    char separator = ','; // Initialisation par défaut du séparateur
    if (std::getline(file, line))
    {
        separator = DetectSeparator(line); // Détecter le séparateur
    }
    file.close();

    Clear();

    m_pData = cv::ml::TrainData::loadFromCSV(nomFichier, 1, 0, -1, cv::String(), separator);
    if (m_pData.empty())
    {
        std::cerr << "Impossible d'ouvrir le fichier: " << nomFichier << std::endl;
        m_erreur = 2;
        return 0;
    }
    m_nbParametres = m_pData->getNVars();
    if (redistribAleat) {
        m_pData->shuffleTrainTest();
    }
    m_allData = m_pData->getSamples();
    m_allLabels = m_pData->getResponses();
    // Trouver le nombre de classes distinctes
    std::vector<float> uniqueClasses;
    uniqueClasses = unique(m_allLabels, true);  // Trouve les valeurs uniques dans les réponses
    m_nbClasses = uniqueClasses.size();  // Le nombre de classes est le nombre de valeurs uniques

    
    // Redimensionner les vecteurs de comptage
    m_nbMesAppParClasse.resize(m_nbClasses, 0);
    m_nbMesOrigParClasse.resize(m_nbClasses, 0);
    m_nbMesTestParClasse.resize(m_nbClasses, 0);
    std::fill(m_nbMesAppParClasse.begin(), m_nbMesAppParClasse.end(), 0);
    std::fill(m_nbMesOrigParClasse.begin(), m_nbMesOrigParClasse.end(), 0);
    std::fill(m_nbMesTestParClasse.begin(), m_nbMesTestParClasse.end(), 0);

   // m_kmaxParClasse.resize(m_nbClasses, 0);

    //cv::FileStorage fsTrain("all_labels.yml", cv::FileStorage::WRITE);
    //fsTrain << "all_labels" << m_allLabels;
    //fsTrain.release();
    
    // Compter les mesures par classe
    for (int i = 0; i < m_allLabels.rows;i++)
    {
        float label = m_allLabels.at<float>(i,0);
        m_nbMesOrigParClasse[int(label)]++;
    }
    
    
    // Répartir les données d'apprentissage et de test
    //SplitData(proportion, inverse, firstHalf);
    float ratio = float(proportion) / 100.0;
    if (ratio < 1.0f) {
        m_pData->setTrainTestSplitRatio(ratio, true);
        m_trainingData = m_pData->getTrainSamples();
        m_trainingLabels = m_pData->getTrainResponses();
        m_testData = m_pData->getTestSamples();
        m_testLabels = m_pData->getTestResponses();
    }
    else {
        m_trainingData = m_allData;
        m_trainingLabels = m_allLabels;
        m_testData.release();
        m_testLabels.release();
    }


    m_VisuData = m_trainingData.clone(); // Copie des données d'entrée

    for (int i = 0; i < m_trainingData.cols; i++) {
        cv::normalize(m_trainingData.col(i), m_VisuData.col(i), 0, 1, cv::NORM_MINMAX);
    }

    for (int i = 0; i < m_trainingLabels.rows; i++)
    {
        float label = m_trainingLabels.at<float>(i, 0);
        m_nbMesAppParClasse[int(label)]++;
    }
    for (int i = 0; i < m_testLabels.rows; i++)
    {
        float label = m_testLabels.at<float>(i, 0);
        m_nbMesTestParClasse[int(label)]++;
    }

    m_nbMesTotal = m_allLabels.rows;
    m_nbMesTotalApp = m_trainingData.rows;
    m_nbMesTotalTest = m_testData.rows;

    // Calculer min et max pour chaque paramètre
    m_mins.resize(m_nbParametres, std::numeric_limits<float>::max());
    m_maxs.resize(m_nbParametres, std::numeric_limits<float>::lowest());

    for (int i = 0; i < m_nbMesTotalApp; i++)
    {
        for (int j = 0; j < m_nbParametres; j++) 
        {
            float val = m_trainingData.at<float>(i, j);
            m_mins[j] = std::min(m_mins[j], val);
            m_maxs[j] = std::max(m_maxs[j], val);
        }
    }
    std::string NomCmc = nomFichier.substr(0, nomFichier.rfind('.')) + ".cmc";
    CreerFichiersCMCEtDat(NomCmc);
    std::string NomFichMinMax = nomFichier.substr(0, nomFichier.rfind('.')) + ".lim";
    SauveMinMax(NomFichMinMax);
    return m_nbMesTotal;
}

int CDonneeFloatMod::LireDonneeCSVRegression(const std::string& nomFichier, int proportion, bool inverse, bool redistribAleat, bool firstHalf)
{

    std::ifstream file(nomFichier);
    if (!file.is_open())
    {
        return 0;
    }
    std::string line;
    char separator = ';'; // Initialisation par défaut du séparateur
    if (std::getline(file, line))
    {
        separator = DetectSeparator(line); // Détecter le séparateur
    }
    file.close();


    Clear();

    m_pData = cv::ml::TrainData::loadFromCSV(nomFichier, 0, 0, -1, cv::String(), separator); // ';'
    if (m_pData.empty())
    {
        std::cerr << "Impossible d'ouvrir le fichier: " << nomFichier << std::endl;
        m_erreur = 2;
        return 0;
    }
    m_nbParametres = m_pData->getNVars();
    if (redistribAleat) {
        m_pData->shuffleTrainTest();
    }
    m_allData = m_pData->getSamples();
    m_allLabels = m_pData->getResponses();

    m_nbClasses =1;  // Le nombre de classes est le nombre de valeurs uniques

    // Redimensionner les vecteurs de comptage
    m_nbMesAppParClasse.resize(m_nbClasses, 0);
    m_nbMesOrigParClasse.resize(m_nbClasses, 0);
    m_nbMesTestParClasse.resize(m_nbClasses, 0);
    std::fill(m_nbMesAppParClasse.begin(), m_nbMesAppParClasse.end(), 0);
    std::fill(m_nbMesOrigParClasse.begin(), m_nbMesOrigParClasse.end(), 0);
    std::fill(m_nbMesTestParClasse.begin(), m_nbMesTestParClasse.end(), 0);

   // m_kmaxParClasse.resize(m_nbClasses, 0);

    //cv::FileStorage fsTrain("all_labels.yml", cv::FileStorage::WRITE);
    //fsTrain << "all_labels" << m_allLabels;
    //fsTrain.release();

    m_nbMesOrigParClasse[0] = m_allData.rows;

    // Répartir les données d'apprentissage et de test

    float ratio = float(proportion) / 100.0;
    m_pData->setTrainTestSplitRatio(ratio, true);
    m_trainingData = m_pData->getTrainSamples();
    m_trainingLabels = m_pData->getTrainResponses();
    m_testData = m_pData->getTestSamples();
    m_testLabels = m_pData->getTestResponses();


    m_VisuData = m_trainingData.clone(); // Copie des données d'entrée

    for (int i = 0; i < m_trainingData.cols; i++) {
        cv::normalize(m_trainingData.col(i), m_VisuData.col(i), 0, 1, cv::NORM_MINMAX);
    }


    m_nbMesOrigParClasse[0] = m_trainingData.rows;
    m_nbMesTestParClasse[0] = m_testData.rows;
    m_nbMesTotal = m_allLabels.rows;
    m_nbMesTotalApp = m_trainingData.rows;
    m_nbMesTotalTest = m_testData.rows;

    // Calculer min et max pour chaque paramètre
    m_mins.resize(m_nbParametres, std::numeric_limits<float>::max());
    m_maxs.resize(m_nbParametres, std::numeric_limits<float>::lowest());

    for (int i = 0; i < m_nbMesTotalApp; i++)
    {
        for (int j = 0; j < m_nbParametres; j++)
        {
            float val = m_trainingData.at<float>(i, j);
            m_mins[j] = std::min(m_mins[j], val);
            m_maxs[j] = std::max(m_maxs[j], val);
        }
    }


    return m_nbMesTotal;
}


void CDonneeFloatMod::RedistribAleat()
{
    // Créer un vecteur d'indices
    std::vector<int> indices(m_nbMesTotal);
    for (int i = 0; i < m_nbMesTotal; i++) {
        indices[i] = i;
    }
    
    // Mélanger les indices
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(indices.begin(), indices.end(), g);
    
    // Créer des matrices temporaires pour stocker les données mélangées
    cv::Mat tempData(m_nbMesTotal, m_nbParametres, CV_32F);
    cv::Mat tempLabels(m_nbMesTotal, 1, m_allLabels.type());
    
    for (int i = 0; i < m_nbMesTotal; i++) {
        int idx = indices[i];
        m_allData.row(idx).copyTo(tempData.row(i));
        m_allLabels.row(idx).copyTo(tempLabels.row(i));
    }
    
    // Remplacer les données originales par les données mélangées
    tempData.copyTo(m_allData);
    tempLabels.copyTo(m_allLabels);
}

void CDonneeFloatMod::SplitData(int proportion, bool inverse, bool firstHalf)
{
    if (m_nbMesTotal == 0 || proportion <= 0 || proportion >= 100) {
        m_nbMesTotalApp = m_nbMesTotal;
        m_nbMesTotalTest = 0;
        m_trainingData = m_allData.clone();
        m_trainingLabels = m_allLabels.clone();
        return;
    }
    
    // Réinitialiser les compteurs
    m_nbMesTotalApp = 0;
    m_nbMesTotalTest = 0;
    std::fill(m_nbMesAppParClasse.begin(), m_nbMesAppParClasse.end(), 0);
    std::fill(m_nbMesTestParClasse.begin(), m_nbMesTestParClasse.end(), 0);
    
    // Compter le nombre de mesures par classe
    std::vector<std::vector<int>> indicesByClass(m_nbClasses);
    for (int i = 0; i < m_nbMesTotal; i++) {
        int label;
        if (m_allLabels.type() == CV_32S) {
            label = m_allLabels.at<int>(i, 0);
        } else {
            // Pour la régression, on ne classe pas par label
            label = 0;
        }
        indicesByClass[label].push_back(i);
    }
    
    // Calculer le nombre de mesures d'apprentissage et de test par classe
    std::vector<int> nbAppByClass(m_nbClasses);
    std::vector<int> nbTestByClass(m_nbClasses);
    
    for (int c = 0; c < m_nbClasses; c++) {
        int nbTotal = indicesByClass[c].size();
        nbAppByClass[c] = nbTotal * proportion / 100;
        nbTestByClass[c] = nbTotal - nbAppByClass[c];
        
        m_nbMesTotalApp += nbAppByClass[c];
        m_nbMesTotalTest += nbTestByClass[c];
    }
    
    // Créer les matrices pour les données d'apprentissage et de test
    m_trainingData = cv::Mat(m_nbMesTotalApp, m_nbParametres, CV_32F);
    m_trainingLabels = cv::Mat(m_nbMesTotalApp, 1, m_allLabels.type());
    m_testData = cv::Mat(m_nbMesTotalTest, m_nbParametres, CV_32F);
    m_testLabels = cv::Mat(m_nbMesTotalTest, 1, m_allLabels.type());
    
    // Remplir les matrices
    int trainIdx = 0;
    int testIdx = 0;
    
    for (int c = 0; c < m_nbClasses; c++) {
        int nbApp = nbAppByClass[c];
        int nbTest = nbTestByClass[c];
        
        // Inverser les ensembles si nécessaire
        if (inverse) {
            std::swap(nbApp, nbTest);
        }
        
        m_nbMesAppParClasse[c] = nbApp;
        m_nbMesTestParClasse[c] = nbTest;
        
        // Déterminer l'ensemble des indices à utiliser pour l'apprentissage
        std::vector<int> trainIndices;
        if (firstHalf) {
            // Utiliser la première moitié
            trainIndices.assign(indicesByClass[c].begin(), 
                               indicesByClass[c].begin() + nbApp);
        } else {
            // Utiliser une sélection aléatoire
            trainIndices.assign(indicesByClass[c].begin(), 
                               indicesByClass[c].begin() + nbApp);
        }
        
        // Construire l'ensemble des indices pour le test (le reste)
        std::vector<int> testIndices;
        std::set_difference(
            indicesByClass[c].begin(), indicesByClass[c].end(),
            trainIndices.begin(), trainIndices.end(),
            std::back_inserter(testIndices)
        );
        
        // Remplir les données d'apprentissage
        for (int idx : trainIndices) {
            m_allData.row(idx).copyTo(m_trainingData.row(trainIdx));
            m_allLabels.row(idx).copyTo(m_trainingLabels.row(trainIdx));
            trainIdx++;
        }
        
        // Remplir les données de test
        for (int idx : testIndices) {
            m_allData.row(idx).copyTo(m_testData.row(testIdx));
            m_allLabels.row(idx).copyTo(m_testLabels.row(testIdx));
            testIdx++;
        }
    }
}

void CDonneeFloatMod::Normalise(float binf, float bsup)
{
    if (m_nbMesTotal == 0 || m_nbParametres == 0) {
        return;
    }
    
    // Calculer min et max pour chaque paramètre
    m_mins.resize(m_nbParametres, std::numeric_limits<float>::max());
    m_maxs.resize(m_nbParametres, std::numeric_limits<float>::lowest());
    
    for (int i = 0; i < m_nbMesTotal; i++) {
        for (int j = 0; j < m_nbParametres; j++) {
            float val = m_allData.at<float>(i, j);
            m_mins[j] = std::min(m_mins[j], val);
            m_maxs[j] = std::max(m_maxs[j], val);
        }
    }
    
    // Normaliser les données
    for (int i = 0; i < m_nbMesTotal; i++) {
        for (int j = 0; j < m_nbParametres; j++) {
            float val = m_allData.at<float>(i, j);
            float range = m_maxs[j] - m_mins[j];
            
            if (range > 0) {
                float normVal = binf + (bsup - binf) * (val - m_mins[j]) / range;
                m_allData.at<float>(i, j) = normVal;
            }
        }
    }
    
    // Mettre à jour les données d'apprentissage et de test
    if (!m_trainingData.empty()) {
        for (int i = 0; i < m_nbMesTotalApp; i++) {
            for (int j = 0; j < m_nbParametres; j++) {
                float val = m_trainingData.at<float>(i, j);
                float range = m_maxs[j] - m_mins[j];
                
                if (range > 0) {
                    float normVal = binf + (bsup - binf) * (val - m_mins[j]) / range;
                    m_trainingData.at<float>(i, j) = normVal;
                }
            }
        }
    }
    
    if (!m_testData.empty()) {
        for (int i = 0; i < m_nbMesTotalTest; i++) {
            for (int j = 0; j < m_nbParametres; j++) {
                float val = m_testData.at<float>(i, j);
                float range = m_maxs[j] - m_mins[j];
                
                if (range > 0) {
                    float normVal = binf + (bsup - binf) * (val - m_mins[j]) / range;
                    m_testData.at<float>(i, j) = normVal;
                }
            }
        }
    }
}

void CDonneeFloatMod::NormaliseSimple()
{
    Normalise(0.0f, 1.0f);
}

void CDonneeFloatMod::SauveMinMax(const std::string& nomFichier)
{
    if (m_mins.empty() || m_maxs.empty()) {
        return;
    }
    
    std::ofstream file(nomFichier);
    if (!file.is_open()) {
        std::cerr << "Impossible d'ouvrir le fichier pour sauvegarder min/max: " << nomFichier << std::endl;
        return;
    }
    
    file << m_nbParametres << std::endl;
    
    for (int i = 0; i < m_nbParametres; i++) {
        file << m_mins[i] << " " << m_maxs[i] << std::endl;
    }
}

void CDonneeFloatMod::LitMinMax(const std::string& nomFichier)
{
    std::ifstream file(nomFichier);
    if (!file.is_open()) {
        std::cerr << "Impossible d'ouvrir le fichier pour lire min/max: " << nomFichier << std::endl;
        return;
    }
    
    int nbParams;
    file >> nbParams;
    
    m_mins.resize(nbParams);
    m_maxs.resize(nbParams);
    
    for (int i = 0; i < nbParams; i++) {
        file >> m_mins[i] >> m_maxs[i];
    }
}

void CDonneeFloatMod::SaveToCSV(const std::string& fileName)
{
    std::ofstream file(fileName);
    if (!file.is_open()) {
        std::cerr << "Impossible d'ouvrir le fichier pour sauvegarder les données: " << fileName << std::endl;
        return;
    }
    
	// Sauvegarder toutes les données avec les étiquettes
	for (int i = 0; i < m_nbMesTotal; i++)
	{
		if (m_allLabels.type() == CV_32S)
		{
			file  << m_allLabels.at<int>(i, 0) << ",";
		}
		else
		{
			file << m_allLabels.at<float>(i, 0) << ",";
		}

		for (int j = 0; j < m_nbParametres; j++)
		{
			file << m_allData.at<float>(i, j);
			if (j < m_nbParametres - 1) 
            {
				file << ",";
			}
		}
		file << std::endl;
	}
}

cv::Ptr<cv::ml::TrainData> CDonneeFloatMod::ToOpenCVTrainData() const
{
    if (m_trainingData.empty() || m_trainingLabels.empty()) {
        return nullptr;
    }
    
    return cv::ml::TrainData::create(
        m_trainingData,
        cv::ml::ROW_SAMPLE,
        m_trainingLabels
    );
}

void CDonneeFloatMod::CreateDataFromMat()
{
    m_pData = cv::ml::TrainData::create(m_allData, cv::ml::ROW_SAMPLE, m_allLabels);
    //if (redistribAleat)
    //{
    //    m_pData->shuffleTrainTest();
    //}

    float ratio = float(50) / 100.0;
    m_pData->setTrainTestSplitRatio(ratio, true);
    m_trainingData = m_pData->getTrainSamples();
    m_trainingLabels = m_pData->getTrainResponses();
    m_testData = m_pData->getTestSamples();
    m_testLabels = m_pData->getTestResponses();

    m_nbMesAppParClasse.resize(m_nbClasses, 0);
    m_nbMesOrigParClasse.resize(m_nbClasses, 0);
    m_nbMesTestParClasse.resize(m_nbClasses, 0);
    std::fill(m_nbMesAppParClasse.begin(), m_nbMesAppParClasse.end(), 0);
    std::fill(m_nbMesOrigParClasse.begin(), m_nbMesOrigParClasse.end(), 0);
    std::fill(m_nbMesTestParClasse.begin(), m_nbMesTestParClasse.end(), 0);

    m_VisuData = m_trainingData.clone(); // visu

    for (int i = 0; i < m_trainingData.cols; i++) {
        cv::normalize(m_trainingData.col(i), m_VisuData.col(i), 0, 1, cv::NORM_MINMAX);
    }

    for (int i = 0; i < m_trainingLabels.rows; i++)
    {
        float label = m_trainingLabels.at<float>(i, 0);
        m_nbMesAppParClasse[int(label)]++;
        m_nbMesOrigParClasse[int(label)]++;
    }
    for (int i = 0; i < m_testLabels.rows; i++)
    {
        float label = m_testLabels.at<float>(i, 0);
        m_nbMesTestParClasse[int(label)]++;
        m_nbMesOrigParClasse[int(label)]++;
    }

    m_nbMesTotal = m_allLabels.rows;
    m_nbMesTotalApp = m_trainingData.rows;
    m_nbMesTotalTest = m_testData.rows;

    // Calculer min et max pour chaque paramètre
    m_mins.resize(m_nbParametres, std::numeric_limits<float>::max());
    m_maxs.resize(m_nbParametres, std::numeric_limits<float>::lowest());

    for (int i = 0; i < m_nbMesTotalApp; i++)
    {
        for (int j = 0; j < m_nbParametres; j++)
        {
            float val = m_trainingData.at<float>(i, j);
            m_mins[j] = std::min(m_mins[j], val);
            m_maxs[j] = std::max(m_maxs[j], val);
        }
    }
}


void CDonneeFloatMod::FromOpenCVTrainData(const cv::Ptr<cv::ml::TrainData>& trainData)
{
    if (!trainData) {
        return;
    }
    
    Clear();
    
    // Récupérer les matrices de données
    m_trainingData = trainData->getTrainSamples();
    m_trainingLabels = trainData->getTrainResponses();
    
    // Mettre à jour les attributs
    m_nbMesTotalApp = m_trainingData.rows;
    m_nbParametres = m_trainingData.cols;
    
    // Compter le nombre de classes
    if (m_trainingLabels.type() == CV_32S) {
        int maxLabel = 0;
        for (int i = 0; i < m_nbMesTotalApp; i++) {
            int label = m_trainingLabels.at<int>(i, 0);
            maxLabel = std::max(maxLabel, label);
        }
        m_nbClasses = maxLabel + 1;
    } else {
        // Pour la régression
        m_nbClasses = 1;
    }
    
    // Initialiser les vecteurs de comptage
    m_nbMesAppParClasse.resize(m_nbClasses, 0);
    m_nbMesOrigParClasse.resize(m_nbClasses, 0);
    std::fill(m_nbMesAppParClasse.begin(), m_nbMesAppParClasse.end(), 0);
    std::fill(m_nbMesOrigParClasse.begin(), m_nbMesOrigParClasse.end(), 0);

    
    // Compter les mesures par classe
    for (int i = 0; i < m_nbMesTotalApp; i++) {
        int label;
        if (m_trainingLabels.type() == CV_32S) {
            label = m_trainingLabels.at<int>(i, 0);
        } else {
            label = 0; // Pour la régression
        }
        m_nbMesAppParClasse[label]++;
        m_nbMesOrigParClasse[label]++;
    }
    
    // Récupérer les données de test si disponibles
    if (trainData->getTestResponses().rows > 0) {
        m_testData = trainData->getTestSamples();
        m_testLabels = trainData->getTestResponses();
        m_nbMesTotalTest = m_testData.rows;
        
        // Mettre à jour les compteurs pour les données de test
        m_nbMesTestParClasse.resize(m_nbClasses, 0);
        
        for (int i = 0; i < m_nbMesTotalTest; i++) {
            int label;
            if (m_testLabels.type() == CV_32S) {
                label = m_testLabels.at<int>(i, 0);
            } else {
                label = 0; // Pour la régression
            }
            m_nbMesTestParClasse[label]++;
        }
    }
    
    // Combiner les données d'apprentissage et de test pour avoir toutes les données
    m_nbMesTotal = m_nbMesTotalApp + m_nbMesTotalTest;
    m_allData = cv::Mat(m_nbMesTotal, m_nbParametres, CV_32F);
    m_allLabels = cv::Mat(m_nbMesTotal, 1, m_trainingLabels.type());
    
    // Copier les données d'apprentissage
    m_trainingData.copyTo(m_allData(cv::Rect(0, 0, m_nbParametres, m_nbMesTotalApp)));
    m_trainingLabels.copyTo(m_allLabels(cv::Rect(0, 0, 1, m_nbMesTotalApp)));
    
    // Copier les données de test s'il y en a
    if (m_nbMesTotalTest > 0) {
        m_testData.copyTo(m_allData(cv::Rect(0, m_nbMesTotalApp, m_nbParametres, m_nbMesTotalTest)));
        m_testLabels.copyTo(m_allLabels(cv::Rect(0, m_nbMesTotalApp, 1, m_nbMesTotalTest)));
    }
}

void CDonneeFloatMod::RedistribCV(int cv, int iter)
{
    if (m_nbMesTotal == 0 || cv <= 1) {
        return;
    }
    
    // Réinitialiser les compteurs
    m_nbMesTotalApp = 0;
    m_nbMesTotalTest = 0;
    
    // Trier les indices par classe
    std::vector<std::vector<int>> indicesByClass(m_nbClasses);
    for (int i = 0; i < m_nbMesTotal; i++) {
        int label;
        if (m_allLabels.type() == CV_32S) {
            label = m_allLabels.at<int>(i, 0);
        } else {
            label = 0; // Pour la régression
        }
        indicesByClass[label].push_back(i);
    }
    
    // Pour chaque classe, diviser les données en cv partitions
    std::vector<std::vector<std::vector<int>>> partitions(m_nbClasses);
    
    for (int c = 0; c < m_nbClasses; c++) {
        int nbTotal = indicesByClass[c].size();
        int nbPerPart = nbTotal / cv;
        int rest = nbTotal % cv;
        
        partitions[c].resize(cv);
        
        int idx = 0;
        for (int p = 0; p < cv; p++) {
            int nbInPart = nbPerPart + (p < rest ? 1 : 0);
            
            for (int i = 0; i < nbInPart && idx < nbTotal; i++, idx++) {
                partitions[c][p].push_back(indicesByClass[c][idx]);
            }
        }
    }
    
    // Déterminer la partition de test pour cette itération
    int testPart = iter % cv;
    
    // Compter le nombre total d'échantillons d'apprentissage et de test
    m_nbMesTotalApp = 0;
    m_nbMesTotalTest = 0;
    
    for (int c = 0; c < m_nbClasses; c++) {
        for (int p = 0; p < cv; p++) {
            if (p == testPart) {
                m_nbMesTestParClasse[c] = partitions[c][p].size();
                m_nbMesTotalTest += m_nbMesTestParClasse[c];
            } else {
                m_nbMesAppParClasse[c] += partitions[c][p].size();
                m_nbMesTotalApp += partitions[c][p].size();
            }
        }
    }
    
    // Créer les matrices pour les données d'apprentissage et de test
    m_trainingData = cv::Mat(m_nbMesTotalApp, m_nbParametres, CV_32F);
    m_trainingLabels = cv::Mat(m_nbMesTotalApp, 1, m_allLabels.type());
    m_testData = cv::Mat(m_nbMesTotalTest, m_nbParametres, CV_32F);
    m_testLabels = cv::Mat(m_nbMesTotalTest, 1, m_allLabels.type());
    
    // Remplir les matrices
    int trainIdx = 0;
    int testIdx = 0;
    
    for (int c = 0; c < m_nbClasses; c++) {
        for (int p = 0; p < cv; p++) {
            for (int idx : partitions[c][p]) {
                if (p == testPart) {
                    // Donnée de test
                    m_allData.row(idx).copyTo(m_testData.row(testIdx));
                    m_allLabels.row(idx).copyTo(m_testLabels.row(testIdx));
                    testIdx++;
                } else {
                    // Donnée d'apprentissage
                    m_allData.row(idx).copyTo(m_trainingData.row(trainIdx));
                    m_allLabels.row(idx).copyTo(m_trainingLabels.row(trainIdx));
                    trainIdx++;
                }
            }
        }
    }
}

void CDonneeFloatMod::RedistribAppTest(int Na)
{
    if (m_nbMesTotal == 0 || Na <= 0 || Na >= m_nbMesTotal) {
        return;
    }
    
    // Réinitialiser les compteurs
    m_nbMesTotalApp = Na;
    m_nbMesTotalTest = m_nbMesTotal - Na;
    
    // Créer les matrices pour les données d'apprentissage et de test
    m_trainingData = cv::Mat(m_nbMesTotalApp, m_nbParametres, CV_32F);
    m_trainingLabels = cv::Mat(m_nbMesTotalApp, 1, m_allLabels.type());
    m_testData = cv::Mat(m_nbMesTotalTest, m_nbParametres, CV_32F);
    m_testLabels = cv::Mat(m_nbMesTotalTest, 1, m_allLabels.type());
    
    // Mélanger les données avant de les diviser
    RedistribAleat();
    
    // Remplir les matrices
    for (int i = 0; i < Na; i++) {
        m_allData.row(i).copyTo(m_trainingData.row(i));
        m_allLabels.row(i).copyTo(m_trainingLabels.row(i));
    }
    
    for (int i = 0; i < m_nbMesTotalTest; i++) {
        m_allData.row(Na + i).copyTo(m_testData.row(i));
        m_allLabels.row(Na + i).copyTo(m_testLabels.row(i));
    }
    
    // Mettre à jour les compteurs par classe
    std::fill(m_nbMesAppParClasse.begin(), m_nbMesAppParClasse.end(), 0);
    std::fill(m_nbMesTestParClasse.begin(), m_nbMesTestParClasse.end(), 0);
    
    for (int i = 0; i < m_nbMesTotalApp; i++) {
        int label;
        if (m_trainingLabels.type() == CV_32S) {
            label = m_trainingLabels.at<int>(i, 0);
        } else {
            label = 0; // Pour la régression
        }
        m_nbMesAppParClasse[label]++;
    }
    
    for (int i = 0; i < m_nbMesTotalTest; i++) {
        int label;
        if (m_testLabels.type() == CV_32S) {
            label = m_testLabels.at<int>(i, 0);
        } else {
            label = 0; // Pour la régression
        }
        m_nbMesTestParClasse[label]++;
    }
}

void CDonneeFloatMod::InitMem(int nbpar, int nbcl, int nbmes)
{
    Clear();
    
    m_nbParametres = nbpar;
    m_nbClasses = nbcl;
    m_nbMesTotal = nbmes;
    
    m_allData = cv::Mat(m_nbMesTotal, m_nbParametres, CV_32F);
    m_allLabels = cv::Mat(m_nbMesTotal, 1, CV_32F);
    
    m_nbMesAppParClasse.resize(m_nbClasses, 0);
    m_nbMesOrigParClasse.resize(m_nbClasses, 0);
    m_nbMesTestParClasse.resize(m_nbClasses, 0);
    std::fill(m_nbMesAppParClasse.begin(), m_nbMesAppParClasse.end(), 0);
    std::fill(m_nbMesOrigParClasse.begin(), m_nbMesOrigParClasse.end(), 0);
    std::fill(m_nbMesTestParClasse.begin(), m_nbMesTestParClasse.end(), 0);

   // m_kmaxParClasse.resize(m_nbClasses, 0);
}

void CDonneeFloatMod::InitMemTab(int nbpar, int nbcl, const std::vector<int>& nbmes, const std::vector<int>& nbmestest)
{
    Clear();
    
    m_nbParametres = nbpar;
    m_nbClasses = nbcl;
    
    // Initialiser les compteurs de mesures
    m_nbMesAppParClasse.resize(m_nbClasses, 0);
    m_nbMesOrigParClasse.resize(m_nbClasses, 0);
    m_nbMesTestParClasse.resize(m_nbClasses, 0);
    std::fill(m_nbMesAppParClasse.begin(), m_nbMesAppParClasse.end(), 0);
    std::fill(m_nbMesOrigParClasse.begin(), m_nbMesOrigParClasse.end(), 0);
    std::fill(m_nbMesTestParClasse.begin(), m_nbMesTestParClasse.end(), 0);
   // m_kmaxParClasse.resize(m_nbClasses, 0);
    
    // Compter le nombre total de mesures
    m_nbMesTotalApp = 0;
    m_nbMesTotalTest = 0;
    
    for (int c = 0; c < m_nbClasses && c < nbmes.size(); c++) {
        m_nbMesAppParClasse[c] = nbmes[c];
        m_nbMesOrigParClasse[c] = nbmes[c];
        m_nbMesTotalApp += nbmes[c];
    }
    
    // Initialiser les compteurs de test si fournis
    if (!nbmestest.empty()) {
        for (int c = 0; c < m_nbClasses && c < nbmestest.size(); c++) {
            m_nbMesTestParClasse[c] = nbmestest[c];
            m_nbMesTotalTest += nbmestest[c];
        }
    }
    
    m_nbMesTotal = m_nbMesTotalApp + m_nbMesTotalTest;
    
    // Allouer les matrices
    m_allData = cv::Mat(m_nbMesTotal, m_nbParametres, CV_32F);
    m_allLabels = cv::Mat(m_nbMesTotal, 1, CV_32S);
    
    if (m_nbMesTotalApp > 0) {
        m_trainingData = cv::Mat(m_nbMesTotalApp, m_nbParametres, CV_32F);
        m_trainingLabels = cv::Mat(m_nbMesTotalApp, 1, CV_32S);
    }
    
    if (m_nbMesTotalTest > 0) {
        m_testData = cv::Mat(m_nbMesTotalTest, m_nbParametres, CV_32F);
        m_testLabels = cv::Mat(m_nbMesTotalTest, 1, CV_32S);
    }
}

void CDonneeFloatMod::NormaliseVisu()
{
    // Cette fonction normalise les données pour la visualisation
    // Dans la version modernisée, nous pouvons utiliser des techniques OpenCV
    
    if (m_visuTest)
    {
        if (!m_testData.empty()) {

            m_VisuData = m_testData.clone(); // Copie des données d'entrée
            for (int i = 0; i < m_testData.cols; i++) {
                cv::normalize(m_testData.col(i), m_VisuData.col(i), 0, 1, cv::NORM_MINMAX);
            }
        }
    }
    else
    {
        if (!m_trainingData.empty()) {

            m_VisuData = m_trainingData.clone(); // Copie des données d'entrée
            for (int i = 0; i < m_trainingData.cols; i++) {
                cv::normalize(m_trainingData.col(i), m_VisuData.col(i), 0, 1, cv::NORM_MINMAX);
            }
        }
    }
}

void CDonneeFloatMod::Repartition(int proportion, bool inverse, bool firstHalf, const std::string& nomFichier)
{
    // Répartit les données selon les proportions indiquées et sauvegarde la répartition
    SplitData(proportion, inverse, firstHalf);
    
    if (!nomFichier.empty()) {
        std::ofstream file(nomFichier);
        if (file.is_open()) {
            file << "# Répartition des données" << std::endl;
            file << "# Nombre total d'échantillons: " << m_nbMesTotal << std::endl;
            file << "# Nombre d'échantillons d'apprentissage: " << m_nbMesTotalApp << std::endl;
            file << "# Nombre d'échantillons de test: " << m_nbMesTotalTest << std::endl;
            file << "# " << std::endl;
            file << "# Classe\tNbTotal\tNbApp\tNbTest" << std::endl;
            
            for (int c = 0; c < m_nbClasses; c++) {
                file << c << "\t" 
                     << m_nbMesOrigParClasse[c] << "\t"
                     << m_nbMesAppParClasse[c] << "\t"
                     << m_nbMesTestParClasse[c] << std::endl;
            }
        }
    }
}

void CDonneeFloatMod::CreerFichiersCMCEtDat(const std::string& nomFichier)
{
    // Cette fonction est utilisée pour créer des fichiers de configuration pour d'autres outils
    // Dans la version modernisée, nous adaptons cette fonction aux nouvelles structures de données

    // Fichier .dat
    std::string NomFich = nomFichier.substr(0, nomFichier.rfind('.')) + "_dataclasse_";

    for (int classLabel = 0; classLabel < m_nbClasses; ++classLabel)
    {
        std::ostringstream filename;
        filename << NomFich << classLabel << ".dat";
        std::ofstream datFile(filename.str());
        if (!datFile.is_open()) return;
        // Écrire l'en-tête
        datFile << m_nbParametres << "\n" << m_nbMesOrigParClasse[classLabel] << std::endl;
        // Écrire les données
        for (int i = 0; i < m_nbMesTotal; i++)
        {
            int label;
            if (m_allLabels.type() == CV_32S) {
                label = m_allLabels.at<int>(i, 0);
            }
            else {
                // Pour la régression, on convertit en classe entière
                label = static_cast<int>(m_allLabels.at<float>(i, 0));
            }
            if (label == classLabel)
            {
                for (int j = 0; j < m_nbParametres; j++) {
                    datFile << m_allData.at<float>(i, j) << " ";
                }
                datFile << std::endl;
            }
        }
    } // end for classes
    
    std::ofstream fichierCMC(nomFichier);
    if (!fichierCMC.is_open())
    {
        MessageBox(NULL, "Impossible de créer le fichier CMC.", "Erreur", MB_OK);
        return;
    }

    // Écrire le nombre de classes
    fichierCMC << m_nbClasses << "\n";

    // Écrire le nombre de fichiers par classe
    for (unsigned int c = 0; c < m_nbClasses; c++)
    {
        fichierCMC << 1 << "\n";
    }

    // Écrire les noms des fichiers pour chaque classe
    for (unsigned int c = 0; c < m_nbClasses; c++)
    {
        fichierCMC << NomFich << c << ".dat\n";
    }

    fichierCMC.close();
}

