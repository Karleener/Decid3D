#if !defined(AFX_DALIOGPROGRESSION_H__FAE4876F_0182_4477_BA48_24DF60D38D03__INCLUDED_)
#define AFX_DALIOGPROGRESSION_H__FAE4876F_0182_4477_BA48_24DF60D38D03__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// DaliogProgression.h : header file
//
#define WM_UPDATE_PROGRESS (WM_USER + 1)
/////////////////////////////////////////////////////////////////////////////
// CDaliogProgression dialog

class CDaliogProgression : public CDialog
{
// Construction
public:
	int ValeurCourante;
	short Valeur_Maxi;
	CDaliogProgression(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CDaliogProgression)
	enum { IDD = IDD_DIALOG_PROGRESS };
	CProgressCtrl	m_ControlProgressBar;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDaliogProgression)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CDaliogProgression)
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg LRESULT OnUpdateProgress(WPARAM wParam, LPARAM lParam);
public:
	void UpdateProgress(int value) {
		if (m_ControlProgressBar.GetSafeHwnd()) {
			m_ControlProgressBar.SetPos(value);
		}
	}

	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
	CString m_Affiche;
	afx_msg void OnTimer(UINT_PTR nIDEvent);
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DALIOGPROGRESSION_H__FAE4876F_0182_4477_BA48_24DF60D38D03__INCLUDED_)
