#if !defined(AFX_DIALCHOIXCLASS_H__352A3651_C550_11D1_A4D2_00A0242E2BA0__INCLUDED_)
#define AFX_DIALCHOIXCLASS_H__352A3651_C550_11D1_A4D2_00A0242E2BA0__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// DialChoixClass.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CDialChoixClass dialog

class CDialChoixClass : public CDialog
{
	BYTE Max;
// Construction
public:
	CDialChoixClass(CWnd* pParent,BYTE,BYTE,BYTE,double initprobc1=1.0,double initprobc2=1.0);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CDialChoixClass)
	enum { IDD = IDD_CHOIXCLASS };
	CSpinButtonCtrl	m_spin2;
	CSpinButtonCtrl	m_spin1;
	BYTE	m_class1;
	BYTE	m_class2;
	double	m_probc1;
	double	m_probc2;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDialChoixClass)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CDialChoixClass)
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DIALCHOIXCLASS_H__352A3651_C550_11D1_A4D2_00A0242E2BA0__INCLUDED_)
