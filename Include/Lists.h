
#ifdef __cplusplus
extern "C" {
#endif

typedef struct tSLIST {
void * pFirst;
} SLIST;

typedef struct tSLITEM {
void * pNext;
} SLITEM;


HANDLE WINAPI CreateList(void);
int    WINAPI DestroyList(HANDLE);
int    WINAPI AddListItem(HANDLE handle,void * pItem,int laenge);
int    WINAPI AddListString(HANDLE handle,char * pItem);
int    WINAPI DeleteListItem(HANDLE handle,void *);
int    WINAPI DeleteListItemByIndex(HANDLE handle, int iIndex);
void * WINAPI GetNextListItem(HANDLE handle,int iIndex,void * pItem,int laenge);
char * WINAPI GetNextListString(HANDLE handle,int iIndex);
void * WINAPI GetPrevListItem(HANDLE handle,int iIndex, void * pItem,int laenge);
int    WINAPI GetListItemCount(HANDLE);

#ifdef __cplusplus
}
#endif

