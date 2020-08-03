
#define STRICT

#include <windows.h>
#include <windowsx.h>
#include <stddef.h>
#include <stdio.h>
#include <io.h>
#include <share.h>
#include <fcntl.h>
#include <sys\stat.h>
#include <process.h>
#include <disasm.h>
#include "iatdoc.h"
#include "rsrc.h"

#pragma pack(1)

IatDoc::IatDoc(void)
{
	pBase = NULL;
	pPE = NULL;
	m_dwModBase = NULL;
}

IatDoc::~IatDoc(void)
{
}

int IatDoc::UpdateDocument(IMAGE_NT_HEADERS * pPEnew, unsigned char * pMem, DWORD dwModBase)
{
//	char szStr[260];
//	unsigned int j;
	IMAGE_IMPORT_DESCRIPTOR * pID;
	void * * pV;

	pBase = pMem;
	pPE = pPEnew;
	m_dwModBase = dwModBase;

	if  (pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].VirtualAddress) {
		startiat = pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].VirtualAddress;
		sizeiat = pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].Size;
	} else {
		UINT min,max;
		if (pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress) {
			pID = (IMAGE_IMPORT_DESCRIPTOR *)(pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress + pBase);
			max = 0;
			min = 0;
			while (pID->FirstThunk) {
				if (pID->FirstThunk < min || (min == 0))
					min = pID->FirstThunk;
				if (pID->FirstThunk > max)
					max = pID->FirstThunk;
				pID++;
			}
			if (max) {
				pV = (void * *)(pBase + max);
				while (*pV) {
					pV++;
					max = max + sizeof(void *);
				}
			}
			startiat = min;
			sizeiat = max - min;
		} else {
			startiat = 0;
			sizeiat = 0;
		}
	}	

	return 1;
}

int IatDoc::GetActualLine()
{
	int i = 0;

	return i;
}

UINT IatDoc::GetLines()
{
	return sizeiat / sizeof(void *);
}

UINT IatDoc::GetValue(UINT uLine,UINT uIndex,char * szStr)
{
	void * * pIAT;
	void * * pIAT2;
	IMAGE_IMPORT_DESCRIPTOR * pID;
	char * pStr;
	UINT ui;
	void * pVoid;
	int i,j;

	*szStr = 0;

    pID = (IMAGE_IMPORT_DESCRIPTOR *)(pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress + pBase);
    pIAT = (void * *)(startiat + pBase);

	pIAT2 = pIAT;

	pIAT = pIAT + uLine;
	pVoid = *pIAT;

	switch (uIndex) {
	case 0:
		sprintf(szStr,"%u",uLine);
		break;
	case 1:
		sprintf(szStr,"%X",	(BYTE *)pIAT - pBase);
		break;
	case 2:
		sprintf(szStr,"%X",	pVoid);
		break;
	case 3:
	case 4:
	case 5:
		for (i = 0;pIAT > pIAT2;i++,pIAT--) {	// an Anfang der Teiltabelle gehen
			if (*(pIAT - 1) == 0) {
				break;
			}
		}
		j = 0;
#if 1
		while (pID->FirstThunk) {
			if (pID->FirstThunk == ((unsigned char *)pIAT) - pBase) {
                _asm {
					mov ecx,this
					mov edx,pID 
					mov eax,[edx.OriginalFirstThunk]
					and eax,eax
					jz  lm1
					add eax,[ecx.pBase]
					mov edx,i
					mov eax,[eax+edx*4]
					mov j,eax
					and eax,eax
					jz  lm1
					test eax,80000000h
					jnz lm1
					add eax,[ecx.pBase]
					movzx ecx,word ptr [eax]
					lea eax,[eax+2]
					mov ui,ecx
lm1:
					mov pStr,eax
                }
				switch (uIndex) {
				case 3:		// Name der Funktion
					if (j >= 0) {
						if (pStr)
							sprintf(szStr,"%s",	pStr);
					} else
						sprintf(szStr,"%X (@%u)",j, j & 0x7FFFFFFF);
					break;
				case 4:		// Name der DLL
//					if (j)
					if (pVoid && pID->Name)
						sprintf(szStr,"%s",(char *)pBase+pID->Name);
					break;
				case 5:
					if (j >= 0 && pStr)
						sprintf(szStr,"%X",	ui);
				}
				break;
			}
			pID++;
		}
#endif
		break;
	}
	return 1;
}

// find name of an address in IAT

UINT IatDoc::GetNameOfAddress(void * * dwAddress,char * pOutput)
{
	void * * pIAT;
	IMAGE_IMPORT_DESCRIPTOR * pID;
	//UINT dwOffset;
	char * pStr;
	int i;
	int iImport = 0;

	pIAT = (void * *)(startiat + pBase);

	dwAddress = (void * *)((DWORD)dwAddress - m_dwModBase + (DWORD)pBase);
	if ((dwAddress >= pIAT) && (dwAddress < (pIAT + sizeiat/sizeof(void *)))) {
		for (i = 0; dwAddress > pIAT;i++,dwAddress--)
			if (*(dwAddress - 1) == 0)
				break;
		pStr = NULL;
	    pID = (IMAGE_IMPORT_DESCRIPTOR *)(pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress + pBase);
		while (pID->FirstThunk) {
			if (pID->FirstThunk == ((unsigned char *)dwAddress) - pBase) {
				_asm {
					mov ecx,this
					mov edx,pID 
					mov eax,[edx.OriginalFirstThunk]
					and eax,eax
					jz	lm1
					add eax,[ecx.pBase]
					mov edx,i
					mov eax,[eax+edx*4]
					and eax,eax
					jz	lm1
					test eax,80000000h
					jnz lm2
					add eax,[ecx.pBase]
//					movzx ecx,word ptr [eax]
					lea eax,[eax+2]
//					mov ui,ecx
					mov pStr,eax
					jmp lm3
lm2:
					and eax,7FFFFFFFh	
					mov iImport,eax
lm1:
lm3:
				}
				if (pStr) {
					strcpy(pOutput,pStr);
					return strlen(pStr);
				} else {
					if (iImport) {
						strcpy(pOutput,(char *)pBase + pID->Name);
						if (!(pStr = strchr(pOutput,'.')))
							pStr = pOutput + strlen(pOutput);
						sprintf(pStr,".@%u",iImport);
						return strlen(pOutput);
					}
				}
				break;
			}
			pID++;
		}
	}


	return 0;
}