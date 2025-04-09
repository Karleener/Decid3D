// DialPmc.cpp : implementation file
//

#include "stdafx.h"
#include "Decid.h"
#include "DialPmc.h"

/////////////////////////////////////////////////////////////////////////////
// CDialPmc dialog


CDialPmc::CDialPmc(CWnd* pParent /*=NULL*/,CString initmmc,CString initpmc,int initR)
	: CDialog(CDialPmc::IDD, pParent)
{

	NomFichMMC=initmmc;
	NomFichPMC=initpmc;
	ValidMMC=ValidPMC=FALSE;
	//{{AFX_DATA_INIT(CDialPmc)
	m_coefR = initR;
	//}}AFX_DATA_INIT
}


void CDialPmc::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDialPmc)
	DDX_Control(pDX, IDC_BUTTON2, m_pmc);
	DDX_Control(pDX, IDC_SPIN2, m_spin);
	DDX_Control(pDX, IDOK, m_sauver);
	DDX_Control(pDX, IDC_BUTTON1, m_mmc);
	DDX_Text(pDX, IDC_EDIT2, m_coefR);
	DDV_MinMaxInt(pDX, m_coefR, 1, 9);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDialPmc, CDialog)
	//{{AFX_MSG_MAP(CDialPmc)
	ON_BN_CLICKED(IDC_BUTTON1, OnButton1)
	ON_BN_CLICKED(IDC_BUTTON2, OnButton2)
	ON_WM_VSCROLL()
	ON_EN_CHANGE(IDC_EDIT2, OnChangeEdit2)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDialPmc message handlers

BOOL CDialPmc::OnInitDialog() 
{
	CDialog::OnInitDialog();


	m_spin.SetRange(1,9);
	m_spin.SetPos(m_coefR);
	CFile Test;

	if(Test.Open(NomFichMMC,CFile::modeRead))
	{
		m_mmc.SetWindowText(Test.GetFileName());
		ValidMMC=TRUE;
		Test.Close();
	}
	if(Test.Open(NomFichPMC,CFile::modeRead))
	{
		m_pmc.SetWindowText(Test.GetFileName());
		ValidPMC=TRUE;
		Test.Close();
	}
	m_sauver.EnableWindow(ValidMMC&&ValidPMC);
	m_pmc.EnableWindow(ValidMMC);


	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CDialPmc::OnButton1() 
{
	CFile Fichier; // variable fichier 	                                        

	CFileDialog * FileData = new CFileDialog (TRUE,"*.mcf",NomFichMMC,OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,"Mesures multi-classes (*.mcf)|*.mcf|",this);

	if (FileData->DoModal()==IDOK)
	{
		if(FileData->GetFileExt()!="mcf")
		{
			MessageBox("Le fichier selectionné n'est pas valide!","Erreur",MB_OK|MB_ICONSTOP);
			m_mmc.SetWindowText("*.mcf");
			ValidMMC=FALSE;
		}
		else if (Fichier.Open(FileData->GetPathName(), CFile::modeRead))
		{ 
			Fichier.Close(); 
			NomFichMMC=FileData->GetPathName();
			m_mmc.SetWindowText(FileData->GetFileName());
			ValidMMC=TRUE;
		}
		else 
		{
			MessageBox("Le fichier  "+FileData->GetFileName()+"  n'existe pas!","Erreur",MB_OK|MB_ICONSTOP);
			ValidMMC=FALSE;
		}
	}

	m_sauver.EnableWindow(ValidMMC&&ValidPMC);
	m_pmc.EnableWindow(ValidMMC);
	UpdateData();
	delete FileData;	
}

void CDialPmc::OnButton2() 
{
	CFile Fichier; // variable fichier 	                                        

	CFileDialog * FileData = new CFileDialog (FALSE,"*.pmf",NomFichPMC,OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,"Polytopes multi-classes (*.pmf)|*.pmf|",this);

	if (FileData->DoModal()==IDOK)
	{
		if(FileData->GetFileExt()!="pmf")
		{
			MessageBox("Le fichier selectionné n'est pas valide!","Erreur",MB_OK|MB_ICONSTOP);
			ValidPMC=FALSE;
		}
		else if (Fichier.Open(FileData->GetPathName(), CFile::modeRead))
		{ 
			Fichier.Close(); 
			NomFichPMC=FileData->GetPathName();
			m_pmc.SetWindowText(FileData->GetFileName());
			ValidPMC=TRUE;
			
		}
		else 
		{
			MessageBox("Le fichier  "+FileData->GetFileName()+"  sera créé","Message",MB_OK|MB_ICONEXCLAMATION);	
			NomFichPMC=FileData->GetPathName();
			m_pmc.SetWindowText(FileData->GetFileName());
			ValidPMC=TRUE;
		}
	}

	m_sauver.EnableWindow(ValidMMC&&ValidPMC);
	UpdateData();
	delete FileData;	
}

void CDialPmc::OnOK() 
{
	UpdateData();

	CDialog::OnOK();
}

void CDialPmc::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	m_coefR=nPos;
	UpdateData(FALSE);

	CDialog::OnVScroll(nSBCode, nPos, pScrollBar);
}

void CDialPmc::OnChangeEdit2() 
{
	UpdateData();	
}
