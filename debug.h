
#ifdef _DEBUG
    extern void DbgMsg( const char *format, ... );
    #define dbg_printf( x ) DbgMsg x
#else
    #define dbg_printf( x )
#endif

