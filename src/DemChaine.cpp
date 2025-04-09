// DemChaine.cpp : implementation file
//

#include "stdafx.h"
#include "Decid.h"
#include "DemChaine.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDemChaine dialog


CDemChaine::CDemChaine(CWnd* pParent /*=NULL*/)
	: CDialog(CDemChaine::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDemChaine)
	m_Chaine = _T("");
	//}}AFX_DATA_INIT
}


void CDemChaine::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDemChaine)
	DDX_Text(pDX, IDC_EDIT1, m_Chaine);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDemChaine, CDialog)
	//{{AFX_MSG_MAP(CDemChaine)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDemChaine message handlers
