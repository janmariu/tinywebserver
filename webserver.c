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

int SOCKET;
int MAX_THREADS=3;
int NUM_THREADS=0;
long COUNTER=0;

struct fileInfo
{
	bool valid;
	int fileHandle;
	int size;
	bool isFolder;
	bool isRegularFile;
};

void* reply(void* inhandle);
char* readFile(char* fileName);
struct fileInfo findFileInfo(char *path);

void configureSocket(int port)
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

void acceptConnections()
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
			pthread_create(&id, NULL, reply, &connectionHandle);
		}
		else
		{
			printf("Dropping connection. MAX_THREADS reached.\r\n");
		}
	}
}

void printBuffer(char* buffer)
{
	printf("#Start buffer:\n");
	while(*buffer != '\0')
	{
		printf("%c", *buffer++);
	}
	printf("\n#End buffer\n");
}

char* findRequestResponse(char* requestBuffer, int bufferSize)
{
   	if(strncmp(requestBuffer, "GET", 3) == 0)
	{
		printf("GET Request\n");
        	char* token = "/";
		char* start = strpbrk(requestBuffer,token);		
		char* iterator = start;

		int length = 0;
		while(*iterator++ != ' ' && length <= bufferSize){ length++; }
		
		char url[length+1];

    	char* p = url;
    	strncpy(p, start,length);
		url[length] = '\0'; //strncpy is too stupid to terminate strings..

    	char* result = readFile(p);
		return result;
	}

	return NULL;
}

void send_response_ok(int sockethandle, int contentlength)
{	
        char *headertemplate = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nContent-Length: %d\r\nConnection: close\r\n";
        char header[strlen(headertemplate-1)];
        snprintf(header, strlen(header), headertemplate, contentlength);
		send(sockethandle, header, strlen(header), 0);
}

void send_response_404(int sockethandle)
{

        char *errormsg = "<body><h1>404 - Not Found</h1></body>";
        char *errortemplate = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nContent-Length: %d\r\nConnection: close\r\n";
        char errorheader[strlen(errortemplate-1)];
        snprintf(errorheader, strlen(errorheader), errortemplate, strlen(errormsg));
        send(sockethandle, errorheader, strlen(errorheader), 0);
        send(sockethandle, "\r\n", 2, 0);
        send(sockethandle, errormsg, strlen(errormsg), 0);
}


char* receiverequest(int sockethandle)
{
	char* result = NULL;
	char* requestBuffer = malloc(1024);
	int recvSize = recv(sockethandle, requestBuffer, 1024, 0); 

	if(recvSize > 0)
	{
		if(strncmp(requestBuffer, "GET", 3) == 0)
		{	
			char* token = "/";
			char* start = strpbrk(requestBuffer,token);		
			char* iterator = start;

			int length = 0;
			while(*iterator++ != ' ' && length <= recvSize){ length++; }
			
            result = malloc(length+1);
	    	strncpy(result, start,length);
    	}
	}
	
	free(requestBuffer);
	return result;
}

bool send_file_contents(int sockethandle, struct fileInfo *fileinfo)
{	
	char* buffer = malloc(fileinfo->size); 
	
	if(fileinfo->fileHandle > 0)
	{
		printf("Sending file\n");
		int bytesRead = read(fileinfo->fileHandle, buffer, fileinfo->size);
		int bytesSent = send(sockethandle, buffer, bytesRead, 0);
		return bytesSent == bytesRead;
	}

	free(buffer);
	return false;
}

void *reply(void *handle)
{
	int sockethandle = *(int*)handle;

	char *path = receiverequest(sockethandle);

	bool requestOk = false;

	if(path != NULL)
	{
		printf("path: %s\n", path);
		struct fileInfo fileinfo = findFileInfo(path);
        printf("regular %d, valid: %d\n", fileinfo.isRegularFile, fileinfo.valid);
		if(fileinfo.isRegularFile && fileinfo.valid)
		{
			printf("Sending response ok\n");
			send_response_ok(sockethandle, fileinfo.size);
			requestOk = send_file_contents(sockethandle, &fileinfo);
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

struct fileInfo findFileInfo(char *path)
{	
	struct fileInfo fileinfo;
	fileinfo.fileHandle = -1;
	fileinfo.size = 0;
    fileinfo.valid = false;
    fileinfo.isRegularFile = false;
    fileinfo.isFolder = false;
    //TODO: Find the sensible way to initialize structs.

	struct stat filestat;
    fileinfo.valid = (stat(path, &filestat) > 0);

	if(fileinfo.valid)
	{
		fileinfo.fileHandle = open(path, O_RDONLY, 0);
		if(fileinfo.fileHandle <= 0) fileinfo.valid = false;
	
		fileinfo.isFolder = S_ISDIR(filestat.st_mode);
		fileinfo.isRegularFile = S_ISREG(filestat.st_mode);
		fileinfo.size = filestat.st_size;

        printf("Fileinfo was valid..\n");
    }

	return fileinfo;
}


char* readFile(char* fileName)
{
	struct stat filestat;
	if(stat(fileName, &filestat) < 0)
	{
		printf("Couldnt stat file: %s \n", fileName);
		return NULL;
	}
	if(!S_ISREG(filestat.st_mode))
	{
		printf("Not regular file. Returning NULL");
		return NULL;		
	}

	int fileSize = filestat.st_size;

	int fh = open(fileName, O_RDONLY, 0);
	char* buffer = malloc(fileSize); 
	char* bufferIter = buffer;
	printf("Opening file: %s \n", fileName);
	
	if(fh > 0)
	{
		int bytesRead = read(fh, buffer, fileSize);

        if(bytesRead <= 0)
            return NULL;

		return buffer; 		
	}	
	else
	{
		printf("Couldnt open file: %s \n", fileName);
	}
	return NULL; 
}

int main()
{
	printf("Starting..\n");
	configureSocket(8080);
	acceptConnections();
	printf("Exiting..\n");
	return 0;
}
