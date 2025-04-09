// MainFrm.cpp : implementation of the CMainFrame class
//

#include "stdafx.h"

#include "Decid.h"

#include "MainFrm.h"
#include "ChildFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CMainFrame

IMPLEMENT_DYNAMIC(CMainFrame, CMDIFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CMDIFrameWnd)
	//{{AFX_MSG_MAP(CMainFrame)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	ON_WM_CREATE()
	ON_WM_INITMENUPOPUP()
	ON_WM_EXITMENULOOP()
	//}}AFX_MSG_MAP
	ON_WM_CLOSE()
END_MESSAGE_MAP()

static UINT indicators[] =
{
	ID_SEPARATOR,           // status line indicator
	ID_INDICATOR_CAPS,
	ID_INDICATOR_NUM,
	ID_INDICATOR_SCRL,
};

/////////////////////////////////////////////////////////////////////////////
// CMainFrame construction/destruction

CMainFrame::CMainFrame()
{
	// TODO: add member initialization code here
	
}

CMainFrame::~CMainFrame()
{
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CMDIFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	if (!m_wndToolBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP
		| CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC) ||
		!m_wndToolBar.LoadToolBar(IDR_MAINFRAME))
	{
		TRACE0("Failed to create toolbar\n");
		return -1;      // fail to create
	}

	if (!m_wndStatusBar.Create(this) ||
		!m_wndStatusBar.SetIndicators(indicators,
		  sizeof(indicators)/sizeof(UINT)))
	{
		TRACE0("Failed to create status bar\n");
		return -1;      // fail to create
	}

	// TODO: Delete these three lines if you don't want the toolbar to
	//  be dockable
	m_wndToolBar.EnableDocking(CBRS_ALIGN_ANY);
	EnableDocking(CBRS_ALIGN_ANY);
	DockControlBar(&m_wndToolBar);
	PostMessage(WM_COMMAND, ID_FILE_NEW);
	return 0;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if( !CMDIFrameWnd::PreCreateWindow(cs) )
		return FALSE;
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CMainFrame diagnostics

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
	CMDIFrameWnd::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
	CMDIFrameWnd::Dump(dc);
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CMainFrame message handlers


#include <vector>

void CMainFrame::OnClose()
{
	HWND hMDIClient = m_hWndMDIClient ? m_hWndMDIClient : ::GetWindow(m_hWnd, GW_CHILD);
	if (!hMDIClient) return;  // Sécurité : si la fenêtre MDI Client n'existe pas

	std::vector<CChildFrame*> childFrames;

	// Fonction pour récupérer toutes les fenêtres enfants
	EnumChildWindows(hMDIClient, [](HWND hwnd, LPARAM lParam) -> BOOL
		{
			CWnd* pWnd = CWnd::FromHandle(hwnd);
			CChildFrame* pChildFrame = (CChildFrame*)(pWnd);
			if (pChildFrame)
			{
				std::vector<CChildFrame*>* pList = reinterpret_cast<std::vector<CChildFrame*>*>(lParam);
				pList->push_back(pChildFrame);
			}
			return TRUE;
		}, (LPARAM)&childFrames);

	// Fermer proprement toutes les fenêtres enfants
	for (CChildFrame* pChildFrame : childFrames)
	{
		if (pChildFrame)
		{
			// Vérifier les données membres ici si besoin
			// Exemple :
			if (pChildFrame->m_closeCallback != nullptr) 
			{
				if (!pChildFrame->m_closeCallback->windowClosed)
				{
					//MessageBox("Merci de fermer la fenetre 3D avant de quitter l'application"); return;

					HWND hWnd = (HWND)pChildFrame->m_render_window_interactor->GetRenderWindow()->GetGenericWindowId();
					if (hWnd)
					{
						::PostMessage(hWnd, WM_CLOSE, 0, 0);
					}
				}
			}

			pChildFrame->SendMessage(WM_CLOSE);
		}
	}

	CMDIFrameWnd::OnClose();  // Ferme la fenêtre principale
	exit(0);
}

void CMainFrame::OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu) {
	CFrameWnd::OnInitMenuPopup(pPopupMenu, nIndex, bSysMenu);

	// Arrêter la rotation VTK si une fenêtre enfant est active
	//CChildFrame* pChild = (CChildFrame*)MDIGetActive();
	//if (pChild) {
	//	pChild->StopRotation();
	//}
}

void CMainFrame::OnExitMenuLoop(BOOL bIsTrackPopupMenu) {
	CFrameWnd::OnExitMenuLoop(bIsTrackPopupMenu);

	// Reprendre l'animation après fermeture du menu
	//CChildFrame* pChild = (CChildFrame*)MDIGetActive();
	//if (pChild) {
	//	pChild->StartRotation();
	//}
}