#include <pthread.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>

int SOCKET;
int MAX_THREADS=3;
int NUM_THREADS=0;
char* WWWROOT = "/Users/janmariu";

struct fileInfo
{
	bool valid;
	int fileHandle;
	int size;
	bool isFolder;
	bool isRegularFile;
};

void* respond_to_request(void* inhandle);
struct fileInfo find_fileinfo(char *path);

void configure_socket(int port)
{
	struct sockaddr_in localaddr;
	localaddr.sin_family = AF_INET;
	localaddr.sin_port = htons(port);
	localaddr.sin_addr.s_addr = 0;

	SOCKET = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(SOCKET < 0)
	{
		printf("Failed to create socket..\n");
        exit(1);
	}

	if(bind(SOCKET, 
		(struct sockaddr*)&localaddr, 
		(socklen_t)sizeof(localaddr)))
	{
		printf("Failed to bind..\n");
        exit(2);
	} 

	if(listen(SOCKET, 10) < 0)
	{
		printf("Couldnt listen to socket..\n");
        exit(3);
	}
}

void accept_connections()
{
	printf("Accepting connections..\n");
	struct sockaddr_in source;
	socklen_t sourcesize = sizeof(source);

	while(1)
	{
		int connectionHandle = accept(SOCKET, 
				(struct sockaddr*)&source, 
				&sourcesize);
		
		if(NUM_THREADS <= MAX_THREADS)
		{
		/*	char ip[15];
			inet_ntop(AF_INET, &(source.sin_addr), ip, 15);
			printf("Connection from: %s\n", ip);
		*/
			NUM_THREADS++;
			pthread_t id;
			pthread_create(&id, NULL, respond_to_request, &connectionHandle);
		}
		else
		{
			printf("Dropping connection. MAX_THREADS reached.\r\n");
		}
	}
}

char* find_contenttype(char* path)
{
    char* extension = strrchr(path, '.');

    if(extension != NULL)
    {
        if((strcmp(extension, ".html") == 0 ||
             strcmp(extension, ".htm") == 0))
        {
            return "text/html";
        }
        else if(strcmp(extension, ".jpg") == 0)
        {
            return "image/jpeg";
        }
        else if(strcmp(extension, ".png") == 0)
        {
            return "image/png";
        }
        else if(strcmp(extension, ".gif") == 0)
        {
            return "image/gif";
        }
        else if(strcmp(extension, ".css") == 0)
        {
            return "text/css";
        }
        else if(strcmp(extension, ".txt") == 0 
            || strcmp(extension, ".c") == 0 
            || strcmp(extension, ".cpp") == 0
            || strcmp(extension, ".log") == 0)
        {
            return "text/plain";
        }
    }
    return "application/octet-stream";
}

void send_response_msg(int sockethandle, char* msg)
{
    char* newline = "\r\n";
    char *headertemplate = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\nConnection: close\r\n";
    char *header = malloc(2048);
    sprintf(header, headertemplate, strlen(msg));
    send(sockethandle, header, strlen(header), 0);
    send(sockethandle, newline, strlen(newline), 0);
    send(sockethandle, msg, strlen(msg), 0);
    send(sockethandle, newline, strlen(newline), 0);
    free(header);
}

void send_response_ok(int sockethandle, char* contenttype, int contentlength)
{
    char *headertemplate = "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n";
    char *header = malloc(2048);
    sprintf(header, headertemplate, contenttype, contentlength);
    send(sockethandle, header, strlen(header), 0);
    send(sockethandle, "\r\n", 2, 0);
    free(header);
}

void send_response_404(int sockethandle)
{
    char *errormsg = "<body><h1>404 - Not Found</h1></body>";
    char *errortemplate = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nContent-Length: %d\r\nConnection: close\r\n";
    char *errorheader = malloc(2048);

    sprintf(errorheader, errortemplate, strlen(errormsg));
    send(sockethandle, errorheader, strlen(errorheader), 0);
    send(sockethandle, "\r\n", 2, 0);
    send(sockethandle, errormsg, strlen(errormsg), 0);
    send(sockethandle, "\r\n", 2, 0);
    free(errorheader);
}

bool send_file_contents(int sockethandle, struct fileInfo *fileinfo)
{
    char* buffer = malloc(fileinfo->size);
    memset(buffer, 0, fileinfo->size);

    if(fileinfo->fileHandle > 0)
    {
        int bytesRead = read(fileinfo->fileHandle, buffer, fileinfo->size);
        int bytesSent = send(sockethandle, buffer, bytesRead, 0);
        return bytesSent == bytesRead;
    }

    free(buffer);
    return false;
}

char* find_requested_resource(int sockethandle)
{
    //TODO: what if the request is bigger than one byte? There is a better way to find the url.
	char* result = NULL;
	char* requestBuffer = malloc(1024);
    memset(requestBuffer, 0, 1024);
	int recvSize = recv(sockethandle, requestBuffer, 1024, 0); 

	if(recvSize > 3)
	{
		if(strncmp(requestBuffer, "GET", 3) == 0)
		{	
			char* token = "/";
			char* start = strpbrk(requestBuffer,token);		
			char* iterator = start;

            if(start != NULL)
            {
                int length = 0;
                while(*iterator++ != ' ' && length <= recvSize){ length++; }
                
                int resultlen = strlen(WWWROOT) + length + 1;
                result = malloc(resultlen);
                memset(result, 0, resultlen);
                result[resultlen] = '\0';

                strncpy(result, WWWROOT, strlen(WWWROOT));
                strncat(result, start, length);
            }
    	}
	}
	
	free(requestBuffer);
	return result;
}

char* create_directory_listing(char* path)
{
    //Strip ending slashes
    while(strlen(path) > 1 && strcmp(path + strlen(path)-1,"/") == 0)
    {
        char *tmppath = path;
        tmppath = tmppath + strlen(path)-1;
        *tmppath = '\0';
    }

    //Create a url relative to WWWROOT
    char *url = path + strlen(WWWROOT);

    DIR *dir = opendir(path);
    if(dir == NULL)
    {
        return NULL;
    }

    char* result = malloc(1);
    *result = '\0';
    char *linktemplate = "<a href=\"%s%s%s\">";
    int urllen = strlen(path);

    struct dirent *item;
    while((item = readdir(dir)) != NULL)
    {
        //Create a link to the url.
        int itemlen = strlen(item->d_name);
        char *tmpurl = malloc(strlen(linktemplate) + urllen + itemlen + 1);
        if(urllen > 1)
        {
            sprintf(tmpurl, linktemplate,url, "/", item->d_name);
        }
        else
        {
            sprintf(tmpurl, linktemplate, "", "", item->d_name);
        }

        //Add the item to the folder listing.
        size_t extrasize = strlen(result) + itemlen + strlen(tmpurl) + 9;
        result = realloc(result, extrasize);

        strcat(result, tmpurl);
        strcat(result, item->d_name);
        strcat(result, "</a><br>");
    }

    return result;
}

struct fileInfo find_fileinfo(char *path)
{
    struct fileInfo fileinfo = 
    {
        .fileHandle = -1,
        .size = 0,
        .valid = false,
        .isRegularFile = false,
        .isFolder = false,
    };
    
    struct stat filestat;
    fileinfo.valid = (stat(path, &filestat) == 0);

    if(fileinfo.valid)
    {
        fileinfo.fileHandle = open(path, O_RDONLY, 0);
        if(fileinfo.fileHandle <= 0) fileinfo.valid = false;

        fileinfo.isFolder = S_ISDIR(filestat.st_mode);
        fileinfo.isRegularFile = S_ISREG(filestat.st_mode);
        fileinfo.size = filestat.st_size;
    }

    return fileinfo;
}

void *respond_to_request(void *handle)
{
	int sockethandle = *(int*)handle;
    char *path = find_requested_resource(sockethandle);
	bool requestOk = false;

	if(path != NULL)
	{
		struct fileInfo fileinfo = find_fileinfo(path);
        printf("serving: %s\n", path);

        if(fileinfo.valid)
        {
            if(fileinfo.isRegularFile)
            {
                char *contenttype = find_contenttype(path);
                send_response_ok(sockethandle, contenttype, fileinfo.size);
                requestOk = send_file_contents(sockethandle, &fileinfo);
            }
            else if(fileinfo.isFolder)
            {
                char* listing = create_directory_listing(path);
                send_response_msg(sockethandle, listing);
                free(listing);
                requestOk = true;
            }

            close(fileinfo.fileHandle);
        }
	}

	if(!requestOk)
	{
		send_response_404(sockethandle);
	}
	
	free(path);
    close(sockethandle);
	NUM_THREADS--;
	pthread_exit(NULL);
}

void print_usage()
{
    printf("Usage: webserver.out <root folder> <port> Example: webserver.out /var/www 8080\n");
}

int main(int argc, char** argv)
{
    if(argc < 3)
    {
        print_usage();
        return -1;
    }

    DIR *dir = opendir(argv[1]);
    if(dir == NULL)
    {
        print_usage();
        return -2;
    }
    closedir(dir);
    WWWROOT = argv[1];

    if(strtol(argv[2], NULL, 10) <= 0)
    {
        printf("Invalid port.\n");
        print_usage();
        return -3;
    }

    printf("Starting..\n");
    configure_socket(atoi(argv[2]));
	accept_connections();
	printf("Exiting..\n");
    return 0;
}
