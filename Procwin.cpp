
#define STRICT

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <shlobj.h>

#include <stdio.h>
#include <io.h>
#include <share.h>
#include <fcntl.h>
#include <psapi.h>
#include <sys\stat.h>
#include <process.h>

#include <queue32.h>

#include "tabwnd.h"
//#include <pehdr.h>
#include "ModView.h"
#include "procwin.h"
#include "CApplication.h"
#include "rsrc.h"
#include "strings.h"

#define NO_OWNERDATA 1		/* ??? */
#define OPTIONDLG 0			/* currently no options */
#define DEBUGPROCESS 0		/* to activate debugging of processes */
#define ALLOCDEBUG 0		/* to debug memory leaks */

typedef struct tLVCInit {
int mask;
int fmt;
int cx;
char * pszText;
} LVCInit;

typedef struct tLVCMain {
int iMode;
const LVCInit * pLVCInit;
int iCount;
} LVCMain;

extern Application * pApp;

typedef BOOL (WINAPI* LPFNENUMPROCESSES)(DWORD *lpidProcess, DWORD cb, LPDWORD lpcbNeeded);
typedef BOOL (WINAPI* LPFNENUMPROCESSMODULES)(HANDLE hProcess, HMODULE *lphModule, DWORD cb, LPDWORD lpcbNeeded);
typedef DWORD (WINAPI* LPFNGETMODULEBASENAME)(HANDLE hProcess, HMODULE hModule, LPSTR lpBaseName, DWORD nSize);
typedef DWORD (WINAPI* LPFNGETMODULEFILENAMEEX)(HANDLE hProcess, HMODULE hModule, LPSTR lpFilename, DWORD nSize);
typedef BOOL (WINAPI* LPFNGETMODULEINFORMATION)(HANDLE hProcess, HMODULE hModule, LPMODULEINFO lpModInfo, DWORD cb);
typedef HANDLE (WINAPI * LPFNCREATETOOLHELP32SNAPSHOT)(DWORD, DWORD);
typedef BOOL (WINAPI * LPFNPROCESS32FIRST)(HANDLE, LPPROCESSENTRY32);
typedef BOOL (WINAPI * LPFNPROCESS32NEXT)(HANDLE, LPPROCESSENTRY32);
typedef BOOL (WINAPI * LPFNTHREAD32FIRST)(HANDLE, LPTHREADENTRY32);
typedef BOOL (WINAPI * LPFNTHREAD32NEXT)(HANDLE, LPTHREADENTRY32);
typedef BOOL (WINAPI * LPFNMODULE32FIRST)(HANDLE, LPMODULEENTRY32);
typedef BOOL (WINAPI * LPFNMODULE32NEXT)(HANDLE, LPMODULEENTRY32);

HINSTANCE g_hLibPSAPI = NULL;
LPFNENUMPROCESSES g_pfnEnumProcesses;
LPFNENUMPROCESSMODULES g_pfnEnumProcessModules;
LPFNGETMODULEBASENAME g_pfnGetModuleBaseName;
LPFNGETMODULEFILENAMEEX g_pfnGetModuleFileNameEx;
LPFNGETMODULEINFORMATION g_pfnGetModuleInformation;

HINSTANCE g_hLibKernel32 = NULL;
LPFNCREATETOOLHELP32SNAPSHOT g_pfnCreateToolhelp32Snapshot;
LPFNPROCESS32FIRST g_pfnProcess32First;
LPFNPROCESS32NEXT g_pfnProcess32Next;
LPFNTHREAD32FIRST g_pfnThread32First;
LPFNTHREAD32NEXT g_pfnThread32Next;
LPFNMODULE32FIRST g_pfnModule32First;
LPFNMODULE32NEXT g_pfnModule32Next;

static char * pszModes[] = {pStrings[MODULSTR],pStrings[MEMSTSTR],pStrings[HEAPSTR], pStrings[WINSTR]};
const static int iCmds[] = {IDM_MODULE, IDM_PROCINF, IDM_HEAP};
const static int iModes[] = {LVMODE_MODULE, LVMODE_PROCINF, LVMODE_HEAP};

const static TBBUTTON button[] = {
	{0,IDM_REFRESH,TBSTATE_ENABLED,TBSTYLE_BUTTON,0,0},
	{1,IDM_SAVEAS,TBSTATE_ENABLED,TBSTYLE_BUTTON,0,0},
	{0,-1,0,TBSTYLE_SEP,0,0},
//	{2,IDM_MODULE,TBSTATE_ENABLED,TBSTYLE_BUTTON,0,0},
//	{3,IDM_PROCINF,TBSTATE_ENABLED,TBSTYLE_BUTTON,0,0},
//	{4,IDM_HEAP,TBSTATE_ENABLED,TBSTYLE_BUTTON,0,0},
//	{0,-1,0,TBSTYLE_SEP,0,0},
	{5,IDM_KILL,0,TBSTYLE_BUTTON,0,0}
};

const static LVCInit tLVCModule[] = {
	{ LVCF_TEXT | LVCF_WIDTH | LVCF_FMT, LVCFMT_LEFT,  16 * 8, pStrings[NAMESTR]},
	{ LVCF_TEXT | LVCF_WIDTH | LVCF_FMT, LVCFMT_RIGHT, 12 * 8, pStrings[ADDRSTR]},
	{ LVCF_TEXT | LVCF_WIDTH | LVCF_FMT, LVCFMT_RIGHT, 12 * 8, pStrings[LENSTR]},
	{ LVCF_TEXT | LVCF_WIDTH | LVCF_FMT, LVCFMT_RIGHT, 12 * 4, pStrings[MODULECNT]},
	{ LVCF_TEXT | LVCF_WIDTH | LVCF_FMT, LVCFMT_LEFT,  64 * 8, pStrings[PATHSTR]}
};

const static LVCInit tLVCHeap[] = {
	{ LVCF_TEXT | LVCF_WIDTH | LVCF_FMT, LVCFMT_LEFT,  12 * 8, "Heap ID"},
	{ LVCF_TEXT | LVCF_WIDTH | LVCF_FMT, LVCFMT_RIGHT, 12 * 8, pStrings[HDLSTR]},
	{ LVCF_TEXT | LVCF_WIDTH | LVCF_FMT, LVCFMT_RIGHT, 12 * 8, pStrings[ADDRSTR]},
	{ LVCF_TEXT | LVCF_WIDTH | LVCF_FMT, LVCFMT_RIGHT, 12 * 8, pStrings[BLKSIZSTR]},
	{ LVCF_TEXT | LVCF_WIDTH | LVCF_FMT, LVCFMT_LEFT,  12 * 8, pStrings[FLAGSTR]}
};
const static LVCInit tLVCProc[] = {
	{ LVCF_TEXT | LVCF_WIDTH | LVCF_FMT, LVCFMT_RIGHT, 12 * 8, "Region"},
	{ LVCF_TEXT | LVCF_WIDTH | LVCF_FMT, LVCFMT_RIGHT, 12 * 8, pStrings[SIZHEXSTR]},
	{ LVCF_TEXT | LVCF_WIDTH | LVCF_FMT, LVCFMT_LEFT,   8 * 8, pStrings[STATSTR]},
	{ LVCF_TEXT | LVCF_WIDTH | LVCF_FMT, LVCFMT_RIGHT, 12 * 8, "AllocBase"},
	{ LVCF_TEXT | LVCF_WIDTH | LVCF_FMT, LVCFMT_LEFT,  12 * 8, "AllocProtect"},
	{ LVCF_TEXT | LVCF_WIDTH | LVCF_FMT, LVCFMT_LEFT,  12 * 8, "Protect"},
	{ LVCF_TEXT | LVCF_WIDTH | LVCF_FMT, LVCFMT_LEFT,   8 * 8, "Type"},
	{ LVCF_TEXT | LVCF_WIDTH | LVCF_FMT, LVCFMT_LEFT,   8 * 8, "Guard"},
	{ LVCF_TEXT | LVCF_WIDTH | LVCF_FMT, LVCFMT_LEFT,   8 * 8, "NoCache"}
};
const static LVCInit tLVCWin[] = {
	{ LVCF_TEXT | LVCF_WIDTH | LVCF_FMT, LVCFMT_LEFT,  12 * 8, "HWND"},
	{ LVCF_TEXT | LVCF_WIDTH | LVCF_FMT, LVCFMT_LEFT,  12 * 8, pStrings[STYLESTR]},
	{ LVCF_TEXT | LVCF_WIDTH | LVCF_FMT, LVCFMT_LEFT,  20 * 8, pStrings[CLASSSTR]},
	{ LVCF_TEXT | LVCF_WIDTH | LVCF_FMT, LVCFMT_LEFT,  48 * 8, pStrings[TEXTSTR]}
};

const static LVCMain lvcm[] = {
	{LVMODE_MODULE,tLVCModule,sizeof(tLVCModule)/sizeof(LVCInit)},
	{LVMODE_HEAP,tLVCHeap,sizeof(tLVCHeap)/sizeof(LVCInit)},
	{LVMODE_PROCINF,tLVCProc,sizeof(tLVCProc)/sizeof(LVCInit)},
	{LVMODE_WINDOWS,tLVCWin,sizeof(tLVCWin)/sizeof(LVCInit)}
};


#if ALLOCDEBUG
typedef HANDLE (WINAPI* LPHEAPALLOC)(HANDLE, DWORD, UINT);
typedef BOOL   (WINAPI* LPHEAPFREE)(HANDLE, DWORD, void *);
static LPHEAPALLOC oldHeapAlloc;
static LPHEAPFREE  oldHeapFree;
static dwHeapCount = 0;
extern "C" HANDLE _heapid;
extern "C" void WINAPI InstallHeapDebug(void *, LPHEAPALLOC *, void *, LPHEAPFREE *);

static void * WINAPI myHeapAlloc(HANDLE heapid, DWORD dwFlags, UINT dwSize)
{
	void * pMem;
	char szStr[128];

	pMem = oldHeapAlloc(heapid,dwFlags,dwSize);
	if (pMem)
		dwHeapCount++;
	sprintf(szStr,"%u. HeapAlloc(%u)=%X\r\n", dwHeapCount, dwSize, pMem);
	OutputDebugString(szStr);
	return pMem;
}
static BOOL WINAPI myHeapFree(HANDLE heapid, DWORD dwFlags, void * pMem)
{
	BOOL rc;
	char szStr[128];

	if (rc = oldHeapFree(heapid, dwFlags, pMem))
		dwHeapCount--;
	sprintf(szStr,"%u. HeapFree(%X)=%u\r\n",dwHeapCount, pMem, rc);
	OutputDebugString(szStr);
	return rc;
}

#endif

#ifdef _DEBUG
void DbgMsg( const char *format, ... )
{
	va_list args;
	char buff[256];

	va_start( args, format );
	vsprintf( buff, format, args );
	OutputDebugString( buff );
	va_end( args );
}
#endif

BOOL SetPSAPI(HWND hWnd, BOOL bUsePSAPI)
{
	DWORD uState = MF_UNCHECKED; 
	HMENU hMenu = GetMenu(hWnd);

	if (bUsePSAPI) {
		if (!g_hLibPSAPI) {
			g_hLibPSAPI = LoadLibrary("PSAPI.DLL");
			if (g_hLibPSAPI > (HINSTANCE)32) {
				g_pfnEnumProcesses = (LPFNENUMPROCESSES)GetProcAddress(g_hLibPSAPI, "EnumProcesses");
				g_pfnEnumProcessModules = (LPFNENUMPROCESSMODULES)GetProcAddress(g_hLibPSAPI, "EnumProcessModules");
				g_pfnGetModuleBaseName = (LPFNGETMODULEBASENAME)GetProcAddress(g_hLibPSAPI, "GetModuleBaseNameA");
				g_pfnGetModuleFileNameEx = (LPFNGETMODULEFILENAMEEX)GetProcAddress(g_hLibPSAPI, "GetModuleFileNameExA");
				g_pfnGetModuleInformation = (LPFNGETMODULEINFORMATION)GetProcAddress(g_hLibPSAPI, "GetModuleInformation");
				if ((!g_pfnEnumProcesses) ||
					(!g_pfnEnumProcessModules) ||
					(!g_pfnGetModuleBaseName) ||
					(!g_pfnGetModuleFileNameEx) ||
					(!g_pfnGetModuleInformation)) {
					MessageBox(hWnd, "Wrong version of PSAPI.DLL", 0, MB_OK);
				} else
					uState = MF_CHECKED;
			} else {
				g_hLibPSAPI = NULL;
				MessageBox(hWnd, "Failed to load PSAPI.DLL", 0, MB_OK);
			}
		} else
			uState = MF_CHECKED;
	} else {
		if (!g_hLibKernel32) {
			g_hLibKernel32 = LoadLibrary("KERNEL32.DLL");
			if (g_hLibKernel32 > (HINSTANCE)32) {
				g_pfnCreateToolhelp32Snapshot = (LPFNCREATETOOLHELP32SNAPSHOT)GetProcAddress(g_hLibKernel32, "CreateToolhelp32Snapshot");
				g_pfnProcess32First = (LPFNPROCESS32FIRST)GetProcAddress(g_hLibKernel32, "Process32First");
				g_pfnProcess32Next = (LPFNPROCESS32NEXT)GetProcAddress(g_hLibKernel32, "Process32Next");
				g_pfnThread32First = (LPFNTHREAD32FIRST)GetProcAddress(g_hLibKernel32, "Thread32First");
				g_pfnThread32Next = (LPFNTHREAD32NEXT)GetProcAddress(g_hLibKernel32, "Thread32Next");
				g_pfnModule32First = (LPFNMODULE32FIRST)GetProcAddress(g_hLibKernel32, "Module32First");
				g_pfnModule32Next = (LPFNMODULE32NEXT)GetProcAddress(g_hLibKernel32, "Module32Next");
			}
			if ((!g_pfnCreateToolhelp32Snapshot) ||
				(!g_pfnProcess32First) ||
				(!g_pfnProcess32Next) ||
				(!g_pfnThread32First) ||
				(!g_pfnThread32Next) ||
				(!g_pfnModule32First) ||
				(!g_pfnModule32Next)) {
				return SetPSAPI(hWnd, TRUE);
			}
		}
	}

	CheckMenuItem(hMenu, IDM_PSAPI, uState);

	return(uState == MF_CHECKED);
}

BOOL CProcWinView::SetFreeBlocksOption(BOOL bValue)
{
	HMENU hMenu = GetMenu(m_hWndMain);

	m_bInclFreeBlocks = bValue;
	if (bValue)
		CheckMenuItem(hMenu, IDM_INCLFREE, MF_CHECKED);
	else
		CheckMenuItem(hMenu, IDM_INCLFREE, MF_UNCHECKED);

	return TRUE;
}

void CProcWinView::EnableSubMenuItems(HMENU hMenu,int iSubMenu, int iMask)
{
#if 0
	HMENU hPopup;
	int i;
	int iID;

	hPopup = GetSubMenu(hMenu, iSubMenu);
	for (i = GetMenuItemCount(hPopup);i;i--) {
		iID = GetMenuItemID(hPopup,i - 1);
		EnableMenuItem(hPopup,i - 1,MF_BYPOSITION | iMask);
		if (iMask & MF_GRAYED)
			SendMessage(m_hWndToolbar,TB_ENABLEBUTTON,iID,MAKELONG(0,0));
		else
			SendMessage(m_hWndToolbar,TB_ENABLEBUTTON,iID,MAKELONG(1,0));
	}
#endif
	return;
}

// Constructor of the main class
// all child windows are created here

CProcWinView::CProcWinView(HINSTANCE hInstance, char * pszAppName, UINT dwStyle, HWND hWndParent,HMENU hMenu) : CExplorerView()
{
	int i;
	OSVERSIONINFO osvi;
	TC_ITEM tci;
	WNDCLASS wc;

	pApp->pProcWinView = this;

	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&osvi);

#if ALLOCDEBUG
	_heapid = HeapCreate(0, 0x8000, 0x100000);
	InstallHeapDebug(myHeapAlloc, &oldHeapAlloc, myHeapFree, &oldHeapFree);
#endif

	pTHHOld = 0;
//	hAltMenu = LoadMenu(hInstance,MAKEINTRESOURCE(IDR_MENU2));
	hCursor = LoadCursor(hInstance,MAKEINTRESOURCE(IDC_CURSOR2));
	hAccel = LoadAccelerators(hInstance,MAKEINTRESOURCE(IDR_ACCELERATOR1));

	for (i = 0;i < LVMODE_MAX;i++) {
		iSortColumn[i] = -1;
	}
	iLVMode = -1;
	iProcLVMode = -1;
	fSelChanged = FALSE;
	m_bUsePSAPI = FALSE;
	m_pDropTarget = NULL;


	if (iCount == 1) {
		memset(&wc,0,sizeof(wc));
		wc.style = 0;
		wc.cbWndExtra = 0;
		wc.lpfnWndProc = wndproc;
		wc.hInstance = hInstance;
		wc.hIcon = LoadIcon(hInstance,MAKEINTRESOURCE(IDI_ICON1));
//		wc.hCursor = LoadCursor(NULL,IDC_SIZEWE);
		wc.hCursor = LoadCursor(NULL,IDC_ARROW);
//		wc.hCursor = LoadCursor(hInstance,MAKEINTRESOURCE(IDC_CURSOR2));
//		wc.hbrBackground = (HBRUSH)(COLOR_ACTIVEBORDER + 1);
		wc.hbrBackground = NULL;
		wc.lpszMenuName = MAKEINTRESOURCE(IDR_MENU1);
		wc.lpszClassName = CLASSNAME;

		if (!RegisterClass(&wc))
			return;
	}

	m_hWndMain = CreateWindowEx(0,
					CLASSNAME,
					pszAppName,
					dwStyle,
					CW_USEDEFAULT,
					CW_USEDEFAULT,
					CW_USEDEFAULT,
					CW_USEDEFAULT,
					hWndParent,
					hMenu,
					hInstance,
					0);
	
	CreateChilds(m_hWndMain,
				WS_VISIBLE | WS_TABSTOP | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS,
				WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS,
				LVS_EX_FULLROWSELECT | LVS_EX_HEADERDRAGDROP,
				WS_VISIBLE | SBARS_SIZEGRIP | CCS_BOTTOM
				);

	m_hWndToolbar = CreateToolbarEx ( m_hWndMain,
					WS_CHILD | WS_VISIBLE | TBSTYLE_TOOLTIPS | TBSTYLE_FLAT,
					IDC_TOOLBAR,
					6,	// number of button images
					hInstance,
					IDB_BITMAP1,
					button,
					sizeof(button)/sizeof(TBBUTTON),// number of buttons in toolbar
					16,16,	// x und y fuer	buttons
					16,15,	// x und y fuer bitmaps
					sizeof(TBBUTTON));

	m_hWndTabCtrl = CreateWindowEx(0,WC_TABCONTROL,
					NULL,
					WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP,
					0,0,0,0,
					(HWND)m_hWndMain,
					(HMENU)IDC_TABCTRL,
					hInstance,
					(LPVOID)NULL);

	tci.mask = TCIF_TEXT | TCIF_PARAM;
	tci.lParam = iCmds[0];
	tci.iImage = 0;
	tci.pszText = pszModes[0];
	TabCtrl_InsertItem(m_hWndTabCtrl,0,&tci);

#if 0
	SendMessage(m_hWndTabCtrl,WM_SETFONT,
				SendMessage(m_hWndStatusBar,WM_GETFONT,0,0),0);
#else
	SetWindowFont(m_hWndTabCtrl, (WPARAM)GetStockObject(ANSI_VAR_FONT), FALSE);
#endif
	
	if (m_hWndTreeView && m_hWndListView && m_hWndStatusBar) {
		SetListViewCols(LVMODE_MODULE);
		SetFreeBlocksOption(TRUE);
		if (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT)
			m_bUsePSAPI = SetPSAPI(m_hWndMain, TRUE);
		else
			SetPSAPI(m_hWndMain, FALSE);
		PostMessage(m_hWndMain,WM_COMMAND,IDM_REFRESH,0);
	} else
		m_hWndMain = 0;
}


void CProcWinView::SetTabs(void)
{
	TC_ITEM tci;
	int i;

	TabCtrl_DeleteItem(m_hWndTabCtrl,2);
	TabCtrl_DeleteItem(m_hWndTabCtrl,1);

	tci.mask = TCIF_TEXT;
	if (iLVMode == LVMODE_WINDOWS)
		tci.pszText = pszModes[3];
	else
		tci.pszText = pszModes[0];

	TabCtrl_SetItem(m_hWndTabCtrl,0,&tci);

	tci.mask = TCIF_TEXT | TCIF_PARAM;
	if (iLVMode == LVMODE_WINDOWS) {
		TabCtrl_SetCurSel(m_hWndTabCtrl,0);
	} else {
		for (i = 1; i < 3; i++) {
			tci.lParam = iCmds[i];
			tci.iImage = i;
			tci.pszText = pszModes[i];
			TabCtrl_InsertItem(m_hWndTabCtrl,i,&tci);
		}
	}
}

//

CProcWinView::~CProcWinView()
{
//	if (hAltMenu)
//		DestroyMenu(hAltMenu);
	if (pTHHOld)
		free(pTHHOld);
}

void GetProcessName(HANDLE hProcess, LPSTR lpszName, DWORD dwSizeMax)
{
	HMODULE hModule;
	DWORD cntModule;

	*lpszName = 0;
	if (g_pfnEnumProcessModules(hProcess, (HMODULE *)&hModule, sizeof(hModule), &cntModule))
		g_pfnGetModuleFileNameEx(hProcess, hModule, lpszName, dwSizeMax);

	return;
}

// fill treeview with list of processes and threads

int CProcWinView::UpdateTreeView()
{
	TV_INSERTSTRUCT tvs;
	HTREEITEM hTree;
	HTREEITEM hTreeFirst = 0;
	HANDLE handle;
	HANDLE handle2;
	DWORD * pProcesses;
	DWORD dwNeeded;
	DWORD cntProcess;
	DWORD dwIndex;
	PROCESSENTRY32  pe;
	THREADENTRY32  te;
	char szStr[260];
	char szStr2[260];
	int i;

	SendMessage(m_hWndTreeView,WM_SETREDRAW,0,0);
//	SendMessage(m_hWndListView,WM_SETREDRAW,0,0);

//	TreeView_SelectItem(m_hWndTreeView,0);
	TreeView_DeleteAllItems(m_hWndTreeView);
//	ListView_DeleteAllItems(m_hWndListView);

	tvs.hInsertAfter = TVI_LAST;
	tvs.item.mask = TVIF_TEXT | TVIF_PARAM;		// nur Text und Textmax in TV_ITEM ist gültig
	tvs.item.cchTextMax = 0;

	if (0) {
		dwNeeded = 10000 * sizeof(DWORD);
		pProcesses = (DWORD *)malloc(dwNeeded);
		g_pfnEnumProcesses(pProcesses, dwNeeded, &cntProcess);
		cntProcess = cntProcess/sizeof(HANDLE);
		i = cntProcess;
		dwIndex = 0;
	} else {
		handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
		pe.dwSize = sizeof(PROCESSENTRY32);
		i = Process32First(handle,&pe);
	}

	while (i) {
//		sprintf(szStr,"%8X %8d %8X %5d %s",pe.th32ProcessID,pe.pcPriClassBase,pe.dwFlags,pe.cntThreads,pe.szExeFile);
		if (0) {
			if (dwIndex >= cntProcess)
				break;
			pe.th32ProcessID = *(pProcesses+dwIndex);
			handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe.th32ProcessID);
			dwIndex++;
			if (handle == NULL)
				continue;
			GetProcessName(handle, (LPSTR)&pe.szExeFile, sizeof(pe.szExeFile));
			pe.pcPriClassBase = GetPriorityClass(handle);
			CloseHandle(handle);
		}
		_splitpath(pe.szExeFile,NULL,NULL,szStr2,NULL);
		sprintf(szStr,"%s, %X, Pri=%u",szStr2,pe.th32ProcessID, pe.pcPriClassBase);
		tvs.item.pszText = szStr;
		tvs.item.lParam = pe.th32ProcessID;
		tvs.hParent = 0;
		if (hTree = TreeView_InsertItem(m_hWndTreeView, &tvs)) {
			if (!hTreeFirst)
				hTreeFirst = hTree;
			tvs.hParent = hTree;
			if (handle2 = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,pe.th32ProcessID)) {
				te.dwSize = sizeof(THREADENTRY32);
				i = Thread32First(handle2,&te);
				while(i) {
					if (te.th32OwnerProcessID == pe.th32ProcessID) {
						sprintf(szStr,"Thread %X, Pri=%u%+d",te.th32ThreadID,te.tpBasePri,te.tpDeltaPri);
						tvs.item.pszText = szStr;
						tvs.item.lParam = te.th32ThreadID;
						TreeView_InsertItem(m_hWndTreeView, &tvs);
					}
					i = Thread32Next(handle2,&te);
				}
				CloseHandle(handle2);
			}
		}
		if (0)
			i = 1;
		else
			i = Process32Next(handle,&pe);
	}

	if (0)
		free(pProcesses);
	else
		CloseHandle(handle);
	

//	SendMessage(m_hWndListView,WM_SETREDRAW,1,0);
	SendMessage(m_hWndTreeView,WM_SETREDRAW,1,0);

	SendMessage(m_hWndStatusBar, SB_SETTEXT, 0, (LPARAM)pStrings[NBUSYSTR]); 

//	SendMessage(m_hWndToolbar,TB_ENABLEBUTTON,IDM_KILL,MAKELONG(0,0));

// SetFocus() sollte auch gleich erstes item selektieren

	TreeView_Select(m_hWndTreeView,TreeView_GetRoot(m_hWndTreeView),TVGN_CARET);
	SetFocus(m_hWndTreeView);

	return 1;
}


BOOL CProcWinView::EnumThreadWindows(HWND hWnd)
{
	char szStr[80];
	LV_ITEM lv;

	lv.mask = LVIF_TEXT | LVIF_PARAM;

	sprintf(szStr,"%X",hWnd);
	lv.pszText = szStr;
	lv.iItem = ListView_GetItemCount(m_hWndListView);
	lv.iSubItem = 0;
	lv.lParam = (LPARAM)hWnd;
	if ((lv.iItem = ListView_InsertItem(m_hWndListView, &lv)) != -1) {
		sprintf(szStr,"%X",GetWindowLong(hWnd,GWL_STYLE));
		lv.mask = LVIF_TEXT;
		lv.iSubItem++;
		ListView_SetItem(m_hWndListView, &lv);

		GetClassName(hWnd,szStr,sizeof(szStr));
		lv.iSubItem++;
		ListView_SetItem(m_hWndListView, &lv);

		GetWindowText(hWnd,szStr,sizeof(szStr));
		lv.iSubItem++;
		ListView_SetItem(m_hWndListView, &lv);
	}
	
	return TRUE;
}


BOOL CALLBACK enumthreadwindowsproc(HWND hWnd, LPARAM lParam)
{
	CProcWinView * pProcWinView = (CProcWinView *)lParam;
	return pProcWinView->EnumThreadWindows(hWnd);
}

// fill listview with hwnds of a thread

int CProcWinView::UpdateListViewWindows(DWORD tid)
{
	char szStr[80];

	CheckSubMenuItem(2, IDM_WINDOWS);
	::EnumThreadWindows(tid,&enumthreadwindowsproc,(LPARAM)this);

	sprintf(szStr,pStrings[NUMOBJSTR],ListView_GetItemCount(m_hWndListView));
	SendMessage(m_hWndStatusBar, SB_SETTEXT, 1, (LPARAM)szStr);

	return 1;
}

int CProcWinView::TranslateProtectFlag(UINT flags,char * szStr,int iMax)
{
	char * pStr = "";

	switch (flags) {
	case PAGE_NOACCESS:
		pStr = "noaccess";
		break;
	case PAGE_READONLY:
		pStr = "readonly";
		break;
	case PAGE_READWRITE:
		pStr = "readwrite";
		break;
	case PAGE_WRITECOPY:
		pStr = "writecopy";
		break;
	case PAGE_EXECUTE:
		pStr = "execute";
		break;
	case PAGE_EXECUTE_READ:
		pStr = "execute_read";
		break;
	case PAGE_EXECUTE_READWRITE:
		pStr = "execute_readwrite";
		break;
	case PAGE_EXECUTE_WRITECOPY:
		pStr = "execute_writecopy";
		break;
	}
	strncpy(szStr,pStr,iMax);
	return 1;
}


int CProcWinView::CheckSubMenuItem(int iSubMenu, int iItem)
{
	int i, j, k, iID;
	HMENU hMenu = GetMenu(m_hWndMain);

	hMenu = GetSubMenu(hMenu,iSubMenu);
	j = GetMenuItemCount(hMenu);

	for (i = 0, k = 0;i < j;i++) {
		iID = GetMenuItemID(hMenu,i);
		if (iID == iItem) {
			break;
		} else
			if (iID == 0)
				k = i + 1;
	}
	if (i == j)
		return 0;

	for (i = k;i < j;i++) {
		iID = GetMenuItemID(hMenu,i);
		if (iID == 0)
			break;
		else 
			if (iID == iItem)
				CheckMenuItem(hMenu,iID,MF_BYCOMMAND | MF_CHECKED);
			else
				CheckMenuItem(hMenu,iID,MF_BYCOMMAND | MF_UNCHECKED);
	}
	return 1;
}


int CProcWinView::UpdateListViewProcInf(DWORD pid)
{
	LV_ITEM lv;
	HANDLE handle;
	DWORD dwBytes;
	char * pStr;
	int j = 0;
	int i;
	BOOL bError = FALSE;
	BYTE * dwAddress = (BYTE *)0x0;
	MEMORY_BASIC_INFORMATION * pMem;
	char szStr[260];
	UINT dwReserved = 0;
	UINT dwCommitted = 0;
	UINT dwAccess = 0;
	UINT dwRW = 0;
	UINT dwInfoSize = sizeof(MEMORY_BASIC_INFORMATION);
	UINT dwObjects = 0;
	HANDLE hQueue;

	iProcLVMode = iLVMode;
	
	CheckSubMenuItem(2,IDM_PROCINF);

	pMem = (MEMORY_BASIC_INFORMATION *)malloc(dwInfoSize);
	if (!pMem)
		return 0;

	hQueue = CreateQueue(sizeof(MEMORY_BASIC_INFORMATION),100);

	i = 0;
//	if (handle = OpenProcess(PROCESS_ALL_ACCESS, 0, pid)) {
	if (handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 0, pid)) {
		while (dwBytes = VirtualQueryEx(handle,dwAddress,pMem,dwInfoSize)) {
			if ((pMem->State == MEM_FREE) && (!m_bInclFreeBlocks))
				;
			else {
				WriteQueue(hQueue,pMem);
				i++;
			}
			dwAddress = dwAddress + pMem->RegionSize;
		}
		CloseHandle(handle);
	} else {
		sprintf(szStr, "OpenProcess(%X) failed [%X]", pid, GetLastError());
		bError = TRUE;
	}
	
	ListView_SetItemCount(m_hWndListView,i);

	for (j = 0; j < i;j++) {
		if (!ReadQueue(hQueue,pMem))
			break;
		lv.mask = LVIF_TEXT | LVIF_PARAM;
		sprintf(szStr,"%08X",pMem->BaseAddress,
			(DWORD)pMem->BaseAddress + pMem->RegionSize - 1);
		lv.pszText = szStr;
		lv.lParam = (LPARAM)pMem->BaseAddress;
		lv.iItem = j;
		lv.iSubItem = 0;
		if ((lv.iItem = ListView_InsertItem(m_hWndListView, &lv)) != -1) {
			lv.mask = LVIF_TEXT;
			lv.iSubItem++;
			sprintf(szStr,"%8X",pMem->RegionSize);
			ListView_SetItem(m_hWndListView, &lv);

			switch (pMem->State) {
			case MEM_COMMIT:
				pStr = "commit";
				if (pMem->BaseAddress < (PVOID)0x80000000) {
					dwCommitted = dwCommitted + pMem->RegionSize;
					if (!(pMem->Protect & PAGE_NOACCESS))
						dwAccess = dwAccess + pMem->RegionSize;
					if (pMem->Protect & PAGE_READWRITE)
						dwRW = dwRW + pMem->RegionSize;
				}
				break;
			case MEM_RESERVE:
				pStr = "reserve";
				if (pMem->BaseAddress < (PVOID)0x80000000)
					dwReserved = dwReserved + pMem->RegionSize;
				break;
			case MEM_FREE:
				pStr = "free";
				break;
			default:
				pStr = "???";
			}
			strcpy(szStr,pStr);
			lv.pszText = szStr;
			lv.iSubItem++;
			ListView_SetItem(m_hWndListView, &lv);

			if (pMem->State == MEM_FREE)
				strcpy(szStr,"");
			else
				sprintf(szStr,"%8X",pMem->AllocationBase);

			lv.pszText = szStr;
			lv.iSubItem++;
			ListView_SetItem(m_hWndListView, &lv);
			if (pMem->State == MEM_FREE)
				strcpy(szStr,"");
			else
				TranslateProtectFlag(pMem->AllocationProtect & 0xFF,szStr,sizeof(szStr));
			lv.iSubItem++;
			ListView_SetItem(m_hWndListView, &lv);
			if (pMem->State == MEM_FREE)
				strcpy(szStr,"");
			else
				TranslateProtectFlag(pMem->Protect & 0xFF,szStr,sizeof(szStr));
			lv.iSubItem++;
			ListView_SetItem(m_hWndListView, &lv);

			if (pMem->State == MEM_FREE)
				pStr = "";
			else
				switch (pMem->Type) {
				case MEM_IMAGE:
					pStr = "image";
					break;
				case MEM_MAPPED:
					pStr = "mapped";
					break;
				case MEM_PRIVATE:
					pStr = "private";
					break;
				default:
					pStr = "???";
				}
			strcpy(szStr,pStr);
			lv.pszText = szStr;
			lv.iSubItem++;
			ListView_SetItem(m_hWndListView, &lv);

			lv.iSubItem++;
			if ((pMem->State != MEM_FREE)
			&& (pMem->Protect & PAGE_GUARD)) {
				strcpy(szStr,"guard");
				lv.pszText = szStr;
				ListView_SetItem(m_hWndListView, &lv);
			}
			lv.iSubItem++;
			if ((pMem->State != MEM_FREE)
			&& (pMem->Protect & PAGE_NOCACHE)) {
				strcpy(szStr,"nocache");
				lv.pszText = szStr;
				ListView_SetItem(m_hWndListView, &lv);
			}

		} else {
			MessageBeep(MB_OK);
		}
		if (pMem->BaseAddress < (PVOID)0x80000000)
			dwObjects++;
	}
	free(pMem);
	DestroyQueue(hQueue);

	if (bError == FALSE)
		sprintf(szStr,
			pStrings[VMEMSTSTR],
			dwObjects,
			(dwReserved+dwCommitted) / 1024,
			dwCommitted / 1024,
			dwAccess / 1024,
			dwRW / 1024);
	
	SendMessage(m_hWndStatusBar, SB_SETTEXT, 1, (LPARAM)szStr);
	
	return 1;
}


void CProcWinView::SetListViewHeapLine2(LV_DISPINFO * pDI)
{
	char szStr[80];
	char * pStr;
	THHeap * pTHH;

	if (!pTHHOld)
		return;

//	pTHH = pTHHOld + pDI->item.iItem;
	pTHH = pTHHOld + pDI->item.lParam;
	
	switch (pDI->item.iSubItem) {
	case 0:
		sprintf(szStr,"%X",pTHH->dwHeapID);
		break;
	case 1:
		sprintf(szStr,"%X",pTHH->hHandle);
		break;
	case 2:
		sprintf(szStr,"%X",pTHH->dwAddress);
		break;
	case 3:
		sprintf(szStr,"%u",pTHH->dwBlockSize);
		break;
	case 4:
		switch (pTHH->dwFlags) {
		case LF32_FIXED:
			pStr = "fixed";
			break;
		case LF32_FREE:
			pStr = "free";
			break;
		case LF32_MOVEABLE:
			pStr = "moveable";
			break;
		default:
			pStr = "???";
			break;
		}
		strcpy(szStr,pStr);
		break;
	}
	strncpy(pDI->item.pszText,szStr,pDI->item.cchTextMax);

	return;
}



int CProcWinView::UpdateListViewHeap(DWORD pid)
{
	HEAPLIST32	hl;
	HEAPENTRY32	he;
	UINT	uBSize,uUsed,uObjects;
	HANDLE	handle;
	//HANDLE * pHeaps;
	char szStr[80];
	int	i,j;
	LV_ITEM lv;
	THHeap thh;
	HANDLE hQueue;

	iProcLVMode = iLVMode;
	CheckSubMenuItem(2,IDM_HEAP);

#if 0
	if (m_bUsePSAPI) {
		i = GetProcessHeaps(1, &handle);
		pHeaps = (DWORD *)malloc(i * sizeof(HANDLE));
		GetProcessHeaps(i, pHeaps);
	} else
#endif
		if (!(handle = CreateToolhelp32Snapshot(TH32CS_SNAPHEAPLIST,pid)))
			return 0;

	if (!(hQueue = CreateQueue(sizeof(THHeap),1000))) {
		CloseHandle(handle);
		return 0;
	}

	hl.dwSize = sizeof(HEAPLIST32);
	i = Heap32ListFirst(handle,&hl);
	
	uBSize = 0;
	uUsed  = 0;
	uObjects = 0;
	j = 0;

	while(i) {
		he.dwSize = sizeof(HEAPENTRY32);
		i = Heap32First(&he,hl.th32ProcessID,hl.th32HeapID);
		while (i) {
//			dbg_printf( "i=%u, addr=%X, size=%X, flags=%X\r\n", i, he.dwAddress, he.dwBlockSize, he.dwFlags);
			thh.dwHeapID = hl.th32HeapID;
			thh.dwAddress = he.dwAddress;
			thh.hHandle = he.hHandle;
			thh.dwBlockSize = he.dwBlockSize;
			thh.dwFlags = he.dwFlags;
			WriteQueue(hQueue,&thh);
			uObjects++;
			uBSize = uBSize + he.dwBlockSize;
			if (he.dwFlags != LF32_FREE)
				uUsed = uUsed + he.dwBlockSize;
			i = Heap32Next(&he);
//			if (he.dwAddress < thh.dwAddress)
//				DebugBreak();
			j++;
		}
		i = Heap32ListNext(handle,&hl);
	}

	if (pTHHOld = (THHeap *)malloc(sizeof(THHeap) * uObjects))
		ReadQueueAllItems(hQueue,(unsigned char *)pTHHOld);

	DestroyQueue(hQueue);

	ListView_SetItemCount(m_hWndListView,uObjects);

#if NO_OWNERDATA
	for (i = 0; i < (int)uObjects;i++) {
		lv.mask = LVIF_TEXT | LVIF_PARAM;
		lv.pszText = LPSTR_TEXTCALLBACK;
		lv.iItem = i;
		lv.lParam = i;
		lv.iSubItem = 0;
		ListView_InsertItem(m_hWndListView, &lv);
	}
#endif
	CloseHandle(handle);

	sprintf(szStr,pStrings[HEAPSTSTR], uObjects, uUsed / 1024);
	SendMessage(m_hWndStatusBar, SB_SETTEXT, 1, (LPARAM)szStr);
	
	return 1;
}

// CDropTarget class definition

class CDropTarget : public IDropTarget
{
	DWORD m_dwRefCount;
	HWND m_hWndMain;
public:
	CDropTarget(HWND hWndMain);
	~CDropTarget();
	STDMETHOD(QueryInterface)(REFIID iid, void** ppvObject);
	STDMETHOD_(ULONG, AddRef)(void);		
	STDMETHOD_(DWORD, Release)(void);
	STDMETHOD(DragEnter)(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect);
	STDMETHOD(DragOver)(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect);
	STDMETHOD(DragLeave)(void);
	STDMETHOD(Drop)(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect);

};
CDropTarget::CDropTarget(HWND hWndMain)
{
	m_hWndMain = hWndMain;
	m_dwRefCount = 1;
}
CDropTarget::~CDropTarget()
{
}

// the CDropTarget object knows 2 interfaces: IUnknown and IDropTarget

STDMETHODIMP CDropTarget::QueryInterface(REFIID riid, void** ppvObject)
{
	*ppvObject = NULL;
	if (IsEqualIID(riid, IID_IUnknown)) {
	  *ppvObject = this;
	} else if (IsEqualIID(riid, IID_IDropTarget)) {
	  *ppvObject = static_cast<IDropTarget*>(this);
	}   
	if (*ppvObject) {
		static_cast<LPUNKNOWN>(*ppvObject)->AddRef();
		return S_OK;
	}
	return E_NOINTERFACE;
}
STDMETHODIMP_(ULONG) CDropTarget::AddRef(void)
{
	m_dwRefCount++;
	return m_dwRefCount;
}
STDMETHODIMP_(ULONG) CDropTarget::Release(void)
{
	m_dwRefCount--;
	if (!m_dwRefCount) {
		delete this;
		return 0;
	}
	return m_dwRefCount;
}
STDMETHODIMP CDropTarget::DragEnter(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
	FORMATETC fe;

	fe.cfFormat = CF_HDROP;
	fe.ptd = NULL;
	fe.dwAspect = DVASPECT_CONTENT;
	fe.lindex = -1;
	fe.tymed = TYMED_HGLOBAL;
	if (S_OK != pDataObj->QueryGetData(&fe)) {
		*pdwEffect = DROPEFFECT_NONE;
	} else {
		*pdwEffect = DROPEFFECT_COPY;
	}
   return S_OK;
}
STDMETHODIMP CDropTarget::DragOver(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
	*pdwEffect = DROPEFFECT_COPY;
	return S_OK;
}
STDMETHODIMP CDropTarget::DragLeave(void)
{
	return S_OK;
}
STDMETHODIMP CDropTarget::Drop(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
	FORMATETC fe;
	STGMEDIUM stgmedium;
	char szPath[MAX_PATH];

	fe.cfFormat = CF_HDROP;
	fe.ptd = NULL;
	fe.dwAspect = DVASPECT_CONTENT;
	fe.lindex = -1;
	fe.tymed = TYMED_HGLOBAL;
	if (S_OK != pDataObj->GetData(&fe, &stgmedium))
		return E_FAIL;

	if (DragQueryFile((HDROP)stgmedium.hGlobal, 0, szPath, MAX_PATH)) {
		MyLoadLibrary(m_hWndMain, szPath);
	}

	ReleaseStgMedium(&stgmedium);

	return S_OK;
}

// set 1 line of listview with module info

void CProcWinView::SetModuleLine(int index, MODULEENTRY32 & me)
{
	LV_ITEM lv;
	char szStr[260];

	lv.mask = LVIF_TEXT | LVIF_PARAM;
	lv.iItem = index;
	lv.iSubItem = 0;
	lv.pszText = me.szModule;
	lv.lParam = (LPARAM)me.modBaseAddr;
	ListView_InsertItem(m_hWndListView, &lv);
	lv.iSubItem++;

	lv.mask = LVIF_TEXT;
	lv.pszText = szStr;
	sprintf(szStr,"%8X",me.modBaseAddr);
	ListView_SetItem(m_hWndListView, &lv);
	lv.iSubItem++;

	sprintf(szStr,"%10u",me.modBaseSize);
	ListView_SetItem(m_hWndListView, &lv);
	lv.iSubItem++;

	sprintf(szStr,"%u",me.ProccntUsage);
	ListView_SetItem(m_hWndListView, &lv);
	lv.iSubItem++;

	sprintf(szStr,"%s",me.szExePath);
	ListView_SetItem(m_hWndListView, &lv);
	lv.iSubItem++;
	return;
}

// fill listview with modules of a process

int CProcWinView::UpdateListViewModules(DWORD pid)
{
	HANDLE handle;
	MODULEENTRY32  me;
	int i,j;
	UINT ul = 0;
	MODULEINFO moduleinfo;
	HANDLE hProcess;
	DWORD dwModules;
	BOOL bError = FALSE;
	HMODULE * pModules;
	char szStr[128];

	// if the currently selected process is ProcWin itself
	// create a droptarget to allow the user to drag&drop a
	// dll into ProcWin's listview panel.

	if ((!m_pDropTarget) && (GetCurrentProcessId() == pid)) {
		if (m_pDropTarget = new CDropTarget(m_hWndMain))
			RegisterDragDrop(m_hWndListView, m_pDropTarget);
	}

	CheckSubMenuItem(2,IDM_MODULE);

	iProcLVMode = iLVMode;

	if (m_bUsePSAPI) {
		hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
		if (hProcess) {
			dwModules = 0;
			g_pfnEnumProcessModules(hProcess, (HMODULE *)&i, sizeof(i), &dwModules);
			pModules = (HMODULE *)malloc(dwModules);
			if (pModules) {
				g_pfnEnumProcessModules(hProcess, pModules, dwModules, &dwModules);
				dwModules = dwModules / sizeof(HMODULE);
				for (i = 0, j = 0; i < (int)dwModules; i++, j++) {
					me.szModule[0] = 0;
					me.szExePath[0] = 0;
					g_pfnGetModuleBaseName(hProcess, *(pModules+i), (LPSTR)&me.szModule, sizeof(me.szModule));
					g_pfnGetModuleFileNameEx(hProcess, *(pModules+i), (LPSTR)&me.szExePath, sizeof(me.szExePath));
					g_pfnGetModuleInformation(hProcess, *(pModules+i), &moduleinfo, sizeof(moduleinfo));
					me.modBaseAddr = (BYTE *)moduleinfo.lpBaseOfDll;
					me.modBaseSize = moduleinfo.SizeOfImage;
					me.ProccntUsage = 0;
					SetModuleLine(j, me);
					ul = ul + moduleinfo.SizeOfImage;

				}
				LocalFree(pModules);
			} else {
				sprintf(szStr, "Memory allocation error");
				bError = TRUE;
			}
			CloseHandle(hProcess);
		} else {
			sprintf(szStr, "OpenProcess(%X) failed[%X]", pid, GetLastError());
			bError = TRUE;
		}

	} else {

		if (handle = g_pfnCreateToolhelp32Snapshot(TH32CS_SNAPMODULE,pid)) {
			me.dwSize = sizeof(MODULEENTRY32);
			j = 0;
			i = g_pfnModule32First(handle, &me);
			while(i) {
				SetModuleLine(j, me);
				j++;
				ul = ul + me.modBaseSize;
				i = g_pfnModule32Next(handle,&me);
			}
			CloseHandle(handle);
		}
	}

	if (bError == FALSE)
		sprintf(szStr, pStrings[MODSTSTR],j,ul / 1024);

	SendMessage(m_hWndStatusBar, SB_SETTEXT, 1, (LPARAM)szStr);

	return 1;
}

int CProcWinView::UpdateListView(DWORD pid, int fMode)
{
	TV_ITEM tvi;
	HCURSOR hCursorOld;

	SendMessage(m_hWndListView,WM_SETREDRAW,0,0);
	SendMessage(m_hWndStatusBar, SB_SETTEXT, 0, (LPARAM)pStrings[BUSYSTR]); 

	hCursorOld = SetCursor(LoadCursor(0,IDC_WAIT));

	if (m_pDropTarget) {
		RevokeDragDrop(m_hWndListView);
		m_pDropTarget->Release();
		m_pDropTarget = NULL;
	}

	ListView_DeleteAllItems(m_hWndListView);

	if (pTHHOld) {
		free(pTHHOld);
		pTHHOld = NULL;
	}
	if (fMode == -1)
		fMode = iLVMode;


	SetListViewCols(fMode);

	if (!pid) {
		tvi.hItem = TreeView_GetSelection(m_hWndTreeView);
		tvi.mask = TVIF_PARAM;
		if (TreeView_GetItem(m_hWndTreeView,&tvi))
			pid = tvi.lParam;
		else
			pid = 0;
	}

	iSortColumn[iLVMode] = -1;
	iSortOrder[iLVMode] = 1;

	for (int i = 0;i < 3 ; i++) {
		if (iModes[i] == iLVMode)
			TabCtrl_SetCurSel(m_hWndTabCtrl,i);
	}

	if (pid)
	switch (iLVMode) {
	case LVMODE_MODULE:
		UpdateListViewModules(pid);
		break;
	case LVMODE_HEAP:
		UpdateListViewHeap(pid);
		break;
	case LVMODE_PROCINF:
		UpdateListViewProcInf(pid);
		break;
	case LVMODE_WINDOWS:
		UpdateListViewWindows(pid);
		break;
	}

	SendMessage(m_hWndListView,WM_SETREDRAW,1,0);
	ListView_SetItemState(m_hWndListView, 0, LVIS_FOCUSED, LVIS_FOCUSED);
	SendMessage(m_hWndStatusBar, SB_SETTEXT, 0, (LPARAM)pStrings[NBUSYSTR]); 

	SetCursor(hCursorOld);

	return 1;
}

#if OPTIONDLG

// the options dialog displays a dialog box with a tab control (2 tabs)

BOOL CProcWinView::OptionsDlg(HWND hWnd,UINT message,WPARAM wparam,LPARAM lparam)
{
	BOOL rc = FALSE;
	HWND hWndChild;
	HIMAGELIST hImageList;
	char szStr[80];
	TC_ITEM tci;
	int i;

	switch (message) {
	case WM_INITDIALOG:
		hWndChild = GetDlgItem(hWnd,IDC_TAB1);
		tci.mask = TCIF_TEXT | TCIF_PARAM;
#if 0
		hImageList = ImageList_LoadBitmap(pApp->hInstance,
					MAKEINTRESOURCE(IDB_BITMAP2),
					20,
					0,
					CLR_NONE);
		if (hImageList)
			TabCtrl_SetImageList(hWndChild,hImageList);
		tci.mask = TCIF_TEXT | TCIF_PARAM | TCIF_IMAGE;
#endif
		for (i = 0; i < 4; i++) {
			tci.lParam = 0;
			tci.iImage = i;
			sprintf(szStr,"Page %u",i);
			tci.pszText = szStr;
			TabCtrl_InsertItem(hWndChild,i,&tci);
		}

		rc = TRUE;
		break;
	case WM_CLOSE:
		EndDialog(hWnd,0);
		break;
	case WM_COMMAND:
		SendMessage(hWnd,WM_CLOSE,0,0);
		break;
	}
	return rc;
}

BOOL CALLBACK optionsproc(HWND hWnd,UINT message,WPARAM wparam,LPARAM lparam)
{
	return pProcWinView->OptionsDlg(hWnd,message,wparam,lparam);
}
#endif

#if DEBUGPROCESS
DWORD WINAPI debugprocessproc(VOID * pVoid)
{
	DWORD rc;
	TV_ITEM tvi;
	CProcWinView * pProcWinView = (CProcWinView *)pVoid;

	HTREEITEM hTreeItem = TreeView_GetSelection(pProcWinView->m_hWndTreeView);

	if (!TreeView_GetParent(pProcWinView->m_hWndTreeView,hTreeItem)) {
		tvi.mask = TVIF_PARAM | TVIF_HANDLE;
		tvi.hItem = hTreeItem;
		if (TreeView_GetItem(pProcWinView->m_hWndTreeView,&tvi)) {
			DEBUG_EVENT de;
			MSG msg;
			DebugActiveProcess(tvi.lParam);
			while (1) {
				WaitForDebugEvent(&de,INFINITE);
				if (de.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT) {
					break;
				}
				ContinueDebugEvent(de.dwProcessId,de.dwThreadId,
							DBG_CONTINUE);
			}
		}
	}
	return 0;
}
#endif

// thread is created with CreateThread(), not with the CRT _beginthread()
// therefore better don't use CRT functions!

DWORD WINAPI killprocessproc(VOID * pVoid)
{
	DWORD rc;
	CProcWinView * pProcWinView = (CProcWinView *)pVoid;
	HWND hWnd = pProcWinView->m_hWndMain;
	char szText[128];

	PostMessage(pProcWinView->m_hWndStatusBar, SB_SETTEXT, 1, (LPARAM)pStrings[TERMPRSTR]); 
	if (TerminateProcess(pProcWinView->m_hKillProcess, 0xFFFFFFFF)) {
		rc = WaitForSingleObject(pProcWinView->m_hKillProcess, 10000); // wait 10 seconds
		switch (rc) {
		case WAIT_TIMEOUT:
			MessageBox(hWnd,pStrings[ERRSTR14],0,MB_OK);
			break;
		case WAIT_FAILED:
			sprintf(szText, pStrings[ERRSTR13], GetLastError());
			MessageBox(hWnd, szText, 0, MB_OK);
			break;
		default:
			PostMessage(hWnd,WM_COMMAND,IDM_REFRESH,0);
		}
	} else {
		MessageBox(hWnd,pStrings[ERRSTR15],0,MB_OK);
	}
	CloseHandle(pProcWinView->m_hKillProcess);
	pProcWinView->m_hKillProcess = 0;
	PostMessage(pProcWinView->m_hWndStatusBar, SB_SETTEXT, 1, (LPARAM)""); 

	return 0;
}

// kill selected prozess (in a separate thread)

void CProcWinView::KillProcess()
{
	TV_ITEM tvi;
	DWORD dwThreadId;
	//DWORD rc;
	DWORD dwFlags;

	// TreeView_GetSelection() returns the "selected" item, but this
	// is not necessarily the item selected by the right mouse button!

	HTREEITEM hTreeItem = TreeView_GetSelection(m_hWndTreeView);

#if 0
	if ( m_hKillProcess ) {
		if ( rc == STILL_ACTIVE ) {
			MessageBox(m_hWndMain,pStrings[THRDSTR1],0,MB_OK);
			return;
		}
	}
#endif

	if (hTreeItem)
		if (!TreeView_GetParent(m_hWndTreeView,hTreeItem)) {
			tvi.mask = TVIF_PARAM | TVIF_HANDLE;
			tvi.hItem = hTreeItem;
			if (TreeView_GetItem(m_hWndTreeView,&tvi))
				if (GetVersion() & 0x80000000)      // win9x platform?
					dwFlags = PROCESS_TERMINATE;
				else
					dwFlags = PROCESS_TERMINATE | SYNCHRONIZE;
				if (m_hKillProcess = OpenProcess(dwFlags, 0, tvi.lParam)) {
					CreateThread(0,0x2000,killprocessproc,this,0,&dwThreadId);
				} else {
					MessageBox(m_hWndMain, pStrings[NOPROCSTR],0,MB_OK);
					UpdateTreeView();
				}
		}
	return;
}

#if DEBUGPROCESS
void CProcWinView::DebugProcess()
{
	DWORD dwThreadId;
	CreateThread(0,0x1000,debugprocessproc,this,0,&dwThreadId);
	return;
}
#endif


void ExploreDir(HWND hWnd, char * pszPath)
{
	int i;
	SHELLEXECUTEINFO sei;

	i = lstrlen(pszPath);
	for (;i;i--)
		if (*(pszPath+i-1) == '\\') {
			*(pszPath+i-1) = 0;
			break;
		}
	sei.cbSize = sizeof(SHELLEXECUTEINFO);
	sei.fMask = SEE_MASK_INVOKEIDLIST;
	sei.hwnd = hWnd;
	sei.lpVerb = "open";
	sei.lpFile = pszPath;
	sei.lpParameters = NULL;
	sei.lpDirectory = NULL;
	sei.nShow = SW_SHOWDEFAULT;
	sei.lpIDList = NULL;
	if (!ShellExecuteEx(&sei)) {
		sprintf(pszPath, "ShellExecuteEx() failed [%X]", GetLastError());
		MessageBox(hWnd, pszPath, 0, MB_OK);
	}
	return;
}

// explore menu item selected
// start explorer, which should display the directory with
// the file being selected. This needs shell functions
// SHParseDisplayName() and SHOpenFolderAndSelectItems(),
// which works for WinXP and newer only.

typedef HRESULT (WINAPI * LPSHPARSEDISPLAYNAME)(PCWSTR, void *, LPCITEMIDLIST *, SFGAOF, SFGAOF *);
typedef HRESULT (WINAPI * LPSHOPENFOLDERANDSELECTITEMS)(LPCITEMIDLIST, UINT, LPCITEMIDLIST, DWORD);

int CProcWinView::OnExplore(HWND hWnd)
{
	int i,j;
	LV_ITEM lvi;
	HMODULE hShell32;
	LPSHPARSEDISPLAYNAME pfnSHParseDisplayName;
	LPSHOPENFOLDERANDSELECTITEMS pfnSHOpenFolderAndSelectItems;
	LPCITEMIDLIST pidl;
	LPMALLOC pMalloc;
	DWORD dwSFGAO;
	char szPath[MAX_PATH+64];
	WCHAR wszPath[MAX_PATH];

	i = ListView_GetNextItem(m_hWndListView,-1,LVNI_SELECTED);
	if (i == -1)
		return 0;
	lvi.mask = LVIF_TEXT;
	lvi.pszText = szPath;
	lvi.cchTextMax = sizeof(szPath);
	lvi.iItem = i;
	lvi.iSubItem = 4;
	if (ListView_GetItem(m_hWndListView,&lvi)) {
		hShell32 = GetModuleHandle("shell32");
		if (hShell32) {
			pfnSHParseDisplayName = (LPSHPARSEDISPLAYNAME)GetProcAddress(hShell32, "SHParseDisplayName");
			if (pfnSHOpenFolderAndSelectItems = (LPSHOPENFOLDERANDSELECTITEMS)GetProcAddress(hShell32, "SHOpenFolderAndSelectItems")) {
				MultiByteToWideChar(CP_ACP,MB_PRECOMPOSED, szPath, -1, wszPath, MAX_PATH);
				j = pfnSHParseDisplayName(wszPath, NULL, &pidl, NULL, &dwSFGAO);
				if (j == S_OK) {
					if (S_OK != pfnSHOpenFolderAndSelectItems(pidl, 0, NULL, NULL))
						MessageBox(hWnd, "SHOpenFolderAndSelectItems() failed", 0, MB_OK);
					if (NOERROR == SHGetMalloc(&pMalloc)) {
						pMalloc->Free((void *)pidl);
						pMalloc->Release();
					}
				} else {
// SHParseDisplayName may fail.
#if 1
					ExploreDir(hWnd, szPath);
#else
					sprintf(szPath, "SHParseDisplayName(%S) failed [%X]", wszPath, j);
					MessageBox(hWnd, szPath, 0, MB_OK);
#endif
				}
			} else
				ExploreDir(hWnd, szPath);
		}
	}

	return 1;
}

int CProcWinView::ShowProperties(HWND hWnd)
{
	int i;
	LV_ITEM lvi;
	SHELLEXECUTEINFO sei;
	char szPath[MAX_PATH];

	i = ListView_GetNextItem(m_hWndListView,-1,LVNI_SELECTED);
	if (i == -1)
		return 0;
	lvi.mask = LVIF_TEXT;
	lvi.pszText = szPath;
	lvi.cchTextMax = sizeof(szPath);
	lvi.iItem = i;
	lvi.iSubItem = 4;
	if (ListView_GetItem(m_hWndListView,&lvi)) {
		sei.cbSize = sizeof(SHELLEXECUTEINFO);
		sei.fMask = SEE_MASK_INVOKEIDLIST;
		sei.hwnd = hWnd;
		sei.lpVerb = "properties";
		sei.lpFile = szPath;
		sei.lpParameters = NULL;
		sei.lpDirectory = NULL;
		sei.nShow = SW_SHOWDEFAULT;
		sei.lpIDList = NULL;
		if (!ShellExecuteEx(&sei)) {
			i = GetLastError();
			sprintf(szPath, "ShellExecuteEx() failed with error %X", i);
			MessageBox(hWnd, szPath, 0, MB_OK); 
		}
	}
	return 1;
}

//

int CProcWinView::ShowModule(HWND hWnd, int iMode)
{
	RECT rect;
	int i,j;
	CModView * pMV = 0;
	DWORD pid = 0;
	DWORD modBase;
	DWORD modSize;
	TV_ITEM tvi;
	LV_ITEM lvi;
	char szWndCap[260];
	char szStr[260];
	char szProcess[64];
	BOOL bModule;

	i = ListView_GetNextItem(m_hWndListView,-1,LVNI_SELECTED);
	if (i == -1)
		return 0;

	szProcess[0] = 0;
	tvi.hItem = TreeView_GetSelection(m_hWndTreeView);
	tvi.mask = TVIF_PARAM | TVIF_TEXT;
	tvi.pszText = szProcess;
	tvi.cchTextMax = sizeof(szProcess);
	if (TreeView_GetItem(m_hWndTreeView,&tvi)) {
		pid = tvi.lParam;
		strcat(szProcess,", ");
	}

	switch (iLVMode) {
	case LVMODE_MODULE: 

		lvi.mask = LVIF_TEXT;
		lvi.iItem = i;

		// get the name of the module

		lvi.iSubItem = 4;
		lvi.pszText = szStr;
		lvi.cchTextMax = sizeof(szStr);
		ListView_GetItem(m_hWndListView,&lvi);
		j = lstrlen(lvi.pszText);
		for (;j;j--)
			if (*(lvi.pszText+j-1) == '\\')
				break;

		// use name + process as window caption

		sprintf(szWndCap, "%s, %s", lvi.pszText+j, szProcess);

		lvi.iSubItem = 3;
		lvi.pszText = szStr;
		lvi.cchTextMax = sizeof(szStr);
		ListView_GetItem(m_hWndListView,&lvi);

		strcat(szWndCap,szStr);

		lvi.iSubItem = 1;
		ListView_GetItem(m_hWndListView,&lvi);
		sscanf(szStr," %X",&modBase);

		lvi.iSubItem = 2;
		ListView_GetItem(m_hWndListView,&lvi);
		sscanf(szStr," %u",&modSize);

		bModule = TRUE;
		break;
	case LVMODE_PROCINF:
		strcpy(szWndCap,szProcess);
		lvi.mask = LVIF_PARAM | LVIF_TEXT;
		lvi.iItem = i;
		lvi.iSubItem = 0;
		lvi.pszText = szStr;
		lvi.cchTextMax = sizeof(szStr);
		ListView_GetItem(m_hWndListView,&lvi);
		sscanf(szStr," %X",&modBase);
//		modBase = lvi.lParam;

		strcat(szWndCap,szStr);

		lvi.iSubItem = 1;
		ListView_GetItem(m_hWndListView,&lvi);
		sscanf(szStr," %X",&modSize);
		bModule = FALSE;
		break;
	case LVMODE_HEAP:
		strcpy(szWndCap,szProcess);
		lvi.mask = LVIF_TEXT;
		lvi.iItem = i;
		lvi.iSubItem = 2;
		lvi.pszText = szStr;
		lvi.cchTextMax = sizeof(szStr);
		ListView_GetItem(m_hWndListView,&lvi);
		sscanf(szStr,"%X",&modBase);
//		modBase = ((THHeap *)(lvi.lParam))->dwAddress;

		lvi.iSubItem = 3;
		ListView_GetItem(m_hWndListView,&lvi);
		sscanf(szStr," %u",&modSize);

		sprintf(szStr,pStrings[HEAPADSTR],modBase);

		strcat(szWndCap,szStr);
		bModule = FALSE;

		break;
	default:
		return 0;
	}

	GetWindowRect(hWnd,&rect);
	pMV = new CModView(szWndCap,0,rect.left + 50, rect.top + 50, hWnd, iMode, pid, modBase, modSize, bModule);
	if (pMV)
		if (!pMV->ShowWindow(SW_SHOWNORMAL))
			delete pMV;

	return 1;
}

// write content of listview in a file

int CProcWinView::SaveListViewContent( PSTR pszFileName, BOOL bSelectedOnly )
{
	int i,iNextItem, iColumns;
	unsigned int col_flg = 0;
	int hFile = -1;
	LV_ITEM lvi;
	LVCOLUMN lvc;
	HGLOBAL hMem;
	char *pMem;
	char szStr[260];

	if (pszFileName) {
		hFile = _sopen(pszFileName,_O_WRONLY | _O_CREAT | _O_BINARY | _O_TRUNC, _SH_DENYWR, _S_IREAD | _S_IWRITE);
		if (hFile == -1)
			return 0;
	} else {
		if (!(hMem = GlobalAlloc(GMEM_FIXED | GMEM_MOVEABLE | GMEM_ZEROINIT, 0x10000)))
			return 0;
		pMem = (char *)GlobalLock(hMem);
		OpenClipboard(m_hWndMain);
		EmptyClipboard();
	}

	for ( i = 0; i < 32;i++ ) {
		lvc.mask = LVCF_TEXT | LVCF_WIDTH;
		lvc.pszText = szStr;
		lvc.cchTextMax = sizeof(szStr);
		if ( !ListView_GetColumn(m_hWndListView,i,&lvc) )
			break;
		if ( lvc.cx == 0 ) /* don't write columns with size 0 */
			continue;
		col_flg |= ( 1 << i );
		if ( (hFile != -1) && (i) )
			_write(hFile,"\t",1);
		if (hFile != -1)
			_write(hFile,szStr,strlen(szStr));
	}
	if (hFile != -1)
		_write(hFile,"\r\n",2);

	iColumns = i;
	if (bSelectedOnly)
		iNextItem = ListView_GetNextItem(m_hWndListView,-1,LVNI_SELECTED);
	else
		iNextItem = ListView_GetNextItem(m_hWndListView,-1,LVNI_ALL);
	while (iNextItem != -1) {
		lvi.iItem = iNextItem;
		for ( i = 0; i < iColumns;i++ ) {
			/* skip columns with size 0 */
			if ( col_flg & ( 1 << i ) ) {
				lvi.iSubItem = i;
				lvi.mask = LVIF_TEXT;
				lvi.pszText = szStr;
				lvi.cchTextMax = sizeof(szStr);
				if (!ListView_GetItem(m_hWndListView,&lvi))
					break;
				if (i)
					if (hFile != -1)
						_write(hFile,"\t",1);
					else
						*pMem++ = '\t';
				if (hFile != -1)
					_write(hFile,szStr,strlen(szStr));
				else {
					strcpy(pMem, szStr);
					pMem = pMem + strlen(pMem);
				}
			}
		}
		if (hFile != -1)
			_write(hFile,"\r\n",2);
		else {
			*pMem++ = '\r';
			*pMem++ = '\n';
		}
		if (bSelectedOnly)
			iNextItem = ListView_GetNextItem(m_hWndListView,iNextItem,LVNI_SELECTED);
		else
			iNextItem = ListView_GetNextItem(m_hWndListView,iNextItem,LVNI_ALL);
	}
	if (hFile != -1)
		_close(hFile);
	else {
		GlobalUnlock(pMem);
		SetClipboardData(CF_TEXT, hMem);
		CloseClipboard();
	}
	return 1;
}

int CProcWinView::OnSaveAs(HWND hWnd)
{
	OPENFILENAME ofn;
	char szStr1[260];
	char szStr2[128];
	char szStr3[128];

	strcpy(szStr1,"");

	memset(szStr2,0,sizeof(szStr2));
	strcpy(szStr2,pStrings[TEXTFSTR]);
	strcpy(szStr2 + strlen(szStr2) + 1,pStrings[TEXTFPAT]);
	memset(szStr3,0,sizeof(szStr3));
	strcpy(szStr3,pStrings[ALLFSTR]);
	strcpy(szStr3 + strlen(szStr3) + 1,pStrings[ALLFPAT]);

	memset(&ofn,0,sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFilter = szStr3;
	ofn.lpstrCustomFilter = szStr2;
	ofn.nMaxCustFilter = sizeof(szStr2);
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = szStr1;
	ofn.nMaxFile = sizeof(szStr1);
	ofn.Flags = OFN_EXPLORER;
//	ofn.Flags = 0;
	if ( GetSaveFileName( &ofn ) ) {
		SaveListViewContent( szStr1, FALSE );
//		MessageBox(hWnd,"Sorry, function not implemented yet",NULL,MB_OK);
	}
	return 1;
}

int CProcWinView::OnCopy(HWND hWnd)
{
	SaveListViewContent(0, TRUE);
	return 1;
}

// load a dll into ProcWin's process

int MyLoadLibrary(HWND hWnd, LPSTR lpszName)
{
	char szPath[MAX_PATH];
	char szOldDir[MAX_PATH];
	char * psz1;

	GetCurrentDirectory(sizeof(szOldDir), szOldDir);

	strcpy(szPath, lpszName);
	for (psz1 = szPath + strlen(szPath) - 1; psz1 != szPath; psz1--)
		if (*psz1 == '\\')
			break;
	*psz1 = 0;
	if (szPath[0])
		if (!SetCurrentDirectory(szPath))
			MessageBox(hWnd, szPath, "SetCurrentDirectory failed", MB_OK);

	if (LoadLibrary(lpszName) <= (HINSTANCE)32)
		MessageBox(hWnd, lpszName, "Cannot load Library", MB_OK);
	else
		PostMessage(hWnd, WM_COMMAND, IDM_LVUPDATE, 0);

	SetCurrentDirectory(szOldDir);

	return strlen(lpszName);
}

int CProcWinView::OnLoadLibrary(HWND hWnd)
{
	OPENFILENAME ofn;
	char szStr1[260];
	char szStr2[128];
	char szStr3[128];

	strcpy(szStr1,"");

	memset(szStr2,0,sizeof(szStr2));
	strcpy(szStr2,pStrings[DLLFSTR]);
	strcpy(szStr2 + strlen(szStr2) + 1,pStrings[DLLFPAT]);
	memset(szStr3,0,sizeof(szStr3));
	strcpy(szStr3,pStrings[ALLFSTR]);
	strcpy(szStr3 + strlen(szStr3) + 1,pStrings[ALLFPAT]);

	memset(&ofn,0,sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFilter = szStr3;
	ofn.lpstrCustomFilter = szStr2;
	ofn.nMaxCustFilter = sizeof(szStr2);
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = szStr1;
	ofn.nMaxFile = sizeof(szStr1);
	ofn.Flags = OFN_EXPLORER;
	if (GetOpenFileName(&ofn)) {
		MyLoadLibrary(hWnd, szStr1);
	}
	return 1;
}

int CProcWinView::OnStartApp(HWND hWnd)
{
	OPENFILENAME ofn;
	char szStr1[260];
	char szStr2[128];
	char szStr3[128];

	strcpy(szStr1,"");

	memset(szStr2,0,sizeof(szStr2));
	strcpy(szStr2,pStrings[APPFSTR]);
	strcpy(szStr2 + strlen(szStr2) + 1,pStrings[APPFPAT]);
	memset(szStr3,0,sizeof(szStr3));
	strcpy(szStr3,pStrings[ALLFSTR]);
	strcpy(szStr3 + strlen(szStr3) + 1,pStrings[ALLFPAT]);

	memset(&ofn,0,sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFilter = szStr3;
	ofn.lpstrCustomFilter = szStr2;
	ofn.nMaxCustFilter = sizeof(szStr2);
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = szStr1;
	ofn.nMaxFile = sizeof(szStr1);
	ofn.Flags = OFN_EXPLORER;
	if (GetOpenFileName(&ofn)) {
		WinExec(szStr1, SW_NORMAL);
	}
	return 1;
}

BOOL CALLBACK setprioritydlgproc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int i;
	DWORD dwBtnId[4] = {IDC_RADIO1, IDC_RADIO2, IDC_RADIO3, IDC_RADIO4};
	DWORD dwClass[4] = {IDLE_PRIORITY_CLASS, NORMAL_PRIORITY_CLASS, HIGH_PRIORITY_CLASS, REALTIME_PRIORITY_CLASS};

	switch (message) {
	case WM_INITDIALOG:
		for (i = 0; i < 4; i++) {
			if (dwClass[i] == lParam) {
				CheckRadioButton(hWnd, IDC_RADIO1, IDC_RADIO4, dwBtnId[i]);
				break;
			}
		}
		return TRUE;
		break;
	case WM_CLOSE:
		EndDialog(hWnd,0);
		break;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK) {
			for (i = 0;i < 4; i++) {
				if (IsDlgButtonChecked(hWnd, dwBtnId[i]))
					break;
			}
			EndDialog(hWnd, dwClass[i]);
		}
		if (LOWORD(wParam) == IDCANCEL) {
			EndDialog(hWnd,0);
		}
		break;
	}
	return FALSE;
}


int CProcWinView::OnSetPriority(HWND hWnd)
{
	TV_ITEM tvi;
	DWORD rc;
	HANDLE hProcess;

	HTREEITEM hTreeItem = TreeView_GetSelection(m_hWndTreeView);

	if (hTreeItem)
		if (!TreeView_GetParent(m_hWndTreeView,hTreeItem)) {
			tvi.mask = TVIF_PARAM | TVIF_HANDLE;
			tvi.hItem = hTreeItem;
			if (TreeView_GetItem(m_hWndTreeView,&tvi))
				if (hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, 0,tvi.lParam)) {
					rc = GetPriorityClass(hProcess);
					CloseHandle(hProcess);
					rc = DialogBoxParam(pApp->hInstance, MAKEINTRESOURCE(IDD_SETPRIORITY),
						hWnd, setprioritydlgproc, rc);
					if (!rc)
						return 0;
					if (hProcess = OpenProcess(PROCESS_SET_INFORMATION, 0,tvi.lParam)) {
						SetPriorityClass(hProcess, rc);
                        CloseHandle(hProcess);
						UpdateTreeView();
					} else {
						MessageBox(m_hWndMain,pStrings[NOPROCSTR],0,MB_OK);
					}
				} else {
					MessageBox(m_hWndMain,pStrings[NOPROCSTR],0,MB_OK);
				}
		}
	return 1;
}

int CProcWinView::OnThreadMsg(HWND hWnd)
{
	TV_ITEM tvi;
	//DWORD rc;

	HTREEITEM hTreeItem = TreeView_GetSelection(m_hWndTreeView);

	if (hTreeItem)
		if (TreeView_GetParent(m_hWndTreeView,hTreeItem)) {
			tvi.mask = TVIF_PARAM | TVIF_HANDLE;
			tvi.hItem = hTreeItem;
			if (TreeView_GetItem(m_hWndTreeView,&tvi))
				PostThreadMessage(tvi.lParam, WM_USER, 0, 0);
		}
	return 1;
}

// resort listview

int CProcWinView::CompareListViewItems(LPARAM lParam1, LPARAM lParam2)
{
	char szStr1[128];
	char szStr2[128];       
	int i;
//	DWORD dw1,dw2;
//	UINT ui1,ui2;
	LV_ITEM lvi1;
	LV_ITEM lvi2;
	THHeap * pTHH1;
	THHeap * pTHH2;

	if (iLVMode != LVMODE_HEAP) {
		lvi1.iItem = (int)lParam1;
		lvi1.iSubItem = iSortColumn[iLVMode];
		lvi1.mask = LVIF_TEXT;
		lvi1.pszText = szStr1;
		lvi1.cchTextMax = sizeof(szStr1);
		ListView_GetItem(m_hWndListView,&lvi1);

		lvi2.iItem = (int)lParam2;
		lvi2.iSubItem = iSortColumn[iLVMode];
		lvi2.mask = LVIF_TEXT;
		lvi2.pszText = szStr2;
		lvi2.cchTextMax = sizeof(szStr2);
		ListView_GetItem(m_hWndListView,&lvi2);
	}

	switch (iLVMode) {
	case LVMODE_HEAP:
		pTHH1 = pTHHOld + lParam1;
		pTHH2 = pTHHOld + lParam2;
		switch (iSortColumn[iLVMode]) {
		case 0:
			i = pTHH1->dwHeapID - pTHH2->dwHeapID;
			break;
		case 1:
			i = (int)(pTHH1->hHandle) - (int)(pTHH2->hHandle);
			break;
		case 2:
			i = pTHH1->dwAddress - pTHH2->dwAddress;
			break;
		case 3:
			i = pTHH1->dwBlockSize - pTHH2->dwBlockSize;
			break;
		case 4:
			i = pTHH1->dwFlags - pTHH2->dwFlags;
			break;
		}
		break;
	default:
		i = strcmp(szStr1,szStr2);
	}
	if (iSortOrder[iLVMode])
		return i;
	else
		return 0 - i;
}

// before sorting fill lParam with iItem

int CProcWinView::SetListViewlParam()
{
	int i,j;
	LV_ITEM lvi;
	
	if (iLVMode != LVMODE_HEAP) {		// for LVMODE_HEAP dont change lParam!
		j = ListView_GetItemCount(m_hWndListView);
		lvi.iSubItem = 0;
		lvi.mask = LVIF_PARAM;
		for (i = 0;i < j;i++) {
			lvi.iItem = i;
			ListView_GetItem(m_hWndListView,&lvi);
			lvi.lParam = i;
			ListView_SetItem(m_hWndListView,&lvi);
		}
	}
	return j;
}

// if more than 1 item is selected, display some totals in status line

int CProcWinView::UpdateStatusBarInfo(void)
{
	UINT dwReserved = 0;
	UINT dwCommitted = 0;
	UINT dwUsed = 0;
	UINT dwDirty = 0;
	UINT dwSize = 0;
	int i,j;
	LV_ITEM lvi;
	char szStr[128];

	i = ListView_GetNextItem(m_hWndListView,-1,LVNI_SELECTED);
	j = 0;
	while (i != -1) {
		lvi.mask = LVIF_TEXT;
		lvi.iItem = i;
		lvi.pszText = szStr;
		lvi.cchTextMax = sizeof(szStr);
		switch (iLVMode) {
		case LVMODE_MODULE:
			lvi.iSubItem = 2;						// Size
			ListView_GetItem(m_hWndListView,&lvi);
			sscanf(szStr," %u",&dwSize);
			dwUsed = dwUsed + dwSize;
			break;
		case LVMODE_PROCINF:
			lvi.iSubItem = 1;
			ListView_GetItem(m_hWndListView,&lvi);
			sscanf(szStr," %X",&dwSize);			// Size in Hex

			lvi.iSubItem = 2;
			ListView_GetItem(m_hWndListView,&lvi);
			if (!strcmp(szStr,"free")) {
				;
			} else {
				dwReserved = dwReserved + dwSize;
				if (!strcmp(szStr,"commit"))
					dwCommitted = dwCommitted + dwSize;
			}
			lvi.iSubItem = 5;
			ListView_GetItem(m_hWndListView,&lvi);
			if (!strcmp(szStr,"readwrite"))
				dwDirty = dwDirty + dwSize;
			break;
		case LVMODE_HEAP:
			lvi.iSubItem = 3;
			ListView_GetItem(m_hWndListView,&lvi);
			if (sscanf(szStr," %u",&dwSize))		// Size in dez
				dwUsed = dwUsed + dwSize;
			break;
		}
		j++;
		i = ListView_GetNextItem(m_hWndListView,i,LVNI_SELECTED);
	}

	if (j > 1) {
		switch (iLVMode) {
		case LVMODE_MODULE:
			sprintf(szStr,pStrings[SELMODULE],
					j, dwUsed/1024);
			break;
		case LVMODE_PROCINF:
			sprintf(szStr,pStrings[SELSTSTR],
					j, dwReserved/1024, dwCommitted/1024, dwDirty/1024);
			break;
		case LVMODE_HEAP:
			sprintf(szStr,pStrings[SELHEAP],
					j, dwUsed/1024, dwUsed);
			break;
		}

		SendMessage(m_hWndStatusBar,SB_SIMPLE,1,0);	// simple mode einschalten		

		SendMessage(m_hWndStatusBar, SB_SETTEXT, 255, (LPARAM)szStr);
	} else
		SendMessage(m_hWndStatusBar,SB_SIMPLE,0,0);		

	fSelChanged = FALSE;

	return 1;
}

//

int CProcWinView::DoCloseWindow(void)
{
	int i;
	LV_ITEM lvi;

	if ((i = ListView_GetNextItem(m_hWndListView,-1,LVNI_SELECTED)) == -1)
		return 0;
	lvi.mask = LVIF_PARAM;
	lvi.iItem = i;
	lvi.iSubItem = 0;
	if (ListView_GetItem(m_hWndListView,&lvi))
		SendMessage((HWND)lvi.lParam,WM_CLOSE,0,0);
	return 1;
}

//

int CProcWinView::DoActivateWindow(int fMode)
{
	int i;
	LV_ITEM lvi;

	if ((i = ListView_GetNextItem(m_hWndListView,-1,LVNI_SELECTED)) == -1)
		return 0;
	lvi.mask = LVIF_PARAM;
	lvi.iItem = i;
	lvi.iSubItem = 0;
	ListView_GetItem(m_hWndListView,&lvi);
	ShowWindow((HWND)lvi.lParam,fMode);

	return 1;
}


int CProcWinView::DoEnableWindow(void)
{
	int i;
	LV_ITEM lvi;

	if ((i = ListView_GetNextItem(m_hWndListView,-1,LVNI_SELECTED)) == -1)
		return 0;
	lvi.mask = LVIF_PARAM;
	lvi.iItem = i;
	lvi.iSubItem = 0;
	if (ListView_GetItem(m_hWndListView,&lvi))
		EnableWindow((HWND)lvi.lParam, TRUE);
	return 1;
}


int CProcWinView::OnFlashWindow(void)
{
	int i;
	LV_ITEM lvi;

	if ((i = ListView_GetNextItem(m_hWndListView,-1,LVNI_SELECTED)) == -1)
		return 0;
	lvi.mask = LVIF_PARAM;
	lvi.iItem = i;
	lvi.iSubItem = 0;
	if (ListView_GetItem(m_hWndListView,&lvi))
		FlashWindow((HWND)lvi.lParam, TRUE);
	return 1;
}


// set Edit main menu items

void CProcWinView::EnableEditMenu(HWND hWnd, int iModeProc, int iModeWin, int lParent)
{
	HMENU hMenu;
	BOOL fTB;

	if (iModeProc & MF_DISABLED)
		fTB = FALSE;
	else
		fTB = TRUE;

	if (iModeProc != -1) {
		hMenu = GetSubMenu(GetMenu(hWnd), 1);
		EnableMenuItem(hMenu,IDM_KILL,iModeProc | MF_BYCOMMAND);
		EnableMenuItem(hMenu,IDM_SETPRIORITY,iModeProc | MF_BYCOMMAND);
#if DEBUGPROCESS
		EnableMenuItem(hMenu,IDM_DEBUGPROCESS,iModeProc | MF_BYCOMMAND);
#endif
		SendMessage(m_hWndToolbar,TB_ENABLEBUTTON,IDM_KILL,MAKELONG(fTB,0));
		if (lParent) 
			EnableMenuItem(hMenu,IDM_THREADMSG, MF_ENABLED | MF_BYCOMMAND);
		else
			EnableMenuItem(hMenu,IDM_THREADMSG, MF_GRAYED | MF_BYCOMMAND);

	}
	if (iModeWin != -1) {
		hMenu = GetSubMenu(GetMenu(hWnd), 4);
		EnableMenuItem(hMenu,IDM_CLOSE,iModeWin | MF_BYCOMMAND);
		EnableMenuItem(hMenu,IDM_ACTIVATE,iModeWin | MF_BYCOMMAND);
		EnableMenuItem(hMenu,IDM_HIDE,iModeWin | MF_BYCOMMAND);
		EnableMenuItem(hMenu,IDM_ENABLE,iModeWin | MF_BYCOMMAND);
		EnableMenuItem(hMenu,IDM_FLASH,iModeWin | MF_BYCOMMAND);
	}
	return;
}

// set View main menu items

void CProcWinView::EnableViewMenu(HWND hWnd, int iMode)
{
	UINT uMode1, uMode2;
	HMENU hMenu = GetSubMenu(GetMenu(hWnd),2);

	if (iMode) {
		uMode1 = MF_ENABLED | MF_BYCOMMAND;
		uMode2 = MF_DISABLED | MF_GRAYED | MF_BYCOMMAND;
	} else {
		uMode2 = MF_ENABLED | MF_BYCOMMAND;
		uMode1 = MF_DISABLED | MF_GRAYED | MF_BYCOMMAND;
	}
	EnableMenuItem(hMenu,IDM_MODULE,uMode1);
	EnableMenuItem(hMenu,IDM_PROCINF,uMode1);
	EnableMenuItem(hMenu,IDM_HEAP,uMode1);
	EnableMenuItem(hMenu,IDM_WINDOWS,uMode2);

	return;
}

// set menu item state of submenu 3 ("object")
// iMode: 0=treeview
// 1=listview, modules
// 2=listview, memory / heap

#define OBJECTMENUITEMS 13

void CProcWinView::EnableExtraMenu(HWND hWnd, int iMode)
{
	UINT uFlags[OBJECTMENUITEMS];
	int i;
	HMENU hMenu = GetSubMenu(GetMenu(hWnd),3);

	switch (iMode) {
	case 0:
		for (i = 0; i < OBJECTMENUITEMS; i++)
			uFlags[i] = MF_BYPOSITION | MF_DISABLED | MF_GRAYED;
		break;
	case 1:
		for (i = 0; i < OBJECTMENUITEMS; i++)
			uFlags[i] = MF_BYPOSITION | MF_ENABLED;
		break;
	case 2:
		uFlags[0] = MF_BYPOSITION | MF_ENABLED;
		for (i = 1; i < OBJECTMENUITEMS; i++)
			uFlags[i] = MF_BYPOSITION | MF_DISABLED | MF_GRAYED;
		break;
	}
	for (i = 0; i < OBJECTMENUITEMS; i++)
		EnableMenuItem(hMenu,i,uFlags[i]);

	return;
}

int CProcWinView::SetMenus(HWND hWnd, LPNMHDR pNMHdr)
{
	int iNewLVMode;
	int iModeProc;
	HMENU hMenu = GetMenu(hWnd);
	HTREEITEM hTreeItem;
	HTREEITEM hTreeParent;
	LPNM_TREEVIEW pNMTV;

	switch (pNMHdr->idFrom) {
	case IDC_TREEVIEW:
		pNMTV = (LPNM_TREEVIEW)pNMHdr;
		if (hTreeItem = TreeView_GetSelection(m_hWndTreeView))
			hTreeParent = TreeView_GetParent(pNMTV->hdr.hwndFrom,hTreeItem);

		if (!hTreeItem) {
			iModeProc = MF_DISABLED | MF_GRAYED;
			iNewLVMode = -1;
			hContextMenu = 0;
		} else
			if (!hTreeParent) {
				iModeProc = MF_ENABLED;
				iNewLVMode = iProcLVMode;
				hContextMenu = GetSubMenu(hMenu,1);
			} else {
				iModeProc = MF_DISABLED | MF_GRAYED;
				iNewLVMode = LVMODE_WINDOWS;
				hContextMenu = GetSubMenu(hMenu,1);
			}
//		if (iNewLVMode != iLVMode) {
		EnableEditMenu(hWnd,iModeProc,-1, (int)hTreeParent);
		EnableMenuItem(hMenu,IDM_COPY,MF_DISABLED | MF_GRAYED | MF_BYCOMMAND);

		EnableViewMenu(hWnd,hTreeParent == 0);
		EnableExtraMenu(hWnd,0);
///		}
		return iNewLVMode;
		break;
	case IDC_LISTVIEW:
		EnableEditMenu(hWnd,-1,MF_DISABLED | MF_GRAYED, 0);
		EnableExtraMenu(hWnd,0);
		if (ListView_GetSelectedCount(m_hWndListView) < 1) {
			EnableMenuItem(GetSubMenu(hMenu,1),IDM_COPY,MF_DISABLED | MF_GRAYED | MF_BYCOMMAND);
			break;
		}
		EnableMenuItem(GetSubMenu(hMenu,1),IDM_COPY,MF_ENABLED | MF_BYCOMMAND);
		switch (iLVMode) {
		case LVMODE_MODULE:
			EnableExtraMenu(hWnd,1);
			hContextMenu = GetSubMenu(hMenu,3);
			break;
		case LVMODE_PROCINF:
		case LVMODE_HEAP:
			EnableExtraMenu(hWnd,2);
			hContextMenu = GetSubMenu(hMenu,3);
			break;
		case LVMODE_WINDOWS:
			EnableEditMenu(hWnd,-1,MF_ENABLED, 0);
			hContextMenu = GetSubMenu(hMenu,4);
			break;
		default:
			hContextMenu = NULL;
		}
		break;
	}
	return 0;
}

// handle WM_NOTIFY

LRESULT CProcWinView::OnNotify(HWND hWnd,WPARAM wParam,LPNMHDR pNMHdr)
{
	LPNM_LISTVIEW pNMLV;
	LRESULT rc = 0;
//	HMENU hMenu;
	LV_DISPINFO * pDI;
	TC_ITEM tci;
	int i;

	switch (pNMHdr->code) {
	case TVN_SELCHANGED: // if another treeview item is selected, renew right panel

		i = SetMenus(hWnd,pNMHdr);

		if (i != iLVMode)
			UpdateListView(0,i);
		else
			PostMessage(hWnd,WM_COMMAND,IDM_LVUPDATE,0);
		break;
	case LVN_GETDISPINFO:
		pDI = (LV_DISPINFO *)pNMHdr;
		if (pDI->item.mask & LVIF_TEXT)
			SetListViewHeapLine2(pDI);
		break;
	case LVN_ITEMCHANGED:
		pNMLV = (LPNM_LISTVIEW)pNMHdr;

		i = SetMenus(hWnd,pNMHdr);

		if (ListView_GetSelectedCount(m_hWndListView) > 1) {
			switch (iLVMode) {
			case LVMODE_MODULE:
			case LVMODE_PROCINF:
			case LVMODE_HEAP:
				if (!fSelChanged) {
					fSelChanged = TRUE;
					PostMessage(hWnd,WM_COMMAND,IDM_SBUPDATE,0);
				}
				break;
			}
		} else
			SendMessage(m_hWndStatusBar,SB_SIMPLE,0,0);
		break;
	case LVN_COLUMNCLICK: // ListView Column Header clicked (# in iSubItem)
		pNMLV = (LPNM_LISTVIEW)pNMHdr;
		if (iSortColumn[iLVMode] == pNMLV->iSubItem)
			iSortOrder[iLVMode] = 1 - iSortOrder[iLVMode];
		else {
			iSortColumn[iLVMode] = pNMLV->iSubItem;
			iSortOrder[iLVMode] = 1;
		}
		SetListViewlParam();
		ListView_SortItems(m_hWndListView,compareproc,(LONG)(LPVOID)this);
		break;
	case TCN_SELCHANGE:
		i = TabCtrl_GetCurSel(m_hWndTabCtrl);
		tci.mask = TCIF_PARAM;
		TabCtrl_GetItem(m_hWndTabCtrl,i,&tci);
		PostMessage(hWnd,WM_COMMAND,(int)tci.lParam,0);
		break;
	case NM_SETFOCUS:
		i = SetMenus(hWnd,pNMHdr);
		break;
	case NM_DBLCLK:
		if (pNMHdr->idFrom == IDC_LISTVIEW)
			if (ListView_GetNextItem(m_hWndListView,-1,LVNI_SELECTED) != -1)
				PostMessage(hWnd,WM_COMMAND,IDM_VIEW,0);
		break;
	default:
		rc = CExplorerView::OnNotify(hWnd,wParam,pNMHdr);
	}
	return rc;
}

// set column headers of listview

// set this to 1 to avoid memory leaks in listview code
#define NODELETEALLCOLUMNS 0

BOOL CProcWinView::SetListViewCols(int fMode)
{
	int i,j;
	const LVCInit * pLVCInit;
	LV_COLUMN lvc;

	if (fMode == iLVMode)
		return 1;


	for (i = 0;i < 4;i++) {
		if (lvcm[i].iMode == fMode) {
#if NODELETEALLCOLUMNS
			while (ListView_DeleteColumn(m_hWndListView,lvcm[i].iCount));	// alte Spalten entfernen
#else
			while (ListView_DeleteColumn(m_hWndListView,0));
#endif
			for (pLVCInit = lvcm[i].pLVCInit, j = lvcm[i].iCount, i = 0;j;j--,i++,pLVCInit++) {
				lvc.mask = pLVCInit->mask;
				lvc.fmt = pLVCInit->fmt;
				lvc.cx = pLVCInit->cx;
				lvc.pszText = pLVCInit->pszText;
#if NODELETEALLCOLUMNS
				if (!ListView_SetColumn(m_hWndListView,i,&lvc))
#endif
					ListView_InsertColumn(m_hWndListView,i,&lvc);
			}
			break;
		}
	}
	iLVMode = fMode;

	SetTabs();

	return TRUE;
}

// compareprozedur

int CALLBACK compareproc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{

	CProcWinView * pProcWinView = (CProcWinView *)lParamSort;

	return pProcWinView->CompareListViewItems(lParam1,lParam2);	
}

// handle WM_COMMAND

LRESULT CProcWinView::OnCommand(HWND hWnd,WPARAM wparam,LPARAM lparam)
{
	LRESULT rc = 0;

	switch (LOWORD(wparam)) {
	case IDM_REFRESH:
		UpdateTreeView();
		break;
	case IDM_LOADLIBRARY:
		OnLoadLibrary(hWnd);
		break;
	case IDM_STARTAPP:
		OnStartApp(hWnd);
		break;
	case IDM_LVUPDATE:
		UpdateListView(0,-1);
		break;
	case IDM_KILL:
		if (IDYES == MessageBox(hWnd, pStrings[AREYOUSURE], pStrings[WARNING], MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2))
			KillProcess();
		break;
	case IDM_SETPRIORITY:
		OnSetPriority(hWnd);
		break;
	case IDM_THREADMSG:
		OnThreadMsg(hWnd);
		break;
#if DEBUGPROCESS
	case IDM_DEBUGPROCESS:
		DebugProcess();
		break;
#endif
	case IDM_MODULE:
		UpdateListView(0,LVMODE_MODULE);
		break;
	case IDM_HEAP:
		UpdateListView(0,LVMODE_HEAP);
		break;
	case IDM_PROCINF:
		UpdateListView(0,LVMODE_PROCINF);
		break;
	case IDM_WINDOWS:
		UpdateListView(0,LVMODE_WINDOWS);
		break;
	case IDM_SAVEAS:
		OnSaveAs(hWnd);
		break;
	case IDM_COPY:
		OnCopy(hWnd);
		break;
	case IDM_VIEW:
		ShowModule(hWnd, MODVIEW_IMAGE);
		break;
	case IDM_PEHEADER:
		ShowModule(hWnd, MODVIEW_HEADER);
		break;
	case IDM_DISASM:
		ShowModule(hWnd, MODVIEW_CODE);
		break;
	case IDM_DATA:
		ShowModule(hWnd, MODVIEW_DATA);
		break;
	case IDM_EXPORT:
		ShowModule(hWnd, MODVIEW_EXPORT);
		break;
	case IDM_IMPORT:
		ShowModule(hWnd, MODVIEW_IMPORT);
		break;
	case IDM_RES:
		ShowModule(hWnd, MODVIEW_RES);
		break;
	case IDM_IAT:
		ShowModule(hWnd, MODVIEW_IAT);
		break;
	case IDM_SECTION:
		ShowModule(hWnd, MODVIEW_SECTION);
		break;
	case IDM_EXPLORE:
		OnExplore(hWnd);
		break;
	case IDM_PROPERTIES:
		ShowProperties(hWnd);
		break;
	case IDM_SBUPDATE:
		UpdateStatusBarInfo();
		break;
	case IDM_CLOSE:
		DoCloseWindow();
		break;
	case IDM_ACTIVATE:
		DoActivateWindow(SW_RESTORE);
		break;
	case IDM_HIDE:
		DoActivateWindow(SW_HIDE);
		break;
	case IDM_ENABLE:
		DoEnableWindow();
		break;
	case IDM_FLASH:
		OnFlashWindow();
		break;
	case IDM_PSAPI:
		m_bUsePSAPI = SetPSAPI(m_hWndMain, 1 - m_bUsePSAPI);
		break;
	case IDM_SELECTALL:
		ListView_SetItemState(m_hWndListView, -1, LVIS_SELECTED, LVIS_SELECTED);
		break;
	case IDM_INCLFREE:
		SetFreeBlocksOption(1 - m_bInclFreeBlocks);
	case IDOK:
		if (GetFocus() == m_hWndListView)
			if (ListView_GetNextItem(m_hWndListView,-1,LVNI_SELECTED) != -1) {
				PostMessage(hWnd,WM_COMMAND,IDM_VIEW,0);
				break;
			}		
		MessageBeep(MB_OK);
		break;
#if OPTIONDLG
	case IDM_OPTIONS:
		DialogBox(pApp->hInstance,
			MAKEINTRESOURCE(IDD_OPTIONS),
			hWnd,
			optionsproc);
		break;
#endif
	default:
		rc = CExplorerView::OnCommand(hWnd,wparam,lparam);
	}
	return rc;
}

// handle WM_ messages

LRESULT CProcWinView::OnMessage(HWND hWnd,UINT message,WPARAM wparam,LPARAM lparam)
{
	LRESULT rc = 0;

	switch (message) {
	case WM_COMMAND:
		rc = OnCommand(hWnd,wparam,lparam);
		break;
	case WM_NOTIFY:
		rc = OnNotify(hWnd,wparam,(LPNMHDR)lparam);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		rc = CExplorerView::OnMessage(hWnd,message,wparam,lparam);
	}
	return rc;
}

// WindowProc of main window

LRESULT CALLBACK wndproc(HWND hWnd,UINT message,WPARAM wparam,LPARAM lparam)
{
	return pApp->pProcWinView->OnMessage(hWnd,message,wparam,lparam);
}
