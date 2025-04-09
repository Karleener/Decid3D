// Graph2Wnd.cpp : implementation file
//

#include "stdafx.h"
#include "Decid.h"
#include "Graph2Wnd.h"
#include "GraphWnd.h"



/////////////////////////////////////////////////////////////////////////////
// CGraph2Wnd



CGraph2Wnd::CGraph2Wnd(long  * InitTablo, long * initTab2, int Nb, char * InitChaineX,char * InitChaineY, char * InitChaineInfo):CGraphWnd( InitTablo,  Nb,  InitChaineX, InitChaineY,  InitChaineInfo)
{
	PenCourbe2.CreatePen (PS_SOLID, 1, RGB (0,0,255));
	GraphPoint2 = (long * ) new long[Nb*sizeof(long)]; 
	for (long i=0;i<Nb;i++) GraphPoint2[i] = initTab2[i];
}

CGraph2Wnd::~CGraph2Wnd()
{
//	PenCourbe2.DeleteObject();
}


BEGIN_MESSAGE_MAP(CGraph2Wnd, CGraphWnd)
	//{{AFX_MSG_MAP(CGraph2Wnd)
//	ON_COMMAND(ID_COUPE_SAUVER, OnCoupeSauver)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGraph2Wnd message handlers



void CGraph2Wnd::InitialiserGraphique()
{
	CGraphWnd::InitialiserGraphique();
	for (int i=0;i<NbPoint;i++)
	{
		if (Get2Y(i)<MinY) MinY = Get2Y(i);	
		if (Get2Y(i)>MaxY) MaxY = Get2Y(i);		
	} 
	if (MinY==MaxY) { MinY-=1; MaxY+=1;}

}

float CGraph2Wnd::Get2Y(int i)
{       
	float V;     
   	V =  float (((long *)GraphPoint2)[i]); 
	return V;
}

void CGraph2Wnd::TraceCourbe(CPaintDC * dc)
{
	
	dc->SelectObject(&PenCourbe2);                          
	dc->MoveTo(PosGrX(0),PosGrY(Get2Y(0)));	                     
	for ( long i=1;i<NbPoint;i++) dc->LineTo(PosGrX(i),PosGrY(Get2Y(i)));
	CGraphWnd::TraceCourbe(dc);

}

afx_msg void CGraph2Wnd::OnCoupeSauver()
{
	FILE *Fichier;
	int don1,don2;
	CFileDialog FileData(FALSE,"*.xls",NULL,OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,"EXCEL (*.xls)|*.xls||",this);
	if (FileData.DoModal()==IDOK)
	{
		if (Fichier=fopen(FileData.GetPathName(),"w"))
		{
			SetCursor(LoadCursor(0,IDC_WAIT));
			for (int i=0;i<(int)NbPoint;i++)
			{
				don1=(int) GetY(i);
				don2=(int)Get2Y(i);
				fprintf(Fichier,"%d;%d;%d\n",i, don1,don2);
			}
			fclose(Fichier);
			SetCursor(LoadCursor(0,IDC_ARROW));
		}
	}   
}


