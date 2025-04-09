#if !defined(AFX_DIALITER_H__D95F2F77_0211_11D7_87C9_0050043AFEB2__INCLUDED_)
#define AFX_DIALITER_H__D95F2F77_0211_11D7_87C9_0050043AFEB2__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// Dialiter.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CDialiter dialog

class CDialiter : public CDialog
{
// Construction
public:
	CDialiter(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CDialiter)
	enum { IDD = IDD_DIALOG_ITER };
	long	m_Binf;
	long	m_Bsup;
	long	m_Iter;
	float	m_Erreur;
	int		m_Coef;
	int		m_ptaleat;
	int		m_bruit;
	int		m_NbSlice;
	BOOL	m_Courbe;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDialiter)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CDialiter)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DIALITER_H__D95F2F77_0211_11D7_87C9_0050043AFEB2__INCLUDED_)
