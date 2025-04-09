#pragma once


// Boîte de dialogue CDialSelPlagePar

class CDialSelPlagePar : public CDialog
{
	DECLARE_DYNAMIC(CDialSelPlagePar)

public:
	CDialSelPlagePar(CWnd* pParent = NULL);   // constructeur standard
	virtual ~CDialSelPlagePar();

// Données de boîte de dialogue
	enum { IDD = IDD_DIALOG_SELPAR };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // Prise en charge de DDX/DDV

	DECLARE_MESSAGE_MAP()
public:
	int m_Plage;
	int m_Pas;
};
