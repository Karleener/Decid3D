// DialBoosting.cpp : implementation file
//

#include "stdafx.h"
#include "Decid.h"
#include "DialBoosting.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDialBoosting dialog


CDialBoosting::CDialBoosting(CWnd* pParent /*=NULL*/)
	: CDialog(CDialBoosting::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDialBoosting)
	m_Choix = 0;
	m_Iter = 50;
	m_Bin = FALSE;
	//}}AFX_DATA_INIT
	m_NomFicBoost="default.boost";
}


void CDialBoosting::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDialBoosting)
	DDX_Radio(pDX, IDC_RADIO1, m_Choix);
	DDX_Text(pDX, IDC_EDIT1, m_Iter);
	DDX_Check(pDX, IDC_CHECK_BIN, m_Bin);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDialBoosting, CDialog)
	//{{AFX_MSG_MAP(CDialBoosting)
	ON_BN_CLICKED(IDC_BUTTON_FIC, OnButtonFic)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDialBoosting message handlers

void CDialBoosting::OnButtonFic() 
{
	// TODO: Add your control notification handler code here
		CFileDialog Model_dia(TRUE,"boost",NULL, OFN_HIDEREADONLY| OFN_OVERWRITEPROMPT,
					"Boosting Files (*.boost)|*.boost|" "All Files (*.*)|*.*||");
		if(Model_dia.DoModal() == IDOK)		m_NomFicBoost = Model_dia.GetPathName();
		else m_NomFicBoost="default.boost";

}
