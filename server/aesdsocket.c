#include <sys/types.h>
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

#define MYPORT       		"9000"
#define BACKLOG      		10
#define MAX_DATA_LEN 		10000
#define MAX_FILE_LEN		100000
#define SOCKET_DATA_FILE	"/var/tmp/aesdsocketdata"

int sockfd, new_fd;
int socketDataFd;

void *get_in_addr(struct sockaddr *sa);
void cleanup(int sigNo);


int main(int argc, char* argv[])
{
	struct sockaddr_storage their_addr;
	socklen_t addr_size;
	struct addrinfo hints, *res;
	struct sigaction newAct ;
	int ret;

	char ipStr[INET6_ADDRSTRLEN];
	int dataLen = 0;
	char recvData[MAX_DATA_LEN];
	char readData[MAX_FILE_LEN];

	int isDaemon = 0;

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
	

    newAct.sa_handler = cleanup;

	sigaction(SIGTERM, &newAct, NULL);
	sigaction(SIGINT, &newAct, NULL);

	ret = bind(sockfd, res->ai_addr, res->ai_addrlen);
	if (ret == -1)
	{
		perror("failed to bind");
		return -1;
	}
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
		// now accept an incoming connection:
		addr_size = sizeof(their_addr);
		while (1)
		{
			new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
			if (new_fd == -1)
			{
				perror("failed to accept new connections");
				return -1;
			}

			inet_ntop(their_addr.ss_family,
					get_in_addr((struct sockaddr *)&their_addr),
					ipStr, sizeof(ipStr));

			syslog(LOG_INFO, "Accepted connection from %s",ipStr);
			
			socketDataFd = open(SOCKET_DATA_FILE, O_CREAT | O_APPEND | O_RDWR, 0666);
			if (socketDataFd == -1)
			{
				perror("Openning Log File");
			}

			// ready to communicate on socket descriptor new_fd!
			while (1)
			{
				memset(&recvData, 0, dataLen);
				memset(&readData, 0, MAX_FILE_LEN);

				dataLen = recv(new_fd, recvData, MAX_DATA_LEN, 0);
				if (dataLen > 0)
				{
					off_t lseek(int fd, off_t offset, int whence);
					lseek(socketDataFd, 0, SEEK_END);
					write(socketDataFd, recvData, strlen(recvData));
					if (recvData[strlen(recvData) - 1] == '\n')
					{
						lseek(socketDataFd, 0, SEEK_SET);
						ret = read(socketDataFd, readData, MAX_FILE_LEN);
						if (ret == -1)
						{
							perror("failed to read the file");
						}

						send(new_fd, readData, strlen(readData), 0);
					}
				}
				else
				{
					syslog(LOG_INFO, "Closed connection from %s",ipStr);
					break;
				}
				
			}
		}
	}
	
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
	syslog(LOG_INFO, "Caught signal, exiting");
	close(sockfd);

	if (socketDataFd > 0)
	{
		int ret = 0;
		ret = close(socketDataFd);
		ret = unlink(SOCKET_DATA_FILE);
		if (ret == -1)
		{
			perror("unlink:");
		}
	}
    
	closelog();

	_exit(0);
}

