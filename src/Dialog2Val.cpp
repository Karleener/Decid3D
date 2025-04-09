// Dialog2Val.cpp : implementation file
//

#include "stdafx.h"
#include "Decid.h"
#include "Dialog2Val.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDialog2Val dialog


CDialog2Val::CDialog2Val(CWnd* pParent /*=NULL*/)
	: CDialog(CDialog2Val::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDialog2Val)
	m_val0 = 0;
	m_val1 = 0;
	//}}AFX_DATA_INIT
}


void CDialog2Val::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDialog2Val)
	DDX_Text(pDX, IDC_EDIT1, m_val0);
	DDX_Text(pDX, IDC_EDIT2, m_val1);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDialog2Val, CDialog)
	//{{AFX_MSG_MAP(CDialog2Val)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDialog2Val message handlers

CDialog2Val::CDialog2Val(int param0, int param1)
{
 m_val0=param0;
 m_val1=param1;

}

void CDialog2Val::OnOK() 
{
	// TODO: Add extra validation here
	UpdateData(TRUE);
	CDialog::OnOK();
}
