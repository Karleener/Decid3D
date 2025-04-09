// CDialSlider.cpp : fichier d'implémentation
//

#include "stdafx.h"
#include "Decid.h"
#include "afxdialogex.h"
#include "CDialSlider.h"
#include "ChildFrm.h"

// boîte de dialogue de CDialSlider

IMPLEMENT_DYNAMIC(CDialSlider, CDialogEx)

CDialSlider::CDialSlider(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_SLIDER, pParent)
	, m_Pos(7)
	, m_Aff(_T(""))
	, m_AfficheErreur(_T(""))
{
	m_pParentFrame = NULL;
}

CDialSlider::~CDialSlider()
{
}

void CDialSlider::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Slider(pDX, IDC_SLIDER1, m_Pos);
	DDX_Text(pDX, IDC_TEXTE, m_Aff);
	DDX_Text(pDX, IDC_AFFICHEERREUR, m_AfficheErreur);
}


BEGIN_MESSAGE_MAP(CDialSlider, CDialogEx)
	ON_WM_HSCROLL()
	ON_WM_CLOSE()
END_MESSAGE_MAP()


// gestionnaires de messages de CDialSlider


void CDialSlider::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	// TODO: ajoutez ici le code de votre gestionnaire de messages et/ou les paramètres par défaut des appels
	if (m_bIsScrolling)
		return;
	m_bIsScrolling = TRUE;
	UpdateData(true);
	int kp = 1+ (int)(m_Pos / 5);
	m_Aff.Format("Valeur = %d", kp);
	UpdateData(false);

	if (m_pParentFrame && kp > 0)
	{
		if (m_pParentFrame->m_Done)
		{
			if (m_pParentFrame->MesDatasMod.m_nbParametres == 3)
			{
				if (nSBCode == SB_ENDSCROLL) m_pParentFrame->UpdateKPPVParameters(kp, this);
			}
			else m_pParentFrame->UpdateKPPVParameters(kp, this);
		}
	}
	m_bIsScrolling = FALSE;
	CDialogEx::OnHScroll(nSBCode, nPos, pScrollBar);
}


void CDialSlider::OnClose()
{
	// TODO: ajoutez ici le code de votre gestionnaire de messages et/ou les paramètres par défaut des appels
	if (m_pParentFrame)
	{
		m_pParentFrame->m_Loaded = false;
		m_pParentFrame->m_VisuDynamique = false;
	}
	CDialogEx::OnClose();
}
