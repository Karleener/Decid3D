// GenGauss.h: interface for the CGenGauss class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_GENGAUSS_H__2D3424E3_D87B_41D5_8015_90308AA711BA__INCLUDED_)
#define AFX_GENGAUSS_H__2D3424E3_D87B_41D5_8015_90308AA711BA__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

const  float  pI = 2.50662; // racine carr‚ de 2 pi;
class CGenGauss  
{
public:
	CGenGauss(CString NomFich="Defaut.dat",float moyx=127.0,float ecarx=10.0,float moyy=127.0,float ecary=10.0,int Nech=100);
	virtual ~CGenGauss();
	int NECH;
	int tabx[257];
	int taby[257];
	int foncnor(int x, float moy, float ecar);
	void remplitab( float moy, float ecar, int tab[]);
	int randv(int nech);
	int gaurand(int tab[],int moy);
	int aleat() ;
	int seconde();

};

#endif // !defined(AFX_GENGAUSS_H__2D3424E3_D87B_41D5_8015_90308AA711BA__INCLUDED_)
