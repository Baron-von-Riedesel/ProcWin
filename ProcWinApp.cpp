
#define STRICT

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <tlhelp32.h>

#include <stdio.h>

#include "CApplication.h"
#include "procwin.h"
#include "rsrc.h"
#include "strings.h"
#include "debug.h"

Application * pApp;

Application::Application(HINSTANCE hInst)
{
	hInstance = hInst;
	hAccel = 0;
	hWndDlg = NULL;
	pProcWinView = NULL;
	return;
}

Application::~Application()
{
	if (pProcWinView)
		delete pProcWinView;
}

// set privilege of this process

static void EnablePrivilege( char *szValue )
{
	HANDLE hMyProcess;
	HANDLE handle;
	TOKEN_PRIVILEGES tp;

	dbg_printf(( "EnablePrivilege(%s) enter\r\n", szValue ));
	hMyProcess = GetCurrentProcess();
	if ( OpenProcessToken( hMyProcess, TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &handle ) ) {
		if ( LookupPrivilegeValue( NULL, szValue, (PLUID)&tp.Privileges[0] ) ) {
			dbg_printf(( "EnablePrivilege(%s): old Attributes=%X\r\n", szValue, tp.Privileges[0].Attributes ));
			tp.PrivilegeCount = 1;
			tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
			AdjustTokenPrivileges( handle, FALSE, &tp, 0, NULL, 0 );
		}
		CloseHandle(handle);
	}
	return;
}

// InitApp

HWND Application::InitApp(int nShowCmd)
{
	INITCOMMONCONTROLSEX ics;
	HRSRC hRes;

	ics.dwSize = sizeof(ics);
	ics.dwICC = ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES | ICC_TAB_CLASSES | ICC_BAR_CLASSES;
	InitCommonControlsEx(&ics);

	// enable privilege to open all processes

	if ((GetVersion() & 0xff) >= 5) {
		EnablePrivilege( SE_DEBUG_NAME );
		EnablePrivilege( SE_SECURITY_NAME );
	}

	if (hRes = FindResource(hInstance,MAKEINTRESOURCE(IDR_ABOUTTEXT),RT_RCDATA)) {
		pStrings[ABOUTTEXT] = (char *)LoadResource(hInstance,hRes);
	} else
		pStrings[ABOUTTEXT] = "???";


	if (pProcWinView = new CProcWinView(hInstance, APPNAME)) {
		if (pProcWinView->m_hWndMain)
			ShowWindow(pProcWinView->m_hWndMain, SW_SHOWNORMAL);
		else
			MessageBox(0,"Cannot create main window",0,MB_OK);

		return pProcWinView->m_hWndMain;
	}
	return 0;
}

int Application::MessageLoop()
{
	MSG msg;

	while (GetMessage(&msg,0,0,0)) {
		if (hWndDlg) {
			if (TranslateAccelerator(hWndDlg,hAccel,&msg))
				continue;
			if (IsDialogMessage(hWndDlg,&msg))
				continue;
		} else {
			if (TranslateAccelerator(pProcWinView->m_hWndMain,hAccel,&msg))
				continue;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 1;
}

// WinMain

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpszCmdLine, int nShowCmd)
{

	if (!(pApp = new Application(hInst)))
		return 1;

	if (!(pApp->InitApp(nShowCmd)))
		return 1;

	for (;*lpszCmdLine;lpszCmdLine++) {
		switch (*lpszCmdLine) {
		case '-':
		case '/':
			if (*(lpszCmdLine+1))
				lpszCmdLine++;
		case ' ':
			break;
		default:
			lpszCmdLine = lpszCmdLine + MyLoadLibrary(0, lpszCmdLine) - 1;
			break;
		}
	}
	OleInitialize(NULL);

	pApp->MessageLoop();

	OleUninitialize();

	delete pApp;

	return 0;
}

