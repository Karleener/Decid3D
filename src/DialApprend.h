#if !defined(AFX_DIALAPPREND_H__B2ADAD5F_49F2_11D5_879F_0050043AFEB2__INCLUDED_)
#define AFX_DIALAPPREND_H__B2ADAD5F_49F2_11D5_879F_0050043AFEB2__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// DialApprend.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CDialApprend dialog

class CDialApprend : public CDialog
{
// Construction
public:
	int Larg;
	int Haut;
	CRect Rect;
	CClientDC *pdc;

	void DessinePoint(int iter,float val);
	CDialApprend(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CDialApprend)
	enum { IDD = IDD_DIALOG_APPREND };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDialApprend)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CDialApprend)
	virtual BOOL OnInitDialog();
	afx_msg void OnDestroy();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DIALAPPREND_H__B2ADAD5F_49F2_11D5_879F_0050043AFEB2__INCLUDED_)
