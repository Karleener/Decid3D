// Dialiter.cpp : implementation file
//

#include "stdafx.h"
#include "Decid.h"
#include "Dialiter.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDialiter dialog


CDialiter::CDialiter(CWnd* pParent /*=NULL*/)
	: CDialog(CDialiter::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDialiter)
	m_Binf = 100;
	m_Bsup = 1000;
	m_Iter = 100;
	m_Erreur = 0.01f;
	m_Coef = 5;
	m_ptaleat = 5;
	m_bruit = 10;
	m_NbSlice = 2000;
	m_Courbe = FALSE;
	//}}AFX_DATA_INIT
}


void CDialiter::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDialiter)
	DDX_Text(pDX, IDC_EDIT1, m_Binf);
	DDX_Text(pDX, IDC_EDIT3, m_Bsup);
	DDX_Text(pDX, IDC_EDIT4, m_Iter);
	DDX_Text(pDX, IDC_EDIT5, m_Erreur);
	DDX_Text(pDX, IDC_EDIT6, m_Coef);
	DDV_MinMaxInt(pDX, m_Coef, 5, 9);
	DDX_Text(pDX, IDC_EDIT7, m_ptaleat);
	DDX_Text(pDX, IDC_EDIT8, m_bruit);
	DDX_Text(pDX, IDC_EDIT9, m_NbSlice);
	DDX_Check(pDX, IDC_CHECK_COURBE, m_Courbe);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDialiter, CDialog)
	//{{AFX_MSG_MAP(CDialiter)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDialiter message handlers
