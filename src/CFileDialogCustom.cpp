#include "stdafx.h"
#include "CFileDialogCustom.h"


//IMPLEMENT_DYNAMIC(CFileDialogCustom, CFileDialog)
BEGIN_MESSAGE_MAP(CFileDialogCustom, CFileDialog)

END_MESSAGE_MAP()



void CFileDialogCustom::OnInitDone()
{


    CFileDialog::OnInitDone();

    // Obtenir la position globale de la bo�te de dialogue
    //CRect dialogRect;
    //GetClientRect(&dialogRect);

    //// D�finir la position des nouveaux contr�les
    //CRect labelRect(dialogRect.left + 10, dialogRect.bottom - 50, dialogRect.left + 150, dialogRect.bottom - 30);
    //CRect editRect(dialogRect.left + 160, dialogRect.bottom - 50, dialogRect.left + 260, dialogRect.bottom - 30);

    //// Ajouter une �tiquette
    //CStatic* label = new CStatic();
    //label->Create(_T("D�nominateur  de la proportion en apprentissage: 1/1  1/4  1/2 ou 1/8 :"), WS_VISIBLE | WS_CHILD, labelRect, this);

    //// Ajouter un champ d'�dition
    //CEdit* edit = new CEdit();
    //edit->Create(WS_VISIBLE | WS_CHILD | WS_BORDER, editRect, this, 1);
    //edit->SetWindowText(_T("2")); // Valeur par d�faut
    return ;
}

BOOL CFileDialogCustom::OnFileNameOK()
{
    CString text;
    //GetDlgItem(1)->GetWindowText(text); // R�cup�rer le texte du champ d'�dition

    GetEditBoxText(121212, text);
    proportion = _ttoi(text);


    return CFileDialog::OnFileNameOK();
}