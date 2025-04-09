#if !defined(AFX_DIALCONFIGPOLY_H__3C7412F2_B72D_11D1_A4BE_00A0242E2BA0__INCLUDED_)
#define AFX_DIALCONFIGPOLY_H__3C7412F2_B72D_11D1_A4BE_00A0242E2BA0__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// DialConfigPoly.h : header file
//
#include "TreeConfig.h"

/////////////////////////////////////////////////////////////////////////////
// CDialConfigPoly dialog

class CDialConfigPoly : public CDialog
{
// Construction
public:
	CString NomFich;

public:
	CDialConfigPoly(CWnd* pParent = NULL,CString initFich="");   // standard constructor
	CImageList *imageTree;
// Dialog Data
	//{{AFX_DATA(CDialConfigPoly)
	enum { IDD = IDD_CONFIGPOLY };
	CButton	m_cmc;
	CButton	m_sauve;
	CButton	m_ajoutclass;
	CButton	m_ajoutfich;
	CButton	m_supprimclass;
	CTreeConfig	m_tree;
	int		m_Prop;
	BOOL	m_Inverse;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDialConfigPoly)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CDialConfigPoly)
	virtual BOOL OnInitDialog();
	afx_msg void OnItemexpandingTree1(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnNommmc();
	afx_msg void OnAjouterclas();
	afx_msg void OnAjouterfich();
	afx_msg void OnSupprimclas();
	afx_msg void OnSelchangedTree1(NMHDR* pNMHDR, LRESULT* pResult);
	virtual void OnOK();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
	BOOL m_Random;
	afx_msg void OnBnClickedOk();
	BOOL m_FirstHalf;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DIALCONFIGPOLY_H__3C7412F2_B72D_11D1_A4BE_00A0242E2BA0__INCLUDED_)
