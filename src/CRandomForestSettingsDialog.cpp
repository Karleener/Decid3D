#include "stdafx.h"
#include "CRandomForestSettingsDialog.h"
#include "afxdialogex.h"

// Constructeur
CRandomForestSettingsDialog::CRandomForestSettingsDialog(CWnd* pParent)
    : CDialogEx(IDD_RANDOM_FOREST_SETTINGS, pParent),
      m_MaxDepth(10),
      m_MinSampleCount(2),
      m_MaxCategories(15),
      m_ActiveVarCount(1),
      m_ForestAccuracy(0.01f),
      m_UseSurrogates(FALSE)
{
}

// Associer les contrôles avec les variables
void CRandomForestSettingsDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_EDIT_MAX_DEPTH, m_MaxDepth);
    DDX_Text(pDX, IDC_EDIT_MIN_SAMPLE_COUNT, m_MinSampleCount);
    DDX_Text(pDX, IDC_EDIT_MAX_CATEGORIES, m_MaxCategories);
    DDX_Text(pDX, IDC_EDIT_ACTIVE_VAR_COUNT, m_ActiveVarCount);
    DDX_Text(pDX, IDC_EDIT_FOREST_ACCURACY, m_ForestAccuracy);
    DDX_Check(pDX, IDC_EDIT_USE_SURROGATES, m_UseSurrogates);
}

// Initialisation des valeurs par défaut
void CRandomForestSettingsDialog::InitializeDefaultValues(int na)
{
    m_MaxDepth = 10;
    m_MinSampleCount = 2;
    m_MaxCategories = 15;
    m_ActiveVarCount = na;
    m_ForestAccuracy = 0.01f;
    m_UseSurrogates = FALSE;
}

BEGIN_MESSAGE_MAP(CRandomForestSettingsDialog, CDialogEx)
END_MESSAGE_MAP()
