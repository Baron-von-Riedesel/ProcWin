
class CProcWinView;


class Application {
public:
	HINSTANCE hInstance;
	HACCEL hAccel;
	HWND hWndDlg;		// active modeless Dialog
	CProcWinView * pProcWinView;

	Application(HINSTANCE);
	~Application(void);
	HWND InitApp(int);
	int MessageLoop(void);
};
