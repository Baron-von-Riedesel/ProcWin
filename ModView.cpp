
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

#include "CApplication.h"
#include "tabwnd.h"
#include "ModView.h"
#include "disasdoc.h"
#include "impdoc.h"
#include "iatdoc.h"
#include "procwin.h"
#include "rsrc.h"
#include "strings.h"

extern Application * pApp;

#define XPIXELS 1
#define DONTUSELASTFINDPOS 1

typedef struct tPEATTR {
char * pName;
UINT dwOffset;
} PEATTR;


static CColHdr chPEH[] = {
	24, pStrings[ATTRSTR],	// "Attribute",
	24, pStrings[VALUESTR],		// "Value",
	0};
static CColHdr chExp[] = {
	9, pStrings[IDXSTR],		// "Index",
	12, pStrings[ADDRSTR],		// "Address",
	9,  pStrings[NUMSTR],
	36, pStrings[NAMESTR],		// "Name",
	0};
static CColHdr chImp[] = {
	9, pStrings[IDXSTR],		// "Index",
	12, "ILT",
	12, "IAT",
	24, pStrings[NAMESTR],		// "Name",
	12, "Timestamp",
	12, "Forward Chain",
	0};
static CColHdr chIAT[] = {
	9, pStrings[IDXSTR],		// "Index",
	12, "Offset (RVA)",
	12, pStrings[ADDRSTR],		// "Address",
	24, "Name (ILT)",
	16, "Dll",
	0};
static CColHdr chDis[] = {
	9, pStrings[ADDRSTR],		// "Address",
	24, "Bytes",
	32, pStrings[TEXTSTR],		// "Text",
	0};
static CColHdr chDmp[] = {
	9, pStrings[ADDRSTR],		// "Address",
	(3 * 16 + 2), "Bytes",
	16, pStrings[TEXTSTR],		// "Text",
	0};
static CColHdr chSec[] = {
	12, pStrings[NAMESTR],	// "Name"
	9, pStrings[SIZESTR],	// "Size"
	9, pStrings[ADDRSTR],		// "Address"
	9, pStrings[SIZRAWSTR],	// "raw Size"
	36, pStrings[FLAGSSTR],		// "Characteristics"
	0};
static CColHdr chRel[] = {
	52, pStrings[TEXTSTR],		// "Text",
	0};

static char * pszModes[NUMMODES] = {
	pStrings[AMODSTR],
	pStrings[PEHDRSTR],
	pStrings[SECTNSTR],
	pStrings[ACODSTR],
	pStrings[ADATSTR],
	pStrings[AEXPSTR],
	pStrings[AIMPSTR],
	pStrings[AIATSTR],
	pStrings[ARESSTR],
	pStrings[RELOCSTR],
	pStrings[ACD16STR]
};
static int iCmds[NUMMODES] = {
	IDM_BLOCK, IDM_PEHEADER, IDM_SECTION, IDM_DISASM, IDM_DATA,
	IDM_EXPORT, IDM_IMPORT, IDM_IAT, IDM_RES, IDM_RELOC, IDM_CODE16};
static int iModes[NUMMODES] = {
	MODVIEW_IMAGE, MODVIEW_HEADER, MODVIEW_SECTION, MODVIEW_CODE, MODVIEW_DATA,
	MODVIEW_EXPORT, MODVIEW_IMPORT, MODVIEW_IAT, MODVIEW_RES, MODVIEW_RELOC, MODVIEW_CODE16};
static CColHdr * tpCH[NUMMODES] = {
	chDmp, chPEH, chSec, chDis, chDmp,
	chExp, chImp, chIAT, chDmp, chRel, chDis};

const static PEATTR PEAttrTab[] = {
	"magic+link version",		offsetof(IMAGE_NT_HEADERS, OptionalHeader.Magic),
	"code size",				offsetof(IMAGE_NT_HEADERS, OptionalHeader.SizeOfCode),
	"initialized data size",	offsetof(IMAGE_NT_HEADERS, OptionalHeader.SizeOfInitializedData),
	"uninitialized data size",	offsetof(IMAGE_NT_HEADERS, OptionalHeader.SizeOfUninitializedData),
	"entry point rva",			offsetof(IMAGE_NT_HEADERS, OptionalHeader.AddressOfEntryPoint),
	"code base rva",			offsetof(IMAGE_NT_HEADERS, OptionalHeader.BaseOfCode),
	"data base rva",			offsetof(IMAGE_NT_HEADERS, OptionalHeader.BaseOfData),
	pStrings[IMGBASSTR],		offsetof(IMAGE_NT_HEADERS, OptionalHeader.ImageBase),
	"section alignment",		offsetof(IMAGE_NT_HEADERS, OptionalHeader.SectionAlignment),
	"file alignment",			offsetof(IMAGE_NT_HEADERS, OptionalHeader.FileAlignment),
	"os version",				offsetof(IMAGE_NT_HEADERS, OptionalHeader.MajorOperatingSystemVersion),
	"image version",			offsetof(IMAGE_NT_HEADERS, OptionalHeader.MajorImageVersion),
	"win32 version",			offsetof(IMAGE_NT_HEADERS, OptionalHeader.Win32VersionValue),
	pStrings[IMGSIZSTR],		offsetof(IMAGE_NT_HEADERS, OptionalHeader.SizeOfImage),
	"size of headers",			offsetof(IMAGE_NT_HEADERS, OptionalHeader.SizeOfHeaders),
	"checksum",					offsetof(IMAGE_NT_HEADERS, OptionalHeader.CheckSum),
	"stack size reserved",		offsetof(IMAGE_NT_HEADERS, OptionalHeader.SizeOfStackReserve),
	"stack size committed",		offsetof(IMAGE_NT_HEADERS, OptionalHeader.SizeOfStackCommit),
	"heap size reserved",		offsetof(IMAGE_NT_HEADERS, OptionalHeader.SizeOfHeapReserve),
	"heap size committed",		offsetof(IMAGE_NT_HEADERS, OptionalHeader.SizeOfHeapCommit),
	"loader flags",				offsetof(IMAGE_NT_HEADERS, OptionalHeader.LoaderFlags),
	"# of rva and sizes",		offsetof(IMAGE_NT_HEADERS, OptionalHeader.NumberOfRvaAndSizes),

	"export directory rva",		offsetof(IMAGE_NT_HEADERS, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress),
	"export directory size",	offsetof(IMAGE_NT_HEADERS, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size),
	"import directory rva",		offsetof(IMAGE_NT_HEADERS, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress),
	"import directory size",	offsetof(IMAGE_NT_HEADERS, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size),
	"resource directory rva",	offsetof(IMAGE_NT_HEADERS, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress),
	"resource directory size",	offsetof(IMAGE_NT_HEADERS, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].Size),
	"exception directory rva",	offsetof(IMAGE_NT_HEADERS, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION].VirtualAddress),
	"exception directory size",	offsetof(IMAGE_NT_HEADERS, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION].Size),
	"security directory rva",	offsetof(IMAGE_NT_HEADERS, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].VirtualAddress),
	"security directory size",	offsetof(IMAGE_NT_HEADERS, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].Size),
	"relocation table rva",		offsetof(IMAGE_NT_HEADERS, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress),
	"relocation table size",	offsetof(IMAGE_NT_HEADERS, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size),
	"debug directory rva",		offsetof(IMAGE_NT_HEADERS, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress),
	"debug directory size",		offsetof(IMAGE_NT_HEADERS, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].Size),
	"arch specific data rva",	offsetof(IMAGE_NT_HEADERS, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_ARCHITECTURE].VirtualAddress),
	"arch specific data size",	offsetof(IMAGE_NT_HEADERS, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_ARCHITECTURE].Size),
	"global ptr rva",			offsetof(IMAGE_NT_HEADERS, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_GLOBALPTR].VirtualAddress),
	"global ptr size",			offsetof(IMAGE_NT_HEADERS, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_GLOBALPTR].Size),
	"TLS directory rva",		offsetof(IMAGE_NT_HEADERS, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress),
	"TLS directory size",		offsetof(IMAGE_NT_HEADERS, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].Size),
	"load config rva",			offsetof(IMAGE_NT_HEADERS, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG].VirtualAddress),
	"load Config size",			offsetof(IMAGE_NT_HEADERS, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG].Size),
	"bound import rva",			offsetof(IMAGE_NT_HEADERS, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT].VirtualAddress),
	"bound import size",		offsetof(IMAGE_NT_HEADERS, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT].Size),
	"import address table rva",	offsetof(IMAGE_NT_HEADERS, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].VirtualAddress),
	"import address table size",offsetof(IMAGE_NT_HEADERS, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].Size),
	"delay load imports rva",	offsetof(IMAGE_NT_HEADERS, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT].VirtualAddress),
	"delay load imports size",	offsetof(IMAGE_NT_HEADERS, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT].Size),
	"COM runtime rva",			offsetof(IMAGE_NT_HEADERS, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR].VirtualAddress),
	"COM runtime size",			offsetof(IMAGE_NT_HEADERS, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR].Size)
};

unsigned char * g_pszFindString = NULL;
int g_iFindStringLength = 0;
BOOL g_bHexFormat = FALSE;

#define NUMPEENTRIES sizeof(PEAttrTab)/sizeof(PEATTR)

HACCEL g_hAccelModule = NULL;
HACCEL g_hAccelMemory = NULL;

// the disassembler supports translation of call/jmp OOOOOOOO
// to call/jmp <Symbol>. For that a callback SearchSymbol is called
// returning 0 indicates Symbol is unknown

IatDoc * g_pIatDoc = NULL;	// callback needs that

extern "C" int __stdcall SearchSymbol(LPSTR pOutStr, _int64 qwAddress);

#define LOWPART_DWORDLONG		((DWORDLONG) 0x00000000FFFFFFFF)
#define LOWDWORD(x)				( (DWORD) ( (x) & LOWPART_DWORDLONG ) )


int __stdcall SearchSymbol(LPSTR pOutStr, _int64 qwAddress)
{
	if (g_pIatDoc)
		return g_pIatDoc->GetNameOfAddress((void * *)LOWDWORD(qwAddress), pOutStr);
	return 0;
}


BOOL CModView::fRegistered = {FALSE};
UINT CModView::dwFlagsOrg = MVF_BYTES;

CModView::CModView(char * pszCaption,int wStyle,int xPos, int yPos, HWND hWndParent, int fMode, UINT dwReqPid, UINT dwReqBase, UINT dwReqSize, BOOL bModule)
{
	HINSTANCE hInstance	= pApp->hInstance;
	int i;
	RECT rect;
	TC_ITEM tci;
	HANDLE handle;
	SYSTEM_INFO si;
	char szStr[260];
//	char szWndCap[260];
	DWORD ul,j;
	HDC hDC;
	TEXTMETRIC tm;
	int iWidth,iHeight;
//	HWND hWnd;
	WNDCLASS wc;
	MEMORYSTATUS ms;
	UINT dwStyle;
	UINT tBase;
	HCURSOR hCursorOld;
	unsigned char * pByte;


	m_hWnd = NULL;
	m_hWndTabCtrl = NULL;
	m_hWndListView = NULL;
	m_pMem = NULL;
	m_pPE = NULL;
	m_dwFlags = dwFlagsOrg;
	m_pDAD = NULL;
	m_pDAD16 = NULL;
	m_pImD = NULL;
	m_pIatDoc = NULL;
	m_dwOffset = 0;
	m_iLVMode = fMode;
	m_pid = dwReqPid;
	m_modBase = dwReqBase;
	m_modSize = dwReqSize;
	m_iModeIndex = 0;
	m_iModeIndexOld = -1;
	m_fCancel = FALSE;
	if (g_pszFindString && (m_pszFindString = (unsigned char *)malloc(g_iFindStringLength+1))) {
		lstrcpy((char *)m_pszFindString, (char *)g_pszFindString);
		m_iFindStringLength = g_iFindStringLength;
		m_bHexFormat = g_bHexFormat;
	} else {
		m_pszFindString = NULL;
		m_iFindStringLength = 0;
		m_bHexFormat = FALSE;
	}

// register wndclass if not done yet

	if (!fRegistered) {
		memset(&wc,0,sizeof(WNDCLASS));
		wc.style = 0;
		wc.cbWndExtra = sizeof(CModView *);
		wc.lpfnWndProc = modwndproc;
		wc.hInstance = hInstance;
		wc.hIcon = LoadIcon(hInstance,MAKEINTRESOURCE(IDI_ICON1));
		wc.hCursor = LoadCursor(NULL,IDC_ARROW);
		wc.hbrBackground = NULL;
		wc.lpszMenuName = 0;
		wc.lpszClassName = "viewmodule";
	
		if (!RegisterClass(&wc)) {
			return;
		}
		fRegistered = TRUE;
		g_hAccelModule = LoadAccelerators(hInstance,MAKEINTRESOURCE(IDR_ACCELERATOR2));
		g_hAccelMemory = LoadAccelerators(hInstance,MAKEINTRESOURCE(IDR_ACCELERATOR3));
	}
	if (bModule)
		m_hAccel = g_hAccelModule;
	else
		m_hAccel = g_hAccelMemory;

// block size mustn't exceed phys mem / 2

	ms.dwLength = sizeof(ms);
	GlobalMemoryStatus(&ms);
	if (m_modSize > (ms.dwTotalPhys / 2)) {
		sprintf(szStr,pStrings[ERRSTR5],ms.dwTotalPhys / (2 * 1024));
		MessageBox(hWndParent,szStr,pStrings[HINTSTR],MB_OK);
		m_modSize = ms.dwTotalPhys / 2;
	}

// alloc memory and read memory of foreign process

	if (m_pMem = (unsigned char *)malloc(m_modSize)) {
		_asm {
			push edi
			mov ecx, this
			mov edi, [ecx]this.m_pMem
			mov ecx, [ecx]this.m_modSize
			shr ecx,2
			mov eax,'CAON'	;fill Memory with "NOAC"
			rep stosd
			pop edi
		}
		hCursorOld = SetCursor(LoadCursor(0,IDC_WAIT));

		if (handle = OpenProcess(PROCESS_VM_READ,FALSE, m_pid)) {
			ReadProcessMemory(handle,(void *)m_modBase,m_pMem,m_modSize,&ul);
#if 0
// if we couldnt read anything, read the first page at least
			if (!ul)
				ReadProcessMemory(handle,(void *)m_modBase,m_pMem,0x1000,&ul);
			for (i = m_modSize - ul, j = ul, tBase = m_modBase;i;i = m_modSize - j - (tBase - m_modBase)) {
				tBase = (tBase + j + 0x1000) & 0xFFFFF000;
				pByte = m_pMem + (tBase - m_modBase);
				i = m_modSize - (tBase - m_modBase);
				ReadProcessMemory(handle,(void *)tBase,pByte,i,&j);
				ul = ul + j;
			}				
#else
			si.dwPageSize = 0x1000;
			GetSystemInfo(&si);
			tBase = m_modBase + ul;
			pByte = m_pMem + ul;
			while (tBase < (m_modBase + m_modSize)) {
//				for (i = m_modSize - ul, j = ul, tBase = m_modBase;ul != m_modSize;i = m_modSize - j - (tBase - m_modBase)) {
				if (ReadProcessMemory(handle, (void *)tBase, pByte, si.dwPageSize, &j))
					ul = ul + si.dwPageSize;
				tBase = tBase + si.dwPageSize;
				pByte = pByte + si.dwPageSize;
			}
#endif
			CloseHandle(handle);
			SetCursor(hCursorOld);
		} else {
			sprintf(szStr,pStrings[ERRSTR1], m_pid);
			MessageBox(hWndParent,szStr,0,MB_OK);
			return;
		}
	}

	if (ul < m_modSize)
		if (!ul) {
			sprintf(szStr,pStrings[ERRSTR2]);
			MessageBeep(MB_OK);
			MessageBox(hWndParent,szStr,0,MB_OK);
			return;
		} else {
			sprintf(szStr,pStrings[ERRSTR3],ul,m_modSize);
			MessageBeep(MB_OK);
			MessageBox(hWndParent,szStr,0,MB_OK);
		}

	if (ul > m_modSize) {
		sprintf(szStr,pStrings[ERRSTR4],ul);
		MessageBeep(MB_OK);
		MessageBox(hWndParent,szStr,0,MB_OK);
	}

	if (bModule)
		m_pPE = (IMAGE_NT_HEADERS *)(m_pMem + *(DWORD *)(m_pMem+0x3C));

// now data is valid, create views

	m_hWnd = CreateWindowEx(0,"viewmodule",
					pszCaption,
					WS_OVERLAPPEDWINDOW | wStyle,
					xPos,
					yPos,
					CW_USEDEFAULT,
					CW_USEDEFAULT,
					(HWND)NULL,// hWndParent,
					(HMENU)NULL,
					hInstance,
					(LPVOID)this);

	if (!m_hWnd) {
		return;
	}

	GetClientRect(m_hWnd, &rect);

	m_hWndTabCtrl = CreateWindowEx(0,WC_TABCONTROL,
					NULL,
					WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP,
					0,
					0,
					rect.right,
					rect.left,
					(HWND)m_hWnd,
					(HMENU)IDC_TABCTRL,
					hInstance,
					(LPVOID)NULL);

	tci.mask = TCIF_TEXT | TCIF_PARAM;
	for (i = 0, j = 0; i < NUMMODES; i++) {
		m_tab[j].uTopIndex = 0;
		m_tab[j].iSelIndex = -1;
		tci.lParam = iModes[i];
		if (iModes[i] == m_iLVMode)
			m_iModeIndex = j;
		tci.iImage = i;
		tci.pszText = pszModes[i];
		if (bModule == FALSE) {
			switch (tci.lParam) {
			case MODVIEW_IMAGE:
				tci.pszText = pStrings[ADATSTR];
				TabCtrl_InsertItem(m_hWndTabCtrl, j, &tci);
				j++;
				break;
			case MODVIEW_CODE: 
				TabCtrl_InsertItem(m_hWndTabCtrl, j, &tci);
				j++;
				break;
			case MODVIEW_CODE16: 
				TabCtrl_InsertItem(m_hWndTabCtrl, j, &tci);
				j++;
				break;
			}
		} else {
			if (tci.lParam != MODVIEW_CODE16) {
				TabCtrl_InsertItem(m_hWndTabCtrl, j, &tci);
				j++;
			}
		}
	}

	dwStyle = LVS_OWNERDATA | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER;
	m_hWndListView = CreateWindowEx( WS_EX_CLIENTEDGE,
					WC_LISTVIEW,
					NULL,
					WS_CHILD | WS_VISIBLE | WS_TABSTOP | dwStyle,
					0,0,0,0,
//					0,0,rect.right,rect.bottom,
					m_hWnd,
					(HMENU)IDC_LISTVIEW,
					hInstance,
					NULL);

	if (!m_hWndListView) {
		return;
	}

	ListView_SetExtendedListViewStyle(m_hWndListView,LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);
	SetWindowFont(m_hWndListView, (WPARAM)GetStockObject(ANSI_FIXED_FONT), FALSE);

	m_hWndStatusBar = CreateWindowEx ( 0,
					STATUSCLASSNAME,
					NULL,
					WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP | CCS_BOTTOM,
					0,0,0,0,
					m_hWnd,
					(HMENU)IDC_STATUSBAR,
					hInstance,
					NULL);

#if 0
	SendMessage(m_hWndTabCtrl,WM_SETFONT,
				SendMessage(m_hWndStatusBar,WM_GETFONT,0,0),0);
#else
	SetWindowFont(m_hWndTabCtrl, (WPARAM)GetStockObject(ANSI_VAR_FONT), FALSE);
#endif

	TabCtrl_SetCurSel(m_hWndTabCtrl, m_iModeIndex);

// all views are hopefully created, now adjust their sizes

	SetListViewCols();
	if (rLastRect.right) {	// Parameter des letzten Windows nehmen
		MoveWindow(m_hWnd,
			rLastRect.left,
			rLastRect.top,
			rLastRect.right - rLastRect.left,
			rLastRect.bottom - rLastRect.top,
			FALSE);
	} else { // oder sind wir das erste Window?
		hDC = GetDC(m_hWndListView);
		GetTextMetrics(hDC,&tm);
		ReleaseDC(m_hWndListView,hDC);
		iWidth = GetSystemMetrics(SM_CXSIZE) * 2;
		iHeight = GetSystemMetrics(SM_CYCAPTION) + 2 * GetSystemMetrics(SM_CYSIZE);
		iHeight = iHeight + 20 * tm.tmHeight; 
//		for (i = 0; i < iColumns;i++)
//			iWidth = iWidth + ListView_GetColumnWidth(m_hWndListView,i);
		iWidth = iWidth + (9 + 16 * 3 + 2 + 16) * (tm.tmAveCharWidth + XPIXELS);
		SetWindowPos(m_hWnd,0,0,0,iWidth,iHeight,SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
	}  

//	use UpdateListView(-1) to display the data

	UpdateListView(-1);
}
		  
CModView::~CModView()
{
	HWND hWndTmp;

	GetListViewCols(m_iModeIndexOld);

	if (m_pDAD)
		delete m_pDAD;
	if (m_pDAD16)
		delete m_pDAD16;
	if (m_pImD)
		delete m_pImD;
	if (m_pIatDoc)
		delete m_pIatDoc;

	if (m_pMem)
		free(m_pMem);
	if (m_pszFindString)
		free(m_pszFindString);
	if (m_hWnd) {			// might be destructor is not called by main window
		hWndTmp = m_hWnd;	// since view is independant.
		m_hWnd = NULL;
		DestroyWindow(hWndTmp);
	}
}

BOOL CModView::ShowWindow(int iParms)
{
	if (m_hWnd && m_hWndListView) {
		::ShowWindow(m_hWnd,iParms);
		return TRUE;
	} else
		return FALSE;
}

BOOL CModView::GetListViewCols(int iMode)
{
	int i;
	TEXTMETRIC tm;
	CColHdr * pCH;
	HDC hDC;

	if (iMode == -1)
		return FALSE;

	hDC = GetDC(m_hWndListView);
	GetTextMetrics(hDC,&tm);
	ReleaseDC(m_hWndListView,hDC);

	for (i = 0,pCH = tpCH[iMode];pCH->iSize;i++,pCH++) {
		pCH->iSize = ListView_GetColumnWidth(m_hWndListView,i) / (tm.tmAveCharWidth + XPIXELS);
	}
	return TRUE;
}

// set column headers of list view

BOOL CModView::SetListViewCols()
{
	int i;
	LV_COLUMN lvc;
	CColHdr * pCH;
	TEXTMETRIC tm;
	HDC hDC;

	GetListViewCols(m_iModeIndexOld);

	hDC = GetDC(m_hWndListView);
	GetTextMetrics(hDC,&tm);
	ReleaseDC(m_hWndListView,hDC);
	// alte Spalten entfernen
	while (ListView_DeleteColumn(m_hWndListView,0));

	for (i = 0, pCH = tpCH[m_iModeIndex]; pCH->iSize; i++,pCH++) {
		lvc.mask = LVCF_TEXT | LVCF_WIDTH;
		lvc.cx = pCH->iSize * (tm.tmAveCharWidth + XPIXELS);
		lvc.pszText = pCH->pText;
		ListView_InsertColumn(m_hWndListView,i,&lvc);
	}
	m_iModeIndexOld = m_iModeIndex;

	return TRUE;
}

typedef struct {
	DWORD rva;
	DWORD size;
} RELOCENTRY;

// updatelistview 

int CModView::UpdateListView(int fNewMode)
{
	PIMAGE_SECTION_HEADER pSection;
	HCURSOR hCursorOld;
	RELOCENTRY * pRelocs;
	unsigned int i,j,k;
	char szStr[260];
	IMAGE_EXPORT_DIRECTORY * pED;


	SendMessage(m_hWndListView,WM_SETREDRAW,0,0);

	SendMessage(m_hWndStatusBar, SB_SETTEXT, 0, (LPARAM)pStrings[BUSYSTR]); 

	hCursorOld = SetCursor(LoadCursor(0,IDC_WAIT));

//	ListView_DeleteAllItems(m_hWndListView);

	if (fNewMode != -1)
		m_iLVMode = fNewMode;

	for (i = 0;i < NUMMODES;i++)
		if (iModes[i] == m_iLVMode) {
			m_iModeIndex = i;
			break;
		}

	m_iLastFindPos = 0;

	SetListViewCols();

	ListView_SetItemCount(m_hWndListView, 0);
	szStr[0] = 0;

	switch (m_iLVMode) {
	case MODVIEW_HEADER:
		ListView_SetItemCount(m_hWndListView,NUMPEENTRIES);
		break;
	case MODVIEW_SECTION:
		ListView_SetItemCount(m_hWndListView,m_pPE->FileHeader.NumberOfSections);
		break;
	case MODVIEW_CODE:
		if (!m_pDAD)
			if (!(m_pDAD = new CDisAsmDoc()))
				return 0;

		if (m_pPE) {
			if (!m_pIatDoc)			// get an IAT doc for symbol translation
				if (m_pIatDoc = new IatDoc())
					m_pIatDoc->UpdateDocument(m_pPE, m_pMem, m_modBase);
			if (m_pPE->OptionalHeader.BaseOfCode) {
				m_pDAD->UpdateDocument(m_pMem,
						m_pPE->OptionalHeader.BaseOfCode,
						m_pPE->OptionalHeader.SizeOfCode, DADF_WIN32);
				sprintf(szStr,pStrings[CODESTAT],
					m_pPE->OptionalHeader.BaseOfCode + m_modBase,
					m_pPE->OptionalHeader.SizeOfCode,
					m_pPE->OptionalHeader.SizeOfCode);
			} else {
				pSection = IMAGE_FIRST_SECTION(m_pPE);
				for (i = 0; i < m_pPE->FileHeader.NumberOfSections;i++, pSection++)
					if (pSection->Characteristics & IMAGE_SCN_CNT_CODE) {
						m_pDAD->UpdateDocument(m_pMem,
							pSection->VirtualAddress,
							pSection->Misc.VirtualSize, DADF_WIN32);
						sprintf(szStr,pStrings[CODESTAT],
							pSection->VirtualAddress + m_modBase,
							pSection->Misc.VirtualSize, pSection->Misc.VirtualSize);
						break;
					}
			}
		} else {
			m_pDAD->UpdateDocument(m_pMem, 0, m_modSize, DADF_WIN32);
			sprintf(szStr,pStrings[CODESTAT], m_modBase, m_modSize, m_modSize);
		}
		ListView_SetItemCount(m_hWndListView,m_pDAD->GetLines());

		break;
	case MODVIEW_CODE16:
		if (!m_pDAD16)
			if (!(m_pDAD16 = new CDisAsmDoc()))
				return 0;
		m_pDAD16->UpdateDocument(m_pMem, 0, m_modSize, DADF_WIN16);
		ListView_SetItemCount(m_hWndListView, m_pDAD16->GetLines());
		sprintf(szStr,pStrings[CODESTAT], m_modBase, m_modSize, m_modSize);
		break;
	case MODVIEW_DATA:
		if (m_pPE->OptionalHeader.BaseOfData) {
			if (m_pPE->OptionalHeader.BaseOfData > m_modSize) 
				i = 0;
			else {
				if ((m_pPE->OptionalHeader.BaseOfData + m_pPE->OptionalHeader.SizeOfInitializedData) > m_modSize) 
					i = (m_modSize - m_pPE->OptionalHeader.BaseOfData) / 16;
				else {
					i = m_pPE->OptionalHeader.SizeOfInitializedData / 16;
					if (m_pPE->OptionalHeader.SizeOfInitializedData % 16)
						i++;
				}
			}
			ListView_SetItemCount(m_hWndListView,i);
			m_dwOffset = m_pPE->OptionalHeader.BaseOfData;
			sprintf(szStr,pStrings[DATASTAT],
				m_pPE->OptionalHeader.BaseOfData + m_modBase,
				m_pPE->OptionalHeader.SizeOfInitializedData,
				m_pPE->OptionalHeader.SizeOfInitializedData);
		} else {
			pSection = IMAGE_FIRST_SECTION(m_pPE);
			for (i = 0; i < m_pPE->FileHeader.NumberOfSections;i++, pSection++)
				if (pSection->Characteristics & IMAGE_SCN_CNT_INITIALIZED_DATA) {
					i = pSection->Misc.VirtualSize / 16;
					if (pSection->Misc.VirtualSize % 16)
						i++;
					ListView_SetItemCount(m_hWndListView,i);
					m_dwOffset = pSection->VirtualAddress;
					sprintf(szStr,pStrings[DATASTAT],
						pSection->VirtualAddress + m_modBase,
						pSection->Misc.VirtualSize,
						pSection->Misc.VirtualSize);
				}
		}
		break;
	case MODVIEW_EXPORT:
		if (m_pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size) {
			pED = (IMAGE_EXPORT_DIRECTORY *)(m_pMem + m_pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
//			ListView_SetItemCount(m_hWndListView,pED->NumberOfNames);
			ListView_SetItemCount(m_hWndListView,pED->NumberOfFunctions);
			sprintf(szStr,pStrings[EXPSTAT],
				m_pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress + m_modBase,
				m_pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size,
				m_pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size);
		}
		break;	
	case MODVIEW_IMPORT:
		if (!m_pImD)
			if (!(m_pImD = new ImpDoc()))
				return 0;

		m_pImD->UpdateDocument(m_pPE,m_pMem);

		ListView_SetItemCount(m_hWndListView,m_pImD->GetLines());

		sprintf(szStr,pStrings[IMPSTAT],
				m_pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress + m_modBase,
				m_pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size,
				m_pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size);
		break;
	case MODVIEW_IAT:
		if (!m_pIatDoc)
			if (!(m_pIatDoc = new IatDoc()))
				return 0;

		m_pIatDoc->UpdateDocument(m_pPE, m_pMem, m_modBase);

		ListView_SetItemCount(m_hWndListView,m_pIatDoc->GetLines());

		if (m_pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].Size) {
			sprintf(szStr,pStrings[IATSTAT],
				m_pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].VirtualAddress + m_modBase,
				m_pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].Size,
				m_pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].Size);
		}
		break;
	case MODVIEW_RELOC:
		if (m_pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size) {
			pRelocs = (RELOCENTRY *)(m_pMem + m_pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
			i = m_pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;

			j = 0;
			while (i) {
				k = (pRelocs->size - sizeof (RELOCENTRY)) / sizeof(WORD);
				j = j + 1 + k / 4;
				if (k % 4)
					j++;
				if (i >= pRelocs->size)
					i = i - pRelocs->size;
				else
					i = 0;
				pRelocs = (RELOCENTRY *)((BYTE *)pRelocs + pRelocs->size);
			}
			ListView_SetItemCount(m_hWndListView, j);
			sprintf(szStr,pStrings[RELOCSTAT],
				m_pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress + m_modBase,
				m_pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size,
				m_pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size);
		}
		break;
	case MODVIEW_RES:
		if (m_pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].Size) {
			i = m_pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].Size / 16;
			if (m_pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].Size % 16)
				i++;
			ListView_SetItemCount(m_hWndListView,i);
			m_dwOffset = m_pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress;
			sprintf(szStr,pStrings[RESSTAT],
				m_pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress + m_modBase,
				m_pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].Size,
				m_pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].Size);
		}
		break;
	default:
		m_dwOffset = 0;
		i = m_modSize / 16;
		if (m_modSize % 16)
			i++;
		ListView_SetItemCount(m_hWndListView,i);
		if (!m_pPE)
			sprintf(szStr,pStrings[DATASTAT],m_modBase,m_modSize,m_modSize);
		else
			sprintf(szStr,pStrings[MODSTAT],m_modBase,m_modSize,m_modSize);
		break;
	}

	while ((i = ListView_GetNextItem(m_hWndListView,-1,LVIS_SELECTED)) != -1)
		ListView_SetItemState(m_hWndListView,i,0,LVIS_FOCUSED | LVIS_SELECTED);

	j = ListView_GetCountPerPage(m_hWndListView);

	if (m_tab[m_iModeIndex].uTopIndex == -1) {	// eventuelle neue Vorgabe berücksichtigen
		switch (m_iLVMode) {
		case MODVIEW_CODE:
		case MODVIEW_CODE16:
			if (m_iLVMode == MODVIEW_CODE)
				i = m_pDAD->GetActualLine();
			else
				i = m_pDAD16->GetActualLine();
			if (i == -1) {
				MessageBeep(-1);
				m_tab[m_iModeIndex].uTopIndex = 0;
				m_tab[m_iModeIndex].iSelIndex = -1;
			} else {
				m_tab[m_iModeIndex].iSelIndex = i;
				if (m_tab[m_iModeIndex].iSelIndex > (int)j / 2)
					m_tab[m_iModeIndex].uTopIndex = m_tab[m_iModeIndex].iSelIndex -  j / 2;
				else
					m_tab[m_iModeIndex].uTopIndex = 0;
			}
			break;
		}
	}

	SendMessage(m_hWndStatusBar, SB_SETTEXT, 1, (LPARAM)szStr); 

	sprintf(szStr,pStrings[LINESTR],0, ListView_GetItemCount(m_hWndListView));
	SendMessage(m_hWndStatusBar, SB_SETTEXT, 2, (LPARAM)szStr); 

	ListView_EnsureVisible(m_hWndListView,m_tab[m_iModeIndex].uTopIndex + j - 1,0);
	ListView_EnsureVisible(m_hWndListView,m_tab[m_iModeIndex].uTopIndex,0);
	if (m_tab[m_iModeIndex].iSelIndex != -1)
		ListView_SetItemState(m_hWndListView,
							m_tab[m_iModeIndex].iSelIndex,
							LVIS_SELECTED | LVIS_FOCUSED,
							LVIS_SELECTED | LVIS_FOCUSED);

	SendMessage(m_hWndListView,WM_SETREDRAW,1,0);

	SendMessage(m_hWndStatusBar, SB_SETTEXT, 0, (LPARAM)pStrings[NBUSYSTR]); 

	SetCursor(hCursorOld);

	return 1;
}

// scan "name of ordinals" table

int searchot(short * pOT,int iEntries, int iRequest)
{
	int rc = -1;

	__asm {
		mov ecx,iEntries
		jecxz label1
		mov edi,pOT
		mov eax,iRequest
		repnz scasw
		jnz label1
		lea eax,[edi-2]
		sub eax,pOT
		shr eax,1
		mov rc,eax
	label1:
	}
	return rc;
}

// display (update) a listview line

int CModView::FillItem(HWND hWnd, int iItem, int iSubItem, char * pszText, int cchTextMax)
{
	unsigned char * pStr;
	RELOCENTRY * pRelocs;
	PIMAGE_SECTION_HEADER pSection;
	char * pStr2;
	IMAGE_EXPORT_DIRECTORY * pED;
	DWORD * pDW;
	UINT * pEAT;
	CDisAsmDoc * pDAD;
	UINT * pExpNames;
	short * pOT;	// export table: pointer to "ordinals of functions"
	int i,j,k;
	BOOL bRelocDir;
	char * pszRelType[4];
//	WORD wRelValue[4];
	WORD * pW;
	int iType;
	char szFormat[40];
	char szStr[512];

	switch (m_iLVMode) {
	case MODVIEW_HEADER:
		switch (iSubItem) {
		case 0:
			sprintf(szStr,"%s",PEAttrTab[iItem].pName);
			break;
		case 1:
			pStr = (unsigned char *)m_pPE + PEAttrTab[iItem].dwOffset;
			sprintf(szStr,"%X",*(UINT *)pStr);
			break;
		}
		strncpy(pszText,szStr,cchTextMax);
		break;
	case MODVIEW_SECTION:
		pSection = IMAGE_FIRST_SECTION(m_pPE);
		pSection = pSection + iItem;
		switch (iSubItem) {
		case 0:
			strncpy(szStr, (LPSTR)pSection->Name, sizeof(pSection->Name));
			szStr[sizeof(pSection->Name)] = 0;
			break;
		case 1:
			sprintf(szStr, "%X", pSection->Misc.VirtualSize);
			break;
		case 2:
			sprintf(szStr, "%X", pSection->VirtualAddress);
			break;
		case 3:
			sprintf(szStr, "%X", pSection->SizeOfRawData);
			break;
		case 4:
			sprintf(szStr, "%X", pSection->Characteristics);
			if (pSection->Characteristics & IMAGE_SCN_CNT_CODE)
				strcat(szStr, ", code");
			if (pSection->Characteristics & IMAGE_SCN_CNT_INITIALIZED_DATA)
				strcat(szStr, ", init. data");
			if (pSection->Characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA)
				strcat(szStr, ", uninit. data");
			if (pSection->Characteristics & IMAGE_SCN_MEM_LOCKED)
				strcat(szStr, ", locked");
			if (pSection->Characteristics & IMAGE_SCN_MEM_PRELOAD)
				strcat(szStr, ", preload");
			if (pSection->Characteristics & IMAGE_SCN_MEM_DISCARDABLE)
				strcat(szStr, ", discardable");
			if (pSection->Characteristics & IMAGE_SCN_MEM_NOT_CACHED)
				strcat(szStr, ", not cachable");
			if (pSection->Characteristics & IMAGE_SCN_MEM_NOT_PAGED)
				strcat(szStr, ", not pageable");
			if (pSection->Characteristics & IMAGE_SCN_MEM_SHARED)
				strcat(szStr, ", shared");
			if (pSection->Characteristics & IMAGE_SCN_MEM_EXECUTE)
				strcat(szStr, ", executable");
			if (pSection->Characteristics & IMAGE_SCN_MEM_READ)
				strcat(szStr, ", readable");
			if (pSection->Characteristics & IMAGE_SCN_MEM_WRITE)
				strcat(szStr, ", writeable");
			break;
		}
		strncpy(pszText,szStr,cchTextMax);
		break;
	case MODVIEW_RELOC:
		pRelocs = (RELOCENTRY *)(m_pMem + m_pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
		j = 0;
		bRelocDir = TRUE;
		while (j < iItem) {
			if (bRelocDir) {
				k = (pRelocs->size - sizeof (RELOCENTRY)) / sizeof(WORD);
				pW = (WORD *)((BYTE *)pRelocs + sizeof(RELOCENTRY));
				bRelocDir = FALSE;
			} else {
				if (k > 4) {
					pW = pW + 4;
					k = k - 4;
				} else {
					k = 0;
					pRelocs = (RELOCENTRY *)((BYTE *)pRelocs + pRelocs->size);
					bRelocDir = TRUE;
				}
			}
			j++;
		}

		if (bRelocDir)
			sprintf(szStr, "RVA=%X, Size=%X", pRelocs->rva, pRelocs->size);
		else {
			szFormat[0] = 0;
			for (i = 0; i < 4; i++)
				if (i < k) {
					lstrcat( szFormat, "%3X,%-10s");
					iType = *(pW+i)>>12;
					switch (iType) {
					case 0:
						pszRelType[i] = "Null";
						break;
					case 1:
						pszRelType[i] = "high";
						break;
					case 2:
						pszRelType[i] = "low";
						break;
					case 3:
						pszRelType[i] = "highlow";
						break;
					case 4:
						pszRelType[i] = "highadj";
						break;
					default:
						pszRelType[i] = "???";
					}
				}

			sprintf(szStr, szFormat,
				*(pW+0) & 0xFFF, pszRelType[0],
				*(pW+1) & 0xFFF, pszRelType[1],
				*(pW+2) & 0xFFF, pszRelType[2],
				*(pW+3) & 0xFFF, pszRelType[3]);
		}
		strncpy(pszText,szStr,cchTextMax);
		break;
	case MODVIEW_EXPORT:
		pED = (IMAGE_EXPORT_DIRECTORY *)(m_pMem + m_pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
		pExpNames = (UINT *)(m_pMem + pED->AddressOfNames);
		pEAT = (UINT *)(m_pMem + pED->AddressOfFunctions);
		pOT = (short *)(m_pMem + pED->AddressOfNameOrdinals);
		szStr[0] = 0;
		switch (iSubItem) {
		case 0:
			sprintf(szStr,"%u",iItem);
			break;
		case 1:
			if (*(pEAT + iItem))
				sprintf(szStr,"%8X",*(pEAT + iItem) + m_modBase);
			else
				strcpy(szStr,"<Null>");
			break;
		case 2:
#if 0	// searchot will find exports with names only,
        // therefore dont use here!
			if ((i = searchot(pOT,pED->NumberOfNames,iItem)) != -1)
				sprintf(szStr,"%u",pED->Base + *(pOT + i));
#else
			if (*(pEAT + iItem))
				sprintf(szStr,"%u",pED->Base + iItem);
#endif
			break;
		case 3:
			if ((i = searchot(pOT,pED->NumberOfNames,iItem)) != -1)
				sprintf(szStr,"%s",m_pMem + *(pExpNames + i));
			break;
		}
		strncpy(pszText,szStr,cchTextMax);
		break;
	case MODVIEW_CODE:
	case MODVIEW_CODE16:
		if (m_iLVMode == MODVIEW_CODE)
			pDAD = m_pDAD;
		else
			pDAD = m_pDAD16;

		pStr = pDAD->GetAddressFromLine(iItem);
		if (pStr) {
			switch (iSubItem) {
			case 0:
				sprintf(szStr,"%X",pStr - m_pMem + m_modBase);
				break;
			case 1:
				pDAD->GetCodeBytes(pStr,szStr);
				break;
			case 2:
				g_pIatDoc = m_pIatDoc;	// activate symbol translation
				pDAD->GetSource(pStr, szStr, m_modBase - (DWORD)m_pMem);
				g_pIatDoc = NULL;		// deactivate symbol translation
				break;
			}
		} else {
			sprintf(szStr,pStrings[ERRSTR6],iItem);
		}
		strncpy(pszText,szStr,cchTextMax);
		break;
	case MODVIEW_IMPORT:
		m_pImD->GetValue(iItem,iSubItem,szStr);
		strncpy(pszText,szStr,cchTextMax);
		break;
	case MODVIEW_IAT:
		m_pIatDoc->GetValue(iItem,iSubItem,szStr);
		strncpy(pszText,szStr,cchTextMax);
		break;
	default:
		switch (iSubItem) {
		case 0:
			sprintf(szStr,"%X",m_modBase + m_dwOffset + iItem * 16);
			break;
		case 1:
			pStr = m_pMem + m_dwOffset + iItem * 16;
			if (m_dwFlags & MVF_DWORDS) {
				pDW = (DWORD *)pStr;
				sprintf(szStr,"%08x  %08x  %08x  %08x",*(pDW+0),*(pDW+1),*(pDW+2),*(pDW+3));
			} else
				sprintf(szStr,
					"%02x %02x %02x %02x %02x %02x %02x %02x | %02x %02x %02x %02x %02x %02x %02x %02x",
					*(pStr+0),*(pStr+1),*(pStr+2),*(pStr+3),
					*(pStr+4),*(pStr+5),*(pStr+6),*(pStr+7),
					*(pStr+8),*(pStr+9),*(pStr+10),*(pStr+11),
					*(pStr+12),*(pStr+13),*(pStr+14),*(pStr+15));
			i = (iItem + 1) * 16 - m_modSize;
			if (i > 0)	{	// ist am ende unvollständige Zeile?
				szStr[(16 - i) * 3 + 2 * (i<8)] = 0;
			}
			break;
		case 2:
			pStr = m_pMem + m_dwOffset + iItem * 16;
			for (i = 0,pStr2 = szStr;i < 16;i++,pStr2++) {
				*pStr2 = *(pStr+i);
				if ((*pStr2 == 0) || (*pStr2 == 10) || (*pStr2 == 13))
					*pStr2 = '.';
			}
			*pStr2 = 0;
			i = (iItem + 1) * 16 - m_modSize;
			if (i > 0)
				szStr[(16 - i)] = 0;
			break;
		} // end switch iSubItem
		strncpy(pszText,szStr,cchTextMax);
		break;
	}	// end switch m_iLVMode
	return 1;
}

// write text (dump, disassembler listing) to file/clipboard 

#define BLOCKLEN 0x8000

int CModView::SaveListViewContent(HWND hWnd, PSTR pszFileName)
{
	int i,iNextItem,  hFile;
	CColHdr * pCH;
	int iLines, iLinesAll, iNewPos, iOldPos = 0;
	int j, iSize = 0;
	int iColumns;
	int iReadType;
	LV_ITEM lvi;
	char szLine[260];
	char szStr[260];
	char * pStr;
	HANDLE hList = 0;
	LPSTR pszLine = 0;
	DWORD dwSize;
	HGLOBAL hGlobal, hNewGlobal;

	for (iColumns = 0, pCH = tpCH[m_iModeIndex]; pCH->iSize; iColumns++,pCH++);

	switch (m_iSaveMode) {

	case 1:	// write to clipboard
		iLinesAll = ListView_GetSelectedCount(m_hWndListView);
		iReadType = LVNI_SELECTED;
		dwSize = BLOCKLEN;
		hGlobal = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE,dwSize);
		if (hGlobal) {
			pStr = (char *)GlobalLock(hGlobal);
		} else {
			pStr = NULL;
		}
		strcpy(szStr,pStrings[COPY2CLPB]);
		break;

	case 0:	// write to file
		iLinesAll = ListView_GetItemCount(m_hWndListView);
		iReadType = LVNI_ALL;
		hFile = _sopen(pszFileName,_O_WRONLY | _O_CREAT | _O_BINARY | _O_TRUNC, _SH_DENYWR, _S_IREAD | _S_IWRITE);
		if (hFile == -1) {
			sprintf(szStr,pStrings[ERRSTR7],pszFileName);
			MessageBox(hWnd,szStr,0,MB_OK);
			m_fCancel = TRUE;
			break;
		}
		pStr = (char *)malloc(BLOCKLEN);
		strcpy(szStr,pStrings[SAVE2DISK]);
		break;
	}
	if (!m_fCancel && !pStr) {
		MessageBox(hWnd,pStrings[MEMERR1],0,MB_OK);
		m_fCancel = TRUE;
	}

	SetWindowText(hWnd,szStr);

	iLines = 0;
	iNextItem = ListView_GetNextItem(m_hWndListView,-1,iReadType);
	while ((iNextItem != -1) && !(m_fCancel)) {
		szLine[0] = 0;
		lvi.iItem = iNextItem;
		pszLine = szLine;
		for (i = 0; i < iColumns;i++) {
			if (!FillItem(hWnd,
						iNextItem,
						i,
						pszLine,
						sizeof(szLine) - (pszLine - szLine)))
				break;
			j = strlen(pszLine);
			if (((m_iLVMode == MODVIEW_CODE) || (m_iLVMode == MODVIEW_CODE16)) && (i == 1)) {
				if (j < 8 * 3) {
					memset(pszLine + j,' ',7 * 3);
					j = 8 * 3;
				}
			}
			pszLine = pszLine + j;
			if ((i != (iColumns - 1)) && j)
				*pszLine++ = '\t';
		}
		*pszLine++ = '\r';
		*pszLine++ = '\n';
		*pszLine = 0;
		j =  pszLine - szLine;
		memcpy(pStr+iSize,szLine,j);
		iSize = iSize + j;
		switch (m_iSaveMode) {
		case 0:
			if (iSize > (BLOCKLEN - sizeof(szLine))) {
				i = _write(hFile,pStr,iSize);
				if (i =! iSize) {
					MessageBeep(MB_OK);
					MessageBox(hWnd,pStrings[ERRSTR11],0,MB_OK);
					m_fCancel = TRUE;
					break;
				}
				iSize = 0;
			}
			break;
		case 1:
			if (iSize > (int)(dwSize - sizeof(szLine))) {
				GlobalUnlock(hGlobal);
				dwSize = dwSize + BLOCKLEN;
				hNewGlobal = GlobalReAlloc(hGlobal,dwSize,GMEM_MOVEABLE);
				if (!hNewGlobal) {
					MessageBeep(MB_OK);
					MessageBox(hWnd,pStrings[MEMERR1],0,MB_OK);
					m_fCancel = TRUE;
					break;
				}
				hGlobal = hNewGlobal;
				pStr = (char *)GlobalLock(hGlobal);
			}
			break;
		}
		iLines++;
		if (hWndProgress) {
			iNewPos = (iLines * 100)/iLinesAll;
			if (iNewPos > 100) iNewPos = 100;
			if (iNewPos >= (iOldPos + 3))
				iOldPos = SendMessage (hWndProgress, PBM_SETPOS, iNewPos, 0L);
		}
		iNextItem = ListView_GetNextItem(m_hWndListView,iNextItem,iReadType);
	}

	switch (m_iSaveMode) {
	case 0:
		if (m_fCancel == FALSE) {
			if (iSize)
				_write(hFile,pStr,iSize);
			free(pStr);
			_close(hFile);
			if (hWndProgress)
				SendMessage (hWndProgress, PBM_SETPOS, 100, 0L);
			sprintf(szStr,pStrings[INFSTR8],pszFileName);
			MessageBox(hWnd,szStr,pStrings[HINTSTR],MB_OK);
		} else {
			free(pStr);
			_close(hFile);
			DeleteFile(pszFileName);
		}
		break;
	case 1:
		GlobalUnlock(hGlobal);
		if (m_fCancel == FALSE) {
			*(pStr+iSize) = 0;
			if (hWndProgress)
				SendMessage (hWndProgress, PBM_SETPOS, 100, 0L);
			OpenClipboard(hWnd);
			EmptyClipboard();
			SetClipboardData(CF_TEXT,hGlobal);
			CloseClipboard();
		} else {
			GlobalFree(hGlobal);
		}
		break;
	}
	if (hWndProgress)
		EndDialog(hWnd,0);
	return 1;
}

// write module in a file

int CModView::SaveRawBytes(HWND hWnd, PSTR pszFileName)
{
	int i,hFile;
	char szStr[280];

	hFile = _sopen(pszFileName,_O_WRONLY | _O_CREAT | _O_BINARY | _O_TRUNC, _SH_DENYWR, _S_IREAD | _S_IWRITE);

	if (hFile != -1) {
		i = _write(hFile,m_pMem,m_modSize);
		_close(hFile);
	} else
		i = 0;

	if (i == (int)m_modSize)
		sprintf(szStr,pStrings[INFSTR9],i,pszFileName);
	else
		if (i)
			sprintf(szStr,pStrings[ERRSTR10],i,m_modSize,pszFileName);
		else
			sprintf(szStr,pStrings[ERRSTR11],pszFileName);

	if (i == (int)m_modSize)
		MessageBox(hWnd,szStr,pStrings[HINTSTR],MB_OK);
	else {
		MessageBeep(MB_OK);
		MessageBox(hWnd,szStr,0,MB_OK);
	}
	return i;
}

DWORD WINAPI savelistviewcontentproc(VOID * pVoid)
{
	int rc;
	CModView * pMV = (CModView *)pVoid;

	rc = pMV->SaveListViewContent(GetParent(pMV->hWndProgress),pMV->pStrTmp);
	return (DWORD)rc;
}

BOOL CModView::ProgressDlg(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

	static HANDLE hThread;	// 2. Thread zur Beendigung von Prozessen/Threads
	DWORD dwThreadId;
	BOOL rc = FALSE;

	switch (message) {
	case WM_INITDIALOG:
		m_fCancel = FALSE;
		hWndProgress = GetDlgItem(hWnd,IDC_PROGRESS1);
//		SetWindowLong(hWndProgress,GWL_EXSTYLE,GetWindowLong(hWndProgress,GWL_EXSTYLE) | PBS_SMOOTH);
		hThread = CreateThread(0,0x1000,savelistviewcontentproc,this,0,&dwThreadId);
		rc = 1;
		break;
	case WM_CLOSE:
		m_fCancel = TRUE;
		rc = 1;
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCANCEL:
			m_fCancel = TRUE;
			break;
		}
		break;
	}
	return rc;
}

BOOL CALLBACK progressdlgproc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	CModView * pMV;

	if (message == WM_INITDIALOG)
		SetWindowLong(hWnd,DWL_USER,lParam);

	pMV = (CModView *)GetWindowLong(hWnd,DWL_USER);

	if (pMV)
		return pMV->ProgressDlg(hWnd,message,wParam,lParam);

	return 0;
}

BOOL CModView::FindDlg(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

	BOOL rc = FALSE;
	HWND hWndEdit;
	DWORD dwLength;
	unsigned char * pszFindString;

	switch (message) {
	case WM_INITDIALOG:
		if (m_pszFindString)
			SetDlgItemText(hWnd,IDC_EDIT1,(char *)m_pszFindString);

		if ((m_iLVMode == MODVIEW_IMAGE) || (m_iLVMode == MODVIEW_DATA) || (m_iLVMode == MODVIEW_RES))
			EnableWindow(GetDlgItem(hWnd,IDC_CHECK1),TRUE);
		else
			m_bHexFormat = FALSE;

		if (m_bHexFormat) 
			CheckDlgButton( hWnd, IDC_CHECK1, BST_CHECKED);

		rc = 1;
		break;
	case WM_CLOSE:
		EndDialog(hWnd, 0);
		rc = 1;
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCANCEL:
			EndDialog(hWnd, 0);
			break;
		case IDOK:
			hWndEdit = GetDlgItem(hWnd,IDC_EDIT1);
			dwLength = GetWindowTextLength(hWndEdit);
			if (dwLength == 0) {
				MessageBeep(MB_OK);
				break;
			}
			if (pszFindString = (unsigned char *)malloc(dwLength+1)) {
				GetWindowText(hWndEdit, (char *)pszFindString, dwLength+1);
			} else
				break;


			m_bHexFormat = IsDlgButtonChecked(hWnd,IDC_CHECK1);
			if (m_bHexFormat) {		// check if hex nibbles are correct
				_strupr((char *)pszFindString);
				if (strspn((char *)pszFindString,"0123456789ABCDEF") != dwLength) {
					MessageBeep(MB_OK);
					MessageBox(hWnd,pStrings[HEXNIBER1],0,MB_OK);
					break;
				}
				if (dwLength & 1) {
					MessageBeep(MB_OK);
					MessageBox(hWnd,pStrings[HEXNIBER2],0,MB_OK);
					break;
				}
			}


			if (m_pszFindString)
				free(m_pszFindString);
			m_pszFindString = pszFindString;
			m_iFindStringLength = dwLength;

			if (g_pszFindString)
				free(g_pszFindString);
			if (g_pszFindString = (unsigned char *)malloc(dwLength+1)) {
				lstrcpy((char *)g_pszFindString, (char *)m_pszFindString);
				g_iFindStringLength = dwLength;
				g_bHexFormat = m_bHexFormat;
			}


			EndDialog(hWnd, 1);
			break;
		}
		break;
	}
	return rc;
}

BOOL CALLBACK finddlgproc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	CModView * pMV;

	if (message == WM_INITDIALOG)
		SetWindowLong(hWnd,DWL_USER,lParam);

	pMV = (CModView *)GetWindowLong(hWnd,DWL_USER);

	if (pMV)
		return pMV->FindDlg(hWnd,message,wParam,lParam);

	return 0;
}

int CModView::OnSaveAs(HWND hWnd, int iMode)
{
	OPENFILENAME ofn;
	char szStr1[260];
	char szStr2[128];
	char szStr3[128];
	HCURSOR hCursorOld;
//	HWND hWndDlg;

	strcpy(szStr1,"");

	memset(szStr3,0,sizeof(szStr3));
	strcpy(szStr3,pStrings[TEXTFSTR]);
	strcpy(szStr3 + strlen(szStr3) + 1,"*.txt");
	memset(szStr2,0,sizeof(szStr2));
	strcpy(szStr2,pStrings[ALLFSTR]);
	strcpy(szStr2 + strlen(szStr2) + 1,"*.*");

	memset(&ofn,0,sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFilter = szStr2;

	if (iMode == IDM_SAVETEXT) {
		ofn.lpstrCustomFilter = szStr3;
		ofn.nMaxCustFilter = sizeof(szStr3);
	} else {
		ofn.lpstrCustomFilter = NULL;
		ofn.nMaxCustFilter = sizeof(szStr2);
	}
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = szStr1;
	ofn.nMaxFile = sizeof(szStr1);
	ofn.Flags = OFN_EXPLORER;
	if (GetSaveFileName(&ofn)) {
		hCursorOld = SetCursor(LoadCursor(0,IDC_WAIT));
		if (iMode == IDM_SAVETEXT) {
			pStrTmp = szStr1;
			m_iSaveMode = 0;
			DialogBoxParam(pApp->hInstance,
							MAKEINTRESOURCE(IDD_PROGRESS),
							hWnd,
							progressdlgproc, (LPARAM)this);
		} else
			SaveRawBytes(hWnd,szStr1);
		SetCursor(hCursorOld);
	}
	return 1;
}

// set new mode (new tab selected in tabbed dialog)

void CModView::SetNewMode(HWND hWnd, int iNewMode)
{

	m_tab[m_iModeIndex].uTopIndex = ListView_GetTopIndex(m_hWndListView);
	m_tab[m_iModeIndex].iSelIndex = ListView_GetNextItem(m_hWndListView,-1,LVNI_SELECTED);

	UpdateListView(iNewMode);

	return;
}

// Create and show context menu
// iMode == 0: called when windows key is pressed
// iMode == 1: called by clicking right mouse button

int CModView::DisplayContextMenu(HWND hWnd, int iMode)
{
	HMENU hMenu, hPopupMenu = 0;
	DWORD dwGrayItems[16];
	DWORD dwDefaultItem;
	DWORD * pdwItems = (DWORD *)&dwGrayItems;
	DWORD * pdwItems2 = (DWORD *)&dwGrayItems;
	POINT pt;
	RECT rect;
	int i;

	i = ListView_GetNextItem(m_hWndListView,-1,LVNI_SELECTED);
	if (iMode)
		GetCursorPos(&pt);
	else {
		GetWindowRect(m_hWndListView,&rect);
		pt.x = rect.left;
		pt.y = rect.top;
		ListView_GetItemRect(m_hWndListView,max(0,i),&rect,LVIR_BOUNDS);
		pt.x = pt.x + rect.left + (rect.bottom - rect.top);
		pt.y = pt.y + rect.bottom;
	}

	hMenu = LoadMenu(pApp->hInstance,MAKEINTRESOURCE(IDR_MENU2));
	hPopupMenu = GetSubMenu(hMenu,0);

	if (i == -1) {
		*pdwItems++ = IDM_EDIT;
		*pdwItems++ = IDM_VIEW;
		*pdwItems++ = IDM_COPY;
		*pdwItems++ = IDM_SETADDRESS;
	}

	dwDefaultItem = IDM_VIEW;

	switch (m_iLVMode) {
	case MODVIEW_RELOC:
	case MODVIEW_HEADER:
	case MODVIEW_IMPORT:
	case MODVIEW_IAT:
		*pdwItems++ = IDM_VIEW;
	case MODVIEW_SECTION:
	case MODVIEW_EXPORT:
		*pdwItems++ = IDM_EDIT;
		*pdwItems++ = IDM_SETADDRESS;
	case MODVIEW_CODE:
	case MODVIEW_CODE16:
		*pdwItems++ = IDM_DWORDS;
		*pdwItems++ = IDM_BYTES;
		break;
	default:	// simple hex format (MODVIEW_IMAGE, MODVIEW_DATA, MODVIEW_RES)
		dwDefaultItem = IDM_EDIT;
		*pdwItems++ = IDM_VIEW;
//		*pdwItems++ = IDM_SETADDRESS;
		if (m_dwFlags & MVF_DWORDS)
			CheckMenuItem(hPopupMenu,IDM_DWORDS, MF_BYCOMMAND | MF_CHECKED);
		else
			CheckMenuItem(hPopupMenu,IDM_BYTES, MF_BYCOMMAND | MF_CHECKED);
		*pdwItems++ = IDM_SAVETEXT;
		break;
	}
	if (!m_pszFindString)
		*pdwItems++ = IDM_FINDNEXT;

	while (pdwItems2 < pdwItems) {
		if (*pdwItems2 == dwDefaultItem)
			dwDefaultItem = -1;
		EnableMenuItem(hPopupMenu, *pdwItems2++, MF_BYCOMMAND | MF_GRAYED);
	}
	SetMenuDefaultItem(hPopupMenu, dwDefaultItem, FALSE);


//	CheckMenuItem(hPopupMenu,iCmds[m_iModeIndex], MF_BYCOMMAND | MF_CHECKED);

	if (hPopupMenu)	{
		SendMessage(m_hWndStatusBar,SB_SIMPLE,1,0);	// simple mode einschalten		
		SendMessage(m_hWndStatusBar,SB_SETTEXT,255,(LPARAM)0);		
		TrackPopupMenu(hPopupMenu, 
						TPM_LEFTALIGN | TPM_LEFTBUTTON,
						pt.x,
						pt.y,
						0,
						hWnd,
						NULL);
		SendMessage(m_hWndStatusBar,SB_SIMPLE,0,0);	// simple mode ausschalten	
	}
	
	DestroyMenu(hMenu);
	return 1;
}

BOOL CModView::EditLineDlg(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	BOOL rc = FALSE;
	int i,j,k;
	unsigned long ul;
	LVITEM lvi;
	char szStr[128];
	char szCap[128];
	UINT ux[16];
	HWND hWndEdit;
	HANDLE hProc;
	DWORD dwOldProtect;
	unsigned char * pStr;
	DWORD * pDW;
	char ch;

	switch (message) {
	case WM_INITDIALOG:
		i = ListView_GetNextItem(m_hWndListView,-1,LVNI_SELECTED);
		if (i == -1) {
			EndDialog(hWnd,0);
			break;
		}
		lvi.iItem = i;
		lvi.iSubItem = 0;
		lvi.mask = LVIF_TEXT;
		lvi.pszText = szStr;
		lvi.cchTextMax = sizeof(szStr);
		if (!ListView_GetItem(m_hWndListView,&lvi)) {
			EndDialog(hWnd,0);
			break;
		}
		sscanf(szStr,"%X",&m_dwEditAddr);
		lvi.iSubItem = 1;
		if (ListView_GetItem(m_hWndListView,&lvi)) {
			j = strlen(szStr);
			if ((m_dwFlags & MVF_BYTES) || (m_iLVMode == MODVIEW_CODE) || (m_iLVMode == MODVIEW_CODE16)) {
				k = 0;
				if ((m_iLVMode != MODVIEW_CODE) && (m_iLVMode != MODVIEW_CODE16) && (j > 3*8)) {
					szStr[3*8] = ' ';
					k = 2;
				}
				m_uNumItems = (j - k + 1) / 3;
			} else {
				m_uNumItems = (j + 2) / 10;
			}

			if (hWndEdit = GetDlgItem(hWnd,IDC_EDIT1)) {
				SetWindowText(hWndEdit,szStr);
			}
			GetWindowText(hWnd,szStr,sizeof(szStr));
			sprintf(szCap,szStr,m_dwEditAddr);
			SetWindowText(hWnd,szCap);
		} 
		rc = 1;
		break; 
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			if (hWndEdit = GetDlgItem(hWnd,IDC_EDIT1)) {
				BOOL fError = FALSE;
				int	dwSize;
				GetWindowText(hWndEdit,szStr,sizeof(szStr));
				pStr = m_pMem + (m_dwEditAddr - m_modBase);
				if ((m_dwFlags & MVF_BYTES) || (m_iLVMode == MODVIEW_CODE) || (m_iLVMode == MODVIEW_CODE16)) {
					dwSize = sizeof(BYTE);
					if (sscanf(	szStr,
							"%X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %c",
							&ux[0],&ux[1],&ux[2],&ux[3],&ux[4],&ux[5],&ux[6],&ux[7],
							&ux[8],&ux[9],&ux[10],&ux[11],&ux[12],&ux[13],&ux[14],&ux[15],&ch) != (int)m_uNumItems) {
						fError = TRUE;
					}
					for (i = 0; i < (int)m_uNumItems; i++)
						if (ux[i] > 255) {
							fError = TRUE;
						}
					if (fError) {
						MessageBeep(MB_OK);
						break;
					}
					for (j = 0; j < (int)m_uNumItems; j++) 
						*pStr++ = BYTE(ux[j]);
				} else {
					dwSize = sizeof(DWORD);
					if (sscanf(	szStr,
							"%X %X %X %X %c",
							&ux[0],&ux[1],&ux[2],&ux[3],&ch) != (int)m_uNumItems) {
						fError = TRUE;
					}
					if (fError) {
						MessageBeep(MB_OK);
						break;
					}
					pDW = (DWORD *)pStr;
					for (j = 0; j < (int)m_uNumItems; j++) 
						*pDW++ = ux[j];
				}
//				_asm int 3;
				if (hProc = OpenProcess(PROCESS_VM_WRITE | PROCESS_VM_OPERATION,FALSE, m_pid)) {
					VirtualProtectEx(hProc,
							(void *)(m_dwEditAddr),
							m_uNumItems * dwSize,
							PAGE_READWRITE,&dwOldProtect);
					WriteProcessMemory(hProc,(void *)(m_dwEditAddr),
						m_pMem + (m_dwEditAddr - m_modBase),
						m_uNumItems * dwSize,&ul);
					VirtualProtectEx(hProc,
							(void *)(m_dwEditAddr),
							m_uNumItems * dwSize,
							dwOldProtect,0);
					CloseHandle(hProc);
					if (ul != (m_uNumItems * dwSize)) {
						sprintf(szStr,pStrings[ERRSTR16], m_pid);
						MessageBox(hWnd,szStr,0,MB_OK);
						break;
					}
				} else {
					MessageBeep(MB_OK);
					sprintf(szStr,pStrings[ERRSTR12], m_pid);
					MessageBox(hWnd,szStr,0,MB_OK);
					break;
				}
			}
		case IDCANCEL:
			PostMessage(hWnd,WM_CLOSE,0,0);
		}
		break;
	case WM_CLOSE:
		EndDialog(hWnd,0);
		break;
	}
	return rc;
}

BOOL CALLBACK editlinedlgproc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	CModView * pMV;

	if (message == WM_INITDIALOG)
		SetWindowLong(hWnd,DWL_USER,lParam);

	pMV = (CModView *)GetWindowLong(hWnd,DWL_USER);

	if (pMV)
		return pMV->EditLineDlg(hWnd,message,wParam,lParam);

	return 0;
}

#define UDM_SETPOS32            (WM_USER+113)
#define UDM_GETPOS32            (WM_USER+114)

// adjust address for disassembler views

BOOL CModView::AdjustAddressDlg(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int i;
	BOOL rc = FALSE;
	DWORD dwLowerBound;
	DWORD dwUpperBound;
	LVITEM lvi;
	char szStr[128];

	switch (message) {
	case WM_INITDIALOG:
		i = ListView_GetNextItem(m_hWndListView,-1,LVNI_SELECTED);
		lvi.iItem = i;
		lvi.iSubItem = 0;
		lvi.mask = LVIF_TEXT;
		lvi.pszText = szStr;
		lvi.cchTextMax = sizeof(szStr);
		if (!ListView_GetItem(m_hWndListView,&lvi)) {
			EndDialog(hWnd,0);
			break;
		}
		sscanf(szStr,"%X",&m_dwEditAddr);
//		SetDlgItemText(hWnd, IDC_EDIT1, szStr);
		SendDlgItemMessage(hWnd, IDC_SPIN1, UDM_SETBUDDY, (WPARAM)GetDlgItem(hWnd, IDC_EDIT1), 0);
		SendDlgItemMessage(hWnd, IDC_SPIN1, UDM_SETBASE, (WPARAM)16, 0);
		dwLowerBound = m_dwEditAddr;
		dwUpperBound = m_dwEditAddr;
		if (i) {
			lvi.iItem = i - 1;
			if (ListView_GetItem(m_hWndListView,&lvi)) {
				sscanf(szStr,"%X",&dwLowerBound);
				dwLowerBound++;
			}
		}
		lvi.iItem = i + 1;
		if (ListView_GetItem(m_hWndListView,&lvi)) {
			sscanf(szStr,"%X",&dwUpperBound);
			dwUpperBound--;
		}

		SendDlgItemMessage(hWnd, IDC_SPIN1, UDM_SETRANGE32, dwLowerBound, dwUpperBound);
		SendDlgItemMessage(hWnd, IDC_SPIN1, UDM_SETPOS32, 0, m_dwEditAddr);
		rc = TRUE;
		break;
	case WM_CLOSE:
		EndDialog(hWnd,0);
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_SPIN1:
			break;
		case IDOK:
			m_dwEditAddr = (DWORD)SendDlgItemMessage(hWnd, IDC_SPIN1, UDM_GETPOS32, 0, 0);
			i = ListView_GetNextItem(m_hWndListView,-1,LVNI_SELECTED);
			if (m_pDAD->SetAddressOfLine(i, (unsigned char *)(m_dwEditAddr + m_pMem - m_modBase)))
				InvalidateRect(m_hWndListView, 0, TRUE);
		case IDCANCEL:
			PostMessage(hWnd,WM_CLOSE,0,0);
		}
		break;
	}
	return rc;
}

BOOL CALLBACK adjustaddrdlgproc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	CModView * pMV;

	if (message == WM_INITDIALOG)
		SetWindowLong(hWnd,DWL_USER,lParam);

	pMV = (CModView *)GetWindowLong(hWnd,DWL_USER);

	if (pMV)
		return pMV->AdjustAddressDlg(hWnd,message,wParam,lParam);

	return 0;
}

// adjust address for simple memory dump views

BOOL CModView::AdjustAddress2Dlg(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int i;
	unsigned int j;
	BOOL rc = FALSE;
	//LVITEM lvi;
	char szStr[128];

	switch (message) {
	case WM_INITDIALOG:
		sprintf(szStr, "%X", m_modBase + m_dwOffset + m_tab[m_iModeIndex].uTopIndex * 16);
		SetDlgItemText(hWnd, IDC_EDIT1, szStr);
		rc = TRUE;
		break;
	case WM_CLOSE:
		EndDialog(hWnd,0);
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			if (GetDlgItemText(hWnd, IDC_EDIT1, szStr, sizeof szStr)) {
				if (sscanf(szStr,"%X",&j)) {
					i = ListView_GetItemCount(m_hWndListView);
					if ((j >= m_modBase + m_dwOffset) && (j < m_modBase + m_dwOffset + i * 16)) {
						j = j - (m_modBase + m_dwOffset);
						m_tab[m_iModeIndex].uTopIndex = j / 16;
						m_tab[m_iModeIndex].iSelIndex = -1;
						EndDialog(hWnd,1);
						break;
					}
				}
			}
			MessageBeep(MB_OK);
			break;
		case IDCANCEL:
			PostMessage(hWnd,WM_CLOSE,0,0);
		}
		break;
	}
	return rc;
}

BOOL CALLBACK adjustaddr2dlgproc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	CModView * pMV;

	if (message == WM_INITDIALOG)
		SetWindowLong(hWnd,DWL_USER,lParam);

	pMV = (CModView *)GetWindowLong(hWnd,DWL_USER);

	if (pMV)
		return pMV->AdjustAddress2Dlg(hWnd,message,wParam,lParam);

	return 0;
}

// search in memory
// m_iLastFindPos is offset in memory block

int CModView::SearchText()
{
	unsigned char *  pFStr;
	unsigned char *  pSrc;
	unsigned char *  pszFindString = m_pszFindString;
	unsigned char *  pszTmp1;
	unsigned char *  pszTmp2;
	int i;
	int fFound = FALSE;
	BOOL fIgnCase = TRUE;
	char c;
	int iBufPos;
	HCURSOR hCursorOld;
	int dwLength = m_iFindStringLength;
	unsigned char nib1;

	hCursorOld = SetCursor(LoadCursor(0,IDC_WAIT));

// convert hex nibble input to bytes
	if (m_bHexFormat) {
		fIgnCase = FALSE;
		dwLength = m_iFindStringLength / 2;
		pszFindString = (unsigned char *)malloc(dwLength);
		if (!pszFindString)
			return 0;
		for (pszTmp1 = m_pszFindString, pszTmp2 = pszFindString;*pszTmp1;pszTmp1++,pszTmp2++) {
			if (*pszTmp1 > '9')
				nib1 = *pszTmp1 - 'A' + 10;
			else
				nib1 = *pszTmp1 - '0';
			nib1 = nib1 << 4;
			pszTmp1++;
			if (*pszTmp1 > '9')
				nib1 = nib1 + *pszTmp1 - 'A' + 10;
			else
				nib1 = nib1 + *pszTmp1 - '0';
			*pszTmp2 = (unsigned char)nib1;
		}
	}

	if (m_iLastFindPos != -1)
		i = ListView_GetNextItem(m_hWndListView,-1,LVNI_SELECTED) + 1;
	else
		i = 0;		// start from beginning

	pSrc = m_pMem + m_dwOffset + i * 16;

	i = (int)(m_pMem + m_modSize - pSrc);
	for (pFStr = pszFindString;i >= dwLength;i--,pSrc++) {
		if (fIgnCase) {
			c = *pFStr | 0x20;
			if (c == (*pSrc | 0x20))
				if (!_memicmp(pszFindString, pSrc, dwLength)) {
					fFound = TRUE;
					break;
				}
		} else {
			if (*pFStr == *pSrc)
				if (!memcmp(pszFindString, pSrc, dwLength)) {
					fFound = TRUE;
					break;
				}
		}
	}
	if (fFound) {
		m_iLastFindPos = pSrc - (m_pMem + m_dwOffset) + 1;
		iBufPos = m_iLastFindPos & 0xFFFFFFF0;
		while ((i = ListView_GetNextItem(m_hWndListView,-1,LVIS_SELECTED)) != -1)
			ListView_SetItemState(m_hWndListView,i,0,LVIS_FOCUSED | LVIS_SELECTED);
		ListView_EnsureVisible(m_hWndListView,iBufPos/16,0);
		ListView_SetItemState(m_hWndListView,iBufPos/16,LVIS_FOCUSED | LVIS_SELECTED,LVIS_FOCUSED | LVIS_SELECTED);
	}

	if (m_bHexFormat)
		free(pszFindString);

	SetCursor(hCursorOld);

	return fFound;
}

// dont search in memory but in listview column
// m_iLastFindPos is listview item

int CModView::SearchTextLV(int iColumn)
{
	int iMax;
	int i, j;
	LV_ITEM lvi;
	BOOL fIgnCase = TRUE;
	int fFound = FALSE;
	unsigned char *pFStr;
	unsigned char *pSrc;
	unsigned char c;
	HCURSOR hCursorOld;
	unsigned char szText[260];

	hCursorOld = SetCursor(LoadCursor(0,IDC_WAIT));
	iMax = ListView_GetItemCount(m_hWndListView);

	if (m_iLastFindPos != -1)
		i = ListView_GetNextItem(m_hWndListView,-1,LVNI_SELECTED) + 1;
	else
		i = 0;		// start from beginning

	for (; i < iMax; i++) {
		lvi.mask = LVIF_TEXT;
		lvi.iItem = i;
		lvi.iSubItem = iColumn;
		lvi.pszText = (char *)szText;
		lvi.cchTextMax = sizeof(szText);
		ListView_GetItem(m_hWndListView, &lvi);
		j = lstrlen((char *)szText);
		pSrc = szText;
		for (pFStr = m_pszFindString;j >= m_iFindStringLength;j--,pSrc++) {
			if (fIgnCase) {
				c = *pFStr | 0x20;
				if (c == (*pSrc | 0x20))
					if (!_memicmp(m_pszFindString,pSrc,m_iFindStringLength)) {
						fFound = TRUE;
						break;
					}
			} else {
				if (*pFStr == *pSrc)
					if (!memcmp(m_pszFindString,pSrc,m_iFindStringLength)) {
						fFound = TRUE;
						break;
					}
			}
		}
		if (fFound)
			break;
	}
	if (fFound) {
		m_iLastFindPos = i + 1;
		while ((j = ListView_GetNextItem(m_hWndListView,-1,LVIS_SELECTED)) != -1)
			ListView_SetItemState(m_hWndListView,j,0,LVIS_FOCUSED | LVIS_SELECTED);
		ListView_EnsureVisible(m_hWndListView,i,0);
		ListView_SetItemState(m_hWndListView,i,LVIS_FOCUSED | LVIS_SELECTED,LVIS_FOCUSED | LVIS_SELECTED);
	}
	SetCursor(hCursorOld);
	return fFound;
}

// WM_COMMAND, IDM_VIEW

void CModView::OnView()
{
	LVITEM lvi;
	int i,j;
	IMAGE_EXPORT_DIRECTORY * pED;
	UINT * pEAT;
	DWORD dw1;
	DWORD dwBaseOfCode;
	DWORD dwSizeOfCode;
	CModView * pMV;
	RECT rect;
	char szStr[80];
	char szStr1[80];
	char szStr2[80];
	char szStr3[80];

	i = ListView_GetNextItem(m_hWndListView,-1,LVNI_SELECTED);
	if (i == -1)
		return;

	switch(m_iLVMode) {
	case MODVIEW_SECTION:
		lvi.iItem = i;
		lvi.iSubItem = 2;
		lvi.mask = LVIF_TEXT;
		lvi.pszText = szStr;
		if (ListView_GetItem(m_hWndListView,&lvi)) {
			if (sscanf(szStr,"%X",&dwBaseOfCode)) {
				dwBaseOfCode = dwBaseOfCode + m_modBase;
//				lvi.iSubItem = 3;			// changed 7.12.2003
				lvi.iSubItem = 1;
				if (ListView_GetItem(m_hWndListView,&lvi)) {
					if (sscanf(szStr,"%X",&dwSizeOfCode)) {
						GetWindowRect(m_hWnd,&rect);
						sprintf(szStr, "%X: %X, %X", m_pid, dwBaseOfCode, dwSizeOfCode);
						pMV = new CModView(szStr,0,rect.left + 50, rect.top + 50, m_hWnd, MODVIEW_IMAGE, m_pid, dwBaseOfCode, dwSizeOfCode, FALSE);
						if (pMV)
							if (!pMV->ShowWindow(SW_SHOWNORMAL))
								delete pMV;
					}
				}
			}
		}
		break;
	case MODVIEW_EXPORT:
		pED = (IMAGE_EXPORT_DIRECTORY *)(m_pMem + m_pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
//		pEAT = (UINT *)(m_pMem + pED->AddressOfNames);
		pEAT = (UINT *)(m_pMem + pED->AddressOfFunctions);
		dw1 = *(pEAT + i);
		if (dw1) {
#if 0
			sprintf(szStr, "addr=%X, cmp1=%X, cmp2=%X", dw1,
					m_pPE->OptionalHeader.BaseOfCode,
					m_pPE->OptionalHeader.BaseOfCode + m_pPE->OptionalHeader.SizeOfCode);
			MessageBox(0,szStr,0,MB_OK);
#endif
			if ((dw1 < m_pPE->OptionalHeader.BaseOfCode) || (dw1 > m_pPE->OptionalHeader.BaseOfCode + m_pPE->OptionalHeader.SizeOfCode)) {
				for (j=0;j<NUMMODES;j++)
					if (iModes[j] == MODVIEW_IMAGE) {
						m_tab[j].uTopIndex = -1; break;
					}
				m_tab[j].uTopIndex = dw1 / 16;
				PostMessage(m_hWnd, WM_COMMAND, IDM_BLOCK, 0);
				break;
			}
			if (!m_pDAD)
				m_pDAD = new CDisAsmDoc();
			if (m_pDAD) {
				for (j=0;j<NUMMODES;j++)
					if (iModes[j] == MODVIEW_CODE) {
						m_tab[j].uTopIndex = -1; break;
					}
				m_pDAD->SetCodeOffset(*(pEAT + i) - m_pPE->OptionalHeader.BaseOfCode);
				PostMessage(m_hWnd, WM_COMMAND, IDM_DISASM, 0);
			}
		}
		break;
	case MODVIEW_CODE:
	case MODVIEW_CODE16:
		lvi.iItem = i;
		lvi.iSubItem = 2;
		lvi.mask = LVIF_TEXT;
		lvi.pszText = szStr;
		lvi.cchTextMax = sizeof(szStr);
		if (ListView_GetItem(m_hWndListView,&lvi)) {
			szStr1[0] = 0;
			szStr2[0] = 0;
			sscanf(szStr,"%s %s %s",szStr1,szStr2,szStr3);
			if (!strcmp(szStr1,"call") || !strcmp(szStr1,"jmp")) {
				sscanf(szStr2,"%X",&dw1);
				if (m_pPE) {
					dwBaseOfCode = m_pPE->OptionalHeader.BaseOfCode;
					dwSizeOfCode = m_pPE->OptionalHeader.SizeOfCode;
				} else {
					dwBaseOfCode = 0;
					dwSizeOfCode = m_modSize;
				}
				if ((dw1 >= m_modBase + dwBaseOfCode)
				&&	(dw1 < m_modBase + dwBaseOfCode + dwSizeOfCode)) {
					if (m_iLVMode == MODVIEW_CODE)
						m_pDAD->SetCodeOffset(dw1 - dwBaseOfCode - m_modBase);
					else
						m_pDAD16->SetCodeOffset(dw1 - dwBaseOfCode - m_modBase);
					m_tab[m_iModeIndex].uTopIndex = -1;
					UpdateListView(-1);
					break;
				}
			}
		}
		MessageBeep(-1);
		break;
	default:
		break;
	}
	return;
}

// WM_COMMAND

LRESULT CModView::OnCommand(HWND hWnd,WPARAM wParam,LPARAM lparam)
{
	LRESULT rc = 0;
	LV_KEYDOWN lvk;
	TC_ITEM tci;
	int i,j,k;

	switch (LOWORD(wParam)) {
	case IDCANCEL:
		PostMessage(hWnd,WM_CLOSE,0,0);
		break;
	case IDOK:
		if (GetFocus() == m_hWndListView) {
			lvk.hdr.code = LVN_KEYDOWN;
			lvk.wVKey = VK_RETURN;
			OnNotify(hWnd,wParam,(LPNMHDR)&lvk);
		}
		break;
	case IDM_SAVEMOD:
	case IDM_SAVETEXT:
		OnSaveAs(hWnd,wParam);
		break;
	case IDM_DWORDS:
		m_dwFlags = m_dwFlags & MVF_FMASK;
		m_dwFlags = m_dwFlags | MVF_DWORDS;
		dwFlagsOrg = m_dwFlags;
		InvalidateRect(m_hWndListView,0,0);
		break;
	case IDM_BYTES:
		m_dwFlags = m_dwFlags & MVF_FMASK;
		m_dwFlags = m_dwFlags | MVF_BYTES;
		dwFlagsOrg = m_dwFlags;
		InvalidateRect(m_hWndListView,0,0);
		break;
	case IDM_SETADDRESS:
		if (ListView_GetSelectedCount(m_hWndListView))
			if ((m_iLVMode == MODVIEW_CODE) || (m_iLVMode == MODVIEW_CODE16))
				DialogBoxParam(pApp->hInstance,
					MAKEINTRESOURCE(IDD_ADJUSTADDRDLG),
					hWnd,
					adjustaddrdlgproc,
					(long)this);
			else
				if (DialogBoxParam(pApp->hInstance,
					MAKEINTRESOURCE(IDD_ADJUSTADDR2DLG),
					hWnd,
					adjustaddr2dlgproc,
					(long)this))
					UpdateListView(-1);
		break;
	case IDM_COPY:
		m_iSaveMode = 1;
		pStrTmp = 0;
		DialogBoxParam(pApp->hInstance,
							MAKEINTRESOURCE(IDD_PROGRESS),
							hWnd,
							progressdlgproc, (LPARAM)this);
		break;
	case IDM_EDIT:
		if (ListView_GetSelectedCount(m_hWndListView))
			DialogBoxParam(pApp->hInstance,
				MAKEINTRESOURCE(IDD_EDITLINEDLG),
				hWnd,
				editlinedlgproc,
				(long)this);
		break;
	case IDM_SELECTALL:
		ListView_SetItemState(m_hWndListView, -1, LVIS_SELECTED, LVIS_SELECTED);
		break;
	case IDM_VIEW:
		OnView();
		break;
	case IDM_BLOCK:
	case IDM_PEHEADER:
	case IDM_DISASM:
	case IDM_DATA:
	case IDM_EXPORT:
	case IDM_IMPORT:
	case IDM_RES:
	case IDM_IAT:
	case IDM_CODE16:
	case IDM_SECTION:
// translate IDM_XXX to MODE_XXX
		for (i = 0;i < NUMMODES;i++)
			if (iCmds[i] == LOWORD(wParam)) {
				k = TabCtrl_GetItemCount(m_hWndTabCtrl);
				tci.mask = TCIF_PARAM;
				for (j = 0;j < k;j++) {
					TabCtrl_GetItem(m_hWndTabCtrl, j, &tci);
					if (tci.lParam == iModes[i]) {
						TabCtrl_SetCurSel(m_hWndTabCtrl, j);
						SetNewMode(hWnd,iModes[i]);
					}
				}
				break;
			}
		break;
	case IDM_FIND:
		if (DialogBoxParam(pApp->hInstance,
						MAKEINTRESOURCE(IDD_FIND),
						hWnd,
						finddlgproc, (LPARAM)this)) {
			m_iLastFindPos = 0;	//reset so we beginn to search from cursor pos
			PostMessage(hWnd, WM_COMMAND, IDM_FINDNEXT, 0);
		}
		break;
	case IDM_FINDNEXT:
		if (!m_pszFindString) {
			MessageBeep(MB_OK);
			break;
		}
		switch (m_iLVMode) {
		case MODVIEW_IMAGE:
		case MODVIEW_DATA:
		case MODVIEW_RES:
			if (!SearchText()) {
				MessageBox(hWnd,pStrings[FINDATEND],pStrings[HINTSTR],MB_OK);
				m_iLastFindPos = -1;
			}
			break;
		case MODVIEW_CODE:
		case MODVIEW_CODE16:
			if (!SearchTextLV(2)) {
				MessageBox(hWnd,pStrings[FINDATEND],pStrings[HINTSTR],MB_OK);
				m_iLastFindPos = -1;
			}
			break;
		case MODVIEW_EXPORT:
			if (!SearchTextLV(3)) {
				MessageBox(hWnd,pStrings[FINDATEND],pStrings[HINTSTR],MB_OK);
				m_iLastFindPos = -1;
			}
			break;
		case MODVIEW_IAT:
			if (!SearchTextLV(3)) {
				MessageBox(hWnd,pStrings[FINDATEND],pStrings[HINTSTR],MB_OK);
				m_iLastFindPos = -1;
			}
			break;
		}
		break;
	default:
		MessageBox(hWnd,"Unknown WM_COMMAND received",0,MB_OK);
		break;
	}
	return rc;
}

// WM_NOTIFY

LRESULT CModView::OnNotify(HWND hWnd,WPARAM wParam,LPNMHDR pNMHdr)
{
	LRESULT rc = 0;
	LV_DISPINFO * pDI;
	TC_ITEM tci;
	int i;
	NMLISTVIEW * pNMLV;
	char szStr[32];

	switch (pNMHdr->code) {
	case LVN_GETDISPINFO:
		pDI = (LV_DISPINFO *)pNMHdr;
		if (pDI->item.mask & LVIF_TEXT)
			FillItem(hWnd,pDI->item.iItem,pDI->item.iSubItem,pDI->item.pszText,pDI->item.cchTextMax);
		break;
	case LVN_KEYDOWN:
		if (((LV_KEYDOWN *)pNMHdr)->wVKey == VK_APPS)
			DisplayContextMenu(hWnd,0);
		if (((LV_KEYDOWN *)pNMHdr)->wVKey == VK_RETURN) {
			if ((m_iLVMode == MODVIEW_IMAGE) || (m_iLVMode == MODVIEW_DATA) || (m_iLVMode == MODVIEW_RES))
				PostMessage(hWnd,WM_COMMAND,IDM_EDIT,0);
			else
				PostMessage(hWnd,WM_COMMAND,IDM_VIEW,0);
		}
		break;
	case TCN_SELCHANGE:
		i = TabCtrl_GetCurSel(m_hWndTabCtrl);
		tci.mask = TCIF_PARAM;
		TabCtrl_GetItem(m_hWndTabCtrl,i,&tci);
		SetNewMode(hWnd,(int)tci.lParam);
		break;
#if 0
	case TCN_KEYDOWN:
		pTCKD = (TC_KEYDOWN *)pNMHdr;
		switch (pTCKD->wVKey) {
		case 'M': i = IDM_BLOCK;break;
		case 'P': i = IDM_PEHEADER;break;
		case 'C': i = IDM_DISASM;break;
		case 'D': i = IDM_DATA;break;
		case 'E': i = IDM_EXPORT;break;
		case 'I': i = IDM_IMPORT;break;
		case 'R': i = IDM_RES;break;
		case 'A': i = IDM_IAT;break;
		default: i = 0;
		}
		if (i)
			PostMessage(hWnd,WM_COMMAND,i,0);
		break;
#endif
	case LVN_ITEMCHANGED:
		pNMLV = (NMLISTVIEW *)pNMHdr;
		if (pNMLV->uChanged & LVIF_STATE)
			if (pNMLV->uNewState & (LVIS_SELECTED | LVIS_FOCUSED)) {
				sprintf(szStr,
					pStrings[LINESTR],
					pNMLV->iItem + 1,
					ListView_GetItemCount(m_hWndListView));
				SendMessage(m_hWndStatusBar, SB_SETTEXT, 2, (LPARAM)szStr); 
			}
		switch(m_iLVMode) {
		case MODVIEW_EXPORT:
			break;
		}
		break;
	case NM_DBLCLK:
		if (wParam != IDC_LISTVIEW)	// nur dblclk auf Listview bearbeiten!
			break;
		if (ListView_GetSelectedCount(m_hWndListView) == 0)
			break;
		switch(m_iLVMode) {
		case MODVIEW_IMAGE:
		case MODVIEW_DATA:
		case MODVIEW_RES:
			PostMessage(hWnd,WM_COMMAND,IDM_EDIT,0);
			break;
		case MODVIEW_SECTION:
		case MODVIEW_CODE:
		case MODVIEW_CODE16:
		case MODVIEW_EXPORT:
			PostMessage(hWnd,WM_COMMAND,IDM_VIEW,0);
			break;
		}
		break;
	case NM_RCLICK:			// rechte Maustaste -> Kontextmenues	
		if (wParam != IDC_LISTVIEW)	// nur rclick auf Listview bearbeiten!
			break;
		DisplayContextMenu(hWnd,1);
		break;
	default:
		break;
	}
	return rc;
}

LRESULT CModView::OnMessage(HWND hWnd,UINT message,WPARAM wparam,LPARAM lparam)
{
	LRESULT rc = 0;

	switch (message) {
	case WM_NOTIFY:
		OnNotify(hWnd,wparam,(LPNMHDR)lparam);
		break;
	case WM_COMMAND:
		OnCommand(hWnd,wparam,lparam);
		break;
	case WM_SETFOCUS:
		SetFocus(m_hWndListView);
		break;
	case WM_ACTIVATE:
		if (LOWORD(wparam) != WA_INACTIVE) {
			pApp->hWndDlg = hWnd;
			pApp->hAccel = m_hAccel;
		}
		break;
	case WM_DESTROY:
		if (m_hWnd) {
			SetWindowLong(hWnd,0,0);
			delete this;
		}
		break;
	default:
		rc = CTabWindowView::OnMessage(hWnd,message,wparam,lparam);
	}
	return rc;
}

// wndproc of "module" window

LRESULT CALLBACK modwndproc(HWND hWnd,UINT message,WPARAM wparam,LPARAM lparam)
{
	LRESULT rc = 0;
	CModView * pMV = (CModView *)GetWindowLong(hWnd,0);

	switch (message) {
	case WM_CREATE:
		SetWindowLong(hWnd,0,(long)((LPCREATESTRUCT)lparam)->lpCreateParams);
		break;
	default:
		if (pMV)
			rc = pMV->OnMessage(hWnd,message,wparam,lparam);
		else
			rc = DefWindowProc(hWnd,message,wparam,lparam);
	}
	return rc;
}

