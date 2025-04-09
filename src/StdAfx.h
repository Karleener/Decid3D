// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__288929A3_2F5A_11D5_879D_0050043AFEB2__INCLUDED_)
#define AFX_STDAFX_H__288929A3_2F5A_11D5_879D_0050043AFEB2__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#define _AFX_ALL_WARNINGS
#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers
//#define _CRT_SECURE_NO_DEPRECATE
#define vtkRenderingContext2D_AUTOINIT 1(vtkRenderingContextOpenGL2)
#define vtkRenderingCore_AUTOINIT 3(vtkInteractionStyle,vtkRenderingFreeType,vtkRenderingOpenGL2)
#define vtkRenderingOpenGL2_AUTOINIT 1(vtkRenderingGL2PSOpenGL2)
//#define NOMINMAX

#include <SDKDDKVer.h>
//#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS      // certains constructeurs CString seront explicite

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include <afxdisp.h>        // MFC Automation classes
#include <afxdtctl.h>		// MFC support for Internet Explorer 4 Common Controls
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>			// MFC support for Windows Common Controls

//#include "QArray.h"
#endif // _AFX_NO_AFXCMN_SUPPORT
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkActor.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolyData.h>
#include <vtkPoints.h>
#include <vtkUnsignedCharArray.h>
#include <vtkImageData.h>
#include <vtkDiscreteMarchingCubes.h>
#include <vtkLookupTable.h>


//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__288929A3_2F5A_11D5_879D_0050043AFEB2__INCLUDED_)
