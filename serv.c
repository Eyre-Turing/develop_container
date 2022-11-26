#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <time.h>
#include <string.h>

#include <uuid/uuid.h>

#define PATH_SIZE	260
#define CMD_SIZE	1024
#define UNIX_SOCK_PATH	"/tmp/et_container_sock"
#define CMD_TIMEOUT	10

struct container_list_t {
	char image_path[PATH_SIZE];
	char mountpoint[PATH_SIZE];
	char id[64];
	struct container_list_t *next;
} container_list = {"", "", "", NULL};

/*
 * result:
 * 0 ok
 * 1 no such image
 * 2 mount failed
 * */
int mount_container(const char *image_path, char *mountpoint)
{
	char shell_cmd[1024];
	FILE *fp;
	int n;
	if (access(image_path, F_OK) != 0) return 1;
	sprintf(shell_cmd, "mktemp -d");
	fp = popen(shell_cmd, "r");
	if (fp == NULL) return 2;
	n = fread(mountpoint, 1, PATH_SIZE - 1, fp);
	if (pclose(fp) != 0) return 2;
	mountpoint[n] = 0;
	strtok(mountpoint, "\n");
	sprintf(shell_cmd, "mount \"%s\" \"%s\"", image_path, mountpoint);
	fp = popen(shell_cmd, "r");
	if (fp == NULL) return 2;
	if (pclose(fp) != 0) return 2;
	return 0;
}

/*
 * result:
 * 0 ok
 * 1 failed
 * */
int umount_container(const char *mountpoint)
{
	char shell_cmd[1024];
	FILE *fp;
	int n;
	sprintf(shell_cmd, "umount \"%s\"", mountpoint);
	fp = popen(shell_cmd, "r");
	if (fp == NULL) return 1;
	if (pclose(fp) != 0) return 1;
	sprintf(shell_cmd, "rm -rf \"%s\"", mountpoint);
	fp = popen(shell_cmd, "r");
	if (fp) pclose(fp);
	return 0;
}

void do_run_container(int sockfd)
{
	struct container_list_t *item;
	uuid_t id_gen;
	int mount_ret;
	char *image_path = strtok(NULL, "\n");
	if (image_path == NULL) {
		send(sockfd, "failed\nno image_path spec\n", 26, 0);
		return ;
	}
	item = (struct container_list_t *) malloc(sizeof(struct container_list_t));
	if (item == NULL) {
		send(sockfd, "failed\nmalloc failed\n", 21, 0);
		return ;
	}
	strcpy(item->image_path, image_path);
	uuid_generate(id_gen);
	uuid_unparse(id_gen, item->id);
	mount_ret = mount_container(item->image_path, item->mountpoint);
	if (mount_ret == 1) {
		send(sockfd, "failed\nno such image\n", 21, 0);
		return ;
	} else if (mount_ret == 2) {
		send(sockfd, "failed\nmount failed\n", 20, 0);
		return ;
	}
	item->next = container_list.next;
	container_list.next = item;
	send(sockfd, "ok\n", 3, 0);
	send(sockfd, item->id, strlen(item->id), 0);
	send(sockfd, "\n", 1, 0);
}

void do_stop_container(int sockfd)
{
	struct container_list_t *item, *prev;
	int flag = 1;
	char *id = strtok(NULL, "\n");
	if (id == NULL) {
		send(sockfd, "failed\nno id spec\n", 18, 0);
		return ;
	}
	for (item = container_list.next, prev = &container_list; item; item = item->next, prev = prev->next) {
		if (strcmp(item->id, id) != 0) continue;
		if (umount_container(item->mountpoint) != 0) {
			send(sockfd, "failed\numount failed\n", 21, 0);
			return ;
		}
		prev->next = item->next;
		free(item);
		flag = 0;
		break;
	}
	send(sockfd, "ok\n", 3, 0);
	if (flag) {
		send(sockfd, "but the container you spec no exist\n", 36, 0);
	}
}

void do_list_container(int sockfd)
{
	struct container_list_t *item;
	send(sockfd, "ok\n", 3, 0);
	for (item = container_list.next; item; item = item->next) {
		send(sockfd, item->id, strlen(item->id), 0);
		send(sockfd, "\t", 1, 0);
		send(sockfd, item->image_path, strlen(item->image_path), 0);
		send(sockfd, "\n", 1, 0);
	}
}

void do_info_container(int sockfd)
{
	struct container_list_t *item;
	char *id = strtok(NULL, "\n");
	if (id == NULL) {
		send(sockfd, "failed\nno id spec\n", 18, 0);
		return ;
	}
	for (item = container_list.next; item; item = item->next) {
		if (strcmp(item->id, id) != 0) continue;
		send(sockfd, "ok\n", 3, 0);
		send(sockfd, item->image_path, strlen(item->image_path), 0);
		send(sockfd, "\n", 1, 0);
		send(sockfd, item->mountpoint, strlen(item->mountpoint), 0);
		send(sockfd, "\n", 1, 0);
		return ;
	}
	send(sockfd, "failed\nthe container you spec no exist\n", 39, 0);
}

/*
 * ----stop service-----
 * >>> request
 * off
 * >>> reponse
 * ok
 * ---------------------
 * 
 * ----run container----
 * >>> request
 * run
 * <image_path>
 * >>> succeed reponse
 * ok
 * <id>
 * >>> failed reponse
 * failed
 * <message>
 * ---------------------
 *
 * ----stop container---
 * >>> request
 * stop
 * <id>
 * >>> succeed reponse
 * ok
 * [message]
 * >>> failed reponse
 * failed
 * <message>
 * ---------------------
 *
 * ----list-------------
 * >>> request
 * list
 * >>> succeed reponse
 * ok
 * <message>
 * ---------------------
 *
 * ----get info---------
 * >>> request
 * info
 * <id>
 * >>> succeed reponse
 * ok
 * <image_path>
 * <mountpoint>
 * >>> failed reponse
 * failed
 * <message>
 * ---------------------
 *
 * */
int do_cli_cmd(int sockfd, char *cmd)
{
	char *line;
	line = strtok(cmd, "\n");
	if (line) {
		if (strcmp(line, "off") == 0) {
			send(sockfd, "ok\n", 3, 0);
			return 1;
		} else if (strcmp(line, "run") == 0) {
			do_run_container(sockfd);
		} else if (strcmp(line, "stop") == 0) {
			do_stop_container(sockfd);
		} else if (strcmp(line, "list") == 0) {
			do_list_container(sockfd);
		} else if (strcmp(line, "info") == 0) {
			do_info_container(sockfd);
		} else {
			send(sockfd, "failed\nunsupport command\n", 25, 0);
		}
	}
	return 0;
}

int recv_cli_cmd(int cli_sockfd)
{
	char cli_cmd[CMD_SIZE] = {0};
	ssize_t cli_cmd_len = 0;
	ssize_t n = 0;
	time_t start_time = time(NULL);
	while (cli_cmd_len < 2 || cli_cmd[cli_cmd_len - 1] != '\n' || cli_cmd[cli_cmd_len - 2] != '\n') {
		if (difftime(time(NULL), start_time) > CMD_TIMEOUT) return 0;
		if (cli_cmd_len >= CMD_SIZE) return 0;
		n = recv(cli_sockfd, cli_cmd + cli_cmd_len, CMD_SIZE - cli_cmd_len - 1, MSG_DONTWAIT);
		if (n < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
			return 0;
		}
		if (n == 0) return 0;
		cli_cmd_len += n;
		cli_cmd[cli_cmd_len] = 0;
	}
	return do_cli_cmd(cli_sockfd, cli_cmd);
}

int main()
{
	int serv_sockfd, cli_sockfd;
	struct sockaddr_un serv_addr, cli_addr;
	socklen_t cli_addr_size;

	serv_sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (serv_sockfd == -1) {
		fprintf(stderr, "create unix socket failed.\n");
		return 1;
	}
	memset(&serv_addr, 0, sizeof(struct sockaddr_un));
	serv_addr.sun_family = AF_UNIX;
	strcpy(serv_addr.sun_path, UNIX_SOCK_PATH);
	if (bind(serv_sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr_un)) == -1) {
		perror("bind failed");
		close(serv_sockfd);
		return 1;
	}
	if (listen(serv_sockfd, 50) == -1) {
		perror("listen failed");
		close(serv_sockfd);
		return 1;
	}
	if (daemon(0, 0) != 0) {
		perror("daemon failed");
		close(serv_sockfd);
		return 1;
	}
	cli_addr_size = sizeof(struct sockaddr_un);
	while (1) {
		cli_sockfd = accept(serv_sockfd, (struct sockaddr *) &cli_addr, &cli_addr_size);
		if (cli_sockfd == -1) {
			continue;
		}
		if (recv_cli_cmd(cli_sockfd) != 0) {
			close(cli_sockfd);
			break;
		}
		close(cli_sockfd);
	}
	close(serv_sockfd);
	unlink(UNIX_SOCK_PATH);
	return 0;
}
