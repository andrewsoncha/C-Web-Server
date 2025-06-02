#include "directory.h"
#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <locale.h>
#include <nl_types.h>
#include <langinfo.h>
#include <string.h>

int isdirectory(const char *path){
	struct stat statbuf;
	if(stat(path, &statbuf) !=0){
		return -1;
	}
	return S_ISDIR(statbuf.st_mode);
}

long long getfilesize(char* filename){
       struct stat buf;
	if(stat(filename, &buf) == -1){
	 return -1;
 	}
	return buf.st_size;
}

struct tm *getmoddate(char* filename){
	struct stat buf;	
	struct tm *tm;
	char datestring[256];
	if(stat(filename, &buf) == -1){
	 return NULL;
 	}
	return localtime(&buf.st_mtime);
}

void gettablerow(char* filename, char* directoryPath, short isfile, char* resultstr){
	char filepath[256];
	char type[10];
	char modifieddate[50];
	int filesize;
	struct tm *tm;
	memset(resultstr, 0, sizeof(resultstr));
	memset(filepath, 0, sizeof(filepath));
	strncpy(filepath, directoryPath, strlen(directoryPath));
	strcat(filepath, filename); 
	printf("filename: %s   filepath: %s  type: ", filename, filepath);
	   switch(isfile){
			case 1:
				printf("directory");
				printf("size: - \n");
				filesize = -1;
				strncpy(type, "directory", 10);
				break;
			case 0:
				printf("file  ");
				filesize = getfilesize(filepath);
				printf("size: %d \n",filesize);
				strncpy(type, "file", 10);
				break;
	   }

		tm = getmoddate(filepath);
	if(tm==NULL){
		return;
	}
	int year, month, day, hour, min;
	year = 1900 + tm->tm_year;
	month = tm->tm_mon+1;
	day = tm->tm_mday;
	hour = tm->tm_hour;
	min = tm->tm_min;
	printf("modified date: %d-%02d-%02d %02d:%02d  \n", year, month, day, hour, min);
	printf("\n");	  
	sprintf(modifieddate, "%d-%02d-%02d %02d:%02d", year, month, day, hour, min);

	int leadingnum=0;
	char sizestr[10];
	char unitstr[5];
	if(filesize==-1){
		strncpy(sizestr, "-", 10);
	}
	else{
		if(filesize<1000){ //b
			leadingnum = filesize;
			strncpy(unitstr, "b", 5);
		}
		else if(filesize<1000000){ //kb
			leadingnum = filesize/1000;
			strncpy(unitstr, "kb", 5);
		}
		else if(filesize<1000000000){ //mb
			leadingnum = filesize/1000000;
			strncpy(unitstr, "mb", 5);
		}
		else{ //gb
			leadingnum = filesize/1000000000;
			strncpy(unitstr, "gb", 5);
		}
		sprintf(sizestr, "%d%s", leadingnum, unitstr);
	}
	
	char namecell[1024];
	char linkpath[1024];
	sprintf(linkpath, "./%s%s",directoryPath+strlen("/serverroot/"), filename);
	sprintf(namecell, "<a href=\"%s\">%s</a>", linkpath, filename);

	char tablerow[1024];
	sprintf(tablerow, "<tr>\n<td>%s</td>\n<td>%s</td>\n<td>%s</td><td>%s</td>\n</tr>\n", type, namecell, modifieddate, sizestr);
	strncpy(resultstr, tablerow, 1024);
	printf("tablerow: %s\n",tablerow);
	return;
}

void drawindexpage(char* directoryPath, char* resultstr){
	DIR *d;
	struct dirent *dir;
	long long filesize;
	memset(resultstr, 0, strlen(resultstr));
	
	//header
	strcat(resultstr, "<!doctype html>\n<html><head>\n");
	char titlebuffer[256];
	snprintf(titlebuffer, 256, "<title>%s index</title>\n", directoryPath);
	strcat(resultstr, titlebuffer);
	strcat(resultstr, "<style>table{\nborder: solid 1px;\n}\nth{\ncolor: blue;\ntext-decoration: underline;\n}\n</style>\n</head>\n");
	strcat(resultstr, "<body>\n");
	char indexlabelbuffer[256];
	snprintf(indexlabelbuffer, 256, "<h1>index of %s</h1>\n", directoryPath);
	strcat(resultstr, indexlabelbuffer);
	
	char correctDirectoryPath[1024];
	snprintf(correctDirectoryPath, 1024, "%s/", directoryPath);
	d = opendir(directoryPath);
	if (d){
		strcat(resultstr, "<table>\n<tr>\n<th>type</th>\n<th>name</th>\n<th>last modified</th>\n<th>size</th>\n</tr>\n");
		char tablerow[1024];
		memset(tablerow, 0, 1024);
		gettablerow("..", correctDirectoryPath, 1, tablerow);
		strcat(resultstr, tablerow);
		memset(tablerow, 0, 1024);
		gettablerow(".", correctDirectoryPath, 1, tablerow);
		strcat(resultstr, tablerow);
		while ((dir=readdir(d)) != NULL){
			if(!strncmp(dir->d_name, ".", 1)){
				continue;
			}
			if(!strncmp(dir->d_name, "..", 2)){
				continue;
			}
			memset(tablerow, 0, 1024);
			gettablerow(dir->d_name, correctDirectoryPath, !(dir->d_type==8), tablerow);
			strcat(resultstr, tablerow);
		}
		closedir(d);
		strcat(resultstr, "</table>\n");
	}
	else{
		strcat(resultstr, "<h2>error opening directory!</h2>\n");
	}
	strcat(resultstr, "</body>\n</html>");
	return;
}
