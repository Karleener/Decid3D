// GenGauss.cpp: implementation of the CGenGauss class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Decid.h"
#include "GenGauss.h"
#include  "math.h"


#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CGenGauss::CGenGauss(CString NomFich,float moyx,float ecarx,float moyy,float ecary,int Nech)
{
	float xf,yf;
	int i;
	srand(seconde());
	remplitab(255.0*moyx,255.0*ecarx,tabx);
	remplitab(255.0*moyy,255.0*ecary,taby);
	FILE *fic;
	fic = fopen(NomFich,"w+");
	fprintf(fic,"2\n"); // nombre de paramètres
	fprintf (fic,"%d\n",Nech);
	for ( i = 0; i < Nech; i++)
	{
	   xf = (float)gaurand(tabx,255.0*moyx);
	   yf = (float)gaurand(taby,255.0*moyy);
	   xf=xf/255;
	   yf=yf/255;
	   fprintf(fic,"%f %f \n", xf, yf);

	}
	fclose(fic);
}

CGenGauss::~CGenGauss()
{

}
int CGenGauss::foncnor(int x, float moy, float ecar)
{
	int  n;
	float   f, g;

	g = float(x);
	g -= moy; g /= ecar;
	g *= g; g/= 2;
	f = (exp(-g) * 10000) / ( pI * ecar);
	n = floor(f);
	if( (f-n) < 0.5)
		return n;
        else        
	return n;
}

void CGenGauss::remplitab( float moy, float ecar, int tab[])
{
	int i;

	tab[0] =  foncnor(0,moy,ecar);
	for( i = 1; i < 256; i++)
	{
	tab[i] = tab[i-1] + foncnor(i,moy,ecar);
	}
//	if (tab[i]>(moy+10*ecar) || tab[i]<moy-10*ecar) tab[i]=moy;
	if (tab[i]==0) tab[i]=moy;
}

int CGenGauss::randv(int nech)
{
	int i,a;
	float y,z;

	a = rand();
	z = float(a);
	z /= 32767;
	z *= nech;
	y = floor(z);
	if ( (z-y) > 0.5)
		y += 1;
	i = int(y);
	return i;
}

int CGenGauss::gaurand(int tab[],int moy)
{
	int h,i;
	
        i = tab[255];	
	h = randv(i);
	if (h==0) return moy;
	i = 0;
	while (( tab[i] < h) && (i < 255)) { i++;}
	return i;
}

int CGenGauss::aleat() // nombre al‚atoire entre -5 et +5
{
	int n;
	n = (rand() / 3276) - 5;
	return n;
}

int CGenGauss::seconde()
{
	int      tt;
	struct   tm *ti;
	time_t   ltime;

	time(&ltime);
	ti = localtime(&ltime);
	tt = ti -> tm_sec;
	return tt;
}
