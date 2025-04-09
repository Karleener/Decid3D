// CDialSegmentationLive.cpp : fichier d'implémentation
//

#include "stdafx.h"
#include "resource.h"
#include "afxdialogex.h"
#include "CDialSegmentationLive.h"


// boîte de dialogue de CDialSegmentationLive

IMPLEMENT_DYNAMIC(CDialSegmentationLive, CDialogEx)

CDialSegmentationLive::CDialSegmentationLive(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_SEGMENTATION_LIVE, pParent)
	, m_Wf(8)
	, m_nbSample(1000)
	, m_nbCluster(4)
	, m_TypeColor(0)
	, m_DataOnly(true)
{

}

CDialSegmentationLive::~CDialSegmentationLive()
{
}

void CDialSegmentationLive::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_LARG_FEN, m_Wf);
	DDV_MinMaxInt(pDX, m_Wf, 1, 32);
	DDX_Text(pDX, IDC_EDIT_NB_ECH, m_nbSample);
	DDV_MinMaxInt(pDX, m_nbSample, 100, 100000);
	DDX_Text(pDX, IDC_EDIT_NB_CLUSTERS, m_nbCluster);
	DDV_MinMaxInt(pDX, m_nbCluster, 2, 32);
	DDX_Radio(pDX, IDC_RADIO_LAB, m_TypeColor);
	DDX_Check(pDX, IDC_CHECK1, m_DataOnly);
}


BEGIN_MESSAGE_MAP(CDialSegmentationLive, CDialogEx)
END_MESSAGE_MAP()


// gestionnaires de messages de CDialSegmentationLive
