
class ImpDoc {
private:
	unsigned char * pBase;
	IMAGE_NT_HEADERS * pPE;
	UINT uLines;
public:
	ImpDoc(void);
	~ImpDoc(void);
	int UpdateDocument(IMAGE_NT_HEADERS * pPE,unsigned char * pMem);
	int GetActualLine(void);
	UINT GetLines(void);
	UINT GetValue(UINT uLine,UINT uIndex, char * pOutput);
};

