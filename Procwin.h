
#define CLASSNAME "ProcWinClass"
#define APPNAME   "ProcWin"

int CALLBACK compareproc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
LRESULT CALLBACK wndproc(HWND hWnd,UINT message,WPARAM wparam,LPARAM lparam);
LRESULT CALLBACK modwndproc(HWND hWnd,UINT message,WPARAM wparam,LPARAM lparam);
int MyLoadLibrary(HWND hWnd, LPSTR lpszName);

#define LVMODE_MODULE	0
#define LVMODE_WINDOWS	1
#define LVMODE_PROCINF	2
#define LVMODE_HEAP		3
#define LVMODE_MAX		3

#include "CExplorerView.h"

typedef struct tTHHeap {
	UINT dwHeapID;
	UINT dwAddress;
	HANDLE hHandle;
	UINT dwBlockSize;
	UINT dwFlags;
} THHeap;


class CProcWinView :public CExplorerView {
	int iLVMode;
	int iProcLVMode;	// save last iLVMode for switch Threads->Process
	int iSortColumn[LVMODE_MAX+1];
	int iSortOrder[LVMODE_MAX+1];
	BOOL fSelChanged;
	BOOL m_bUsePSAPI;
	BOOL m_bInclFreeBlocks;
	THHeap * pTHHOld;
//	HMENU hAltMenu;				// alternate menu for cintextsub menus
	IDropTarget * m_pDropTarget;// listview window is drag&drop target
public:
    HANDLE m_hKillProcess;      // handle of process to kill

	CProcWinView(HINSTANCE hInstance, char * pszAppName, UINT dwStyle=WS_OVERLAPPEDWINDOW, HWND hWndParent=NULL,HMENU hMenu=NULL);
	~CProcWinView();
	BOOL EnumThreadWindows(HWND hWnd);
	int CompareListViewItems(LPARAM lParam1, LPARAM lParam2);
	LRESULT OnMessage(HWND hWnd,UINT message,WPARAM wparam,LPARAM lparam);
private:
	BOOL SetFreeBlocksOption(BOOL);
	int UpdateTreeView(void);
	int UpdateListViewWindows(DWORD tid);
    int UpdateListViewModules(DWORD pid);
    void SetModuleLine(int, MODULEENTRY32 &);
	int UpdateListViewHeap(DWORD pid);
	int UpdateListViewProcInf(DWORD pid);
	int UpdateListView(DWORD pid, int fMode);
	BOOL SetListViewCols(int fMode);
	LRESULT OnNotify(HWND hWnd,WPARAM wparam,LPNMHDR);
	LRESULT OnCommand(HWND hWnd,WPARAM wparam,LPARAM lparam);
	void KillProcess(void);
	void DebugProcess(void);
//	int ShowProperties(HWND hWnd);
	int OnSaveAs(HWND hWnd);
	int OnCopy(HWND hWnd);
	int OnLoadLibrary(HWND hWnd);
	int OnStartApp(HWND hWnd);
	int OnSetPriority(HWND hWnd);
	int OnThreadMsg(HWND hWnd);
	int SetListViewlParam(void);
	int TranslateProtectFlag(UINT flags,char * szStr,int iMax);
	int CheckSubMenuItem(int, int);
	void EnableSubMenuItems(HMENU, int, int);
	int SaveListViewContent(char *, BOOL);
//	int PropDialogProc1(HWND hWnd,UINT message,WPARAM wparam,LPARAM lparam);
//	int PropDialogProc2(HWND hWnd,UINT message,WPARAM wparam,LPARAM lparam);
	int OptionsDlg(HWND hWnd,UINT message,WPARAM wparam,LPARAM lparam);
	int UpdateStatusBarInfo(void);
	int DoCloseWindow();
	int DoActivateWindow(int fMode);
	int DoEnableWindow();
	int OnFlashWindow();
//	int ViewPEDlg(HWND hWnd, UINT message,WPARAM wparam,LPARAM lparam);
	void SetListViewHeapLine2(LV_DISPINFO * pDI);
	int SetMenus(HWND, LPNMHDR);
	int ShowModule(HWND,int iMode);
	void SetTabs(void);
	void EnableEditMenu(HWND, int, int, int);
	void EnableViewMenu(HWND, int);
	void EnableExtraMenu(HWND, int);
	int OnExplore(HWND);
	int ShowProperties(HWND);
};


