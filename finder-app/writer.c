#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>

int main(int argc, char* argv[])
{
	int fd = 0;
	int ret = 0;

	openlog("Writer", LOG_PID, LOG_USER);

	if (argc != 3)
	{
		printf("ERROR! writer file_path string\n");
		syslog(LOG_ERR, "ERROR! writer file_path string");
		return 1;
	}
	
	fd = open(argv[1], O_RDWR | O_CREAT, S_IRWXU| S_IRWXG);
	if (fd == -1)
	{
		perror("open");
		syslog(LOG_ERR, "writer application failed to open the file %s", argv[1]);
		return 1;
	}
	
	ret = write(fd, argv[2], strlen(argv[2]));
	if (ret == -1)
	{
		perror("write");
		syslog(LOG_ERR, "Writer application failed to write %s to the file %s", argv[2], argv[1]);
		return 1;
	}

	write(fd, "\n", 1);

	syslog(LOG_DEBUG, "Writing %s to %s", argv[2], argv[1]);
	closelog();
	return 0;
}

