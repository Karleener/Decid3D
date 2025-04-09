#if !defined(AFX_DIALVAM_H__A7C18772_2F56_11D5_879D_0050043AFEB2__INCLUDED_)
#define AFX_DIALVAM_H__A7C18772_2F56_11D5_879D_0050043AFEB2__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// DialVam.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CDialVam dialog

class CDialVam : public CDialog
{
// Construction
public:
	CDialVam(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CDialVam)
	enum { IDD = IDD_DIALVAL };
	int		m_K;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDialVam)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CDialVam)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DIALVAM_H__A7C18772_2F56_11D5_879D_0050043AFEB2__INCLUDED_)
