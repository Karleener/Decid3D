#include "stdafx.h"
#include "CFileDialogCustom.h"


//IMPLEMENT_DYNAMIC(CFileDialogCustom, CFileDialog)
BEGIN_MESSAGE_MAP(CFileDialogCustom, CFileDialog)

END_MESSAGE_MAP()



void CFileDialogCustom::OnInitDone()
{


    CFileDialog::OnInitDone();

    // Obtenir la position globale de la boîte de dialogue
    //CRect dialogRect;
    //GetClientRect(&dialogRect);

    //// Définir la position des nouveaux contrôles
    //CRect labelRect(dialogRect.left + 10, dialogRect.bottom - 50, dialogRect.left + 150, dialogRect.bottom - 30);
    //CRect editRect(dialogRect.left + 160, dialogRect.bottom - 50, dialogRect.left + 260, dialogRect.bottom - 30);

    //// Ajouter une étiquette
    //CStatic* label = new CStatic();
    //label->Create(_T("Dénominateur  de la proportion en apprentissage: 1/1  1/4  1/2 ou 1/8 :"), WS_VISIBLE | WS_CHILD, labelRect, this);

    //// Ajouter un champ d'édition
    //CEdit* edit = new CEdit();
    //edit->Create(WS_VISIBLE | WS_CHILD | WS_BORDER, editRect, this, 1);
    //edit->SetWindowText(_T("2")); // Valeur par défaut
    return ;
}

BOOL CFileDialogCustom::OnFileNameOK()
{
    CString text;
    //GetDlgItem(1)->GetWindowText(text); // Récupérer le texte du champ d'édition

    GetEditBoxText(121212, text);
    proportion = _ttoi(text);


    return CFileDialog::OnFileNameOK();
}