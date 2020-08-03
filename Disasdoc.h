
#define DADF_WIN16 0
#define DADF_WIN32 3

class CDisAsmDoc {
private:
	unsigned char * pBase;
	UINT iUnAssLines;
	UINT m_dwCodeOffs;	// current offset for disassembler
	UINT m_dwBaseOfCode;	// is PE.OptionalHeader.BaseOfCode
	UINT m_dwFlags;
	unsigned char * * pDisAssArray; //hQueue
public:
	CDisAsmDoc(void);
	~CDisAsmDoc(void);
	int UpdateDocument(unsigned char * pMem, UINT dwBaseOfCode, UINT dwSizeOfCode, UINT dwFlags);
	int GetActualLine(void);
	UINT GetLines(void);
	unsigned char * GetAddressFromLine(UINT uLine);
	BOOL SetAddressOfLine(UINT uLine, unsigned char *);
	int GetCodeBytes(unsigned char *, char *);
	int GetSource(unsigned char *, char *, UINT);
	int SetCodeOffset(UINT);
};

