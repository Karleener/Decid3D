#if !defined(AFX_COURBE_H__4DFA3D81_CCD5_440A_9E20_534BB17E793A__INCLUDED_)
#define AFX_COURBE_H__4DFA3D81_CCD5_440A_9E20_534BB17E793A__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// Courbe.h : header file
//
#include <vector>
using namespace std;

/////////////////////////////////////////////////////////////////////////////
// CCourbe frame

class CCourbe : public CFrameWnd
{
	DECLARE_DYNCREATE(CCourbe)
public:
	CCourbe();           // protected constructor used by dynamic creation
	vector<double>m_x;
	vector<double>m_y;
	vector<double>m_z;
// Attributes
public:

// Operations
public:

//	void Dessine();

	int CCourbe::Dessine(const vector<double>& x, const vector<double>& y, const vector<double>& z, int limit);
	double imin, imax; bool minmaxdefined;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CCourbe)
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CCourbe();

	// Generated message map functions
	//{{AFX_MSG(CCourbe)
	afx_msg void OnPaint();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_COURBE_H__4DFA3D81_CCD5_440A_9E20_534BB17E793A__INCLUDED_)
