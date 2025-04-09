#pragma once
#include "afxdialogex.h"


// boîte de dialogue de CDialConfigGen

class CDialConfigGen : public CDialogEx
{
	DECLARE_DYNAMIC(CDialConfigGen)

public:
	CDialConfigGen(CWnd* pParent = nullptr,  int w = 800, float tr = 0.5, int gr = 80,int nb=4,bool anim=false,bool don=false);   // constructeur standard
	virtual ~CDialConfigGen();

// Données de boîte de dialogue
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_CONFIG_GEN };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // Prise en charge de DDX/DDV

	DECLARE_MESSAGE_MAP()
public:
	int m_win3D;
	float m_tr;
	int m_grid;
	int m_NbThread;
	BOOL m_Animate;
	BOOL m_DataOnly;

};
