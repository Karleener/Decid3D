#if !defined(AFX_DIALBOOSTING_H__7F9A7975_E767_4139_A4B8_7C9FD9754D78__INCLUDED_)
#define AFX_DIALBOOSTING_H__7F9A7975_E767_4139_A4B8_7C9FD9754D78__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// DialBoosting.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CDialBoosting dialog

class CDialBoosting : public CDialog
{
// Construction
public:
	CString m_NomFicBoost;
	CDialBoosting(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CDialBoosting)
	enum { IDD = IDD_DIALOG_BOOST };
	int		m_Choix;
	int		m_Iter;
	BOOL	m_Bin;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDialBoosting)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CDialBoosting)
	afx_msg void OnButtonFic();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DIALBOOSTING_H__7F9A7975_E767_4139_A4B8_7C9FD9754D78__INCLUDED_)
