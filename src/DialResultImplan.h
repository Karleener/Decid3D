#if !defined(AFX_DIALRESULTIMPLAN_H__A568EF51_069D_11D7_87C9_0050043AFEB2__INCLUDED_)
#define AFX_DIALRESULTIMPLAN_H__A568EF51_069D_11D7_87C9_0050043AFEB2__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// DialResultImplan.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CDialResultImplan dialog

class CDialResultImplan : public CDialog
{
// Construction
public:
	CDialResultImplan(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CDialResultImplan)
	enum { IDD = IDD_DIALOG_RESULT };
	CListBox	m_Liste;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDialResultImplan)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CDialResultImplan)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DIALRESULTIMPLAN_H__A568EF51_069D_11D7_87C9_0050043AFEB2__INCLUDED_)
