// DialVam.cpp : implementation file
//

#include "stdafx.h"
#include "Decid.h"
#include "DialVam.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDialVam dialog


CDialVam::CDialVam(CWnd* pParent /*=NULL*/)
	: CDialog(CDialVam::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDialVam)
	m_K = 2;
	//}}AFX_DATA_INIT
}


void CDialVam::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDialVam)
	DDX_Text(pDX, IDC_EDIT1, m_K);
	DDV_MinMaxInt(pDX, m_K, 1, 100);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDialVam, CDialog)
	//{{AFX_MSG_MAP(CDialVam)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDialVam message handlers
