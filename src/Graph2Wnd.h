// Graph2Wnd.h : header file
//
#include "GraphWnd.h"

/////////////////////////////////////////////////////////////////////////////
// CGraph2Wnd frame

class CGraph2Wnd : public CGraphWnd
{
	
public:
	CGraph2Wnd(long  * ,long * , int , char * ,char * , char * =NULL);           // protected constructor used by dynamic creation

// Attributes
public:
CPen	PenCourbe2;
long *	GraphPoint2;
// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CGraph2Wnd)
	//}}AFX_VIRTUAL

// Implementation
	virtual ~CGraph2Wnd();

protected:
	// Generated message map functions
	//{{AFX_MSG(CGraph2Wnd)
	afx_msg void OnCoupeSauver();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	virtual void InitialiserGraphique();
	float Get2Y(int );
	virtual void TraceCourbe(CPaintDC *);
};

/////////////////////////////////////////////////////////////////////////////
