#if !defined(AFX_DIALBOOST_H__DC365C80_49A8_4972_8BDD_F20EAF68BDF3__INCLUDED_)
#define AFX_DIALBOOST_H__DC365C80_49A8_4972_8BDD_F20EAF68BDF3__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// DialBoost.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CDialBoost dialog

class CDialBoost : public CDialog
{
// Construction
public:
	CDialBoost(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CDialBoost)
	enum { IDD = IDD_DIALOG2 };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDialBoost)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CDialBoost)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DIALBOOST_H__DC365C80_49A8_4972_8BDD_F20EAF68BDF3__INCLUDED_)
