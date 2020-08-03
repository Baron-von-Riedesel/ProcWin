
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

//#include <pehdr.h>
#include <disasm.h>
#include <lists.h>

#include "disasdoc.h"
#include "rsrc.h"

extern "C" int __stdcall SearchSymbol(LPSTR pOutStr, _int64 qwAddress);

CDisAsmDoc::CDisAsmDoc(void)
{
	iUnAssLines = 0;
	m_dwCodeOffs = 0;
	pBase = 0;
	pDisAssArray = NULL;
}

CDisAsmDoc::~CDisAsmDoc(void)
{
	if (pDisAssArray)
		VirtualFree(pDisAssArray,0,MEM_RELEASE);
}

// dwBaseOfCode & dwSizeOfCode are valid only if pPE is NULL

int CDisAsmDoc::UpdateDocument(unsigned char * pMem, UINT dwBaseOfCode, UINT dwSizeOfCode, UINT dwFlags)
{
	char szStr[260];
	unsigned char * pByte;
	unsigned char * pByte2;
	unsigned int j;
	unsigned char * * ppC;

	pBase = pMem;
	m_dwFlags = dwFlags;

	if (!pDisAssArray) {
		m_dwBaseOfCode = dwBaseOfCode;
		pByte = pBase + m_dwBaseOfCode;
		pByte2 = pByte + dwSizeOfCode;
		for (iUnAssLines = 0;pByte < pByte2;iUnAssLines++) {
			pByte = UnAssemble(szStr, pByte, m_dwFlags, 0, SearchSymbol );
		}
		pDisAssArray = (unsigned char * *)VirtualAlloc(NULL,
									iUnAssLines * sizeof(unsigned char *),
									MEM_COMMIT | MEM_TOP_DOWN,
									PAGE_READWRITE);
		if (pDisAssArray) {
			pByte = pBase + m_dwBaseOfCode;
			ppC = pDisAssArray;
			for (j = iUnAssLines; j ; j--) {
				*ppC++ = pByte;
				pByte = UnAssemble(szStr, pByte, m_dwFlags, 0, SearchSymbol );
			}
		}
	}
	return 1;
}

int CDisAsmDoc::GetActualLine()
{
//	IMAGE_NT_HEADERS * pPE = (IMAGE_NT_HEADERS *)(pBase + *(DWORD *)(pBase+0x3C));
	unsigned char * * ppC;
	UINT i = 0;

	if (m_dwCodeOffs)
		 for (ppC = pDisAssArray;
			(i < iUnAssLines) && (*ppC < pBase + m_dwCodeOffs + m_dwBaseOfCode);
			i++, ppC++);
	if (i == iUnAssLines)
		return -1;
	else
		return i;
}

UINT CDisAsmDoc::GetLines()
{
	return iUnAssLines;
}

unsigned char * CDisAsmDoc::GetAddressFromLine(UINT uLine)
{
	if (pDisAssArray)
		return *(pDisAssArray + uLine);
	else
		return NULL;
}
BOOL CDisAsmDoc::SetAddressOfLine(UINT uLine, unsigned char * pAddress)
{
	unsigned char * pByte;
	//unsigned char * pByte2;
	unsigned int j;
	unsigned char * * ppC;
	BOOL rc = FALSE;
	char szStr[260];

//	pByte = pAddress - m_dwCodeOffs;
	pByte = pAddress;
	if (pDisAssArray && (uLine < iUnAssLines)) {
		ppC = pDisAssArray + uLine;
		if (*ppC != pByte) {
			for (j = uLine; j < iUnAssLines; j++) {
				*ppC++ = pByte;
				pByte = UnAssemble(szStr, pByte, m_dwFlags, 0, SearchSymbol );
			}
			rc = TRUE;
		}
	}
	
	return rc;
}

int CDisAsmDoc::GetCodeBytes(unsigned char * pStr, char * pStr2)
{
	char szStr1[64];
	unsigned char * pStr1;

	pStr1 = UnAssemble(szStr1, pStr, m_dwFlags, 0, SearchSymbol );
	*pStr2 = 0;
	for (;pStr < pStr1;pStr++,pStr2 = pStr2 + 3) {
		sprintf(pStr2,"%02X ",*pStr);
	}
	return 1;
}

int CDisAsmDoc::GetSource(unsigned char * pStr, char * pStr2, UINT dwKorr)
{
	pStr = UnAssemble(pStr2, pStr, m_dwFlags, dwKorr, SearchSymbol );
	return 1;
}

int CDisAsmDoc::SetCodeOffset( UINT dwOffset)
{
	m_dwCodeOffs = dwOffset;
	return m_dwCodeOffs;
}
