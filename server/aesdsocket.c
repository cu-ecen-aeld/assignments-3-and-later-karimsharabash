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
#include <sys/queue.h>
#include <stdatomic.h>
#include <stdbool.h>
// #include "linkedList.h"


#define MYPORT       			"9000"
#define BACKLOG      			12
#define MAX_DATA_LEN 			200
#define MAX_FILE_LEN			4096
#define SOCKET_DATA_FILE		"/var/tmp/aesdsocketdata"


int sockfd;
int threadsCompleted = 0;
timer_t timer_id;

atomic_bool running = true;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct socketData
{
	int 			 socketId;
	struct sockaddr *socketAddr;
}socketData_t;

//linked list structures
struct entry {
	pthread_t thread;
	SLIST_ENTRY(entry) entries;            
};
void cleanup(int sigNo);


SLIST_HEAD(ThreadsList, entry) threadQueue;

void *get_in_addr(struct sockaddr *sa);
void *socketHandling (void* data);
void intervalTimerThread(union sigval sv);

int main(int argc, char* argv[])
{
	struct sockaddr_storage theirAddr;
	socklen_t addrSize;
	struct addrinfo hints, *res;
	struct sigaction newAct ;
	int ret;
	int newClientFd;
	int isDaemon = 0;

    struct sigevent sev;
    struct itimerspec intervalTimeSpec;

	struct entry *tmpNode;

	openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);

	for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            isDaemon = 1;
            break;
        }
    }

	// first, load up address structs with getaddrinfo():
	pthread_mutex_init(&lock, NULL);

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
		perror("setsockopt");
 		exit(-1);
    }


	ret = bind(sockfd, res->ai_addr, res->ai_addrlen);
	if (ret == -1)
	{
		perror("failed to bind");
		return -1;
	}

	freeaddrinfo(res);

	memset(&newAct, 0, sizeof(newAct));
	newAct.sa_handler = cleanup;

	sigaction(SIGTERM, &newAct, NULL);
	sigaction(SIGINT, &newAct, NULL);

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
		intervalTimeSpec.it_value.tv_sec = 10;        // Initial delay: 10 second
		intervalTimeSpec.it_value.tv_nsec = 0;
		intervalTimeSpec.it_interval.tv_sec = 10;     // Repeat every 10 seconds
		intervalTimeSpec.it_interval.tv_nsec = 0;
		
		// Start the timer
		if (timer_settime(timer_id, 0, &intervalTimeSpec, NULL) == -1) {
			perror("timer_settime");
			exit(1);
		}

		SLIST_INIT(&threadQueue);

		// now accept an incoming connection:
		addrSize = sizeof(theirAddr);
		while (atomic_load(&running)) 
		{
			newClientFd = accept(sockfd, (struct sockaddr *)&theirAddr, &addrSize);
			if (newClientFd == -1) { /* error handling */ }

			// Allocate memory for each client's socket data
			socketData_t *clientSocketPtr = malloc(sizeof(socketData_t));
			if (!clientSocketPtr) {
				syslog(LOG_USER | LOG_ERR, "Failed to malloc for clientSocketPtr");
				close(newClientFd); // Close the accepted socket
				continue;
			}
			clientSocketPtr->socketId = newClientFd;
			// Allocate and copy the sockaddr data so each thread has its own
			clientSocketPtr->socketAddr = malloc(sizeof(struct sockaddr_storage)); // Use storage for generic
			if (!clientSocketPtr->socketAddr) {
				syslog(LOG_USER | LOG_ERR, "Failed to malloc for socketAddr");
				free(clientSocketPtr);
				close(newClientFd);
				continue;
			}
			memcpy(clientSocketPtr->socketAddr, &theirAddr, sizeof(struct sockaddr_storage));


			tmpNode = malloc(sizeof( struct entry));
			if(!tmpNode){
				perror("cannot malloc for entry\n");
			}

			ret = pthread_create(&tmpNode->thread, NULL, socketHandling, clientSocketPtr);
			if (ret != 0 )
			{
				perror("pthread_create:");
				return -1;
			}

			SLIST_INSERT_HEAD(&threadQueue,tmpNode, entries);

			struct entry* current = SLIST_FIRST(&threadQueue);
            struct entry* next_cleanup;

			while (current != NULL && threadsCompleted > 0 ) {
                next_cleanup = SLIST_NEXT(current, entries);
				ret = pthread_tryjoin_np(current->thread, NULL);
				if (ret == 0)
				{	
					SLIST_REMOVE(&threadQueue, current, entry, entries);
                    free(current);
				}
                
                current = next_cleanup;
            }
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

	// Node *NodePtr = NULL;
	int ret = 0;

	timer_delete(timer_id);
	atomic_store(&running, false);

	syslog(LOG_INFO, "Caught signal, exiting");
	close(sockfd);


	/* kill all the exsited threads */
    struct entry* current = SLIST_FIRST(&threadQueue);
    while (current != NULL) {
        pthread_join(current->thread, NULL); // Wait for thread to finish
        struct entry* next_cleanup = SLIST_NEXT(current, entries); // Get next before removing current
        // The clientSocketPtr and its socketAddr should be freed by the thread itself.
        SLIST_REMOVE(&threadQueue, current, entry, entries); // Remove from list
        free(current); // Free the entry struct
        current = next_cleanup;
    }

	closelog(); 

	pthread_mutex_destroy(&lock);


	ret = remove(SOCKET_DATA_FILE);
	if (ret == -1)
	{
		perror("remove:");
	}

	_exit(0);

}

char* read_from_file(pthread_mutex_t* mutex) {
    ssize_t fsize = 0;
    char* buff = NULL;

    pthread_mutex_lock(mutex);
    int read_fd = open(SOCKET_DATA_FILE, O_RDONLY);

    if (read_fd == -1) {
        syslog(LOG_USER | LOG_ERR, "Could not open file to read");
        pthread_mutex_unlock(mutex);
        return NULL;
    }

    struct stat file_stat;
    fstat(read_fd, &file_stat);
    fsize = file_stat.st_size + 1;
    close(read_fd);
    pthread_mutex_unlock(mutex);

    syslog(LOG_USER | LOG_DEBUG, "File Size: %d", (int)fsize);

    buff = (char*) malloc(fsize);
    if (buff == NULL) {
        syslog(LOG_USER | LOG_ERR, "Could not allocate memory to read file");
        return NULL;
    }

    memset(buff, '\0', fsize);
    syslog(LOG_USER | LOG_DEBUG, "Reading from %s", SOCKET_DATA_FILE);

    pthread_mutex_lock(mutex);
    read_fd = open(SOCKET_DATA_FILE, O_RDONLY);
    if (read_fd == -1) {
        syslog(LOG_USER | LOG_ERR, "Could not open file to read");
        free(buff);
        pthread_mutex_unlock(mutex);
        return NULL;
    }

    ssize_t bytes_read = read(read_fd, buff, fsize - 1);
    if (bytes_read < 0) {
        syslog(LOG_USER | LOG_ERR, "Error reading from file: %s", strerror(errno));
        free(buff);
        close(read_fd);
        pthread_mutex_unlock(mutex);
        return NULL;
    }

    close(read_fd);
    pthread_mutex_unlock(mutex);
    return buff;
}

void *socketHandling (void* data)
{
	socketData_t *clientData = (socketData_t *) data; // Cast the incoming void*
	int clientFd = clientData->socketId;
	struct sockaddr *clientAdd = clientData->socketAddr;
	int dataLen = 0;
	char recvData[MAX_DATA_LEN];
	char *readData;
	char ipStr[INET6_ADDRSTRLEN];
	int ret = 0;
	int fileFd;

	inet_ntop(clientAdd->sa_family,
				get_in_addr(clientAdd),
				ipStr, sizeof(ipStr));

	syslog(LOG_INFO, "Accepted connection from %s",ipStr);

	// ready to communicate on socket descriptor socketDataFd!
	while((dataLen = recv(clientFd, recvData, MAX_DATA_LEN, 0)) > 0 && atomic_load(&running)) {

		pthread_mutex_lock(&lock);

		fileFd = open(SOCKET_DATA_FILE, O_CREAT | O_APPEND | O_RDWR, 0666);
		if (fileFd == -1)
		{
			perror("Openning Log File");
		}
		ret = write(fileFd, recvData, dataLen);
		if (ret == -1)
		{
			perror("write");
		}
		pthread_mutex_unlock(&lock);
		close(fileFd);

		if (recvData[dataLen -1] == '\n')
 			break;
 	}

	readData = read_from_file(&lock);
	if (ret == -1)
	{
		perror("failed to read the file");
	}
	send(clientFd, readData, strlen(readData), 0);

	threadsCompleted++;

	close(clientFd);

	syslog(LOG_INFO, "Closing connection from %s",ipStr);

	free(clientData);

	pthread_exit(NULL);
}


void intervalTimerThread(union sigval sv)
{
	// Create a buffer to hold the formatted time string
    char buffer[100];
	size_t timeBuffSize = 0;
	int fileFd;
	int ret = 0;
    
    // Get the current time
     time_t now;
    struct tm *timeinfo;

    now = time(NULL); // Get the current time in seconds since the epoch
    timeinfo = localtime(&now); // Convert to local time

    // Format the time according to RFC 2822
    // RFC 2822 format: "%a, %d %b %Y %H:%M:%S %z"
    timeBuffSize = strftime(buffer, sizeof(buffer), "timestamp:%a, %d %b %Y %H:%M:%S %z\n", timeinfo);

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
	ret = write(fileFd, buffer, timeBuffSize);
	if (ret == -1)
	{
		perror("write");
	}
	close(fileFd);
	pthread_mutex_unlock(&lock);
}

