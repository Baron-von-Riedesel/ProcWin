
#ifdef __cplusplus
extern "C" {
#endif

HANDLE WINAPI CreateQueue(int iLength,int iItems);
int    WINAPI DestroyQueue(HANDLE);
int    WINAPI WriteQueue(HANDLE handle,void * pItem);
int    WINAPI ReadQueue(HANDLE handle,void * pItem);
int    WINAPI ReadQueueItem(HANDLE handle,int iItem, void * pItem);
int    WINAPI ReadQueueAllItems(HANDLE handle,unsigned char * pArray);

#ifdef __cplusplus
}
#endif

