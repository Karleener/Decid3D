// DialGauss1.cpp : implementation file
//

#include "stdafx.h"
#include "Decid.h"
#include "DialGauss1.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDialGauss dialog


CDialGauss::CDialGauss(CWnd* pParent /*=NULL*/)
	: CDialog(CDialGauss::IDD, pParent)
	, m_NbClasses(3)
{
	//{{AFX_DATA_INIT(CDialGauss)
	m_Sample = 1000;
	m_Prefix = _T("Gauss3D");
	//}}AFX_DATA_INIT
}


void CDialGauss::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDialGauss)
	DDX_Text(pDX, IDC_EDIT4, m_Sample);
	DDX_Text(pDX, IDC_EDIT5, m_Prefix);
	//}}AFX_DATA_MAP
	DDX_Text(pDX, IDC_EDIT_NB_CLASSES, m_NbClasses);
	DDV_MinMaxInt(pDX, m_NbClasses, 2, 10);
}


BEGIN_MESSAGE_MAP(CDialGauss, CDialog)
	//{{AFX_MSG_MAP(CDialGauss)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDialGauss message handlers
