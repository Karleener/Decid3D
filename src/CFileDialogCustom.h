#pragma once
#include <afxdlgs.h>
class CFileDialogCustom :    public CFileDialog
{

public:
    int proportion; // Stocker la proportion choisie par l'utilisateur

    CFileDialogCustom(BOOL bOpenFileDialog, // TRUE pour ouvrir un fichier, FALSE pour enregistrer
        LPCTSTR lpszDefExt = NULL,
        LPCTSTR lpszFileName = NULL,
        DWORD dwFlags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
        LPCTSTR lpszFilter = NULL,
        CWnd* pParentWnd = NULL)
        : CFileDialog(bOpenFileDialog, lpszDefExt, lpszFileName, dwFlags, lpszFilter, pParentWnd),
        proportion(2) // Proportion par défaut
    {
    }


    virtual void OnInitDone();

    virtual BOOL OnFileNameOK();


    DECLARE_MESSAGE_MAP() // Déclare une message map
};

