#define _GNU_SOURCE 
#include <sys/types.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <syslog.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include "linkedList.h"


#define MYPORT       			"9000"
#define BACKLOG      			10
#define MAX_DATA_LEN 			1000
#define MAX_FILE_LEN			20000
#define SOCKET_DATA_FILE		"/var/tmp/aesdsocketdata"


int sockfd;
// int socketDataFd;
int threadsCompleted = 0;
timer_t timer_id;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t readLock = PTHREAD_MUTEX_INITIALIZER;

typedef struct socketData
{
	int 			 socketId;
	struct sockaddr *socketAddr;
}socketData_t;

void *get_in_addr(struct sockaddr *sa);
void cleanup(int sigNo);
void *socketHandling (void* data);
void intervalTimerThread(union sigval sv);

LinkedList threadsList;

int main(int argc, char* argv[])
{
	struct sockaddr_storage theirAddr;
	socklen_t addrSize;
	struct addrinfo hints, *res;
	struct sigaction newAct ;
	int ret;
	int newClientFd;
	int isDaemon = 0;
	pthread_t tid;
	socketData_t clientSocket;


    struct sigevent sev;
    struct itimerspec intervalTimeSpec;

	// Node *current;
	// Node *nextNode;

	
    initList(&threadsList);

	for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            isDaemon = 1;
            break;
        }
    }

	// first, load up address structs with getaddrinfo():

	openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

	getaddrinfo(NULL, MYPORT, &hints, &res);

	// make a socket, bind it, and listen on it:

	sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	
	/* Setup to allow reusing socket and port*/
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == -1)
    {
        printf("setsockopt ADDR: %s\n", strerror(errno));
        exit(-1);
    }

	memset(&newAct, 0, sizeof(newAct));
    newAct.sa_handler = cleanup;

	sigaction(SIGTERM, &newAct, NULL);
	sigaction(SIGINT, &newAct, NULL);


	ret = bind(sockfd, res->ai_addr, res->ai_addrlen);
	if (ret == -1)
	{
		perror("failed to bind");
		return -1;
	}

	freeaddrinfo(res);

	ret = listen(sockfd, BACKLOG);
	if (ret == -1)
	{
		perror("failed to listen");
		return -1;
	}

	int childPid = fork();

	if (childPid > 0) /* Parent */
	{
		if (isDaemon == 1)
		{
			return 0;
		}
	}
	else              /* Child */
	{

		if (isDaemon == 1)
		{
			setsid();

			if ((chdir("/")) < 0) {
				perror("chdir");
				_exit(-1);
			}

			close(0);
			close(1);
			close(2);
		}

		// Configure the timer to generate signal
		sev.sigev_notify = SIGEV_THREAD;
		sev.sigev_notify_function = intervalTimerThread;
		sev.sigev_notify_attributes = NULL;
		
		// Create the timer
		if (timer_create(CLOCK_REALTIME, &sev, &timer_id) == -1) {
			perror("timer_create");
			exit(1);
		}

		// Configure timer intervals
		//memset(&intervalTimeSpec, 0, sizeof(intervalTimeSpec));
		intervalTimeSpec.it_value.tv_sec = 10;        // Initial delay: 10 second
		intervalTimeSpec.it_value.tv_nsec = 0;
		intervalTimeSpec.it_interval.tv_sec = 10;     // Repeat every 10 seconds
		intervalTimeSpec.it_interval.tv_nsec = 0;
		
		// Start the timer
		if (timer_settime(timer_id, 0, &intervalTimeSpec, NULL) == -1) {
			perror("timer_settime");
			exit(1);
		}


		// now accept an incoming connection:
		addrSize = sizeof(theirAddr);
		while (1) 
		{
			newClientFd = accept(sockfd, (struct sockaddr *)&theirAddr, &addrSize);
			if (newClientFd == -1)
			{
				perror("failed to accept new connections");
				return -1;
			}

			clientSocket.socketId = newClientFd;
			clientSocket.socketAddr = (struct sockaddr *)&theirAddr;


			ret = pthread_create(&tid, NULL, socketHandling, &clientSocket);
			if (ret != 0 )
			{
				perror("pthread_create:");
				return -1;
			}
			// current = getHead(&threadsList); // Get the head of the list
			// nextNode = NULL;          // To store the next node safely

			// while (current != NULL && threadsCompleted > 0) {
			// 	nextNode = current->next; // Save the next node before modifying the list
			// 	ret = pthread_tryjoin_np(current->data, NULL);
			// 	if (ret == 0)
			// 	{
			// 		removeNode(&threadsList, current->data); // Remove the current node
			// 		threadsCompleted--;
			// 	}
			// 	current = nextNode; // Move to the next node
			// }

			addNodeToHead(&threadsList, (int)tid);
		}

	}

	pthread_exit(NULL);
	
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void cleanup(int sigNo)
{

	Node *NodePtr = NULL;
	int ret = 0;

	syslog(LOG_INFO, "Caught signal, exiting");
	close(sockfd);

	timer_delete(timer_id);

	// if (socketDataFd > 0)
	// {
		
	// 	ret = close(socketDataFd);
	// 	ret = unlink(SOCKET_DATA_FILE);
	// 	if (ret == -1)
	// 	{
	// 		perror("unlink:");
	// 	}
	// }

	ret = unlink(SOCKET_DATA_FILE);
	if (ret == -1)
	{
		perror("unlink:");
	}

	/* kill all the exsited threads */
    
	while ((NodePtr = getHead(&threadsList)) != NULL)
	{
		ret = pthread_cancel(NodePtr->data);
		// if (ret != 0)
		// {
		// 	perror("pthread_cancel failed");
		// }
		ret = pthread_join(NodePtr->data, NULL);
		if (ret !=0 )
		{
			perror("pthread_join:");
			break;
		}

		removeHead(&threadsList);
	}
	closelog(); 

	_exit(0);

}

void *socketHandling (void* data)
{
	int clientFd = ((socketData_t *) data)->socketId;
	// struct sockaddr *clientAdd = ((socketData_t *) data)->socketAddr;
	int dataLen = 0;
	char recvData[MAX_DATA_LEN];
	char readData[MAX_FILE_LEN];
	// char ipStr[INET6_ADDRSTRLEN];
	int ret = 0;
	int fileFd;

	// inet_ntop(clientAdd->sa_family,
	// 			get_in_addr(clientAdd),
	// 			ipStr, sizeof(ipStr));

	// syslog(LOG_INFO, "Accepted connection from %s",ipStr);

	// printf("Accepted connection");

	// ready to communicate on socket descriptor socketDataFd!
	while (1)
	{
		memset(&recvData, 0, sizeof(recvData));
		dataLen = recv(clientFd, recvData, MAX_DATA_LEN, 0);
		if (dataLen > 0)
		{
			
			pthread_mutex_lock(&lock);
			fileFd = open(SOCKET_DATA_FILE, O_CREAT | O_APPEND | O_RDWR, 0666);
			if (fileFd == -1)
			{
				perror("Openning Log File");
			}
			//lseek(socketDataFd, 0, SEEK_END);
			write(fileFd, recvData, dataLen);

			
			if (recvData[dataLen - 1] == '\n')
			{	

				lseek(fileFd, 0, SEEK_SET);
				ret = read(fileFd, readData, MAX_FILE_LEN);
				if (ret == -1)
				{
					perror("failed to read the file");
				}
				close(fileFd);
				pthread_mutex_unlock(&lock);
				

				send(clientFd, readData, strlen(readData), 0);
			}
			else
			{
				close(fileFd);
				pthread_mutex_unlock(&lock);
			}
		}
		else
		{
			// syslog(LOG_INFO, "Closed connection from %s",ipStr);
			// printf("Closed connection");

			break;
		}
	}

	threadsCompleted++;

	close(clientFd);

	pthread_exit(NULL);
}


void intervalTimerThread(union sigval sv)
{
	// Create a buffer to hold the formatted time string
    char buffer[100];
	size_t timeBuffSize = 0;
	int fileFd;
    
    // Get the current time
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime); // Get the current time in seconds since the epoch
    timeinfo = localtime(&rawtime); // Convert to local time

    // Format the time according to RFC 2822
    // RFC 2822 format: "%a, %d %b %Y %H:%M:%S %z"
    timeBuffSize = strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S %z\n", timeinfo);

	if ( timeBuffSize == 0)
	{
		perror("strftime:");
	}

	pthread_mutex_lock(&lock);
	fileFd = open(SOCKET_DATA_FILE, O_CREAT | O_APPEND | O_RDWR, 0666);
	if (fileFd == -1)
	{
		perror("Openning Log File");
	}
	//lseek(socketDataFd, 0, SEEK_END);
	write(fileFd, buffer, timeBuffSize);
	close(fileFd);
	pthread_mutex_unlock(&lock);
}

