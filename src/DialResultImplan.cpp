// DialResultImplan.cpp : implementation file
//

#include "stdafx.h"
#include "Decid.h"
#include "DialResultImplan.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDialResultImplan dialog


CDialResultImplan::CDialResultImplan(CWnd* pParent /*=NULL*/)
	: CDialog(CDialResultImplan::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDialResultImplan)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CDialResultImplan::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDialResultImplan)
	DDX_Control(pDX, IDC_LIST2, m_Liste);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDialResultImplan, CDialog)
	//{{AFX_MSG_MAP(CDialResultImplan)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDialResultImplan message handlers
