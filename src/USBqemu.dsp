# Microsoft Developer Studio Project File - Name="USBqemu" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=USBqemu - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "USBqemu.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "USBqemu.mak" CFG="USBqemu - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "USBqemu - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "USBqemu - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "USBqemu - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "USBQEMU_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "USBQEMU_EXPORTS" /FR /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40c /d "NDEBUG"
# ADD RSC /l 0x40c /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386 /out:"h:\program files\pcsx2_0.9.4\plugins\USBqemu.dll"

!ELSEIF  "$(CFG)" == "USBqemu - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "USBQEMU_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "USBQEMU_EXPORTS" /FR /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40c /d "_DEBUG"
# ADD RSC /l 0x40c /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /out:"h:\program files\pcsx2_0.9.4\plugins\USBqemu.dll" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "USBqemu - Win32 Release"
# Name "USBqemu - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\Win32\Config.cpp
# End Source File
# Begin Source File

SOURCE=".\qemu-usb\usb-base.cpp"
# End Source File
# Begin Source File

SOURCE=".\qemu-usb\usb-kbd.cpp"
# End Source File
# Begin Source File

SOURCE=".\qemu-usb\usb-ohci.cpp"
# End Source File
# Begin Source File

SOURCE=.\USB.cpp
# End Source File
# Begin Source File

SOURCE=.\Win32\USBlinuz.def
# End Source File
# Begin Source File

SOURCE=.\Win32\USBlinuz.rc
# End Source File
# Begin Source File

SOURCE=".\qemu-usb\vl.cpp"
# End Source File
# Begin Source File

SOURCE=.\Win32\Win32.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=".\usb-mic\adcuser.h"
# End Source File
# Begin Source File

SOURCE=".\usb-mic\audio.h"
# End Source File
# Begin Source File

SOURCE=".\usb-mic\demo.h"
# End Source File
# Begin Source File

SOURCE=.\PS2Edefs.h
# End Source File
# Begin Source File

SOURCE=.\PS2Etypes.h
# End Source File
# Begin Source File

SOURCE=.\Win32\resource.h
# End Source File
# Begin Source File

SOURCE=".\usb-mic\type.h"
# End Source File
# Begin Source File

SOURCE=".\qemu-usb\usb.h"
# End Source File
# Begin Source File

SOURCE=".\usb-mic\usb.h"
# End Source File
# Begin Source File

SOURCE=.\USB.h
# End Source File
# Begin Source File

SOURCE=".\usb-mic\usbcfg.h"
# End Source File
# Begin Source File

SOURCE=".\usb-mic\usbcore.h"
# End Source File
# Begin Source File

SOURCE=".\usb-mic\usbdesc.h"
# End Source File
# Begin Source File

SOURCE=".\usb-mic\usbhw.h"
# End Source File
# Begin Source File

SOURCE=".\usb-mic\usbreg.h"
# End Source File
# Begin Source File

SOURCE=".\usb-mic\usbuser.h"
# End Source File
# Begin Source File

SOURCE=".\qemu-usb\vl.h"
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
