
#define STRICT

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <stdio.h>
#include "tabwnd.h"
#include "rsrc.h"
#include "procwin.h"
#include "CApplication.h"

extern Application * pApp;

RECT CTabWindowView::rLastRect = {0,0,0,0};

// on WM_SIZE adjust child windows

int CTabWindowView::ResizeClients(HWND hWnd)
{
	RECT rect;
	RECT rectSB;
	HDWP hdwp;
	int aWidths[3];

	GetClientRect(hWnd,&rect);

	GetWindowRect(m_hWndStatusBar,&rectSB);
	rectSB.bottom = rectSB.bottom - rectSB.top;

	aWidths[0] = rect.right / 5 * 1;
	aWidths[1] = rect.right / 5 * 4;
	aWidths[2] = -1;
	SendMessage (m_hWndStatusBar, SB_SETPARTS, 3, (LPARAM)aWidths);

	hdwp = BeginDeferWindowPos(3);

// set the status bar
	
	DeferWindowPos( hdwp, m_hWndStatusBar, NULL,
			0, rect.bottom - rectSB.bottom, rect.right, rectSB.bottom,
			SWP_NOZORDER | SWP_NOACTIVATE);

// set the tab control

	rect.bottom = rect.bottom - rectSB.bottom;

	DeferWindowPos( hdwp, m_hWndTabCtrl, NULL,
			rect.left, rect.top, rect.right, rect.bottom,
			SWP_NOZORDER | SWP_NOACTIVATE);

//	GetClientRect(hWndTabCtrl,&rect);
	TabCtrl_AdjustRect(m_hWndTabCtrl, FALSE, &rect); 

// set the list view control (set HWND_TOP and not SMW_NOZORDER!!!)	

	DeferWindowPos( hdwp, m_hWndListView, HWND_TOP,
			rect.left, rect.top, rect.right - rect.left,rect.bottom - rect.top,
			SWP_NOACTIVATE);

	return EndDeferWindowPos(hdwp);
}

// handle WM_ messages

LRESULT CTabWindowView::OnMessage(HWND hWnd,UINT message,WPARAM wparam,LPARAM lparam)
{
	char szStr[80];
	LRESULT rc = 0;
//  POINT pt;

	switch (message) {
	case WM_SIZE:
		if (wparam != SIZE_MINIMIZED) {
			ResizeClients(hWnd);
			if (wparam != SIZE_MAXIMIZED)
				GetWindowRect(hWnd,&rLastRect);
		}
		break;
	case WM_MOVE:
		if ((!IsIconic(hWnd)) && (!IsZoomed(hWnd)))
			GetWindowRect(hWnd,&rLastRect);
		break;
#if 0
	case WM_ENTERMENULOOP:
		SendMessage(m_hWndStatusBar,SB_SIMPLE,1,0);	// simple mode einschalten		
		break;
	case WM_EXITMENULOOP:
		SendMessage(m_hWndStatusBar,SB_SIMPLE,0,0);	// simple mode ausschalten	
		break;
#endif
	case WM_MENUSELECT:
		if (LOWORD(wparam) && !(HIWORD(wparam) & MF_SYSMENU)) {
			szStr[0] = 0;
			LoadString(pApp->hInstance,LOWORD(wparam),szStr,sizeof(szStr));
			SendMessage(m_hWndStatusBar,SB_SETTEXT,255,(LPARAM)szStr);		
		}
		break;
#if 0
	case WM_PAINT:
		DoPaint(hWnd);
		break;
	case WM_SETFOCUS:
		if (hWndFocus)
			SetFocus(hWndFocus);
		break;
#endif
	default:
		rc = DefWindowProc(hWnd,message,wparam,lparam);
	}
	return rc;
}
