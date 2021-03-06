/*
 * security server
 *
 * Copyright (c) 2000 - 2010 Samsung Electronics Co., Ltd.
 * Contact: Bumjin Im <bj.im@samsung.com>
 *
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/param.h>
#include <fcntl.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/socket.h>
#include "security-server.h"
#include "test.h"

int *g_permissions = NULL;
gid_t *g_groups = NULL;
int g_perm_num, g_group_num;

char *object_label = NULL;
char *subject_label = NULL;
char *access_rights = NULL;

/*
 * 0 - old mode (uid/gid)
 * 1 - new mode (SMACK labels)
 */
int mode_new = 0;

void printusage(char *cmdline)
{
	printf("%s\n", "Usage: ");
	printf("%s -u uid -g gid1 gid2 gid3... -p gid_a gid_b gid_c ...\n", cmdline);
        printf("%s\n", "or:");
	printf("%s -s subject -o object -a access-rights\n", cmdline);
	printf("%s\n", "[Options]");
	printf("%s\n", "-u: UID that the process are running as");
	printf("%s\n", "    Only one UID is allowed.");
	printf("%s\n", "-g: GIDs that the process belongs to");
	printf("%s\n", "-p: GIDs that the process wants to get privilege");
	printf("%s\n", "-s: subject label (label of the process)");
	printf("%s\n", "-o: object label to be accessed");
	printf("%s\n", "-a: accessed rights requested (one or more of the letterrs rwx)");
	printf("%s\n", "Examples:");
	printf("%s -u 5000 -g 6001 6002 6003 6004 6005 6006 6007 -p 6001 6002 6010\n", cmdline);
	printf("%s -s mylabel -o objlabel -a rx\n", cmdline);
}

int privilege_control(int argc, char *argv[])
{

        if (argc == 7 && !strcmp(argv[1], "-s"))
        {
            return privilege_control_new(argc, argv);
        }
        else
        {
            return privilege_control_old(argc, argv);
        }
}

int privilege_control_old(int argc, char *argv[])
{
	int option = 0; /* 0: no, 1: uID, 2: gid, 3: permission */
	int uid_flag = 0, gid_flag= 0, perm_flag = 0, i = 1, number, uid = 0, j;

	while(i < argc)
	{
		if(strcmp(argv[i], "-u") == 0)
		{
			if(uid_flag != 0)
			{
				printf("%s\n", "-u option already used");
				printusage(argv[0]);
				exit(1);
			}
			option = 1;
			uid_flag = 1;
		}
		else if (strcmp(argv[i], "-g") == 0)
		{
			if(gid_flag != 0)
			{
				printf("%s\n", "-g option already used");
				printusage(argv[0]);
				exit(1);
			}
			option = 2;
			gid_flag = 1;
		}
		else if (strcmp(argv[i], "-p") == 0)
		{
			if(perm_flag != 0)
			{
				printf("%s\n", "-p option already used");
				printusage(argv[0]);
				exit(1);
			}
			option = 3;
			perm_flag = 1;
		}
		else
		{
			errno = 0;
			number = strtoul(argv[i], 0, 10);
			if(errno != 0)
			{
				printf("%s\n", "Invalid option");
				printusage(argv[0]);
				exit(1);
			}
			switch(option)
			{
				case 1:
					if(uid != 0)
					{
						printf("%s\n", "You cannot assign more than 1 uID");
						printusage(argv[0]);
						exit(1);
					}
					uid = number;
					break;
				case 2:
					for(j=0;i<g_group_num;j++)
					{
						if(number == g_groups[j])
							break;
					}
					g_groups = (gid_t *)realloc(g_groups, sizeof(gid_t) * (++g_group_num));
					g_groups[g_group_num -1] = number;
					break;
				case 3:
					for(j=0;i<g_perm_num;j++)
					{
						if(number == g_permissions[j])
							break;
					}
					g_permissions = (int *)realloc(g_permissions, sizeof(int) * (++g_perm_num));
					g_permissions[g_perm_num -1] = number;
					break;
				default:
					printf("%s\n", "Invalid option");
					printusage(argv[0]);
					exit(1);
					break;
			}
		}
		i++;
	}
	if(g_group_num == 0 || g_perm_num == 0)
	{
		printf("%s\n", "You must assign groups and permissions");
		printusage(argv[0]);
		exit(1);
	}
	if(setgroups(g_group_num, g_groups) != 0)
	{
		printf("%s\n", "Error on setgroups{}");
		exit(1);
	}

	setgid(uid);
	setuid(uid);
	return 0;
}

int privilege_control_new(int argc, char *argv[])
{
    if (argc == 7 && !strcmp(argv[1], "-s") &&
                     !strcmp(argv[3], "-o") &&
                     !strcmp(argv[5], "-a"))
    {
        int fd, len, ret; int e;
        mode_new = 1;
        subject_label = argv[2];
        object_label = argv[4];
        access_rights = argv[6];
        fd = open("/proc/self/attr/current", O_WRONLY|O_TRUNC, 0);
        if (fd < 0)
        { 
            printf("Cannot set my own smack label... maybe I'm not root?");
            exit(1);
        }
        len = strlen(subject_label);
        ret = write(fd, subject_label, len);
        if (ret != len)
        { 
            e = errno;
            printf("(2)Cannot set my own smack label... maybe I'm not root?");
            exit(1);
        }
        close(fd);
        setgid(1);
        setuid(1);
    }
    else
    {
        printusage(argv[0]);
        exit(1);
    }

    return 0;
}

int connect_to_testserver()
{
	struct sockaddr_un clientaddr;
	int client_len = 0, localsockfd, ret;

	/* Create a socket */
	if((localsockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		printf("%s\n", "Error on socket()");
		return 0;
	}

	bzero(&clientaddr, sizeof(clientaddr));
	clientaddr.sun_family = AF_UNIX;
	strncpy(clientaddr.sun_path, SECURITY_SERVER_TEST_SOCK_PATH, strlen(SECURITY_SERVER_TEST_SOCK_PATH));
	clientaddr.sun_path[strlen(SECURITY_SERVER_TEST_SOCK_PATH)] = 0;
	client_len = sizeof(clientaddr);
	if(connect(localsockfd, (struct sockaddr*)&clientaddr, client_len) < 0)
	{
		printf("%s\n", "Error on connect");
		close(localsockfd);
		return 0;
	}
	return localsockfd;
}

void printhex(unsigned char *data, int size)
{
	int i;
	for(i=0;i<size;i++)
	{
		if(data[i] < 0x10)
			printf("0");

		printf("%X ", data[i]);
		if(((i+1) % 16) == 0 && i != 0)
			printf("\n");
	}
	printf("\n");
}


int send_request(int sock_fd, unsigned char *cookie, int perm)
{
	unsigned char buf[28] = {0, 0, 0, 0, };
	int size;
	memcpy(buf + 4, cookie, 20);
	memcpy(buf + 24, &perm, sizeof(int));
	size = write(sock_fd, buf, 28);
	if(size < 28)
	{
		printf("Cannot send\n");
		close(sock_fd);
		exit(1);
	}
	return 0;
}

/*
 * @param direct 0=via security server 1=directly from IPC socket
 */
int send_request_new_cookie(int sock_fd,
                            const char *cookie,
                            const char *subject_label,
                            const char *access_rights)
{
    unsigned char buf[1024] = {17, 0, 0, 0, };
    int olen, alen;
    int size, ret;
    olen = strlen(subject_label);
    alen = strlen(access_rights);
    size = 24+2*sizeof(int)+olen+alen;
    memcpy(buf + 4, cookie, 20);
    memcpy(buf + 24, &olen, sizeof(int));
    memcpy(buf + 28, &alen, sizeof(int));
    memcpy(buf + 32, subject_label, olen);
    memcpy(buf + 32 + olen, access_rights, alen);
    ret = write(sock_fd, buf, size);
    if(ret < size)
    {
    	printf("Cannot send\n");
    	close(sock_fd);
    	exit(1);
    }
    return 0;
}

int send_request_new_direct(int sock_fd,
                            const char *object_label,
                            const char *access_rights)
{
    unsigned char buf[1024] = {17, 0, 0, 1, };
    int olen, alen;
    int size, ret;
    olen = strlen(object_label);
    alen = strlen(access_rights);
    size = 24+2*sizeof(int)+olen+alen;
    memcpy(buf + 4, &olen, sizeof(int));
    memcpy(buf + 8, &alen, sizeof(int));
    memcpy(buf + 12, object_label, olen);
    memcpy(buf + 12 + olen, access_rights, alen);
    ret = write(sock_fd, buf, size);
    if(ret < size)
    {
    	printf("Cannot send\n");
    	close(sock_fd);
    	exit(1);
    }
    return 0;
}

int recv_result(int sock_fd)
{
	int buf, size;
	size = read(sock_fd, &buf, sizeof(int));
	if(size < sizeof(int))
	{
		printf("Cannot recv\n");
		close(sock_fd);
		exit(1);
	}
	return buf;
}



int main(int argc, char *argv[])
{
	int ret, sock_fd, i, cur_pid, cnt;
	unsigned char cookie[20];
	char tmpchar[100];

	g_group_num = 0;
	g_perm_num = 0;

	ret = getuid();
	if(ret != 0)
	{
		printf("You must be root to test. Current UID: %d\nExiting...\n", ret);
		exit(1);
	}

	if(argc < 2)
	{
		printf("%s\n", "Error: No group identified");
		printusage(argv[0]);
		exit(1);
	}

	system("touch /opt/home/root/pid_cycle");
	privilege_control(argc, argv);

	printf("Strting test. My PID is %d\n", getpid());

	printf("TC C1: security_server_get_cookie_size()\n");
	ret = security_server_get_cookie_size();
	if(ret != 20)
	{
		printf("TC C1 failed. %d\n", ret);
		exit(1);

	}
	printf("TC C1: PASSED\n\n");
	sleep(1);

	printf("TC C2: security_server_request_cookie(): normal case\n");
	ret = security_server_request_cookie(cookie, 20);
	if(ret != SECURITY_SERVER_API_SUCCESS)
	{
		printf("TC C2 failed. %d\n", ret);
		exit(1);
	}
	printhex(cookie, 20);
	printf("TC C2: PASSED\n\n");
	sleep(1);

	printf("TC C3: security_server_request_cookie(): Too small buffer size\n");
	ret = security_server_request_cookie(cookie, 10);
	if(ret != SECURITY_SERVER_API_ERROR_BUFFER_TOO_SMALL)
	{
		printf("TC C3 failed. %d\n", ret);
		exit(1);
	}
	printf("TC C3: PASSED\n\n");
	sleep(1);

	printf("TC C4: security_server_check_privilege(): client is not allowed\n");
        if (mode_new)
        {
            printf("TC C4: SKIPPED because new mode (SMACK-based) was selected.\n");
        }
        else
        {
	    ret = security_server_check_privilege(cookie, g_permissions[0]);
	    if(ret != SECURITY_SERVER_API_ERROR_AUTHENTICATION_FAILED)
	    {
	    	printf("TC C4 failed. %d\n", ret);
	    	exit(1);
	    }
	    printf("TC C4: PASSED\n\n");
	    sleep(1);
        }

	printf("TC C5: security_server_get_gid(): client is not allowed\n");
	ret = security_server_get_gid("telephony");
	if(ret != SECURITY_SERVER_API_ERROR_AUTHENTICATION_FAILED)
	{
		printf("TC C5 failed. %d\n", ret);
		exit(1);
	}
	printf("TC C5: PASSED\n\n");
	sleep(1);

	printf("TC C6: security_server_get_object_name(): client is not allowed\n");
        if (mode_new)
        {
            printf("TC C6: SKIPPED because new mode (SMACK-based) was selected.\n");
        }
        else
        {
	    ret = security_server_get_object_name(g_groups[0], tmpchar, sizeof(tmpchar));
	    if(ret != SECURITY_SERVER_API_ERROR_AUTHENTICATION_FAILED)
	    {
	    	printf("TC C6 failed. %d\n", ret);
	    	exit(1);
	    }
	    printf("TC C6: PASSED\n\n");
	    sleep(1);
        }

	printf("TC C7: Requesting access to test server via security server\n");
        if (mode_new)
        {
            printf("object:  >%s<\n", object_label);
            printf("subject: >%s<\n", subject_label);
            printf("access:  >%s<\n", access_rights);

            sock_fd = connect_to_testserver();
    	    if(sock_fd < 1)
    	    {
    	    	printf("Socket connection error\n");
    	    	exit(1);
    	    }
    	    ret = send_request_new_cookie(sock_fd, cookie, object_label, access_rights);
    	    if(ret != 0)
    	    {
    	    	printf("send failed\n");
    	    	close(sock_fd);
    	    	exit(1);
    	    }
    	    ret = recv_result(sock_fd);
    	    if(ret == SECURITY_SERVER_API_SUCCESS)
    	    {
    	    	printf("Permission granted\n\n");
    	    }
    	    else if(ret == SECURITY_SERVER_API_ERROR_ACCESS_DENIED)
    	    {
    	    	printf("Permission denied\n\n");
    	    }
    	    else
    	    {
    	    	printf("Recv error\n");
    	    	close(sock_fd);
    	    	exit(1);
    	    }
    	    close(sock_fd);
    	    sock_fd = 0;
    	    sleep(1);
        }
        else
        {
    	    for(i=0;i<g_perm_num;i++)
    	    {
    	    	sock_fd = connect_to_testserver();
    	    	if(sock_fd < 1)
    	    	{
    	    		printf("Socket connection error\n");
    	    		exit(1);
    	    	}
    	    	printf("%d: requesting %d permission...sockfd=%d\n", i, g_permissions[i], sock_fd);
    	    	ret = send_request(sock_fd, cookie, g_permissions[i]);
    	    	if(ret != 0)
    	    	{
    	    		printf("send failed\n");
    	    		close(sock_fd);
    	    		exit(1);
    	    	}
    
    	    	ret = recv_result(sock_fd);
    	    	if(ret == SECURITY_SERVER_API_SUCCESS)
    	    	{
    	    		printf("Permission granted\n\n");
    	    	}
    	    	else if(ret == SECURITY_SERVER_API_ERROR_ACCESS_DENIED)
    	    	{
    	    		printf("Permission denied\n\n");
    	    	}
    	    	else
    	    	{
    	    		printf("Recv error\n");
    	    		close(sock_fd);
    	    		exit(1);
    	    	}
    	    	close(sock_fd);
    	    	sock_fd = 0;
    	    }
    	    sleep(1);
        }

	printf("TC C7a: Requesting access to test server with direct IPC\n");
        if (mode_new)
        {
            printf("subject: >%s<\n", subject_label);
            printf("object:  >%s<\n", object_label);
            printf("access:  >%s<\n", access_rights);

            sock_fd = connect_to_testserver();
    	    if(sock_fd < 1)
    	    {
    	    	printf("Socket connection error\n");
    	    	exit(1);
    	    }
    	    ret = send_request_new_direct(sock_fd, object_label, access_rights);
    	    if(ret != 0)
    	    {
    	    	printf("send failed\n");
    	    	close(sock_fd);
    	    	exit(1);
    	    }
    	    ret = recv_result(sock_fd);
    	    if(ret == SECURITY_SERVER_API_SUCCESS)
    	    {
    	    	printf("Permission granted\n\n");
    	    }
    	    else if(ret == SECURITY_SERVER_API_ERROR_ACCESS_DENIED)
    	    {
    	    	printf("Permission denied\n\n");
    	    }
    	    else
    	    {
    	    	printf("Recv error\n");
    	    	close(sock_fd);
    	    	exit(1);
    	    }
    	    close(sock_fd);
    	    sock_fd = 0;
    	    sleep(1);
        }
        else
        {
            printf("TC C7a: SKIPPED because old mode (uid/gid-based) was selected.\n");
        }

	printf("TC 08: Requesting cookie for same PID with different path\n");
	printf("       Exiting this process to cycle different process as same PID.\n");
	printf("       Please look at the test server's terminal for the result\n");
	cur_pid = getpid();
	sock_fd = connect_to_testserver();
	if(sock_fd < 1)
	{
		printf("Socket connection error\n");
		exit(1);
	}

	unsigned char buf[32] = {255, 255, 255, 255, };
	int size;

	memcpy(buf + 4, cookie, 20);
	memcpy(buf + 24, &cur_pid, sizeof(int));
	ret = getuid();
	memcpy(buf + 28, &ret, sizeof(int));

	size = write(sock_fd, buf, 32);
	if(size < 32)
	{
		printf("Cannot send\n");
		close(sock_fd);
		exit(1);
	}
	if(sock_fd > 0)
		close(sock_fd);
	if(g_groups != NULL)
		free(g_groups);
	if(g_permissions != NULL)
		free(g_permissions);
	return 0;
}

