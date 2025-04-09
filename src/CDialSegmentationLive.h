#pragma once
#include "afxdialogex.h"


// boîte de dialogue de CDialSegmentationLive

class CDialSegmentationLive : public CDialogEx
{
	DECLARE_DYNAMIC(CDialSegmentationLive)

public:
	CDialSegmentationLive(CWnd* pParent = nullptr);   // constructeur standard
	virtual ~CDialSegmentationLive();

// Données de boîte de dialogue
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_SEGMENTATION_LIVE };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // Prise en charge de DDX/DDV

	DECLARE_MESSAGE_MAP()
public:
	int m_Wf;
	int m_nbSample;
	int m_nbCluster;
	int m_TypeColor;
	BOOL m_DataOnly;
};
