#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include "file.h"
#include <errno.h>

/**
 * Loads a file into memory and returns a pointer to the data.
 * 
 * Buffer is not NUL-terminated.
 */
struct file_data *file_load(char *filename)
{
    char *buffer, *p;
    struct stat buf;
    int bytes_read, bytes_remaining, total_bytes = 0;

    // Get the file size
    if (stat(filename, &buf) == -1) {
        return NULL;
    }

    // Make sure it's a regular file
    if (!(buf.st_mode & S_IFREG)) {
        return NULL;
    }

    // Open the file for reading
    FILE *fp = fopen(filename, "rb");

    if (fp == NULL) {
        return NULL;
    }

    // Allocate that many bytes
    bytes_remaining = buf.st_size;
    p = buffer = malloc(bytes_remaining);

    if (buffer == NULL) {
        return NULL;
    }

    // Read in the entire file
    while (bytes_read = fread(p, 1, bytes_remaining, fp), bytes_read != 0 && bytes_remaining > 0) {
        if (bytes_read == -1) {
            free(buffer);
            return NULL;
        }

        bytes_remaining -= bytes_read;
        p += bytes_read;
        total_bytes += bytes_read;
    }

    // Allocate the file data struct
    struct file_data *filedata = malloc(sizeof *filedata);

    if (filedata == NULL) {
        free(buffer);
        return NULL;
    }

    filedata->data = buffer;
    filedata->size = total_bytes;

    return filedata;
}

int make_dir(char *savename){
	char pathname[100] = "";
	int lastSlashIdx  = strlen(savename);
	for(int i=strlen(savename)-1;i>=0;i--){
		if(savename[i]=='/'){
			lastSlashIdx = i;
			break;
		}	
	}
	strncpy(pathname, savename, lastSlashIdx+1);
	printf("make_dir    pathname: %s\n\n\n",pathname);
	
	int mkdir_result = mkdir(pathname, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH | S_IXOTH);
	printf("mkdir_result: %d %d\n",mkdir_result, errno);
}

int file_write(char *savename, struct file_data *filetowrite){
	int fd;
	ssize_t writtenbytes;
	printf("before make_dir savename: %s\n",savename);
	make_dir(savename);
	fd = open(savename, O_CREAT|O_TRUNC|O_RDWR, S_IRWXU|S_IRWXG|S_IRWXO);
	if(fd<0){
		fprintf(stderr, "open error for %s\n",savename);
	}
    printf("filetowrite->data: %s\n",filetowrite->data);
    printf("filetowrite->size: %d\n",filetowrite->size);
	writtenbytes = write(fd, filetowrite->data, filetowrite->size);
	close(fd);
	file_free(filetowrite);
	if(writtenbytes!=filetowrite->size){
		return -1;
	}
	else{
		return 0;
	}
}

/**
 * Frees memory allocated by file_load().
 */
void file_free(struct file_data *filedata)
{
    free(filedata->data);
    free(filedata);
}
