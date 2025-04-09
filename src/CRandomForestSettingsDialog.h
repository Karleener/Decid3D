#pragma once
#include "afxwin.h"
#include "resource.h"
#include "afxdialogex.h"

class CRandomForestSettingsDialog : public CDialogEx
{
public:
    CRandomForestSettingsDialog(CWnd* pParent = nullptr); // Constructeur

    // ID de la boîte de dialogue
    enum { IDD = IDD_RANDOM_FOREST_SETTINGS };

protected:
    virtual void DoDataExchange(CDataExchange* pDX); // Support pour DDX/DDV

    DECLARE_MESSAGE_MAP()

public:
    // Variables membres pour chaque paramètre
    int m_MaxDepth;
    int m_MinSampleCount;
    int m_MaxCategories;
    int m_ActiveVarCount;
    float m_ForestAccuracy;
    BOOL m_UseSurrogates;

    // Méthode pour initialiser les valeurs par défaut
    void InitializeDefaultValues(int na);
};
