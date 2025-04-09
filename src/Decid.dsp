# Microsoft Developer Studio Project File - Name="Decid" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=Decid - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Decid.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Decid.mak" CFG="Decid - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Decid - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "Decid - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Decid - Win32 Release"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MD /W3 /GX /Ox /Ot /Oa /Ow /Og /Oi /Op /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /FR /Yu"stdafx.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40c /d "NDEBUG" /d "_AFXDLL"
# ADD RSC /l 0x40c /d "NDEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /machine:I386
# ADD LINK32 /nologo /subsystem:windows /machine:I386

!ELSEIF  "$(CFG)" == "Decid - Win32 Debug"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /FR /Yu"stdafx.h" /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40c /d "_DEBUG" /d "_AFXDLL"
# ADD RSC /l 0x40c /d "_DEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# SUBTRACT LINK32 /incremental:no

!ENDIF 

# Begin Target

# Name "Decid - Win32 Release"
# Name "Decid - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\Boosting.cpp
# End Source File
# Begin Source File

SOURCE=.\BoostMc.cpp
# End Source File
# Begin Source File

SOURCE=.\BoostMcBis.cpp
# End Source File
# Begin Source File

SOURCE=.\ChildFrm.cpp
# End Source File
# Begin Source File

SOURCE=.\ConfigNeurone.cpp
# End Source File
# Begin Source File

SOURCE=.\Courbe.cpp
# End Source File
# Begin Source File

SOURCE=.\DaliogProgression.cpp
# End Source File
# Begin Source File

SOURCE=.\Decid.cpp
# End Source File
# Begin Source File

SOURCE=.\Decid.rc
# End Source File
# Begin Source File

SOURCE=.\DemChaine.cpp
# End Source File
# Begin Source File

SOURCE=.\DialApprend.cpp
# End Source File
# Begin Source File

SOURCE=.\DialBoosting.cpp
# End Source File
# Begin Source File

SOURCE=.\DialChoixClass.cpp
# End Source File
# Begin Source File

SOURCE=.\DialConfigPoly.cpp
# End Source File
# Begin Source File

SOURCE=.\DialGauss1.cpp
# End Source File
# Begin Source File

SOURCE=.\Dialiter.cpp
# End Source File
# Begin Source File

SOURCE=.\Dialog2Val.cpp
# End Source File
# Begin Source File

SOURCE=.\DialPmc.cpp
# End Source File
# Begin Source File

SOURCE=.\DialResultImplan.cpp
# End Source File
# Begin Source File

SOURCE=.\DialVam.cpp
# End Source File
# Begin Source File

SOURCE=.\DonneeFloat.cpp
# End Source File
# Begin Source File

SOURCE=.\GenGauss.cpp
# End Source File
# Begin Source File

SOURCE=.\kppv.cpp
# End Source File
# Begin Source File

SOURCE=.\MainFrm.cpp
# End Source File
# Begin Source File

SOURCE=.\PolMcFloat.cpp
# End Source File
# Begin Source File

SOURCE=.\Reseau.cpp
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# ADD CPP /Yc"stdafx.h"
# End Source File
# Begin Source File

SOURCE=.\svm.cpp
# End Source File
# Begin Source File

SOURCE=.\SVMParametres1.cpp
# End Source File
# Begin Source File

SOURCE=.\TreeConfig.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\Boosting.h
# End Source File
# Begin Source File

SOURCE=.\BoostMc.h
# End Source File
# Begin Source File

SOURCE=.\BoostMcBis.h
# End Source File
# Begin Source File

SOURCE=.\ChildFrm.h
# End Source File
# Begin Source File

SOURCE=.\ConfigNeurone.h
# End Source File
# Begin Source File

SOURCE=.\Courbe.h
# End Source File
# Begin Source File

SOURCE=.\DaliogProgression.h
# End Source File
# Begin Source File

SOURCE=.\Decid.h
# End Source File
# Begin Source File

SOURCE=.\DemChaine.h
# End Source File
# Begin Source File

SOURCE=.\DialApprend.h
# End Source File
# Begin Source File

SOURCE=.\DialBoosting.h
# End Source File
# Begin Source File

SOURCE=.\DialChoixClass.h
# End Source File
# Begin Source File

SOURCE=.\DialConfigPoly.h
# End Source File
# Begin Source File

SOURCE=.\DialGauss1.h
# End Source File
# Begin Source File

SOURCE=.\Dialiter.h
# End Source File
# Begin Source File

SOURCE=.\Dialog2Val.h
# End Source File
# Begin Source File

SOURCE=.\DialPmc.h
# End Source File
# Begin Source File

SOURCE=.\DialResultImplan.h
# End Source File
# Begin Source File

SOURCE=.\DialVam.h
# End Source File
# Begin Source File

SOURCE=.\DonneeFloat.h
# End Source File
# Begin Source File

SOURCE=.\GenGauss.h
# End Source File
# Begin Source File

SOURCE=.\kppv.h
# End Source File
# Begin Source File

SOURCE=.\MainFrm.h
# End Source File
# Begin Source File

SOURCE=.\PolMcFloat.h
# End Source File
# Begin Source File

SOURCE=.\Reseau.h
# End Source File
# Begin Source File

SOURCE=.\Resource.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=.\svm.h
# End Source File
# Begin Source File

SOURCE=.\SVMParametres1.h
# End Source File
# Begin Source File

SOURCE=.\TreeConfig.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\res\bitmaptr.bmp
# End Source File
# Begin Source File

SOURCE=.\res\cursor1.cur
# End Source File
# Begin Source File

SOURCE=.\res\cursor2.cur
# End Source File
# Begin Source File

SOURCE=.\res\cursor3.cur
# End Source File
# Begin Source File

SOURCE=.\res\cursor4.cur
# End Source File
# Begin Source File

SOURCE=.\res\cursor5.cur
# End Source File
# Begin Source File

SOURCE=.\res\Decid.ico
# End Source File
# Begin Source File

SOURCE=.\res\Decid.rc2
# End Source File
# Begin Source File

SOURCE=.\res\icon1.ico
# End Source File
# Begin Source File

SOURCE=.\res\Toolbar.bmp
# End Source File
# End Group
# Begin Source File

SOURCE=.\ReadMe.txt
# End Source File
# End Target
# End Project
# Section Decid : {A7C1876F-2F56-11D5-879D-0050043AFEB2}
# 	1:12:IDD_DIAL2VAL:106
# 	2:16:Resource Include:resource.h
# 	2:16:CLASS: CDial2Val:CDial2Val
# 	2:10:DIALPARA.H:DIALPARA1.H
# 	2:16:CLASS: CDialPara:CDialPara
# 	2:10:ENUM: enum:enum
# 	2:12:DIALPARA.CPP:DIALPARA1.CPP
# 	2:15:CLASS: CDialVal:CDialVal
# 	2:19:Application Include:Decid.h
# 	2:12:IDD_DIAL2VAL:IDD_DIAL2VAL
# End Section
# Section Decid : {A7C1876A-2F56-11D5-879D-0050043AFEB2}
# 	2:15:AfficheMesPol.h:AfficheMesPol.h
# 	2:21:CLASS: CAfficheMesPol:CAfficheMesPol
# 	2:17:AfficheMesPol.cpp:AfficheMesPol.cpp
# 	2:19:Application Include:Decid.h
# End Section
# Section Decid : {A7C1876E-2F56-11D5-879D-0050043AFEB2}
# 	1:14:IDD_CHOIXCLASS:105
# 	2:16:Resource Include:resource.h
# 	2:22:CLASS: CDialChoixClass:CDialChoixClass
# 	2:10:ENUM: enum:enum
# 	2:16:DialChoixClass.h:DialChoixClass.h
# 	2:19:Application Include:Decid.h
# 	2:14:IDD_CHOIXCLASS:IDD_CHOIXCLASS
# 	2:18:DialChoixClass.cpp:DialChoixClass.cpp
# End Section
# Section Decid : {A7C18767-2F56-11D5-879D-0050043AFEB2}
# 	1:14:IDD_CONFIGPOLY:102
# 	2:16:Resource Include:resource.h
# 	2:16:DialConfigPoly.h:DialConfigPoly.h
# 	2:14:IDD_CONFIGPOLY:IDD_CONFIGPOLY
# 	2:10:ENUM: enum:enum
# 	2:18:DialConfigPoly.cpp:DialConfigPoly.cpp
# 	2:22:CLASS: CDialConfigPoly:CDialConfigPoly
# 	2:19:Application Include:Decid.h
# End Section
# Section Decid : {A7C1876D-2F56-11D5-879D-0050043AFEB2}
# 	2:12:GraphWnd.cpp:GraphWnd.cpp
# 	2:16:CLASS: CGraphWnd:CGraphWnd
# 	2:10:GraphWnd.h:GraphWnd.h
# 	2:19:Application Include:Decid.h
# 	2:10:ENUM: Type:Type
# End Section
# Section Decid : {71A9559F-014E-11D7-9400-0004769C1E17}
# 	1:19:IDD_DIALOG_SVM_PARA:108
# 	2:16:Resource Include:resource.h
# 	2:16:SVMParametres1.h:SVMParametres1.h
# 	2:21:CLASS: CSVMParametres:CSVMParametres
# 	2:19:IDD_DIALOG_SVM_PARA:IDD_DIALOG_SVM_PARA
# 	2:10:ENUM: enum:enum
# 	2:18:SVMParametres1.cpp:SVMParametres1.cpp
# 	2:19:Application Include:Decid.h
# End Section
# Section Decid : {A7C18770-2F56-11D5-879D-0050043AFEB2}
# 	1:11:IDD_DIALVAL:107
# 	2:16:Resource Include:resource.h
# 	2:16:CLASS: CDial2Val:CDial2Val
# 	2:10:DIALPARA.H:DIALPARA2.H
# 	2:16:CLASS: CDialPara:CDialPara
# 	2:10:ENUM: enum:enum
# 	2:12:DIALPARA.CPP:DIALPARA2.CPP
# 	2:15:CLASS: CDialVal:CDialVal
# 	2:19:Application Include:Decid.h
# 	2:11:IDD_DIALVAL:IDD_DIALVAL
# End Section
# Section Decid : {A7C1876C-2F56-11D5-879D-0050043AFEB2}
# 	2:13:Graph2Wnd.cpp:Graph2Wnd.cpp
# 	2:17:CLASS: CGraph2Wnd:CGraph2Wnd
# 	2:19:Application Include:Decid.h
# 	2:11:Graph2Wnd.h:Graph2Wnd.h
# End Section
# Section Decid : {A7C18769-2F56-11D5-879D-0050043AFEB2}
# 	2:12:TreeConfig.h:TreeConfig.h
# 	2:18:CLASS: CTreeConfig:CTreeConfig
# 	2:14:TreeConfig.cpp:TreeConfig.cpp
# 	2:19:Application Include:Decid.h
# End Section
# Section Decid : {A7C1876B-2F56-11D5-879D-0050043AFEB2}
# 	1:9:IDD_PARAM:104
# 	2:16:Resource Include:resource.h
# 	2:9:IDD_PARAM:IDD_PARAM
# 	2:10:DIALPARA.H:DIALPARA.H
# 	2:16:CLASS: CDial2Val:CDial2Val
# 	2:16:CLASS: CDialPara:CDialPara
# 	2:10:ENUM: enum:enum
# 	2:12:DIALPARA.CPP:DIALPARA.CPP
# 	2:15:CLASS: CDialVal:CDialVal
# 	2:19:Application Include:Decid.h
# End Section
# Section Decid : {A7C18768-2F56-11D5-879D-0050043AFEB2}
# 	1:7:IDD_PMC:103
# 	2:16:Resource Include:resource.h
# 	2:15:CLASS: CDialPmc:CDialPmc
# 	2:11:DialPmc.cpp:DialPmc.cpp
# 	2:10:ENUM: enum:enum
# 	2:7:IDD_PMC:IDD_PMC
# 	2:19:Application Include:Decid.h
# 	2:9:DialPmc.h:DialPmc.h
# End Section
