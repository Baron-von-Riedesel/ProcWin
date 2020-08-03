
class CDisAsmDoc;
class ImpDoc;
class IatDoc;

class CColHdr {
public:
	int iSize;
	char * pText;
};

class CTab {
public:
	UINT uTopIndex;
	int iSelIndex;
};

typedef enum _ModViewType {
	MODVIEW_IMAGE	= 0,
	MODVIEW_HEADER	= 1,
	MODVIEW_CODE	= 2,
	MODVIEW_DATA	= 3,
	MODVIEW_EXPORT	= 4,
	MODVIEW_IMPORT	= 5,
	MODVIEW_RES		= 6,
	MODVIEW_IAT		= 7,
	MODVIEW_CODE16	= 8,
	MODVIEW_SECTION	= 9,
	MODVIEW_RELOC	= 10
} MODVIEWTYPE;

#define NUMMODES 11

class CModView: public CTabWindowView {
private:
	static BOOL fRegistered;
	static UINT dwFlagsOrg;
	HACCEL m_hAccel;
	HWND m_hWnd;
	int m_iLVMode;
	DWORD m_modBase;
	DWORD m_modSize;
	unsigned char * m_pMem;
	CDisAsmDoc * m_pDAD;
	CDisAsmDoc * m_pDAD16;
	ImpDoc * m_pImD;
	IatDoc * m_pIatDoc;
	CTab m_tab[NUMMODES];
	int m_iModeIndex;
	int m_iModeIndexOld;
	BOOL m_fCancel;
	unsigned char * m_pszFindString;
	int m_iFindStringLength;
	int m_iLastFindPos;
	BOOL m_bHexFormat;		// search string are hex nibbles
	int m_iSaveMode;
//	DWORD m_dwProcId;
	DWORD m_pid;
	IMAGE_NT_HEADERS * m_pPE;
	UINT m_dwFlags;			// see MVF_xxx below
	UINT m_dwOffset;

	DWORD m_dwEditAddr;	// mem addr der editierten Zeile
	UINT m_uNumItems;		// for line editor
//	UINT iLastExp;
	int SearchText(void);
	int SearchTextLV(int iColumn);
public:
	HWND hWndProgress;
	char * pStrTmp;		// pointer to strings (is temp for communication)
	CModView(char * pszAppName,int wStyle,int xPos, int yPos, HWND hWndParent, int fMode, UINT pid, UINT dwBase, UINT dwSize, BOOL bModule);
	~CModView();
	int SaveListViewContent(HWND hWnd, PSTR pszFileName);
	BOOL ProgressDlg(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	BOOL EditLineDlg(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	BOOL FindDlg(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	BOOL AdjustAddressDlg(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	BOOL AdjustAddress2Dlg(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT OnMessage(HWND hWnd,UINT message,WPARAM wparam,LPARAM lparam);
	BOOL ShowWindow(int);
private:
	int UpdateListView(int);
	BOOL GetListViewCols(int iMode);
	BOOL SetListViewCols(void);
	int ResizeClients(HWND hWnd);
	LPARAM OnNotify(HWND, WPARAM, LPNMHDR);
	LPARAM OnCommand(HWND, WPARAM, LPARAM);
	int OnSaveAs(HWND hWnd, int iMode);
	int SaveRawBytes(HWND hWnd, PSTR pszFileName);
	int DisplayContextMenu(HWND, int);
	int EditLine(HWND);
	int FillItem(HWND,int iItem, int iSubItem, char * pszText, int cchTextMax);
	void SetNewMode(HWND,int);
	void OnView(void);
};

#define MVF_FMASK	~3
#define MVF_DWORDS	1
#define MVF_BYTES	2