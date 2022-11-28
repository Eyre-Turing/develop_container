#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define UNIX_SOCK_PATH	"/tmp/et_container_sock"

#define BUF_SIZE	1024
#define PATH_SIZE	260

void print_usage(const char *self_name)
{
	fprintf(stdout, "Usage:\n"
		"  %s help                  show this message and exit.\n"
		"  %s off                   close the container\'s daemon service.\n"
		"  %s ps                    show all running container.\n"
		"  %s run <image_path>      run a container, <image_path> must be full path.\n"
		"  %s stop <id>             stop a running container.\n"
		"  %s fg <id> <shell_cmd>   exec a <shell_cmd> in front office.\n"
		"  %s bg <id> <shell_cmd>   exec a <shell_cmd> in background.\n",
		self_name, self_name, self_name, self_name, self_name, self_name, self_name);
}

int get_mountpoint(int sockfd, const char *id, char *mountpoint)
{
	char buf[BUF_SIZE];
	int len = 0;
	int n;
	char *line;
	sprintf(buf, "info\n%s\n\n", id);
	send(sockfd, buf, strlen(buf), 0);
	while (1) {
		n = recv(sockfd, buf + len, BUF_SIZE - 1, 0);
		if (n <= 0) break;
		len += n;
		buf[len] = 0;
	}
	line = strtok(buf, "\n");
	if (line == NULL || strcmp(line, "failed") == 0) return 1;
	line = strtok(NULL, "\n");
	if (line == NULL) return 1;
	line = strtok(NULL, "\n");
	if (line == NULL) return 1;
	strcpy(mountpoint, line);
	return 0;
}

int main(int argc, char *argv[])
{
	int sockfd;
	struct sockaddr_un addr;
	char buf[BUF_SIZE];
	int n;
	char mountpoint[PATH_SIZE];
	if (argc < 2) {
		print_usage(argv[0]);
		return 1;
	}
	if (strcmp(argv[1], "help") == 0) {
		print_usage(argv[0]);
		return 0;
	}
	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("socket");
		return 1;
	}
	memset(&addr, 0, sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, UNIX_SOCK_PATH);
	if (connect(sockfd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)) == -1) {
		perror("connect");
		close(sockfd);
		return 1;
	}

	if (strcmp(argv[1], "off") == 0) {
		send(sockfd, "off\n\n", 5, 0);
		while (1) {
			n = recv(sockfd, buf, BUF_SIZE - 1, 0);
			if (n <= 0) break;
			buf[n] = 0;
			fprintf(stdout, "%s", buf);
		}
		close(sockfd);
		return 0;
	}
	if (strcmp(argv[1], "ps") == 0) {
		send(sockfd, "list\n\n", 6, 0);
		while (1) {
			n = recv(sockfd, buf, BUF_SIZE - 1, 0);
			if (n <= 0) break;
			buf[n] = 0;
			fprintf(stdout, "%s", buf);
		}
		close(sockfd);
		return 0;
	}
	if (strcmp(argv[1], "run") == 0) {
		if (argc != 3) {
			fprintf(stderr, "param error\n");
			close(sockfd);
			return 1;
		}
		sprintf(buf, "run\n%s\n\n", argv[2]);
		send(sockfd, buf, strlen(buf), 0);
		while (1) {
			n = recv(sockfd, buf, BUF_SIZE - 1, 0);
			if (n <= 0) break;
			buf[n] = 0;
			fprintf(stdout, "%s", buf);
		}
		close(sockfd);
		return 0;
	}
	if (strcmp(argv[1], "stop") == 0) {
		if (argc != 3) {
			fprintf(stderr, "param error\n");
			close(sockfd);
			return 1;
		}
		sprintf(buf, "stop\n%s\n\n", argv[2]);
		send(sockfd, buf, strlen(buf), 0);
		while (1) {
			n = recv(sockfd, buf, BUF_SIZE - 1, 0);
			if (n <= 0) break;
			buf[n] = 0;
			fprintf(stdout, "%s", buf);
		}
		close(sockfd);
		return 0;
	}
	if (strcmp(argv[1], "fg") == 0) {
		if (argc < 4) {
			fprintf(stderr, "param error\n");
			close(sockfd);
			return 1;
		}
		if (get_mountpoint(sockfd, argv[2], mountpoint) != 0) {
			fprintf(stderr, "failed to get mountpoint\n");
			close(sockfd);
			return 1;
		}
		close(sockfd);
		if (chroot(mountpoint) != 0) {
			perror("chroot");
			return 1;
		}
		if (chdir("/") != 0) {
			perror("chdir");
			return 1;
		}
		execvp(argv[3], argv + 3);
		perror("execvp");
		return 1;
	}
	if (strcmp(argv[1], "bg") == 0) {
		if (argc < 4) {
			fprintf(stderr, "param error\n");
			close(sockfd);
			return 1;
		}
		if (get_mountpoint(sockfd, argv[2], mountpoint) != 0) {
			fprintf(stderr, "failed to get mountpoint\n");
			close(sockfd);
			return 1;
		}
		close(sockfd);
		if (fork() == 0) {
			if (chroot(mountpoint) != 0) _exit(1);
			if (chdir("/") != 0) _exit(1);
			execvp(argv[3], argv + 3);
			perror("execvp");
			_exit(1);
		}
		return 0;
	}

	fprintf(stderr, "unspport '%s'\n", argv[1]);
	close(sockfd);
	return 1;
}
