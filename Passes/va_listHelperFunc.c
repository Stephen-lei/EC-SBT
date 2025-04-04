#include <stdio.h>
#include <stdarg.h>

typedef FILE ee_FILE;

// Macro to enable debug print statements only when DEBUG is defined
#ifdef DEBUG
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...) (void)0
#endif

//declare i32 @vprint_mcsema(i64, i64, i64, i64, i64, i64, double, double, double, double, double, double, double, double)
int vfprintf_mcsema(ee_FILE *fp,const char* str,...) {
    DEBUG_PRINT("进入了vfprintf_mcsema的runtime support\n");
    int rv;
    va_list args;
    va_start(args, str);
    rv = vfprintf(fp, str, args);
    va_end(args);
    DEBUG_PRINT("结束了vfprintf_mcsema的runtime support\n");
    return rv;
}


int vfprintf_inline_mcsema(ee_FILE *fp,const char* str,...) {
    DEBUG_PRINT("进入了vfprintf_inline_mcsema的runtime support\n");
    int rv;
    va_list args;
    va_start(args, str);
    rv = vfprintf(fp, str, args);
    va_end(args);
    DEBUG_PRINT("结束了vfprintf_inline_mcsema的runtime support\n");
    return rv;
}






int vprintf_mcsema(const char* str,...) {
    DEBUG_PRINT("进入了vprinf_mcsema的runtime support\n");
    int rv;
    va_list args;
    va_start(args, str);
    rv = vprintf(str, args);
    va_end(args);
    DEBUG_PRINT("结束了vprinf_mcsema的runtime support\n");
    return rv;
}



int vsprintf_mcsema(char* str,const char* format, ...) {
    DEBUG_PRINT("进入了vsprinf_mcsema的runtime support\n");
    int rv;
    va_list args;
    va_start(args, format);
    rv = vsprintf(str,format, args);
    va_end(args);
    DEBUG_PRINT("结束了vsprinf_mcsema的runtime support\n");
    return rv;
}


int vsnprintf_mcsema(char* str,size_t size,const char* format,...) {
    DEBUG_PRINT("进入了vsprinf_mcsema的runtime support\n");
    int rv;
    va_list args;
    va_start(args, format); 
    rv = vsnprintf(str,size,format, args);
    va_end(args);
    DEBUG_PRINT("结束了vsprinf_mcsema的runtime support\n");
    return rv;
}



int vfscanf_mcsema(ee_FILE *fp,const char* str,...) {
    DEBUG_PRINT("进入了vfscanf_mcsema的runtime support\n");
    int rv;
    va_list args;
    va_start(args, str);
    rv = vfscanf(fp, str, args);
    va_end(args);
    DEBUG_PRINT("结束了vfscanf_mcsema的runtime support\n");
    return rv;
}

int vsscanf_mcsema(char* str,const char* format, ...) {
    DEBUG_PRINT("进入了vsscanf_mcsema的runtime support\n");
    int rv;
    va_list args;
    va_start(args, format);
    rv = vsscanf(str,format, args);
    va_end(args);
    DEBUG_PRINT("结束了vsscanf_mcsema的runtime support\n");
    return rv;
}

