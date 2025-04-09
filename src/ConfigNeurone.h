#if !defined(AFX_CONFIGNEURONE_H__9615EE4F_4093_11D5_879D_0050043AFEB2__INCLUDED_)
#define AFX_CONFIGNEURONE_H__9615EE4F_4093_11D5_879D_0050043AFEB2__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ConfigNeurone.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CConfigNeurone dialog

class CChildFrame;

class CConfigNeurone : public CDialog
{
// Construction
public:
	float Seuil;
	CChildFrame* m_pParentFrame;
	CConfigNeurone(CWnd* pParent = NULL);   // standard constructor
	BOOL Create(CWnd* pParentWnd = nullptr) {
		return CDialog::Create(IDD_DIALOG_CONFIG_NEURONE, pParentWnd);
	}
	void SetParentFrame(CChildFrame* pParentFrame) 
	{
		m_pParentFrame = pParentFrame;
	}
// Dialog Data
	//{{AFX_DATA(CConfigNeurone)
	enum { IDD = IDD_DIALOG_CONFIG_NEURONE };
	CEdit	m_EditC3;
	CEdit	m_EditC2;
	CEdit	m_EditC1;
	CSliderCtrl	m_Slider;
	long	m_NbIter;
	CString	m_Seuil;
	int		m_c1;
	int		m_c2;
	int		m_c3;
	float	m_eta;
	int		m_HL;
	float	m_Erreur;
	int Npar;
	int NbClasses;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CConfigNeurone)
	public:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
public:

	// Generated message map functions
	//{{AFX_MSG(CConfigNeurone)
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	virtual BOOL OnInitDialog();
	afx_msg void OnChangeEdit1();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedButtonDessinnn();
	afx_msg void OnEnChangeEditC1();
	afx_msg void OnEnChangeEditC2();
	afx_msg void OnEnChangeEditC3();
	afx_msg void OnPaint();
	int m_activation;
	afx_msg void OnBnClickedButtonMaj();
	CButton m_BoutonMAJ;
	CString m_AfficheTexte;
	afx_msg void OnClose();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CONFIGNEURONE_H__9615EE4F_4093_11D5_879D_0050043AFEB2__INCLUDED_)
