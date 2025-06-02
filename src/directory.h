#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <locale.h>
#include <nl_types.h>
#include <langinfo.h>
#include <string.h>


extern int isdirectory(const char *path);
extern long long getfilesize(char* filename);
extern struct tm *getmoddate(char* filename);
extern void gettablerow(char* filename, char* directoryPath, short isfile, char* resultstr);
extern void drawindexpage(char* directoryPath, char* resultstr);