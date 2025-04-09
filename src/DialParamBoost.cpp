// DialParamBoost.cpp : implementation file
//

#include "stdafx.h"
#include "Decid.h"
#include "DialParamBoost.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDialParamBoost dialog


CDialParamBoost::CDialParamBoost(CWnd* pParent /*=NULL*/)
	: CDialog(CDialParamBoost::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDialParamBoost)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CDialParamBoost::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDialParamBoost)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDialParamBoost, CDialog)
	//{{AFX_MSG_MAP(CDialParamBoost)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDialParamBoost message handlers
