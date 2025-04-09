// DialApprend.cpp : implementation file
//

#include "stdafx.h"
#include "Decid.h"
#include "DialApprend.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDialApprend dialog


CDialApprend::CDialApprend(CWnd* pParent /*=NULL*/)
	: CDialog(CDialApprend::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDialApprend)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CDialApprend::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDialApprend)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDialApprend, CDialog)
	//{{AFX_MSG_MAP(CDialApprend)
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDialApprend message handlers

void CDialApprend::DessinePoint(int iter, float val)
{
	int y;
	y= (int)(val*(float)Haut);
	pdc->MoveTo(iter,y);
}

BOOL CDialApprend::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	GetWindowRect(&Rect);
	Larg=Rect.Width();
	Haut=Rect.Height();
	// TODO: Add extra initialization here
	pdc= new CClientDC(this);
	CPen Stylo(PS_SOLID,1,RGB(255,0,0));
	pdc->SelectObject(&Stylo);
	pdc->LineTo(0,0);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CDialApprend::OnDestroy() 
{
	ReleaseDC(pdc);
	CDialog::OnDestroy();
	
	// TODO: Add your message handler code here
	
}
