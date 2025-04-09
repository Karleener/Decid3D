// DialGauss.cpp : implementation file
//

#include "stdafx.h"
#include "Decid.h"
#include "DialGauss.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDialGauss dialog


CDialGauss::CDialGauss(CWnd* pParent /*=NULL*/)
	: CDialog(CDialGauss::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDialGauss)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CDialGauss::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDialGauss)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDialGauss, CDialog)
	//{{AFX_MSG_MAP(CDialGauss)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDialGauss message handlers
