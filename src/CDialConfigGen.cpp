// CDialConfigGen.cpp : fichier d'implémentation
//

#include "stdafx.h"
#include "Decid.h"
#include "afxdialogex.h"
#include "CDialConfigGen.h"


// boîte de dialogue de CDialConfigGen

IMPLEMENT_DYNAMIC(CDialConfigGen, CDialogEx)

CDialConfigGen::CDialConfigGen(CWnd* pParent /*=nullptr*/,int w, float tr, int gr,int nb, bool anim,bool don)
	: CDialogEx(IDD_DIALOG_CONFIG_GEN, pParent)
	, m_win3D(w)
	, m_tr(tr)
	, m_grid(gr)
	, m_Animate(anim)
	, m_DataOnly(don)
	, m_NbThread(nb)
{

}

CDialConfigGen::~CDialConfigGen()
{
}

void CDialConfigGen::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_WIN, m_win3D);
	DDV_MinMaxInt(pDX, m_win3D, 300, 1800);
	DDX_Text(pDX, IDC_EDIT_TR, m_tr);
	DDX_Text(pDX, IDC_EDIT_GRID, m_grid);
	DDV_MinMaxInt(pDX, m_grid, 20, 300);
	DDX_Check(pDX, IDC_CHECK1, m_Animate);
	DDX_Check(pDX, IDC_CHECK_DATAONLY, m_DataOnly);
	DDX_Text(pDX, IDC_EDIT_THREAD, m_NbThread);
	DDV_MinMaxInt(pDX, m_NbThread, 1, 8);
}


BEGIN_MESSAGE_MAP(CDialConfigGen, CDialogEx)
END_MESSAGE_MAP()


// gestionnaires de messages de CDialConfigGen
