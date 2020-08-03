
# NMAKE makefile.
# creates PROCWIN.EXE
# use one of the following tools:
# - MS VC v5, v6
# - MS VisualC++ Toolkit 2003 (+ MS Platform SDK)
# - MS VC++ Express Edition 2005
# just compiler, linker and resource compiler are needed
#
# 3 additional libs are supplied:
# - libc32s.lib: a small C runtime library, MSVC compatible
# - lib32w.lib: contains some "Queue" helper functions
# - jdisasm.lib: import library for jdisasm.dll

name = procwin

# directories for some proprietary include/library files

INCDIR=Include
LIBDIR=Libs

!ifndef DEBUG
DEBUG=0
!endif

SRCMODS = \
!include modules.inc

OBJMODS = $(SRCMODS:.CPP=.OBJ)
!if $(DEBUG)
OBJMODS = $(OBJMODS:.\=DEBUG\)
!else
OBJMODS = $(OBJMODS:.\=RELEASE\)
!endif


!if $(DEBUG)
LOPTD=/DEBUG:FULL /PDB:$*.pdb
AOPTD=-Zi -DDEBUG
COPTD=-Zi -Od -D "_DEBUG"
OUTDIR=DEBUG
DISASMLIB=jdisasm.lib
!else
LOPTD=
AOPTD=
COPTD=-Ox -D "NDEBUG"
OUTDIR=RELEASE
DISASMLIB=jdisasm.lib
!endif

LANGCC=-D "AFX_TARG_ENG"
LANGRC=/l 0x409 /d AFX_TARG_ENG 

VCDIR=\msvc71
PSDK=\MSSDK
WININC=\WinInc

INCS=rsrc.h ProcWin.h ModView.h Strings.h Tabwnd.h ImpDoc.h IatDoc.h CApplication.h CExplorerView.h DisasDoc.h
CC  = $(VCDIR)\bin\cl.exe -c -nologo -W3 -ML -Fo$* $(COPTD) -D "WIN32" -D "_WINDOWS" $(LANGCC) -I $(INCDIR) -I $(VCDIR)\Include -I "$(PSDK)\Include"
RC  = $(VCDIR)\bin\rc.exe $(LANGRC) /d AFX_TARG_NEUD /d AFX_RESOURCE_DLL /fo $*.res -i "$(PSDK)\Include"
LIBS=kernel32.lib advapi32.lib user32.lib gdi32.lib shell32.lib comctl32.lib comdlg32.lib ole32.lib uuid.lib libc32s.lib lib32w.lib $(DISASMLIB)
LOPT=/OUT:$*.exe /MAP:$*.map /SUBSYSTEM:WINDOWS $(LOPTD) /LIBPATH:$(LIBDIR) /MERGE:.CRT=.data
LINK=link.exe /nologo /OPT:NOWIN98 /LIBPATH:"$(WININC)\Lib" /NODEFAULTLIB

$(OUTDIR)\$(name).exe: $(OBJMODS) $(OUTDIR)\$(name).res Makefile
	@$(LINK) $(OBJMODS) $(OUTDIR)\$(name).res $(LIBS) $(LOPT)

.cpp{$(OUTDIR)}.obj:
	@$(CC) $<

.rc{$(OUTDIR)}.res:
	@$(RC) $<

$(OBJMODS): $(INCS) Makefile

$(OUTDIR)\$(name).res: $(name).rc Makefile RES\ProcWin.ICO RES\Splith.CUR RES\Toolbar.BMP $(name)E.rtf

clean:
	del $(OUTDIR)\*.obj
