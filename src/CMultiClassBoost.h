#pragma once

#include <opencv2/opencv.hpp>
#include <opencv2/ml.hpp>
#include <map>
#include <vector>
#include <string>

using namespace cv;
using namespace cv::ml;

class MultiClassBoost : public cv::Algorithm {
public:
    // Constructeur
    MultiClassBoost() {}
    virtual ~MultiClassBoost() {}
    int m_boosttype;
    int m_weakcount;
    int m_MaxDepth;
    int m_nbfeatures;
    // Méthode statique create()
    static Ptr<MultiClassBoost> create() 
    {
        return Ptr<MultiClassBoost>(new MultiClassBoost());
    }
    void InitParam(int BoostType = Boost::REAL, int WeakCount = 100, int MaxDepth = 3)
    {
         m_boosttype= BoostType;
         m_weakcount= WeakCount;
         m_MaxDepth= MaxDepth;
         m_nbfeatures = 0;
    }


    // Entraînement avec One-vs-All
    bool train(const Mat& trainData, const Mat& labels,int NbClass) {
        // Vérification des dimensions
        if (trainData.rows != labels.rows) {
            CV_Error(Error::StsBadArg, "trainData and labels must have the same number of rows.");
        }

        numClasses = NbClass;
        classModels.clear();
        m_nbfeatures = trainData.cols;
       
        // Entraîner un modèle par classe
        for (int c = 0; c < numClasses;c++) 
        {
            Mat binaryLabels(labels.rows, 1, CV_32SC1);
            for (int i = 0; i < labels.rows; ++i) {
                binaryLabels.at<int>(i, 0) = (labels.at<int>(i, 0) == c) ? 1 : -1;
            }

            Ptr<Boost> model = Boost::create();
            model->setBoostType(m_boosttype);
            model->setWeakCount(m_weakcount);
            model->setMaxDepth(m_MaxDepth);
            model->train(trainData, ROW_SAMPLE, binaryLabels);
            classModels.push_back(model);
        }

        return true;
    }

    bool TrainBoostIterative(Mat& trainData, Mat& labels, int NbClass, int maxIterations)
    {
        if (trainData.rows != labels.rows) {
            CV_Error(Error::StsBadArg, "trainData and labels must have the same number of rows.");
        }

        numClasses = NbClass;
        classModels.clear();

        // Entraîner un modèle par classe
        for (int c = 0; c < numClasses; c++)
        {
            Mat binaryLabels(labels.rows, 1, CV_32SC1);
            for (int i = 0; i < labels.rows; ++i) {
                binaryLabels.at<int>(i, 0) = (labels.at<int>(i, 0) == c) ? 1 : -1;
            }

            Ptr<Boost> model = Boost::create();
            model->setBoostType(m_boosttype);
            model->setWeakCount(maxIterations);
            model->setMaxDepth(m_MaxDepth);
            model->train(trainData, ROW_SAMPLE, binaryLabels);
            classModels.push_back(model);
        }

        return true;
    }


    // Prédiction pour un échantillon
    int predict(const Mat& sample) const {
        if (classModels.empty()) {
            CV_Error(Error::StsError, "The model has not been trained.");
        }

        float maxConfidence = -std::numeric_limits<float>::max();
        int bestClass = 0;
        int cl = 0;
        // Appliquer chaque modèle et choisir celui avec la meilleure confiance
        for (const auto& model : classModels) {
            float response = model->predict(sample);
            if (response > maxConfidence) {
                maxConfidence = response;
                bestClass = cl;
            }
            cl++;
        }
       return bestClass;
    }

    void save(const std::string& filename) const {
        FileStorage fs(filename, FileStorage::WRITE);
        if (!fs.isOpened()) {
            CV_Error(Error::StsError, "Could not open file to save model.");
        }

        fs << "numClasses" << numClasses;

        // Sauvegarder les modèles
        int cl = 0;
        for (const auto&  model : classModels) {
            std::string modelName = filename+"_class_" + std::to_string(cl);
            model->save(modelName);
            cl++;
        }

        fs.release();
    }

    void load(const std::string& filename) 
    {
        FileStorage fs(filename, FileStorage::READ);
        if (!fs.isOpened()) {
            CV_Error(Error::StsError, "Could not open file to load model.");
        }

        fs["numClasses"] >> numClasses;

        // Lire les modèles
        classModels.clear();
        for (int i = 0; i < numClasses; ++i) {
            std::string modelName = filename+"_class_" + std::to_string(i);
            Ptr<Boost> model = Boost::load(modelName);
            m_nbfeatures = model->getVarCount();
            classModels.push_back(model);
        }

        fs.release();
    }
    // Autres méthodes requises pour cv::Algorithm
    void write(FileStorage& fs) const override {
        //save(fs.getFileName()); 
        int kk = 0;
    }

    void read(const FileNode& fn) override {
        load(fn.name());
    }

private:
    int numClasses = 0;                              // Nombre de classes
    std::vector<Ptr<Boost>> classModels;           // Modèles pour chaque classe
   
};
