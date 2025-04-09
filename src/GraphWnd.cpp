// GraphWnd.cpp : implementation file
//

#include "stdafx.h"
#include "Decid.h"
#include "GraphWnd.h"
#include "MainFrm.h"
#include "math.h"
/////////////////////////////////////////////////////////////////////////////
// CGraphWnd

IMPLEMENT_DYNCREATE(CGraphWnd, CMDIChildWnd)

BEGIN_MESSAGE_MAP(CGraphWnd, CMDIChildWnd)
	//{{AFX_MSG_MAP(CGraphWnd)
	ON_WM_PAINT()
//	ON_COMMAND(ID_COUPE_EXIT, OnCoupeExit)
//	ON_COMMAND(ID_COUPE_OPTIONS_CURSEUR, OnAfficheCurseur)
//	ON_COMMAND(ID_COUPE_OPTIONS_LISSAGE1, OnLisseCoupe1)
//	ON_COMMAND(ID_COUPE_OPTIONS_LISSAGE2, OnLisseCoupe2)    
//	ON_COMMAND(ID_INFORMATION, OnInformation)
	ON_WM_KEYDOWN()
	ON_WM_SYSKEYDOWN()
//	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_WM_DESTROY()
	ON_WM_CREATE()
	ON_WM_SIZE()
//	ON_COMMAND(ID_COUPE_SAUVER, OnCoupeSauver)
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONDOWN()
	ON_WM_RBUTTONDOWN()
//	ON_COMMAND(ID_OPTIONS_ZOOM, OnOptionsZoom)
//	ON_UPDATE_COMMAND_UI(ID_OPTIONS_ZOOM, OnUpdateOptionsZoom)
//	ON_UPDATE_COMMAND_UI(ID_COUPE_OPTIONS_CURSEUR, OnUpdateCoupeOptionsCurseur)
	ON_WM_CHILDACTIVATE()
	ON_WM_LBUTTONUP()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CGraphWnd::CGraphWnd()
{
}

CGraphWnd::CGraphWnd(int  * InitTab, int Nb, char * InitChaineX,char * InitChaineY, char * InitChaineInfo, BOOL InitGraphProfil)
{
    MIN_MAX_AUTO = TRUE;
	ChaineX = InitChaineX;
	ChaineY = InitChaineY;   
	ChaineInfo = InitChaineInfo;
	NbPoint = Nb; 
	Tab =  InitTab;  
	LeType = TInt;
	GraphProfil = InitGraphProfil;
	Initialiser();     
}

CGraphWnd::CGraphWnd(long  * InitTab, int Nb, char * InitChaineX,char * InitChaineY, char * InitChaineInfo, BOOL InitGraphProfil)
{  
    MIN_MAX_AUTO = TRUE;
	ChaineX = InitChaineX;
	ChaineY = InitChaineY;   
	ChaineInfo = InitChaineInfo;
	NbPoint = Nb; 
	Tab =  InitTab; 
	LeType = TLong; 
	GraphProfil = InitGraphProfil;
	Initialiser(); 
}

CGraphWnd::CGraphWnd(float  * InitTab, int Nb, char * InitChaineX,char * InitChaineY, char * InitChaineInfo, BOOL InitGraphProfil)
{  
    MIN_MAX_AUTO = TRUE;
	ChaineX = InitChaineX;
	ChaineY = InitChaineY;   
	ChaineInfo = InitChaineInfo;
	NbPoint = Nb; 
	Tab =  InitTab; 
	LeType = TFloat; 
	GraphProfil = InitGraphProfil;
	Initialiser(); 
}

CGraphWnd::~CGraphWnd()
{
	delete GraphPoint;    
	PenAxe.DeleteObject();
	PenCourbe.DeleteObject(); 	
	PenCurseur.DeleteObject();
	PenZoom.DeleteObject();   
}




/////////////////////////////////////////////////////////////////////////////
// CGraphWnd message handlers

BOOL CGraphWnd::Create(LPCTSTR lpszWindowName, CMDIFrameWnd* pParentWnd, const RECT& rect, DWORD dwStyle, CCreateContext* pContext) 
{
	// TODO: Add your specialized code here and/or call the base class
	 
	CRect rect1(0,0,300+8,200+27+HautWndInfo);
/*
	if (menu.m_hMenu==NULL && GraphProfil) menu.LoadMenu(IDR_COUPE);
	else if (menu.m_hMenu==NULL && !GraphProfil) menu.LoadMenu(IDR_HISTO);
	m_hMenuShared = menu.m_hMenu;
*/

	BOOL LeBool = CMDIChildWnd::Create(
		AfxRegisterWndClass( CS_HREDRAW | CS_VREDRAW | CS_BYTEALIGNWINDOW | CS_SAVEBITS,NULL,(HBRUSH)(COLOR_WINDOW+1)/*(HBRUSH)(COLOR_BACKGROUND)*/,NULL)
		,lpszWindowName, dwStyle, rect1, pParentWnd, pContext);

	GetClientRect (rcClient); 
	if (ChaineInfo!=NULL) AfficheInfo1(ChaineInfo);
	return LeBool;
}

int CGraphWnd::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CMDIChildWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	// Create an array for status bar indicators
	UINT pIndicators[2] = {ID_SEPARATOR, ID_SEPARATOR};
/*
	if (!m_wndStatusBarCoupe.Create(this, 
		WS_CHILD | WS_VISIBLE | CBRS_BOTTOM , ID_COUPE_STATUS_BAR) ||
		!m_wndStatusBarCoupe.SetIndicators(pIndicators,
		2))
		TRACE0("Failed to create Status Bar\n");
*/
  Invalidate(TRUE);
	
	m_wndStatusBarCoupe.GetStatusBarCtrl().ModifyStyle(SBARS_SIZEGRIP,CCS_NORESIZE,SWP_SHOWWINDOW);

	UINT nID, nStyle;
	int nWidth;
	m_wndStatusBarCoupe.GetPaneInfo(0, nID, nStyle, nWidth);
	m_wndStatusBarCoupe.SetPaneInfo(0, nID, nStyle, nWidth/2);

	bar.Create(this);
	bar.m_bAutoDelete=0;
//	bar.LoadToolBar(IDR_GRAPH);
	// TODO: Remove this if you don't want tool tips or a resizeable toolbar
	bar.SetBarStyle(bar.GetBarStyle() |
		CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC);
	bar.OnUpdateCmdUI( (CFrameWnd*) GetMDIFrame(), FALSE);

	return 0;
}

void CGraphWnd::Initialiser()
{
	HautWndInfo = 32;

	PenCurseur.CreatePen(PS_DASHDOT, 1, RGB(20,150,0));
	PenAxe.CreatePen (PS_SOLID, 1, RGB (0,0,0));
	PenCourbe.CreatePen (PS_SOLID, 1, RGB (255,0,0));
	PenZoom.CreatePen(PS_DASHDOTDOT  ,1,RGB(150,150,150));
	LeCurseur = FALSE;
	LeLissage = FALSE;
	AvSouris =FALSE;
	ZOOM =FALSE;
	PremZoom=TRUE;
	AppuieZoom=FALSE;
	ZonZoomSelected=FALSE;
	AlloueMemoire();  
}




void CGraphWnd::SetValeurTab(int * p)
{
	for (int i=0;i<NbPoint;i++) ((int *)GraphPoint)[i]= p[i]; 	
	if (MIN_MAX_AUTO)
	{
		InitialiserGraphique();
	}
	ParametreGraphique();
}

 
 
void CGraphWnd:: SetValeurTab(long * p)
{
	for (int i=0;i<NbPoint;i++) ((long *)GraphPoint)[i]= p[i];  
	if (MIN_MAX_AUTO)
	{
		InitialiserGraphique();
	}
	ParametreGraphique();
}

void CGraphWnd:: SetValeurTab(float * p)
{
	for (int i=0;i<NbPoint;i++) ((float *)GraphPoint)[i]= p[i];  
	if (MIN_MAX_AUTO)
	{
		InitialiserGraphique();
	}
	ParametreGraphique();
}

 
 
void CGraphWnd::AlloueMemoire()
{ 
  switch (LeType)
  {   
  	case TInt : { 
								  int * p = (int *) Tab;
									GraphPoint =  (int *) new int[NbPoint*sizeof(int)];     
									SetValeurTab(p);    
									
									break;
								}	
  	case TLong: { 
								  long * p = (long *) Tab;
									GraphPoint = (long * ) new long[NbPoint*sizeof(long)]; 
									SetValeurTab(p);
	                break;
  							}   
  	case TFloat: { 
								  	float * p = (float *) Tab;
									GraphPoint = (float * ) new float[NbPoint*sizeof(float)]; 
									SetValeurTab(p);
	                break;
  							}   
  							        
  }                                 

}

float CGraphWnd::GetY(int i)
{       
	float V;     
	switch (LeType)
	{   
		case TInt :  		V =  float (((int *)GraphPoint)[i]); break;
		case TLong:    	V =  float (((long *)GraphPoint)[i]); break;     
		case TFloat:    V =  float (((float *)GraphPoint)[i]); break;     
	}
	return V;
}



void CGraphWnd::SetY(int i, float V)
{    
	switch (LeType)
	{
		case TInt 		: ((int *) GraphPoint)[i] = (int) V; break;
		case TLong		: ((long *) GraphPoint)[i] = (long) V; break;
		case TFloat		: ((float *) GraphPoint)[i] = (float)V; break;
	}	
}


void CGraphWnd::InitialiserGraphique()
{ 
	MinY = GetY(0);  
	MaxY = GetY(0);
	for (int i=1;i<NbPoint;i++)
	{
		if (GetY(i)<MinY) MinY = GetY(i);	
		if (GetY(i)>MaxY) MaxY = GetY(i);		
	} 
	if (MinY==MaxY) { MinY-=2; MaxY+=2;}
}	


	 
void CGraphWnd::ParametreGraphique()
{
	int decalage = 30+HautWndInfo; 
	if (this->m_hWnd!=0) GetClientRect(rcClient);
	WGrD = rcClient.Width()-decalage;   
	WGrG = decalage;    
	HGrB = rcClient.Height()-decalage;   
	HGrH = decalage;                     
	Ay = (float)(HGrB-HGrH)/(float)(MinY-MaxY); 
	By = (float)HGrB - Ay*(float)MinY;
		  
	Bx = (float)WGrG;
	Ax = (float)(WGrD-Bx)/(NbPoint-1);
	if(MIN_MAX_AUTO)rcInflate = CRect(0,HautWndInfo,rcClient.Width(),rcClient.Height()-20);
	else rcInflate = CRect(PosGrX(0),PosGrY(MaxY),PosGrX(NbPoint-1)+1,PosGrY(MinY)+1);
}


int CGraphWnd::PosGrX(long X)
{
	return (int) (Ax*X+Bx);
}


int CGraphWnd::PosGrY(float Y)
{
	return (int) (Ay*Y+By);
}

void CGraphWnd::OnDestroy() 
{
	CMDIChildWnd::OnDestroy();
	// TODO: Add your message handler code here
	if (GetMDIFrame()->MDIGetActive() == NULL)
	{
		CMenu toto;
		toto.LoadMenu(IDR_MAINFRAME);
		GetMDIFrame()->MDISetMenu(&toto,NULL);
		AfxGetMainWnd()->DrawMenuBar();
	}
}

void CGraphWnd::TraceCourbe(CPaintDC * dc)
{
	dc->SelectObject(&PenCourbe);                          
	dc->MoveTo(PosGrX(0),PosGrY(GetY(0)));	                     
	for (int i=1;i<NbPoint;i++) dc->LineTo(PosGrX(i),PosGrY(GetY(i)));
}

void CGraphWnd::TraceAxe(CPaintDC * dc)
{
	dc->SelectObject(&PenAxe);                          
	// axe vertical
	dc->MoveTo(PosGrX(0),PosGrY(MinY));	                     
	dc->LineTo(PosGrX(0),PosGrY(MaxY));
	// axe horizontal
	if(MinY<0) 
	{
		dc->MoveTo(PosGrX(0),PosGrY(0));	//dc->MoveTo(PosGrX(0),PosGrY(MinY));	                     
		dc->LineTo(PosGrX(NbPoint-1),PosGrY(0)); //dc->LineTo(PosGrX(NbPoint-1),PosGrY(MinY));
	}
	else 
	{
		dc->MoveTo(PosGrX(0),PosGrY(MinY));
		dc->LineTo(PosGrX(NbPoint-1),PosGrY(MinY));
	}
	TracerGraduation(dc);
	EcrireCommentaire(dc);
}



afx_msg void CGraphWnd::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	
	int OldBkMode = dc.SetBkMode(TRANSPARENT);
	if (MIN_MAX_AUTO)
	{	InitialiserGraphique();
	}
	ParametreGraphique();
	TraceAxe(&dc);
	TraceCourbe(&dc);
	AfficheDimCoupe();
	if (LeLissage) LeLissage = FALSE;
	ReleaseDC(&dc);
	dc.SetBkMode(OldBkMode);
	if(ZonZoomSelected)DessineZonZOOM(CPoint(PosGrX(XZ2),PosGrY(YZ2)));
	if(LeCurseur)AffichCurseur();
}

void CGraphWnd::TracerGraduation(CPaintDC * dc)
{            
	int NbEtiX = (WGrD - WGrG)/30;
	int NbEtiY = (HGrB - HGrH)/30; 
	if ((NbEtiX!=0) && (NbEtiY!=0))
	{ 
		int x,y;
		char echelle[30];
			  
		// horizontal   
		if(MinY<0) y = PosGrY(0);//PosGrY(MinY);
		else y = PosGrY(MinY);
		for (int i=0; i<=NbEtiX;i++)	 
		{ 
			x = PosGrX(i*(NbPoint-1)/NbEtiX); 
			dc->MoveTo(x,y-3);
			dc->LineTo(x,y+3);
			itoa((int)(i*(NbPoint-1)/NbEtiX),echelle,10);
			dc->TextOut(x-5,y+10,echelle);		
		}        
			
		// vertical   
		x = PosGrX(0); 
		for (i=0; i<=NbEtiY;i++)	 
		{ 
			y = PosGrY(MinY+i*(MaxY-MinY)/NbEtiY);	
			dc->MoveTo(x-3,y);
			dc->LineTo(x+3,y);	 
			switch (LeType)
			{
				case TInt :		itoa ((int)(MinY+i*(MaxY-MinY)/NbEtiY),echelle,10);break; 
				case TLong :	ltoa ((long)(MinY+i*(MaxY-MinY)/NbEtiY),echelle,10);break;  
				case TFloat : sprintf(echelle,"%3.1f",(float)(MinY+i*(MaxY-MinY)/NbEtiY));break;
			}	
			dc->TextOut(x-30,y-5,echelle);		
		}
	}
}

void CGraphWnd::EcrireCommentaire(CPaintDC * dc)
{
	dc->TextOut(PosGrX(0)-10,PosGrY(MaxY)-20,ChaineX);	
	dc->TextOut(PosGrX(NbPoint),PosGrY(MinY) - 20,ChaineY);				
}

void CGraphWnd::ChaineAfficher(char * ch,int PosCour)
{ 
	float V = GetY(PosCour);                          
	switch (LeType)
	{
		case TFloat :	sprintf(ch,"X = %d   Y = %3.1f", PosCour,(float) V);  break;
		case TInt 	:	sprintf(ch,"X = %d   Y = %d", PosCour,(int) V); 	break;
		case TLong 	:	sprintf(ch,"X = %d   Y = %ld", PosCour,(long) V);	break;
	}
}

                                                     
void CGraphWnd::BougerCurseur(int posinc)
{
	InvalidateRect(rcInflate,TRUE);
	UpdateWindow();    
	AffichCurseur();
	PosCur+=posinc;  
}

void CGraphWnd::AffichCurseur()
{
	CClientDC dc(this);   
	dc.SelectObject(&PenCurseur);
	if (PosCur<0) PosCur = 0;
	if (PosCur>NbPoint-1) PosCur = NbPoint-1;

	if(PosCur>0)
	{
		dc.MoveTo(PosGrX(PosCur),PosGrY(MinY));
		dc.LineTo(PosGrX(PosCur), PosGrY(MaxY));
	}
	if(GetY(PosCur)>=MinY&&GetY(PosCur)<=MaxY)
	{
		dc.MoveTo(PosGrX(0),PosGrY(GetY(PosCur)));
		dc.LineTo(PosGrX(NbPoint-1),PosGrY(GetY(PosCur)));  
	}
	char Chaine[100];
	ChaineAfficher(Chaine,PosCur);
	AfficheInfo1(Chaine);
	ReleaseDC(&dc);

}

void CGraphWnd::AfficheInfo2(char * chaine)
{
	//pInfoWnd1->SendMessage(Wm_msg,0,(LONG)(LPSTR)chaine);
	m_wndStatusBarCoupe.SetPaneText(0, chaine);
}

void CGraphWnd::AfficheInfo1(char * chaine)
{
	//pInfoWnd2->SendMessage(Wm_msg,0,(LONG)(LPSTR)chaine);
	m_wndStatusBarCoupe.SetPaneText(1, chaine);
}

void CGraphWnd::AfficheDimCoupe()
{
}

void CGraphWnd::OnCoupeSauver()
{
	FILE *Fichier;
	CFileDialog FileData(FALSE,"*.xlc",NULL,OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,"EXCEL (*.xlc)|*.xlc||",this);
	if (FileData.DoModal()==IDOK)
	{
		if (Fichier=fopen(FileData.GetPathName(),"w"))
		{
			SetCursor(LoadCursor(0,IDC_WAIT));
			for (int i=0;i<(int)NbPoint;i++)
			{       
				if (LeType == TFloat)	fprintf(Fichier,"%d %3.1f\n",i, (float)GetY(i));
				if (LeType == TInt)	fprintf(Fichier,"%d %d\n",i, (int)GetY(i));
				if (LeType == TLong)	fprintf(Fichier,"%d %ld\n",i, (long)GetY(i));
			}
			fclose(Fichier);
			SetCursor(LoadCursor(0,IDC_ARROW));
		}
	}   
}


afx_msg void CGraphWnd::OnAfficheCurseur()
{
	if(LeCurseur)
	{
		LeCurseur=FALSE;
		AvSouris=FALSE;
		Invalidate();
	}
	else
	{
		LeCurseur = TRUE;
		AvSouris =FALSE;
		PosCur = 0;
		AffichCurseur();
	}
}
void CGraphWnd::OnUpdateCoupeOptionsCurseur(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(LeCurseur);	
}

afx_msg void CGraphWnd::OnInformation()
{   
	float moy = 0.0F,sig=0.0F,n,Sig,Moy;   
	char chaine[100];
	for(int i=0;i<NbPoint;i++)
	{   
		n = (float) GetY(i);
		moy += n; 
		sig += n*n;
	}
	Moy = (float) moy / (float)NbPoint;
	Sig = (float) sig / (float)NbPoint;
	Sig = (float) sqrt((double)(Sig-Moy*Moy));
	sprintf(chaine,"Niveau moyen : %3.1f\n écart type : %3.2f\n variance : %3.2f",Moy,Sig,Sig*Sig);    
	MessageBox(chaine,"Informations statistiques");
}

afx_msg void CGraphWnd::OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (LeCurseur)
	{
	if((nChar==67)||(nChar==99))  // Lettre 'c' ou 'C' pour enlever le curseur
	{
		::SetFocus(hOldFocus);
		AfficheInfo1("");
		AfficheInfo2("");
		InvalidateRect(rcInflate,TRUE);
	}
		
	if(nChar==37)   //Fleche à gauche <-                    
	if (PosCur>=1)	BougerCurseur((int)-1);
	if(nChar==39)   //Fleche à droite ->
	if (PosCur<NbPoint-1)		BougerCurseur((int)1);
	}	
}



afx_msg void CGraphWnd::OnSysKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags)
{
 if (LeCurseur)
 {
	if(nChar==37)   //Fleche à gauche <-
		if (PosCur>10)		BougerCurseur(-10);
	
	if(nChar==39)   //Fleche à gauche ->
		if (PosCur<NbPoint-10)		BougerCurseur(10);
 }	
}

afx_msg void CGraphWnd::OnLisseCoupe1()
{
	int i;
	float temp1,temp2;
	LeLissage = TRUE;
	temp1 = (float)(GetY(0)+GetY(1)+GetY(2))/3;
	for(i=2;i<NbPoint;i++)
	{
		temp2 = (float)(GetY(i-1)+GetY(i)+GetY(i+1))/3;
		SetY(i-1,temp1);
		temp1 = temp2;
	}
	InvalidateRect(rcInflate,TRUE);
}


afx_msg void CGraphWnd::OnLisseCoupe2()
{
	int i;
	float temp1,temp2;
	LeLissage = TRUE;
	temp1 = (float)(GetY(0)+2*GetY(1)+GetY(2))/4;
	for(i=1;i<NbPoint;i++)
	{
		temp2 = (float)(GetY(i-1)+2*GetY(i)+GetY(i+1))/4;
		SetY(i-1,temp1);
		temp1 = temp2;
	}
	InvalidateRect(rcInflate,TRUE);
}

afx_msg void CGraphWnd::OnSize(UINT nType, int cx, int cy) 
{
	if (this->m_hWnd) GetClientRect (rcClient); 
	if ((rcClient.Height()<250) || (rcClient.Width()<250))	
	{ 	
		SetWindowPos(NULL ,0,0,300,300,SWP_NOMOVE );       // SWP_NOMOVE        
		cx = 300;cy = 300;
	
	}

	UINT nID, nStyle;
	int nWidth;
	m_wndStatusBarCoupe.GetPaneInfo(0, nID, nStyle, nWidth);
	m_wndStatusBarCoupe.SetPaneInfo(0, nID, nStyle, cx/2);

	CMDIChildWnd::OnSize(nType, cx, cy);
	
	// TODO: Add your message handler code here
}

afx_msg void CGraphWnd::OnCoupeExit()
{
	MDIDestroy();
}

afx_msg void CGraphWnd::OnEditCopy() 
{
	HBITMAP bitm;
	HDC hMemDC;
    CClientDC dc(this);
	VERIFY(::OpenClipboard(GetSafeHwnd()));

	//HDC hDC = CreateDC("DISPLAY",NULL,NULL,NULL);
	bitm = CreateCompatibleBitmap(dc.GetSafeHdc(),rcClient.Width(),rcClient.Height()-2*HautWndInfo);
	hMemDC 	= CreateCompatibleDC(dc.GetSafeHdc());
	SelectObject(hMemDC, bitm);                 
	BitBlt(hMemDC,0,0,rcClient.Width(),rcClient.Height()-2*HautWndInfo,dc.GetSafeHdc(),0,HautWndInfo,SRCCOPY);
	
    VERIFY(::EmptyClipboard());
    VERIFY(::SetClipboardData(CF_BITMAP,bitm));
    VERIFY(::CloseClipboard());

	DeleteDC(hMemDC);
	//DeleteDC(hDC);
	DeleteObject(bitm);  
	ReleaseDC(&dc); // le dc est relaché  
}





void CGraphWnd::OnMouseMove(UINT nFlags, CPoint point) 
{
	if(ZOOM)
	{
		int pos=(int)((float)point.x/Ax-Bx/Ax);
		CString Chtot;
		if(!ZonZoomSelected)
		{
//			SetCursor(LoadCursor(AfxGetInstanceHandle( ),MAKEINTRESOURCE(IDC_RECT)));
			if(!AppuieZoom)
			{
				Chtot.Format("x=%d",pos);
				m_wndStatusBarCoupe.SetPaneText(1, Chtot);
			}
		}
		
		if(AppuieZoom)
		{
			CPoint pt=point;
			if(pt.x<PosGrX(0)) pt.x=PosGrX(0);
			if(pt.x>PosGrX(NbPoint-1)+1) pt.x=PosGrX(NbPoint-1)+1;
			if(pt.y>PosGrY(MinY)+1) pt.y=PosGrY(MinY)+1;
			if(pt.y<PosGrY(MaxY)) pt.y=PosGrY(MaxY);
			AfficheZonZoom(pt);
			pos=(int)((float)pt.x/Ax-Bx/Ax);
			Chtot.Format("x=%d à x=%d",XZ1,pos);
			m_wndStatusBarCoupe.SetPaneText(1, Chtot);
		}
	}

	if (LeCurseur && AvSouris)
	{
		SetCursor(LoadCursor(0,IDC_UPARROW));
		PosCur=(int)((float)point.x/Ax-Bx/Ax);
		BougerCurseur(0);
	}	
	CMDIChildWnd::OnMouseMove(nFlags, point);
}

void CGraphWnd::OnLButtonDown(UINT nFlags, CPoint point) 
{
	if(ZOOM&&!ZonZoomSelected)
	{
		AppuieZoom=true;
		
		if(PremZoom)
		{
			PremZoom=FALSE;
			XZ1=(long)((float)point.x/Ax-Bx/Ax);
			YZ1=(float)((float)point.y/Ay-By/Ay);
			if(XZ1<0) XZ1=0;
			if(XZ1>NbPoint-1) XZ1=NbPoint-1;
			if(YZ1<MinY) YZ1=MinY;
			if(YZ1>MaxY) YZ1=MaxY;
		}
	}
	CMDIChildWnd::OnLButtonDown(nFlags, point);
}

void CGraphWnd::OnLButtonUp(UINT nFlags, CPoint point) 
{
	// TODO: Add your message handler code here and/or call default
	if(ZOOM&&!ZonZoomSelected)
	{
		int TamponX;
		float TamponY;
		AppuieZoom=false;
		PremZoom=TRUE;
		ZonZoomSelected=TRUE;
		XZ2=(long)((float)point.x/Ax-Bx/Ax);
		YZ2=(float)((float)point.y/Ay-By/Ay);
		if(XZ2<0) XZ2=0;
		if(XZ2>NbPoint-1) XZ2=NbPoint-1;
		if(YZ2<MinY) YZ2=MinY;
		if(YZ2>MaxY) YZ2=MaxY;
	
		if(XZ1>XZ2)
		{
			TamponX=XZ1;
			XZ1=XZ2;
			XZ2=TamponX;
			
		}

		if(YZ1<YZ2)
		{
			TamponY=YZ1;
			YZ1=YZ2;
			YZ2=TamponY;
		}

		
		while(GetY(XZ1)<YZ2 && XZ1<XZ2)
		{
			XZ1++;
		}
		while(GetY(XZ2)<YZ2 && XZ2>XZ1)
		{
			XZ2--;
		}
		
		if(XZ1==XZ2)
		{
			MessageBox("Impossible de créer le zoom!");
			OnOptionsZoom();
		}
		else
		{
			CString ch;
			CGraphWnd * Zoom;
			switch (LeType)
				{
					case TInt :		Zoom=(CGraphWnd*) new CGraphWnd(((int*)GraphPoint+XZ1),XZ2-XZ1+1,"Position","Niveau");break; 
					case TLong :	Zoom=(CGraphWnd*) new CGraphWnd(((long*)GraphPoint+XZ1),XZ2-XZ1+1,"Position","Niveau");break;  
					case TFloat :	Zoom=(CGraphWnd*) new CGraphWnd(((float*)GraphPoint+XZ1),XZ2-XZ1+1,"Position","Niveau");break;
				}
			Zoom->MIN_MAX_AUTO=FALSE;
			Zoom->MinY=YZ2;
			Zoom->MaxY=YZ1;
			switch (LeType)
				{
					case TInt :		Zoom->SetValeurTab((int*)GraphPoint+XZ1);break; 
					case TLong :	Zoom->SetValeurTab((long*)GraphPoint+XZ1);break;  
					case TFloat :	Zoom->SetValeurTab((float*)GraphPoint+XZ1);break;
				}
			ch.Format(" de (%d,%3.1f)-(%d,%3.1f)",XZ1,YZ1,XZ2,YZ2);
			Zoom->Create("Zoom"+ch,GetMDIFrame());
		}

	}
	Invalidate();
	SetCursor(LoadCursor(NULL,IDC_ARROW));
	CMDIChildWnd::OnLButtonUp(nFlags, point);
}

void CGraphWnd::OnRButtonDown(UINT nFlags, CPoint point) 
{
	if(AvSouris)
	{
		SetCursor(LoadCursor(0,IDC_ARROW));
		AvSouris=FALSE;	
	}
	else
	{
		
		if (LeCurseur)
		{
			AvSouris=TRUE;
			SetCursor(LoadCursor(0,IDC_UPARROW));
			PosCur=(int)((float)point.x/Ax-Bx/Ax);
			BougerCurseur(0);
		}
	}
	CMDIChildWnd::OnRButtonDown(nFlags, point);
}

void CGraphWnd::OnOptionsZoom() 
{
	if(ZOOM)
	{
		ZOOM=FALSE;
		ZonZoomSelected=FALSE;
		Invalidate();
	}
	else
	{
		ZOOM=TRUE;
		AvSouris=FALSE;
	}


	
}

void CGraphWnd::OnUpdateOptionsZoom(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(ZOOM);
}




void CGraphWnd::AfficheZonZoom(CPoint point)
{
	InvalidateRect(rcInflate,TRUE);
	UpdateWindow();    
	
	DessineZonZOOM(point);
}

void CGraphWnd::DessineZonZOOM(CPoint point)
{
	CClientDC dc(this);   
	dc.SelectStockObject(NULL_BRUSH);
	dc.SelectObject(&PenZoom);

	dc.Rectangle(CRect(PosGrX(XZ1),PosGrY(YZ1),point.x,point.y));  
	ReleaseDC(&dc);
}

