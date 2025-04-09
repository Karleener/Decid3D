#if !defined(AFX_DIALPMC_H__9AB014D1_BF0F_11D1_A4CB_00A0242E2BA0__INCLUDED_)
#define AFX_DIALPMC_H__9AB014D1_BF0F_11D1_A4CB_00A0242E2BA0__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// DialPmc.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CDialPmc dialog

class CDialPmc : public CDialog
{
// Construction
public:
	CString NomFichMMC,NomFichPMC;
protected:
	BOOL ValidMMC,ValidPMC;
public:
	CDialPmc(CWnd* pParent = NULL,CString initmmc="",CString initpmc="",int initR=5);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CDialPmc)
	enum { IDD = IDD_PMC };
	CButton	m_pmc;
	CSpinButtonCtrl	m_spin;
	CButton	m_sauver;
	CButton	m_mmc;
	int		m_coefR;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDialPmc)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CDialPmc)
	virtual BOOL OnInitDialog();
	afx_msg void OnButton1();
	virtual void OnOK();
	afx_msg void OnButton2();
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnChangeEdit2();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DIALPMC_H__9AB014D1_BF0F_11D1_A4CB_00A0242E2BA0__INCLUDED_)
