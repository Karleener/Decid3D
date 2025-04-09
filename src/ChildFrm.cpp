// ChildFrm.cpp : implementation of the CChildFrame class
//
//#define vtkRenderingContext2D_AUTOINIT 1(vtkRenderingContextOpenGL2)
//#define vtkRenderingCore_AUTOINIT 3(vtkInteractionStyle,vtkRenderingFreeType,vtkRenderingOpenGL2)
//#define vtkRenderingOpenGL2_AUTOINIT 1(vtkRenderingGL2PSOpenGL2)
#include "stdafx.h"

#include <vtkDiscreteMarchingCubes.h>
#include <vtkNamedColors.h>
#include <vtkObjectFactory.h>
#include <vtkLogger.h>
#include <vtkCamera.h>

#include <vtkNew.h>
#include <vtkProperty.h>
#include <vtkPointData.h>
#include <vtkCellArray.h>
#include <vtkVertexGlyphFilter.h>

#include <vtkPolyDataAlgorithm.h>
#include <vtkFloatArray.h>
#include <vtkAxesActor.h>
#include <vtkOrientationMarkerWidget.h>
#include <vtkWindowToImageFilter.h>
#include <vtkPNGWriter.h>
#include <vtkSmoothPolyDataFilter.h>
#include <vtkResampleToImage.h>
#include <vtkContourFilter.h>
#include <vtkSubdivisionFilter.h>
#include <vtkLoopSubdivisionFilter.h>
#include <vtkWindowedSincPolyDataFilter.h>
#include <vtkCleanPolyData.h>


#include "math.h"
#include "winbase.h" 
#include "Decid.h"
#include "dialvam.h"
#include "dialconfigpoly.h"
#include "dialboosting.h"
#include "dialgauss1.h"
#include "CDialConfigGen.h"


#include "DonneeFloatMod.h"
#include "ChildFrm.h"

#include "Dialog2Val.h"
#include "CDialSegmentationLive.h"
#include "ConfigNeurone.h"
#include "demchaine.h"
#include "SVMParametres1.h"
#include "courbe.h"
#include "gengauss.h"
#include "dialresultimplan.h"

#include "conio.h"

#include "CRandomForestSettingsDialog.h"
#include "CFileDialogCustom.h"
#include "CMultiClassBoost.h"


#include <mutex>
#include <random>
#include <filesystem> 
#include <numeric>  // Pour std::accumulate
#include <map>
#include <algorithm>
#include <opencv2/dnn.hpp>



using namespace cv;
using namespace cv::ml;
using namespace std;
using namespace cv::dnn;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CString ChaineModel = "";

std::mutex fenetreMutex;

/////////////////////////////////////////////////////////////////////////////
// CChildFrame

IMPLEMENT_DYNCREATE(CChildFrame, CMDIChildWnd)

BEGIN_MESSAGE_MAP(CChildFrame, CMDIChildWnd)
	//{{AFX_MSG_MAP(CChildFrame)
	ON_COMMAND(ID_FILE_CLOSE, OnFileClose)
	ON_WM_SETFOCUS()
	ON_WM_CREATE()
	ON_WM_CLOSE()
	ON_WM_PAINT()
	//ON_WM_SIZE()
	ON_WM_SIZING()
	ON_WM_TIMER()
	ON_WM_GETMINMAXINFO()
	ON_COMMAND(ID_CONFIGURATION_CHOIXDESPARAMTRES, OnConfigurationChoixdesparamtres)
	ON_COMMAND(ID_CONFIGURATION_FICHIERMINMAXDENORMALISATION, OnConfigurationFichierminmaxdenormalisation)
	ON_COMMAND(ID_CONFIGURATION_INFOSURLESMESURES, OnConfigurationInfosurlesmesures)
	ON_COMMAND(ID_CONFIGURATION_FICHIERMULTICLASSE, OnConfigurationFichiermulticlasse)
	//	ON_COMMAND(ID_CONFIGURATION_VISUALISATIONDESMESURES, OnConfigurationVisualisationdesmesures)
	ON_COMMAND(ID_CONFIGURATION_NORMALISE, OnConfigurationNormalise)

	ON_COMMAND(ID_FENTRE_RAFRAICHIR, OnFentreRafraichir)
	//ON_COMMAND(ID_FENTRE_MAXIMIZE, OnFentreMaximize)
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_MOUSEMOVE()

	ON_COMMAND(ID_EDITION_GAUSSIENNES2D, OnEditionGaussiennes2d)
	//}}AFX_MSG_MAP
	ON_COMMAND(ID_CONFIGURATION_VISUALISATIONDES32845, &CChildFrame::OnConfigurationVisualisationdesTest)
	ON_COMMAND(ID_SVM_SVMOPENCVTRAINAUTO, &CChildFrame::OnSvmSvmopencvtrainauto)
	ON_COMMAND(ID_SVM_SVMOPENCVVISUFRONTIERE, &CChildFrame::OnSvmSvmopencvvisufrontiere)
	ON_COMMAND(ID_SVM_SVMOPENCVEVALUATION, &CChildFrame::OnSvmSvmopencvevaluation)
	ON_COMMAND(ID_KPPV_KPPVOPENCVAPPRENTISSAGE, &CChildFrame::OnKppvKppvopencvapprentissage)
	ON_COMMAND(ID_KPPV_KPPVOPENCVVISUFRONTIERE, &CChildFrame::OnKppvKppvopencvvisufrontiere)
	ON_COMMAND(ID_KPPV_KPPVOPENCVEVALUATION, &CChildFrame::OnKppvKppvopencvevaluation)
	ON_COMMAND(ID_RANDOMFOREST_RANDOMFORESTOPENCVAPPRENTISSAGE, &CChildFrame::OnRandomforestRandomforestopencvapprentissage)
	ON_COMMAND(ID_RANDOMFOREST_RANDOMFORESTOPENCVVISUFRONTIERE, &CChildFrame::OnRandomforestRandomforestopencvvisufrontiere)
	ON_COMMAND(ID_RANDOMFOREST_RANDOMFORESTEVALUATION, &CChildFrame::OnRandomforestRandomforestevaluation)
	ON_COMMAND(ID_PERCEPTRON_MLPOPENCVAPPRENTISSAGE, &CChildFrame::OnPerceptronMlpopencvapprentissage)
	ON_COMMAND(ID_PERCEPTRON_MLPOPENCVVISUFRONTI32857, &CChildFrame::OnPerceptronMlpopencvvisufrontiere)
	ON_COMMAND(ID_PERCEPTRON_MLPOPENCVEVALUATION, &CChildFrame::OnPerceptronMlpopencvevaluation)
	ON_COMMAND(ID_CONFIGURATION_SAUVERAUFORMATCSV, &CChildFrame::OnConfigurationSauverauformatcsv)
	ON_COMMAND(ID_CONFIGURATION_LIREAUFORMATCSV, &CChildFrame::OnConfigurationLireauformatcsv)
	ON_COMMAND(ID_SVM_SVMOPENCVDECISIONSURFICHIERDEDONN32861, &CChildFrame::OnSvmSvmopencvdecisionsurfichierdedonnee)
	ON_COMMAND(ID_KPPV_KPPVOPENCVDECISIONSURFICHIERDEDONNEES, &CChildFrame::OnKppvKppvopencvdecisionsurfichierdedonnees)
	ON_COMMAND(ID_PERCEPTRON_MLPOPENCVDECISIONSURFICHIERDEDONNEES, &CChildFrame::OnPerceptronMlpopencvdecisionsurfichierdedonnees)
	ON_COMMAND(ID_RANDOMFOREST_RANDOMFORESTOPENCVDECISIONFICHIERDEDONNEE, &CChildFrame::OnRandomforestRandomforestopencvdecisionfichierdedonnee)
	ON_COMMAND(ID_SVM_SVMOPENCVTRAINDYNAMIQUE, &CChildFrame::OnSvmSvmopencvtraindynamique)
	ON_COMMAND(ID_CONFIGURATION_R32871, &CChildFrame::OnConfigurationRegressionLireCsv)
	ON_COMMAND(ID_SVM_SVMOPENCVREGRESSION, &CChildFrame::OnSvmSvmopencvregression)
	ON_COMMAND(ID_KPPV_KPPVOPENCVAPPRENTISSAGEDYNAMIQUE, &CChildFrame::OnKppvKppvopencvapprentissagedynamique)
	ON_COMMAND(ID_NAIVEBAYES_APPRENTISSAGE, &CChildFrame::OnNaivebayesApprentissage)
	ON_COMMAND(ID_NAIVEBAYES_VISUFRONTIERE, &CChildFrame::OnNaivebayesVisufrontiere)
	ON_COMMAND(ID_NAIVEBAYES_EVALUATION, &CChildFrame::OnNaivebayesEvaluation)
	ON_COMMAND(ID_EM_APPRENTISSAGE, &CChildFrame::OnEmApprentissage)
	ON_COMMAND(ID_EM_VISUFRONTIERE, &CChildFrame::OnEmVisufrontiere)
	ON_COMMAND(ID_EM_EVALUATION, &CChildFrame::OnEmEvaluation)
	ON_COMMAND(ID_DTREES_APPRENTISSAGE, &CChildFrame::OnDtreesApprentissage)
	ON_COMMAND(ID_DTREES_VISUFRONTIERE, &CChildFrame::OnDtreesVisufrontiere)
	ON_COMMAND(ID_DTREES_EVALUATION, &CChildFrame::OnDtreesEvaluation)
	ON_COMMAND(ID_NAIVEBAYES_DECISIONSURFICHIERDEDONNEES, &CChildFrame::OnNaivebayesDecisionsurfichierdedonnees)
	ON_COMMAND(ID_BOOSTINGOPENCV_DECISIONSURFICHIERDEDONNEES, &CChildFrame::OnBoostingopencvDecisionsurfichierdedonnees)
	ON_COMMAND(ID_KPPV_KPPVOPENCVREGRESSION, &CChildFrame::OnKppvKppvopencvregression)
	ON_COMMAND(ID_RANDOMFOREST_RANDOMFORESTREGRESSION, &CChildFrame::OnRandomforestRandomforestregression)
	ON_COMMAND(ID_PERCEPTRON_APPRENTISSAGEDYNAMIQUE, &CChildFrame::OnPerceptronApprentissagedynamique)
	ON_COMMAND(ID_PERCEPTRON_MLPENREGRESSION, &CChildFrame::OnPerceptronMlpenregression)
	ON_COMMAND(ID_EDITION_GAUSSIENNES3D, &CChildFrame::OnEditionGaussiennes3d)
	ON_COMMAND(ID_CONFIGURATION_PARAMETRES_LOG, &CChildFrame::OnConfigurationParametresLog)
	ON_COMMAND(ID_EDITION_SPIRALES3D, &CChildFrame::OnEditionSpirales3d)
	ON_COMMAND(ID_BOOSTINGOPENCV_UNEITERATION, &CChildFrame::OnBoostingopencvUneiteration)
	ON_COMMAND(ID_BOOSTINGOPENCV_RESET, &CChildFrame::OnBoostingopencvReset)
	ON_COMMAND(ID_SVM_VALIDATIONCROISEE32906, &CChildFrame::OnSvmValidationcroisee)
	ON_COMMAND(ID_EM_ENREGISTRERLESDONNEESCLASSEESDANSUNCSV, &CChildFrame::OnEmEnregistrerlesdonneesclasseesdansuncsv)
	ON_COMMAND(ID_IMAGE_AQUISITIONETVISU3D, &CChildFrame::OnImageAquisitionetvisu3d)
	ON_COMMAND(ID_IMAGE_SEGMENTATIONLAB, &CChildFrame::OnImageSegmentationlab)
	ON_COMMAND(ID_IMAGE_APPRENTISSAGEVIAIMAGEFIXE, &CChildFrame::OnImageApprentissageviaimagefixe)
	//ON_COMMAND(ID_IMAGE_APPRENTISSAGEDESCRIPTEURSDEFOURIER, &CChildFrame::OnImageApprentissagedescripteursdefourier)
	//ON_COMMAND(ID_IMAGE_APPRENTISSAGEDESCRIPTEURSDEFOURIERFIXE, &CChildFrame::OnImageApprentissagedescripteursdefourierfixe)
	//ON_COMMAND(ID_IMAGE_CLASSEUNEIMAGEAVECDESC, &CChildFrame::OnImageClasseuneimageavecdesc)
	//ON_COMMAND(ID_IMAGE_APPRENDUNELISTEDIMAGES, &CChildFrame::OnImageApprendunelistedimages)
	//ON_COMMAND(ID_IMAGE_DECISIONSURUNELISTEDIMAGES, &CChildFrame::OnImageDecisionsurunelistedimages)

END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CChildFrame construction/destruction

CChildFrame::CChildFrame()
{
	// TODO: add member initialization code here
	Visumes = FALSE;
	Visu = FALSE;
	VisuNeurone = FALSE;
	AppNeurone = FALSE;
	VisuNeuronePerceptron = FALSE;
	ConfigPerceptron = FALSE;
	ApprendPerceptron = FALSE;
	GenereGauss = false;
	BoutonDroit = FALSE;
	m_VisuDynamique = false;
	m_Done = true;
	VisuCamLive = false;
	param0 = 0;
	param1 = 1;
	NomFichMinMax = "";
	m_Proportion = 2;
	IterBoost = 1;
	AffBitmap = FALSE;

	m_Inverse = false;
	m_Random = false;
	m_FirstHalf = false;
	VisuTest = false; // dar defaut on visualise les points d'apprentisssage
	m_UpdateData = false;

	m_pClassifier = nullptr;
	m_pSVMParamDlg = nullptr;
	m_pKPPVparamDlg = nullptr;
	m_pNNParamDlg = nullptr;

	m_renderer == nullptr;
	m_closeCallback = nullptr;
	m_render_window_interactor = nullptr;
	m_Loaded = false;
	m_rotationObserver = nullptr;

	m_PathData = "";
	m_TexteTemps = "";

	m_cube_size = 800.0;
	m_grid_size = 80;
	m_TitreFen3D = "Frontieres de classification";
	m_NbThread = 4;
	LoadConfig("Decid.ini");
	//uchar Bleu, Vert, Rouge;

	for (int c = 0; c < 64; c++)
	{
		// Utilisation de modulation par 8 au lieu de 3 pour plus de variété
		int pattern = c % 8;

		// Valeurs de base pour chaque canal
		int Rouge = 0, Vert = 0, Bleu = 0;

		// Intensité variable basée sur la position
		int intensity = 255 - ((c / 8) * 32);
		intensity = std::max(intensity, 63); // Garantit une intensité minimale

		// Application de différents motifs pour plus de variété
		switch (pattern) {
		case 0: // Rouge pur
			Rouge = intensity;
			break;
		case 1: // Vert pur
			Vert = intensity;
			break;
		case 2: // Bleu pur
			Bleu = intensity;
			break;
		case 3: // Rouge + Vert (jaune)
			Rouge = intensity;
			Vert = intensity;
			break;
		case 4: // Rouge + Bleu (magenta)
			Rouge = intensity;
			Bleu = intensity;
			break;
		case 5: // Vert + Bleu (cyan)
			Vert = intensity;
			Bleu = intensity;
			break;
		case 6: // Rouge + Vert + Bleu (blanc/gris)
			Rouge = intensity;
			Vert = intensity;
			Bleu = intensity;
			break;
		case 7: // Couleur complémentaire
			Rouge = (c % 2) == 0 ? intensity : intensity / 2;
			Vert = (c % 3) == 0 ? intensity : intensity / 3;
			Bleu = (c % 4) == 0 ? intensity : intensity / 4;
			break;
		}

		colorPalette.push_back(Vec3b(Bleu, Vert, Rouge));
	}

	//colorPalette =
	//{
	//Vec3b(0, 0, 255),     // Rouge (format BGR)
	//Vec3b(0, 255, 0),     // Vert
	//Vec3b(255, 0, 0),     // Bleu
	//Vec3b(0, 255, 255),   // Jaune
	//Vec3b(255, 0, 255),   // Magenta
	//Vec3b(255, 255, 0),   // Cyan
	//Vec3b(128, 0, 255),   // Orange
	//Vec3b(0, 128, 255),   // Rose
	//Vec3b(255, 128, 0),   // Bleu clair
	//Vec3b(128, 255, 0)    // Vert clair
	//};
	//m_pima = new CIma(128);
}

CChildFrame::~CChildFrame()
{

	//m_render_window_interactor = nullptr; // Suffit pour libérer la mémoire
	//m_render_window = nullptr;
	//m_renderer = nullptr;

	CleanUpVTK();
	//if (m_closeCallback)
	//	if (!m_closeCallback->IsWindowClosed())
	//	{
	//		m_render_window_interactor->TerminateApp();
	//		m_render_window->Finalize();
	//		delete (m_closeCallback);
	//		m_closeCallback = nullptr;
	//	}
}

BOOL CChildFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	if (!CMDIChildWnd::PreCreateWindow(cs))
		return FALSE;
	cs.style &= ~WS_MAXIMIZEBOX;
	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
	cs.lpszClass = AfxRegisterWndClass(0);
	GenereGauss = false;
	CMDIFrameWnd* pMainFrame = (CMDIFrameWnd*)AfxGetMainWnd();
	if (pMainFrame)
	{
		CRect mainFrameRect;
		pMainFrame->GetClientRect(&mainFrameRect);

		// Taille de la fenêtre principale
		int mainFrameWidth = mainFrameRect.Width();
		int mainFrameHeight = mainFrameRect.Height();

		// Forcer la fenêtre à être carrée dès sa création
		int initialSize = 600; // Taille carrée par défaut
		cs.cx = initialSize;   // Largeur
		cs.cy = initialSize;   // Hauteur

		int childLeft = mainFrameRect.left + (mainFrameWidth - initialSize) / 2;
		int childTop = mainFrameRect.top + (mainFrameHeight - initialSize) / 2;
		cs.x = childLeft;
		cs.y = childTop;
	}

	return TRUE;
}

void CChildFrame::OnFentreRafraichir()
{
	// TODO: Add your command handler code here
	AffBitmap = FALSE;

	Invalidate(TRUE);
}

void CChildFrame::OnFentreMaximize()
{
	// TODO: Add your command handler code here
	ShowWindow(SW_SHOWMAXIMIZED);

}


/////////////////////////////////////////////////////////////////////////////
// CChildFrame diagnostics

#ifdef _DEBUG
void CChildFrame::AssertValid() const
{
	CMDIChildWnd::AssertValid();
}

void CChildFrame::Dump(CDumpContext& dc) const
{
	CMDIChildWnd::Dump(dc);
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CChildFrame message handlers
void CChildFrame::OnFileClose()
{
	// To close the frame, just send a WM_CLOSE, which is the equivalent
	// choosing close from the system menu.

	SendMessage(WM_CLOSE);
}



void CChildFrame::OnClose()
{
	// To close the frame, just send a WM_CLOSE, which is the equivalent
	// choosing close from the system menu.
	if (VisuCamLive)
	{
		VisuCamLive = false;
		cv::destroyAllWindows();
	}
	Cleannonmodal(true);

	CleanUpVTK();
	//if (m_closeCallback)
	//if (!m_closeCallback->IsWindowClosed())
	//{
	//	//m_render_window_interactor = nullptr;
	//	//m_render_window_interactor->TerminateApp();
	//	//m_render_window->Finalize();
	//	delete (m_closeCallback);
	//	m_closeCallback = nullptr;
	//	
	//}
	CMDIChildWnd::OnClose();

}

int CChildFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CMDIChildWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	// create a view to occupy the client area of the frame
/*
	if (!m_wndView.Create(NULL, NULL, AFX_WS_DEFAULT_VIEW,
		CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, NULL))
	{
		TRACE0("Failed to create view window\n");
		return -1;
	}
*/
	return 0;
}

void CChildFrame::OnSetFocus(CWnd* pOldWnd)
{
	CMDIChildWnd::OnSetFocus(pOldWnd);

	//	m_wndView.SetFocus();
}

BOOL CChildFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
	// let the view have first crack at the command
//	if (m_wndView.OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
//		return TRUE;

	// otherwise, do default handling
	return CMDIChildWnd::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}


void CChildFrame::OnConfigurationFichiermulticlasse()
{

	Visu = FALSE;
	AffBitmap = FALSE;

	CDialConfigPoly Dlg(this, NomFichCMC);

	if (Dlg.DoModal() == IDOK)
	{
		m_Inverse = Dlg.m_Inverse;
		m_Random = Dlg.m_Random;
		NomFichCMC = Dlg.NomFich;
		NomFichMMC = NomFichCMC.Left(NomFichCMC.ReverseFind('.')) + ".mcf";
		if (Dlg.m_Prop == 0) m_Proportion = 12;
		if (Dlg.m_Prop == 1) m_Proportion = 25;
		if (Dlg.m_Prop == 2) m_Proportion = 50;
		if (Dlg.m_Prop == 3) m_Proportion = 75;

		SetWindowText(NomFichMMC);
		std::string nomf(NomFichCMC);
		if (MesDatasMod.LireDonnee(nomf, m_Proportion, Dlg.m_Inverse, Dlg.m_Random, Dlg.m_FirstHalf) != 1)
		{
			MessageBox("erreur de fichier Data");
			return;
		}
		NomFichMinMax = NomFichCMC.Left(NomFichCMC.ReverseFind('.')) + ".lim";

		//		MesDatas.SauveMinMax(NomFichMinMax);
		//		MesDatas.NormaliseSimple(); // pour la visualisation
			//	MesDatas.NormaliseVisu();

		ValideDecision = FALSE;
		ValideMenu = FALSE;
		ValideVisuMes = TRUE;

	}
	Invalidate();

}



void CChildFrame::ParametreGraphique()
{
	int decalage = 20;
	int legende = 20;
	if (IsWindow(GetSafeHwnd())) GetClientRect(rcClient);
	WinRight = rcClient.Width() - decalage;
	WinLeft = decalage;
	WinBottom = rcClient.Height() - decalage;
	WinTop = decalage + legende; //20 pour légende                    
	Ay = (float)(WinTop - WinBottom);
	By = (float)WinBottom;

	Bx = (float)WinLeft;
	Ax = (float)(WinRight - WinLeft);
}


int CChildFrame::PosGrX(float X)
{
	return (int)(Ax * X + Bx);
}

int CChildFrame::PosGrY(float Y)
{
	return (int)(Ay * Y + By);
}

void CChildFrame::AxeGraph(CPaintDC& dc)
{

	CPen PenAxe(PS_SOLID, 1, RGB(0, 0, 0));
	dc.SelectObject(&PenAxe);
	dc.MoveTo(PosGrX(0), PosGrY(0));
	dc.LineTo(PosGrX(0), PosGrY(1));
	dc.MoveTo(PosGrX(0), PosGrY(0));
	dc.LineTo(PosGrX(1), PosGrY(0));

	for (int i = 1; i < 100; i += 1)
	{
		dc.MoveTo(PosGrX(0.0F), PosGrY((float)i / 100.0F));
		dc.LineTo(PosGrX(0.005F), PosGrY((float)i / 100.0F));
		dc.MoveTo(PosGrX((float)i / 100.0F), PosGrY(0.0F));
		dc.LineTo(PosGrX((float)i / 100.0F), PosGrY(0.005F));
	}
	DeleteObject(&PenAxe);
}

void CChildFrame::OnSize(UINT nType, int cx, int cy)
{
	CMDIChildWnd::OnSize(nType, cx, cy);
	if (IsWindow(GetSafeHwnd())) GetClientRect(rcClient);
	if ((rcClient.Height() < 230) || (rcClient.Width() < 200))
	{
		SetWindowPos(NULL, 0, 0, 200, 230, SWP_NOMOVE);       // SWP_NOMOVE        
		cx = 200; cy = 230;
	}

	//if (nType != SIZE_MINIMIZED)
	//{
	//	
	//	int newSize = max(cx, cy);
	//	SetWindowPos(nullptr, 0, 0, newSize, newSize,SWP_NOMOVE );
	//}
	AffBitmap = FALSE;
	Invalidate(TRUE);
}

void CChildFrame::AfficheLegende(CPaintDC& dc, int Larg, int Haut, int decalage)
{
	int cl;
	int Rouge, Vert, Bleu;
	//dc.FillSolidRect(decalage, 2 * decalage, Larg - 2 * decalage, Haut - 3 * decalage, RGB(255, 255, 255));
	dc.FillSolidRect(0, 0, Larg, 2 * decalage, RGB(240, 240, 240));
	dc.FillSolidRect(0, Haut - decalage, Larg, Haut, RGB(240, 240, 240));
	dc.FillSolidRect(0, 0, decalage, Larg, RGB(240, 240, 240));
	dc.FillSolidRect(Larg - decalage, 0, Larg, Haut, RGB(240, 240, 240));
	dc.SetBkMode(TRANSPARENT);
	if (!VisuTest) dc.TextOut(180, 0, "Training samples");
	else dc.TextOut(180, 0, "Test samples");
	dc.TextOut(20, 0, "Classe:");
	dc.SetBkMode(OPAQUE);
	//AxeGraph(dc);
	CPen Pen1(PS_SOLID, 1, RGB(0, 255, 0));
	dc.SelectObject(&Pen1);
	if (MesDatasMod.m_nbParametres == 1) { param1 = 0; param0 = 0; }
	CString TextClass;
	dc.SetTextColor(RGB(255, 255, 255));
	if (MesDatasMod.m_allData.empty()) return;
	for (cl = 0; cl < MesDatasMod.m_nbClasses; cl++)
	{
		//if ((cl % 3) == 0) Rouge = 255 - (16 * cl); else Rouge = 0;
		//if (((cl + 1) % 3) == 0) Vert = 255 - (16 * cl);  else Vert = 0;
		//if (((cl + 2) % 3) == 0) Bleu = 255 - (16 * cl); else Bleu = 0;
		if (cl < colorPalette.size())
		{
			Rouge = colorPalette[cl][2];
			Vert = colorPalette[cl][1];
			Bleu = colorPalette[cl][0];
		}
		else { Rouge = Vert = Bleu = 128; }


		dc.SetBkColor(RGB(Rouge, Vert, Bleu));
		TextClass.Format("%d", cl);
		dc.TextOut(30 + cl * 20, 20, TextClass);
	} // cl
}

void CChildFrame::AfficheSample(void* ptr, int typeptr, int Larg, int Haut, int decalage, bool berase = false)
{
	CPaintDC* ppdc = nullptr;
	CDC* pcdc = nullptr;
	if (typeptr == 0)
	{
		ppdc = (CPaintDC*)ptr;
		if (berase) ppdc->FillSolidRect(decalage, 2 * decalage, Larg - 2 * decalage, Haut - 3 * decalage, RGB(128, 128, 128));
	}
	else
	{
		pcdc = (CDC*)ptr;
		if (berase) pcdc->FillSolidRect(decalage, 2 * decalage, Larg - 2 * decalage, Haut - 3 * decalage, RGB(128, 128, 128));
	}
	for (int id = 0; id < MesDatasMod.m_VisuData.rows; id++)
	{
		int Rouge, Vert, Bleu;
		unsigned int c;
		if (!VisuTest) c = (unsigned int)MesDatasMod.m_trainingLabels.at<float>(id, 0);
		else c = (unsigned int)MesDatasMod.m_testLabels.at<float>(id, 0);
		if (MesDatasMod.m_nbClasses == 1) c = 0; // regression

		if (c < colorPalette.size())
		{
			Rouge = colorPalette[c][2];
			Vert = colorPalette[c][1];
			Bleu = colorPalette[c][0];
		}
		else { Rouge = Vert = Bleu = 128; }
		//if ((c % 3) == 0) Rouge = 255 - (8 * c); else Rouge = 0;
		//if (((c + 1) % 3) == 0) Vert = 255 - (8 * c); else Vert = 0;
		//if (((c + 2) % 3) == 0) Bleu = 255 - (8 * c); else Bleu = 0;
		CPen Stylo(PS_SOLID, 3, RGB(Rouge, Vert, Bleu));
		if (typeptr == 0) ppdc->SelectObject(&Stylo);
		else  pcdc->SelectObject(&Stylo);
		float cx = MesDatasMod.m_VisuData.at< float >(id, param0);
		float cy = MesDatasMod.m_VisuData.at< float >(id, param1);
		int taillepoint = 2;
		if (typeptr == 0)
		{
			if (c & 1) ppdc->FillSolidRect(PosGrX(cx), PosGrY(cy), taillepoint, taillepoint, RGB(Rouge, Vert, Bleu));
			else ppdc->Ellipse(PosGrX(cx), PosGrY(cy), PosGrX(cx) + taillepoint, PosGrY(cy) + taillepoint);
		}
		else
		{
			if (c & 1) pcdc->FillSolidRect(PosGrX(cx), PosGrY(cy), taillepoint, taillepoint, RGB(Rouge, Vert, Bleu));
			else pcdc->Ellipse(PosGrX(cx), PosGrY(cy), PosGrX(cx) + taillepoint, PosGrY(cy) + taillepoint);
		}
	}
}

void CChildFrame::OnPaint()
{
	CPaintDC dc(this); // device context for painting


	if (IsWindow(GetSafeHwnd())) GetClientRect(rcClient);
	int Larg = rcClient.Width();
	int Haut = rcClient.Height();
	int decalage = 20;
	if (MesDatasMod.m_nbParametres < 1)
	{
		dc.FillSolidRect(decalage, 2 * decalage, Larg - 2 * decalage, Haut - 3 * decalage, RGB(128, 128, 128));
		dc.FillSolidRect(0, 0, Larg, 2 * decalage, RGB(240, 240, 240));
		dc.FillSolidRect(0, Haut - decalage, Larg, Haut, RGB(240, 240, 240));
		dc.FillSolidRect(0, 0, decalage, Larg, RGB(240, 240, 240));
		dc.FillSolidRect(Larg - decalage, 0, Larg, Haut, RGB(240, 240, 240));
		dc.TextOut(decalage, 0, "Menu Configuration -> Lire données au format CSV (F3)");
		ReleaseDC(&dc);
		return;
	}

	ParametreGraphique();
	std::ostringstream  titre;
	titre << "Nombre d'attributs : " << MesDatasMod.m_nbParametres << "    Nb Echantillons d'apprentissage : " << MesDatasMod.m_nbMesTotalApp;
	SetWindowText(titre.str().c_str());

	if (m_pClassifier != nullptr)
	{
		VisufrontiereOpencv(&m_pClassifier, LastType, dc);
		AfficheLegende(dc, Larg, Haut, decalage);
	}
	else
	{
		///////////////////////////////////
		AfficheLegende(dc, Larg, Haut, decalage);

		AfficheSample(&dc, 0, Larg, Haut, decalage, true);

		if (AffBitmap)
		{
			CDC hdcComp;
			if (hdcComp.CreateCompatibleDC(&dc))
			{
				hdcComp.SelectObject(&hbmScreen);
				dc.BitBlt(0, 0, Larg, Haut, &hdcComp, 0, 0, SRCCOPY);
				ReleaseDC(&hdcComp);
			}
			ReleaseDC(&dc);
		}
	}

}


void CChildFrame::OnConfigurationChoixdesparamtres()
{
	// TODO: Add your command handler code here
	if (MesDatasMod.m_nbParametres == 1) { param1 = 0; param0 = 0; }
	CDialog2Val Dial2Val;
	Dial2Val.m_val0 = param0;
	Dial2Val.m_val1 = param1;


	if (Dial2Val.DoModal() == IDOK)
	{
		if (Dial2Val.m_val0<0 || Dial2Val.m_val0>(MesDatasMod.m_nbParametres - 1)) { MessageBox("valeur impossible"); return; }
		if (Dial2Val.m_val1<0 || Dial2Val.m_val1>(MesDatasMod.m_nbParametres - 1)) { MessageBox("valeur impossible"); return; }
		param0 = Dial2Val.m_val0;
		param1 = Dial2Val.m_val1;
	}
	Invalidate(TRUE);
}



//void CChildFrame::OnPerceptronLancerlapprentissage()
//{

//}

//void CChildFrame::OnPerceptronConfiguration()
//{

//
//}

//void CChildFrame::OnPerceptronVisualiserladcisionsurtoutlespace()
//{
//	// TODO: Add your command handler code here
////	Reseau.LireWetZ("essai");
//	if (AppNeurone == FALSE) { MessageBox("Train the network before"); return; }
//	if (VisuNeurone)	VisuNeurone = FALSE;
//	else VisuNeurone = TRUE;
//	Invalidate(TRUE);
//	/*	for (par=0;par<MesDatas.NPAR;par++)
//		{
//			for (l=0;l<100;l++)
//			{
//
//			}
//		}
//	*/
//}

void CChildFrame::OnConfigurationFichierminmaxdenormalisation()
{
	// TODO: Add your command handler code here
	CFileDialog FileData2(TRUE, "*.lim", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Min max (*.lim)|*.lim||", this);
	if (FileData2.DoModal() == IDOK)
	{
		NomFichMinMax = FileData2.GetPathName();
		MesDatasMod.LireDonnee((LPCTSTR)NomFichCMC, m_Proportion, m_Inverse, m_Random); // modifie Tmin et Tmax
		MesDatasMod.LitMinMax((LPCTSTR)NomFichMinMax); // remodifie Tmin et Tmax
		MesDatasMod.m_visuTest = VisuTest;
		MesDatasMod.Normalise(0.1F, 0.9F);
		MesDatasMod.NormaliseVisu();
		Invalidate(TRUE);
	}
}

void CChildFrame::OnConfigurationInfosurlesmesures()
{
	// TODO: Add your command handler code here
	int i;
	if (MesDatasMod.m_allData.empty()) { MessageBox("chargez les données"); return; }
	CString ch1, ch;
	ch1.Format("Nombre de classes :%d\nNombre d'attributs  :%d\n\n", MesDatasMod.m_nbClasses, MesDatasMod.m_nbParametres);
	for (i = 0; i < MesDatasMod.m_nbClasses; i++)
	{
		ch.Format("Nb mesures de classe %d : App:%ld  Test:%ld\n", i, MesDatasMod.m_nbMesAppParClasse[i], MesDatasMod.m_nbMesTestParClasse[i]);
		ch1 += ch;
	}
	MessageBox(ch1, "Info", MB_OK);
	if (MesDatasMod.m_nbParametres < 30)
	{
		ch1.Format("Valeurs Min et Max pour la normalisation\n\n");
		for (i = 0; i < MesDatasMod.m_nbParametres; i++)
		{
			ch.Format("Attribut %d  min=%3.5f  max=%3.5f\n", i, MesDatasMod.m_mins[i], MesDatasMod.m_maxs[i]);
			ch1 += ch;
		}
		MessageBox(ch1, "Info", MB_OK);
	}

}


double CChildFrame::Estimateur(int x0, int xn)

{
	//modif	
	/*	if (abs(x0-xn)>(double) (Hparam*Hparam)/2.0) return 0;
		else return  25500*(2.0/(double)Hparam - (double)(4*abs(x0-xn))/(double)(Hparam*Hparam*Hparam));*/
	if (abs(x0 - xn) > Hparam) return 0;
	else return  100000000 * (1 - (double)(abs(x0 - xn)) / (double)(Hparam));
}



void CChildFrame::OnConfigurationNormalise()
{
	// TODO: Add your command handler code here
	MesDatasMod.Normalise(0.1, 0.9);

}

//void CChildFrame::OnSvmValidationcroisee()
//{
//	// TODO: Add your command handler code here
//	int cross_validation = 0;
//	int elements, max_index, j;
//	int ligne = 0;
//	int  par, mes;
//	unsigned int c;
//	int iter, CV = 5;
//	unsigned int clasmes, cla, i;
//	struct svm_model* model;
//	struct svm_problem prob;  // set by read_problem
//	struct svm_node* x_space;
//	struct svm_node* x_test;
//	long* erreurT = (long*)malloc(MesDatas.NbClass * sizeof(long));
//	float erreurG, ErreurTot = 0;
//	char model_file_name[1024];
//	float** MatConf;
//	MatConf = (float**)malloc(MesDatas.NbClass * sizeof(float*));
//	for (clasmes = 0; clasmes < MesDatas.NbClass; clasmes++) // pour toutes les classes de l'ensemble de données
//	{
//		MatConf[clasmes] = (float*)malloc(MesDatas.NbClass * sizeof(float));
//		for (cla = 0; cla < MesDatas.NbClass; cla++)
//		{
//			MatConf[clasmes][cla] = 0.0;
//		}
//	}
//
//
//
//	CDialVam DialVal;
//	if (DialVal.DoModal() == IDCANCEL) return;
//	CV = DialVal.m_K;
//	CSVMParametres Boite_svm;
//
//	Boite_svm.B_Learning = TRUE;
//
//	if (Boite_svm.DoModal() == IDOK)
//	{
//		if (!Boite_svm.erreur_para)   return;
//		if (Boite_svm.p.weight[0] == Boite_svm.p.weight[1])   Boite_svm.p.nr_weight = 0;
//		if (Boite_svm.Path_Model != "")   strcpy(model_file_name, Boite_svm.Path_Model);
//		else   strcpy(model_file_name, "svm.model");
//	}
//	else return;
//
//	if (Boite_svm.p.gamma == 0)  Boite_svm.p.gamma = 1.0 / MesDatas.NPAR;
//	if (!Boite_svm.p.C)  Boite_svm.p.C = 100;
//
//
//
//	if (Boite_svm.p.weight[0] == Boite_svm.p.weight[1]) Boite_svm.p.nr_weight = 0;
//
//
//	if (m_Random) MesDatas.RedistribAleat();
//
//	for (c = 0; c < MesDatas.NbClass; c++)	 erreurT[c] = 0;
//
//	delete Boite_svm;
//
//	x_test = (struct svm_node*)malloc(sizeof(struct svm_node) * (MesDatas.NPAR + 1) * 2);
//
//	for (iter = 0; iter < CV; iter++)
//	{
//		MesDatas.RedistribCV(CV, iter);
//		prob.l = 0;
//		elements = 0;
//		ligne = 0;
//		for (i = 0; i < MesDatas.NbClass; i++)	  prob.l += MesDatas.NbMes[i]; //nombre d'elements total
//		elements = prob.l * (MesDatas.NPAR + 1); //nombre d'elements total*(nb. de parametre+1)
//		prob.y = (double*)malloc(sizeof(double) * prob.l);
//		prob.x = (struct svm_node**)malloc(sizeof(struct svm_node*) * prob.l);
//		x_space = (struct svm_node*)malloc(sizeof(struct svm_node) * elements);
//		max_index = 0;
//		j = 0;
//
//		for (clasmes = 0; clasmes < MesDatas.NbClass; clasmes++) // pour toutes lesclasses de l'ensemble de données
//		{
//
//			for (mes = 0; mes < MesDatas.NbMes[clasmes]; mes++) // pour toutes lesmesures
//			{
//				prob.x[ligne] = &x_space[j];
//				prob.y[ligne] = clasmes + 1;
//				for (par = 0; par < MesDatas.NPAR; par++)
//				{
//					i = mes * MesDatas.NPAR + par;
//					x_space[j].index = par + 1;
//					x_space[j].value = (float)(MesDatas.TabApp[clasmes][i]);
//					++j;
//				}//for par
//				x_space[j++].index = -1;
//				ligne++;
//			} // for mes
//
//		}// for classmes
//
//		model = svm_train(&prob, &(Boite_svm.p));
//
//		for (c = 0; c < MesDatas.NbClass; c++) // pour toutes les classes de l'ensemble de données
//		{
//			for (mes = 0; mes < MesDatas.NbMesTest[c]; mes++) // pour toutes les mesures
//			{
//				for (par = 0; par < MesDatas.NPAR; par++)
//				{
//					x_test[par].index = par + 1;
//					x_test[par].value = (double)MesDatas.TabTest[c][mes * MesDatas.NPAR + par];
//				}//for par
//				x_test[MesDatas.NPAR].index = -1;
//				double v = svm_predict(model, x_test) - 1.0;
//				if (v != (double)c)	 erreurT[c]++;
//				MatConf[c][(int)v]++;
//
//			} // for mes
//		}// for c
//		svm_save_model(model_file_name, model);
//		svm_destroy_model(model);
//		free(x_space);
//		free(prob.x);
//		free(prob.y);
//	} // iteration cv
//
//	free(x_test);
//
//	long ntot = 0;
//	ErreurTot = 0;
//	CString ch1 = "", ch;
//	ch.Format("erreur en CV%d \n ", CV);
//	ch1 += ch;
//	for (c = 0; c < MesDatas.NbClass; c++)
//	{
//		ntot += MesDatas.NbMesOrig[c];
//		erreurG = ((float)erreurT[c] * 100.0F) / MesDatas.NbMesOrig[c];
//		ch.Format("c%d :%d echantillons mal classés soit e:%03.4f%% \n", c, erreurT[c], erreurG);
//		ch1 += ch;
//		ErreurTot += (erreurG)*MesDatas.NbMesOrig[c];
//	}
//	ErreurTot = (ErreurTot / (float)(ntot));
//	ch.Format("erreur totale:%03.4f %% \n", ErreurTot);
//	ch1 += ch;
//
//
//	ch1 = ch1 + "\nMatrice de Confusion \n\n";
//
//	for (clasmes = 0; clasmes < MesDatas.NbClass; clasmes++) // pour toutes les classes de l'ensemble de données
//	{
//		for (cla = 0; cla < MesDatas.NbClass; cla++)
//		{
//			MatConf[clasmes][cla] = (MatConf[clasmes][cla] * 100.0F) / MesDatas.NbMesOrig[clasmes];
//			ch.Format("%3.2f%%\t", MatConf[clasmes][cla]);
//			ch1 = ch1 + ch;
//		}
//		ch1 += "\n";
//	}
//
//	SauveTexte("resultsvm validation croisee.txt", ch1);
//	MessageBox(ch1);
//
//	free(erreurT);
//
//
//	for (clasmes = 0; clasmes < MesDatas.NbClass; clasmes++) // pour toutes les classes de l'ensemble de données
//	{
//		free(MatConf[clasmes]);
//	}
//	free(MatConf);
//
//	// on recharge automatiquement les donnees
//	if (MesDatas.LireDonnee(NomFichMMC, m_Proportion, m_Inverse, m_Random) != 1)
//	{
//		MessageBox("erreur  relecture Data");
//		return;
//	}
//	Invalidate(TRUE);
//}

void CChildFrame::OnRButtonDown(UINT nFlags, CPoint point)
{
	// TODO: Add your message handler code here and/or call default
	BoutonDroit = TRUE;
	Centre = point;
	OldPoint.x = OldPoint.y = 0;
	CMDIChildWnd::OnRButtonDown(nFlags, point);
}

void CChildFrame::OnRButtonUp(UINT nFlags, CPoint point)
{
	// TODO: Add your message handler code here and/or call default
	ParametreGraphique();
	BoutonDroit = FALSE;
	CString t, ft;
	t.Format("%s", m_datasPath.c_str());

	if (GenereGauss == 1)
	{
		CDialGauss DialGauss;
		if (DialGauss.DoModal() == IDCANCEL) return;
		//		GenereGauss=0;

		float Fray, Fx, Fy;
		Fx = ((float)Centre.x - (float)Bx) / (float)Ax;
		Fy = ((float)Centre.y - (float)By) / (float)Ay;
		Fray = (float)Rayon / (float)Ax;
		ft.Format("_x=%1.3f_y=%1.3f_Ect=%1.3f_ech=%d.dat", Fx, Fy, Fray, DialGauss.m_Sample);
		CGenGauss(t + DialGauss.m_Prefix + ft, Fx, Fray, Fy, Fray, DialGauss.m_Sample);
	}

	CMDIChildWnd::OnRButtonUp(nFlags, point);
}

void CChildFrame::OnMouseMove(UINT nFlags, CPoint point)
{
	// TODO: Add your message handler code here and/or call default

	CClientDC dc(this);
	CPen Stylo(PS_SOLID, 1, RGB(255, 0, 0));
	dc.SelectObject(&Stylo);
	int x1, x2, y1, y2;
	dc.SetROP2(R2_XORPEN);
	if (BoutonDroit)
	{
		//efface l'ancien
		if (OldPoint.x != 0 && OldPoint.y != 0)
		{
			if (OldPoint.x > Centre.x)
			{
				x2 = OldPoint.x;
				Rayon = abs(x2 - Centre.x);
				x1 = Centre.x - Rayon;
				y1 = Centre.y - Rayon;
				y2 = Centre.y + Rayon;
			}
			else
			{
				x1 = OldPoint.x;
				Rayon = abs(x1 - Centre.x);
				x2 = Centre.x - Rayon;
				y1 = Centre.y - Rayon;
				y2 = Centre.y + Rayon;
			}
			dc.Ellipse(x1, y1, x2, y2);

		} // si pas premier dessin
		//dessine le nouveau
		if (point.x > Centre.x)
		{
			x2 = point.x;
			Rayon = abs(x2 - Centre.x);
			x1 = Centre.x - Rayon;
			y1 = Centre.y - Rayon;
			y2 = Centre.y + Rayon;
		}
		else
		{
			x1 = point.x;
			Rayon = abs(x1 - Centre.x);
			x2 = Centre.x - Rayon;
			y1 = Centre.y - Rayon;
			y2 = Centre.y + Rayon;
		}
		dc.Ellipse(x1, y1, x2, y2);
	}
	if (point.x != OldPoint.x || point.y != OldPoint.y) OldPoint = point;

	ReleaseDC(&dc);
	CMDIChildWnd::OnMouseMove(nFlags, point);
}

void CChildFrame::OnEditionGaussiennes2d()
{
	// TODO: Add your command handler code here
	GenereGauss = true;

}


void CChildFrame::InitTps()
{
	QueryPerformanceFrequency(&LiPerfFreq);
	QueryPerformanceCounter(&LiDebut);

}

void CChildFrame::FinTps(DWORD ntot)
{
	QueryPerformanceCounter(&LiFin);
	//CString Texte;
	__int64 itps;
	itps = 1000000 * (LiFin.QuadPart - LiDebut.QuadPart);
	itps = itps / LiPerfFreq.QuadPart;
	float tps = (float(itps) / (float(ntot)));
	m_TexteTemps.Format("temps de décision par échantillon = %.3f microsecondes	", tps);
	//MessageBox(Texte);
}


void CChildFrame::SauveTexte(CString NomFich, CString Chaine)
{
	CStdioFile Res;
	Res.Open(NomFich, CFile::modeCreate | CFile::modeWrite);
	Res.WriteString(Chaine);
	Res.Close();
}

//void CChildFrame::OnMenuCreationFichierAppDesc()
//{
//
//	// on mélange les données
//	if (m_Random) MesDatas.RedistribAleat();
//
//	CStringArray TabRefNom;
//
//	int j, i, NBclass, * NBFichParclass = NULL;
//	CStdioFile Fichier; // variable fichier
//	CString text, Temp, NomCmc;
//
//
//	// on relis le fichier cmc
//	if (Fichier.Open(NomFichCMC, CFile::modeRead))
//	{
//
//		TabRefNom.RemoveAll();
//		Fichier.ReadString(text);
//		NBclass = atoi(LPCSTR(text));
//
//
//		NBFichParclass = new int[NBclass];
//
//		for (i = 0; i < NBclass; i++)
//		{
//			Fichier.ReadString(text);
//			NBFichParclass[i] = atoi(LPCSTR(text));
//		}
//
//		for (i = 0; i < NBclass; i++)
//		{
//			for (j = 0; j < NBFichParclass[i]; j++)
//			{
//				Fichier.ReadString(text);
//				TabRefNom.Add(text);
//			}
//			Temp = text.Right(text.GetLength() - text.ReverseFind('\\') - 1);
//			//TabRefNom.Add(Temp.Left(Temp.GetLength()-4));
//		}
//		if (NBFichParclass != NULL) delete NBFichParclass;
//	} // ouverture ok
//
//
//
//
//
//// on eclate le fichier pour chaque classe
//
//	int Taille, par;
//	unsigned int c;
//	CWordArray TabIndice;
//	float* pTabTemp;
//
//	CString ChaineTempApp;
//	CString ChaineTempDesc;
//	CString ChaineTemp;
//
//	CStdioFile FichierApp;
//	CStdioFile FichierDesc;
//	for (c = 0; c < MesDatas.NbClass; c++)
//	{
//		Taille = MesDatas.NbMesOrig[c];
//		TabIndice.SetSize(Taille);
//		pTabTemp = (float*)malloc(MesDatas.NPAR * MesDatas.NbMesOrig[c] * sizeof(float));
//		memcpy(pTabTemp, MesDatas.Tab[c], MesDatas.NPAR * MesDatas.NbMesOrig[c] * sizeof(float));
//
//		ChaineTempApp = TabRefNom[c];
//		ChaineTemp = "_App";
//
//		ChaineTempApp.Insert(ChaineTempApp.GetLength() - 4, ChaineTemp.GetBuffer(10));
//
//		//on teste la parité
//		if (MesDatas.NbMesOrig[c] % 2 == 0)
//		{// si paire
//			if (FichierApp.Open(ChaineTempApp, CFile::modeCreate | CFile::modeWrite))
//			{
//				ChaineTemp.Format("%d ", MesDatas.NPAR);
//				FichierApp.WriteString(ChaineTemp);
//				FichierApp.WriteString("\n");
//				ChaineTemp.Format("%d ", MesDatas.NbMesOrig[c] / 2);
//				FichierApp.WriteString(ChaineTemp);
//				FichierApp.WriteString("\n");
//			}
//
//			ChaineTempDesc = TabRefNom[c];
//			ChaineTemp = "_Desc";
//
//			ChaineTempDesc.Insert(ChaineTempDesc.GetLength() - 4, ChaineTemp.GetBuffer(10));
//
//			if (FichierDesc.Open(ChaineTempDesc, CFile::modeCreate | CFile::modeWrite))
//			{
//				ChaineTemp.Format("%d ", MesDatas.NPAR);
//				FichierDesc.WriteString(ChaineTemp);
//				FichierDesc.WriteString("\n");
//				ChaineTemp.Format("%d ", MesDatas.NbMesOrig[c] / 2);
//				FichierDesc.WriteString(ChaineTemp);
//				FichierDesc.WriteString("\n");
//			}
//		}
//		else
//		{
//			if (FichierApp.Open(ChaineTempApp, CFile::modeCreate | CFile::modeWrite))
//			{
//				ChaineTemp.Format("%d ", MesDatas.NPAR);
//				FichierApp.WriteString(ChaineTemp);
//				FichierApp.WriteString("\n");
//				ChaineTemp.Format("%d ", (MesDatas.NbMesOrig[c] / 2) + 1);
//				FichierApp.WriteString(ChaineTemp);
//				FichierApp.WriteString("\n");
//			}
//
//			ChaineTempDesc = TabRefNom[c];
//			ChaineTemp = "_Desc";
//
//			ChaineTempDesc.Insert(ChaineTempDesc.GetLength() - 4, ChaineTemp.GetBuffer(10));
//
//			if (FichierDesc.Open(ChaineTempDesc, CFile::modeCreate | CFile::modeWrite))
//			{
//				ChaineTemp.Format("%d ", MesDatas.NPAR);
//				FichierDesc.WriteString(ChaineTemp);
//				FichierDesc.WriteString("\n");
//				ChaineTemp.Format("%d ", (MesDatas.NbMesOrig[c] / 2) + 1);
//				FichierDesc.WriteString(ChaineTemp);
//			}
//		}
//
//
//
//
//		for (i = 0; i < MesDatas.NbMesOrig[c]; i++) TabIndice[i] = i;
//		for (i = 0; i < MesDatas.NbMesOrig[c]; i++)
//		{
//
//
//
//			if (i < MesDatas.NbMesOrig[c] / 2)
//			{
//				for (par = 0; par < MesDatas.NPAR; par++)
//				{
//					if (FichierApp.m_pStream != NULL)
//					{
//						ChaineTemp.Format("%f ", MesDatas.Tab[c][i * MesDatas.NPAR + par]);
//						FichierApp.WriteString(ChaineTemp);
//					}
//				}
//			}
//			else
//			{
//				for (par = 0; par < MesDatas.NPAR; par++)
//				{
//					if (FichierDesc.m_pStream != NULL)
//					{
//						ChaineTemp.Format("%f ", MesDatas.Tab[c][i * MesDatas.NPAR + par]);
//						FichierDesc.WriteString(ChaineTemp);
//					}
//				}
//			}
//
//			if (i < MesDatas.NbMesOrig[c] / 2) FichierApp.WriteString("\n");
//			else FichierDesc.WriteString("\n");
//
//
//			//TabIndice.RemoveAt(ialeat);
//			Taille--;
//		}
//		free(pTabTemp);
//
//
//		FichierApp.Close();
//		FichierDesc.Close();
//	}
//
//
//	// on créer deux fichier cmc
//
//
//	CStdioFile FichierCmcApp;
//	CStdioFile FichierCmcDesc;
//
//	FichierCmcApp.Open("Configuration_Desc.cmc", CFile::modeCreate | CFile::modeWrite);
//	FichierCmcDesc.Open("Configuration_App.cmc", CFile::modeCreate | CFile::modeWrite);
//	ChaineTemp.Format("%d", MesDatas.NbClass);
//
//	FichierCmcApp.WriteString(ChaineTemp);
//	FichierCmcApp.WriteString("\n");
//	FichierCmcDesc.WriteString(ChaineTemp);
//	FichierCmcDesc.WriteString("\n");
//
//	for (c = 0; c < MesDatas.NbClass; c++)
//	{
//		ChaineTemp.Format("%d", 1);
//		FichierCmcApp.WriteString(ChaineTemp);
//		FichierCmcApp.WriteString("\n");
//		FichierCmcDesc.WriteString(ChaineTemp);
//		FichierCmcDesc.WriteString("\n");
//	}
//
//	for (c = 0; c < MesDatas.NbClass; c++)
//	{
//		ChaineTempApp = TabRefNom[c];
//		ChaineTemp = "_App";
//		ChaineTempApp.Insert(ChaineTempApp.GetLength() - 4, ChaineTemp.GetBuffer(10));
//		FichierCmcApp.WriteString(ChaineTempApp);
//		FichierCmcApp.WriteString("\n");
//
//		ChaineTempDesc = TabRefNom[c];
//		ChaineTemp = "_Desc";
//		ChaineTempDesc.Insert(ChaineTempDesc.GetLength() - 4, ChaineTemp.GetBuffer(10));
//		FichierCmcDesc.WriteString(ChaineTempDesc);
//		FichierCmcDesc.WriteString("\n");
//	}
//
//	FichierCmcDesc.Close();
//	FichierCmcApp.Close();
//
//	MessageBox("La séparation est terminée, \n les fichiers *.dat ainsi que \n 2 fichiers *.cmc ont été créés", "séparation terminée", MB_OK);
//
//}





void CChildFrame::OnConfigurationVisualisationdesTest()
{
	// TODO : ajoutez ici le code de votre gestionnaire de commande
	if (MesDatasMod.m_allData.empty()) { MessageBox("Veuillez charger les données"); return; }
	if (!VisuTest) VisuTest = true; else VisuTest = false;
	MesDatasMod.m_visuTest = VisuTest;
	MesDatasMod.NormaliseVisu();
	Invalidate(TRUE);
}

struct SContexte
{
	CDaliogProgression* pdial;
	Ptr<cv::ml::SVM>* classif;
	CChildFrame* ptr;
};


void CChildFrame::trainSVM(float g, bool Tauto)
{
	Ptr<ParamGrid> gammaGrid1 = ParamGrid::create(0.01, g, pow(10.0, 0.5));
	Ptr<ParamGrid> cGrid = SVM::getDefaultGridPtr(SVM::C); // Grille par défaut pour C
	Ptr<ParamGrid> pGrid = SVM::getDefaultGridPtr(SVM::P); // Grille pour epsilon
	Ptr<ParamGrid> nuGrid = SVM::getDefaultGridPtr(SVM::NU); // Grille pour nu
	Ptr<SVM> pClassifier = m_pClassifier.dynamicCast<SVM>();

	//if (Tauto) pClassifier->trainAuto(m_trainData, cv::ml::ROW_SAMPLE, m_Label, 5, cGrid, gammaGrid1, pGrid, nuGrid);
	//else pClassifier->train(m_trainData, cv::ml::ROW_SAMPLE, m_Label);

	if (Tauto) pClassifier->trainAuto(MesDatasMod.m_trainingData, cv::ml::ROW_SAMPLE, m_Label, 5, cGrid, gammaGrid1, pGrid, nuGrid);
	else pClassifier->train(MesDatasMod.m_trainingData, cv::ml::ROW_SAMPLE, m_Label);

	EndTraining = true;

}

void CChildFrame::OnSvmSvmopencvtrainauto()
{

	Cleannonmodal(false);
	if (MesDatasMod.m_nbParametres == 0 || MesDatasMod.m_nbClasses == 1) 		OnConfigurationLireauformatcsv();
	if (MesDatasMod.m_nbParametres == 0) return;
	CSVMParametres Boite_svm;
	Boite_svm.B_Learning = TRUE;
	if (Boite_svm.DoModal() != IDOK)
	{
		return;
	}

	CFileDialog Model_dia(FALSE, "cvmodel", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		"Model Files (*.cvmodel)|*.cvmodel|" "All Files (*.*)|*.*||");
	Model_dia.m_ofn.lpstrTitle = _T("Enregistrer le modèle");

	CString Path_Model;
	cv::String pm;
	if (Model_dia.DoModal() == IDOK)
	{
		Path_Model = Model_dia.GetPathName();
		pm.assign(Path_Model);
	}
	else return;

	MesDatasMod.m_trainingLabels.convertTo(m_Label, CV_32SC1);

	Ptr <cv::ml::SVM > svm = cv::ml::SVM::create();
	if (Boite_svm.p.kernel_type == RBF)
	{
		svm->setKernel(cv::ml::SVM::RBF);
		svm->setGamma(Boite_svm.p.gamma);
	}
	if (Boite_svm.p.kernel_type == LINEAR)
	{
		svm->setKernel(cv::ml::SVM::LINEAR);
	}
	if (Boite_svm.p.kernel_type == POLY)
	{
		svm->setKernel(cv::ml::SVM::POLY);
		svm->setDegree(Boite_svm.p.degree);
		svm->setGamma(Boite_svm.p.gamma);
		svm->setCoef0(Boite_svm.p.coef0);

	}
	svm->setType(cv::ml::SVM::C_SVC);
	;
	//Ptr<ParamGrid> gammaGrid1 = ParamGrid::create(0.01, Boite_svm.p.gamma, pow(10.0, 0.5));
	//Ptr<ParamGrid> cGrid = SVM::getDefaultGridPtr(SVM::C); // Grille par défaut pour C
	//Ptr<ParamGrid> pGrid = SVM::getDefaultGridPtr(SVM::P); // Grille pour epsilon
	//Ptr<ParamGrid> nuGrid = SVM::getDefaultGridPtr(SVM::NU); // Grille pour nu

	//	svm->setTermCriteria(TermCriteria(TermCriteria::MAX_ITER + TermCriteria::EPS, 10000, 1e-6));


	Fenetre_Progression = new CDaliogProgression();
	Fenetre_Progression->Create(IDD_DIALOG_PROGRESS, this); // Création non modale
	Fenetre_Progression->ShowWindow(SW_SHOW);
	Fenetre_Progression->SetWindowTextA("Apprentissage en cours...");
	Fenetre_Progression->m_ControlProgressBar.SetRange(0, 100);

	SetTimer(1, 500, NULL);
	m_pClassifier = svm;
	EndTraining = false;
	//	if (Boite_svm.m_TrainAuto)
	//	{
			//svm->trainAuto(m_trainData, cv::ml::ROW_SAMPLE, m_Label, 5, cGrid, gammaGrid1, pGrid, nuGrid);
	std::thread trainingThread(&CChildFrame::trainSVM, this, (float)Boite_svm.p.gamma, Boite_svm.m_TrainAuto);
	trainingThread.detach();

	//}
	//else
	//{
	//	svm->train(m_trainData, cv::ml::ROW_SAMPLE, m_Label);
	//}

	MSG msg;
	while (/*GetMessage(&msg, nullptr, 0, 0) &&*/ !EndTraining)
	{
		//if (msg.message == WM_TIMER || msg.message == WM_PAINT)
		//{
		//	TranslateMessage(&msg);
		//	DispatchMessage(&msg);
		//}

		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_TIMER || msg.message == WM_PAINT)
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}

	if (Fenetre_Progression) {
		Fenetre_Progression->SendMessage(WM_CLOSE); // Fermer proprement la fenêtre
		delete Fenetre_Progression;
		Fenetre_Progression = nullptr;
	}

	KillTimer(1);
	svm->save(pm);

	double g = svm->getGamma();
	CString Texte;
	Texte.Format("Gamma optimum = %3.2lf \nFichier cvmodel sauvegardé", g);
	MessageBox(Texte);
	m_pClassifier = svm;
	LastType = mSVM;
	//if (MesDatasMod.m_nbParametres == 2) 
	{

		Invalidate(); // VisufrontiereOpencv(&svm, mSVM);
	}
	m_Loaded = false;

}

void CChildFrame::Cleannonmodal(bool release)
{
	//if (VisuCamLive)
	//{
	//	cv::destroyAllWindows();
	//	VisuCamLive = false;
	//}
	if (m_pSVMParamDlg)
	{
		m_pSVMParamDlg->DestroyWindow();
		delete m_pSVMParamDlg;
		m_pSVMParamDlg = nullptr;
	}

	if (m_pKPPVparamDlg) {
		m_pKPPVparamDlg->DestroyWindow();
		delete m_pKPPVparamDlg;
		m_pKPPVparamDlg = nullptr;
	}
	if (m_pNNParamDlg) {
		m_pNNParamDlg->DestroyWindow();
		delete m_pNNParamDlg;
		m_pNNParamDlg = nullptr;
	}
	if (release)
	{
		m_trainData.release();
		m_Label.release();
	}
	//if (m_closeCallback != nullptr)
	//{
	//	//m_render_window_interactor->Disable();
	//	CleanUpVTK();

	//	if (!m_closeCallback->IsWindowClosed())
	//	{
	//		HWND hWnd = (HWND)m_render_window_interactor->GetRenderWindow()->GetGenericWindowId();
	//		if (hWnd)
	//		{
	//			::SendMessage(hWnd, WM_CLOSE, 0, 0);
	//		}
	//	}
	//	//delete(m_closeCallback);
	//	m_closeCallback = nullptr;
	//}
}

void CChildFrame::OnSvmSvmopencvvisufrontiere()
{

	Cleannonmodal();
	if (MesDatasMod.m_nbParametres > 3) { MessageBox("Nombre d'attributs superieur à 3", MB_OK); return; }

	if (!LoadClassifier(mSVM)) return;
	int nbParametres = m_pClassifier.dynamicCast<SVM>()->getVarCount();
	if (nbParametres < 2 || nbParametres > 3 || MesDatasMod.m_nbParametres != nbParametres)
	{
		MessageBox("Nombre d'attributs incompatible avec la visualisation");
		return;
	}
	m_Loaded = true;
	if (MesDatasMod.m_nbParametres == 3 && m_pClassifier != nullptr && LastType == mSVM)
	{
		m_TitreFen3D = "Classifieur SVM";
		Visu3d(m_pClassifier, mSVM);
	}
	if (MesDatasMod.m_nbParametres == 2 && m_pClassifier != nullptr && LastType == mSVM)
	{
		Invalidate(); 
	}

}


void CChildFrame::OnSvmSvmopencvevaluation()
{
	String pm;
	CFileDialog FileData2(TRUE, "*.cvmodel", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Model (*.cvmodel)|*.cvmodel||", this);
	if (FileData2.DoModal() != IDOK) return;
	ChaineModel = FileData2.GetPathName();
	m_PathData = ChaineModel.Left(ChaineModel.ReverseFind('.'));
	pm.assign(ChaineModel);

	Ptr<SVM> svm = Algorithm::load<SVM>(pm);

	if (svm->getVarCount() != MesDatasMod.m_nbParametres)
	{
		MessageBox("Nombre d'attributs du modele différent du nombre d'attributs des données");
		return;
	}
	m_pClassifier = svm;
	LastType = mSVM;
	Evaluation(&svm, mSVM);



}


SMetriques CChildFrame::Evaluation(void* ptr, TypeClassif TypeClassifieur, bool save)
{
	// TODO: Add your command handler code here
	SMetriques Metriques{ 0.0, 0.0, 0.0, 0.0, 0.0 };
	if (MesDatasMod.m_allData.empty()) { MessageBox("Chargez les données"); return Metriques; }
	if (MesDatasMod.m_nbMesTestParClasse[0] == 0) { MessageBox("Le nombre d'échantillons de test est nul, évaluation impossible"); return Metriques; }
	CString Chaine, Texte, cht;
	long  par;
	unsigned int clasmes, cla;
	float erreurG = 0.0;
	float** MatConf;
	float* TabRappel;
	float* TabPrecision;
	DWORD ntot;
	float ErreurTot = 0;
	double v = 0;
	long* erreurT;

	MatConf = (float**)malloc(MesDatasMod.m_nbClasses * sizeof(float*));
	TabRappel = (float*)malloc(MesDatasMod.m_nbClasses * sizeof(float));
	TabPrecision = (float*)malloc(MesDatasMod.m_nbClasses * sizeof(float));



	for (clasmes = 0; clasmes < MesDatasMod.m_nbClasses; clasmes++) // pour toutes les classes de l'ensemble de données
	{
		MatConf[clasmes] = (float*)malloc(MesDatasMod.m_nbClasses * sizeof(float));
		for (cla = 0; cla < MesDatasMod.m_nbClasses; cla++)
		{
			MatConf[clasmes][cla] = 0.0;
		}
	}

	ErrorSVM = 0;


	erreurT = (long*)malloc(MesDatasMod.m_nbClasses * sizeof(long));
	for (clasmes = 0; clasmes < MesDatasMod.m_nbClasses; clasmes++) // pour toutes les classes de l'ensemble de données
	{
		erreurT[clasmes] = 0;
	}

	InitTps();
	ntot = MesDatasMod.m_testData.rows;
	for (int k = 0; k < ntot; k++)
	{
		clasmes = (int)MesDatasMod.m_testLabels.at<float>(k, 0);
		Mat sampleMat = Mat(1, MesDatasMod.m_nbParametres, CV_32FC1);
		for (par = 0; par < MesDatasMod.m_nbParametres; par++)
		{
			sampleMat.at<float>(0, par) = MesDatasMod.m_testData.at<float>(k, par);
		}//for par
		v = Decide(ptr, TypeClassifieur, sampleMat);
		if (v<0 || v >MesDatasMod.m_nbClasses)
		{
			MessageBox("Erreur dans les données"); return Metriques;
		}
		if (v != (double)clasmes) erreurT[clasmes]++;
		MatConf[clasmes][(int)v]++;
	}


	for (clasmes = 0; clasmes < MesDatasMod.m_nbClasses; clasmes++) // pour toutes les classes de l'ensemble de données
	{
		erreurG = ((float)erreurT[clasmes] * 100.0F) / MesDatasMod.m_nbMesTestParClasse[clasmes];
		Texte.Format("Erreur classe n°%d : %d échantillons mal classés soit %3.4f%%\n", clasmes, erreurT[clasmes], erreurG);
		cht = cht + Texte;
		ErreurTot += (erreurG)*MesDatasMod.m_nbMesTestParClasse[clasmes];
	}// for clasmes
	FinTps(ntot);
	Chaine = m_TexteTemps + "\n\n" + cht;
	ErrorSVM = erreurG;
	ErreurTot = (ErreurTot / (float)(ntot));
	Texte.Format("Erreur Totale %3.4f%%\n\n", ErreurTot);
	Chaine = Chaine + Texte;
	Chaine = Chaine + "Matrice de Confusion (%) \n\n";

	for (clasmes = 0; clasmes < MesDatasMod.m_nbClasses; clasmes++) // pour toutes les classes de l'ensemble de données
	{
		float Fp = 0;
		for (cla = 0; cla < MesDatasMod.m_nbClasses; cla++)
		{
			if (cla != clasmes) Fp += MatConf[cla][clasmes];
		}
		TabPrecision[clasmes] = (float)MatConf[clasmes][clasmes] / ((float)MatConf[clasmes][clasmes] + Fp);
	}

	for (clasmes = 0; clasmes < MesDatasMod.m_nbClasses; clasmes++) // pour toutes les classes de l'ensemble de données
	{
		float Fn = 0;
		for (cla = 0; cla < MesDatasMod.m_nbClasses; cla++)
		{
			if (cla != clasmes) Fn += MatConf[clasmes][cla];
		}
		TabRappel[clasmes] = (float)MatConf[clasmes][clasmes] / ((float)MatConf[clasmes][clasmes] + Fn);
	}

	float Rappel, Precision;
	Rappel = 0.0;
	Precision = 0.0;

	for (clasmes = 0; clasmes < MesDatasMod.m_nbClasses; clasmes++) // pour toutes les classes de l'ensemble de données
	{
		Rappel += TabRappel[clasmes];
		Precision += TabPrecision[clasmes];
	}
	Rappel /= MesDatasMod.m_nbClasses;
	Precision /= MesDatasMod.m_nbClasses;


	for (clasmes = 0; clasmes < MesDatasMod.m_nbClasses; clasmes++) // pour toutes les classes de l'ensemble de données
	{
		for (cla = 0; cla < MesDatasMod.m_nbClasses; cla++)
		{
			MatConf[cla][clasmes] = (MatConf[cla][clasmes] * 100.0F) / MesDatasMod.m_nbMesTestParClasse[cla];
			Texte.Format("%3.2f\t", MatConf[cla][clasmes]);
			Chaine = Chaine + Texte;
		}
		Chaine += "\n";
	}
	/*
		DWORD fin=GetTickCount();
		fin=fin-debut;
		float tps=(float(fin)/(float(ntot)*1000.0));
		Texte.Format("temps de décision par échantillon = %f ms	",tps);
		MessageBox(Texte);
	*/

	Texte.Format("Rappel    %3.4f\n", Rappel);
	Chaine = Chaine + Texte;
	Texte.Format("Precision %3.4f\n", Precision);
	Chaine = Chaine + Texte;

	float Accuracy = 0.0;
	for (clasmes = 0; clasmes < MesDatasMod.m_nbClasses; clasmes++)
	{
		Accuracy += (MatConf[clasmes][clasmes] / 100.0F) * MesDatasMod.m_nbMesTestParClasse[clasmes];
	}

	Accuracy /= ntot;
	Texte.Format("Accuracy  %3.4f\n", Accuracy);
	Chaine = Chaine + Texte;
	float F1 = 2 * (Precision * Rappel) / (Precision + Rappel);
	Texte.Format("F1 Score  %3.4f\n\n", F1);
	Chaine = Chaine + Texte;

	/*float WeightedPrecision = 0.0, WeightedRappel = 0.0, WeightedF1 = 0.0;
	for (clasmes = 0; clasmes < MesDatasMod.m_nbClasses; clasmes++)
	{
		float weight = (float)MesDatasMod.m_nbMesTestParClasse[clasmes] / ntot;
		WeightedPrecision += TabPrecision[clasmes] * weight;
		WeightedRappel += TabRappel[clasmes] * weight;
		WeightedF1 += 2 * (TabPrecision[clasmes] * TabRappel[clasmes]) /
			(TabPrecision[clasmes] + TabRappel[clasmes]) * weight;
	}
	Texte.Format("Rappel pondéré     %3.4f\n", WeightedRappel);
	Chaine = Chaine + Texte;
	Texte.Format("Précision pondérée %3.4f\n", WeightedPrecision);
	Chaine = Chaine + Texte;
	Texte.Format("Score F1 pondéré   %3.4f\n", WeightedF1);
	Chaine = Chaine + Texte;*/

	free(erreurT);
	CString NomResultat;
	switch (TypeClassifieur) {
	case mSVM: {
		NomResultat = m_PathData + "_resultats_SVM.txt";
		break;
	}
	case mKPPV: {
		NomResultat = m_PathData + "_resultats_KPPV.txt";
		break;
	}
	case mRF: {
		NomResultat = m_PathData + "_resultats_RF.txt";
		break;
	}
	case mMLP: {
		NomResultat = m_PathData + "_resultats_MLP.txt";
		break;
	}
	case mNB: {
		NomResultat = m_PathData + "_resultats_NBAYES.txt";
		break;
	}
	case mEM: {
		NomResultat = m_PathData + "_resultats_EM.txt";
		break;
	}
	case mDT: {
		NomResultat = m_PathData + "_resultats_BOOSTING.txt";
		break;
	}
	default:
		NomResultat = m_PathData + "_resultats.txt";
		break;
	}


	m_PathData;

	if (save)
	{
		SauveTexte(NomResultat, Chaine);
		if (MesDatasMod.m_nbClasses < 10)
			MessageBox(Chaine);
		else
			MessageBox("Le nombre de classe est supérieur à 9,\nl'affichage de la matrice de confusion est trop grand,\nvoyez les résultats dans le fichier \n" + NomResultat);
	}

	for (clasmes = 0; clasmes < MesDatasMod.m_nbClasses; clasmes++) // pour toutes les classes de l'ensemble de données
	{
		free(MatConf[clasmes]);
	}
	free(MatConf);

	free(TabRappel);
	free(TabPrecision);

	Metriques.ErreurGlobale = ErreurTot / 100.0;
	Metriques.Accuracy = Accuracy;
	Metriques.Precision = Precision;
	Metriques.Rappel = Rappel;
	Metriques.F1score = F1;

	return Metriques;

}

void CChildFrame::OnKppvKppvopencvapprentissage()
{
	Cleannonmodal(true);
	if (MesDatasMod.m_nbParametres == 0 || MesDatasMod.m_nbClasses == 1) 		OnConfigurationLireauformatcsv();
	if (MesDatasMod.m_allData.empty()) return;

	CFileDialog Model_dia(FALSE, "cvkppv", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		"Model Files (*.cvkppv)|*.cvkppv|" "All Files (*.*)|*.*||");
	Model_dia.m_ofn.lpstrTitle = _T("Enregistrer le modèle");
	CString Path_Model;
	cv::String pm;
	if (Model_dia.DoModal() == IDOK)
	{
		Path_Model = Model_dia.GetPathName();
		pm.assign(Path_Model);
	}
	else return;

	MesDatasMod.m_trainingLabels.convertTo(m_Label, CV_32SC1);

	//Mat trainData = Mat(MesDatas.NbMesTotApp, MesDatas.NPAR, CV_32FC1);
	//Mat Label = Mat(MesDatas.NbMesTotApp, 1, CV_32SC1);
	//int train_sample_count = MesDatas.NbMesTotApp;
	//int  k, i, par;
	//unsigned int c;
	//k = 0;
	//for (c = 0; c < MesDatas.NbClass; c++)
	//{
	//	for (i = 0; i < MesDatas.NbMes[c]; i++)
	//	{
	//		for (par = 0; par < MesDatas.NPAR; par++)
	//		{
	//			trainData.at<float>(k, par) = MesDatas.TabApp[c][i * MesDatas.NPAR + par];
	//		}
	//		Label.at<int>(k, 0) = c;
	//		k++;
	//	}
	//}


	Ptr <ml::KNearest > knn = ml::KNearest::create();
	knn->train(MesDatasMod.m_trainingData, ml::ROW_SAMPLE, MesDatasMod.m_trainingLabels);
	knn->save(pm);
	CString Texte;
	Texte.Format("Fichier cvkppv sauvegardé");
	MessageBox(Texte);
	m_pClassifier = knn;
	LastType = mKPPV;
	if (MesDatasMod.m_nbParametres == 2)
	{
		Invalidate();
	}
	m_Loaded = false;
}


void CChildFrame::OnKppvKppvopencvvisufrontiere()
{
	Cleannonmodal();
	if (MesDatasMod.m_nbParametres > 3) { MessageBox("Nombre d'attributs superieur à 3", MB_OK); return; }

	if (!LoadClassifier(mKPPV)) return;
	int nbParametres = m_pClassifier.dynamicCast<KNearest>()->getVarCount();
	if (nbParametres < 2 || nbParametres > 3 || MesDatasMod.m_nbParametres!= nbParametres)
	{
		MessageBox("Nombre d'attributs incompatible avec la visualisation");
		return;
	}
	m_Loaded = true;
	if (MesDatasMod.m_nbParametres == 3 && m_pClassifier != nullptr && LastType == mKPPV)
	{
		m_TitreFen3D = "Classifieur KPPV";
		Visu3d(m_pClassifier, mKPPV);
	}
	if (MesDatasMod.m_nbParametres == 2 && m_pClassifier != nullptr && LastType == mKPPV)
	{
		Invalidate(); 
	}

}


void CChildFrame::OnKppvKppvopencvevaluation()
{
	Cleannonmodal(false);
	String pm;
	CFileDialog FileData(TRUE, "*.cvkppv", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Model (*.cvkppv)|*.cvkppv||", this);
	if (FileData.DoModal() != IDOK) return;
	ChaineModel = FileData.GetPathName();
	m_PathData = ChaineModel.Left(ChaineModel.ReverseFind('.'));
	pm.assign(ChaineModel);
	Ptr<KNearest> kppv = Algorithm::load<KNearest>(pm);
	if (kppv->getVarCount() != MesDatasMod.m_nbParametres)
	{
		MessageBox("Nombre d'attributs du modele différent du nombre d'attributs des données");
		return;
	}
	m_pClassifier = kppv;
	LastType = mKPPV;
	Evaluation(&kppv, mKPPV);
}


void CChildFrame::DecidDataOpencv(void* ptr, CDonneeFloatMod& data, std::ofstream& FicRes, TypeClassif TypeClassifieur)
{
	if (!FicRes.is_open()) {
		MessageBox("Erreur de fichier resultat");
		return;
	}

	Mat& trainingData = data.m_trainingData;
	int numSamples = trainingData.rows;
	int numFeatures = trainingData.cols;

	for (int mes = 0; mes < numSamples; mes++) 
	{
		Mat sampleMat = trainingData.row(mes);
		float v = 0.0;

		switch (TypeClassifieur) 
{
		case mSVM: {
			Ptr<SVM> pClassifier = *((Ptr<SVM>*)(ptr));
			v = pClassifier->predict(sampleMat);
			break;
		}
		case mKPPV: {
			Ptr<KNearest> pClassifier = *((Ptr<KNearest>*)(ptr));
			v = pClassifier->predict(sampleMat);
			break;
		}
		case mRF: {
			Ptr<RTrees> pClassifier = *((Ptr<RTrees>*)(ptr));
			v = pClassifier->predict(sampleMat);
			break;
		}
		case mMLP: {
			Ptr<ANN_MLP> pClassifier = *((Ptr<ANN_MLP>*)(ptr));
			Mat result;
			v = pClassifier->predict(sampleMat, result);
			Point maxLoc;
			minMaxLoc(result, nullptr, nullptr, nullptr, &maxLoc);
			v = (float)maxLoc.x;
			break;
		}
		case mNB: {
			Ptr<NormalBayesClassifier> pClassifier = *((Ptr<NormalBayesClassifier>*)(ptr));
			v = pClassifier->predict(sampleMat);
			break;
		}
		case mEM: {
			Ptr<EM> pClassifier = *((Ptr<EM>*)(ptr));
			Mat probs;
			v = pClassifier->predict(sampleMat, probs);
			break;
		}
		case mDT: {
			Ptr<MultiClassBoost> pClassifier = *((Ptr<MultiClassBoost>*)(ptr));
			v = pClassifier->predict(sampleMat);
			int predictedClass = static_cast<int>(round(v));
			v = (float)predictedClass;
			break;
		}
		default:
			MessageBox("Type de classifieur inconnu");
			return;
		}

		if (v < 0) {
			MessageBox("Erreur dans les données");
			return;
		}
		else {
			FicRes << (int)v << "\n";
		}
	}
}


float CChildFrame::Decide(void* ptr, TypeClassif TypeClassifieur, Mat& sampleMat)
{
	float v;
	Mat result;
	if (m_pClassifier == nullptr) return 0.0;
	switch (TypeClassifieur) {
	case mSVM: {
		//Ptr<SVM> pClassifier = *((Ptr<SVM>*)(ptr));
		Ptr<SVM> pClassifier = m_pClassifier.dynamicCast<SVM>();
		//m_pClassifier = pClassifier;
		v = pClassifier->predict(sampleMat);
		break;
	}
	case mKPPV: {
		Ptr<KNearest> pClassifier = m_pClassifier.dynamicCast<KNearest>();
		//	m_pClassifier = pClassifier;
		v = pClassifier->predict(sampleMat);
		break;
	}
	case mRF: {
		Ptr<RTrees> pClassifier = m_pClassifier.dynamicCast<RTrees>();
		//	m_pClassifier = pClassifier;
		v = pClassifier->predict(sampleMat);
		break;
	}
	case mMLP: {
		Ptr<ANN_MLP> pClassifier = m_pClassifier.dynamicCast<ANN_MLP>();
		//m_pClassifier = pClassifier;
		pClassifier->predict(sampleMat, result);
		Point maxLoc;
		minMaxLoc(result, nullptr, nullptr, nullptr, &maxLoc);
		v = (float)maxLoc.x;
		break;
	}
	case mNB: {
		Ptr<NormalBayesClassifier> pClassifier = m_pClassifier.dynamicCast<NormalBayesClassifier>();
		//m_pClassifier = pClassifier;
		v = pClassifier->predict(sampleMat);
		break;
	}
	case mEM: {
		Ptr<EM> pClassifier = m_pClassifier.dynamicCast<EM>();
		//m_pClassifier = pClassifier;
		Mat probs;
		v = pClassifier->predict(sampleMat, probs);
		if (clusterToClass.size() != 0)
			if (v < clusterToClass.size()) v = (float)clusterToClass[(int)v]; else v = 0;
		break;
	}
	case mDT: {
		Ptr<MultiClassBoost> pClassifier = m_pClassifier.dynamicCast<MultiClassBoost>();
		//m_pClassifier = pClassifier;
		v = pClassifier->predict(sampleMat);
		int predictedClass = static_cast<int>(round(v));
		v = (float)predictedClass;
		break;
	}
	default:
		//	m_pClassifier = nullptr;
		break;
	}
	return v;

}


//void CChildFrame::VisufrontiereOpencv(void* ptr, TypeClassif TypeClassifieur, CPaintDC& dc)
//{
//	String pm;
//	int step = 2; // Taille d'un pixel logique
//	CDC memDC;
//	CBitmap bitmap;
//	CRect Rect;
//	GetClientRect(&Rect);
//	int decalage = 20;
//	int LargW = Rect.Width();
//	int HautW = Rect.Height();
//	int LargZ = LargW -decalage;
//	int HautZ = HautW - 3 * decalage;
//	memDC.CreateCompatibleDC(&dc);
//	bitmap.CreateCompatibleBitmap(&dc, LargW, HautW);
//	CBitmap* pOldBitmap = memDC.SelectObject(&bitmap);
//	memDC.BitBlt(0, 0, LargW, HautW, &dc, 0, 0, SRCCOPY);
//
//	float coefx = MesDatasMod.m_maxs[0] - MesDatasMod.m_mins[0];
//	float coefy = MesDatasMod.m_maxs[1] - MesDatasMod.m_mins[1];
//
//	if (MesDatasMod.m_nbParametres == 2)
//	{
//		ParametreGraphique();
//		std::mutex dcMutex;
//		// Fonction pour traiter une portion de l'écran
//		auto process_section = [&](int startRow, int endRow, CDC* pDC)
//			{
//				float Rouge, Vert, Bleu;
//				float cxp, cyp, cx, cy;
//				for (int row = startRow; row < endRow; row += step)
//				{
//					//cy = ((float)row - By) / Ay; // Calculer Y logique
//					//cy = (float)(row) / (float)(HautZ);
//					cy = ((float)(HautW - decalage - row) - By) / Ay;
//					cy = std::max(0.0f, std::min(1.0f, cy));
//					for (int col = 0; col < LargZ; col += step)
//					{
//						//float cx = ((float)col - Bx) / Ax; // Calculer X logique
//						//cx = (float)col / (float(LargZ));
//						cx = ((float)col - Bx) / Ax;
//						cx = std::max(0.0f, std::min(1.0f, cx));
//			
//						if (coefx != 0) cxp = cx * coefx + MesDatasMod.m_mins[0];
//						if (coefy != 0) cyp = cy * coefy + MesDatasMod.m_mins[1];
//
//						// Préparer les données pour la prédiction
//						Mat sampleMat = (Mat_<float>(1, 2) << cxp, cyp);
//						float v = 0.0f;
//	
//
//						v = Decide(ptr, TypeClassifieur, sampleMat);
//
//						// Déterminer les couleurs
//						int fv = (int)v;
//						unsigned int c = fv + 5;
//
//						if (MesDatasMod.m_nbClasses == 2) {
//							if (fv == 1) { Rouge = 255; Vert = 255; Bleu = 0; }
//							else { Rouge = 0; Vert = 255; Bleu = 0; }
//						}
//						else {
//							if ((c % 3) == 0) Rouge = 255 - (16 * c); else Rouge = 0;
//							if (((c + 1) % 3) == 0) Vert = 255 - (16 * c); else Vert = 0;
//							if (((c + 2) % 3) == 0) Bleu = 255 - (16 * c); else Bleu = 0;
//						}
//
//						// Dessiner le pixel sur la position logique convertie en coordonnées écran
//						std::lock_guard<std::mutex> lock(dcMutex);
//						pDC->FillSolidRect(col, HautW-decalage-row, step, step, RGB((int)Rouge, (int)Vert, (int)Bleu));
//					}
//				}
//			};
//
//		// Définir le nombre de threads
//		int numThreads =  m_NbThread; // Par exemple, utiliser 4 threads
//		std::vector<std::thread> threads;
//		int rowsPerThread = HautZ / numThreads;
//
//		// Lancer les threads
//		for (int t = 0; t < numThreads; ++t) {
//			int startRow = t * rowsPerThread;
//			int endRow = (t == numThreads - 1) ? HautZ : startRow + rowsPerThread;
//			threads.emplace_back(process_section, startRow, endRow, &memDC);
//		}
//
//		// Attendre la fin des threads
//		for (auto& thread : threads) {
//			thread.join();
//		}
//
//
//	} // si NPAR =2
//	// Dessiner les points de données
//	else
//	{
//		memDC.FillSolidRect(decalage, 2 * decalage, LargW - 2 * decalage, HautW - 3 * decalage, RGB(255, 255, 255));
//	}
//
//	//for (unsigned int c = 0; c < MesDatas.NbClass; c++) 
//	//{
//	//	int Rouge, Vert, Bleu;
//	//	if ((c % 3) == 0) Rouge = 255 - (16 * c); else Rouge = 0;
//	//	if (((c + 1) % 3) == 0) Vert = 255 - (16 * c); else Vert = 0;
//	//	if (((c + 2) % 3) == 0) Bleu = 255 - (16 * c); else Bleu = 0;
//
//	//	for (int i = 0; i < MesDatas.NbMes[c]; i++) {
//	//		float cx = MesDatas.TabVisu[c][i * MesDatas.NPAR + param0];
//	//		float cy = MesDatas.TabVisu[c][i * MesDatas.NPAR + param1];
//	//		memDC.FillSolidRect(PosGrX(cx), PosGrY(cy), 4, 4, RGB(Rouge, Vert, Bleu));
//	//	}
//	//}
//
//	for (unsigned int id = 0; id < MesDatasMod.m_VisuData.rows; id++)
//	{
//		int Rouge, Vert, Bleu;
//		unsigned int c =(unsigned int) MesDatasMod.m_trainingLabels.at<float>(id, 0);
//		if ((c % 3) == 0) Rouge = 255 - (16 * c); else Rouge = 0;
//		if (((c + 1) % 3) == 0) Vert = 255 - (16 * c); else Vert = 0;
//		if (((c + 2) % 3) == 0) Bleu = 255 - (16 * c); else Bleu = 0;
//		float cx= MesDatasMod.m_VisuData.at< float > (id, 0);
//		float cy = MesDatasMod.m_VisuData.at< float >(id, 1);
//		memDC.FillSolidRect(PosGrX(cx), PosGrY(cy), 4, 4, RGB(Rouge, Vert, Bleu));
//
//	}
//
//	// Copier le contexte mémoire vers le DC principal
//	dc.BitBlt(0, 0, LargW, HautW, &memDC, 0, 0, SRCCOPY);
//
//	// Nettoyer
//	memDC.SelectObject(pOldBitmap);
//	bitmap.DeleteObject();
//
//	LastType = TypeClassifieur;
//}

void CChildFrame::VisufrontiereOpencv(void* ptr, TypeClassif TypeClassifieur, CPaintDC& dc)
{
	String pm;
	int step = 2; // Taille d'un pixel logique
	CDC memDC;
	CBitmap bitmap;
	CRect Rect;
	GetClientRect(&Rect);
	int decalage = 20;
	int LargW = Rect.Width();
	int HautW = Rect.Height();
	memDC.CreateCompatibleDC(&dc);
	bitmap.CreateCompatibleBitmap(&dc, LargW, HautW);
	CBitmap* pOldBitmap = memDC.SelectObject(&bitmap);
	memDC.BitBlt(0, 0, LargW, HautW, &dc, 0, 0, SRCCOPY);

	float coefx = MesDatasMod.m_maxs[0] - MesDatasMod.m_mins[0];
	float coefy = MesDatasMod.m_maxs[1] - MesDatasMod.m_mins[1];

	if (MesDatasMod.m_nbParametres == 2)
	{
		ParametreGraphique();
		std::mutex dcMutex;
		// Fonction pour traiter une portion de l'écran
		auto process_section = [&](int startRow, int endRow, CDC* pDC)
			{
				float Rouge, Vert, Bleu;
				float cxp, cyp;

				for (int screenY = startRow; screenY < endRow; screenY += step)
				{
					for (int screenX = WinLeft; screenX < WinRight; screenX += step)
					{
						// Convertir les coordonnées écran en coordonnées normalisées [0,1]
						float normalizedX = (float)(screenX - Bx) / Ax;
						float normalizedY = (float)(screenY - By) / Ay;
						// Convertir les coordonnées normalisées en vraies valeurs
						cxp = normalizedX * coefx + MesDatasMod.m_mins[0];
						cyp = normalizedY * coefy + MesDatasMod.m_mins[1];
						// Préparer les données pour la prédiction
						Mat sampleMat = (Mat_<float>(1, 2) << cxp, cyp);
						float v = 0.0f;
						v = Decide(ptr, TypeClassifieur, sampleMat);
						// Déterminer les couleurs
						int fv = (int)v;
						unsigned int c = fv; // +5

						//if (MesDatasMod.m_nbClasses == 2) {
						//	if (fv == 1) { Rouge = 255; Vert = 255; Bleu = 0; }
						//	else { Rouge = 0; Vert = 255; Bleu = 0; }
						//}
						//else {
							//if ((c % 3) == 0) Rouge = 255 - (8 * c); else Rouge = 0;
							//if (((c + 1) % 3) == 0) Vert = 255 - (8 * c); else Vert = 0;
							//if (((c + 2) % 3) == 0) Bleu = 255 - (8 * c); else Bleu = 0;
//						}
						// Dessiner le pixel

						if (c < colorPalette.size())
						{
							uchar dec = 64;
							Rouge = max(colorPalette[c][2] - dec, 0);
							Vert = max(colorPalette[c][1] - dec, 0);
							Bleu = max(colorPalette[c][0] - dec, 0);
						}
						else { Rouge = Vert = Bleu = 200; }
						std::lock_guard<std::mutex> lock(dcMutex);
						pDC->FillSolidRect(screenX, screenY, step, step, RGB((int)Rouge, (int)Vert, (int)Bleu));
					}
				}
			};

		// Définir le nombre de threads
		int numThreads = m_NbThread; // Par exemple, utiliser 4 threads
		std::vector<std::thread> threads;
		int rowsPerThread = (WinBottom - WinTop) / numThreads;
		// Lancer les threads
		for (int t = 0; t < numThreads; ++t) {
			int startRow = WinTop + t * rowsPerThread;
			int endRow = (t == numThreads - 1) ? WinBottom : startRow + rowsPerThread;
			threads.emplace_back(process_section, startRow, endRow, &memDC);
		}
		// Attendre la fin des threads
		for (auto& thread : threads) {
			thread.join();
		}
		AfficheSample(&memDC, 1, LargW, HautW, decalage, false);
	} // si NPAR =2

	else  AfficheSample(&memDC, 1, LargW, HautW, decalage, true);
	// Copier le contexte mémoire vers le DC principal
	dc.BitBlt(0, 0, LargW, HautW, &memDC, 0, 0, SRCCOPY);
	// Nettoyer
	memDC.SelectObject(pOldBitmap);
	bitmap.DeleteObject();

	LastType = TypeClassifieur;
}

void CChildFrame::OnRandomforestRandomforestopencvapprentissage()
{
	Cleannonmodal(true);
	if (MesDatasMod.m_nbParametres == 0 || MesDatasMod.m_nbClasses == 1) 		OnConfigurationLireauformatcsv();
	if (MesDatasMod.m_allData.empty()) return;

	int maxDepth;
	int minSampleCount;
	int maxTrees;
	int activeVarCount;
	//float forestAccuracy ;
	BOOL useSurrogates;
	CRandomForestSettingsDialog dlg;
	dlg.InitializeDefaultValues((int)sqrt(MesDatasMod.m_nbParametres));

	if (dlg.DoModal() == IDOK)
	{
		// Récupérer les valeurs des paramètres
		maxDepth = dlg.m_MaxDepth;
		minSampleCount = dlg.m_MinSampleCount;
		maxTrees = dlg.m_MaxCategories;
		activeVarCount = dlg.m_ActiveVarCount;
		//float forestAccuracy = dlg.m_ForestAccuracy;
		useSurrogates = dlg.m_UseSurrogates;
	}
	else return;

	CFileDialog Model_dia(FALSE, "cvrf", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Model Files (*.cvrf)|*.cvrf|" "All Files (*.*)|*.*||");
	CString Path_Model;
	Model_dia.m_ofn.lpstrTitle = _T("Enregistrer le modèle");
	cv::String pm;
	if (Model_dia.DoModal() == IDOK)
	{
		Path_Model = Model_dia.GetPathName();
		Path_Model = Path_Model.Left(Path_Model.ReverseFind('.'));
		CString modelFilename;
		modelFilename.Format(
			_T("__RandomForest_trees_%d_depth_%d_surrogates_%d_minsamplecount_%d.cvrf"),
			maxTrees,       // Nombre d'arbres
			maxDepth,       // Profondeur maximale
			useSurrogates, minSampleCount  // Utilisation des surrogates (0 ou 1)
		);
		Path_Model = Path_Model + modelFilename;
		pm.assign(Path_Model);
	}
	else return;

	//Mat trainData = Mat(MesDatas.NbMesTotApp, MesDatas.NPAR, CV_32FC1);
	//Mat Label = Mat(MesDatas.NbMesTotApp, 1, CV_32SC1);
	//int train_sample_count = MesDatas.NbMesTotApp;

	//unsigned int c, k;
	//k = 0;
	//for (c = 0; c < MesDatas.NbClass; c++)
	//{
	//	for (int i = 0; i < MesDatas.NbMes[c]; i++)
	//	{
	//		for (long par = 0; par < MesDatas.NPAR; par++)
	//		{
	//			trainData.at<float>(k, par) = MesDatas.TabApp[c][i * MesDatas.NPAR + par];
	//		}
	//		Label.at<int>(k, 0) = c;
	//		k++;
	//	}
	//}

	MesDatasMod.m_trainingLabels.convertTo(m_Label, CV_32SC1);

	Ptr <ml::RTrees > rt = ml::RTrees::create();
	rt->setMaxDepth(maxDepth);
	rt->setMinSampleCount(minSampleCount);
	rt->setUseSurrogates(useSurrogates);
	rt->setActiveVarCount(activeVarCount);
	rt->setTermCriteria(TermCriteria(TermCriteria::MAX_ITER + TermCriteria::EPS, maxTrees, 0.01));


	rt->train(MesDatasMod.m_trainingData, ml::ROW_SAMPLE, m_Label);
	rt->save(pm);
	CString Texte;
	Texte.Format("Fichier cvrf sauvegardé");
	MessageBox(Texte);
	//if (MesDatas.NPAR == 2) VisufrontiereOpencv(&rt, mRF);
	m_pClassifier = rt;
	LastType = mRF;
	if (MesDatasMod.m_nbParametres == 2)
	{
		Invalidate();
	}
	m_Loaded = false;
}


void CChildFrame::OnRandomforestRandomforestopencvvisufrontiere()
{

	Cleannonmodal();

	if (MesDatasMod.m_nbParametres > 3) { MessageBox("Nombre d'attributs superieur à 3", MB_OK); return; }

	if (!LoadClassifier(mRF)) return;
	int nbParametres = m_pClassifier.dynamicCast<RTrees>()->getVarCount();
	if (nbParametres < 2 || nbParametres > 3 || MesDatasMod.m_nbParametres != nbParametres)
	{
		MessageBox("Nombre d'attributs incompatible avec la visualisation");
		return;
	}
	m_Loaded = true;
	if (MesDatasMod.m_nbParametres == 3 && m_pClassifier != nullptr && LastType == mRF)
	{
		m_TitreFen3D = "Classifieur Random Forest";
		Visu3d(m_pClassifier, mRF);
	}
	if (MesDatasMod.m_nbParametres == 2 && m_pClassifier != nullptr && LastType == mRF)
	{
		Invalidate();
	}


}


void CChildFrame::OnRandomforestRandomforestevaluation()
{
	String pm;
	CFileDialog FileData(TRUE, "*.cvrf", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Model (*.cvrf)|*.cvrf||", this);
	if (FileData.DoModal() != IDOK) return;
	ChaineModel = FileData.GetPathName();
	m_PathData = ChaineModel.Left(ChaineModel.ReverseFind('.'));
	pm.assign(ChaineModel);
	Ptr<RTrees> rt = Algorithm::load<RTrees>(pm);
	if (rt->getVarCount() != MesDatasMod.m_nbParametres)
	{
		MessageBox("Nombre d'attributs du modele différent du nombre d'attributs des données");
		return;
	}
	m_pClassifier = rt;
	LastType = mRF;
	Evaluation(&rt, mRF);
}


void CChildFrame::OnPerceptronMlpopencvapprentissage()
{

	Cleannonmodal(false);
	if (MesDatasMod.m_nbParametres == 0 || MesDatasMod.m_nbClasses == 1) 		OnConfigurationLireauformatcsv();
	if (MesDatasMod.m_allData.empty()) return;

	CConfigNeurone ConfigN;

	ConfigN.m_NbIter = 100;
	ConfigN.m_c1 = 2;
	ConfigN.m_HL = 1;
	ConfigN.Seuil = 0.5;
	ConfigN.NbClasses = MesDatasMod.m_nbClasses;
	ConfigN.Npar = MesDatasMod.m_nbParametres;
	if (ConfigN.DoModal() != IDOK)
	{
		return;
	}

	CFileDialog Model_dia(FALSE, "cvMlp", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Model Files (*.cvMlp)|*.cvMlp|" "All Files (*.*)|*.*||");
	CString Path_Model;
	Model_dia.m_ofn.lpstrTitle = _T("Enregistrer le modèle");
	cv::String pm;
	if (Model_dia.DoModal() == IDOK)
	{
		Path_Model = Model_dia.GetPathName();
		pm.assign(Path_Model);
	}
	else return;

	//Mat trainData = Mat(MesDatas.NbMesTotApp, MesDatas.NPAR, CV_32FC1);
	//Mat Label = Mat(MesDatas.NbMesTotApp, 1, CV_32SC1);
	//int train_sample_count = MesDatas.NbMesTotApp;

	//unsigned int c, k;
	//k = 0;
	//for (c = 0; c < MesDatas.NbClass; c++)
	//{
	//	for (int i = 0; i < MesDatas.NbMes[c]; i++)
	//	{
	//		for (long par = 0; par < MesDatas.NPAR; par++)
	//		{
	//			trainData.at<float>(k, par) = MesDatas.TabApp[c][i * MesDatas.NPAR + par];
	//		}
	//		Label.at<int>(k, 0) = c;
	//		k++;
	//	}
	//}

	MesDatasMod.m_trainingLabels.convertTo(m_Label, CV_32SC1);
	OneHot = cv::Mat::zeros(m_Label.rows, MesDatasMod.m_nbClasses, CV_32F);

	const int* labelsPtr = m_Label.ptr<int>(0);

	for (int i = 0; i < m_Label.rows; i++)
	{
		float* outputPtr = OneHot.ptr<float>(i);
		int id = labelsPtr[i];
		outputPtr[id] = 1.f;
	}
	//cv::FileStorage file("mamat.txt", cv::FileStorage::WRITE);
	//file << "onehot"<<OneHot;
	//file.release();

	Fenetre_Progression = new CDaliogProgression;
	Fenetre_Progression->Create(IDD_DIALOG_PROGRESS, NULL);
	Fenetre_Progression->Valeur_Maxi = ConfigN.m_NbIter;
	Fenetre_Progression->m_ControlProgressBar.SetRange(0, ConfigN.m_NbIter);
	Fenetre_Progression->SetWindowTextA("Apprentissage en cours");
	Fenetre_Progression->ShowWindow(SW_SHOW);

	const int hlz = 2;

	Ptr <ANN_MLP> mlp = ANN_MLP::create();
	mlp->setTermCriteria(TermCriteria(TermCriteria::MAX_ITER + TermCriteria::EPS, 1, 1e-6));
	Mat_<int> layerSizes(1, ConfigN.m_HL + 2);
	layerSizes(0, 0) = MesDatasMod.m_nbParametres;
	if (ConfigN.m_HL > 0) layerSizes(0, 1) = max(ConfigN.m_c1, 2);
	if (ConfigN.m_HL > 1) layerSizes(0, 2) = max(ConfigN.m_c2, 2);
	if (ConfigN.m_HL > 2) layerSizes(0, 3) = max(ConfigN.m_c3, 2);
	//layerSizes(0, 1) = hlz;
	layerSizes(0, ConfigN.m_HL + 1) = MesDatasMod.m_nbClasses;
	mlp->setLayerSizes(layerSizes);

	mlp->setTrainMethod(ANN_MLP::BACKPROP, ConfigN.m_eta);
	if (ConfigN.m_activation == 1) mlp->setActivationFunction(ANN_MLP::ActivationFunctions::SIGMOID_SYM);
	if (ConfigN.m_activation == 0)  mlp->setActivationFunction(ANN_MLP::ActivationFunctions::IDENTITY);
	if (ConfigN.m_activation == 2) mlp->setActivationFunction(ANN_MLP::ActivationFunctions::RELU);

	Ptr<TrainData> trainingData = TrainData::create(MesDatasMod.m_trainingData, SampleTypes::ROW_SAMPLE, OneHot);

	mlp->setTermCriteria(TermCriteria(TermCriteria::MAX_ITER + TermCriteria::EPS, 10, 1e-6)); // pour demarrer
	bool success = mlp->train(trainingData);

	long displayInterval = ConfigN.m_NbIter / 10;
	int currentIteration = 0;
	while (currentIteration < ConfigN.m_NbIter) {
		// Limiter le nombre d'itérations pour cet appel
		int iterationsLeft = std::min(displayInterval, ConfigN.m_NbIter - currentIteration);
		mlp->setTermCriteria(TermCriteria(TermCriteria::MAX_ITER + TermCriteria::EPS, iterationsLeft, 1e-6));

		// Entraîner pour les itérations limitées
		//bool success = mlp->train(trainingData);
		bool success = mlp->train(trainingData, cv::ml::ANN_MLP::TrainFlags::UPDATE_WEIGHTS);

		if (!success) {
			//cerr << "Erreur : échec de l'entraînement." << endl;
			break;
		}

		// Afficher la progression
		currentIteration += iterationsLeft;
		float progress = (currentIteration * 100.0f) / ConfigN.m_NbIter;
		//cout << "Progression : " << progress << "%" << endl;

		// Optionnel : Vérifier les performances intermédiaires (perte, précision, etc.)
		// Par exemple, calculer l'erreur sur les données d'entraînement
		Mat response;
		Mat labels = trainingData->getResponses();
		mlp->predict(trainingData->getSamples(), response);

		//FileStorage fs("label.txt", FileStorage::WRITE); // Ouvre le fichier en mode écriture
		//if (!fs.isOpened()) {
		//	return;
		//}
		//fs << "label" << labels; // Sauvegarde la matrice
		//fs.release();

		//FileStorage fsb("reponse.txt", FileStorage::WRITE); // Ouvre le fichier en mode écriture
		//if (!fsb.isOpened()) {
		//	return;
		//}
		//fsb << "response" << response; // Sauvegarde la matrice
		//fsb.release();

		double error = norm(labels, response, NORM_L2) / labels.rows;
		//cout << "Erreur intermédiaire : " << error << endl;
		Fenetre_Progression->m_ControlProgressBar.SetPos(currentIteration);
		Fenetre_Progression->m_Affiche.Format("Erreur en cours : %3.5f", error);
		Fenetre_Progression->UpdateData(false);
	}

	//cout << "Entraînement terminé après " << currentIteration << " itérations." << endl;


	//mlp->train(trainingData);
	mlp->save(pm);
	CString Texte;
	if (mlp->isTrained()) Texte.Format("Fichier cvMlp sauvegardé");
	else  Texte.Format("Probleme de convergence");
	MessageBox(Texte);
	Fenetre_Progression->DestroyWindow();
	delete Fenetre_Progression;

	m_pClassifier = mlp;
	LastType = mMLP;
	if (MesDatasMod.m_nbParametres == 2)
	{
		Invalidate();
	}
	m_Loaded = false;
}


void CChildFrame::OnPerceptronMlpopencvvisufrontiere()
{
	Cleannonmodal(true);

	if (MesDatasMod.m_nbParametres > 3) { MessageBox("Nombre d'attributs superieur à 3", MB_OK); return; }

	if (!LoadClassifier(mMLP)) return;
	int nbParametres = m_pClassifier.dynamicCast<ANN_MLP>()->getVarCount();
	if (nbParametres < 2 || nbParametres > 3 || MesDatasMod.m_nbParametres != nbParametres)
	{
		MessageBox("Nombre d'attributs incompatible avec la visualisation");
		return;
	}
	m_Loaded = true;
	if (MesDatasMod.m_nbParametres == 3 && m_pClassifier != nullptr && LastType == mMLP)
	{
		m_TitreFen3D = "Classifieur MLP";
		Visu3d(m_pClassifier, mMLP);
	}
	if (MesDatasMod.m_nbParametres == 2 && m_pClassifier != nullptr && LastType == mMLP)
	{
		Invalidate();
	}
}


void CChildFrame::OnPerceptronMlpopencvevaluation()
{
	Cleannonmodal(false);
	String pm;
	CFileDialog FileData(TRUE, "*.cvMlp", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Model (*.cvMlp)|*.cvMlp||", this);
	if (FileData.DoModal() != IDOK) return;
	ChaineModel = FileData.GetPathName();
	m_PathData = ChaineModel.Left(ChaineModel.ReverseFind('.'));
	pm.assign(ChaineModel);
	Ptr<ANN_MLP> mlp = Algorithm::load<ANN_MLP>(pm);
	if (mlp->getVarCount() != MesDatasMod.m_nbParametres)
	{
		MessageBox("Nombre d'attributs du modele différent du nombre d'attributs des données");
		return;
	}
	m_pClassifier = mlp;
	LastType = mMLP;
	Evaluation(&mlp, mMLP);
}


void CChildFrame::OnSizing(UINT nSide, LPRECT lpRect)
{
	// Calculer la largeur et la hauteur actuelles
	int width = lpRect->right - lpRect->left;
	int height = lpRect->bottom - lpRect->top;

	// Ajuster pour maintenir un aspect carré
	int newSize = width;

	// Ajuster les dimensions du rectangle de redimensionnement
	switch (nSide)
	{
	case WMSZ_LEFT:
	case WMSZ_RIGHT:
		lpRect->bottom = lpRect->top + newSize;
		break;
	case WMSZ_TOP:
	case WMSZ_BOTTOM:
		lpRect->right = lpRect->left + height;
		break;
	case WMSZ_TOPLEFT:
	case WMSZ_BOTTOMRIGHT:
		lpRect->right = lpRect->left + newSize;
		lpRect->bottom = lpRect->top + newSize;
		break;
	case WMSZ_TOPRIGHT:
	case WMSZ_BOTTOMLEFT:
		lpRect->right = lpRect->left + newSize;
		lpRect->bottom = lpRect->top + newSize;
		break;
	}

	Invalidate(TRUE);
}

void CChildFrame::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
	CMDIChildWnd::OnGetMinMaxInfo(lpMMI);

	//// Obtenir la taille de l'écran de travail
	//CRect workArea;
	//SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);

	//// Déterminer la plus petite dimension pour forcer un carré
	//int maxSize = min(workArea.Width(), workArea.Height())-100;

	//// Vérifier si la fenêtre est en cours de maximisation
	//WINDOWPLACEMENT wp;
	//GetWindowPlacement(&wp);

	//if (wp.showCmd == SW_MAXIMIZE)
	//{
	//	// Définir la taille maximale pour l'état maximisé
	//	lpMMI->ptMaxSize.x = maxSize;  // Largeur maximale
	//	lpMMI->ptMaxSize.y = maxSize;  // Hauteur maximale

	//	// Centrer la fenêtre maximisée
	//	lpMMI->ptMaxPosition.x = 100+(workArea.Width() - maxSize) / 2;
	//	lpMMI->ptMaxPosition.y =100+ (workArea.Height() - maxSize) / 2;
	//}

	//// Taille minimale carrée (facultatif)
	//int minSize = 200; // Taille minimale arbitraire
	//lpMMI->ptMinTrackSize.x = minSize;
	//lpMMI->ptMinTrackSize.y = minSize;

	//// Autoriser une redimension normale
	//lpMMI->ptMaxTrackSize.x = workArea.Width();  // Largeur maximale pour redimensionnement manuel
	//lpMMI->ptMaxTrackSize.y = workArea.Height(); // Hauteur maximale pour redimensionnement manuel
}

void CChildFrame::OnConfigurationSauverauformatcsv()
{
	// TODO: ajoutez ici le code de votre gestionnaire de commande
	CFileDialog Model_dia(FALSE, "csv", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		"Model Files (*.csv)|*.csv|" "All Files (*.*)|*.*||");
	Model_dia.m_ofn.lpstrTitle = _T("Enregistrer les données");

	CString Path_Data;
	cv::String pm;
	if (Model_dia.DoModal() == IDOK)
	{
		Path_Data = Model_dia.GetPathName();
		pm.assign(Path_Data);
	}
	else return;

	MesDatasMod.SaveToCSV(Path_Data.GetBuffer(Path_Data.GetLength() + 1));
}




void CChildFrame::OnConfigurationLireauformatcsv()
{
	int IdP = 121212;
	if (m_closeCallback != nullptr)
	{
		if (!m_closeCallback->IsWindowClosed())
		{
			MessageBox("Veuillez fermer la fenêtre de rendu 3D"); return;
		}
	}
	Cleannonmodal(true);

	int proportion = 2;
	CString Path_Data;
	cv::String pm;
	//CFileDialog dlg(TRUE, "csv", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
	//	"Model Files (*.csv)|*.csv|" "All Files (*.*)|*.*||");
	//dlg.m_ofn.lpstrTitle = _T("Lire les données");

	//if (dlg.DoModal() == IDOK)
	//{
	//	Path_Data = dlg.GetPathName();
	//	pm.assign(Path_Data);
	//}
	//else return;

	CFileDialogCustom dlg(TRUE, _T("csv"), NULL, OFN_FILEMUSTEXIST, _T("Fichiers CSV (*.csv)|*.csv||"));
	dlg.m_ofn.lpstrTitle = _T("Lire les données");
	dlg.AddText(IdP - 2, "Proportion en % d'échantillons d'apprentissage");
	dlg.AddText(IdP - 1, "(95, 50, 25 ou 12)");
	dlg.AddEditBox(IdP, "50");

	if (dlg.DoModal() == IDOK) {
		Path_Data = dlg.GetPathName();
		pm.assign(Path_Data);
		proportion = dlg.proportion; // Récupérer la proportion saisie par l'utilisateur

		if (proportion != 95 && proportion != 50 && proportion != 25 && proportion != 12) {
			MessageBox(_T("Veuillez entrer la valeur 95, 50, 25 ou 12"), _T("Erreur"), MB_OK | MB_ICONERROR);
			return;
		}
	}
	else 	return; // L'utilisateur a annulé

	//if (proportion == 100) proportion = 1;
	//if (proportion == 50) proportion = 2;
	//if (proportion == 25) proportion = 4;
	//if (proportion == 12) proportion = 8;


//	int r = MesDatas.LireDonneeCSV(Path_Data, proportion, false, true, false);
	int r = MesDatasMod.LireDonneeCSV((LPCTSTR)Path_Data, proportion, false, true, false);


	//int r = 0;
	//m_pTrainData = cv::ml::TrainData::loadFromCSV((LPCTSTR)Path_Data, 1, 0,-1,cv::String(),',');
	//m_nbParametres = m_pTrainData->getNVars();
	//cv::Mat responses = m_pTrainData->getResponses();
	//// Trouver le nombre de classes distinctes
	//std::vector<float> uniqueClasses;
	//uniqueClasses=unique(responses,true );  // Trouve les valeurs uniques dans les réponses
	//m_nbClasses = uniqueClasses.size();  // Le nombre de classes est le nombre de valeurs uniques

	if (r == -1)
	{
		MessageBox("Erreur de fichier CSV");
		return;
	}
	if (r == -2)
	{
		MessageBox("Donnees manquantes dans le fichier CSV");
		return;
	}
	if (r == -3)
	{
		MessageBox("Fichier CSV incorrect");
		return;
	}
	if (r == -4) MessageBox("Attention certaines classes contiennent moins de 12 échantillons ce qui est trop faible");

	if (r == -5)
	{
		MessageBox("Attention certaines classes sont vides, le numéro des classes ne correspondra pas au CSV");
	}
	if (r == -6)
	{
		MessageBox("Attention certaines classes contiennent moins de 12 échantillons ce qui est trop faible");
		MessageBox("Attention certaines classes sont vides, le numéro des classes ne correspondra pas au CSV");
	}
	NomFichMinMax = Path_Data.Left(Path_Data.ReverseFind('.')) + ".lim";

	//		MesDatas.SauveMinMax(NomFichMinMax);
	//		MesDatas.NormaliseSimple(); // pour la visualisation
		//	MesDatas.NormaliseVisu();

	ValideDecision = FALSE;
	ValideMenu = FALSE;
	ValideVisuMes = TRUE;

	m_pClassifier = nullptr;
	Invalidate();
	//if (m_renderer == nullptr && MesDatas.NPAR==3) InitialisationVTK();

	OnBoostingopencvReset();

}


//void CChildFrame::OnSvmSvmopencvdecisionsurfichierdedonnee()
//{
	// TODO: Add your command handler code here
	//int par, i, cf, Npar = 0, Nbmes;
	//CDonneeFloat Datas;
	//CString Temp;
	//MessageBox("Chargez d'abord le fichier dat puis le modele de classification\n\n Merci de vérifier la compatibilté de vos fichiers dat,\n Premiere ligne : nombre d'attributs\n Deuxième ligne : Nobmre d'échantillons\nEnsuite un echantillon par ligne, séparateur espace", MB_OK);

	//CFileDialog FileData2(TRUE, "*.dat", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "(*.dat)|*.dat||", this);
	//if (FileData2.DoModal() == IDOK)
	//{
	//	Temp = FileData2.GetPathName();
	//}
	//else return;

	//FILE* FicTest = fopen(Temp, "rb");
	//Temp = Temp + "_Result_classif_SVM.txt";
	//FILE* FicRes = fopen(Temp, "w");
	//fscanf(FicTest, "%d\n", &Npar);
	//fscanf(FicTest, "%d\n", &Nbmes);
	//Datas.InitMem(Npar, 1, Nbmes);

	//float aa;
	//double v;
	//for (i = 0; i < Nbmes; i++)
	//{
	//	for (par = 0; par < Npar - 1; par++)
	//	{
	//		fscanf(FicTest, "%f ", &aa);
	//		Datas.Tab[0][i * Datas.NPAR + par] = aa;
	//	}
	//	fscanf(FicTest, "%f\n", &aa);
	//	Datas.Tab[0][i * Datas.NPAR + par] = aa;
	//} //
	//fclose(FicTest);

	//String pm;
	//CFileDialog FileData(TRUE, "*.cvmodel", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Model (*.cvmodel)|*.cvmodel||", this);
	//if (FileData.DoModal() != IDOK) return;
	//ChaineModel = FileData.GetPathName();
	//pm.assign(ChaineModel);
	//Ptr<SVM> svm = Algorithm::load<SVM>(pm);

	//DecidDataOpencv(&svm, Datas, FicRes, 1);
	//fclose(FicRes);
	//MessageBox("le fichier " + Temp + " a été sauvegardé ", MB_OK);

//}

CString  CChildFrame::LoadData(CDonneeFloatMod& datas)
{
	CFileDialogCustom dlg(TRUE, _T("csv"), NULL, OFN_FILEMUSTEXIST, _T("Fichiers CSV (*.csv)|*.csv||"));
	dlg.m_ofn.lpstrTitle = _T("Lire les données");
	CString Path_Data;
	string pm;

	if (dlg.DoModal() == IDOK)
	{
		Path_Data = dlg.GetPathName();
		pm.assign(Path_Data);
	}
	else 	return ""; // L'utilisateur a annulé

	int r = datas.LireDonneeCSV((LPCTSTR)Path_Data, 100, false, false, false);
	return Path_Data;
}


void CChildFrame::CrossValidateSVM(int k)
{
	Cleannonmodal(false);
	CString foldMsg = "";
	if (MesDatasMod.m_nbParametres == 0 || MesDatasMod.m_nbClasses == 1) 		OnConfigurationLireauformatcsv();
	if (MesDatasMod.m_nbParametres == 0) return;
	CSVMParametres Boite_svm;
	Boite_svm.m_TrainAuto = FALSE;
	Boite_svm.B_Learning = TRUE;
	if (Boite_svm.DoModal() != IDOK)
	{
		return;
	}

	Ptr <cv::ml::SVM > svm = cv::ml::SVM::create();
	if (Boite_svm.p.kernel_type == RBF)
	{
		svm->setKernel(cv::ml::SVM::RBF);
		svm->setGamma(Boite_svm.p.gamma);
	}
	if (Boite_svm.p.kernel_type == LINEAR)
	{
		svm->setKernel(cv::ml::SVM::LINEAR);
	}
	if (Boite_svm.p.kernel_type == POLY)
	{
		svm->setKernel(cv::ml::SVM::POLY);
		svm->setDegree(Boite_svm.p.degree);
		svm->setGamma(Boite_svm.p.gamma);
		svm->setCoef0(Boite_svm.p.coef0);
	}
	svm->setType(cv::ml::SVM::C_SVC);

	cv::Mat allData = MesDatasMod.m_allData;
	cv::Mat allLabels = MesDatasMod.m_allLabels;

	int numSamples = allData.rows;
	if (numSamples < k) {
		//MessageBox("Pas assez d'échantillons pour k folds.", "Erreur", MB_OK | MB_ICONERROR);
		return;
	}

	std::map<int, std::vector<int>> indicesParClasse;
	for (int i = 0; i < numSamples; i++) {
		int label = allLabels.at<int>(i, 0);
		indicesParClasse[label].push_back(i);
	}

	// Mélanger les indices pour éviter les biais
	std::random_device rd;
	std::mt19937 g(rd());
	for (auto& [label, indices] : indicesParClasse) {
		shuffle(indices.begin(), indices.end(), g);
	}

	// Répartition stratifiée en k-folds
	std::vector<std::vector<int>> folds(k);
	int foldIndex = 0;
	for (const auto& [label, indices] : indicesParClasse) {
		for (int idx : indices) {
			folds[foldIndex % k].push_back(idx);
			foldIndex++;
		}
	}

	std::vector<double> accuracies; // Stocker les précisions de chaque fold
	std::vector<double> recalls; // Stocker le rappel moyen de chaque fold

	for (int fold = 0; fold < k; fold++)
	{
		cv::Mat trainData, trainLabels, testData, testLabels;

		// Construire les ensembles d'entraînement et de test
		for (int i = 0; i < k; i++) {
			for (int idx : folds[i]) {
				if (i == fold) {
					testData.push_back(allData.row(idx));
					testLabels.push_back(allLabels.row(idx));
				}
				else {
					trainData.push_back(allData.row(idx));
					trainLabels.push_back(allLabels.row(idx));
				}
			}
		}

		// Convertir les labels en format compatible
		trainLabels.convertTo(trainLabels, CV_32SC1);
		testLabels.convertTo(testLabels, CV_32SC1);

		svm->train(trainData, cv::ml::ROW_SAMPLE, trainLabels);

		// Évaluation du modèle
		int correctPredictions = 0;
		std::map<int, int> TP, FN; // Stocker TP et FN par classe
		std::map<int, int> totalActual; // Nombre d'instances par classe

		for (int i = 0; i < testData.rows; i++) {
			int predictedLabel = (int)svm->predict(testData.row(i));
			int trueLabel = (int)testLabels.at<int>(i, 0);

			if (predictedLabel == trueLabel) {
				correctPredictions++;
				TP[trueLabel]++;
			}
			else {
				FN[trueLabel]++;
			}
			totalActual[trueLabel]++;
		}

		// Calcul de l'accuracy
		double accuracy = (double)correctPredictions / testData.rows;
		accuracies.push_back(accuracy);

		// Calcul du recall moyen pour ce fold
		double recallSum = 0.0;
		int numClasses = totalActual.size();

		for (const auto& [label, total] : totalActual) {
			double recall = (total > 0) ? (double)TP[label] / total : 0.0;
			recallSum += recall;
		}

		double meanRecall = recallSum / numClasses;
		recalls.push_back(meanRecall);
		CString temp;
		temp.Format("Fold %d/%d - Accuracy: %.2f%% - Recall: %.2f%%\n", fold + 1, k, accuracy * 100, meanRecall * 100);
		foldMsg += temp;
		//MessageBox(foldMsg, "Validation Croisée", MB_OK);
	}

	// Moyenne des performances
	double meanAccuracy = accumulate(accuracies.begin(), accuracies.end(), 0.0) / accuracies.size();
	double meanRecall = accumulate(recalls.begin(), recalls.end(), 0.0) / recalls.size();
	CString finalMsg;
	finalMsg.Format("\nValidation Croisée %d-fold\nPrécision Moyenne: %.2f%%\nRappel Moyen: %.2f%%", k, meanAccuracy * 100, meanRecall * 100);
	MessageBox(foldMsg + finalMsg, "Validation Croisée Terminée", MB_OK);
}


void CChildFrame::OnSvmSvmopencvdecisionsurfichierdedonnee()
{

	Cleannonmodal(true);
	CDonneeFloatMod datas;
	CString Path_Data = LoadData(datas);
	if (Path_Data == "") return;
	// Générer le fichier de résultats
	CString resultFilePath = Path_Data + "_Result_classif_SVM.txt";
	std::ofstream fileRes(resultFilePath.GetString());
	if (!fileRes.is_open()) {
		MessageBox("Impossible de créer le fichier de résultats.", MB_OK);
		return;
	}

	// Ouvrir le fichier du modèle SVM
	CFileDialog fileModelDialog(TRUE, "*.cvmodel", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Modèles SVM (*.cvmodel)|*.cvmodel||", this);
	if (fileModelDialog.DoModal() != IDOK) {
		return;
	}

	CString modelPath = fileModelDialog.GetPathName();
	std::string modelPathStr = LPCTSTR(modelPath.GetString());
	Ptr<SVM> svm = Algorithm::load<SVM>(modelPathStr);
	if (svm.empty()) {
		MessageBox("Impossible de charger le modèle SVM.", MB_OK);
		return;
	}

	// Effectuer la classification
	DecidDataOpencv(&svm, datas, fileRes, mSVM);

	fileRes.close();
	MessageBox("Le fichier de résultats " + resultFilePath + " a été sauvegardé.", MB_OK);
}


void CChildFrame::OnKppvKppvopencvdecisionsurfichierdedonnees()
{
	Cleannonmodal(true);
	CDonneeFloatMod datas;
	CString filePath;
	// Chargement du fichier de donnee
	filePath = LoadData(datas);
	if (filePath == "") return;

	// Générer le fichier de résultats
	CString resultFilePath = filePath + "_Result_classif_KPPV.txt";
	std::ofstream fileRes(resultFilePath.GetString());
	if (!fileRes.is_open()) {
		MessageBox("Impossible de créer le fichier de résultats.", MB_OK);
		return;
	}

	// Ouvrir le fichier du modèle SVM
	CFileDialog fileModelDialog(TRUE, "*.cvkppv", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Modèles KPPV (*.cvkppv)|*.cvkppv||", this);
	if (fileModelDialog.DoModal() != IDOK) {
		return;
	}
	String pm;
	ChaineModel = fileModelDialog.GetPathName();
	pm.assign(ChaineModel);
	Ptr<KNearest> kppv = Algorithm::load<KNearest>(pm);
	if (kppv.empty()) {
		MessageBox("Impossible de charger le modèle Kppv.", MB_OK);
		return;
	}

	// Effectuer la classification
	DecidDataOpencv(&kppv, datas, fileRes, mKPPV);

	fileRes.close();
	MessageBox("Le fichier de résultats " + resultFilePath + " a été sauvegardé.", MB_OK);
}


void CChildFrame::OnPerceptronMlpopencvdecisionsurfichierdedonnees()
{
	Cleannonmodal(true);
	CDonneeFloatMod datas;
	CString filePath;
	// Chargement du fichier de donnee
	filePath = LoadData(datas);
	if (filePath == "") return;

	// Générer le fichier de résultats
	CString resultFilePath = filePath + "_Result_classif_MLP.txt";
	std::ofstream fileRes(resultFilePath.GetString());
	if (!fileRes.is_open()) {
		MessageBox("Impossible de créer le fichier de résultats.", MB_OK);
		return;
	}

	// Ouvrir le fichier du modèle MLP
	std::string pm;
	CFileDialog FileData(TRUE, "*.cvMlp", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Model (*.cvMlp)|*.cvMlp||", this);
	if (FileData.DoModal() != IDOK) return;
	ChaineModel = FileData.GetPathName();
	pm.assign(ChaineModel);
	Ptr<ANN_MLP> mlp = Algorithm::load<ANN_MLP>(pm);
	if (mlp.empty()) {
		MessageBox("Impossible de charger le modèle MLP.", MB_OK);
		return;
	}

	// Effectuer la classification
	DecidDataOpencv(&mlp, datas, fileRes, mMLP);

	fileRes.close();
	MessageBox("Le fichier de résultats " + resultFilePath + " a été sauvegardé.", MB_OK);
}


void CChildFrame::OnRandomforestRandomforestopencvdecisionfichierdedonnee()
{
	CDonneeFloatMod datas;
	CString filePath;
	// Chargement du fichier de donnee
	filePath = LoadData(datas);
	if (filePath == "") return;

	// Générer le fichier de résultats
	CString resultFilePath = filePath + "_Result_classif_RF.txt";
	std::ofstream fileRes(resultFilePath.GetString());
	if (!fileRes.is_open()) {
		MessageBox("Impossible de créer le fichier de résultats.", MB_OK);
		return;
	}

	// Ouvrir le fichier du modèle RandomForest
	String pm;
	CFileDialog FileData(TRUE, "*.cvrf", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Model (*.cvrf)|*.cvrf||", this);
	if (FileData.DoModal() != IDOK) return;
	ChaineModel = FileData.GetPathName();
	pm.assign(ChaineModel);
	Ptr<RTrees> rt = Algorithm::load<RTrees>(pm);

	// Effectuer la classification
	DecidDataOpencv(&rt, datas, fileRes, mRF);

	fileRes.close();
	MessageBox("Le fichier de résultats " + resultFilePath + " a été sauvegardé.", MB_OK);
}


void CChildFrame::OnSvmSvmopencvtraindynamique()
{
	if (MesDatasMod.m_nbClasses == 1) 		OnConfigurationLireauformatcsv();
	if (MesDatasMod.m_nbParametres != 2 && MesDatasMod.m_nbParametres != 3)
	{
		MessageBox("Fonctionne uniquement pour des données à 2  ou 3 attributs");
		return;
	}

	Cleannonmodal(true);
	m_Loaded = true;
	m_VisuDynamique = true;
	if (!m_pSVMParamDlg)
	{

		m_pSVMParamDlg = new CSVMParametres();
		m_pSVMParamDlg->m_TrainAuto = false;
		m_pSVMParamDlg->Create(this);
		m_pSVMParamDlg->SetWindowText("SVM à noyau RBF - le curseur permet de choisir la valeur de GAMMA");
		m_pSVMParamDlg->m_ControlSliderGamma.EnableWindow(TRUE);
		m_pSVMParamDlg->m_ControleAffGamma.EnableWindow(TRUE);
		m_pSVMParamDlg->ShowWindow(SW_SHOW);
		m_pSVMParamDlg->SetParentFrame(this); // Passer le pointeur du parent
		m_pSVMParamDlg->m_Control_Rbf.SetCheck(1);
		m_pSVMParamDlg->OnCheckRbf();
		m_pSVMParamDlg->m_Control_Lineaire.EnableWindow(FALSE);
		m_pSVMParamDlg->m_Control_Rbf.EnableWindow(FALSE);
		m_pSVMParamDlg->m_Control_Sigmo_s.EnableWindow(FALSE);
		m_pSVMParamDlg->m_Control_Poly.EnableWindow(FALSE);
		m_pSVMParamDlg->m_B_auto.EnableWindow(FALSE);
	}
	else
	{
		m_pSVMParamDlg->ShowWindow(SW_SHOW);
		m_pSVMParamDlg->SetParentFrame(this); // Passer le pointeur du parent
	}
	if (m_Label.empty())
	{
		//m_trainData.release();
		m_Label.release();
		//m_trainData = Mat(MesDatas.NbMesTotApp, MesDatas.NPAR, CV_32FC1);
		//MesDatasMod.m_trainingData.copyTo(m_trainData);
		MesDatasMod.m_trainingLabels.convertTo(m_Label, CV_32SC1);
		//m_Label = Mat(MesDatas.NbMesTotApp, 1, CV_32SC1);
		int train_sample_count = MesDatasMod.m_nbMesTotalApp;// NbMesTotApp;
		//int k, i, par;
		//unsigned int c;
		//k = 0;
		//for (c = 0; c < MesDatas.NbClass; c++)
		//{
		//	for (i = 0; i < MesDatas.NbMes[c]; i++)
		//	{
		//		for (par = 0; par < MesDatas.NPAR; par++)
		//		{
		//			m_trainData.at<float>(k, par) = MesDatas.TabApp[c][i * MesDatas.NPAR + par];
		//		}
		//		m_Label.at<int>(k, 0) = c;
		//		k++;
		//	}
		//}




		Ptr <cv::ml::SVM > svm = cv::ml::SVM::create();
		svm->setKernel(cv::ml::SVM::RBF);
		svm->setGamma(1.0);
		svm->train(MesDatasMod.m_trainingData, cv::ml::ROW_SAMPLE, m_Label);

		m_pClassifier = svm;
		LastType = mSVM;

		SMetriques m = Evaluation(&m_pClassifier, mSVM, false);
		m_pSVMParamDlg->m_TexteErreur.Format("Erreur =      %1.3f\nRappel =    %1.3f\nPrecision = %1.3f", m.ErreurGlobale, m.Rappel, m.Precision);
		m_pSVMParamDlg->UpdateData(false);
		Invalidate();
		if (MesDatasMod.m_nbParametres == 3)
		{
			m_TitreFen3D = "Classifieur SVM";
			Visu3d(m_pClassifier, LastType);
		}
	}
}

void CChildFrame::UpdateSVMParameters(float gammaValue, CSVMParametres* pboitesvm)
{
	//Mettre à jour les paramètres d'apprentissage
	if (m_pClassifier && LastType == mSVM)
	{
		Ptr<SVM> pClassifier = m_pClassifier.dynamicCast<SVM>();
		// Relancer l'apprentissage
		if (m_pClassifier && m_Label.empty() == false)
		{
			//if (pboitesvm->p.kernel_type == RBF)
			//{
			//	pClassifier->setKernel(cv::ml::SVM::RBF);
			//}
			//if (pboitesvm->p.kernel_type == LINEAR)
			//{
			//	pClassifier->setKernel(cv::ml::SVM::LINEAR);
			//}
			//if (pboitesvm->p.kernel_type == POLY)
			//{
			//	pClassifier->setKernel(cv::ml::SVM::POLY);
			//	pClassifier->setDegree(pboitesvm->p.degree);
			//	pClassifier->setGamma(pboitesvm->p.gamma);
			//	pClassifier->setCoef0(pboitesvm->p.coef0);

			//}
			pClassifier->setGamma(gammaValue);
			pClassifier->train(MesDatasMod.m_trainingData, ROW_SAMPLE, m_Label);
			m_pClassifier = pClassifier;
			LastType = mSVM;
			SMetriques m = Evaluation(&pClassifier, mSVM, false);
			pboitesvm->m_TexteErreur.Format("Erreur =      %1.3f\nRappel =    %1.3f\nPrecision = %1.3f", m.ErreurGlobale, m.Rappel, m.Precision);
			pboitesvm->UpdateData(false);
			Invalidate(); // VisufrontiereOpencv(&svm, mSVM);
			if (MesDatasMod.m_nbParametres == 3)
			{
				m_TitreFen3D = "Classifieur SVM";
				Visu3d(m_pClassifier, mSVM);
			}
		}
	}
	if (m_pClassifier && LastType != mSVM)
	{
		m_Label.release(); // pour forcer l'apprentissage
		OnSvmSvmopencvtraindynamique();
	}

}

void CChildFrame::OnConfigurationRegressionLireCsv()
{
	int IdP = 121212;
	Cleannonmodal(true);


	int proportion = 2;
	CString Path_Data;
	cv::String pm;

	CFileDialogCustom dlg(TRUE, _T("csv"), NULL, OFN_FILEMUSTEXIST, _T("Fichiers CSV (*.csv)|*.csv||"));
	dlg.m_ofn.lpstrTitle = _T("Lire les données pour la régression");
	dlg.AddText(IdP - 2, "Proportion en % d'échantillons d'apprentissage");
	dlg.AddText(IdP - 1, "(100, 50, 25 ou 12)");
	dlg.AddEditBox(IdP, "50");

	if (dlg.DoModal() == IDOK) {
		Path_Data = dlg.GetPathName();
		pm.assign(Path_Data);
		proportion = dlg.proportion; // Récupérer la proportion saisie par l'utilisateur

		if (proportion != 100 && proportion != 50 && proportion != 25 && proportion != 12) {
			MessageBox(_T("Veuillez entrer la valeur 100, 50, 25 ou 12"), _T("Erreur"), MB_OK | MB_ICONERROR);
			return;
		}
	}
	else 	return; // L'utilisateur a annulé

	int r = MesDatasMod.LireDonneeCSVRegression((LPCTSTR)Path_Data, proportion, false, true, false);
	if (r == -1)
	{
		MessageBox("Erreur de fichier CSV");
		return;
	}
	if (r == -2)
	{
		MessageBox("Donnees manquantes dans le fichier CSV");
		return;
	}
	if (r == -3)
	{
		MessageBox("Fichier CSV incorrect");
		return;
	}

	NomFichMinMax = Path_Data.Left(Path_Data.ReverseFind('.')) + ".lim";

	ValideDecision = FALSE;
	ValideMenu = FALSE;
	ValideVisuMes = TRUE;

	m_pClassifier = nullptr;
	Invalidate();

}


void CChildFrame::OnSvmSvmopencvregression()
{
	CSVMParametres Boite_svm;
	Boite_svm.B_Learning = TRUE;

	if (Boite_svm.DoModal() != IDOK)
	{
		return;
	}

	// Configuration et entraînement du modèle
	Ptr<cv::ml::SVM> svm = cv::ml::SVM::create();
	svm->setType(cv::ml::SVM::EPS_SVR); // Régression avec SVM
	//svm->setKernel(Boite_svm.p.kernel_type == RBF ? cv::ml::SVM::RBF :
	//	Boite_svm.p.kernel_type == LINEAR ? cv::ml::SVM::LINEAR :
	//	Boite_svm.p.kernel_type == POLY ? cv::ml::SVM::POLY : cv::ml::SVM::LINEAR);
	svm->setKernel(cv::ml::SVM::RBF);
	svm->setGamma(Boite_svm.p.gamma);
	svm->setC(1.0);
	svm->setP(0.01); // Epsilon pour la régression EPS_SVR
	svm->setDegree(Boite_svm.p.degree);
	svm->setCoef0(Boite_svm.p.coef0);

	//FileStorage fs("label.txt", FileStorage::WRITE); // Ouvre le fichier en mode écriture
	//if (!fs.isOpened()) {
	//	return;
	//}
	//fs << "label" << m_Label; // Sauvegarde la matrice
	//fs.release();

	//FileStorage fs2("m_data.txt", FileStorage::WRITE); // Ouvre le fichier en mode écriture
	//if (!fs2.isOpened()) {
	//	return;
	//}
	//fs2 << "data" << m_trainData; // Sauvegarde la matrice
	//fs2.release();

	//Ptr<cv::ml::TrainData> tdata = cv::ml::TrainData::loadFromCSV(".\\datas\\databases\\rppg\\GreenDatasetV.csv", 0, 0, 1);
	//Mat ns;

	//tdata->setTrainTestSplitRatio(0.5, false);
	//Mat res = tdata->getTrainResponses();
	//Mat td = tdata->getTrainSamples();
	//normalize(td, ns, 0, 1, NORM_MINMAX);

	//FileStorage fs3("m_datao.txt", FileStorage::WRITE); // Ouvre le fichier en mode écriture
	//if (!fs3.isOpened()) {
	//	return;
	//}
	//fs3 << "data" << td; // Sauvegarde la matrice
	//fs3.release();
	double gopt = 0;
	//normalize(m_trainData, m_trainData, 0, 1, NORM_MINMAX);
	if (Boite_svm.m_TrainAuto)
	{
		Ptr<ParamGrid> cGrid1 = SVM::getDefaultGridPtr(SVM::C);
		//Ptr<ParamGrid> gammaGrid1 = SVM::getDefaultGridPtr(SVM::GAMMA);
		//Ptr<ParamGrid> gammaGrid1 = ParamGrid::create(0.01, Boite_svm.p.gamma, pow(10.0, 0.5));
		Ptr<ParamGrid> gammaGrid1 = ParamGrid::create(0.01, Boite_svm.p.gamma, 2);
		//Ptr<ParamGrid> gammaGrid1 = ParamGrid::create(0.001, Boite_svm.p.gamma, 100);
		//Ptr<ParamGrid> pGrid = SVM::getDefaultGridPtr(SVM::P);
		//Ptr<ParamGrid> cGrid1 = ParamGrid::create(1.0, 1.0, 1);
		//Ptr<ParamGrid> gammaGrid1 = ParamGrid::create(0.01, Boite_svm.p.gamma, 1);
		Ptr<ParamGrid> pGrid1 = ParamGrid::create(0.001, 0.1, 4);
		//svm->trainAuto(ns, cv::ml::ROW_SAMPLE, res, 2);
		svm->trainAuto(MesDatasMod.m_trainingData, cv::ml::ROW_SAMPLE, MesDatasMod.m_trainingLabels, 2, cGrid1, gammaGrid1, pGrid1);
		gopt = svm->getGamma(); // get optimum
		//svm->setGamma(gopt);
		//svm->train(m_trainData, cv::ml::ROW_SAMPLE, m_Label);
	}
	else
	{
		svm->train(MesDatasMod.m_trainingData, cv::ml::ROW_SAMPLE, MesDatasMod.m_trainingLabels);
	}

	// Sauvegarder le modèle
		// Sélectionner le fichier pour sauvegarder le modèle
	CFileDialog Model_dia(FALSE, "cvmodel", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		"Model Files (*.cvmodel)|*.cvmodel|" "All Files (*.*)|*.*||");
	Model_dia.m_ofn.lpstrTitle = _T("Enregistrer le modèle");

	CString Path_Model;
	cv::String pm;
	if (Model_dia.DoModal() == IDOK)
	{
		Path_Model = Model_dia.GetPathName();
		pm.assign(Path_Model);
	}
	else return;

	svm->save(pm);

	// Évaluer les données de test
	CString resultFilePath = Path_Model + "_Result_regression_SVM.csv";
	std::ofstream fileRes(resultFilePath.GetString());
	std::ofstream outFile(resultFilePath.GetString());
	outFile << "Valeur attendue,Valeur prédite\n";

	Mat predictedLabels = Mat(MesDatasMod.m_nbMesTotalTest, 1, CV_32FC1);
	for (int i = 0; i < MesDatasMod.m_nbMesTotalTest; i++)
	{
		float prediction = svm->predict(MesDatasMod.m_testData.row(i));
		predictedLabels.at<float>(i, 0) = prediction;
		outFile << MesDatasMod.m_testLabels.at<float>(i, 0) << "," << prediction << "\n"; // Sauvegarder les résultats
	}
	outFile.close();

	// Calculer la MSE
	Mat diff = MesDatasMod.m_testLabels - predictedLabels;  // Différence entre vrai et prédit
	Mat diffabs = abs(MesDatasMod.m_testLabels - predictedLabels);  // Valeur absolue des différences
	double mae = sum(diffabs)[0] / MesDatasMod.m_nbMesTotalTest;
	diff = diff.mul(diff);                  // Carré de la différence
	double mse = sum(diff)[0] / MesDatasMod.m_nbMesTotalTest; // Moyenne
	//FileStorage fs3("m_datatest.txt", FileStorage::WRITE); // Ouvre le fichier en mode écriture
	//if (!fs3.isOpened()) {
	//	return;
	//}
	//fs3 << "datatest" << testData; // Sauvegarde la matrice
	//fs3.release();
	CString Texte;
	Texte.Format("Modèle sauvegardé dans %s\nRésultats sauvegardés dans fichier _Result_regression_SVM.csv\n avec MSE=%3.4f \n MAE=%3.4f \n gopt=%3.3f", Path_Model, mse, mae, gopt);
	MessageBox(Texte);

}

void CChildFrame::UpdateKPPVParameters(int KValue, CDialSlider* ps)
{
	//Mettre à jour les paramètres d'apprentissage
	if (m_pClassifier && LastType == mKPPV)
	{
		Ptr<KNearest> pClassifier = m_pClassifier.dynamicCast<KNearest>();
		// Relancer l'apprentissage
		if (m_pClassifier && MesDatasMod.m_trainingData.empty() == false)
		{
			pClassifier->setDefaultK(KValue);
			pClassifier->train(MesDatasMod.m_trainingData, ROW_SAMPLE, m_Label);
			m_pClassifier = pClassifier;
			LastType = mKPPV;
			SMetriques m = Evaluation(&pClassifier, mKPPV, false);
			ps->UpdateData(true);
			ps->m_AfficheErreur.Format("Erreur =      %1.3f\nRappel =    %1.3f\nPrecision = %1.3f", m.ErreurGlobale, m.Rappel, m.Precision);
			ps->UpdateData(false);
			Invalidate();
			if (MesDatasMod.m_nbParametres == 3)
			{
				m_TitreFen3D = "Classifieur KPPV";
				Visu3d(m_pClassifier, LastType);
			}
		}
	}
	if (m_pClassifier && LastType != mKPPV)
	{
		m_Label.release(); // pour forcer l'apprentissage
		OnKppvKppvopencvapprentissagedynamique();
	}

}


void CChildFrame::UpdateNNParameters(CConfigNeurone* pnn)
{
	if (m_pClassifier && LastType == mMLP)
	{


		Ptr<ANN_MLP> pClassifier = m_pClassifier.dynamicCast<ANN_MLP>();

		//Ptr <ANN_MLP> pClassifier = ANN_MLP::create();

		// Relancer l'apprentissage
		if (pClassifier && MesDatasMod.m_trainingData.empty() == false)
		{

			Fenetre_Progression = new CDaliogProgression;
			Fenetre_Progression->Create(IDD_DIALOG_PROGRESS, NULL);
			Fenetre_Progression->Valeur_Maxi = m_pNNParamDlg->m_NbIter;
			Fenetre_Progression->m_ControlProgressBar.SetRange(0, m_pNNParamDlg->m_NbIter);
			Fenetre_Progression->SetWindowTextA("Apprentissage en cours");
			Fenetre_Progression->ShowWindow(SW_SHOW);

			Mat_<int> layerSizes(1, m_pNNParamDlg->m_HL + 2);
			layerSizes(0, 0) = MesDatasMod.m_nbParametres;
			if (m_pNNParamDlg->m_HL > 0) layerSizes(0, 1) = max(m_pNNParamDlg->m_c1, 2);
			if (m_pNNParamDlg->m_HL > 1) layerSizes(0, 2) = max(m_pNNParamDlg->m_c2, 2);
			if (m_pNNParamDlg->m_HL > 2) layerSizes(0, 3) = max(m_pNNParamDlg->m_c3, 2);
			//layerSizes(0, 1) = hlz;
			layerSizes(0, m_pNNParamDlg->m_HL + 1) = MesDatasMod.m_nbClasses;
			pClassifier->clear();
			pClassifier->setLayerSizes(layerSizes);
			pClassifier->setTrainMethod(ANN_MLP::BACKPROP, m_pNNParamDlg->m_eta);
			pClassifier->setActivationFunction(ANN_MLP::ActivationFunctions::SIGMOID_SYM);


			long displayInterval = m_pNNParamDlg->m_NbIter / 10;
			int currentIteration = 0;

			Ptr<TrainData> trainingData = TrainData::create(MesDatasMod.m_trainingData, SampleTypes::ROW_SAMPLE, OneHot);

			pClassifier->setTermCriteria(TermCriteria(TermCriteria::MAX_ITER + TermCriteria::EPS, 100, 1e-6)); // pour demarrer
			pClassifier->train(trainingData);

			while (currentIteration < m_pNNParamDlg->m_NbIter)
			{
				// Limiter le nombre d'itérations pour cet appel
				int iterationsLeft = std::min(displayInterval, m_pNNParamDlg->m_NbIter - currentIteration);
				pClassifier->setTermCriteria(TermCriteria(TermCriteria::MAX_ITER + TermCriteria::EPS, iterationsLeft, 1e-6));

				// Entraîner pour les itérations limitées
				//bool success = pClassifier->train(trainingData, cv::ml::ANN_MLP::UPDATE_MODEL);
				bool success = pClassifier->train(trainingData, cv::ml::ANN_MLP::TrainFlags::UPDATE_WEIGHTS);

				if (!success) {
					MessageBox("Erreur : échec de l'entraînement.");
					break;
				}
				currentIteration += iterationsLeft;
				float progress = (currentIteration * 100.0f) / m_pNNParamDlg->m_NbIter;
				Mat response;
				//Mat labels = trainingData->getResponses();
				pClassifier->predict(trainingData->getSamples(), response);

				//FileStorage fs("lab.txt", FileStorage::WRITE); // Ouvre le fichier en mode écriture
				//if (!fs.isOpened()) {
				//	return;
				//}
				//fs << "lab" << labels; // Sauvegarde la matrice
				//fs.release();

				//fs.open("response.txt", FileStorage::WRITE); // Ouvre le fichier en mode écriture
				//if (!fs.isOpened()) {
				//	return;
				//}
				//fs << "response" << response; // Sauvegarde la matrice
				//fs.release();
				double error = norm(OneHot, response, NORM_L2) / OneHot.rows;
				//cout << "Erreur intermédiaire : " << error << endl;
				m_pClassifier = pClassifier;
				LastType = mMLP;
				SMetriques m = Evaluation(&pClassifier, mMLP, false);
				Fenetre_Progression->m_ControlProgressBar.SetPos(currentIteration);
				Fenetre_Progression->m_Affiche.Format("Erreur sur apprentissage : %3.5f\nErreur =      %1.3f\nRappel =    %1.3f\nPrecision = %1.3f", error, m.ErreurGlobale, m.Rappel, m.Precision);
				//Fenetre_Progression->m_Affiche.Format("Erreur sur apprentissage : %3.5f", error);
				Fenetre_Progression->UpdateData(false);

				m_pClassifier = pClassifier;

				Invalidate();
				UpdateWindow();

				if (MesDatasMod.m_nbParametres == 3)
				{
					m_TitreFen3D = "Classifieur Reseau de neurone";
					Visu3d(m_pClassifier, LastType);
				}

			}
			m_pNNParamDlg->m_AfficheTexte = Fenetre_Progression->m_Affiche;
			m_pNNParamDlg->UpdateData(false);
			Fenetre_Progression->DestroyWindow();
			delete Fenetre_Progression;
			m_pClassifier = pClassifier;
			//Invalidate(); 
		}
		//if (MesDatas.NPAR == 3)
		//{
		//	Visu3d(m_pClassifier, LastType);
		//}
	}
	if (m_pClassifier && LastType != mMLP)
	{
		m_trainData.release(); // pour forcer l'apprentissage
		OnPerceptronApprentissagedynamique();
	}

}

void CChildFrame::OnKppvKppvopencvapprentissagedynamique()
{
	if (MesDatasMod.m_nbParametres != 2 && MesDatasMod.m_nbParametres != 3)
	{
		MessageBox("Fonctionne uniquement pour des données à 2 ou 3 attributs");
		return;
	}

	Cleannonmodal(true);
	m_Loaded = true;
	m_VisuDynamique = true;
	m_Done = true;

	if (!m_pKPPVparamDlg)
	{

		m_pKPPVparamDlg = new CDialSlider();
		m_pKPPVparamDlg->Create(this);
		m_pKPPVparamDlg->SetWindowText("Nombre de voisins K");
		m_pKPPVparamDlg->ShowWindow(SW_SHOW);
		m_pKPPVparamDlg->SetParentFrame(this); // Passer le pointeur du parent
	}
	else
	{
		m_pKPPVparamDlg->ShowWindow(SW_SHOW);
		m_pKPPVparamDlg->SetParentFrame(this); // Passer le pointeur du parent
	}
	if (m_Label.empty())
	{
		//m_trainData.release();
		//m_Label.release();
		//m_trainData = Mat(MesDatas.NbMesTotApp, MesDatas.NPAR, CV_32FC1);
		//m_Label = Mat(MesDatas.NbMesTotApp, 1, CV_32SC1);
		//int train_sample_count = MesDatas.NbMesTotApp;
		//int k, i, par;
		//unsigned int c;
		//k = 0;
		//for (c = 0; c < MesDatas.NbClass; c++)
		//{
		//	for (i = 0; i < MesDatas.NbMes[c]; i++)
		//	{
		//		for (par = 0; par < MesDatas.NPAR; par++)
		//		{
		//			m_trainData.at<float>(k, par) = MesDatas.TabApp[c][i * MesDatas.NPAR + par];
		//		}
		//		m_Label.at<int>(k, 0) = c;
		//		k++;
		//	}
		//}
		MesDatasMod.m_trainingLabels.convertTo(m_Label, CV_32SC1);

		Ptr <ml::KNearest > knn = ml::KNearest::create();
		knn->setDefaultK(7);
		m_Done = false;
		knn->train(MesDatasMod.m_trainingData, ml::ROW_SAMPLE, m_Label);
		m_Done = true;
		m_pClassifier = knn;
		LastType = mKPPV;

		SMetriques m = Evaluation(&m_pClassifier, mKPPV, false);
		m_pKPPVparamDlg->m_AfficheErreur.Format("Erreur =      %1.3f\nRappel =    %1.3f\nPrecision = %1.3f", m.ErreurGlobale, m.Rappel, m.Precision);
		m_pKPPVparamDlg->UpdateData(false);
		Invalidate();
		if (MesDatasMod.m_nbParametres == 3)
		{
			m_TitreFen3D = "Classifieur KPPV";
			Visu3d(m_pClassifier, LastType);
		}

	}
}


void CChildFrame::OnNaivebayesApprentissage()
{
	Cleannonmodal(true);
	if (MesDatasMod.m_nbParametres == 0) 		OnConfigurationLireauformatcsv();
	if (MesDatasMod.m_allData.empty()) return;
	CFileDialog Model_dia(FALSE, "cvNB", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		"Model Files (*.cvNB)|*.cvNB|" "All Files (*.*)|*.*||");
	Model_dia.m_ofn.lpstrTitle = _T("Enregistrer le modèle");
	CString Path_Model;
	cv::String pm;
	if (Model_dia.DoModal() == IDOK)
	{
		Path_Model = Model_dia.GetPathName();
		pm.assign(Path_Model);
	}
	else return;
	//Mat trainData = Mat(MesDatas.NbMesTotApp, MesDatas.NPAR, CV_32FC1);
	//Mat Label = Mat(MesDatas.NbMesTotApp, 1, CV_32SC1);
	//int train_sample_count = MesDatas.NbMesTotApp;
	//int  k, i, par;
	//unsigned int c;
	//k = 0;
	//for (c = 0; c < MesDatas.NbClass; c++)
	//{
	//	for (i = 0; i < MesDatas.NbMes[c]; i++)
	//	{
	//		for (par = 0; par < MesDatas.NPAR; par++)
	//		{
	//			trainData.at<float>(k, par) = MesDatas.TabApp[c][i * MesDatas.NPAR + par];
	//		}
	//		Label.at<int>(k, 0) = c;
	//		k++;
	//	}
	//}
	MesDatasMod.m_trainingLabels.convertTo(m_Label, CV_32SC1);


	Ptr<NormalBayesClassifier> naiveBayes = NormalBayesClassifier::create();
	naiveBayes->train(MesDatasMod.m_trainingData, ROW_SAMPLE, m_Label);
	naiveBayes->save(pm);
	CString Texte;
	Texte.Format("Fichier cvNB sauvegardé");
	MessageBox(Texte);
	m_pClassifier = naiveBayes;
	LastType = mNB;
	if (MesDatasMod.m_nbParametres == 2)
	{

		Invalidate();
	}
	m_Loaded = false;
}


void CChildFrame::OnNaivebayesVisufrontiere()
{
	//Cleannonmodal(false);
	//String pm;
	//CFileDialog FileData(TRUE, "*.cvNB", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Model (*.cvNB)|*.cvNB||", this);
	//if (FileData.DoModal() != IDOK) return;
	//ChaineModel = FileData.GetPathName();
	//pm.assign(ChaineModel);
	//Ptr<NormalBayesClassifier> naiveBayes = Algorithm::load<NormalBayesClassifier>(pm);
	//if (MesDatasMod.m_nbParametres == 2)
	//{
	//	m_pClassifier = naiveBayes;
	//	LastType = mNB;
	//	Invalidate();
	//}
	//else MessageBox("Nombre d'attributs différent de 2", MB_OK);


	Cleannonmodal();

	if (MesDatasMod.m_nbParametres > 3) { MessageBox("Nombre d'attributs superieur à 3", MB_OK); return; }

	if (!LoadClassifier(mNB)) return;
	int nbParametres = m_pClassifier.dynamicCast<NormalBayesClassifier>()->getVarCount();
	if (nbParametres < 2 || nbParametres > 3 || MesDatasMod.m_nbParametres != nbParametres)
	{
		MessageBox("Nombre d'attributs incompatible avec la visualisation");
		return;
	}
	m_Loaded = true;
	if (MesDatasMod.m_nbParametres == 3 && m_pClassifier != nullptr && LastType == mNB)
	{
		m_TitreFen3D = "Classifieur Random Forest";
		Visu3d(m_pClassifier, mNB);
	}
	if (MesDatasMod.m_nbParametres == 2 && m_pClassifier != nullptr && LastType == mNB)
	{
		Invalidate();
	}
}


void CChildFrame::OnNaivebayesEvaluation()
{
	Cleannonmodal(false);
	String pm;
	CFileDialog FileData(TRUE, "*.cvNB", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Model (*.cvNB)|*.cvNB||", this);
	if (FileData.DoModal() != IDOK) return;
	ChaineModel = FileData.GetPathName();
	m_PathData = ChaineModel.Left(ChaineModel.ReverseFind('.'));
	pm.assign(ChaineModel);
	Ptr<NormalBayesClassifier> naiveBayes = Algorithm::load<NormalBayesClassifier>(pm);
	if (naiveBayes->getVarCount() != MesDatasMod.m_nbParametres)
	{
		MessageBox("Nombre d'attributs du modele différent du nombre d'attributs des données");
		return;
	}
	m_pClassifier = naiveBayes;
	LastType = mNB;
	Evaluation(&naiveBayes, mNB);
}


void CChildFrame::OnEmApprentissage()
{
	Cleannonmodal(true);
	if (MesDatasMod.m_nbParametres == 0) 		OnConfigurationLireauformatcsv();
	if (MesDatasMod.m_allData.empty()) return;
	if (MesDatasMod.m_nbClasses != 1) MessageBox("Ceci est une méthode non supervisée basée sur les Kmeans\nles classes ne seront pas prises en compte pendant l'apprentissage");

	CDialVam Dial;
	int r = Dial.DoModal();
	int Cluster = Dial.m_K;
	if (Cluster < 2 || Cluster>20) Cluster = 2;


	CFileDialog Model_dia(FALSE, "cvEM", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		"Model Files (*.cvEM)|*.cvEM|" "All Files (*.*)|*.*||");
	Model_dia.m_ofn.lpstrTitle = _T("Enregistrer le modèle");
	CString Path_Model;
	cv::String pm;
	if (Model_dia.DoModal() == IDOK)
	{
		Path_Model = Model_dia.GetPathName();
		pm.assign(Path_Model);
	}
	else return;

	Ptr<EM> em = EM::create();
	em->setClustersNumber(Cluster);
	em->trainEM(MesDatasMod.m_trainingData);

	// Création de la matrice de confusion
	Mat confusionMatrix = Mat::zeros(MesDatasMod.m_nbClasses, Cluster, CV_32S);
	Mat probabilities;
	em->predict(MesDatasMod.m_trainingData, probabilities);
	for (int i = 0; i < probabilities.rows; i++)
	{
		Point maxLoc;
		minMaxLoc(probabilities.row(i), nullptr, nullptr, nullptr, &maxLoc);
		int trueLabel = (int)MesDatasMod.m_trainingLabels.at<float>(i, 0);
		confusionMatrix.at<int>(trueLabel, maxLoc.x)++;
	}

	// Trouver la correspondance optimale entre les clusters et les classes réelles
	clusterToClass.resize(Cluster);
	clusterToClass = AssignClustersToClasses(confusionMatrix);


	em->save(pm);
	CString Texte;
	Texte.Format("Fichier %s sauvegardé", Path_Model);
	MessageBox(Texte);
	m_pClassifier = em;
	LastType = mEM;
	if (MesDatasMod.m_nbParametres == 2)
	{
		Invalidate();
	}
	m_Loaded = false;
}


void CChildFrame::OnEmVisufrontiere()
{
	Cleannonmodal(false);
	if (MesDatasMod.m_nbParametres > 3) { MessageBox("Nombre d'attributs superieur à 3", MB_OK); return; }
	String pm;
	CFileDialog FileData(TRUE, "*.cvEM", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Model (*.cvEM)|*.cvEM||", this);
	if (FileData.DoModal() != IDOK) return;
	ChaineModel = FileData.GetPathName();
	pm.assign(ChaineModel);

	Ptr<EM> em = Algorithm::load<EM>(pm);
	if (MesDatasMod.m_nbParametres == 2)
	{
		m_pClassifier = em;
		LastType = mEM;
		int Cluster = em->getClustersNumber();
		// Création de la matrice de confusion
		Mat confusionMatrix = Mat::zeros(MesDatasMod.m_nbClasses, Cluster, CV_32S);
		Mat probabilities;
		em->predict(MesDatasMod.m_trainingData, probabilities);
		for (int i = 0; i < probabilities.rows; i++)
		{
			Point maxLoc;
			minMaxLoc(probabilities.row(i), nullptr, nullptr, nullptr, &maxLoc);
			int trueLabel = (int)MesDatasMod.m_trainingLabels.at<float>(i, 0);
			confusionMatrix.at<int>(trueLabel, maxLoc.x)++;
		}

		// Trouver la correspondance optimale entre les clusters et les classes réelles
		clusterToClass.resize(MesDatasMod.m_nbClasses);
		clusterToClass = AssignClustersToClasses(confusionMatrix);
		Invalidate();
	}
	else
	{
		m_pClassifier = em;
		LastType = mEM;
		if (MesDatasMod.m_nbParametres == 3 && m_pClassifier != nullptr)
		{
			m_TitreFen3D = "Classifieur EM non supervisé";
			Visu3d(m_pClassifier, mEM);
		} 
		
	}
		
		
}

void CChildFrame::OnEmEnregistrerlesdonneesclasseesdansuncsv()
{
	Cleannonmodal(false);
	String pm;
	CFileDialog FileData(TRUE, "*.cvEM", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Model (*.cvEM)|*.cvEM||", this);
	if (FileData.DoModal() != IDOK) return;
	ChaineModel = FileData.GetPathName();
	pm.assign(ChaineModel);
	Ptr<EM> em = Algorithm::load<EM>(pm);
	m_pClassifier = em;
	LastType = mEM;


	// Création de la matrice de confusion
	Mat confusionMatrix = Mat::zeros(MesDatasMod.m_nbClasses, MesDatasMod.m_nbClasses, CV_32S);
	Mat probabilities;
	em->predict(MesDatasMod.m_trainingData, probabilities);
	for (int i = 0; i < probabilities.rows; i++)
	{
		Point maxLoc;
		minMaxLoc(probabilities.row(i), nullptr, nullptr, nullptr, &maxLoc);
		int trueLabel = (int)MesDatasMod.m_trainingLabels.at<float>(i, 0);
		confusionMatrix.at<int>(trueLabel, maxLoc.x)++;
	}

	// Trouver la correspondance optimale entre les clusters et les classes réelles
	//clusterToClass.resize(MesDatasMod.m_nbClasses);
	//clusterToClass = AssignClustersToClasses(confusionMatrix);

	clusterToClass.clear();// pour que les classes soient celles d'origine du EM

	int par;
	// TODO: ajoutez ici le code de votre gestionnaire de commande
	CFileDialog Model_dia(FALSE, "csv", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		"Model Files (*.csv)|*.csv|" "All Files (*.*)|*.*||");
	Model_dia.m_ofn.lpstrTitle = _T("Enregistrer les données");

	CString Path_Data;
	if (Model_dia.DoModal() == IDOK)
	{
		Path_Data = Model_dia.GetPathName();
		pm.assign(Path_Data);
	}
	std::ofstream file(pm);
	if (!file.is_open()) {
		std::cerr << "Impossible d'ouvrir le fichier pour sauvegarder les données: " << pm << std::endl;
		return;
	}
	for (int i = 0; i < MesDatasMod.m_allData.rows; i++)
	{
		Mat sampleMat = Mat(1, MesDatasMod.m_nbParametres, CV_32FC1);
		for (par = 0; par < MesDatasMod.m_nbParametres; par++)
		{
			sampleMat.at<float>(0, par) = MesDatasMod.m_allData.at<float>(i, par);
		}//for par
		int v = (int)Decide(em, LastType, sampleMat);
		file << v << ",";
		for (int j = 0; j < MesDatasMod.m_nbParametres; j++)
		{
			file << MesDatasMod.m_allData.at<float>(i, j);
			if (j < MesDatasMod.m_nbParametres - 1)
			{
				file << ",";
			}
		}
		file << std::endl;
	}

}

//std::vector<int> CChildFrame::AssignClustersToClasses(const Mat& costMatrix)
//{
//	int numClasses = costMatrix.rows;
//	int numClusters = costMatrix.cols;
//
//	// Vérification : la matrice doit être carrée pour l'algorithme hongrois
//	int n = max(numClasses, numClusters);
//	Mat_<int> cost = Mat::zeros(n, n, CV_32S);
//
//	// Copier les valeurs dans la matrice carrée
//	for (int i = 0; i < numClasses; i++) {
//		for (int j = 0; j < numClusters; j++) {
//			cost(i, j) = costMatrix.at<int>(i, j);
//		}
//	}
//
//	// Étape 1: Soustraction du minimum de chaque ligne
//	for (int i = 0; i < n; i++) {
//		int minValue = INT_MAX;
//		for (int j = 0; j < n; j++) {
//			minValue = min(minValue, cost(i, j));
//		}
//		for (int j = 0; j < n; j++) {
//			cost(i, j) -= minValue;
//		}
//	}
//
//	// Étape 2: Soustraction du minimum de chaque colonne
//	for (int j = 0; j < n; j++) {
//		int minValue = INT_MAX;
//		for (int i = 0; i < n; i++) {
//			minValue = min(minValue, cost(i, j));
//		}
//		for (int i = 0; i < n; i++) {
//			cost(i, j) -= minValue;
//		}
//	}
//
//	std::vector<int> rowAssignment(n, -1);
//	std::vector<int> colAssignment(n, -1);
//	std::vector<bool> rowCovered(n, false);
//	std::vector<bool> colCovered(n, false);
//
//	// Assignation initiale
//	int numAssigned = 0;
//	while (numAssigned < n) {
//		// Rechercher les zéros non couverts
//		for (int i = 0; i < n; i++) {
//			if (!rowCovered[i]) {
//				for (int j = 0; j < n; j++) {
//					if (!colCovered[j] && cost(i, j) == 0) {
//						rowAssignment[i] = j;
//						colAssignment[j] = i;
//						rowCovered[i] = true;
//						colCovered[j] = true;
//						numAssigned++;
//						break;
//					}
//				}
//			}
//		}
//
//		// Si toutes les lignes sont couvertes, sortir de la boucle
//		if (numAssigned == n) break;
//
//		// Sinon, ajuster la matrice de coût
//		int minUncovered = INT_MAX;
//		for (int i = 0; i < n; i++) {
//			if (!rowCovered[i]) {
//				for (int j = 0; j < n; j++) {
//					if (!colCovered[j]) {
//						minUncovered = min(minUncovered, cost(i, j));
//					}
//				}
//			}
//		}
//
//		// Ajuster la matrice de coût
//		for (int i = 0; i < n; i++) {
//			for (int j = 0; j < n; j++) {
//				if (rowCovered[i]) {
//					if (colCovered[j]) {
//						cost(i, j) += minUncovered;
//					}
//				}
//				else {
//					if (!colCovered[j]) {
//						cost(i, j) -= minUncovered;
//					}
//				}
//			}
//		}
//
//		// Réinitialiser les couvertures pour la prochaine itération
//		std::fill(rowCovered.begin(), rowCovered.end(), false);
//		std::fill(colCovered.begin(), colCovered.end(), false);
//	}
//
//	// Extraire les assignations finales
//	std::vector<int> assignment(numClusters, -1);
//	for (int i = 0; i < numClasses && i < numClusters; i++) {
//		if (rowAssignment[i] < numClusters) {
//			assignment[i] = rowAssignment[i];
//		}
//	}
//
//	return assignment;
//}

std::vector<int> CChildFrame::AssignClustersToClasses(const Mat& confusionMatrix)
{
	int numClasses = confusionMatrix.rows;
	int numClusters = confusionMatrix.cols;
	std::vector<int> assignment(numClusters, -1);
	std::vector<bool> classUsed(numClasses, false);

	// Pour chaque cluster, trouver la classe avec le maximum de correspondances
	for (int cluster = 0; cluster < numClusters; cluster++) {
		int bestClass = -1;
		int maxCount = -1;

		// Trouver la meilleure classe non utilisée pour ce cluster
		for (int cls = 0; cls < numClasses; cls++) {
			if (!classUsed[cls] && confusionMatrix.at<int>(cls, cluster) > maxCount) {
				maxCount = confusionMatrix.at<int>(cls, cluster);
				bestClass = cls;
			}
		}

		if (bestClass != -1) {
			assignment[cluster] = bestClass;
			classUsed[bestClass] = true;
		}
	}

	// Assigner les clusters restants à des classes non utilisées
	for (int cluster = 0; cluster < numClusters; cluster++) {
		if (assignment[cluster] == -1) {
			for (int cls = 0; cls < numClasses; cls++) {
				if (!classUsed[cls]) {
					assignment[cluster] = cls;
					classUsed[cls] = true;
					break;
				}
			}
		}
	}

	return assignment;
}


void CChildFrame::OnEmEvaluation()
{
	Cleannonmodal(false);
	if (MesDatasMod.m_nbClasses == 1)
	{
		MessageBox("L'évaluation ne fonctionne qu'avec un CSV contenant les labels d'au moins 2 classes");
		return;
	}
	String pm;
	CFileDialog FileData(TRUE, "*.cvEM", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Model (*.cvEM)|*.cvEM||", this);
	if (FileData.DoModal() != IDOK) return;
	ChaineModel = FileData.GetPathName();
	m_PathData = ChaineModel.Left(ChaineModel.ReverseFind('.'));
	pm.assign(ChaineModel);
	Ptr<EM> em = Algorithm::load<EM>(pm);
	if (em->getVarCount() != MesDatasMod.m_nbParametres)
	{
		MessageBox("Nombre d'attributs du modele différent du nombre d'attributs des données");
		return;
	}
	m_pClassifier = em;
	LastType = mEM;
	int Cluster = em->getClustersNumber();
	if (Cluster != MesDatasMod.m_nbClasses)
	{
		MessageBox("Le nombre de clusters est différent du nombre de classe des données, évaluation impossible");
		return;
	}
	// Création de la matrice de confusion
	Mat confusionMatrix = Mat::zeros(MesDatasMod.m_nbClasses, Cluster, CV_32S);
	Mat probabilities;
	em->predict(MesDatasMod.m_trainingData, probabilities);
	for (int i = 0; i < probabilities.rows; i++)
	{
		Point maxLoc;
		minMaxLoc(probabilities.row(i), nullptr, nullptr, nullptr, &maxLoc);
		int trueLabel = (int)MesDatasMod.m_trainingLabels.at<float>(i, 0);
		confusionMatrix.at<int>(trueLabel, maxLoc.x)++;
	}

	// Trouver la correspondance optimale entre les clusters et les classes réelles
	clusterToClass.resize(MesDatasMod.m_nbClasses);
	clusterToClass = AssignClustersToClasses(confusionMatrix);



	Evaluation(&em, mEM);
}


void CChildFrame::OnDtreesApprentissage()
{
	Cleannonmodal(true);
	if (MesDatasMod.m_nbParametres == 0 || MesDatasMod.m_nbClasses == 1) 		OnConfigurationLireauformatcsv();
	if (MesDatasMod.m_allData.empty()) return;

	CFileDialog Model_dia(FALSE, "cvDT", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		"Model Files (*.cvDT)|*.cvDT|" "All Files (*.*)|*.*||");
	Model_dia.m_ofn.lpstrTitle = _T("Enregistrer le modèle");
	CString Path_Model;
	cv::String pm;
	if (Model_dia.DoModal() == IDOK)
	{
		Path_Model = Model_dia.GetPathName();
		pm.assign(Path_Model);
	}
	else return;

	MesDatasMod.m_trainingLabels.convertTo(m_Label, CV_32SC1);
	Ptr<MultiClassBoost> dTree = MultiClassBoost::create();

	dTree->InitParam(cv::ml::Boost::DISCRETE, 50, 5);
	dTree->train(MesDatasMod.m_trainingData, m_Label, MesDatasMod.m_nbClasses);

	dTree->save(pm);
	CString Texte;
	Texte.Format("Fichier cvDT sauvegardé");
	MessageBox(Texte);
	m_pClassifier = dTree;
	LastType = mDT;
	if (MesDatasMod.m_nbParametres == 2)
	{

		Invalidate();
	}
	m_Loaded = false;
}


void CChildFrame::OnDtreesVisufrontiere()
{
	//Cleannonmodal(false);
	//String pm;
	//CFileDialog FileData(TRUE, "*.cvDT", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Model (*.cvDT)|*.cvDT||", this);
	//if (FileData.DoModal() != IDOK) return;
	//ChaineModel = FileData.GetPathName();
	//pm.assign(ChaineModel);
	//Ptr<MultiClassBoost> dTree = MultiClassBoost::create();
	//dTree->load(pm);
	////Ptr<MultiClassBoost> dTree = Algorithm::load<MultiClassBoost>(pm);
	//if (MesDatasMod.m_nbParametres == 2)
	//{
	//	m_pClassifier = dTree;
	//	LastType = mDT;
	//	Invalidate();
	//}
	//else MessageBox("Nombre d'attributs différent de 2", MB_OK);


	Cleannonmodal();

	if (MesDatasMod.m_nbParametres > 3) { MessageBox("Nombre d'attributs superieur à 3", MB_OK); return; }

	CFileDialog FileData(TRUE, "*.cvDT", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Model (*.cvDT)|*.cvDT||", this);
	if (FileData.DoModal() != IDOK) return;
	ChaineModel = FileData.GetPathName();
	String pm;
	pm.assign(ChaineModel);
	Ptr<MultiClassBoost> dTree = MultiClassBoost::create();
	dTree->load(pm);

	int nbParametres = dTree->m_nbfeatures;
	if (nbParametres < 2 || nbParametres > 3 || MesDatasMod.m_nbParametres != nbParametres)
	{
		MessageBox("Nombre d'attributs incompatible avec la visualisation");
		return;
	}
	m_Loaded = true;
	m_pClassifier = dTree;
	LastType = mDT;
	if (MesDatasMod.m_nbParametres == 3 && m_pClassifier != nullptr && LastType == mDT)
	{
		m_TitreFen3D = "Classifieur Boosting";
		Visu3d(m_pClassifier, mDT);
	}
	if (MesDatasMod.m_nbParametres == 2 && m_pClassifier != nullptr && LastType == mDT)
	{
		Invalidate();
	}
}


void CChildFrame::OnDtreesEvaluation()
{
	Cleannonmodal(false);
	String pm;
	CFileDialog FileData(TRUE, "*.cvDT", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Model (*.cvDT)|*.cvDT||", this);
	if (FileData.DoModal() != IDOK) return;
	ChaineModel = FileData.GetPathName();
	m_PathData = ChaineModel.Left(ChaineModel.ReverseFind('.'));
	pm.assign(ChaineModel);
	//Ptr<MultiClassBoost> dTree = Algorithm::load<MultiClassBoost>(pm);

	Ptr<MultiClassBoost> dTree = MultiClassBoost::create();
	dTree->load(pm);
	//if (dTree->getVarCount() != MesDatas.NPAR)
	//{
	//	MessageBox("Nombre d'attributs du modele différent du nombre d'attributs des données");
	//	return;
	//}
	m_pClassifier = dTree;
	LastType = mDT;
	Evaluation(&dTree, mDT);
}


void CChildFrame::OnNaivebayesDecisionsurfichierdedonnees()
{
	Cleannonmodal(false);
	CDonneeFloatMod datas;
	CString filePath;
	// Chargement du fichier de donnee
	filePath = LoadData(datas);
	if (filePath == "") return;

	// Générer le fichier de résultats
	CString resultFilePath = filePath + "_Result_classif_NaiveBayes.txt";
	std::ofstream fileRes(resultFilePath.GetString());
	if (!fileRes.is_open()) {
		MessageBox("Impossible de créer le fichier de résultats.", MB_OK);
		return;
	}

	// Ouvrir le fichier du modèle 
	CFileDialog fileModelDialog(TRUE, "*.cvNB", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Modèles cvNB (*.cvNB)|*.cvNB||", this);
	if (fileModelDialog.DoModal() != IDOK) {
		return;
	}
	String pm;
	ChaineModel = fileModelDialog.GetPathName();
	pm.assign(ChaineModel);
	Ptr<NormalBayesClassifier> nb = Algorithm::load<NormalBayesClassifier>(pm);
	if (nb.empty()) {
		MessageBox("Impossible de charger le modèle Naive Bayes.", MB_OK);
		return;
	}

	// Effectuer la classification
	DecidDataOpencv(&nb, datas, fileRes, mNB);

	fileRes.close();
	MessageBox("Le fichier de résultats " + resultFilePath + " a été sauvegardé.", MB_OK);
}


void CChildFrame::OnBoostingopencvDecisionsurfichierdedonnees()
{
	CDonneeFloatMod datas;
	CString filePath;
	// Chargement du fichier de donnee
	filePath = LoadData(datas);
	if (filePath == "") return;

	// Générer le fichier de résultats
	CString resultFilePath = filePath + "_Result_classif_BoostingOpencv.txt";
	std::ofstream fileRes(resultFilePath.GetString());
	if (!fileRes.is_open()) {
		MessageBox("Impossible de créer le fichier de résultats.", MB_OK);
		return;
	}

	// Ouvrir le fichier du modèle 
	CFileDialog fileModelDialog(TRUE, "*.cvDT", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Modèles cvDT (*.cvDT)|*.cvDT||", this);
	if (fileModelDialog.DoModal() != IDOK) {
		return;
	}
	String pm;
	ChaineModel = fileModelDialog.GetPathName();
	pm.assign(ChaineModel);
	Ptr<MultiClassBoost> dTree = MultiClassBoost::create();
	dTree->load(pm);
	if (dTree.empty()) {
		MessageBox("Impossible de charger le modèle Boosting.", MB_OK);
		return;
	}

	// Effectuer la classification
	DecidDataOpencv(&dTree, datas, fileRes, mDT);

	fileRes.close();
	MessageBox("Le fichier de résultats " + resultFilePath + " a été sauvegardé.", MB_OK);
}


void CChildFrame::OnKppvKppvopencvregression()
{
	// TODO: ajoutez ici le code de votre gestionnaire de commande
}


void CChildFrame::OnRandomforestRandomforestregression()
{

	Ptr<RTrees> rtrees = RTrees::create();

	// Configurer les paramètres pour la régression
	rtrees->setMaxDepth(50);
	rtrees->setMinSampleCount(5);
	rtrees->setRegressionAccuracy(0.0001);
	rtrees->setTermCriteria(TermCriteria(TermCriteria::MAX_ITER, 1000, 0.0001));
	rtrees->train(MesDatasMod.m_trainingData, cv::ml::ROW_SAMPLE, MesDatasMod.m_trainingLabels);

	CFileDialog Model_dia(FALSE, "cvRT", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		"Model Files (*.cvRT)|*.cvRT|" "All Files (*.*)|*.*||");
	Model_dia.m_ofn.lpstrTitle = _T("Enregistrer le modèle");

	CString Path_Model;
	cv::String pm;
	if (Model_dia.DoModal() == IDOK)
	{
		Path_Model = Model_dia.GetPathName();
		pm.assign(Path_Model);
	}
	else return;

	rtrees->save(pm);

	// Évaluer les données de test
	CString resultFilePath = Path_Model + "_Result_regression_RF.csv";
	std::ofstream outFile(resultFilePath.GetString());
	outFile << "Valeur attendue,Valeur prédite\n";

	Mat predictedLabels = Mat(MesDatasMod.m_nbMesTotalTest, 1, CV_32FC1);
	for (int i = 0; i < MesDatasMod.m_nbMesTotalTest; i++)
	{
		float prediction = rtrees->predict(MesDatasMod.m_testData.row(i));
		predictedLabels.at<float>(i, 0) = prediction;
		outFile << MesDatasMod.m_testLabels.at<float>(i, 0) << "," << prediction << "\n"; // Sauvegarder les résultats
	}
	outFile.close();

	// Calculer la MSE
	Mat diff = MesDatasMod.m_testLabels - predictedLabels;  // Différence entre vrai et prédit
	Mat diffabs = abs(MesDatasMod.m_testLabels - predictedLabels);  // Valeur absolue des différences
	double mae = sum(diffabs)[0] / MesDatasMod.m_nbMesTotalTest;
	diff = diff.mul(diff);                  // Carré de la différence
	double mse = sum(diff)[0] / MesDatasMod.m_nbMesTotalTest; // Moyenne

	CString Texte;
	Texte.Format("Modèle sauvegardé dans %s\nRésultats sauvegardés dans fichier _Result_regression_RF.csv\n avec MSE=%3.4f \n MAE=%3.4f", Path_Model, mse, mae);
	MessageBox(Texte);



}


void CChildFrame::OnPerceptronApprentissagedynamique()
{
	if (MesDatasMod.m_nbClasses == 1) 		OnConfigurationLireauformatcsv();

	if (MesDatasMod.m_nbParametres != 2 && MesDatasMod.m_nbParametres != 3)
	{
		MessageBox("Fonctionne uniquement pour des données à 2 ou 3 attributs");
		return;
	}


	Cleannonmodal(true);
	m_Loaded = true;
	m_VisuDynamique = true;
	if (!m_pNNParamDlg)
	{

		m_pNNParamDlg = new  CConfigNeurone();
		m_pNNParamDlg->m_NbIter = 50;
		m_pNNParamDlg->m_c1 = 2;
		m_pNNParamDlg->m_HL = 1;
		m_pNNParamDlg->Seuil = 0.5;
		m_pNNParamDlg->NbClasses = MesDatasMod.m_nbClasses;
		m_pNNParamDlg->Npar = MesDatasMod.m_nbParametres;
		m_pNNParamDlg->m_eta = 0.01;
		m_pNNParamDlg->Create(this);
		m_pNNParamDlg->m_BoutonMAJ.EnableWindow(true);
		m_pNNParamDlg->ShowWindow(SW_SHOW);
		m_pNNParamDlg->SetParentFrame(this); // Passer le pointeur du parent
	}
	else
	{
		m_pNNParamDlg->ShowWindow(SW_SHOW);
		m_pNNParamDlg->SetParentFrame(this); // Passer le pointeur du parent
	}
	if (m_Label.empty())
	{
		//m_trainData.release();
		m_Label.release();
		//m_trainData = Mat(MesDatas.NbMesTotApp, MesDatas.NPAR, CV_32FC1);
		//m_Label = Mat(MesDatas.NbMesTotApp, 1, CV_32SC1);
		//int train_sample_count = MesDatas.NbMesTotApp;
		//int k, i, par;
		//unsigned int c;
		//k = 0;
		//for (c = 0; c < MesDatas.NbClass; c++)
		//{
		//	for (i = 0; i < MesDatas.NbMes[c]; i++)
		//	{
		//		for (par = 0; par < MesDatas.NPAR; par++)
		//		{
		//			m_trainData.at<float>(k, par) = MesDatas.TabApp[c][i * MesDatas.NPAR + par];
		//		}
		//		m_Label.at<int>(k, 0) = c;
		//		k++;
		//	}
		//}

		MesDatasMod.m_trainingLabels.convertTo(m_Label, CV_32SC1);
		OneHot = cv::Mat::zeros(m_Label.rows, MesDatasMod.m_nbClasses, CV_32F);
		const int* labelsPtr = m_Label.ptr<int>(0);
		for (int i = 0; i < m_Label.rows; i++)
		{
			float* outputPtr = OneHot.ptr<float>(i);
			int id = labelsPtr[i];
			outputPtr[id] = 1.f;
		}
		const int hlz = 2;

		Ptr <ANN_MLP> mlp = ANN_MLP::create();
		mlp->setTermCriteria(TermCriteria(TermCriteria::MAX_ITER + TermCriteria::EPS, 1, 1e-4));
		Mat_<int> layerSizes(1, m_pNNParamDlg->m_HL + 2);
		layerSizes(0, 0) = MesDatasMod.m_nbParametres;
		if (m_pNNParamDlg->m_HL > 0) layerSizes(0, 1) = max(m_pNNParamDlg->m_c1, 2);
		if (m_pNNParamDlg->m_HL > 1) layerSizes(0, 2) = max(m_pNNParamDlg->m_c2, 2);
		if (m_pNNParamDlg->m_HL > 2) layerSizes(0, 3) = max(m_pNNParamDlg->m_c3, 2);
		//layerSizes(0, 1) = hlz;
		layerSizes(0, m_pNNParamDlg->m_HL + 1) = MesDatasMod.m_nbClasses;
		mlp->setLayerSizes(layerSizes);
		mlp->setTrainMethod(ANN_MLP::BACKPROP, m_pNNParamDlg->m_eta);
		mlp->setActivationFunction(ANN_MLP::ActivationFunctions::SIGMOID_SYM);

		Ptr<TrainData> trainingData = TrainData::create(MesDatasMod.m_trainingData, SampleTypes::ROW_SAMPLE, OneHot);
		mlp->setTermCriteria(TermCriteria(TermCriteria::MAX_ITER + TermCriteria::EPS, m_pNNParamDlg->m_NbIter, 1e-4));
		Fenetre_Progression = new CDaliogProgression;
		Fenetre_Progression->Create(IDD_DIALOG_PROGRESS, NULL);
		Fenetre_Progression->Valeur_Maxi = m_pNNParamDlg->m_NbIter;
		Fenetre_Progression->m_ControlProgressBar.SetRange(0, m_pNNParamDlg->m_NbIter);
		Fenetre_Progression->SetWindowTextA("Premier Apprentissage en cours");
		Fenetre_Progression->ShowWindow(SW_SHOW);

		bool success = mlp->train(trainingData);
		Fenetre_Progression->DestroyWindow();
		delete Fenetre_Progression;


		m_pClassifier = mlp;
		LastType = mMLP;
		Invalidate();
		if (MesDatasMod.m_nbParametres == 3)
		{
			m_TitreFen3D = "Classifieur Reseau de Neurone";
			Visu3d(m_pClassifier, LastType);
		}
	}


}


void CChildFrame::OnPerceptronMlpenregression()
{
	if (m_pNNParamDlg) {
		m_pNNParamDlg->DestroyWindow();
		delete m_pNNParamDlg;
		m_pNNParamDlg = nullptr;
	}

	CConfigNeurone ConfigN;

	ConfigN.m_NbIter = 100;
	ConfigN.m_c1 = 2;
	ConfigN.m_HL = 1;
	ConfigN.Seuil = 0.5;
	ConfigN.NbClasses = MesDatasMod.m_nbClasses;
	ConfigN.Npar = MesDatasMod.m_nbParametres;
	if (ConfigN.DoModal() != IDOK)
	{
		return;
	}

	//cv::FileStorage file("mamat.txt", cv::FileStorage::WRITE);
	//file << "onehot"<<OneHot;
	//file.release();

	Fenetre_Progression = new CDaliogProgression;
	Fenetre_Progression->Create(IDD_DIALOG_PROGRESS, NULL);
	Fenetre_Progression->Valeur_Maxi = ConfigN.m_NbIter;
	Fenetre_Progression->m_ControlProgressBar.SetRange(0, ConfigN.m_NbIter);
	Fenetre_Progression->SetWindowTextA("Apprentissage en cours");
	Fenetre_Progression->ShowWindow(SW_SHOW);

	const int hlz = 2;

	Ptr <ANN_MLP> mlp = ANN_MLP::create();
	mlp->setTermCriteria(TermCriteria(TermCriteria::MAX_ITER + TermCriteria::EPS, 1, 1e-6));
	Mat_<int> layerSizes(1, ConfigN.m_HL + 2);
	layerSizes(0, 0) = MesDatasMod.m_nbParametres;
	if (ConfigN.m_HL > 0) layerSizes(0, 1) = max(ConfigN.m_c1, 2);
	if (ConfigN.m_HL > 1) layerSizes(0, 2) = max(ConfigN.m_c2, 2);
	if (ConfigN.m_HL > 2) layerSizes(0, 3) = max(ConfigN.m_c3, 2);
	//layerSizes(0, 1) = hlz;
	layerSizes(0, ConfigN.m_HL + 1) = 1; // une seule sortie en regression  
	mlp->setLayerSizes(layerSizes);

	mlp->setTrainMethod(ANN_MLP::BACKPROP, ConfigN.m_eta);
	if (ConfigN.m_activation == 1) mlp->setActivationFunction(ANN_MLP::ActivationFunctions::SIGMOID_SYM);
	if (ConfigN.m_activation == 0)  mlp->setActivationFunction(ANN_MLP::ActivationFunctions::IDENTITY);
	if (ConfigN.m_activation == 2) mlp->setActivationFunction(ANN_MLP::ActivationFunctions::RELU);

	Ptr<TrainData> trainingData = TrainData::create(MesDatasMod.m_trainingData, SampleTypes::ROW_SAMPLE, MesDatasMod.m_trainingLabels);

	mlp->setTermCriteria(TermCriteria(TermCriteria::MAX_ITER + TermCriteria::EPS, 10, 1e-6)); // pour demarrer
	bool success = mlp->train(trainingData);

	long displayInterval = ConfigN.m_NbIter / 10;
	int currentIteration = 0;
	while (currentIteration < ConfigN.m_NbIter) {
		// Limiter le nombre d'itérations pour cet appel
		int iterationsLeft = std::min(displayInterval, ConfigN.m_NbIter - currentIteration);
		mlp->setTermCriteria(TermCriteria(TermCriteria::MAX_ITER + TermCriteria::EPS, iterationsLeft, 1e-6));

		// Entraîner pour les itérations limitées
		//bool success = mlp->train(trainingData);
		bool success = mlp->train(trainingData, cv::ml::ANN_MLP::TrainFlags::UPDATE_WEIGHTS);

		if (!success) {
			//cerr << "Erreur : échec de l'entraînement." << endl;
			break;
		}

		// Afficher la progression
		currentIteration += iterationsLeft;
		float progress = (currentIteration * 100.0f) / ConfigN.m_NbIter;
		//cout << "Progression : " << progress << "%" << endl;

		// Optionnel : Vérifier les performances intermédiaires (perte, précision, etc.)
		// Par exemple, calculer l'erreur sur les données d'entraînement
		Mat response;
		Mat labels = trainingData->getResponses();
		mlp->predict(trainingData->getSamples(), response);

		//FileStorage fs("label.txt", FileStorage::WRITE); // Ouvre le fichier en mode écriture
		//if (!fs.isOpened()) {
		//	return;
		//}
		//fs << "label" << labels; // Sauvegarde la matrice
		//fs.release();

		//FileStorage fsb("reponse.txt", FileStorage::WRITE); // Ouvre le fichier en mode écriture
		//if (!fsb.isOpened()) {
		//	return;
		//}
		//fsb << "response" << response; // Sauvegarde la matrice
		//fsb.release();

		double mse = norm(labels, response, NORM_L2) / labels.rows;
		// Calcul de la MAE
		Mat diff = abs(labels - response); // Différences absolues entre labels et réponses
		double mae = sum(diff)[0] / labels.rows;
		//cout << "Erreur intermédiaire : " << error << endl;
		Fenetre_Progression->m_ControlProgressBar.SetPos(currentIteration);
		Fenetre_Progression->m_Affiche.Format("MSE : %1.6f et MAE : %1.6f", mse, mae);
		Fenetre_Progression->UpdateData(false);
	}

	//cout << "Entraînement terminé après " << currentIteration << " itérations." << endl;
	CFileDialog Model_dia(FALSE, "cvMlp", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Model Files (*.cvMlp)|*.cvMlp|" "All Files (*.*)|*.*||");
	CString Path_Model;
	Model_dia.m_ofn.lpstrTitle = _T("Enregistrer le modèle");
	cv::String pm;
	if (Model_dia.DoModal() == IDOK)
	{
		Path_Model = Model_dia.GetPathName();
		pm.assign(Path_Model);
	}
	else return;
	mlp->save(pm);


	CString Texte;
	Fenetre_Progression->DestroyWindow();
	delete Fenetre_Progression;


	Mat Resp;
	mlp->predict(MesDatasMod.m_testData, Resp);
	CString resultFilePath = Path_Model + "_Result_regression_NN.csv";
	std::ofstream outFile(resultFilePath.GetString());
	outFile << "Valeur attendue,Valeur prédite\n";
	for (int i = 0; i < MesDatasMod.m_nbMesTotalTest; i++)
	{
		outFile << MesDatasMod.m_testLabels.at<float>(i, 0) << "," << Resp.at<float>(i, 0) << "\n"; // Sauvegarder les résultats
	}
	outFile.close();

	//	FileStorage fsb("datatest.txt", FileStorage::WRITE); // Ouvre le fichier en mode écriture
	//if (!fsb.isOpened()) {
	//	return;
	//}
	//fsb << "datatest" << testData; // Sauvegarde la matrice
	//fsb.release();

	Mat diff = MesDatasMod.m_testLabels - Resp;  // Différence entre vrai et prédit
	Mat diffabs = abs(MesDatasMod.m_testLabels - Resp);  // Valeur absolue des différences
	double mae = sum(diffabs)[0] / MesDatasMod.m_testLabels.rows;
	diff = diff.mul(diff);                  // Carré de la différence
	double mse = sum(diff)[0] / MesDatasMod.m_testLabels.rows; // Moyenne

	Texte.Format("Résultats sauvegardés dans _Result_regression_NN.csv\n avec MSE=%3.6f \n MAE=%3.6f", mse, mae);
	MessageBox(Texte);

}



//void GenerateGaussianPoints(vtkSmartPointer<vtkPoints> points,	vtkSmartPointer<vtkFloatArray> colors,
//	double cx, double cy, double cz,
//	double sigma, int numPoints, int colorId)
//{
//	std::default_random_engine generator;
//	std::normal_distribution<double> distributionX(cx, sigma);
//	std::normal_distribution<double> distributionY(cy, sigma);
//	std::normal_distribution<double> distributionZ(cz, sigma);
//
//	for (int i = 0; i < numPoints; ++i)
//	{
//		double x = distributionX(generator);
//		double y = distributionY(generator);
//		double z = distributionZ(generator);
//		points->InsertNextPoint(x, y, z);
//		colors->InsertNextValue(colorId);
//	}
//}



//void CChildFrame::Visu3d(void* ptr, TypeClassif TypeClassifieur)
//{
//	vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
//	vtkSmartPointer<vtkFloatArray> colors = vtkSmartPointer<vtkFloatArray>::New();
//	colors->SetNumberOfComponents(1);
//	colors->SetName("Colors");
//
//	// Paramètres des distributions
//	//int numPointsPerClass = 1000;
//	double cube_size = 800.0;
//	int grid_size = 70;
//	float tr = 0.5; // tranparence
//
//	for (int cl = 0; cl < MesDatas.NbClass; cl++)
//	{
//		for (int k = 0; k < MesDatas.NbMes[cl]; k++)
//		{
//			int p = k * MesDatas.NPAR;
//
//			if (MesDatas.NPAR == 3) points->InsertNextPoint(MesDatas.TabApp[cl][p + 0] * cube_size, MesDatas.TabApp[cl][p + 1] * cube_size, MesDatas.TabApp[cl][p + 2] * cube_size);
//			else if (MesDatas.NPAR == 2) points->InsertNextPoint(MesDatas.TabApp[cl][p + 0] * cube_size, MesDatas.TabApp[cl][p + 1] * cube_size, 0);
//			colors->InsertNextValue(cl);
//		}
//	}
//
//	vtkSmartPointer<vtkPolyData> polyData = vtkSmartPointer<vtkPolyData>::New();
//	polyData->SetPoints(points);
//	polyData->GetPointData()->SetScalars(colors);
//
//
//	auto glyphFilter = vtkSmartPointer<vtkVertexGlyphFilter>::New();
//	glyphFilter->SetInputData(polyData);
//	glyphFilter->Update();
//
//	vtkSmartPointer<vtkLookupTable> lookupTable = vtkSmartPointer<vtkLookupTable>::New();
//	lookupTable->SetNumberOfTableValues(3);
//	lookupTable->Build();
//
//	lookupTable->SetTableValue(0, 1.0, 0.0, 0.0); // Rouge
//	lookupTable->SetTableValue(1, 0.0, 0.0, 1.0); // Bleu
//	lookupTable->SetTableValue(2, 0.0, 1.0, 0.0); // Vert
//
//	vtkSmartPointer<vtkPolyDataMapper> point_mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
//	point_mapper->SetInputConnection(glyphFilter->GetOutputPort());
//	point_mapper->SetScalarModeToUsePointData();
//	point_mapper->SetLookupTable(lookupTable);
//	point_mapper->SetScalarRange(0, 2);
//
//	auto point_actor = vtkSmartPointer<vtkActor>::New();
//	point_actor->SetMapper(point_mapper);
//	point_actor->GetProperty()->SetPointSize(2);
//
//	double step = cube_size / grid_size;
//
//	// Création de l'image (grille 3D)
//	vtkSmartPointer<vtkImageData> image = vtkSmartPointer<vtkImageData>::New();
//	image->SetDimensions(grid_size, grid_size, grid_size);
//	image->SetSpacing(step, step, step);
//	image->SetOrigin(0, 0, 0);
//
//	// Tableau pour les scalaires
//	vtkSmartPointer<vtkIntArray> scalar_data = vtkSmartPointer<vtkIntArray>::New();
//	scalar_data->SetName("Classifier");
//	scalar_data->SetNumberOfComponents(1);
//
//	//Ptr<SVM> pClassifier = m_pClassifier.dynamicCast<SVM>();
//
//	Mat sampleMat = Mat(1, MesDatas.NPAR, CV_32FC1);
//
//
//	// Remplir la grille avec les valeurs du classifieur
//	for (int z = 0; z < grid_size; ++z)
//	{
//		for (int y = 0; y < grid_size; ++y)
//		{
//			for (int x = 0; x < grid_size; ++x)
//			{
//				double px = x * step;
//				double py = y * step;
//				double pz = z * step;
//				sampleMat.at<float>(0, 0) = px / cube_size;
//				sampleMat.at<float>(0, 1) = py / cube_size;
//				if (MesDatas.NPAR == 3) sampleMat.at<float>(0, 2) = pz / cube_size;
//
//				//int value = (int)pClassifier->predict(sampleMat);
//				int value = (int)Decide(ptr, TypeClassifieur, sampleMat);
//				scalar_data->InsertNextValue(value);
//
//			}
//		}
//	}
//
//	// Assigner les scalaires à l'image
//	image->GetPointData()->SetScalars(scalar_data);
//
//	// Appliquer Marching Cubes pour extraire la frontière
//	//vtkSmartPointer<vtkMarchingCubes> contour = vtkSmartPointer<vtkMarchingCubes>::New();
//	//contour->SetInputData(image);
//	//contour->SetValue(0, 0); // Frontière à f(x) = 0
//
//	vtkSmartPointer<vtkDiscreteMarchingCubes> contour = vtkSmartPointer<vtkDiscreteMarchingCubes>::New();
//	contour->SetInputData(image);
//	contour->GenerateValues(3, 0, 2);  // 3 valeurs (0, 1, 2)
//
//
//	// Mapper pour l'affichage
//	vtkSmartPointer<vtkPolyDataMapper> contour_mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
//	contour_mapper->SetInputConnection(contour->GetOutputPort());
//
//	vtkSmartPointer<vtkLookupTable> lut = vtkSmartPointer<vtkLookupTable>::New();
//	lut->SetNumberOfTableValues(3);
//	lut->SetTableValue(0, 1.0, 0.01, 0.01, tr); // Rouge pour la classe 0
//	lut->SetTableValue(1, 0.01, 1.0, 0.01, tr); // Vert pour la classe 1
//	lut->SetTableValue(2, 0.01, 0.01, 1.0, tr); // Bleu pour la classe 2
//	lut->Build();
//
//	contour_mapper->SetLookupTable(lut);
//	contour_mapper->SetScalarRange(0, 2);
//
//	// Acteur pour la visualisation
//	vtkSmartPointer<vtkActor> contour_actor = vtkSmartPointer<vtkActor>::New();
//	contour_actor->SetMapper(contour_mapper);
//	//	contour_actor->GetProperty()->SetColor(0.0, 1.0, 1.0);  // Vert pour la frontière
//	contour_actor->GetProperty()->SetOpacity(tr);  // Transparence
//
//	// Rendu
//	vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
//	renderer->AddActor(contour_actor);
//	renderer->AddActor(point_actor);
//	renderer->SetBackground(0.001, 0.001, 0.001); // Couleur de fond
//
//	vtkSmartPointer<vtkRenderWindow> render_window = vtkSmartPointer<vtkRenderWindow>::New();
//	render_window->AddRenderer(renderer);
//	render_window->SetSize(800, 800);
//	vtkSmartPointer<vtkRenderWindowInteractor> render_window_interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
//	render_window_interactor->SetRenderWindow(render_window);
//
//	// Lancer l'interaction
//	render_window->Render();
//	render_window_interactor->Start();// TODO: ajoutez ici le code de votre gestionnaire de commande
//}

bool CChildFrame::LoadClassifier() {
	// Définition des extensions et filtres associés
	struct ClassifierInfo {
		CString extension;
		CString description;
		function<Ptr<StatModel>(const String&)> loader;
		TypeClassif type;
	};

	vector<ClassifierInfo> classifiers = {
		{ _T("*.cvmodel"), _T("SVM Model (*.cvmodel)"), [](const String& path) { return Algorithm::load<SVM>(path); }, mSVM },
		{ _T("*.cvkppv"), _T("KNN Model (*.cvkppv)"), [](const String& path) { return Algorithm::load<KNearest>(path); }, mKPPV },
		{ _T("*.cvRF"), _T("Random Forest (*.cvRF)"), [](const String& path) { return Algorithm::load<RTrees>(path); }, mRF },
		{ _T("*.cvMlp"), _T("MLP Model (*.cvMlp)"), [](const String& path) { return Algorithm::load<ANN_MLP>(path); }, mMLP },
		{ _T("*.cvNB"), _T("Naive Bayes (*.cvNB)"), [](const String& path) { return Algorithm::load<NormalBayesClassifier>(path); }, mNB },
		{ _T("*.cvEM"), _T("Expectation Maximization (*.cvEM)"), [](const String& path) { return Algorithm::load<EM>(path); }, mEM },
		{ _T("*.cvDT"), _T("Decision Tree (*.cvDT)"), nullptr, mDT } // Chargeur spécial pour MultiClassBoost
	};

	// Construction du filtre combiné
	CString filter = _T("Tous les fichiers classifieurs (");
	for (const auto& clf : classifiers) {
		filter += clf.extension + _T(";");
	}
	filter.TrimRight(_T(";")); // Supprimer le dernier point-virgule
	filter += _T(")|");
	for (const auto& clf : classifiers) {
		filter += clf.extension + _T(";");
	}
	filter.TrimRight(_T(";")); // Supprimer le dernier point-virgule
	filter += _T("||");

	// Ajouter chaque modèle séparément
	for (const auto& clf : classifiers) {
		filter += clf.description + _T("|") + clf.extension + _T("||");
	}

	// Affichage de la boîte de dialogue
	CFileDialog FileDialog(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, filter, this);
	if (FileDialog.DoModal() != IDOK) return false;

	// Récupération du fichier sélectionné
	CString ChaineModel = FileDialog.GetPathName();
	m_PathData = ChaineModel.Left(ChaineModel.ReverseFind('.'));
	String pm(ChaineModel);

	// Détection du classifieur en fonction de l'extension
	for (const auto& clf : classifiers) {
		if (ChaineModel.Right(clf.extension.GetLength() - 1).CompareNoCase(clf.extension.Mid(1)) == 0)
		{
			m_pClassifier.release();
			if (clf.type == mDT) {
				// Gestion spéciale pour MultiClassBoost
				Ptr<MultiClassBoost> dTree = MultiClassBoost::create();
				dTree->load(pm);
				m_pClassifier = dTree;
				MesDatasMod.m_nbParametres = dTree->m_nbfeatures;
			}
			else {
				// Charger un modèle standard OpenCV
				m_pClassifier = clf.loader(pm);
				Ptr<StatModel> pClassifier = m_pClassifier.dynamicCast<StatModel>();
				MesDatasMod.m_nbParametres = pClassifier->getVarCount();
			}
			LastType = clf.type;

			return true;
		}
	}

	// Si aucune extension ne correspond
	return false;
}





bool CChildFrame::LoadClassifier(TypeClassif tc)
{

	String pm;
	CString ch1, ch2;
	switch (tc)
	{
	case mSVM:
	{
		ch1 = "*.cvmodel";
		ch2 = "Model (*.cvmodel)|*.cvmodel||";
		CFileDialog FileData2(TRUE, ch1, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, ch2, this);
		if (FileData2.DoModal() != IDOK) return false;
		ChaineModel = FileData2.GetPathName();
		m_PathData = ChaineModel.Left(ChaineModel.ReverseFind('.'));
		pm.assign(ChaineModel);
		m_pClassifier.release();
		m_pClassifier = Algorithm::load<SVM>(pm);

		break;
	}
	case mKPPV: {
		ch1 = "*.cvkppv";
		ch2 = "Model (*.cvkppv)|*.cvkppv||";
		CFileDialog FileData2(TRUE, ch1, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, ch2, this);
		if (FileData2.DoModal() != IDOK) return false;
		ChaineModel = FileData2.GetPathName();
		m_PathData = ChaineModel.Left(ChaineModel.ReverseFind('.'));
		pm.assign(ChaineModel);
		m_pClassifier.release();
		m_pClassifier = Algorithm::load<KNearest>(pm);
		break;
	}
	case mRF: {
		ch1 = "*.cvRF";
		ch2 = "Model (*.cvRF)|*.cvRF||";
		CFileDialog FileData2(TRUE, ch1, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, ch2, this);
		if (FileData2.DoModal() != IDOK) return false;
		ChaineModel = FileData2.GetPathName();
		m_PathData = ChaineModel.Left(ChaineModel.ReverseFind('.'));
		pm.assign(ChaineModel);
		m_pClassifier.release();
		m_pClassifier = Algorithm::load<RTrees>(pm);
		break;
	}
	case mMLP: {
		ch1 = "*.cvMlp";
		ch2 = "Model (*.cvMlp)|*.cvMlp||";
		CFileDialog FileData2(TRUE, ch1, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, ch2, this);
		if (FileData2.DoModal() != IDOK) return false;
		ChaineModel = FileData2.GetPathName();
		m_PathData = ChaineModel.Left(ChaineModel.ReverseFind('.'));
		pm.assign(ChaineModel);
		m_pClassifier.release();
		m_pClassifier = Algorithm::load<ANN_MLP>(pm);
		break;
	}
	case mNB: {
		ch1 = "*.cvNB";
		ch2 = "Model (*.cvNB)|*.cvNB||";
		CFileDialog FileData2(TRUE, ch1, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, ch2, this);
		if (FileData2.DoModal() != IDOK) return false;
		ChaineModel = FileData2.GetPathName();
		m_PathData = ChaineModel.Left(ChaineModel.ReverseFind('.'));
		pm.assign(ChaineModel);
		m_pClassifier.release();
		m_pClassifier = Algorithm::load<NormalBayesClassifier>(pm);
		break;
	}
	case mEM: {
		ch1 = "*.cvEM";
		ch2 = "Model (*.cvEM)|*.cvEM||";
		CFileDialog FileData2(TRUE, ch1, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, ch2, this);
		if (FileData2.DoModal() != IDOK) return false;
		ChaineModel = FileData2.GetPathName();
		m_PathData = ChaineModel.Left(ChaineModel.ReverseFind('.'));
		pm.assign(ChaineModel);
		m_pClassifier.release();
		m_pClassifier = Algorithm::load<EM>(pm);
		break;
	}
	case mDT: {
		ch1 = "*.cvDT";
		ch2 = "Model (*.cvDT)|*.cvDT||";
		CFileDialog FileData2(TRUE, ch1, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, ch2, this);
		if (FileData2.DoModal() != IDOK) return false;
		ChaineModel = FileData2.GetPathName();
		m_PathData = ChaineModel.Left(ChaineModel.ReverseFind('.'));
		pm.assign(ChaineModel);
		m_pClassifier.release();
		Ptr<MultiClassBoost> dTree = MultiClassBoost::create();
		dTree->load(pm);
		m_pClassifier = dTree;
		break;
	}
	default:
		//	m_pClassifier = nullptr;
		return false;
		break;
	}
	LastType = tc;
	return true;
}

void CChildFrame::RotateScene()
{

	vtkCamera* camera = m_renderer->GetActiveCamera();

	camera->Azimuth(m_angleAzimuth);

	// Rotation verticale avec limitation [-80°, 80°]
	//double newElevation = m_elevationSum + m_angleElevation;
	//if (newElevation > -80.0 && newElevation < 80.0)
	//{
	//	camera->Elevation(m_angleElevation);
	//	m_elevationSum = newElevation;
	//}

	//camera->SetViewUp(0, 0, 1); // Assurer un haut constant
	m_render_window->Render(); // Rafraîchir l'affichage

}

void CChildFrame::processGridSection(int startZ, int endZ, int gridSize, double step, double cubeSize,
	int NPAR, TypeClassif TypeClassifieur, void* ptr,
	vtkSmartPointer<vtkIntArray> finalScalarData, std::mutex& dataMutex)
{
	cv::Mat sampleMat = cv::Mat(1, NPAR, CV_32FC1);

	for (int z = startZ; z < endZ; ++z) {
		for (int y = 0; y < gridSize; ++y) {
			for (int x = 0; x < gridSize; ++x) {
				double px = x * step;
				double py = y * step;
				double pz = z * step;
				sampleMat.at<float>(0, 0) = px / cubeSize;
				sampleMat.at<float>(0, 1) = py / cubeSize;
				if (NPAR == 3) sampleMat.at<float>(0, 2) = pz / cubeSize;

				int value = (int)Decide(ptr, TypeClassifieur, sampleMat);
				vtkIdType index = (z * gridSize * gridSize) + (y * gridSize) + x;
				{
					std::lock_guard<std::mutex> lock(dataMutex);
					finalScalarData->SetValue(index, value);
				}
			}
		}
	}
}

void CChildFrame::VtkRenderingThread()
{
	////// Configurer l'interacteur pour utiliser un timer au lieu de bloquer
	////m_render_window_interactor->Initialize();
	////m_render_window_interactor->CreateRepeatingTimer(30); // 30ms = ~33 fps

	////// Configurer un observateur pour gérer les mises à jour
	////vtkSmartPointer<vtkCallbackCommand> timerCallback = vtkSmartPointer<vtkCallbackCommand>::New();
	////timerCallback->SetCallback([](vtkObject* caller, unsigned long eventId, void* clientData, void* callData) {
	////	CChildFrame* self = static_cast<CChildFrame*>(clientData);
	////	self->m_render_window->Render();

	////	// Vérifier si on doit arrêter le thread
	////	if (self->m_stopVtkThread || self->m_closeCallback->IsWindowClosed()) {
	////		vtkRenderWindowInteractor* interactor = static_cast<vtkRenderWindowInteractor*>(caller);
	////		interactor->ExitCallback();
	////	}
	////	});
	////timerCallback->SetClientData(this);

	////// Ajouter l'observateur de timer
	////m_render_window_interactor->AddObserver(vtkCommand::TimerEvent, timerCallback);

	////// Démarrer la boucle d'événements de l'interacteur
	////m_render_window_interactor->Start();

	////// Le thread se termine quand l'interacteur est arrêté
	////m_vtkRunning = false;
}


void CChildFrame::Visu3d(void* ptr, TypeClassif TypeClassifieur)
{
	//InitialisationVTK();

	m_Done = false;


	float tr = m_opacity; // tranparence 0.5 par defaut
	m_lut_contour = vtkSmartPointer<vtkLookupTable>::New();
	m_lut_contour->SetNumberOfTableValues(MesDatasMod.m_nbClasses);
	for (int cl = 0; cl < MesDatasMod.m_nbClasses; cl++)
	{
		double Rouge, Vert, Bleu;
		//if ((cl % 3) == 0) Rouge = 255 - (16 * cl); else Rouge = 0;
		//if (((cl + 1) % 3) == 0) Vert = 255 - (16 * cl);  else Vert = 0;
		//if (((cl + 2) % 3) == 0) Bleu = 255 - (16 * cl); else Bleu = 0;
		if (cl < colorPalette.size())
		{
			uchar dec = 64;
			Rouge = (double)max(colorPalette[cl][2] - dec, 0);
			Vert = (double)max(colorPalette[cl][1] - dec, 0);
			Bleu = (double)max(colorPalette[cl][0] - dec, 0);
		}
		else { Rouge = Vert = Bleu = 200; }
		Rouge /= 255.0;
		Vert /= 255.0;
		Bleu /= 255.0;

		m_lut_contour->SetTableValue(cl, Rouge, Vert, Bleu, tr); // Rouge pour la classe 0
	}
	m_lut_contour->Build();

	m_lookupTable = vtkSmartPointer<vtkLookupTable>::New();
	m_lookupTable->SetNumberOfTableValues(MesDatasMod.m_nbClasses);
	for (int cl = 0; cl < MesDatasMod.m_nbClasses; cl++)
	{
		double Rouge, Vert, Bleu;
		//if ((cl % 3) == 0) Rouge = 255 - (16 * cl); else Rouge = 0;
		//if (((cl + 1) % 3) == 0) Vert = 255 - (16 * cl);  else Vert = 0;
		//if (((cl + 2) % 3) == 0) Bleu = 255 - (16 * cl); else Bleu = 0;
		if (cl < colorPalette.size())
		{
			Rouge = (double)(colorPalette[cl][2]);
			Vert = (double)(colorPalette[cl][1]);
			Bleu = (double)(colorPalette[cl][0]);
		}
		else { Rouge = Vert = Bleu = 200; }

		m_lookupTable->SetTableValue(cl, Rouge / 255, Vert / 255, Bleu / 255); // Rouge
	}
	m_lookupTable->Build();

	if (m_closeCallback == nullptr) m_closeCallback = CloseCallback::New();
	if (m_closeCallback != nullptr)
	{
		if (m_closeCallback->IsWindowClosed())
		{
			if (!m_VisuDynamique)
			{
				Fenetre_Progression = new CDaliogProgression;
				Fenetre_Progression->Create(IDD_DIALOG_PROGRESS, NULL);
				Fenetre_Progression->Valeur_Maxi = 100;
				Fenetre_Progression->ShowWindow(SW_SHOW);
				Fenetre_Progression->SetWindowTextA("Veuillez patienter");
				//Fenetre_Progression->SetTimer(1, 1000, NULL);
			}

			double step = m_cube_size / m_grid_size;
			if (/*m_render_window_interactor == nullptr &&*/ MesDatasMod.m_nbParametres == 3)
			{
				m_points = vtkSmartPointer<vtkPoints>::New();
				m_colors = vtkSmartPointer<vtkFloatArray>::New();
				m_colors->SetNumberOfComponents(1);
				m_colors->SetName("Colors");

				// Création de l'image (grille 3D)
				m_classificationGrid = vtkSmartPointer<vtkImageData>::New();
				m_classificationGrid->SetDimensions(m_grid_size, m_grid_size, m_grid_size);
				m_classificationGrid->SetSpacing(step, step, step);
				m_classificationGrid->SetOrigin(0, 0, 0);

				for (int k = 0; k < MesDatasMod.m_trainingData.rows; k++)
				{
					int cl = (int)MesDatasMod.m_trainingLabels.at<float>(k, 0);
					if (MesDatasMod.m_nbParametres == 3) m_points->InsertNextPoint(MesDatasMod.m_trainingData.at<float>(k, 0) * m_cube_size, MesDatasMod.m_trainingData.at<float>(k, 1) * m_cube_size, MesDatasMod.m_trainingData.at<float>(k, 2) * m_cube_size);
					else if (MesDatasMod.m_nbParametres == 2) m_points->InsertNextPoint(MesDatasMod.m_trainingData.at<float>(k, 0) * m_cube_size, MesDatasMod.m_trainingData.at<float>(k, 1) * m_cube_size, 0);
					m_colors->InsertNextValue(cl);
				}


				m_polyData = vtkSmartPointer<vtkPolyData>::New();
				m_polyData->SetPoints(m_points);
				m_polyData->GetPointData()->SetScalars(m_colors);
				auto glyphFilter = vtkSmartPointer<vtkVertexGlyphFilter>::New();
				glyphFilter->SetInputData(m_polyData);
				glyphFilter->Update();


				m_point_mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
				m_point_mapper->SetInputConnection(glyphFilter->GetOutputPort());
				m_point_mapper->SetScalarModeToUsePointData();
				m_point_mapper->SetLookupTable(m_lookupTable);
				m_point_mapper->SetScalarRange(0, MesDatasMod.m_nbClasses - 1);

				m_point_actor = vtkSmartPointer<vtkActor>::New();
				m_point_actor->SetMapper(m_point_mapper);
				m_point_actor->GetProperty()->SetPointSize(2);

				m_contour = vtkSmartPointer<vtkDiscreteMarchingCubes>::New(); // pour frontieres
				m_contour_mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
				//m_lut_contour = vtkSmartPointer<vtkLookupTable>::New();
				//m_lut_contour->SetNumberOfTableValues(MesDatas.NbClass);

				m_contour_actor = vtkSmartPointer<vtkActor>::New();

				if (!m_DataOnly)
				{
					////vtkSmartPointer<vtkIntArray> scalar_data = vtkSmartPointer<vtkIntArray>::New();
					////scalar_data->SetName("Classifier");
					////scalar_data->SetNumberOfComponents(1);
					////Mat sampleMat = Mat(1, MesDatas.NPAR, CV_32FC1);
					////// Remplir la grille avec les valeurs du classifieur
					////for (int z = 0; z < m_grid_size; ++z)
					////{
					////	for (int y = 0; y < m_grid_size; ++y)
					////	{
					////		for (int x = 0; x < m_grid_size; ++x)
					////		{
					////			double px = x * step;
					////			double py = y * step;
					////			double pz = z * step;
					////			sampleMat.at<float>(0, 0) = px / m_cube_size;
					////			sampleMat.at<float>(0, 1) = py / m_cube_size;
					////			if (MesDatas.NPAR == 3) sampleMat.at<float>(0, 2) = pz / m_cube_size;
					////			int value = (int)Decide(ptr, TypeClassifieur, sampleMat);
					////			scalar_data->InsertNextValue(value);
					////		}
					////	}
					////}
					////m_classificationGrid->GetPointData()->SetScalars(scalar_data);
					//double step = m_cube_size / m_grid_size;
					double step = m_cube_size / m_grid_size;
					int numThreads = m_NbThread;
					int slicePerThread = m_grid_size / numThreads;
					std::vector<std::thread> threads;
					std::mutex dataMutex;

					// **1. Pré-allouer le tableau global pour stocker les résultats**
					vtkSmartPointer<vtkIntArray> finalScalarData = vtkSmartPointer<vtkIntArray>::New();
					finalScalarData->SetNumberOfComponents(1);
					finalScalarData->SetNumberOfTuples(m_grid_size * m_grid_size * m_grid_size); // Pré-allocation

					// **2. Démarrer les threads**
					for (int i = 0; i < numThreads; ++i)
					{
						int startZ = i * slicePerThread;
						int endZ = (i == numThreads - 1) ? m_grid_size : (i + 1) * slicePerThread;
						threads.emplace_back([this, startZ, endZ, step, TypeClassifieur, ptr, finalScalarData, &dataMutex]()
							{
								processGridSection(startZ, endZ, this->m_grid_size, step, this->m_cube_size, this->MesDatasMod.m_nbParametres, TypeClassifieur, ptr, finalScalarData, dataMutex);
							});
					}
					for (auto& t : threads) {
						t.join();
					}

					// Mettre à jour le VTK
					m_classificationGrid->GetPointData()->SetScalars(finalScalarData);

					// fin partie multithread


					m_contour->SetInputData(m_classificationGrid);
					m_contour->GenerateValues(MesDatasMod.m_nbClasses, 0, MesDatasMod.m_nbClasses - 1);  // 3 valeurs (0, 1, 2)
					m_contour->Update();

					//vtkSmartPointer<vtkCleanPolyData> cleaner = vtkSmartPointer<vtkCleanPolyData>::New();
					//cleaner->SetInputConnection(m_contour->GetOutputPort());
					//cleaner->SetTolerance(0.005);
					//cleaner->ConvertPolysToLinesOn();
					//cleaner->ConvertStripsToPolysOn();

					//vtkSmartPointer<vtkSmoothPolyDataFilter> smoother = vtkSmartPointer<vtkSmoothPolyDataFilter>::New();
					//smoother->SetInputConnection(cleaner->GetOutputPort());
					//smoother->SetNumberOfIterations(10);
					//smoother->SetRelaxationFactor(0.5);

					//vtkSmartPointer<vtkWindowedSincPolyDataFilter> inter = vtkSmartPointer<vtkWindowedSincPolyDataFilter>::New();
					//inter->SetInputConnection(smoother->GetOutputPort());
					//inter->SetNumberOfIterations(50);
					//inter->SetPassBand(0.1);  // Facteur de lissage
					//inter->FeatureEdgeSmoothingOn();
					//inter->BoundarySmoothingOn();
					//inter->Update();

					//vtkSmartPointer<vtkSubdivisionFilter> subdivider = vtkSmartPointer<vtkLoopSubdivisionFilter>::New();
					//subdivider->SetInputConnection(inter->GetOutputPort());
					//subdivider->SetNumberOfSubdivisions(4);  // Augmente la résolution du maillage
					//subdivider->Update();

					m_contour_mapper->SetInputConnection(m_contour->GetOutputPort());

					m_contour_mapper->SetLookupTable(m_lut_contour);
					m_contour_mapper->SetScalarRange(0, MesDatasMod.m_nbClasses - 1);
					m_contour_actor->SetMapper(m_contour_mapper);
					m_contour_actor->GetProperty()->SetOpacity(tr);  // Transparence
				}

				m_renderer = vtkSmartPointer<vtkRenderer>::New();
				m_render_window = vtkSmartPointer<vtkRenderWindow>::New();
				m_render_window->SetWindowName(m_TitreFen3D.c_str());
				m_render_window->AddRenderer(m_renderer);
				m_render_window_interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
				m_render_window_interactor->SetRenderWindow(m_render_window);

				if (!m_DataOnly) m_renderer->AddActor(m_contour_actor);
				m_renderer->AddActor(m_point_actor);
				m_renderer->SetBackground(0.001, 0.001, 0.001); // Couleur de fond
				//if (m_closeCallback==nullptr) m_closeCallback = CloseCallback::New();
				//m_render_window_interactor->AddObserver(vtkCommand::ExitEvent, m_closeCallback);
				m_closeCallback->windowClosed = false; // fenetre ouverte
				m_render_window_interactor->AddObserver(vtkCommand::ExitEvent, m_closeCallback);

				if (!m_VisuDynamique)
				{
					//Fenetre_Progression->KillTimer(1);
					Fenetre_Progression->DestroyWindow();
					delete Fenetre_Progression;
				}
				// Mettre à jour l'actor existant
				if (!m_DataOnly) m_contour_actor->SetMapper(m_contour_mapper);

				m_render_window->SetSize(m_windowWidth, m_windowHeight);
				m_render_window->Render();
				//m_render_window_interactor->AddObserver(vtkCommand::TimerEvent, this, &CChildFrame::RotateScene);
				if (m_animate3D) StartRotation();
				m_Done = true;

				// test modif thread 18 mars
				// m_render_window_interactor->Start(); // a remettre 18 mars
				m_render_window_interactor->ProcessEvents();


				//if (m_closeCallback->windowClosed == false)
			   //{
			   //	// Arrêter le thread précédent s'il est en cours d'exécution
			   //	if (m_vtkRunning)
			   //	{
			   //		m_stopVtkThread = true;
			   //		if (m_vtkThread.joinable()) {
			   //			m_vtkThread.join();
			   //		}
			   //		m_stopVtkThread = false;
			   //	}

			   //	// Lancer un nouveau thread pour le rendu VTK
			   //	m_vtkRunning = true;
			   //	m_vtkThread = std::thread(&CChildFrame::VtkRenderingThread, this);
			   //	m_vtkThread.detach(); // Détacher le thread pour qu'il s'exécute indépendamment

			   //	// Ne pas appeler m_render_window_interactor->Start() ici
			   //}




			}
		}
		else
		{
			if (m_DataOnly)
			{
				if (m_UpdateData)
				{
					// Clear previous points and colors
					m_points->Reset();
					m_colors->Reset();

					// Add updated points from MesDatasMod.m_trainingData
					for (int k = 0; k < MesDatasMod.m_trainingData.rows; k++)
					{
						int cl = (int)MesDatasMod.m_trainingLabels.at<float>(k, 0);
						if (MesDatasMod.m_nbParametres == 3)
							m_points->InsertNextPoint(
								MesDatasMod.m_trainingData.at<float>(k, 0) * m_cube_size,
								MesDatasMod.m_trainingData.at<float>(k, 1) * m_cube_size,
								MesDatasMod.m_trainingData.at<float>(k, 2) * m_cube_size
							);
						else if (MesDatasMod.m_nbParametres == 2)
							m_points->InsertNextPoint(
								MesDatasMod.m_trainingData.at<float>(k, 0) * m_cube_size,
								MesDatasMod.m_trainingData.at<float>(k, 1) * m_cube_size,
								0
							);
						m_colors->InsertNextValue(cl);
					}

					// Mark the data as modified to trigger update
					m_polyData->Modified();
					m_point_mapper->Update();
				}
				m_Done = true; return;
			}
			else
			{
				vtkSmartPointer<vtkActorCollection> actors = m_renderer->GetActors();
				actors->InitTraversal(); // Initialise le parcours des acteurs

				int count = 0;
				while (actors->GetNextActor()) {
					count++; // Incrémente le compteur pour chaque acteur trouvé
				}
				if (count == 1) m_renderer->AddActor(m_contour_actor);
			}

			if (!m_Loaded)  if (!LoadClassifier(TypeClassifieur)) {
				m_Done = false; return;
			}
			StopRotation();
			if (!m_VisuDynamique)
			{
				Fenetre_Progression = new CDaliogProgression;
				Fenetre_Progression->Create(IDD_DIALOG_PROGRESS, NULL);
				Fenetre_Progression->Valeur_Maxi = 10;
				Fenetre_Progression->ShowWindow(SW_SHOW);
				Fenetre_Progression->SetWindowTextA("Veuillez patienter");
				//Fenetre_Progression->SetTimer(1, 1000, NULL);
			}


			if (m_UpdateData)
			{
				// Clear previous points and colors
				m_points->Reset();
				m_colors->Reset();

				// Add updated points from MesDatasMod.m_trainingData
				for (int k = 0; k < MesDatasMod.m_trainingData.rows; k++)
				{
					int cl = (int)MesDatasMod.m_trainingLabels.at<float>(k, 0);
					if (MesDatasMod.m_nbParametres == 3)
						m_points->InsertNextPoint(
							MesDatasMod.m_trainingData.at<float>(k, 0) * m_cube_size,
							MesDatasMod.m_trainingData.at<float>(k, 1) * m_cube_size,
							MesDatasMod.m_trainingData.at<float>(k, 2) * m_cube_size
						);
					else if (MesDatasMod.m_nbParametres == 2)
						m_points->InsertNextPoint(
							MesDatasMod.m_trainingData.at<float>(k, 0) * m_cube_size,
							MesDatasMod.m_trainingData.at<float>(k, 1) * m_cube_size,
							0
						);
					m_colors->InsertNextValue(cl);
				}

				// Mark the data as modified to trigger update
				m_polyData->Modified();
				m_point_mapper->Update();
			}


			double step = m_cube_size / m_grid_size;
			int numThreads = m_NbThread;
			int slicePerThread = m_grid_size / numThreads;
			std::vector<std::thread> threads;
			std::mutex dataMutex;

			// **1. Pré-allouer le tableau global pour stocker les résultats**
			vtkSmartPointer<vtkIntArray> finalScalarData = vtkSmartPointer<vtkIntArray>::New();
			finalScalarData->SetNumberOfComponents(1);
			finalScalarData->SetNumberOfTuples(m_grid_size * m_grid_size * m_grid_size); // Pré-allocation

			// **2. Démarrer les threads**
			for (int i = 0; i < numThreads; ++i)
			{
				int startZ = i * slicePerThread;
				int endZ = (i == numThreads - 1) ? m_grid_size : (i + 1) * slicePerThread;
				threads.emplace_back([this, startZ, endZ, step, TypeClassifieur, ptr, finalScalarData, &dataMutex]()
					{
						processGridSection(startZ, endZ, this->m_grid_size, step, this->m_cube_size, this->MesDatasMod.m_nbParametres, TypeClassifieur, ptr, finalScalarData, dataMutex);
					});
			}
			for (auto& t : threads) {
				t.join();
			}

			m_classificationGrid->GetPointData()->SetScalars(finalScalarData);
			m_classificationGrid->Modified();

			if (!m_VisuDynamique)
			{
				Fenetre_Progression->DestroyWindow();
				delete Fenetre_Progression;
			}

			m_contour->SetInputData(m_classificationGrid);
			//m_contour->GenerateValues(3, 0, 2);  // 3 valeurs (0, 1, 2)
			m_contour->GenerateValues(MesDatasMod.m_nbClasses, 0, MesDatasMod.m_nbClasses - 1);
			m_contour->Update();
			m_contour_mapper->SetInputConnection(m_contour->GetOutputPort());
			// Mettre à jour l'actor existant

			m_contour_mapper->SetLookupTable(m_lut_contour);
			m_contour_mapper->SetScalarRange(0, MesDatasMod.m_nbClasses - 1);
			m_contour_actor->SetMapper(m_contour_mapper);
			m_contour_actor->GetProperty()->SetOpacity(tr);  // Transparence
			m_render_window->SetWindowName(m_TitreFen3D.c_str());
			m_Done = true;
			if (m_animate3D) StartRotation();
			m_render_window->Render();
		}

	}
}

//void CChildFrame::CleanUpVTK()
//{
//	if (m_render_window_interactor)
//	{
//		m_render_window_interactor->TerminateApp();
//		m_render_window_interactor->Delete();
//		m_render_window_interactor = nullptr;
//	}
//
//	if (m_render_window)
//	{
//		m_render_window->Finalize();
//		m_render_window->Delete();
//	//	m_render_window = nullptr;
//	}
//
//	if (m_classificationGrid)
//	{
//		m_classificationGrid->Delete();
//	//	m_classificationGrid = nullptr;
//	}
//
//	if (m_contour_actor)
//	{
//		m_contour_actor->Delete();
//	//	m_contour_actor = nullptr;
//	}
//
//	if (m_closeCallback)
//	{
//		delete(m_closeCallback);
//		m_closeCallback = nullptr;
//	}
//
//}

void CChildFrame::CleanUpVTK()
{

	//if (m_render_window && m_render_window->GetMapped())
	//{
	//	m_render_window->RemoveRenderer(m_renderer);  // Retirer le renderer avant la suppression
	//	m_render_window->Finalize();  // Finaliser correctement la fenêtre
	//	//	m_render_window->Delete();
	//}

	//if (m_render_window_interactor)
	//{
	//	m_render_window_interactor->Disable();  // Désactiver l'interaction proprement
	////	m_render_window_interactor->RemoveObservers(vtkCommand::ExitEvent);
	//	
	//	m_render_window_interactor->TerminateApp(); 
	//	//m_render_window_interactor->SetRenderWindow(nullptr); // Détache la fenêtre avant suppression
	//	
	//	//int r=m_render_window_interactor->GetReferenceCount();
	//	//if (r > 1) { m_render_window_interactor->Delete(); m_render_window_interactor = nullptr; }
	//	
	//}
	// 
	// test 18 mars
	//if (m_vtkRunning)
	//{
	//	m_stopVtkThread = true;
	//	if (m_vtkThread.joinable()) {
	//		m_vtkThread.join();
	//	}
	//}


	if (m_render_window && m_render_window->GetMapped())
	{
		m_render_window->RemoveRenderer(m_renderer);  // Retirer le renderer avant la suppression
		m_render_window->Finalize();  // Finaliser correctement la fenêtre
		//	m_render_window->Delete();

	}

	if (m_render_window_interactor)
	{
		//m_render_window_interactor->Disable();  // Désactiver l'interaction proprement
		m_render_window_interactor->TerminateApp();
		m_render_window_interactor->SetRenderWindow(nullptr); // Détache la fenêtre avant suppression

		//m_render_window_interactor->Delete();

	}


	if (m_renderer)
	{
		//	m_renderer->Delete();

	}

	if (m_classificationGrid)
	{
		//	m_classificationGrid->Delete();

	}

	if (m_contour_actor)
	{
		//	m_contour_actor->Delete();

	}

	//if (m_closeCallback)
	//{
	//	delete(m_closeCallback);
	//	m_closeCallback = nullptr;
	//}
}

//void CChildFrame::InitialisationVTK()
//{
//	//if (m_closeCallback != nullptr)
//	//{
//	////	if (!m_closeCallback->IsWindowClosed())
//	//	{
//	//		CleanUpVTK();
//	//	}
//	//}
//	double step = m_cube_size / m_grid_size;
//	if (m_render_window_interactor == nullptr && MesDatasMod.m_nbParametres ==3)
//	{
//		m_points = vtkSmartPointer<vtkPoints>::New();
//		m_colors = vtkSmartPointer<vtkFloatArray>::New();
//		m_colors->SetNumberOfComponents(1);
//		m_colors->SetName("Colors");
//
//		// Création de l'image (grille 3D)
//		m_classificationGrid = vtkSmartPointer<vtkImageData>::New();
//		m_classificationGrid->SetDimensions(m_grid_size, m_grid_size, m_grid_size);
//		m_classificationGrid->SetSpacing(step, step, step);
//		m_classificationGrid->SetOrigin(0, 0, 0);
//
//
//		float tr = 0.5; // tranparence
//
//		for (int cl = 0; cl < MesDatas.NbClass; cl++)
//		{
//			for (int k = 0; k < MesDatas.NbMes[cl]; k++)
//			{
//				int p = k * MesDatas.NPAR;
//
//				if (MesDatas.NPAR == 3) m_points->InsertNextPoint(MesDatas.TabApp[cl][p + 0] * m_cube_size, MesDatas.TabApp[cl][p + 1] * m_cube_size, MesDatas.TabApp[cl][p + 2] * m_cube_size);
//				else if (MesDatas.NPAR == 2) m_points->InsertNextPoint(MesDatas.TabApp[cl][p + 0] * m_cube_size, MesDatas.TabApp[cl][p + 1] * m_cube_size, 0);
//				m_colors->InsertNextValue(cl);
//			}
//		}
//
//		m_polyData = vtkSmartPointer<vtkPolyData>::New();
//		m_polyData->SetPoints(m_points);
//		m_polyData->GetPointData()->SetScalars(m_colors);
//
//
//		auto glyphFilter = vtkSmartPointer<vtkVertexGlyphFilter>::New();
//		glyphFilter->SetInputData(m_polyData);
//		glyphFilter->Update();
//
//		m_lookupTable = vtkSmartPointer<vtkLookupTable>::New();
//		m_lookupTable->SetNumberOfTableValues(3);
//		m_lookupTable->Build();
//
//		m_lookupTable->SetTableValue(0, 1.0, 0.0, 0.0); // Rouge
//		m_lookupTable->SetTableValue(1, 0.0, 0.0, 1.0); // Bleu
//		m_lookupTable->SetTableValue(2, 0.0, 1.0, 0.0); // Vert
//
//		m_point_mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
//		m_point_mapper->SetInputConnection(glyphFilter->GetOutputPort());
//		m_point_mapper->SetScalarModeToUsePointData();
//		m_point_mapper->SetLookupTable(m_lookupTable);
//		m_point_mapper->SetScalarRange(0, 2);
//
//		m_point_actor = vtkSmartPointer<vtkActor>::New();
//		m_point_actor->SetMapper(m_point_mapper);
//		m_point_actor->GetProperty()->SetPointSize(2);
//
//
//		// Rendu
//		m_renderer = vtkSmartPointer<vtkRenderer>::New();
//		m_renderer->AddActor(m_point_actor);
//		m_renderer->SetBackground(0.001, 0.001, 0.001); // Couleur de fond
//
//		m_render_window = vtkSmartPointer<vtkRenderWindow>::New();
//		m_render_window->AddRenderer(m_renderer);
//
//		vtkSmartPointer<vtkRenderWindowInteractor> m_render_window_interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
//		m_render_window_interactor->SetRenderWindow(m_render_window);
//
//		m_contour = vtkSmartPointer<vtkDiscreteMarchingCubes>::New(); // pour frontieres
//
//
//		
//
//
//		m_contour_mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
//		//m_contour_mapper->SetInputConnection(m_contour->GetOutputPort());
//
//		//vtkSmartPointer<vtkLookupTable> lut = vtkSmartPointer<vtkLookupTable>::New();
//		//lut->SetNumberOfTableValues(3);
//		//lut->SetTableValue(0, 1.0, 0.01, 0.01, tr); // Rouge pour la classe 0
//		//lut->SetTableValue(1, 0.01, 1.0, 0.01, tr); // Vert pour la classe 1
//		//lut->SetTableValue(2, 0.01, 0.01, 1.0, tr); // Bleu pour la classe 2
//		//lut->Build();
//		//m_contour_mapper->SetLookupTable(lut);
//		//m_contour_mapper->SetScalarRange(0, 2);
//
//		// Acteur pour la visualisation
//		m_contour_actor = vtkSmartPointer<vtkActor>::New();
//		//m_contour_actor->SetMapper(m_contour_mapper);
//		
//		m_contour_actor->GetProperty()->SetOpacity(tr);  // Transparence
//
//		//m_renderer->AddActor(m_contour_actor);
//
//		//if (m_closeCallback==nullptr) m_closeCallback = CloseCallback::New();
//		//m_render_window_interactor->AddObserver(vtkCommand::ExitEvent, m_closeCallback);
//
//		m_render_window->SetSize(800, 800);
//	
//		m_render_window->Render();
//		m_render_window_interactor->Start();
//
//	}
//}

//void CChildFrame::OnKppvKppvvisu3d()
//{
//	if (!LoadClassifier(mKPPV)) return;
//	if (m_pClassifier.dynamicCast<KNearest>()->getVarCount() != 3)
//	{
//		MessageBox("Nombre d'attributs du modele différent du nombre d'attributs des données 3D");
//		return;
//	}
//	m_Loaded = true;
//
//	if (MesDatasMod.m_nbParametres == 3 && m_pClassifier != nullptr && LastType == mKPPV)
//	{
//		m_TitreFen3D = "Classifieur KPPV";
//		Visu3d(m_pClassifier, mKPPV);
//	}
//}


//void CChildFrame::OnRandomforestRandomforestvisu3d()
//{
//
//
//	if (!LoadClassifier(mRF)) return;
//	if (m_pClassifier.dynamicCast<RTrees>()->getVarCount() != 3)
//	{
//		MessageBox("Nombre d'attributs du modele différent du nombre d'attributs des données 3D");
//		return;
//	}
//	m_Loaded = true;
//
//
//	if (MesDatasMod.m_nbParametres == 3 && m_pClassifier != nullptr && LastType == mRF)
//	{
//		m_TitreFen3D = "Classifieur Random Forest";
//		Visu3d(m_pClassifier, mRF);
//	}
//}
//
//
//void CChildFrame::OnNaivebayesVisu3d()
//{
//
//	if (!LoadClassifier(mNB)) return;
//	if (m_pClassifier.dynamicCast<NormalBayesClassifier>()->getVarCount() != 3)
//	{
//		MessageBox("Nombre d'attributs du modele différent du nombre d'attributs des données 3D");
//		return;
//	}
//	m_Loaded = true;
//
//	if (MesDatasMod.m_nbParametres == 3 && m_pClassifier != nullptr && LastType == mNB)
//	{
//		m_TitreFen3D = "Classifieur Naive Bayes";
//		Visu3d(m_pClassifier, mNB);
//	}
//}


//void CChildFrame::OnBoostingopencvVisu3d()
//{
//
//	if (!LoadClassifier(mDT)) return;
//	m_Loaded = true;
//
//	if (MesDatasMod.m_nbParametres == 3 && m_pClassifier != nullptr && LastType == mDT)
//	{
//		m_TitreFen3D = "Frontieres Boosting";
//		Visu3d(m_pClassifier, mDT);
//	}
//}

//void CChildFrame::OnPerceptronVisu3d()
//{
//
//	if (!LoadClassifier(mMLP)) return;
//	if (m_pClassifier.dynamicCast<ANN_MLP>()->getVarCount() != 3)
//	{
//		MessageBox("Nombre d'attributs du modele différent du nombre d'attributs des données 3D");
//		return;
//	}
//	m_Loaded = true;
//
//	if (MesDatasMod.m_nbParametres == 3 && m_pClassifier != nullptr && LastType == mMLP)
//	{
//		m_TitreFen3D = "Classifieur reseau de neurones";
//		Visu3d(m_pClassifier, mMLP);
//	}
//}
//void CChildFrame::OnSvmSvmopencvvisu3d()
//{
//
//	Cleannonmodal();
//	if (MesDatasMod.m_nbParametres > 3) { MessageBox("Nombre d'attributs superieur à 3", MB_OK); return; }
//
//	if (!LoadClassifier(mSVM)) return;
//	if (m_pClassifier.dynamicCast<SVM>()->getVarCount() <2 || m_pClassifier.dynamicCast<SVM>()->getVarCount()>3)
//	{
//		MessageBox("Nombre d'attributs du modele différent du nombre d'attributs des données");
//		return;
//	}
//	m_Loaded = true;
//
//
//	if (MesDatasMod.m_nbParametres == 3 && m_pClassifier != nullptr && LastType == mSVM)
//	{
//		m_TitreFen3D = "Classifieur SVM";
//		Visu3d(m_pClassifier, mSVM);
//	}
//	if (MesDatasMod.m_nbParametres == 2 && m_pClassifier != nullptr && LastType == mSVM) 
//	{
//		Invalidate(); // VisufrontiereOpencv(&svm, mSVM);
//	}
//
//}

//void CChildFrame::OnSvmSvmopencv3ddynamique()
//{
//	String pm;
//	CFileDialog FileData2(TRUE, "*.cvmodel", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Model (*.cvmodel)|*.cvmodel||", this);
//	if (FileData2.DoModal() != IDOK) return;
//	ChaineModel = FileData2.GetPathName();
//	m_PathData = ChaineModel.Left(ChaineModel.ReverseFind('.'));
//	pm.assign(ChaineModel);
//
//	Ptr<SVM> svm = Algorithm::load<SVM>(pm);
//
//	if (svm->getVarCount() != MesDatas.NPAR)
//	{
//		MessageBox("Nombre d'attributs du modele différent du nombre d'attributs des données");
//		return;
//	}
//	m_pClassifier = svm;
//	LastType = mSVM;
//
//
//	int grid_size = 70;
//	float tr = 0.5; // tranparenc
//	double step = cube_size / grid_size;
//
//
//	// Tableau pour les scalaires
//	vtkSmartPointer<vtkIntArray> scalar_data = vtkSmartPointer<vtkIntArray>::New();
//	scalar_data->SetName("Classifier");
//	scalar_data->SetNumberOfComponents(1);
//
//	//Ptr<SVM> pClassifier = m_pClassifier.dynamicCast<SVM>();
//
//	Mat sampleMat = Mat(1, MesDatas.NPAR, CV_32FC1);
//
//	int dims[3];
//	m_classificationGrid->GetDimensions(dims);
//	// Remplir la grille avec les valeurs du classifieur
//	for (int z = 0; z < dims[2]; z++)
//	{
//		for (int y = 0; y < dims[1]; y++)
//		{
//			for (int x = 0; x < dims[0]; x++)
//			{
//				double px = x * step;
//				double py = y * step;
//				double pz = z * step;
//				sampleMat.at<float>(0, 0) = px / cube_size;
//				sampleMat.at<float>(0, 1) = py / cube_size;
//				if (MesDatas.NPAR == 3) sampleMat.at<float>(0, 2) = pz / cube_size;
//
//				//int value = (int)pClassifier->predict(sampleMat);
//				int value = (int)Decide(m_pClassifier, LastType, sampleMat);
//				scalar_data->InsertNextValue(value);
//
//			}
//		}
//	}
//
//	// Assigner les scalaires à l'image
//
//	m_classificationGrid->GetPointData()->SetScalars(scalar_data);
//	m_classificationGrid->Modified();
//	m_contour->SetInputData(m_classificationGrid);
//	m_contour->GenerateValues(3, 0, 2);  // 3 valeurs (0, 1, 2)
//	m_contour_mapper->SetInputConnection(m_contour->GetOutputPort());
//	// Mettre à jour l'actor existant
//	m_contour_actor->SetMapper(m_contour_mapper);
//	m_render_window->Render();
//
//}
void CChildFrame::GenerateGaussianPoints(vtkSmartPointer<vtkPoints> points, vtkSmartPointer<vtkFloatArray> colors,
	double cx, double cy, double cz,
	double sigma, int numPoints, int colorId)
{
	std::default_random_engine generator;
	std::normal_distribution<double> distributionX(cx, sigma);
	std::normal_distribution<double> distributionY(cy, sigma);
	std::normal_distribution<double> distributionZ(cz, sigma);

	for (int i = 0; i < numPoints; ++i)
	{
		double x = distributionX(generator);
		double y = distributionY(generator);
		double z = distributionZ(generator);
		points->InsertNextPoint(x, y, z);
		colors->InsertNextValue(colorId);
	}
}

//void CChildFrame::GenerateSpiralPoints(	vtkSmartPointer<vtkPoints> points,	vtkSmartPointer<vtkFloatArray> colors,	double phase, double diameter, double length, int turns,
//	double noiseStdDev, int numPoints, double x0, double y0, double z0, int colorId)
//{
//	std::default_random_engine generator(std::random_device{}());
//	std::normal_distribution<double> noise(0.0, noiseStdDev);
//
//	for (int i = 0; i < numPoints; ++i)
//	{
//		double t = static_cast<double>(i) / numPoints; // Normalisé entre [0,1]
//		double angle = phase + 2.0 * 3.14159 * turns * t;
//		double radius = diameter / 2.0;
//
//		// Coordonnées de la spirale
//		double x = x0 + radius * cos(angle);
//		double y = y0 + radius * sin(angle);
//		double z = z0+ t * length;
//
//		// Ajout du bruit
//		x += noise(generator);
//		y += noise(generator);
//		z += noise(generator);
//
//		// Clamping pour éviter les dépassements
//		x = std::min(std::max(x, 0.0), 1.0);
//		y = std::min(std::max(y, 0.0), 1.0);
//		z = std::min(std::max(z, 0.0), 1.0);
//
//		points->InsertNextPoint(x, y, z);
//		colors->InsertNextValue(colorId); // Associer une classe couleur
//	}
//}

void CChildFrame::GenerateSpiralPoints(vtkSmartPointer<vtkPoints> points, vtkSmartPointer<vtkFloatArray> colors,
	double phase, double diameter, double length, int turns,
	double noiseStdDev, int numPoints, double x0, double y0, double z0, int colorId)
{
	std::default_random_engine generator(std::random_device{}());
	std::normal_distribution<double> noise(0.0, noiseStdDev);

	// Générer un vecteur aléatoire pour l'axe du ressort
	std::uniform_real_distribution<double> dist(-1.0, 1.0);
	double vx = dist(generator);
	double vy = dist(generator);
	double vz = dist(generator);

	// Normaliser le vecteur (pour s'assurer qu'il représente bien une direction)
	double norm = std::sqrt(vx * vx + vy * vy + vz * vz);
	vx /= norm;
	vy /= norm;
	vz /= norm;

	// Trouver un vecteur perpendiculaire à (vx, vy, vz)
	double ux = -vy;
	double uy = vx;
	double uz = 0;
	norm = std::sqrt(ux * ux + uy * uy + uz * uz);
	if (norm < 1e-6) { // Si trop petit, choisir une autre direction
		ux = -vz;
		uy = 0;
		uz = vx;
		norm = std::sqrt(ux * ux + uy * uy + uz * uz);
	}
	ux /= norm;
	uy /= norm;
	uz /= norm;

	// Calculer v = w × u (produit vectoriel)
	double vx2 = vy * uz - vz * uy;
	double vy2 = vz * ux - vx * uz;
	double vz2 = vx * uy - vy * ux;

	// Générer les points
	for (int i = 0; i < numPoints; ++i)
	{
		double t = static_cast<double>(i) / numPoints; // Normalisé entre [0,1]
		double angle = phase + 2.0 * 3.14159 * turns * t;
		double radius = diameter / 2.0;

		// Coordonnées de la spirale dans le repère local
		double xLocal = radius * cos(angle);
		double yLocal = radius * sin(angle);
		double zLocal = t * length;

		// Appliquer la rotation
		double x = x0 + xLocal * ux + yLocal * vx2 + zLocal * vx;
		double y = y0 + xLocal * uy + yLocal * vy2 + zLocal * vy;
		double z = z0 + xLocal * uz + yLocal * vz2 + zLocal * vz;

		// Ajout du bruit
		x += noise(generator);
		y += noise(generator);
		z += noise(generator);

		points->InsertNextPoint(x, y, z);
		colors->InsertNextValue(colorId); // Associer une classe couleur
	}
}


void CChildFrame::SavePointsToCSV(const std::string& filename, vtkSmartPointer<vtkPoints> points, vtkSmartPointer<vtkFloatArray> colors)
{
	std::ofstream file(filename);
	double cx, cy, cz;
	if (!file.is_open())
	{
		std::cerr << "Erreur lors de l'ouverture du fichier." << std::endl;
		return;
	}

	// Écrire l'entête (facultatif)
	file << "Classe,x,y,z\n";

	// Parcourir les points et les écrire dans le fichier
	for (vtkIdType i = 0; i < points->GetNumberOfPoints(); ++i)
	{
		double* point = points->GetPoint(i);
		int color = (int)colors->GetValue(i);

		cx = point[0];
		if (point[0] < 0) cx = 0;
		if (point[0] > 1) cx = 1;
		cy = point[1];
		if (point[1] < 0) cy = 0;
		if (point[1] > 1) cy = 1;
		cz = point[2];
		if (point[2] < 0) cz = 0;
		if (point[2] > 1) cz = 1;
		// Écrire la ligne : classe, x, y, z
		file << color << "," << cx << "," << cy << "," << cz << "\n";
	}

	file.close();
	//std::cout << "Les points ont été sauvegardés dans " << filename << std::endl;
}


void CChildFrame::OnEditionGaussiennes3d()
{

	CDialGauss DialGauss;
	CString ft;
	if (DialGauss.DoModal() == IDCANCEL) return;
	// Paramètres des distributions
	int numPointsPerClass = DialGauss.m_Sample;
	vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
	vtkSmartPointer<vtkFloatArray> colors = vtkSmartPointer<vtkFloatArray>::New();
	int seed;
	int      tt;
	struct   tm* ti;
	time_t   ltime;
	time(&ltime);
	ti = localtime(&ltime);
	seed = ti->tm_sec;

	std::string NomFichier;
	ft.Format(
		_T("%s_%d_seed_%d_classes_%d_samples.csv"),
		DialGauss.m_Prefix,
		seed,
		DialGauss.m_NbClasses,
		numPointsPerClass
	);
	NomFichier.assign(ft);


	srand(seed);
	for (int k = 0; k < DialGauss.m_NbClasses; k++)
	{

		double cx = ((float)rand()) / RAND_MAX;
		double cy = ((float)rand()) / RAND_MAX;
		double cz = ((float)rand()) / RAND_MAX;
		double sigma = ((float)rand()) / RAND_MAX;
		cx = (cx * 0.8) + 0.1;
		cy = (cy * 0.8) + 0.1;
		cz = (cz * 0.8) + 0.1;
		sigma = (sigma * 0.05) + 0.01; // entre 0.01 et 0.06
		GenerateGaussianPoints(points, colors, cx, cy, cz, sigma, numPointsPerClass, k);
	}
	SavePointsToCSV(m_datasPath + NomFichier, points, colors);
	MessageBox("Le fichier " + ft + " a été enregistré");
}


void CChildFrame::OnTimer(UINT_PTR nIDEvent)
{
	int t = 0;
	if (nIDEvent == 1 && Fenetre_Progression)
	{
		static int position = 0;
		Fenetre_Progression->m_ControlProgressBar.SetPos(position);
		Fenetre_Progression->Invalidate();
		Fenetre_Progression->UpdateWindow();
		position = (position + 10) % 101;
	}
	CWnd::OnTimer(nIDEvent);
}

void CChildFrame::LoadConfig(const std::string& filename)
{
	char buffer[MAX_PATH]; // Buffer pour stocker le chemin
	if (GetModuleFileNameA(NULL, buffer, MAX_PATH))
	{
		std::filesystem::path exePath(buffer);
		m_exePath = exePath.parent_path().string();
		//std::cout << "Chemin de l'exécutable : " << m_exePath << std::endl;
	}
	else {
		std::cerr << "Erreur : impossible de récupérer le chemin de l'exécutable.\n";
		m_exePath = "./";
	}

	// Créer le dossier "datas" s'il n'existe pas
	std::string folder = m_exePath + "\\datas";

	if (_mkdir(folder.c_str()) == 0) {
		//std::cout << "Dossier 'datas' créé.\n";

	}
	else {
		std::cerr << "Erreur : impossible de créer le dossier 'datas'.\n";
	}

	m_datasPath = folder + "\\";
	//// Définir "datas" comme dossier courant
	//if (_chdir(folder.c_str()) != 0) {
	//	std::cerr << "Erreur : impossible de définir 'datas' comme dossier courant.\n";
	//}

	// Ouvrir le fichier de configuration dans le dossier "datas"
	std::ifstream file(m_exePath + "\\" + filename);
	if (!file) {
		std::cerr << "Fichier de config non trouvé'. Utilisation des valeurs par défaut.\n";
		return;
	}

	std::string line, section;
	while (std::getline(file, line)) {
		if (line.empty() || line[0] == ';' || line[0] == '#') continue; // Ignore les commentaires et lignes vides

		if (line[0] == '[') {
			section = line.substr(1, line.find(']') - 1);
		}
		else
		{
			std::istringstream is_line(line);
			std::string key;
			if (std::getline(is_line, key, '='))
			{
				std::string value;
				if (std::getline(is_line, value))
				{
					if (section == "Window") {
						if (key == "Width") m_windowWidth = std::stoi(value);
						if (key == "Height") m_windowHeight = std::stoi(value);
					}
					else if (section == "VTK")
					{
						if (key == "Grid") m_grid_size = std::stod(value);
						if (key == "Opacity") m_opacity = std::stod(value);
						if (key == "Animation") {
							int a = std::stod(value);
							if (a == 0) m_animate3D = false; else m_animate3D = true;
						}
						if (key == "DataOnly") {
							int a = std::stod(value);
							if (a == 0) m_DataOnly = false; else m_DataOnly = true;
						}
						if (key == "NbThread") m_NbThread = std::stod(value);
					}
					//else if (section == "Classifieur") {
					//	if (key == "Threshold") m_classifThreshold = std::stod(value);
					//}
				}
			}
		}
	}
}


void CChildFrame::SaveConfig(const std::string& filename)
{
	char buffer[MAX_PATH]; // Buffer pour stocker le chemin
	if (GetModuleFileNameA(NULL, buffer, MAX_PATH))
	{
		std::filesystem::path exePath(buffer);
		m_exePath = exePath.parent_path().string() + "\\";
		//std::cout << "Chemin de l'exécutable : " << m_exePath << std::endl;
	}
	else {
		std::cerr << "Erreur : impossible de récupérer le chemin de l'exécutable.\n";
		m_exePath = ".\\";
	}

	std::ofstream file(m_exePath + filename);
	if (!file) {
		std::cerr << "Erreur : impossible d'ouvrir " << filename << " en écriture.\n";
		return;
	}

	file << "[Window]\n";
	file << "Width=" << m_windowWidth << "\n";
	file << "Height=" << m_windowHeight << "\n\n";

	file << "[VTK]\n";
	file << "Grid=" << m_grid_size << "\n";
	file << "Opacity=" << m_opacity << "\n\n";
	file << "Animation=" << m_animate3D << "\n\n";
	file << "DataOnly=" << m_DataOnly << "\n\n";
	file << "NbThread=" << m_NbThread << "\n\n";
	//file << "[Classifieur]\n";
	//file << "Threshold=" << m_classifThreshold << "\n";

	file.close();
}


void CChildFrame::OnConfigurationParametresLog()
{
	if (m_closeCallback != nullptr)
	{
		if (!m_closeCallback->IsWindowClosed())
		{
			MessageBox("Veuillez fermer la fenêtre de rendu 3D"); return;
		}
	}
	CDialConfigGen Dconf(nullptr, m_windowHeight, m_opacity, m_grid_size, m_NbThread, m_animate3D, m_DataOnly);
	if (Dconf.DoModal() == IDOK)
	{
		m_grid_size = Dconf.m_grid;
		m_opacity = Dconf.m_tr;
		m_windowHeight = Dconf.m_win3D;
		m_windowWidth = Dconf.m_win3D;
		m_animate3D = Dconf.m_Animate;
		m_DataOnly = Dconf.m_DataOnly;
		m_NbThread = Dconf.m_NbThread;
		//if (m_animate3D) StartRotation(); else StopRotation();
		SaveConfig("Decid.ini");
	}
}



void CChildFrame::StopRotation()
{
	if (m_render_window_interactor)
	{
		if (m_rotationObserver)
		{
			m_render_window_interactor->RemoveObserver(m_rotationObserver);
			m_rotationObserver = nullptr;
		}
		m_render_window_interactor->DestroyTimer(); // Arrête le timer
	}
}

void CChildFrame::StartRotation()
{
	//if (m_render_window_interactor) m_render_window_interactor->CreateRepeatingTimer(50); // Relance le timer
	if (m_render_window_interactor == nullptr) return;
	if (m_rotationObserver)
	{
		m_render_window_interactor->RemoveObserver(m_rotationObserver);
		m_rotationObserver = nullptr;
		m_render_window_interactor->DestroyTimer();
	}

	m_rotationObserver = vtkSmartPointer<vtkCallbackCommand>::New();
	m_rotationObserver->SetCallback([](vtkObject*, unsigned long, void* clientData, void*)
		{
			auto self = static_cast<CChildFrame*>(clientData);
			self->RotateScene(); // Fonction qui applique la rotation
		});

	m_rotationObserver->SetClientData(this);
	m_render_window_interactor->AddObserver(vtkCommand::TimerEvent, m_rotationObserver);
	m_render_window_interactor->CreateRepeatingTimer(50);
}

void CChildFrame::OnEditionSpirales3d()
{

	CDialGauss DialGauss;
	CString ft;
	if (DialGauss.DoModal() == IDCANCEL) return;
	// Paramètres des distributions
	int numPointsPerClass = DialGauss.m_Sample;
	vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
	vtkSmartPointer<vtkFloatArray> colors = vtkSmartPointer<vtkFloatArray>::New();
	int seed;
	int      tt;
	struct   tm* ti;
	time_t   ltime;
	time(&ltime);
	ti = localtime(&ltime);
	seed = ti->tm_sec;
	std::string NomFichier;
	ft.Format(
		_T("%s_%d_seed_%d_classes_%d_samples.csv"),
		DialGauss.m_Prefix,
		seed,
		DialGauss.m_NbClasses,
		numPointsPerClass
	);
	NomFichier.assign(ft);


	srand(seed);
	for (int k = 0; k < DialGauss.m_NbClasses; k++)
	{

		double cx = ((float)rand()) / RAND_MAX;
		double cy = ((float)rand()) / RAND_MAX;
		double cz = ((float)rand()) / RAND_MAX;
		double sigma = ((float)rand()) / RAND_MAX;
		double phase = (((float)rand()) / RAND_MAX) * 0.001;
		double diam = (((float)rand()) / RAND_MAX) * 0.3 + 0.01;
		double longueur = (((float)rand()) / RAND_MAX) * 0.8 + 0.01;
		double tour = (((float)rand()) / RAND_MAX) * 3 + 1;
		cx = (cx * 0.8) + 0.1;
		cy = (cy * 0.8) + 0.1;
		cz = (cz * 0.8) + 0.1;
		sigma = (sigma * 0.02) + 0.001; // entre 0.01 et 0.06
		GenerateSpiralPoints(points, colors, phase, diam, longueur, (int)tour, sigma, numPointsPerClass, cx, cy, cz, k);
		//GenerateGaussianPoints(points, colors, cx, cy, cz, sigma, numPointsPerClass, k);
	}
	SavePointsToCSV(m_datasPath + NomFichier, points, colors);
	MessageBox("Le fichier " + ft + " a été enregistré");
}


void CChildFrame::OnBoostingopencvUneiteration()
{

	Cleannonmodal(true);
	if (MesDatasMod.m_nbParametres == 0 || MesDatasMod.m_nbClasses == 1) 		OnConfigurationLireauformatcsv();
	if (MesDatasMod.m_allData.empty()) return;

	MesDatasMod.m_trainingLabels.convertTo(m_Label, CV_32SC1);
	Ptr<MultiClassBoost> dTree = MultiClassBoost::create();

	dTree->InitParam(cv::ml::Boost::DISCRETE, 100, 1);
	dTree->TrainBoostIterative(MesDatasMod.m_trainingData, m_Label, MesDatasMod.m_nbClasses, IterBoost);

	m_pClassifier = dTree;
	LastType = mDT;
	if (MesDatasMod.m_nbParametres == 2)
	{
		Invalidate();
	}
	m_Loaded = false;

	IterBoost++;
}


void CChildFrame::OnBoostingopencvReset()
{
	IterBoost = 1;
	m_pClassifier = nullptr;
	Invalidate();
}


void CChildFrame::OnSvmValidationcroisee()
{
	CDialVam Dial;
	int r = Dial.DoModal();
	int kfold = Dial.m_K;
	if (kfold < 2 || kfold>10) kfold = 5;
	if (r == IDOK) CrossValidateSVM(kfold);
}




//void CChildFrame::OnImageAquisitionetvisu3d()
//{
//	StopAcquisition();
//
//	// Démarrer un nouveau thread d'acquisition
//	m_stopAcquisition = false;
//	m_acquisitionThread = std::thread(&CChildFrame::ImageAquisitionetvisu3dThread, this);
//	m_acquisitionThread.detach();
//
//	//////if (VisuCamLive)
//	//////{
//	//////	cv::destroyAllWindows();
//	//////	VisuCamLive = false;
//	//////	MesDatasMod.Clear();
//	//////}
//	//////CDialSegmentationLive Dial;
//	//////if (Dial.DoModal() == IDCANCEL) return;
//
//
//	//////int wf = Dial.m_Wf;        // Taille des fenêtres (wf x wf)
//	//////int NbFen = Dial.m_nbSample;    // Nombre de fenêtres à extraire
//	//////int key = 0;
//	//////int NbImage = 0;
//	//////int par = 0;
//	//////MesDatasMod.InitMem(3, Dial.m_nbCluster, NbFen);
//	//////Mat ImageSeg;
//
//	//////
//
//	//////// Ouvrir la webcam
//	//////cv::VideoCapture cap(0,cv::CAP_DSHOW);  // 0 pour la webcam par défaut
//
//	//////if (!cap.isOpened()) {
//	//////	MessageBox(_T("Erreur: Impossible d'ouvrir la webcam"), _T("Erreur"), MB_ICONERROR);
//	//////	return;
//	//////}
//	//////VisuCamLive = true;
//	//////do
//	//////{
//
//	//////	cv::Mat frame;
//	//////	cap >> frame;
//	//////	frame.copyTo(ImageSeg);
//	//////	if (frame.empty()) {
//	//////		MessageBox(_T("Erreur: Image vide"), _T("Erreur"), MB_ICONERROR);
//	//////		cap.release();
//	//////		return;
//	//////	}
//
//	//////	// Convertir l'image en espace couleur LAB
//	//////	cv::Mat labImage;
//	//////	if (Dial.m_TypeColor == 0) cv::cvtColor(frame, labImage, cv::COLOR_BGR2Lab);
//	//////	if (Dial.m_TypeColor == 1) cv::cvtColor(frame, labImage, cv::COLOR_BGR2HSV);
//	//////	if (Dial.m_TypeColor == 2) cv::cvtColor(frame, labImage, cv::COLOR_BGR2RGB);
//	//////	// Créer une matrice pour stocker les valeurs LAB moyennes
//	//////	// 3 colonnes pour L, a, b et NbFen lignes
//	//////	cv::Mat labValues(NbFen, 3, CV_32F);
//
//	//////	// Générer des fenêtres aléatoires et calculer les valeurs LAB moyennes
//	//////	std::random_device rd;
//	//////	std::mt19937 gen(rd());
//	//////	std::uniform_int_distribution<> distX(0, frame.cols - wf);
//	//////	std::uniform_int_distribution<> distY(0, frame.rows - wf);
//
//	//////	for (int i = 0; i < NbFen; i++) {
//	//////		// Générer des coordonnées aléatoires pour la fenêtre
//	//////		int x = distX(gen);
//	//////		int y = distY(gen);
//
//	//////		// Extraire la fenêtre
//	//////		cv::Mat window = labImage(cv::Rect(x, y, wf, wf));
//
//	//////		// Calculer les valeurs LAB moyennes
//	//////		cv::Scalar meanValues = cv::mean(window);
//
//	//////		// Stocker les valeurs dans la matrice
//	//////		labValues.at<float>(i, 0) = static_cast<float>(meanValues[0])/255.0; // L
//	//////		labValues.at<float>(i, 1) = static_cast<float>(meanValues[1])/255.0; // a
//	//////		labValues.at<float>(i, 2) = static_cast<float>(meanValues[2])/255.0; // b
//	//////	}
//	//////	cv::imshow("Appuyez sur la barre d'espace pour lancer l'apprentissage", frame);
//	//////	cv::namedWindow("Segmentation", cv::WINDOW_NORMAL); // Création d'une fenêtre fixe
//	//////	CWnd* pwnd = FindWindowA(NULL, "Segmentation");
//	//////	if (/*key ==0x20 ||*/ NbImage > 10) // space
//	//////	{
//	//////		// apprentissage clusters
//	//////		Ptr<EM> em = EM::create();
//	//////		em->setClustersNumber(MesDatasMod.m_nbClasses);
//	//////		labValues.copyTo(MesDatasMod.m_allData);
//	//////		em->trainEM(MesDatasMod.m_allData);
//	//////		m_pClassifier = em;
//	//////		LastType = mEM;
//	//////		// labelisation data
//	//////		for (int i = 0; i < MesDatasMod.m_allData.rows; i++)
//	//////		{
//	//////			Mat sampleMat = Mat(1, MesDatasMod.m_nbParametres, CV_32FC1);
//	//////			for (par = 0; par < MesDatasMod.m_nbParametres; par++)
//	//////			{
//	//////				sampleMat.at<float>(0, par) = MesDatasMod.m_allData.at<float>(i, par);
//	//////			}//for par
//	//////			MesDatasMod.m_allLabels.at<float>(i,0) = Decide(em, LastType, sampleMat);
//	//////		}
//	//////		MesDatasMod.CreateDataFromMat();
//	//////		Invalidate();
//	//////		m_DataOnly = true;
//	//////		m_VisuDynamique = true;
//	//////		m_Loaded = true;
//	//////		m_UpdateData = true;
//
//	//////		MSG msg;
//	//////		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
//	//////		{
//	//////				TranslateMessage(&msg);
//	//////				DispatchMessage(&msg);
//	//////		}
//	//////
//
//	//////		Visu3d(m_pClassifier, LastType);
//	//////	}
//	//////	if (NbImage > 100 && MesDatasMod.m_nbParametres==3)
//	//////	{
//	//////		DWORD debut = GetTickCount();
//	//////		ParallelSegmentation(ImageSeg, labImage, wf);
//	//////		DWORD fin = GetTickCount();
//	//////		fin = fin - debut;
//	//////		float tps = (float(fin) / (1000.0));
//	//////		std::ostringstream  titre;
//	//////		titre << "Segmentation EM - temps par image = " << tps << "ms";
//	//////		cv::imshow("Segmentation", ImageSeg);
//	//////		if (pwnd) pwnd->SetWindowTextA(titre.str().c_str());
//	//////	}
//
//	//////
//	//////	key=waitKey(1);
//	//////	NbImage ++;
//	//////} while(key != 0x1B && VisuCamLive);
//
//	//////// Libérer la webcam
//	//////cap.release();
//	//////cv::destroyAllWindows();
//	//////VisuCamLive = false;
//	//////m_UpdateData = false;
//}

void CChildFrame::OnImageAquisitionetvisu3d()
{
	bool istrained = false;
	if (VisuCamLive)
	{
		cv::destroyAllWindows();
		VisuCamLive = false;
		MesDatasMod.Clear();
	}
	CDialSegmentationLive Dial;
	if (Dial.DoModal() == IDCANCEL) return;


	int wf = Dial.m_Wf;        // Taille des fenêtres (wf x wf)
	int NbFen = Dial.m_nbSample;    // Nombre de fenêtres à extraire
	int key = 0;
	int NbImage = 0;
	int par = 0;
	MesDatasMod.InitMem(3, Dial.m_nbCluster, NbFen);
	Mat ImageSeg;

	// Ouvrir la webcam
	cv::VideoCapture cap(0, cv::CAP_DSHOW);  // 0 pour la webcam par défaut

	if (!cap.isOpened()) {
		MessageBox(_T("Erreur: Impossible d'ouvrir la webcam"), _T("Erreur"), MB_ICONERROR);
		return;
	}
	VisuCamLive = true;
	bool oldBool = m_DataOnly;
	do
	{

		cv::Mat frame;
		cap >> frame;
		frame.copyTo(ImageSeg);
		if (frame.empty()) {
			MessageBox(_T("Erreur: Image vide"), _T("Erreur"), MB_ICONERROR);
			cap.release();
			return;
		}

		// Convertir l'image en espace couleur LAB
		cv::Mat labImage;
		if (Dial.m_TypeColor == 0) cv::cvtColor(frame, labImage, cv::COLOR_BGR2Lab);
		if (Dial.m_TypeColor == 1) cv::cvtColor(frame, labImage, cv::COLOR_BGR2HSV);
		if (Dial.m_TypeColor == 2) cv::cvtColor(frame, labImage, cv::COLOR_BGR2RGB);
		if (Dial.m_TypeColor == 3) cv::cvtColor(frame, labImage, cv::COLOR_BGR2YCrCb);
		// Créer une matrice pour stocker les valeurs LAB moyennes
		// 3 colonnes pour L, a, b et NbFen lignes
		cv::Mat labValues(NbFen, 3, CV_32F);

		// Générer des fenêtres aléatoires et calculer les valeurs LAB moyennes
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> distX(0, frame.cols - wf);
		std::uniform_int_distribution<> distY(0, frame.rows - wf);

		for (int i = 0; i < NbFen; i++) {
			// Générer des coordonnées aléatoires pour la fenêtre
			int x = distX(gen);
			int y = distY(gen);
			if (wf > 1)
			{
				// Extraire la fenêtre
				cv::Mat window = labImage(cv::Rect(x, y, wf, wf));
				// Calculer les valeurs LAB moyennes
				cv::Scalar meanValues = cv::mean(window);
				// Stocker les valeurs dans la matrice
				labValues.at<float>(i, 0) = static_cast<float>(meanValues[0]) / 255.0; // L
				labValues.at<float>(i, 1) = static_cast<float>(meanValues[1]) / 255.0; // a
				labValues.at<float>(i, 2) = static_cast<float>(meanValues[2]) / 255.0; // b
			}
			else // wf = 1
			{
				// Extraire la fenêtre
				cv::Vec3b pixel = labImage.at<cv::Vec3b>(y, x);
				// Stocker les valeurs dans la matrice
				labValues.at<float>(i, 0) = static_cast<float>(pixel[0]) / 255.0; // L
				labValues.at<float>(i, 1) = static_cast<float>(pixel[1]) / 255.0; // a
				labValues.at<float>(i, 2) = static_cast<float>(pixel[2]) / 255.0; // b
			}

		}

		cv::imshow("Appuyez sur la barre d'espace pour lancer l'apprentissage", frame);
		cv::namedWindow("Segmentation", cv::WINDOW_NORMAL); // Création d'une fenêtre fixe
		CWnd* pwnd = FindWindowA(NULL, "Segmentation");
		if (key == 0x20) // space
		{
			// apprentissage clusters
			Ptr<EM> em = EM::create();
			em->setClustersNumber(MesDatasMod.m_nbClasses);
			labValues.copyTo(MesDatasMod.m_allData);
			em->trainEM(MesDatasMod.m_allData);
			istrained = true;
			m_pClassifier = em;
			LastType = mEM;
			// labelisation data
			for (int i = 0; i < MesDatasMod.m_allData.rows; i++)
			{
				Mat sampleMat = Mat(1, MesDatasMod.m_nbParametres, CV_32FC1);
				for (par = 0; par < MesDatasMod.m_nbParametres; par++)
				{
					sampleMat.at<float>(0, par) = MesDatasMod.m_allData.at<float>(i, par);
				}//for par
				MesDatasMod.m_allLabels.at<float>(i, 0) = Decide(em, LastType, sampleMat);
			}
			MesDatasMod.CreateDataFromMat();
			Invalidate();
			m_DataOnly = Dial.m_DataOnly;
			m_VisuDynamique = true;
			m_Loaded = true;
			m_UpdateData = true;

			Visu3d(m_pClassifier, LastType);
		}
		if (NbImage > 10 && MesDatasMod.m_nbParametres == 3 && istrained)
		{
			DWORD debut = GetTickCount();
			ParallelSegmentation(ImageSeg, labImage, wf, 0);
			DWORD fin = GetTickCount();
			fin = fin - debut;
			float tps = (float(fin));
			std::ostringstream  titre;
			titre << "Segmentation EM - temps par image = " << tps << "ms";
			cv::imshow("Segmentation", ImageSeg);
			if (pwnd) pwnd->SetWindowTextA(titre.str().c_str());
			if (istrained)
			{
				// classer les data pour les afficher toujours avec la même classe
				labValues.copyTo(MesDatasMod.m_allData);
				for (int i = 0; i < MesDatasMod.m_allData.rows; i++)
				{
					Mat sampleMat = Mat(1, MesDatasMod.m_nbParametres, CV_32FC1);
					for (par = 0; par < MesDatasMod.m_nbParametres; par++)
					{
						sampleMat.at<float>(0, par) = MesDatasMod.m_allData.at<float>(i, par);
					}//for par
					MesDatasMod.m_allLabels.at<float>(i, 0) = Decide(m_pClassifier, LastType, sampleMat);
				}
				MesDatasMod.CreateDataFromMat(); // split training / testing et prepare visu
				Invalidate();
				Visu3d(m_pClassifier, LastType);
			}

		}


		key = waitKey(1);
		NbImage++;
	} while (key != 0x1B && VisuCamLive);

	// Libérer la webcam
	cap.release();
	cv::destroyAllWindows();
	VisuCamLive = false;
	m_UpdateData = false;
	m_DataOnly = oldBool;
}



void CChildFrame::ProcessImageSegment(Mat& ImageSeg, const Mat& labImage, int wf, int startRow, int endRow, std::mutex& imgMutex)
{
	Mat sampleMat = Mat(1, MesDatasMod.m_nbParametres, CV_32FC1);
	for (int l = startRow; l < endRow; l++)
	{
		for (int c = 0; c < ImageSeg.cols - wf; c++)
		{
			if (wf > 1)
			{
				Mat window = labImage(Rect(c, l, wf, wf));
				// Calculer les valeurs LAB moyennes
				cv::Scalar meanValues = mean(window);
				sampleMat.at<float>(0, 0) = static_cast<float>(meanValues[0]) / 255.0; // L
				sampleMat.at<float>(0, 1) = static_cast<float>(meanValues[1]) / 255.0; // a
				sampleMat.at<float>(0, 2) = static_cast<float>(meanValues[2]) / 255.0; // b
			}
			else
			{
				Vec3b pixel = labImage.at<Vec3b>(l, c);
				sampleMat.at<float>(0, 0) = static_cast<float>(pixel[0]) / 255.0; // L
				sampleMat.at<float>(0, 1) = static_cast<float>(pixel[1]) / 255.0; // a
				sampleMat.at<float>(0, 2) = static_cast<float>(pixel[2]) / 255.0; // b
			}
			int classId = (int)Decide(m_pClassifier, LastType, sampleMat);
			Vec3b color = (classId >= 0 && classId < colorPalette.size()) ?
				colorPalette[classId] : Vec3b(128, 128, 128); // Gris par défaut

			// Protection de l'accès à ImageSeg
			lock_guard<mutex> lock(imgMutex);
			ImageSeg.at<Vec3b>(l, c) = color;
		}
	}
}

//void CChildFrame::ProcessImageSegmentFourier(Mat& ImageSeg, const Mat& labImage, int wf, int startRow, int endRow, std::mutex& imgMutex)
//{
//	//CIma ima(wf);
//	Mat sampleMat = Mat(1, MesDatasMod.m_nbParametres, CV_32FC1);
//	for (int l = startRow; l < endRow; l += wf / 2)
//	{
//		for (int c = 0; c < ImageSeg.cols - wf; c += wf / 2)
//		{
//			Mat window = labImage(Rect(c, l, wf, wf));
//			double* DesFour;
//			DesFour = m_pima->DescripteursFourier1Gray(window);
//			// Stocker les valeurs dans la matrice
//			sampleMat.at<float>(0, 0) = static_cast<float>(DesFour[1]); // L
//			sampleMat.at<float>(0, 1) = static_cast<float>(DesFour[7]); // a
//			sampleMat.at<float>(0, 2) = static_cast<float>(DesFour[9]); // b
//			free(DesFour);
//			int classId = (int)Decide(m_pClassifier, LastType, sampleMat);
//			Vec3b color = (classId >= 0 && classId < colorPalette.size()) ?
//				colorPalette[classId] : Vec3b(128, 128, 128); // Gris par défaut
//
//			// Protection de l'accès à ImageSeg
//			lock_guard<mutex> lock(imgMutex);
//			for (int ll = l; ll < l + wf / 2; ll++)
//			{
//				for (int cc = c; cc < c + wf / 2; cc++)		ImageSeg.at<Vec3b>(ll, cc) = color;
//			}
//		}
//	}
//}

void CChildFrame::ParallelSegmentation(Mat& ImageSeg, const Mat& labImage, int wf, int Typeprocess = 0)
{

	vector<thread> threads;
	mutex imgMutex;
	int rowsPerThread = (ImageSeg.rows - wf) / m_NbThread;

	for (int i = 0; i < m_NbThread; i++) {
		int startRow = i * rowsPerThread;
		int endRow = (i == m_NbThread - 1) ? (ImageSeg.rows - wf) : startRow + rowsPerThread;
		if (Typeprocess == 0) threads.emplace_back(&CChildFrame::ProcessImageSegment, this, ref(ImageSeg), cref(labImage), wf, startRow, endRow, ref(imgMutex));
	//	if (Typeprocess == 1) threads.emplace_back(&CChildFrame::ProcessImageSegmentFourier, this, ref(ImageSeg), cref(labImage), wf, startRow, endRow, ref(imgMutex));

	}
	// Attendre la fin de tous les threads
	for (auto& th : threads) {
		th.join();
	}
}


void CChildFrame::OnImageSegmentationlab()
{
	if (VisuCamLive)
	{
		cv::destroyAllWindows();
		VisuCamLive = false;
	}
	CDialSegmentationLive Dial;
	if (Dial.DoModal() == IDCANCEL) return;
	int wf = Dial.m_Wf;        // Taille des fenêtres (wf x wf)

	int key = 0;
	int NbImage = 0;
	int par = 0;
	Mat ImageSeg;
	//	if (m_pClassifier == nullptr || MesDatasMod.m_nbParametres != 3)
	{
		if (!LoadClassifier()) return;
		if (MesDatasMod.m_nbParametres != 3)
		{
			MessageBox("Le nombre d'attributs du classifieur est différent de 3"); return;
		}
	}
	string TitreClassifieur;

	// Ouvrir la webcam
	cv::VideoCapture cap(0, cv::CAP_DSHOW);  // 0 pour la webcam par défaut

	if (!cap.isOpened()) {
		MessageBox(_T("Erreur: Impossible d'ouvrir la webcam"), _T("Erreur"), MB_ICONERROR);
		return;
	}
	VisuCamLive = true;
	cv::namedWindow("Segmentation", cv::WINDOW_NORMAL); // Création d'une fenêtre fixe
	CWnd* pwnd = FindWindowA(NULL, "Segmentation");
	do
	{
		switch (LastType) {
		case mSVM: { TitreClassifieur = "Segmentation SVM"; break; }
		case mKPPV: { TitreClassifieur = "Segmentation KPPV";	break; }
		case mRF: { TitreClassifieur = "Segmentation Random Forest";	break; }
		case mMLP: { TitreClassifieur = "Segmentation MLP";	break; }
		case mNB: { TitreClassifieur = "Segmentation Naive Bayes"; break; }
		case mEM: { TitreClassifieur = "Segmentation EM";	break; }
		case mDT: { TitreClassifieur = "Segmentation Boosting";		break; }
		default:TitreClassifieur = "Segmentation";	break;
		}

		cv::Mat frame;
		cap >> frame;
		frame.copyTo(ImageSeg);
		if (frame.empty()) {
			MessageBox(_T("Erreur: Image vide"), _T("Erreur"), MB_ICONERROR);
			cap.release();
			return;
		}

		// Convertir l'image en espace couleur LAB
		cv::Mat labImage;
		if (Dial.m_TypeColor == 0) cv::cvtColor(frame, labImage, cv::COLOR_BGR2Lab);
		if (Dial.m_TypeColor == 1) cv::cvtColor(frame, labImage, cv::COLOR_BGR2HSV);
		if (Dial.m_TypeColor == 2) cv::cvtColor(frame, labImage, cv::COLOR_BGR2RGB);
		if (Dial.m_TypeColor == 3) cv::cvtColor(frame, labImage, cv::COLOR_BGR2YCrCb);
		cv::imshow("Webcam", frame);

		if (m_pClassifier != nullptr)
		{
			DWORD debut = GetTickCount();
			ParallelSegmentation(ImageSeg, labImage, wf, 0);
			DWORD fin = GetTickCount();
			fin = fin - debut;
			float tps = (float(fin));
			std::ostringstream  titre;
			titre << TitreClassifieur << " temps par image = " << tps << "ms";
			cv::imshow("Segmentation", ImageSeg);
			if (pwnd) pwnd->SetWindowTextA(titre.str().c_str());
		}

		key = waitKey(1);
		NbImage++;
	} while (key != 0x1B && VisuCamLive);

	// Libérer la webcam
	cap.release();
	cv::destroyAllWindows();
	VisuCamLive = false;
}

void CChildFrame::OnImageApprentissageviaimagefixe()
{
	bool istrained = false;
	if (VisuCamLive)
	{
		cv::destroyAllWindows();
		VisuCamLive = false;
		MesDatasMod.Clear();
	}

	// Dialogue pour les paramètres de segmentation
	CDialSegmentationLive Dial;
	if (Dial.DoModal() == IDCANCEL) return;

	// Dialogue pour sélectionner l'image
	CFileDialog dlgFile(TRUE, _T("jpg"), NULL, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
		_T("Images (*.jpg;*.jpeg;*.png;*.bmp)|*.jpg;*.jpeg;*.png;*.bmp|Tous les fichiers (*.*)|*.*||"), NULL);

	if (dlgFile.DoModal() == IDCANCEL) return;

	// Récupérer le chemin de l'image
	CString filePath = dlgFile.GetPathName();

	// Charger l'image
	cv::Mat frame = cv::imread(LPCTSTR(filePath));
	if (frame.empty()) {
		MessageBox(_T("Erreur: Impossible de charger l'image"), _T("Erreur"), MB_ICONERROR);
		return;
	}

	// Création du nom de fichier pour sauvegarder le modèle
	CString saveFilePath = dlgFile.GetPathName();
	saveFilePath.Replace(dlgFile.GetFileExt(), _T("cvEM"));

	int wf = Dial.m_Wf;        // Taille des fenêtres (wf x wf)
	int NbFen = Dial.m_nbSample;    // Nombre de fenêtres à extraire
	int par = 0;
	MesDatasMod.InitMem(3, Dial.m_nbCluster, NbFen);
	Mat ImageSeg;
	frame.copyTo(ImageSeg);

	// Convertir l'image en espace couleur approprié
	cv::Mat labImage;
	if (Dial.m_TypeColor == 0) cv::cvtColor(frame, labImage, cv::COLOR_BGR2Lab);
	if (Dial.m_TypeColor == 1) cv::cvtColor(frame, labImage, cv::COLOR_BGR2HSV);
	if (Dial.m_TypeColor == 2) cv::cvtColor(frame, labImage, cv::COLOR_BGR2RGB);
	if (Dial.m_TypeColor == 3) cv::cvtColor(frame, labImage, cv::COLOR_BGR2YCrCb);

	// Créer une matrice pour stocker les valeurs couleur moyennes
	cv::Mat labValues(NbFen, 3, CV_32F);

	// Générer des fenêtres aléatoires et calculer les valeurs moyennes
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> distX(0, frame.cols - wf);
	std::uniform_int_distribution<> distY(0, frame.rows - wf);

	for (int i = 0; i < NbFen; i++) {
		// Générer des coordonnées aléatoires pour la fenêtre
		int x = distX(gen);
		int y = distY(gen);
		// Extraire la fenêtre
		cv::Mat window = labImage(cv::Rect(x, y, wf, wf));
		// Calculer les valeurs moyennes
		cv::Scalar meanValues = cv::mean(window);
		// Stocker les valeurs dans la matrice
		labValues.at<float>(i, 0) = static_cast<float>(meanValues[0]) / 255.0;
		labValues.at<float>(i, 1) = static_cast<float>(meanValues[1]) / 255.0;
		labValues.at<float>(i, 2) = static_cast<float>(meanValues[2]) / 255.0;
	}

	// Afficher l'image originale
	cv::imshow("Image originale", frame);
	cv::namedWindow("Segmentation", cv::WINDOW_NORMAL);
	CWnd* pwnd = FindWindowA(NULL, "Segmentation");

	// Apprentissage des clusters
	Ptr<EM> em = EM::create();
	em->setClustersNumber(MesDatasMod.m_nbClasses);
	labValues.copyTo(MesDatasMod.m_allData);
	em->trainEM(MesDatasMod.m_allData);
	istrained = true;
	m_pClassifier = em;
	LastType = mEM;

	// Labelisation des données
	for (int i = 0; i < MesDatasMod.m_allData.rows; i++)
	{
		Mat sampleMat = Mat(1, MesDatasMod.m_nbParametres, CV_32FC1);
		for (par = 0; par < MesDatasMod.m_nbParametres; par++)
		{
			sampleMat.at<float>(0, par) = MesDatasMod.m_allData.at<float>(i, par);
		}
		MesDatasMod.m_allLabels.at<float>(i, 0) = Decide(em, LastType, sampleMat);
	}

	MesDatasMod.CreateDataFromMat();
	Invalidate();

	bool oldBool = m_DataOnly;
	m_DataOnly = Dial.m_DataOnly;
	m_VisuDynamique = true;
	m_Loaded = true;
	m_UpdateData = true;

	Visu3d(m_pClassifier, LastType);

	// Segmentation de l'image
	if (MesDatasMod.m_nbParametres == 3)
	{
		DWORD debut = GetTickCount();
		ParallelSegmentation(ImageSeg, labImage, wf, 0);
		DWORD fin = GetTickCount();
		fin = fin - debut;
		float tps = (float(fin) / (1000.0));
		std::ostringstream titre;
		titre << "Segmentation EM - temps par image = " << tps << "ms";
		cv::imshow("Segmentation", ImageSeg);
		if (pwnd) pwnd->SetWindowTextA(titre.str().c_str());
	}
	CString colorSpaceSuffix;
	switch (Dial.m_TypeColor) {
	case 0: colorSpaceSuffix = _T("_segmentee_LAB"); break;
	case 1: colorSpaceSuffix = _T("_segmentee_HSV"); break;
	case 2: colorSpaceSuffix = _T("_segmentee_RGB"); break;
	case 3: colorSpaceSuffix = _T("_segmentee_YCrCb"); break;
	default: colorSpaceSuffix = _T("_segmentee"); break;
	}

	// Créer le nom du fichier de sauvegarde pour l'image segmentée
	CString saveImagePath = dlgFile.GetPathName();
	saveImagePath = saveImagePath.Left(saveImagePath.ReverseFind('.')) + colorSpaceSuffix + _T(".png");

	// Sauvegarder l'image segmentée
	try {
		cv::imwrite(LPCTSTR(saveImagePath), ImageSeg);
		CString message;
		message.Format(_T("Image segmentée sauvegardée avec succès dans %s"), saveImagePath);
		MessageBox(message, _T("Sauvegarde réussie"), MB_ICONINFORMATION);
	}
	catch (const cv::Exception& e) {
		CString errorMsg;
		errorMsg.Format(_T("Erreur lors de la sauvegarde de l'image segmentée: %s"), CString(e.what()));
		MessageBox(errorMsg, _T("Erreur"), MB_ICONERROR);
	}
	// Sauvegarder le modèle
	try {
		FileStorage fs(LPCTSTR(saveFilePath), FileStorage::WRITE);
		if (fs.isOpened()) {
			em->write(fs);
			fs.release();
			CString message;
			message.Format(_T("Modèle sauvegardé avec succès dans %s"), saveFilePath);
			MessageBox(message, _T("Sauvegarde réussie"), MB_ICONINFORMATION);
		}
		else {
			MessageBox(_T("Erreur lors de l'ouverture du fichier pour la sauvegarde"), _T("Erreur"), MB_ICONERROR);
		}
	}
	catch (const cv::Exception& e) {
		CString errorMsg;
		errorMsg.Format(_T("Erreur lors de la sauvegarde du modèle: %s"), CString(e.what()));
		MessageBox(errorMsg, _T("Erreur"), MB_ICONERROR);
	}

	// Attendre que l'utilisateur ferme les fenêtres
	cv::waitKey(0);
	cv::destroyAllWindows();

	m_UpdateData = false;
	m_DataOnly = oldBool;
}


//void CChildFrame::OnImageApprentissagedescripteursdefourier()
//{
//	bool istrained = false;
//	if (VisuCamLive)
//	{
//		cv::destroyAllWindows();
//		VisuCamLive = false;
//		MesDatasMod.Clear();
//	}
//	CDialSegmentationLive Dial;
//	if (Dial.DoModal() == IDCANCEL) return;
//
//
//	int wf = 32;// Dial.m_Wf;        // Taille des fenêtres (wf x wf)
//	int NbFen = Dial.m_nbSample;    // Nombre de fenêtres à extraire
//	int key = 0;
//	int NbImage = 0;
//	int par = 0;
//	MesDatasMod.InitMem(3, Dial.m_nbCluster, NbFen);
//	Mat ImageSeg;
//
//	// Ouvrir la webcam
//	cv::VideoCapture cap(0, cv::CAP_DSHOW);  // 0 pour la webcam par défaut
//
//	if (!cap.isOpened()) {
//		MessageBox(_T("Erreur: Impossible d'ouvrir la webcam"), _T("Erreur"), MB_ICONERROR);
//		return;
//	}
//	VisuCamLive = true;
//	bool oldBool = m_DataOnly;
//	do
//	{
//
//		cv::Mat frame;
//		cap >> frame;
//		frame.copyTo(ImageSeg);
//		if (frame.empty()) {
//			MessageBox(_T("Erreur: Image vide"), _T("Erreur"), MB_ICONERROR);
//			cap.release();
//			return;
//		}
//
//
//		cv::Mat labImage;
//
//		// Créer une matrice pour stocker les valeurs LAB moyennes
//		// 3 colonnes pour L, a, b et NbFen lignes
//		cv::Mat FourierValue(NbFen, 3, CV_32F);
//
//		// Générer des fenêtres aléatoires et calculer les valeurs LAB moyennes
//		std::random_device rd;
//		std::mt19937 gen(rd());
//		std::uniform_int_distribution<> distX(0, frame.cols - wf);
//		std::uniform_int_distribution<> distY(0, frame.rows - wf);
//		//CIma ima(wf);
//		for (int i = 0; i < NbFen; i++) {
//			// Générer des coordonnées aléatoires pour la fenêtre
//			int x = distX(gen);
//			int y = distY(gen);
//			// Extraire la fenêtre
//			cv::Mat window = frame(cv::Rect(x, y, wf, wf));
//			// Calculer les descripteurs de Fourier dans cette fenetre
//			double* DesFour;
//			DesFour = m_pima->DescripteursFourier1Gray(window);
//			// Stocker les valeurs dans la matrice
//			FourierValue.at<float>(i, 0) = static_cast<float>(DesFour[1]);
//			FourierValue.at<float>(i, 1) = static_cast<float>(DesFour[7]);
//			FourierValue.at<float>(i, 2) = static_cast<float>(DesFour[9]);
//
//			free(DesFour);
//		}
//
//		cv::imshow("Appuyez sur la barre d'espace pour lancer l'apprentissage", frame);
//		cv::namedWindow("Segmentation", cv::WINDOW_NORMAL); // Création d'une fenêtre fixe
//		CWnd* pwnd = FindWindowA(NULL, "Segmentation");
//		if (key == 0x20) // space
//		{
//			// apprentissage clusters
//			Ptr<EM> em = EM::create();
//			em->setClustersNumber(MesDatasMod.m_nbClasses);
//			FourierValue.copyTo(MesDatasMod.m_allData);
//			em->trainEM(MesDatasMod.m_allData);
//			istrained = true;
//			m_pClassifier = em;
//			LastType = mEM;
//			// labelisation data
//			for (int i = 0; i < MesDatasMod.m_allData.rows; i++)
//			{
//				Mat sampleMat = Mat(1, MesDatasMod.m_nbParametres, CV_32FC1);
//				for (par = 0; par < MesDatasMod.m_nbParametres; par++)
//				{
//					sampleMat.at<float>(0, par) = MesDatasMod.m_allData.at<float>(i, par);
//				}//for par
//				MesDatasMod.m_allLabels.at<float>(i, 0) = Decide(em, LastType, sampleMat);
//			}
//			MesDatasMod.CreateDataFromMat();
//			Invalidate();
//			m_DataOnly = Dial.m_DataOnly;
//			m_VisuDynamique = true;
//			m_Loaded = true;
//			m_UpdateData = true;
//
//			Visu3d(m_pClassifier, LastType);
//		}
//		if (NbImage > 10 && MesDatasMod.m_nbParametres == 3 && istrained)
//		{
//			DWORD debut = GetTickCount();
//			ParallelSegmentation(ImageSeg, frame, wf, 1);
//			DWORD fin = GetTickCount();
//			fin = fin - debut;
//			float tps = (float(fin));
//			std::ostringstream  titre;
//			titre << "Segmentation EM - temps par image = " << tps << "ms";
//			cv::imshow("Segmentation", ImageSeg);
//			if (pwnd) pwnd->SetWindowTextA(titre.str().c_str());
//			if (istrained)
//			{
//				// classer les data pour les afficher toujours avec la même classe
//				FourierValue.copyTo(MesDatasMod.m_allData);
//				for (int i = 0; i < MesDatasMod.m_allData.rows; i++)
//				{
//					Mat sampleMat = Mat(1, MesDatasMod.m_nbParametres, CV_32FC1);
//					for (par = 0; par < MesDatasMod.m_nbParametres; par++)
//					{
//						sampleMat.at<float>(0, par) = MesDatasMod.m_allData.at<float>(i, par);
//					}//for par
//					MesDatasMod.m_allLabels.at<float>(i, 0) = Decide(m_pClassifier, LastType, sampleMat);
//				}
//				MesDatasMod.CreateDataFromMat(); // split training / testing et prepare visu
//				Invalidate();
//				Visu3d(m_pClassifier, LastType);
//			}
//
//		}
//
//
//		key = waitKey(1);
//		NbImage++;
//	} while (key != 0x1B && VisuCamLive);
//
//	// Libérer la webcam
//	cap.release();
//	cv::destroyAllWindows();
//	VisuCamLive = false;
//	m_UpdateData = false;
//	m_DataOnly = oldBool;
//} // fin 
//
//
//void CChildFrame::OnImageApprentissagedescripteursdefourierfixe()
//{
//	CFileDialog dlg(TRUE, NULL, NULL, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY, _T("Images (*.bmp;*.jpg;*.png)|*.bmp;*.jpg;*.png|All Files (*.*)|*.*||"));
//	dlg.m_ofn.lpstrTitle = _T("Choisir un dossier contenant des imagettes de textures");
//	int wf = 128;
//	int Wwf = 1024;
//	Mat ImageLarge;
//	// effacer les données précédentes
//	MesDatasMod.Clear();
//	MesDatasMod.m_nbClasses = 0;
//
//	if (dlg.DoModal() == IDOK)
//	{
//		CString folderPath = dlg.GetPathName();
//		std::filesystem::path path(folderPath.GetString());
//		if (!std::filesystem::is_directory(path))
//		{
//			path = path.parent_path();
//		}
//		// ouvre un fichier qui contiendra les noms de fichiers, sans le path
//		std::ofstream file;
//		file.open(path.string() + "\\ClassesNames.txt");
//
//		// Parcourir les fichiers du dossier
//		for (const auto& entry : std::filesystem::directory_iterator(path))
//		{
//			if (entry.is_regular_file())
//			{
//				std::string filePath = entry.path().string();
//				cv::Mat image = cv::imread(filePath);
//				if (!image.empty())
//				{
//					// ecrit le nom du fichier sans le path ni son extension dans file
//					std::filesystem::path path(filePath);
//					std::string fileName = path.stem().string();
//					file << fileName << std::endl;
//
//					cv::resize(image, ImageLarge, cv::Size(Wwf, Wwf));
//
//					for (int l = 0; l < ImageLarge.rows - wf; l += wf / 2)
//					{
//						for (int c = 0; c < ImageLarge.cols - wf; c += wf / 2)
//						{
//							Mat window = ImageLarge(Rect(c, l, wf, wf));
//							double* DesFour;
//							DesFour = m_pima->DescripteursFourier1Gray(window);
//							cv::Mat descripteurs(1, m_pima->m_Npar, CV_32F);
//							for (int i = 0; i < m_pima->m_Npar; i++)
//							{
//								descripteurs.at<float>(0, i) = static_cast<float>(DesFour[i]);
//							}
//							free(DesFour);
//							MesDatasMod.m_allData.push_back(descripteurs);
//							MesDatasMod.m_allLabels.push_back((float)MesDatasMod.m_nbClasses);
//						}
//					}
//					MesDatasMod.m_nbClasses++;
//				} // si image ok
//			} // si fichier
//		} // pour toutes les imagettes do dossier
//		//
//		file.close();
//		MesDatasMod.m_nbParametres = m_pima->m_Npar;
//		MesDatasMod.CreateDataFromMat(); // split training / testing et prepare visu
//		Invalidate();
//	} // si ok
//
//
//	CFolderPickerDialog folderPickerDialog(NULL, OFN_FILEMUSTEXIST, this, 0);
//	if (folderPickerDialog.DoModal() != IDOK) return;
//
//	CString folderPath = folderPickerDialog.GetPathName();
//	std::filesystem::path rootPath(folderPath.GetString());
//
//	std::ofstream trainFile("train_selected_texture.txt");
//	std::ofstream testFile("test_selected_texture.txt");
//
//	if (!trainFile.is_open() || !testFile.is_open()) {
//		MessageBox("Erreur d'ouverture des fichiers de sortie", "Erreur", MB_OK | MB_ICONERROR);
//		return;
//	}
//
//	for (const auto& entry : std::filesystem::directory_iterator(rootPath))
//	{
//		if (entry.is_directory()) {
//			std::string className = entry.path().filename().string();
//			std::vector<std::string> fileNames;
//
//			for (const auto& fileEntry : std::filesystem::directory_iterator(entry.path())) {
//				if (fileEntry.is_regular_file()) {
//					fileNames.push_back(fileEntry.path().filename().string());
//				}
//			}
//
//			std::shuffle(fileNames.begin(), fileNames.end(), std::mt19937{ std::random_device{}() });
//			size_t halfSize = fileNames.size() / 2;
//			for (size_t i = 0; i < fileNames.size(); ++i) {
//				std::string relativePath = className + "/" + fileNames[i];
//				if (i < halfSize) {
//					trainFile << relativePath << std::endl;
//				}
//				else {
//					testFile << relativePath << std::endl;
//				}
//			}
//		}
//	}
//
//	trainFile.close();
//	testFile.close();
//
//
//} // fin OnImageApprentissagedescripteursdefourierfixe
//
//void CChildFrame::OnImageClasseuneimageavecdesc()
//{
//	// TODO: ajoutez ici le code de votre gestionnaire de commande
//	// en te basant sur la fonction OnImageApprentissagedescripteursdefourierfixe, ouvre une image etc un classifieur et la liste des classes et affiche comme classe le nom du fichier
//	CFileDialog dlg(TRUE, NULL, NULL, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY, _T("Images (*.bmp;*.jpg;*.png)|*.bmp;*.jpg;*.png|All Files (*.*)|*.*||"));
//	dlg.m_ofn.lpstrTitle = _T("Choisir une image à classer");
//
//	CFileDialog dlgtxt(TRUE, NULL, NULL, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY, _T("Images (*.txt)|*.txt"));
//	dlgtxt.m_ofn.lpstrTitle = _T("Choisir les noms des classes");
//
//
//	LoadClassifier();
//
//	int wf = 128;
//	int Wwf = 1024;
//	Mat ImageLarge;
//	// effacer les données précédentes
//	MesDatasMod.Clear();
//	MesDatasMod.m_nbClasses = 0;
//	// ouvre le fichier qui contiendra les noms de fichiers à partir d'une CFileDialog
//
//	std::ofstream file;
//	// choisit le fichier image
//	if (dlg.DoModal() == IDOK)
//	{
//
//		CString ImageName = dlg.GetPathName();
//		std::filesystem::path path(ImageName.GetString());
//		if (!std::filesystem::is_directory(path))
//		{
//			path = path.parent_path();
//		}
//
//		// choisir la liste des noms
//		if (dlgtxt.DoModal() != IDOK) return; 
//		CString ImageListe = dlgtxt.GetPathName();
//		// ouvre en lecture un fichier qui contiendra les noms de fichiers
//
//		file.open(ImageListe,ios::in);
//		// lit les noms des classes et les stocke dans un vecteur de string
//		std::vector<std::string> classesNames;
//		std::ifstream filetxt(dlgtxt.GetPathName());
//		std::string line;
//		while (std::getline(filetxt, line))
//		{
//			classesNames.push_back(line);
//		}
//		file.close();
//		if (classesNames.size() == 0)
//		{
//			MessageBox(_T("Pas de classes trouvées"), _T("Erreur"), MB_ICONERROR);
//			return;
//		}
//		// charge l'image
//		MesDatasMod.m_nbClasses = classesNames.size();
//
//		cv::Mat image = cv::imread((LPCTSTR)ImageName);
//		cv::resize(image, ImageLarge, cv::Size(Wwf, Wwf));
//		Mat sampletMat = Mat(1, m_pima->m_Npar, CV_32FC1);
//		vector<int> classes;
//		// remplir classes de 0
//		for (int i = 0; i < MesDatasMod.m_nbClasses; i++) classes.push_back(0);
//		for (int l = 0; l < ImageLarge.rows - wf; l += wf / 2)
//		{
//			for (int c = 0; c < ImageLarge.cols - wf; c += wf / 2)
//			{
//				Mat window = ImageLarge(Rect(c, l, wf, wf));
//				double* DesFour;
//				DesFour = m_pima->DescripteursFourier1Gray(window);
//				for (int i = 0; i < m_pima->m_Npar; i++)
//				{
//					sampletMat.at<float>(0, i) = static_cast<float>(DesFour[i]);
//				}
//				free(DesFour);
//				int classId = (int)Decide(m_pClassifier, LastType, sampletMat);
//				classes[classId]++;
//			}
//		}
//		int classeFinale = 0;
//		// classeFinale est la classe qui a le plus de votes
//		for (int i = 0; i < MesDatasMod.m_nbClasses; i++)
//		{
//			if (classes[i] > classes[classeFinale]) classeFinale = i;
//		}
//		// affiche le nom de la classe
//		if (classeFinale < classesNames.size()) MessageBox(_T("Classe de l'image : ") + CString(classesNames[classeFinale].c_str()), _T("Classe"), MB_ICONINFORMATION);
//		else
//		{
//			MessageBox(_T("Classe de l'image : ") + CString("Classe inconnue"), _T("Classe"), MB_ICONINFORMATION);
//		}
//	}
//}
//
//void CChildFrame::OnImageApprendunelistedimages()
//{
//	CFileDialog FileData(TRUE, "*.txt", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Text Files (*.txt)|*.txt||", this);
//	if (FileData.DoModal() != IDOK) return;
//
//	CString filePath = FileData.GetPathName();
//	std::ifstream file(filePath);
//	if (!file.is_open()) {
//		MessageBox("Erreur d'ouverture du fichier", "Erreur", MB_OK | MB_ICONERROR);
//		return;
//	}
//	std::filesystem::path path(filePath.GetString());
//	if (!std::filesystem::is_directory(path))
//	{
//		path = path.parent_path();
//	}
//	std::map<std::string, std::vector<std::string>> classToFileMap;
//	std::string line;
//	while (std::getline(file, line)) {
//		size_t pos = line.find('/');
//		if (pos != std::string::npos) {
//			std::string className = line.substr(0, pos);
//			classToFileMap[className].push_back(line);
//		}
//	}
//	file.close();
//
//	MesDatasMod.Clear();
//	MesDatasMod.m_nbClasses = classToFileMap.size();
//	MesDatasMod.m_nbParametres = 0; // This will be updated later
//	int nc = 0;
//	for (const auto& classEntry : classToFileMap) 
//	{
//		const std::string& className = classEntry.first;
//		const std::vector<std::string>& fileNames = classEntry.second;
//
//		for (const std::string& fileName : fileNames)
//		{
//			string nomf = path.string() + "/../images/" + fileName;
//			Mat image = imread(nomf);
//			if (image.empty()) {
//				CString msg;
//				msg.Format("Erreur de lecture de l'image : %s", fileName.c_str());
//				MessageBox(msg, "Erreur", MB_OK | MB_ICONERROR);
//				continue;
//			}
//
//			// Generate Fourier descriptors for the image
//			cvtColor(image, image, cv::COLOR_BGR2GRAY);
//			GenerateFourierDescriptors(image,nc);
//
//			// Update the number of parameters if necessary
//			if (MesDatasMod.m_nbParametres == 0) {
//				MesDatasMod.m_nbParametres = MesDatasMod.m_allData.cols;
//			}
//		}
//	//	MessageBox("classe : " + CString(className.c_str()), "Info", MB_OK | MB_ICONINFORMATION);
//		nc++;
//	}
//
//	MesDatasMod.CreateDataFromMat(); // split training / testing et prepare visu
//	Invalidate();
//}
//
//void CChildFrame::GenerateFourierDescriptors(const Mat& image, int NumClasse)
//{
//	// Placeholder function to generate Fourier descriptors from an image
//	// Replace this with actual implementation
//	int wf = 128;
//	int Wwf = 1024;
//	std::vector<float> descriptors;
//	Mat ImageLarge;
//	cv::resize(image, ImageLarge, cv::Size(Wwf, Wwf));
//
//	for (int l = 0; l < ImageLarge.rows - wf; l += wf /2)
//	{
//		for (int c = 0; c < ImageLarge.cols - wf; c += wf/2 )
//		{
//			Mat window = ImageLarge(Rect(c, l, wf, wf));
//			double* DesFour;
//			DesFour = m_pima->DescripteursFourier1Gray(window);
//			cv::Mat descripteurs(1, m_pima->m_Npar, CV_32F);
//			for (int i = 0; i < m_pima->m_Npar; i++)
//			{
//				descripteurs.at<float>(0, i) = static_cast<float>(DesFour[i]);
//			}
//			free(DesFour);
//			MesDatasMod.m_allData.push_back(descripteurs);
//			MesDatasMod.m_allLabels.push_back((float)NumClasse);
//		}
//	}
//
//	return ;
//}
//void CChildFrame::OnImageDecisionsurunelistedimages()
//{
//
//	if (!LoadClassifier()) {
//		MessageBox("Erreur de chargement du classifieur", "Erreur", MB_OK | MB_ICONERROR);
//		return;
//	}
//
//
//
//	CFileDialog FileData(TRUE, "*.txt", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Text Files (*.txt)|*.txt||", this);
//	if (FileData.DoModal() != IDOK) return;
//
//	CString filePath = FileData.GetPathName();
//	std::ifstream file(filePath);
//	if (!file.is_open()) {
//		MessageBox("Erreur d'ouverture du fichier", "Erreur", MB_OK | MB_ICONERROR);
//		return;
//	}
//	std::filesystem::path path(filePath.GetString());
//	if (!std::filesystem::is_directory(path))
//	{
//		path = path.parent_path();
//	}
//	std::map<std::string, std::vector<std::string>> classToFileMap;
//	std::string line;
//	while (std::getline(file, line)) {
//		size_t pos = line.find('/');
//		if (pos != std::string::npos) {
//			std::string className = line.substr(0, pos);
//			classToFileMap[className].push_back(line);
//		}
//	}
//	file.close();
//
//
//
//	MesDatasMod.Clear();
//	MesDatasMod.m_nbClasses = classToFileMap.size();
//	MesDatasMod.m_nbParametres = 0; // This will be updated later
//	int nc = 0;
//	for (const auto& classEntry : classToFileMap)
//	{
//		const std::string& className = classEntry.first;
//		const std::vector<std::string>& fileNames = classEntry.second;
//
//		for (const std::string& fileName : fileNames)
//		{
//
//			std::ofstream resultFile(path.string() + "/classification_results.txt", std::ios::app);
//			if (!resultFile.is_open()) {
//				MessageBox("Erreur d'ouverture du fichier de résultats", "Erreur", MB_OK | MB_ICONERROR);
//				return;
//			}
//
//
//			string nomf = path.string() + "/../images/" + fileName;
//			Mat image = imread(nomf);
//			if (image.empty())
//			{
//				CString msg;
//				msg.Format("Erreur de lecture de l'image : %s", fileName.c_str());
//				MessageBox(msg, "Erreur", MB_OK | MB_ICONERROR);
//				continue;
//			}
//
//			MesDatasMod.Clear();
//			MesDatasMod.m_nbClasses = classToFileMap.size();
//			// Generate Fourier descriptors for the image
//			cvtColor(image, image, cv::COLOR_BGR2GRAY);
//			GenerateFourierDescriptors(image, 0);
//			MesDatasMod.m_nbParametres = MesDatasMod.m_allData.cols;
//			Mat sampletMat = Mat(1, MesDatasMod.m_nbParametres, CV_32FC1);
//			vector<int> classes;
//			for (int i = 0; i < MesDatasMod.m_nbClasses; i++) classes.push_back(0);
//			for (int ns = 0; ns < MesDatasMod.m_allData.rows; ns++)
//			{
//				for (int par = 0; par < MesDatasMod.m_nbParametres; par++)
//				{
//					sampletMat.at<float>(0, par) = MesDatasMod.m_allData.at<float>(ns, par);
//				}
//				int classId = (int)Decide(m_pClassifier, LastType, sampletMat);
//				if (classId < MesDatasMod.m_nbClasses) classes[classId]++;
//			}
//			int classeFinale = 0;
//			// classeFinale est la classe qui a le plus de votes
//			for (int i = 0; i < MesDatasMod.m_nbClasses; i++)
//			{
//				if (classes[i] > classes[classeFinale]) classeFinale = i;
//			}
//			// Write the result to the result file
//			resultFile << fileName << " " << classeFinale << std::endl;
//			resultFile.close();
//
//		}
//
//	}
//	
//	MessageBox("Classification terminée. Les résultats sont enregistrés dans classification_results.txt", "Succès", MB_OK);
//}


