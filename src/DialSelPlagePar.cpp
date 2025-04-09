// DialSelPlagePar.cpp : fichier d'implémentation
//

#include "stdafx.h"
#include "Decid.h"
#include "DialSelPlagePar.h"


// Boîte de dialogue CDialSelPlagePar

IMPLEMENT_DYNAMIC(CDialSelPlagePar, CDialog)

CDialSelPlagePar::CDialSelPlagePar(CWnd* pParent /*=NULL*/)
	: CDialog(CDialSelPlagePar::IDD, pParent)
	, m_Plage(10)
	, m_Pas(5)
{

}

CDialSelPlagePar::~CDialSelPlagePar()
{
}

void CDialSelPlagePar::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_NBPAR, m_Plage);
	DDX_Text(pDX, IDC_EDIT_PAS, m_Pas);
}


BEGIN_MESSAGE_MAP(CDialSelPlagePar, CDialog)
END_MESSAGE_MAP()


// Gestionnaires de messages de CDialSelPlagePar
