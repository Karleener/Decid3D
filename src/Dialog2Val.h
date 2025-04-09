#if !defined(AFX_DIALOG2VAL_H__6910777B_3026_11D5_879D_0050043AFEB2__INCLUDED_)
#define AFX_DIALOG2VAL_H__6910777B_3026_11D5_879D_0050043AFEB2__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// Dialog2Val.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CDialog2Val dialog

class CDialog2Val : public CDialog
{
// Construction
public:
	CDialog2Val(int param0,int param1);
	CDialog2Val(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CDialog2Val)
	enum { IDD = IDD_DIALOG_2VAL };
	int		m_val0;
	int		m_val1;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDialog2Val)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CDialog2Val)
	virtual void OnOK();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DIALOG2VAL_H__6910777B_3026_11D5_879D_0050043AFEB2__INCLUDED_)
