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

int SOCKET;
int MAX_THREADS=3;
int NUM_THREADS=0;
long COUNTER=0;

void* reply(void* inhandle);
char* readFile(char* fileName);

struct fileInfo
{
	int fileHandle;
	int size;
};

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
        char *errormsg = "<body><h1>404 - Not Found</h1></body>";
        char *errortemplate = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nContent-Length: %d\r\nConnection: close\r\n";
        char errorheader[strlen(errortemplate-1)];
        snprintf(errorheader, strlen(errorheader), errortemplate, strlen(errormsg));
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

void *reply(void *inhandle)
{
	int handle = *(int*)inhandle;

	char* requestBuffer = malloc(1024);
	int recvSize = recv(handle, requestBuffer, 1024, 0); 
	if(recvSize < 0)
	{
		free(requestBuffer);
		printf("Bad request\n");
		pthread_exit(NULL);
	}

	char* returnValue = findRequestResponse(requestBuffer, recvSize);
	free(requestBuffer);

    	if(returnValue != NULL)
    	{
		int length = strlen(returnValue);
		printf("return payload length: %d\n", length);
		send(handle, returnValue, length, 0);
		free(returnValue);
    	}
    	else
    	{
		send_response_404(handle);
    	}

	close(handle);
	NUM_THREADS--;
	printf("Thread exiting..\n");
	pthread_exit(NULL);
}

char* readFile(char* fileName)
{
	struct stat filestat;
	if(stat(fileName, &filestat) < 0)
	{
		printf("Couldnt stat file: %s \n", fileName);
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
