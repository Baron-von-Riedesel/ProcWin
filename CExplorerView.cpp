
#define STRICT

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>
#include "CApplication.h"
#include "CExplorerView.h"
#include "rsrc.h"
#include "strings.h"
#include "richedit.h"
#include "debug.h"

#define USEINSTREAM 1

#if (_RICHEDIT_VER >= 0x0200 )
#define RICHEDIT_DLL "riched20.dll"
#else
#define RICHEDIT_DLL "riched32.dll"
#endif

extern Application * pApp;

int g_dwCnt;	// counter for rich edit streamin callback

int CExplorerView::iCount = 0;

CExplorerView::CExplorerView()
{
	iCount++;
	hHalftoneBrush = 0;
	m_hWndMain = 0;
	m_hWndFocus = 0;
	hContextMenu = 0;
	hCursor = 0;
	fMenuLoop = FALSE;
}

CExplorerView::~CExplorerView()
{
	if (hHalftoneBrush)
		DeleteObject(hHalftoneBrush);
	iCount--;
}

int CExplorerView::CreateChilds(HWND hWndMain, UINT dwTVStyle, UINT dwLVStyle, UINT dwLVExStyle, UINT dwSBStyle)
{
	HINSTANCE hInstance = GetWindowInstance(hWndMain);

	m_hWndTreeView = CreateWindowEx( WS_EX_CLIENTEDGE, WC_TREEVIEW,
			NULL,
			WS_CHILD | dwTVStyle,
			0,0,0,0,
			hWndMain,
			(HMENU)IDC_TREEVIEW,
			hInstance, NULL);

	m_hWndListView = CreateWindowEx( WS_EX_CLIENTEDGE, WC_LISTVIEW,
			NULL,
			WS_CHILD | dwLVStyle,
			0,0,0,0,
			hWndMain,
			(HMENU)IDC_LISTVIEW,
			hInstance, NULL);

	if (m_hWndListView)
		ListView_SetExtendedListViewStyle(m_hWndListView,dwLVExStyle);

	m_hWndStatusBar = CreateWindowEx ( 0,
			STATUSCLASSNAME,
			NULL,
			WS_CHILD | dwSBStyle,
			0,0,0,0,
			hWndMain,
			(HMENU)IDC_STATUSBAR,
			hInstance,
			NULL);
	return 1;
}

// user resizes TreeView/ListView. This is the only function with GDI stuff

void CExplorerView::DrawResizeLine(HWND hwnd, int xPos)
{
        
	HBRUSH hBrushOld;
	HDC hdc; 
	RECT rc;
	
	if (!hHalftoneBrush) {
		int i;
		HBITMAP hBitmap;
		WORD pattern[8];

		for (i=0;i<8;i++) 
			pattern[i] = 0x5555 << (i & 1);
										// Create a pattern halftone brush
		hBitmap = CreateBitmap(8,8,1,1,pattern);
		hHalftoneBrush = CreatePatternBrush(hBitmap);
		DeleteObject(hBitmap);				// bitmap no longer needed
	}
	GetClientRect(m_hWndTreeView, &rc);

	hdc = GetDC(hwnd);
	hBrushOld = (HBRUSH)SelectObject(hdc, hHalftoneBrush); 
	PatBlt(hdc,xPos,iToolbarHeight + 2, 4, rc.bottom, PATINVERT);

	SelectObject(hdc, hBrushOld);		// clean up 
	ReleaseDC(hwnd, hdc);

	iOldResizeLine = xPos;
}

// on WM_SIZE adjust client windows

int CExplorerView::ResizeClients(HWND hWnd)
{
	RECT rect;
	RECT rectTV;
	RECT rectSB;
	RECT rectTB;
	HDWP hdwp;

	GetClientRect(hWnd,&rect);
	
	GetWindowRect(m_hWndToolbar,&rectTB);
	rectTB.bottom = rectTB.bottom - rectTB.top;
	
	GetWindowRect(m_hWndStatusBar,&rectSB);
	rectSB.bottom = rectSB.bottom - rectSB.top;

	GetWindowRect(m_hWndTreeView,&rectTV);
	rectTV.right = rectTV.right - rectTV.left;

	if (rectTV.right == 0) { 
		int aWidths[2];
		rectTV.right = MulDiv(rect.right,1,3) - 2;
		aWidths[0] = rect.right / 3;
		aWidths[1] = -1;
		SendMessage (m_hWndStatusBar, SB_SETPARTS, 2, (LPARAM)aWidths);
	}

	iToolbarHeight = rectTB.bottom;

	hdwp = BeginDeferWindowPos(5);

// set the status bar
	
	DeferWindowPos(hdwp, m_hWndStatusBar, NULL,
				0,rect.bottom - rectSB.bottom,
				rect.right,rectSB.bottom,
				SWP_NOZORDER | SWP_NOACTIVATE);

// set the toolbar control	
	
	DeferWindowPos(hdwp, m_hWndToolbar, NULL,
				0,0,rect.right,rectTB.bottom,
				SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

// set the tree view control	
	
	DeferWindowPos(hdwp, m_hWndTreeView, NULL,
				0, rectTB.bottom,	rectTV.right,
				rect.bottom - rectSB.bottom - rectTB.bottom,
				SWP_NOZORDER | SWP_NOACTIVATE);

// set the tab control	

	rect.left = rectTV.right + 4;
	rect.top = rectTB.bottom;
	rect.right = rect.right - rectTV.right - 4;
	rect.bottom = rect.bottom - rectSB.bottom - rectTB.bottom;

	DeferWindowPos(hdwp, m_hWndTabCtrl, NULL, 
				rect.left,
				rect.top,
				rect.right,
				rect.bottom,
				SWP_NOZORDER | SWP_NOACTIVATE);


// set the list view control	

	TabCtrl_AdjustRect(m_hWndTabCtrl, FALSE, &rect); 

	DeferWindowPos(hdwp, m_hWndListView, HWND_TOP, 
				rect.left, rect.top,
				rect.right - rect.left + rectTV.right +  4,
				rect.bottom - rect.top + rectTB.bottom,
				SWP_NOACTIVATE);

	EndDeferWindowPos(hdwp);
	return 1;
}

// adjust treeview & listview only

int CExplorerView::AdjustTreeAndList(HWND hWnd,int iWidth)
{
	RECT rect;
	RECT rectTV;
	HDWP hdwp;

	GetClientRect(hWnd,&rect);
	if (iWidth > 0 && iWidth < rect.right) {
		GetWindowRect(m_hWndTreeView,&rectTV);
		rectTV.right = iWidth - rect.left;
		hdwp = BeginDeferWindowPos(3);
		DeferWindowPos(hdwp, m_hWndTreeView, NULL,
				0,0,
				rectTV.right,
				rectTV.bottom - rectTV.top,
				SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);

		rect.left = rectTV.right + 4;
		rect.top = iToolbarHeight;
		rect.right = rect.right - rectTV.right - 4;
		rect.bottom = rectTV.bottom - rectTV.top;

		DeferWindowPos(hdwp, m_hWndTabCtrl, NULL, 
					rect.left,
					rect.top,
					rect.right,
					rect.bottom,
					SWP_NOZORDER | SWP_NOACTIVATE);
		
		TabCtrl_AdjustRect(m_hWndTabCtrl, FALSE, &rect); 

		DeferWindowPos(hdwp, m_hWndListView, HWND_TOP,
				rect.left, rect.top,
				rect.right - rect.left + rectTV.right + 4,
				rect.bottom - rect.top + iToolbarHeight,
				SWP_NOACTIVATE);
		EndDeferWindowPos(hdwp);
	}
	return 1;
}

// WM_PAINT main window
// the only painting required from the main window is the splitter bar

void CExplorerView::DoPaint(HWND hWnd)
{
	PAINTSTRUCT ps;
	HDC hDC;
	RECT rect;
	HBRUSH hBrush;

	hDC = BeginPaint(hWnd,&ps);
	hBrush = CreateSolidBrush(GetSysColor(COLOR_ACTIVEBORDER));
	GetWindowRect(m_hWndTreeView,&rect);
	rect.left = rect.right - rect.left;
	rect.bottom = rect.bottom - rect.top + iToolbarHeight; 
	rect.right = rect.left + 4;
	rect.top = iToolbarHeight;
	FillRect(hDC,&rect,hBrush);
	DeleteObject(hBrush);
	EndPaint(hWnd,&ps);

	return;
}

#if USEINSTREAM

DWORD CALLBACK editstreamcb(DWORD dwCookie, LPBYTE pbBuff, LONG cb, LONG FAR *pcb)
{
	int i;

	i = lstrlen(pStrings[ABOUTTEXT] + g_dwCnt);
	if (i > cb) {
		*pcb = cb;
		memcpy(pbBuff,pStrings[ABOUTTEXT] + g_dwCnt,cb);
		g_dwCnt = g_dwCnt + cb;
		return 0;
	} else {
		*pcb = i;
		memcpy(pbBuff,pStrings[ABOUTTEXT] + g_dwCnt,i);
		return 0;
	}
}

#endif

// the about dialog displays a static dialog box

BOOL CALLBACK aboutproc(HWND hWnd,UINT message,WPARAM wparam,LPARAM lparam)
{
	BOOL rc = FALSE;
	HWND hRE;
	EDITSTREAM editstream;

	switch (message) {
	case WM_INITDIALOG:
#if USEINSTREAM
		hRE = GetDlgItem(hWnd,IDC_ABOUT);
		editstream.dwCookie = 1;
		editstream.dwError = 0;
		editstream.pfnCallback = editstreamcb;
		g_dwCnt = 0;
		SendMessage(hRE, EM_STREAMIN, SF_RTF, (LPARAM)&editstream);
#else
		SetDlgItemText(hWnd,IDC_ABOUT,pStrings[ABOUTTEXT]);
#endif
		SetFocus(GetDlgItem(hWnd,IDC_ABOUT));
		break;
	case WM_CLOSE:
		EndDialog(hWnd,0);
		break;
	case WM_COMMAND:
		switch (LOWORD(wparam)) {
		case IDOK:
		case IDCANCEL:
			SendMessage(hWnd,WM_CLOSE,0,0);
		}
		break;
	}
	return rc;
}

// display context menu
// for treeview and listview panel

int CExplorerView::DisplayContextMenu(HWND hWnd, LPNMHDR pNMHdr)
{
	RECT rect;
	POINT pt;
	int i, idCmd;
	HTREEITEM hTreeItem;
	TV_HITTESTINFO tvht;


	if (pNMHdr->code == NM_RCLICK)
		GetCursorPos(&pt);

	dbg_printf(( "CExplorerView::DisplayContextMenu(): id=%X\r\n", pNMHdr->idFrom ));

	switch ( pNMHdr->idFrom ) {
	case IDC_TREEVIEW: /* for treeview panel */
		GetCursorPos(&tvht.pt);
		ScreenToClient(m_hWndTreeView, &tvht.pt);
		TreeView_HitTest( m_hWndTreeView, &tvht );
		if (tvht.hItem)
			hTreeItem = tvht.hItem;
		else
			hTreeItem = TreeView_GetSelection(m_hWndTreeView);
		if (pNMHdr->code != NM_RCLICK) {
			GetWindowRect(m_hWndTreeView,&rect);
			pt.x = rect.left;
			pt.y = rect.top;
			TreeView_GetItemRect(m_hWndTreeView, hTreeItem,&rect,TRUE);
			pt.x = pt.x + rect.left + (rect.bottom - rect.top);
			pt.y = pt.y + rect.bottom;
		}
		idCmd = TrackPopupMenu(hContextMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD,
							   pt.x, pt.y, 0,  hWnd, NULL);
		if (idCmd) {
			TreeView_Select( m_hWndTreeView, hTreeItem, TVGN_CARET);
			SendMessage( hWnd, WM_COMMAND, idCmd, 0);
		}
		break;
	case IDC_LISTVIEW: /* for listview panel */
		if ((i = ListView_GetNextItem(m_hWndListView,-1,LVNI_SELECTED)) == -1)
			break;
		if (pNMHdr->code != NM_RCLICK) {
			GetWindowRect(m_hWndListView,&rect);
			pt.x = rect.left;
			pt.y = rect.top;
			ListView_GetItemRect(m_hWndListView,i,&rect,LVIR_BOUNDS);
			pt.x = pt.x + rect.left + (rect.bottom - rect.top);
			pt.y = pt.y + rect.bottom;
		}
		TrackPopupMenu(hContextMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON,
					   pt.x, pt.y, 0, hWnd, NULL);
		break;
	default: /* other windows */
		/* if it's the listview's header control, open a menu that contains
		 * all columns and allow the user to remove/readd the column.
		 */
		if ( ListView_GetHeader( m_hWndListView ) == pNMHdr->hwndFrom ) {
			HMENU hMenu = CreatePopupMenu();
			HD_ITEM hdi;
			char text[80];
			GetCursorPos( &pt );
			for ( i = 0; ; i++ ) {
				hdi.mask = HDI_TEXT | HDI_WIDTH;
				hdi.cchTextMax = sizeof( text );
				hdi.pszText = text;
				if ( !Header_GetItem( pNMHdr->hwndFrom, i, &hdi ) )
					break;
				AppendMenu( hMenu, MF_STRING | ( hdi.cxy ? MF_CHECKED : 0 ), i + 1, hdi.pszText );
			}
			if ( i = TrackPopupMenu( hMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD,
									pt.x, pt.y, 0, hWnd, NULL) ) {
				i--;
				/* the column is not physically removed, just the width is set to 0.
				 */
				hdi.mask = HDI_WIDTH | HDI_TEXT | HDI_LPARAM;
				hdi.cchTextMax = sizeof( text );
				hdi.pszText = text;
				Header_GetItem( pNMHdr->hwndFrom, i, &hdi );
				hdi.mask = HDI_WIDTH;
				if ( hdi.cxy ) {
					hdi.lParam = hdi.cxy;
					hdi.mask |= HDI_LPARAM;
					hdi.cxy = 0;
				} else {
					hdi.cxy = ( hdi.lParam ? hdi.lParam : strlen( hdi.pszText ) * 9 );
				}
				Header_SetItem( pNMHdr->hwndFrom, i, &hdi );
			};
			DestroyMenu( hMenu );
			dbg_printf(( "CExplorerView::DisplayContextMenu(): header context menu\r\n" ));
			break;
		}
		dbg_printf(( "CExplorerView::DisplayContextMenu(): id=%X not handled\r\n", pNMHdr->idFrom ));
		break;
	}
	return 1;
}

// handle all WM_NOTIFY messages

LRESULT CExplorerView::OnNotify(HWND hWnd,WPARAM wparam,LPNMHDR pNMHdr)
{
	LPTOOLTIPTEXT pTTT;
	LRESULT rc = 0;

	switch (pNMHdr->code) {
	case TVN_KEYDOWN:
		if (((TV_KEYDOWN *)pNMHdr)->wVKey == VK_APPS)
			DisplayContextMenu(hWnd, pNMHdr);

		break;
	case LVN_KEYDOWN:
		if (((LV_KEYDOWN *)pNMHdr)->wVKey == VK_APPS)
			DisplayContextMenu(hWnd, pNMHdr);

		break;
	case TTN_NEEDTEXT:	// we need hInst and resource id (in lpszText)
						// help string ID is command ID!
		pTTT = (LPTOOLTIPTEXT)pNMHdr;
		pTTT->hinst = GetWindowInstance(hWnd);
		pTTT->lpszText = (LPSTR)pTTT->hdr.idFrom;
		break;
	case NM_RCLICK:			// right mouse button -> context menues
		DisplayContextMenu(hWnd, pNMHdr);
		break;
	case NM_KILLFOCUS:
		m_hWndFocus = pNMHdr->hwndFrom;
		break;
	}
	return rc;
}

// handle WM_COMMAND messages

LRESULT CExplorerView::OnCommand(HWND hWnd,WPARAM wparam,LPARAM lparam)
{
	static HMODULE hLibRichEdit = 0;
	LRESULT rc = 0;

	switch (LOWORD(wparam)) {
	case IDM_ABOUT:
		if (hLibRichEdit == 0)
			hLibRichEdit = LoadLibrary(RICHEDIT_DLL);
		DialogBox(GetWindowInstance(hWnd),
			MAKEINTRESOURCE(IDD_ABOUT),
			hWnd,
			aboutproc);
		break;
	case IDM_EXIT:
		DestroyWindow(hWnd);
		break;
	}
	return rc;
}

// handle WM_xxx messages except WM_COMMAND and WM_NOTIFY

LRESULT CExplorerView::OnMessage(HWND hWnd,UINT message,WPARAM wparam,LPARAM lparam)
{
	char szStr[80];
	LRESULT rc = 0;
	POINT pt;

	switch (message) {
	case WM_SIZE:
		if (wparam != SIZE_MINIMIZED)
			ResizeClients(hWnd);
		break;
	case WM_LBUTTONDOWN:
		pt.x = LOWORD(lparam);
		pt.y = HIWORD(lparam);
		if (ChildWindowFromPoint(hWnd,pt) == hWnd) {
			RECT rect;
			RECT rect2;
			SetCapture(hWnd);
			GetWindowRect(m_hWndTreeView, &rect);
			GetWindowRect(m_hWndListView, &rect2);
			UnionRect(&rect, &rect, &rect2);
			ClipCursor(&rect);
			iOldResizeLine = -1;
			DrawResizeLine(hWnd,LOWORD(lparam));
		}
		break;
	case WM_LBUTTONUP:
		if (GetCapture() == hWnd) {
			ClipCursor(NULL);
			ReleaseCapture();
			DrawResizeLine(hWnd,iOldResizeLine);
			AdjustTreeAndList(hWnd,(int)LOWORD(lparam));
		}
		break;
	case WM_MOUSEMOVE:
		if (GetCapture() == hWnd) { 	// nur beachten wenn cursor "gecaptured" ist
			DrawResizeLine(hWnd,iOldResizeLine);
			DrawResizeLine(hWnd,LOWORD(lparam));
		}
		break;
	case WM_SETCURSOR:
		if ((hCursor) && (LOWORD(lparam) == HTCLIENT)) {
			DWORD dw;
			dw = GetMessagePos();
			pt.x = LOWORD(dw);
			pt.y = HIWORD(dw);
			ScreenToClient(hWnd,&pt);
			if (hWnd == ChildWindowFromPoint(hWnd,pt)) {
				SetCursor(hCursor);
				rc = 1;                                 
				break;
			}
		}
		rc = DefWindowProc(hWnd,message,wparam,lparam);
		break;
	case WM_ENTERMENULOOP:
		SendMessage(m_hWndStatusBar,SB_SIMPLE,1,0);	// simple mode einschalten
		fMenuLoop = TRUE;
		break;
	case WM_EXITMENULOOP:
		SendMessage(m_hWndStatusBar,SB_SIMPLE,0,0);	// simple mode ausschalten	
		fMenuLoop = FALSE;
		break;
	case WM_MENUSELECT:
		if (fMenuLoop) {
			szStr[0] = 0;
			LoadString(GetWindowInstance(hWnd),LOWORD(wparam),szStr,sizeof(szStr));
			SendMessage(m_hWndStatusBar,SB_SETTEXT,255,(LPARAM)szStr);		
		}
		break;
	case WM_PAINT:
		DoPaint(hWnd);
		break;
	case WM_ACTIVATE:
		if (LOWORD(wparam) != WA_INACTIVE) {
			pApp->hWndDlg = hWnd;
			pApp->hAccel = hAccel;
		}
		break;
	case WM_SETFOCUS:
		if (m_hWndFocus)
			SetFocus(m_hWndFocus);
		break;
	default:
		rc = DefWindowProc(hWnd,message,wparam,lparam);
	}
	return rc;
}
