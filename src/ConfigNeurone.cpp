// ConfigNeurone.cpp : implementation file
//

#include "stdafx.h"
#include "Decid.h"
#include "ConfigNeurone.h"
#include<vector>
#include <algorithm>
#include "ChildFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CConfigNeurone dialog


CConfigNeurone::CConfigNeurone(CWnd* pParent /*=NULL*/)
	: CDialog(CConfigNeurone::IDD, pParent)
	, m_activation(1)
	, m_AfficheTexte(_T(""))
{
	//{{AFX_DATA_INIT(CConfigNeurone)
	m_NbIter = 1;
	m_c1 = 2;
	m_c2 = 2;
	m_c3 = 2;
	m_eta = 0.01f;
	m_HL = 1;
	m_Erreur = 0.0f;
	//}}AFX_DATA_INIT
}


void CConfigNeurone::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CConfigNeurone)
	DDX_Control(pDX, IDC_EDIT_C3, m_EditC3);
	DDX_Control(pDX, IDC_EDIT_C2, m_EditC2);
	DDX_Control(pDX, IDC_EDIT_C1, m_EditC1);
	DDX_Control(pDX, IDC_SLIDER1, m_Slider);
	DDX_Text(pDX, IDC_EDIT3, m_NbIter);
	DDV_MinMaxLong(pDX, m_NbIter, 10, 100000);
	DDX_Text(pDX, IDC_SEUIL, m_Seuil);
	DDX_Text(pDX, IDC_EDIT_C1, m_c1);
	//	DDV_MinMaxInt(pDX, m_c1, 2, 1000);
	DDX_Text(pDX, IDC_EDIT_C2, m_c2);
	//DDV_MinMaxInt(pDX, m_c2, 2, 1000);
	DDX_Text(pDX, IDC_EDIT_C3, m_c3);
	//DDV_MinMaxInt(pDX, m_c3, 2, 1000);
	DDX_Text(pDX, IDC_EDIT_ETA, m_eta);
	DDX_Text(pDX, IDC_EDIT1, m_HL);
	DDV_MinMaxInt(pDX, m_HL, 1, 3);
	DDX_Text(pDX, IDC_EDIT_ERREUR, m_Erreur);
	//}}AFX_DATA_MAP
	DDX_Radio(pDX, IDC_RADIO_LIN, m_activation);
	DDX_Control(pDX, IDC_BUTTON_MAJ, m_BoutonMAJ);
	DDX_Text(pDX, IDC_AFFICHE_TEXTE, m_AfficheTexte);
}


BEGIN_MESSAGE_MAP(CConfigNeurone, CDialog)
	//{{AFX_MSG_MAP(CConfigNeurone)
	ON_WM_HSCROLL()
	ON_EN_CHANGE(IDC_EDIT1, OnChangeEdit1)
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_BUTTON_DESSINNN, &CConfigNeurone::OnBnClickedButtonDessinnn)
	ON_EN_CHANGE(IDC_EDIT_C1, &CConfigNeurone::OnEnChangeEditC1)
	ON_EN_CHANGE(IDC_EDIT_C2, &CConfigNeurone::OnEnChangeEditC2)
	ON_EN_CHANGE(IDC_EDIT_C3, &CConfigNeurone::OnEnChangeEditC3)
	ON_WM_PAINT()
	ON_BN_CLICKED(IDC_BUTTON_MAJ, &CConfigNeurone::OnBnClickedButtonMaj)
	ON_WM_CLOSE()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CConfigNeurone message handlers

void CConfigNeurone::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	// TODO: Add your message handler code here and/or call default
	
	CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
	Seuil=(float)m_Slider.GetPos()/100.0F;
	UpdateData(TRUE);
	m_Seuil.Format("Seuil de décision = %1.2f",Seuil);
	UpdateData(FALSE);

}

BOOL CConfigNeurone::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here
	m_Seuil.Format("Seuil de décision = %1.2f",Seuil);
	UpdateData(FALSE);
	m_Slider.SetPos(Seuil*100);
	if (m_HL==1) {m_EditC1.EnableWindow(TRUE);m_EditC2.EnableWindow(FALSE);m_EditC3.EnableWindow(FALSE);}
	if (m_HL==2) {m_EditC1.EnableWindow(TRUE);m_EditC2.EnableWindow(TRUE);m_EditC3.EnableWindow(FALSE);}
	if (m_HL==3) {m_EditC1.EnableWindow(TRUE);m_EditC2.EnableWindow(TRUE);m_EditC3.EnableWindow(TRUE);}
	OnBnClickedButtonDessinnn();
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CConfigNeurone::OnChangeEdit1() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);
	if (m_HL==1) {m_EditC1.EnableWindow(TRUE);m_EditC2.EnableWindow(FALSE);m_EditC3.EnableWindow(FALSE);}
	if (m_HL==2) {m_EditC1.EnableWindow(TRUE);m_EditC2.EnableWindow(TRUE);m_EditC3.EnableWindow(FALSE);}
	if (m_HL==3) {m_EditC1.EnableWindow(TRUE);m_EditC2.EnableWindow(TRUE);m_EditC3.EnableWindow(TRUE);}
	OnBnClickedButtonDessinnn();
}


void CConfigNeurone::OnBnClickedButtonDessinnn()
{
	CClientDC dc(this); // contexte de dessin
	UpdateData(TRUE);
	CRect rect;
	GetClientRect(&rect);
	
	int dialogWidth = rect.Width();
	int dialogHeight = rect.Height();
  // Définir la position et les dimensions pour dessiner le graphe
	int startX = dialogWidth / 2;  // Position de départ sur l'axe X (milieu de la boîte)
	int startY = 50;  // Position de départ sur l'axe Y
	int neuronRadius = 10;   // Rayon des cercles représentant les neurones

	// Exemple de structure du réseau (remplacer par les valeurs dynamiques)
	CRect ZoneDessin(rect);
	ZoneDessin.left = startX- neuronRadius;
	dc.FillSolidRect(&ZoneDessin, ::GetSysColor(COLOR_WINDOW)); // Remplir avec la couleur de fond
	std::vector<int> networkStructure;
	networkStructure.push_back(Npar);
	if (m_HL >= 1)
	{
		networkStructure.push_back(m_c1);
	}
	if (m_HL >= 2)
	{
		networkStructure.push_back(m_c2);
	}
	if (m_HL >= 3)
	{
		networkStructure.push_back(m_c3);
	}
	networkStructure.push_back(NbClasses); // Nombre de sorties
	  // Calculer automatiquement l'espacement vertical
	int numLayers = networkStructure.size();
	int layerSpacing = ( (dialogWidth/2) - 2 * neuronRadius) / (numLayers > 1 ? numLayers - 1 : 1);

	// Calculer automatiquement l'espacement vertical
	int maxNeurons = *std::max_element(networkStructure.begin(), networkStructure.end());
	int neuronSpacing = (dialogHeight - 2 * neuronRadius) / (maxNeurons > 1 ? maxNeurons - 1 : 1);
	// Tableau pour mémoriser les positions des neurones
	std::vector<std::vector<CPoint>> neuronPositions;

	// Dessiner les neurones
// Dessiner les neurones

	CBrush BrushRed(RGB(200, 10, 10));
	CBrush BrushBlue(RGB(10, 10, 200));
	CBrush* pOldBrush = dc.SelectObject(&BrushRed);

	for (size_t layer = 0; layer < networkStructure.size(); ++layer)
	{
		if (layer==1) 	dc.SelectObject(&BrushRed);
		if (layer >= 1 && layer < networkStructure.size()-1) dc.SelectObject(pOldBrush);
		if (layer==networkStructure.size()-1) dc.SelectObject(&BrushBlue);
		int numNeurons = networkStructure[layer];
		int layerX = startX + layer * layerSpacing;

		std::vector<CPoint> layerPositions;

		for (int neuron = 0; neuron < numNeurons; ++neuron)
		{
			int offsetY = (dialogHeight - (numNeurons - 1) * neuronSpacing) / 2;
			int neuronY = offsetY + neuron * neuronSpacing;
			CPoint neuronCenter(layerX, neuronY);
			layerPositions.push_back(neuronCenter);

			// Dessiner un cercle pour chaque neurone
			dc.Ellipse(neuronCenter.x - neuronRadius, neuronCenter.y - neuronRadius,neuronCenter.x + neuronRadius, neuronCenter.y + neuronRadius);
		}

		neuronPositions.push_back(layerPositions);
	}
	dc.SelectObject(pOldBrush);
	// Dessiner les connexions entre les couches
	for (size_t layer = 0; layer < neuronPositions.size() - 1; ++layer)
	{
		const auto& currentLayer = neuronPositions[layer];
		const auto& nextLayer = neuronPositions[layer + 1];

		for (const auto& currentNeuron : currentLayer)
		{
			for (const auto& nextNeuron : nextLayer)
			{
				dc.MoveTo(currentNeuron);
				dc.LineTo(nextNeuron);
			}
		}
	}


}


void CConfigNeurone::OnEnChangeEditC1()
{
	// TODO:  S'il s'agit d'un contrôle RICHEDIT, le contrôle ne
	// envoyez cette notification sauf si vous substituez CDialog::OnInitDialog()
	// fonction et appelle CRichEditCtrl().SetEventMask()
	// avec l'indicateur ENM_CHANGE ajouté au masque grâce à l'opérateur OR.

	// TODO:  Ajoutez ici le code de votre gestionnaire de notification de contrôle

	OnBnClickedButtonDessinnn();
}


void CConfigNeurone::OnEnChangeEditC2()
{
	// TODO:  S'il s'agit d'un contrôle RICHEDIT, le contrôle ne
	// envoyez cette notification sauf si vous substituez CDialog::OnInitDialog()
	// fonction et appelle CRichEditCtrl().SetEventMask()
	// avec l'indicateur ENM_CHANGE ajouté au masque grâce à l'opérateur OR.

	// TODO:  Ajoutez ici le code de votre gestionnaire de notification de contrôle

	OnBnClickedButtonDessinnn();
}



void CConfigNeurone::OnEnChangeEditC3()
{
	// TODO:  S'il s'agit d'un contrôle RICHEDIT, le contrôle ne
	// envoyez cette notification sauf si vous substituez CDialog::OnInitDialog()
	// fonction et appelle CRichEditCtrl().SetEventMask()
	// avec l'indicateur ENM_CHANGE ajouté au masque grâce à l'opérateur OR.

	// TODO:  Ajoutez ici le code de votre gestionnaire de notification de contrôle

	OnBnClickedButtonDessinnn();

}


void CConfigNeurone::OnPaint()
{
	CPaintDC dc(this); // device context for painting
					   // TODO: ajoutez ici le code de votre gestionnaire de messages
					   // N'appelez pas CDialog::OnPaint() pour la peinture des messages
	OnBnClickedButtonDessinnn();
}


void CConfigNeurone::OnBnClickedButtonMaj()
{
	UpdateData(true);
	if (m_c1 < 2) m_c1 = 2;
	if (m_c2 < 2) m_c2 = 2;
	if (m_c3 < 2) m_c3 = 2;
	UpdateData(false);
	OnBnClickedButtonDessinnn();
	if (m_pParentFrame) {
		m_pParentFrame->UpdateNNParameters(this);
	}


}


void CConfigNeurone::OnClose()
{
	// TODO: ajoutez ici le code de votre gestionnaire de messages et/ou les paramètres par défaut des appels
	if (m_pParentFrame)
	{
		m_pParentFrame->m_Loaded = false;
		m_pParentFrame->m_VisuDynamique = false;
	}
	CDialog::OnClose();
}
