
class IatDoc {
private:
	unsigned char * pBase;		// ptr to Module base in procwin addr space
	IMAGE_NT_HEADERS * pPE;		// ptr to PE struct in procwin addr space
	DWORD m_dwModBase;			// module base in client addr space
	UINT startiat;
	UINT sizeiat;
public:
	IatDoc(void);
	~IatDoc(void);
	int UpdateDocument(IMAGE_NT_HEADERS * pPE,unsigned char * pMem, DWORD dwModBase);
	int GetActualLine(void);
	UINT GetLines(void);
	UINT GetValue(UINT uLine,UINT uIndex, char * pOutput);
	UINT GetNameOfAddress(void * * dwAddress,char * pOutput);
};

