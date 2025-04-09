// DialBoost.cpp : implementation file
//

#include "stdafx.h"
#include "Decid.h"
#include "DialBoost.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDialBoost dialog


CDialBoost::CDialBoost(CWnd* pParent /*=NULL*/)
	: CDialog(CDialBoost::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDialBoost)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CDialBoost::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDialBoost)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDialBoost, CDialog)
	//{{AFX_MSG_MAP(CDialBoost)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDialBoost message handlers
