#if !defined(AFX_DIALGAUSS_H__765BCECB_5475_4D2B_AC98_08F79FFAFB83__INCLUDED_)
#define AFX_DIALGAUSS_H__765BCECB_5475_4D2B_AC98_08F79FFAFB83__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// DialGauss.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CDialGauss dialog

class CDialGauss : public CDialog
{
// Construction
public:
	CDialGauss(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CDialGauss)
	enum { IDD = IDD_DIALOG_GAUSS };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDialGauss)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CDialGauss)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DIALGAUSS_H__765BCECB_5475_4D2B_AC98_08F79FFAFB83__INCLUDED_)
