// DialChoixClass.cpp : implementation file
//

#include "stdafx.h"
#include "Decid.h"
#include "DialChoixClass.h"


/////////////////////////////////////////////////////////////////////////////
// CDialChoixClass dialog


CDialChoixClass::CDialChoixClass(CWnd* pParent,BYTE initclass1,BYTE initclass2,BYTE initMAX,double initprobc1,double initprobc2)
	: CDialog(CDialChoixClass::IDD, pParent)
{
	Max=initMAX-1;
	//{{AFX_DATA_INIT(CDialChoixClass)
	m_class1 = initclass1;
	m_class2 = initclass2;
	m_probc1 = initprobc1;
	m_probc2 = initprobc2;
	//}}AFX_DATA_INIT
}


void CDialChoixClass::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDialChoixClass)
	DDX_Control(pDX, IDC_SPIN2, m_spin2);
	DDX_Control(pDX, IDC_SPIN1, m_spin1);
	DDX_Text(pDX, IDC_EDIT1, m_class1);
	DDV_MinMaxByte(pDX, m_class1, 0, 255);
	DDX_Text(pDX, IDC_EDIT2, m_class2);
	DDV_MinMaxByte(pDX, m_class2, 0, 255);
	DDX_Text(pDX, IDC_EDIT3, m_probc1);
	DDV_MinMaxDouble(pDX, m_probc1, 0., 1.);
	DDX_Text(pDX, IDC_EDIT4, m_probc2);
	DDV_MinMaxDouble(pDX, m_probc2, 0., 1.);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDialChoixClass, CDialog)
	//{{AFX_MSG_MAP(CDialChoixClass)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDialChoixClass message handlers

BOOL CDialChoixClass::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	m_spin1.SetRange(0,Max);
	m_spin2.SetRange(0,Max);
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}


void CDialChoixClass::OnOK() 
{
	UpdateData();
	if(m_class1>Max||m_class2>Max) 
	{
		CString text;
		text.Format("Valeur comprise entre 0 et %d",Max);
		MessageBox(text,NULL,MB_OK|MB_ICONEXCLAMATION);

	}
	else CDialog::OnOK();
}

