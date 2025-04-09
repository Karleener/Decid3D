// DaliogProgression.cpp : implementation file
//

#include "stdafx.h"
#include "Decid.h"
#include "DaliogProgression.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDaliogProgression dialog


CDaliogProgression::CDaliogProgression(CWnd* pParent /*=NULL*/)
	: CDialog(CDaliogProgression::IDD, pParent)
	, m_Affiche(_T(""))
{
	//{{AFX_DATA_INIT(CDaliogProgression)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CDaliogProgression::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDaliogProgression)
	DDX_Control(pDX, IDC_PROGRESS_BAR, m_ControlProgressBar);
	//}}AFX_DATA_MAP
	DDX_Text(pDX, IDC_TEXTE, m_Affiche);
}


BEGIN_MESSAGE_MAP(CDaliogProgression, CDialog)
	//{{AFX_MSG_MAP(CDaliogProgression)
	ON_WM_PAINT()
	ON_MESSAGE(WM_UPDATE_PROGRESS, &CDaliogProgression::OnUpdateProgress)
	//}}AFX_MSG_MAP
	ON_WM_TIMER()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDaliogProgression message handlers

BOOL CDaliogProgression::OnInitDialog() 
{
	CDialog::OnInitDialog();
 	
	m_ControlProgressBar.SetPos(0);
	m_ControlProgressBar.SetRange(0,Valeur_Maxi);
	ValeurCourante = 0;
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CDaliogProgression::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	

	/*UpdateData(TRUE);
	m_ControlProgressBar.SetPos(ValeurCourante);
	UpdateData(FALSE);*/
	

	// Do not call CDialog::OnPaint() for painting messages
}


LRESULT CDaliogProgression::OnUpdateProgress(WPARAM wParam, LPARAM lParam) {
	int position = static_cast<int>(wParam);
	m_ControlProgressBar.SetPos(position);
	return 0;
}

void CDaliogProgression::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: ajoutez ici le code de votre gestionnaire de messages et/ou les paramètres par défaut des appels
	int a = m_ControlProgressBar.GetPos();
	m_ControlProgressBar.SetPos(a+1);
	UpdateWindow();
	CDialog::OnTimer(nIDEvent);
}
