

class CTabWindowView {
public:
	static RECT rLastRect;
	HWND m_hWndTabCtrl;
	HWND m_hWndStatusBar;
	HWND m_hWndListView;

	int ResizeClients(HWND hWnd);
	LRESULT OnMessage(HWND hWnd,UINT message,WPARAM wparam,LPARAM lparam);
};

