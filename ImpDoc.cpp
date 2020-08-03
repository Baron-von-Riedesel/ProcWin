

#define STRICT

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <stddef.h>
#include <stdio.h>
#include <io.h>
#include <share.h>
#include <fcntl.h>
#include <sys\stat.h>
#include <process.h>
#include <disasm.h>
#include <lists.h>
//#include "ModView.h"
//#include "procwin.h"
#include "impdoc.h"
#include "rsrc.h"

ImpDoc::ImpDoc(void)
{
	pBase = 0;
	pPE = 0;
	uLines = 0;
}

ImpDoc::~ImpDoc(void)
{
}

int ImpDoc::UpdateDocument(IMAGE_NT_HEADERS * pPEnew, unsigned char * pMem)
{
//	char szStr[260];
//	unsigned int j;
//	unsigned char * * ppC;

	pBase = pMem;
	pPE = pPEnew;

	return 1;
}

int ImpDoc::GetActualLine()
{
//	unsigned char * * ppC;
	int i = 0;

	return i;
}

UINT ImpDoc::GetLines()
{
	IMAGE_IMPORT_DESCRIPTOR * pID;

    pID = (IMAGE_IMPORT_DESCRIPTOR *)(pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress + pBase);

	uLines = 0;
	if (pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size)
		while (pID->OriginalFirstThunk || pID->Name) {
			pID++;
			uLines++;
		}

	return uLines;
}

UINT ImpDoc::GetValue(UINT uLine,UINT uIndex,char * szStr)
{
	IMAGE_IMPORT_DESCRIPTOR * pID;

	*szStr = 0;

	pID = (IMAGE_IMPORT_DESCRIPTOR *)(pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress + pBase);

	pID = pID + uLine;

	switch (uIndex) {
	case 0:
		sprintf(szStr,"%u",uLine);
		break;
	case 1:
		sprintf(szStr,"%X",	pID->OriginalFirstThunk);
		break;
	case 2:
		sprintf(szStr,"%X",	pID->FirstThunk);
		break;
	case 3:
		sprintf(szStr,"%X (%s)",pID->Name,(char *)(pBase+pID->Name));
		break;
	case 4:
		sprintf(szStr,"%X",	pID->TimeDateStamp);
		break;
	case 5:
		sprintf(szStr,"%X",	pID->ForwarderChain);
		break;
	}
	return 1;
}


