#if !defined(AFX_DIALGAUSS1_H__810851C7_B496_4519_8831_57183A44D9B7__INCLUDED_)
#define AFX_DIALGAUSS1_H__810851C7_B496_4519_8831_57183A44D9B7__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// DialGauss1.h : header file
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
	int		m_Sample;
	CString	m_Prefix;
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
public:
	int m_NbClasses;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DIALGAUSS1_H__810851C7_B496_4519_8831_57183A44D9B7__INCLUDED_)
