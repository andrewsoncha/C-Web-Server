/**
 * webserver.c -- A webserver written in C
 * 
 * Test with curl (if you don't have it, install it):
 * 
 *    curl -D - http://localhost:3490/
 *    curl -D - http://localhost:3490/d20
 *    curl -D - http://localhost:3490/date
 * 
 * You can also test the above URLs in your browser! They should work!
 * 
 * Posting Data:
 * 
 *    curl -D - -X POST -H 'Content-Type: text/plain' -d 'Hello, sample data!' http://localhost:3490/save
 * 
 * (Posting data is harder to test from a browser.)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/file.h>
#include <fcntl.h>
#include <pthread.h>

#include "net.h"
#include "file.h"
#include "mime.h"
#include "cache.h"
#include "directory.h"

#define PORT "3490"  // the port users will be connecting to
#define MX_CLIENTS 256

#define SERVER_FILES "./serverfiles"
#define SERVER_ROOT "./serverroot"
#define TIME_DIFF 60

struct pthread_args {
	int fd;
	struct cache *cache;
};

int clnt_socks[MX_CLIENTS];
int clnt_cnt=0;

pthread_mutex_t mutx;

/**
 * Send an HTTP response
 *
 * header:       "HTTP/1.1 404 NOT FOUND" or "HTTP/1.1 200 OK", etc.
 * content_type: "text/plain", etc.
 * body:         the data to send.
 * 
 * Return the value from the send() function.
 */
int send_response(int fd, char *header, char *content_type, void *body, int content_length)
{
    const int max_response_size = 262144;
    char response[max_response_size];
	int header_length = strlen(header);
	int content_type_len = strlen(content_type);
	if((header_length+content_type_len+content_length)>max_response_size){ //size of packet exceeds max_response_size
		return -1;
	}
	time_t now = time(NULL);
	struct tm* timeFormat = localtime(&now);
	char *dayOfWeek[7] = {
		"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
	};
	char *monthName[12] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	int wday, month, day, hour, min, sec, year;
	wday = timeFormat->tm_wday; 
	month = timeFormat->tm_mon; 
	day = timeFormat->tm_mday; 
	hour = timeFormat->tm_hour; 
	min = timeFormat->tm_min; 
	sec = timeFormat->tm_sec;
	year = 1900+timeFormat->tm_year;
	
	
    // Build HTTP response and store it in response
	int charCnt=0;
	charCnt += sprintf(response, "%s\n",header);
	charCnt += sprintf(response+charCnt, "Date: %s %s %d %d:%d:%d PST %d\n", dayOfWeek[wday], monthName[month], day, hour, min, sec, year);
	charCnt += sprintf(response+charCnt, "Connection: close\n");
	charCnt += sprintf(response+charCnt, "Content-Length: %d\n",content_length);
	charCnt += sprintf(response+charCnt, "Content-Type: %s\n\n", content_type);
	memcpy(response+charCnt, body, content_length);
	
	int response_length = charCnt+content_length;

    // Send it all!
    int rv = send(fd, response, response_length, 0);

    if (rv < 0) {
        perror("send");
    }

    return rv;
}


/**
 * Send a /d20 endpoint response
 */
void get_d20(int fd)
{
    // Generate a random number between 1 and 20 inclusive
	srand(time(NULL));
    int random_number = rand()%20+1;
	char number_str[3] = {0,0,0};
	sprintf(number_str, "%d\n",random_number);
	puts(number_str);

    // Use send_response() to send it back as text/plain data
	
	char header[] = "HTTP/1.1 200 OK";
	char content_type[] = "text/plain";
	send_response(fd, header, content_type, number_str, strlen(number_str));
}

/**
 * Send a 404 response
 */
void resp_404(int fd)
{
    char filepath[4096];
    struct file_data *filedata; 
    char *mime_type;

    // Fetch the 404.html file
    snprintf(filepath, sizeof filepath, "%s/404.html", SERVER_FILES);
    filedata = file_load(filepath);

    if (filedata == NULL) {
        fprintf(stderr, "cannot find system 404 file\n");
        exit(3);
    }

    mime_type = mime_type_get(filepath);
	printf("Serving 404!\n");
	printf("404 file: %s\n",filedata->data);

    send_response(fd, "HTTP/1.1 404 NOT FOUND", mime_type, filedata->data, filedata->size);
	printf("response sent!\n");

    file_free(filedata);
	printf("filedata freed!\n");
}

/**
 * Read and return a file from disk or cache
 */
void get_file(int fd, struct cache *cache, char *request_path)
{
	char filePath[1020];
	struct file_data *fileContent;
	char* mime_type;
	int foundInCache = 0;
	strncpy(filePath, request_path, 1020);
	printf("filePath:  %s \n",filePath);
	
	pthread_mutex_lock(&mutx);
	struct cache_entry *entry = cache_get(cache, request_path);
	if(entry!=NULL){
		time_t current = time(NULL);
		if(current - entry->created_at < TIME_DIFF){
			printf("file found from entry. Serving from cache\n");
			
			print_cache(cache);
			foundInCache = 1;
		}
		else{
			cache_delete(cache, entry);
		}	
		if(foundInCache==1){
			send_response(fd, "HTTP/1.1 200 OK", entry->content_type, entry->content, entry->content_length);
		}
	}
	pthread_mutex_unlock(&mutx);
	
	if(foundInCache==1){
		return;
	}
	
	fileContent = file_load(filePath);
	if(fileContent==NULL){ // if the requested file doesn't exist or is not a regular file, serve 404
		printf("FILE NOT FOUND! SERVING 404!\n"); 
		resp_404(fd);
	}
	else{
		printf("file not found from entry. serving from disk\n");
		printf("FILE FOUND. SERVING 200\n");
		mime_type = mime_type_get(filePath);
		printf("mime type got! %s\n",mime_type);
		pthread_mutex_lock(&mutx);
		printf("mutex locked!\n");
		cache_put(cache, request_path, mime_type, fileContent->data, fileContent->size);
		print_cache(cache);
		pthread_mutex_unlock(&mutx);
		printf("mutex unlocked!\n");
		send_response(fd, "HTTP/1.1 200 OK", mime_type, fileContent->data, fileContent->size);
		file_free(fileContent);
	}
}

void get_directory(int fd, struct cache* cache, char *request_path){
	int foundInCache = 0;
	pthread_mutex_lock(&mutx);
	struct cache_entry *entry = cache_get(cache, request_path);
	if(entry!=NULL){
		time_t current = time(NULL);
		if(current - entry->created_at < TIME_DIFF){
			printf("directory found from entry. Serving from cache\n");
			print_cache(cache);
			foundInCache = 1;
			return;
		}
		else{
			cache_delete(cache, entry);
		}	
		if(foundInCache==1){
			send_response(fd, "HTTP/1.1 200 OK", entry->content_type, entry->content, entry->content_length);
		}
	}
	pthread_mutex_unlock(&mutx);
	if(foundInCache==1){
		return;
	}
	
	char index_page[8192] = "";
	printf("directory not found from entry. Drawing new Index page\n");
	
	drawindexpage(request_path, index_page);
	int indexLen = strlen(index_page);
	
	pthread_mutex_lock(&mutx);
	cache_put(cache, request_path, "text/html", index_page, indexLen);
	pthread_mutex_unlock(&mutx);
	send_response(fd, "HTTP/1.1 200 OK", "text/html", index_page, indexLen);
	
	print_cache(cache);
}

/**
 * Search for the end of the HTTP header
 * 
 * "Newlines" in HTTP can be \r\n (carriage return followed by newline) or \n
 * (newline) or \r (carriage return).
 */
char *find_start_of_body(char *header, int bytes_recv)
{ //just skip the first four lines of request
    int newLineCnt = 0;
	char* endOfHeader;
	
	endOfHeader = strstr(header, "\r\n\r\n");
	return (endOfHeader==NULL)?NULL:(endOfHeader+4);
}

int save_file(char* endPoint, char* fileContent, int fileSize){
	printf("save_file called\n");
	struct file_data *newFile;
	newFile = (struct file_data*)malloc(sizeof(struct file_data));
	newFile->data = (void*)malloc(fileSize);
	printf("newFile allocated!\n");
	printf("\n\nendPoint: %s\n\n",endPoint);
	newFile->size = fileSize;
	printf("strlen(fileContent): %d   fileSize: %d\n",strlen(fileContent), fileSize);
	memcpy(newFile->data, fileContent, fileSize);
	
	file_write(endPoint, newFile);
}

void post_save(int fd, char* savePath, char* request, int bytes_recvd){
	printf("request type is POST!\n");
	printf("request: %s\n",request);
	char* startOfBody = find_start_of_body(request, bytes_recvd);
	int fileSize = bytes_recvd - (startOfBody-request);
	printf("startOfBody: %s\n",startOfBody);
	printf("fileSize: %d\n",fileSize);
	printf("savePath: %s\n",savePath);
	save_file(savePath, startOfBody, fileSize);

	//send response. application/json {"status":"ok"}
	char returnStatus[] = "{\"status\":\"ok\"}\n";
	char content_type[] = "application/json";
	printf("content_type: %s\n",content_type);
	printf("return Status: %s\n",returnStatus);
	printf("return len: %d\n",strlen(returnStatus));
	send_response(fd, "HTTP/1.1 200 OK", content_type, returnStatus, strlen(returnStatus));
}

void handle_get(int fd, char* endPoint, struct cache *cache){
	printf("_____\n");
	printf("handle_get(%s) %d   %d\n",endPoint, strlen(endPoint), strlen("/"));
	printf("_____\n");
	if((strlen(endPoint)==strlen("/d20"))&&(strncmp(endPoint, "/d20", strlen(endPoint))==0)){
		printf("endPoint is /d20!\n");
		get_d20(fd);
	}
	else if(strlen(endPoint)==strlen("/")){
		printf("serving index page!\n");
		char filePath[1100] = "";
		sprintf(filePath, "%s%s",SERVER_ROOT, "/index.html");
		printf("filePath: %s\n",filePath);
		get_file(fd, cache, filePath);
	}
	else{
		char filePath[1100] = "";
		sprintf(filePath, "%s%s",SERVER_ROOT, endPoint);
		printf("is directory: %d\n",isdirectory(filePath));
		if(isdirectory(filePath)==1){ //when filePath is a directory
			get_directory(fd, cache, filePath);
		}
		else{ //either when filePath is a file(isdirectory==0) or when file or directory does not exist(isdirectory==-1)
			get_file(fd, cache, filePath);
		}
	}
}

/**
 * Handle HTTP request and send response
 */
void handle_http_request(struct pthread_args *args)
{
	int fd = args->fd;
	struct cache *cache = args->cache;
	
    const int request_buffer_size = 65536; // 64K
    char request[request_buffer_size];

    // Read request
    int bytes_recvd = recv(fd, request, request_buffer_size - 1, 0);
	printf("bytes_recvd: %d\n",bytes_recvd);
	printf("request:   %s\n",request);
	
    if (bytes_recvd < 0) {
        perror("recv");
        return;
    }

    // Read the first two components of the first line of the request 
 
    // If GET, handle the get endpoints

    //    Check if it's /d20 and handle that special case
    //    Otherwise serve the requested file by calling get_file()
	int readCnt=0;
	
	char requestType[10] = "", endPoint[1000] = "", version[10] = "";
	
	sscanf(request, " %s %s ", requestType, endPoint);
	puts(requestType);
	puts(endPoint);
	//printf("requestType:   %s", requestType);
	printf("endPoint:   %s\n", endPoint);
	if(strncmp(requestType, "GET", strlen("GET"))==0){
		printf("request type is GET!\n");
		handle_get(fd, endPoint, cache);
	}
	else if(strncmp(requestType, "POST", strlen("POST"))==0){
		printf("request type is POST!\n");
		/* if(strstr(endPoint, "/save/")==endPoint){ //if endPoint starts with "/save/"
			char* savePath = (char*)malloc(strlen("./serverroot/")+strlen(endPoint)-strlen("/save/"));
			strncpy(savePath, "./serverroot/", strlen("./serverroot/"));
			strcat(savePath,(strstr(endPoint, "/save/")+strlen("/save/")));
			printf("savePath: %s\n",savePath);
			post_save(fd, savePath, request, bytes_recvd);
			free(savePath);
		}*/
		char savePath[100] = "";
		// char* savePath = (char*)malloc(strlen("./serverroot")+strlen(endPoint));
		printf("savePath len: %d\n",strlen(savePath));
		for(int i=0;i<strlen(savePath);i++){
			savePath[i] = 0;
		}
		strncpy(savePath, "./serverroot", strlen("./serverroot"));
		for(int i=0;i<strlen(savePath);i++){
			printf("%c",savePath[i]);
		}
		printf("\n");
		printf("savePath: %s\n",savePath);
		strncpy(savePath+strlen("./serverroot"), endPoint, strlen(endPoint));
		printf("endPoint: %s\n",endPoint);
		printf("savePath: %s\n",savePath);
		post_save(fd, savePath, request, bytes_recvd);
	}
	
	//close socket and remove thread from clnt_threads
	printf("closing socket %d!\n",fd);
	close(fd);
	
	pthread_mutex_lock(&mutx);
	for(int i=0;i<clnt_cnt;i++){
		if(fd==clnt_socks[i]){
			while(i++<clnt_cnt-1){ //pull sockets to the front by 1
				clnt_socks[i] = clnt_socks[i+1];
			}
			break;
		}
	}
	clnt_cnt--;
	pthread_mutex_unlock(&mutx);
}

/**
 * Main
 */
int main(void)
{
    int newfd;  // listen on sock_fd, new connection on newfd
    struct sockaddr_storage their_addr; // connector's address information
    char s[INET6_ADDRSTRLEN];

    struct cache *cache = cache_create(10, 1);

	pthread_mutex_init(&mutx, NULL);
		
	
    // Get a listening socket
    int listenfd = get_listener_socket(PORT);

    if (listenfd < 0) {
        fprintf(stderr, "webserver: fatal error getting listening socket\n");
        exit(1);
    }

    printf("webserver: waiting for connections on port %s...\n", PORT);

    // This is the main loop that accepts incoming connections and
    // responds to the request. The main parent process
    // then goes back to waiting for new connections.
    
    while(1) {
        socklen_t sin_size = sizeof their_addr;

        // Parent process will block on the accept() call until someone
        // makes a new connection:
        newfd = accept(listenfd, (struct sockaddr *)&their_addr, &sin_size);
        if (newfd == -1) {
            perror("accept");
            continue;
        }

        // Print out a message that we got the connection
        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        printf("server: got connection from %s\n", s);
        
        // newfd is a new socket descriptor for the new connection.
        // listenfd is still listening for new connections.
		
		if(clnt_cnt < MX_CLIENTS){
			pthread_mutex_lock(&mutx);
			clnt_socks[clnt_cnt++] = newfd;
			pthread_mutex_unlock(&mutx);
			
			struct pthread_args args;
			args.fd = newfd;
			args.cache = cache;

			pthread_t t_id;
			pthread_create(&t_id, NULL, handle_http_request, (void*)&args);
			pthread_detach(t_id);
		}

    }
	pthread_mutex_destroy(&mutx);

    // Unreachable code(hopefully)

    return 0;
}
