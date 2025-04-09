#ifndef __GraphWnd_H__           
#define __GraphWnd_H__
// GraphWnd.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CGraphWnd frame
enum Type {TInt, TLong, TFloat};

class CGraphWnd : public CMDIChildWnd
{
	DECLARE_DYNCREATE(CGraphWnd)
protected:
	CGraphWnd();           // protected constructor used by dynamic creation
public:
	CGraphWnd(int * InitTab, int Nb, char * InitChaineX,char * InitChaineY, char *  InitChaineInfo = NULL, BOOL InitGraphProfil = FALSE);
	CGraphWnd(long * InitTab, int Nb, char * InitChaineX,char * InitChaineY, char *  InitChaineInfo = NULL, BOOL InitGraphProfil = FALSE);
	CGraphWnd(float * InitTab, int Nb, char * InitChaineX,char * InitChaineY, char *  InitChaineInfo = NULL, BOOL InitGraphProfil = FALSE);
// Attributes
protected:
	CToolBar bar;
	CStatusBar  m_wndStatusBarCoupe;
	Type LeType;
	void 	* GraphPoint; //(x,ng)   
	void 	* Tab;  
	BOOL First;
	BOOL GraphProfil;
public:
	BOOL MIN_MAX_AUTO;

	
	public:
	void AlloueMemoire();
	float GetY(int);       
	void SetY(int i, float V);
		
	protected:
	CMenu menu;     // menu for all COUPE windows
	int NbPoint;
	CRect rcClient;
	char * ChaineInfo;
	public:
	CRect rcInflate;
	
	  
	protected:  
	CWnd * FeneAppel;
	//CInfoWnd * pInfoWnd1;
	//CInfoWnd * pInfoWnd2;
	char * 	ChaineX, * ChaineY;
	int WGrD,WGrG,HGrB,HGrH;                   
	  
	// Variables servant pour l'affichage du curseur
	int PosCur;
	BOOL LeCurseur,LeLissage,AvSouris,ZOOM;
	HWND hOldFocus;
	
	//Variables pour le ZOOM
	long XZ1,XZ2;
	float YZ1,YZ2;
	BOOL AppuieZoom,PremZoom,ZonZoomSelected;

	
	float Ax,Ay,Bx,By;
	  
	CPen	PenAxe, PenCourbe, PenCurseur,PenZoom;
	int HautWndInfo;
	public:
	float MinY,MaxY;
// Operations
public:
	void SetValeurTab(int * InitTab);    
	void SetValeurTab(long * InitTab);
	void SetValeurTab(float * InitTab);  
protected:
	void Initialiser();   
	              
	void ChaineAfficher(char * ,int );

	void 	BougerCurseur(int);
	virtual void 	InitialiserGraphique();
	void 	ParametreGraphique();
protected:
	int 	PosGrX(long );
	int 	PosGrY(float );
	virtual void 	TraceCourbe(CPaintDC * dc);
	void 	TraceAxe(CPaintDC * dc);
	void 	TracerGraduation(CPaintDC * dc);
	void 	EcrireCommentaire(CPaintDC * dc);  
	void 	AfficheInfo1(char * chaine);
	void 	AfficheInfo2(char * chaine);
	virtual void AfficheDimCoupe();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CGraphWnd)
	public:
	virtual BOOL Create(LPCTSTR lpszWindowName,  CMDIFrameWnd* pParentWnd, const RECT& rect = rectDefault, DWORD dwStyle = WS_CHILD | WS_VISIBLE | WS_OVERLAPPEDWINDOW, CCreateContext* pContext = NULL);
	//}}AFX_VIRTUAL

// Implementation
protected:
	void AffichCurseur();
	void DessineZonZOOM(CPoint);
	void AfficheZonZoom(CPoint);
	virtual ~CGraphWnd();

	// Generated message map functions
	//{{AFX_MSG(CGraphWnd)
	afx_msg void OnPaint(void);
	afx_msg void OnCoupeExit();
	afx_msg void OnAfficheCurseur();
	afx_msg void OnLisseCoupe1();
	afx_msg void OnLisseCoupe2();
	afx_msg void OnInformation();
	afx_msg void OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnSysKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnEditCopy();
	afx_msg void OnDestroy();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnCoupeSauver();
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnOptionsZoom();
	afx_msg void OnUpdateOptionsZoom(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCoupeOptionsCurseur(CCmdUI* pCmdUI);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#endif //GraphWnd.h
/////////////////////////////////////////////////////////////////////////////
