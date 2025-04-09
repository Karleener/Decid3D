#pragma once
#include "afxdialogex.h"


// boîte de dialogue de CDialSlider
class CChildFrame;

class CDialSlider : public CDialogEx
{
	DECLARE_DYNAMIC(CDialSlider)

public:
	CDialSlider(CWnd* pParent = nullptr);   // constructeur standard
	void SetParentFrame(CChildFrame* pParentFrame) {
		m_pParentFrame = pParentFrame;
	}
	BOOL Create(CWnd* pParentWnd = nullptr) {
		return CDialog::Create(IDD_DIALOG_SLIDER, pParentWnd);
	}
	virtual ~CDialSlider();
	CChildFrame* m_pParentFrame;
// Données de boîte de dialogue
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_SLIDER };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // Prise en charge de DDX/DDV

	DECLARE_MESSAGE_MAP()
public:
	int m_Pos;
	BOOL m_bIsScrolling = FALSE;
	CString m_Aff;
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	CString m_AfficheErreur;
	afx_msg void OnClose();
};
