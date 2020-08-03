
#define IDC_TREEVIEW  0x800
#define IDC_LISTVIEW  0x801
#define IDC_STATUSBAR 0x802
#define IDC_TOOLBAR   0x803
#define IDC_TABCTRL	  0x804



class CExplorerView {
public:
	static int iCount;
	HWND m_hWndMain;        /* handle main window */
	HWND m_hWndTreeView;    /* handle treeview child */
	HWND m_hWndListView;    /* handle listview child */
	HWND m_hWndToolbar;     /* handle toolbar child */
	HWND m_hWndStatusBar;   /* handle statusbar child */
	HWND m_hWndTabCtrl;     /* handle tab control child */
	HWND m_hWndFocus;
    HACCEL hAccel;
	int iToolbarHeight;
	int iOldResizeLine;
	HBRUSH hHalftoneBrush;
	HMENU hContextMenu;
	UINT dwTVStyle;
	UINT dwLVStyle;
	HCURSOR hCursor;
	BOOL fMenuLoop;

	CExplorerView();
	~CExplorerView();
	int ResizeClients(HWND hWnd);
	int AdjustTreeAndList(HWND hWnd, int);
	void DrawResizeLine(HWND, int);
	LRESULT OnMessage(HWND hWnd,UINT message,WPARAM wparam,LPARAM lparam);
	void DoPaint(HWND hWnd);
	LRESULT OnCommand(HWND hWnd,WPARAM wparam,LPARAM lparam);
	LRESULT OnNotify(HWND hWnd,WPARAM wparam,LPNMHDR);
	int DisplayContextMenu(HWND hWnd,LPNMHDR);
	int CreateChilds(HWND hWndParent, UINT dwTVStyle,UINT dwLVStyle, UINT dwLVExStyle, UINT dwSBStyle);
//	virtual int UpdateTreeView(void) = 0;
};

