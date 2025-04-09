// ChildFrm.h : interface of the CChildFrame class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_CHILDFRM_H__288929A7_2F5A_11D5_879D_0050043AFEB2__INCLUDED_)
#define AFX_CHILDFRM_H__288929A7_2F5A_11D5_879D_0050043AFEB2__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


#include "DonneeFloatMod.h"
#include "svmparametres1.h"
#include "ConfigNeurone.h"

#include "CDialSlider.h"
#include "DaliogProgression.h"	// Added by ClassView

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/ml.hpp>
#include <vtkCallbackCommand.h>
#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkActor.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolyData.h>
#include <vtkPoints.h>
#include <vtkUnsignedCharArray.h>
#include <vtkImageData.h>
#include <vtkDiscreteMarchingCubes.h>
#include <vtkLookupTable.h>
#include <vtkCommand.h>


#include <direct.h>
#include <io.h>
//#include "ima.h"


using namespace cv;
using namespace cv::ml;
using namespace std;

enum TypeClassif
{
	mSVM,
	mKPPV,
	mRF,
	mMLP,
	mNB,  // Naive Bayes
	mEM,  // Expectation-Maximization
	mDT   // Decision Tree
};

struct Point3D {
	double x, y, z;
	int classID;
};


struct SMetriques
{
	float ErreurGlobale, Accuracy, Rappel, Precision, F1score;
};




class CloseCallback : public vtkCommand
{
public:
	static CloseCallback* New() { return new CloseCallback; }

	void Execute(vtkObject* caller, unsigned long eventId, void*) override
	{
		if (eventId == vtkCommand::ExitEvent)
		{
			windowClosed = true;
		}
	}

	bool IsWindowClosed() const { return windowClosed; }

public:
	bool windowClosed = true;
};



class CChildFrame : public CMDIChildWnd
{
	DECLARE_DYNCREATE(CChildFrame)
public:
	CChildFrame();

// Attributes
public:
	vtkSmartPointer<vtkRenderer> m_renderer;
	vtkSmartPointer<vtkRenderWindow> m_render_window;
	vtkSmartPointer<vtkRenderWindowInteractor> m_render_window_interactor;
	// Points d'apprentissage
	vtkSmartPointer<vtkPolyData> m_polyData;
	vtkSmartPointer<vtkPoints> m_points;
	vtkSmartPointer<vtkFloatArray> m_colors;
	vtkSmartPointer<vtkPolyDataMapper> m_point_mapper;
	vtkSmartPointer<vtkActor> m_point_actor;
	// Frontière de classification
	vtkSmartPointer<vtkImageData> m_classificationGrid;
	vtkSmartPointer<vtkPolyDataMapper> m_contour_mapper;
	vtkSmartPointer<vtkActor> m_contour_actor;
	vtkSmartPointer<vtkDiscreteMarchingCubes> m_contour;
	vtkSmartPointer<vtkLookupTable> m_lookupTable;
	vtkSmartPointer<vtkCallbackCommand> m_rotationObserver;
	vtkSmartPointer<vtkLookupTable> m_lut_contour;

	void VtkRenderingThread();
	//std::thread m_vtkThread;
	//bool m_vtkRunning = false;
	//std::atomic<bool> m_stopVtkThread{ false };

	std::thread m_acquisitionThread;
	std::atomic<bool> m_stopAcquisition{ false };
	//void ImageAquisitionetvisu3dThread();
	//void StopAcquisition();
	//CIma* m_pima;
	std::vector<std::string> class_names;

	CloseCallback* m_closeCallback;
	//void GenerateFourierDescriptors(const Mat& image, int Numclasse);

	CSVMParametres* m_pSVMParamDlg;
	CDialSlider* m_pKPPVparamDlg;
	CConfigNeurone* m_pNNParamDlg;
	CString NomFichProj,NomFichAppr,NomFichParam,NomImageAppr;	
	CString NomFichMMC,NomFichCMC,NomFichPMC,NomFichMinMax;
	CString m_PathData,m_TexteTemps;
	LARGE_INTEGER LiDebut,LiFin,LiPerfFreq;

	 BOOL m_Inverse;
	 BOOL m_Random;
	 BOOL m_FirstHalf;
	 BOOL AppNeurone;

	//CDonneeFloat MesDatas;
	CDonneeFloatMod MesDatasMod;

	//cv::Ptr<cv::ml::TrainData> m_pTrainData;
	//int m_nbParametres;
	//int m_nbClasses;
	
	BOOL Visu,ValidPol;
	BOOL VisuNeuronePerceptron;
	BOOL ApprendPerceptron;
	BOOL ConfigPerceptron;
	int CoeffPoly;
	BOOL ParamCalcul;
	BOOL ValideMenu;
	BOOL AffectNonClas;
	BOOL Visumes;
	BOOL ValideDecision,ValideVisuMes;
	BOOL VisuTest;
	CRect rcClient;
	int WinRight,WinLeft,WinBottom,WinTop;
	float Ax,Ay,Bx,By;
	int param0, param1;
	float ErrorSVM;
	int IterBoost;
	std::string m_exePath, m_datasPath;
	int m_windowWidth = 800;
	int m_windowHeight = 800;
	bool m_animate3D = true;
	bool m_DataOnly = false;
	bool m_UpdateData = false;
	int m_NbThread=4;
	std::string m_TitreFen3D;
	std::vector<cv::Vec3b> colorPalette; // Palette de couleurs pour les classes

	cv::Ptr<cv::Algorithm> m_pClassifier;
	TypeClassif LastType;

	int Hparam;
	double Estimateur (int,int);


protected:
	void AxeGraph(CPaintDC&);

// Operations
public:
	void ParallelSegmentation(Mat& ImageSeg, const Mat& labImage, int wf, int Typeprocess);
	void ProcessImageSegment(Mat& ImageSeg, const Mat& labImage, int wf, int startRow, int endRow, std::mutex& imgMutex);
	//void ProcessImageSegmentFourier(Mat& ImageSeg, const Mat& labImage, int wf, int startRow, int endRow, std::mutex& imgMutex);
	std::vector<int> AssignClustersToClasses(const cv::Mat& confusionMatrix);
	std::vector<int> clusterToClass;
	void SaveConfig(const std::string& filename);
	void LoadConfig(const std::string& filename);
	double m_opacity = 0.7;
	double m_cube_size=800;
	int m_grid_size=80;
	double m_angleAzimuth = 0.7;  // Vitesse de rotation horizontale
	double m_angleElevation = 0.25; // Vitesse de rotation verticale
	double m_elevationSum = 0.0;  // Somme des élévations pour limiter l'angle
	void StopRotation();
	void StartRotation();
	void processGridSection(int startZ, int endZ, int gridSize, double step, double cubeSize,int NPAR, TypeClassif TypeClassifieur, void* ptr, vtkSmartPointer<vtkIntArray> finalScalarData, std::mutex& dataMutex);
	void GenerateGaussianPoints(vtkSmartPointer<vtkPoints> points, vtkSmartPointer<vtkFloatArray> colors,double cx, double cy, double cz,
		double sigma, int numPoints, int colorId);
	void GenerateSpiralPoints(vtkSmartPointer<vtkPoints> points, vtkSmartPointer<vtkFloatArray> colors, double phase, double diameter, double length, int turns,
		double noiseStdDev, int numPoints, double x0, double y0, double z0, int colorId);
	void SavePointsToCSV(const std::string& filename, vtkSmartPointer<vtkPoints> points, vtkSmartPointer<vtkFloatArray> colors);
	void ParametreGraphique();
	int PosGrX(float);
	int PosGrY(float);
	SMetriques Evaluation(void* ptr, TypeClassif TypeClassifier, bool save=true );
	void VisufrontiereOpencv(void* ptr, TypeClassif TypeClassifieur,CPaintDC &dcp);
	void Visu3d(void* ptr, TypeClassif TypeClassifieur);
	float Decide(void* ptr, TypeClassif TypeClassifieur, cv::Mat &sampleMat);
	void DecidDataOpencv(void* ptr, CDonneeFloatMod &data, std::ofstream &FicRes, TypeClassif TypeClassifieur);
	void UpdateSVMParameters(float gammaValue, CSVMParametres* pboitesvm);
	void UpdateKPPVParameters(int KValue,  CDialSlider* ps);
	void UpdateNNParameters(CConfigNeurone* pnn);
	void Cleannonmodal(bool release=false);
	bool LoadClassifier(TypeClassif tc);
	bool LoadClassifier();
	void CrossValidateSVM(int k);
	//void InitialisationVTK();
	void CleanUpVTK();
	void RotateScene();
	CString LoadData(CDonneeFloatMod& data);
	void trainSVM(float g,bool Tauto);
	bool EndTraining;
	bool m_VisuDynamique;
	cv::Mat m_trainData ;
	cv::Mat m_Label ;
	CBitmap hbmScreen;
	cv::Mat OneHot;
	bool m_Loaded;
	bool m_Done;
	bool VisuCamLive;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CChildFrame)
	public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
	//}}AFX_VIRTUAL

// Implementation
public:


	void SauveTexte(CString NomFich,CString Chaine);
	void FinTps(DWORD ntot);
	void InitTps();
	CDaliogProgression* Fenetre_Progression;
	BOOL GenereGauss;
	CPoint Centre,OldPoint;
	int Rayon;
	BOOL BoutonDroit;
	BOOL AffBitmap;
	int m_Proportion;
	BOOL VisuNeurone;

	void AfficheLegende(CPaintDC& dc, int Larg, int Haut, int decalage);
	void AfficheSample(void *ptr,int typeptr ,  int Larg, int Haut, int decalage, bool berase);
	// view for the client area of the frame.
//	CChildView m_wndView;
	virtual ~CChildFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

// Generated message map functions
protected:
	//{{AFX_MSG(CChildFrame)

	afx_msg void OnClose();
	afx_msg void OnFileClose();
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnConfigurationFichiermulticlasse();
	//afx_msg void OnConfigurationVisualisationdesmesures();
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnConfigurationChoixdesparamtres();
	afx_msg void OnConfigurationFichierminmaxdenormalisation();
	afx_msg void OnConfigurationInfosurlesmesures();
	afx_msg void OnConfigurationNormalise();
	afx_msg void OnFentreRafraichir();
	afx_msg void OnFentreMaximize();
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnEditionGaussiennes2d();
//	afx_msg void OnMenuCreationFichierAppDesc();
	afx_msg void OnSizing(UINT nSide, LPRECT lpRect);
	afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
;

	afx_msg void OnConfigurationVisualisationdesTest();
	afx_msg void OnSvmSvmopencvtrainauto();
	afx_msg void OnSvmSvmopencvvisufrontiere();
	afx_msg void OnSvmSvmopencvevaluation();
	afx_msg void OnKppvKppvopencvapprentissage();
	afx_msg void OnKppvKppvopencvvisufrontiere();
	afx_msg void OnKppvKppvopencvevaluation();
	afx_msg void OnRandomforestRandomforestopencvapprentissage();
	afx_msg void OnRandomforestRandomforestopencvvisufrontiere();
	afx_msg void OnRandomforestRandomforestevaluation();
	afx_msg void OnPerceptronMlpopencvapprentissage();
	afx_msg void OnPerceptronMlpopencvvisufrontiere();
	afx_msg void OnPerceptronMlpopencvevaluation();
	afx_msg void OnConfigurationSauverauformatcsv();
	afx_msg void OnConfigurationLireauformatcsv();
	afx_msg void OnSvmSvmopencvdecisionsurfichierdedonnee();
	afx_msg void OnKppvKppvopencvdecisionsurfichierdedonnees();
	afx_msg void OnPerceptronMlpopencvdecisionsurfichierdedonnees();
	afx_msg void OnRandomforestRandomforestopencvdecisionfichierdedonnee();
	afx_msg void OnSvmSvmopencvtraindynamique();
	afx_msg void OnConfigurationRegressionLireCsv();
	afx_msg void OnSvmSvmopencvregression();
	afx_msg void OnKppvKppvopencvapprentissagedynamique();
	afx_msg void OnNaivebayesApprentissage();
	afx_msg void OnNaivebayesVisufrontiere();
	afx_msg void OnNaivebayesEvaluation();
	afx_msg void OnEmApprentissage();
	afx_msg void OnEmVisufrontiere();
	afx_msg void OnEmEvaluation();
	afx_msg void OnDtreesApprentissage();
	afx_msg void OnDtreesVisufrontiere();
	afx_msg void OnDtreesEvaluation();
	afx_msg void OnNaivebayesDecisionsurfichierdedonnees();
	afx_msg void OnBoostingopencvDecisionsurfichierdedonnees();
	afx_msg void OnKppvKppvopencvregression();
	afx_msg void OnRandomforestRandomforestregression();
	afx_msg void OnPerceptronApprentissagedynamique();
	afx_msg void OnPerceptronMlpenregression();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnEditionGaussiennes3d();
	afx_msg void OnConfigurationParametresLog();
	afx_msg void OnEditionSpirales3d();
	afx_msg void OnBoostingopencvUneiteration();
	afx_msg void OnBoostingopencvReset();
	afx_msg void OnSvmValidationcroisee();
	afx_msg void OnEmEnregistrerlesdonneesclasseesdansuncsv();
	afx_msg void OnImageAquisitionetvisu3d();
	afx_msg void OnImageSegmentationlab();
	afx_msg void OnImageApprentissageviaimagefixe();
	//afx_msg void OnImageApprentissagedescripteursdefourier();
	//afx_msg void OnImageApprentissagedescripteursdefourierfixe();
	//afx_msg void OnImageClasseuneimageavecdesc();
	//afx_msg void OnImageApprendunelistedimages();
	//afx_msg void OnImageDecisionsurunelistedimages();

};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CHILDFRM_H__288929A7_2F5A_11D5_879D_0050043AFEB2__INCLUDED_)
