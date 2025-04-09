#if !defined(AFX_DIALPARAMBOOST_H__7EFD38F1_6D74_4CA8_B2ED_7280755CEC14__INCLUDED_)
#define AFX_DIALPARAMBOOST_H__7EFD38F1_6D74_4CA8_B2ED_7280755CEC14__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// DialParamBoost.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CDialParamBoost dialog

class CDialParamBoost : public CDialog
{
// Construction
public:
	CDialParamBoost(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CDialParamBoost)
	enum { IDD = IDD_DIALOG2 };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDialParamBoost)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CDialParamBoost)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DIALPARAMBOOST_H__7EFD38F1_6D74_4CA8_B2ED_7280755CEC14__INCLUDED_)
