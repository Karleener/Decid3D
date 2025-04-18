// Decid.h : main header file for the DECID application
//

#if !defined(AFX_DECID_H__288929A1_2F5A_11D5_879D_0050043AFEB2__INCLUDED_)
#define AFX_DECID_H__288929A1_2F5A_11D5_879D_0050043AFEB2__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// CDecidApp:
// See Decid.cpp for the implementation of this class
//

class CDecidApp : public CWinApp
{
public:
	CDecidApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDecidApp)
	public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	//}}AFX_VIRTUAL

// Implementation
protected:
	HMENU m_hMDIMenu;
	HACCEL m_hMDIAccel;

public:
	//{{AFX_MSG(CDecidApp)
	afx_msg void OnAppAbout();
	afx_msg void OnFileNew();
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DECID_H__288929A1_2F5A_11D5_879D_0050043AFEB2__INCLUDED_)
