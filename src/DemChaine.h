#if !defined(AFX_DEMCHAINE_H__10AF0701_6001_11D5_879F_0050043AFEB2__INCLUDED_)
#define AFX_DEMCHAINE_H__10AF0701_6001_11D5_879F_0050043AFEB2__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// DemChaine.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CDemChaine dialog

class CDemChaine : public CDialog
{
// Construction
public:
	CDemChaine(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CDemChaine)
	enum { IDD = IDD_DIALOG1 };
	CString	m_Chaine;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDemChaine)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CDemChaine)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DEMCHAINE_H__10AF0701_6001_11D5_879F_0050043AFEB2__INCLUDED_)
