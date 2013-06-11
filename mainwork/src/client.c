#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdlib.h>
#include "timer.h"
#include <unistd.h>
//#include <readline/readline.h>
//#include <readline/history.h>

#define WRITE 1
#define READ 0
#define IN 0
#define OUT 1
#define ERR 2

#define SERVER_MSG "[tcp-client] "
#define DEBUG_PRINT(fmt, ...) printf(SERVER_MSG fmt, ##__VA_ARGS__)

void usage() {
  printf(
    "usage: tcp-client [OPTIONS] FILE\n"
    "\n"
    "OPTIONS:\n"
    "  -a                                IP address\n"
    "  -p                                Port number\n"
    "  -H                                See this message\n"
    );
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char** argv) {
	/* 1. cmd line args */
	char *addr = "127.0.0.1";
	char *port = "4500";
	char *fname = 0;
	int opt, option_index;
	while( (opt = getopt(argc, argv, "a:p:H")) != -1 ) {
		switch(opt) {
		case 'a': addr = optarg; break;
		case 'p': port = optarg; break;
		case 'H': usage(); exit(0); break;
		default:  usage(); exit(1); break;
		}
	}
	while (optind < argc) {
		if (!fname) {
		  fname = argv[optind++];
		} else {
		  printf("Error: extra arguments on command line.\n\n");
		  usage();
		  exit(1);
		}
	}
	if (!fname) {
    	printf("Error: Please supply a file!\n\n");
    	usage();
    	exit(1);
  	}
	DEBUG_PRINT("input address = %s\n", addr);
	DEBUG_PRINT("input port    = %s\n", port);
	DEBUG_PRINT("file name 	   = %s\n", fname);
	DEBUG_PRINT("\n");

	/* 2. data structures */
	struct addrinfo hints, *server_info;
	memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	/* 3. info */
	int rv;
	if( (rv = getaddrinfo(addr, port, &hints, &server_info)) != 0) {
		DEBUG_PRINT("Error: getaddrinfo: %s\n", gai_strerror(rv));
		exit(EXIT_FAILURE);
	}

	/* 4. server socket */
	int server_socket;
	struct addrinfo *p;
	for(p = server_info; p != NULL; p = p->ai_next) {
		if( (server_socket = socket(p->ai_family, p->ai_socktype,
									p->ai_protocol)
			) == -1 ) {
			DEBUG_PRINT("Error: Failed to open socket\n");
			continue;
		}
		if(connect(server_socket, p->ai_addr, p->ai_addrlen) == -1) {
			close(server_socket);
			DEBUG_PRINT("Error: Failed to connect socket\n");
			continue;
		}
		break;
	}
	if(p == NULL) {
		DEBUG_PRINT("Error: Client failed to connect!\n");
		exit(EXIT_FAILURE);
	}
	char ip[50];
	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *) p->ai_addr),
			  ip, sizeof(ip));
	DEBUG_PRINT("Connected to %s\n", ip);

	freeaddrinfo(server_info);

	/* 5. file */
	FILE *client_file = fopen(fname, "r");
	if(!client_file) {
		DEBUG_PRINT("Error: Failed to open %s\n", fname);
		exit(EXIT_FAILURE);
	}

	/* 6. send */
	int max_len = 1024;
	char packet[max_len];
	int packet_len;
	char ack[max_len];
	int ack_len;
	int bytes_sent;
	int bytes_total = 0;
	start_timeofday();
	start_timer();

	/* */
	char sendline[1024],recvline[1024];
	pid_t childpid;
	int childToParent[2];
	int ParentTochild[2];
	if(pipe(ParentTochild) == -1)
	{
		perror("ParentTochild pipe initialization error");
		exit(-1);
	}
	if(pipe(childToParent) == -1)
	{
		perror("childToParent pipe initialization error");
		exit(-1);
	}

	while( (packet_len = fread(packet, sizeof(char), max_len, client_file)) > 0 ) {

		if(fork() == 0) {
			close(ParentTochild[WRITE]);
			dup2(server_socket, OUT);
			dup2(ParentTochild[READ], IN);
			close(ParentTochild[READ]);
			close(server_socket);
			execl("/bin/gzip","gzip","-c",NULL);
		} else {
			close(ParentTochild[READ]);
			write(ParentTochild[WRITE], packet, packet_len);
			close(ParentTochild[WRITE]);
		}

		// if(fork() == 0) {
		// 	dup2(server_socket, IN);
		// 	//dup2(childToParent[WRITE], OUT);
		// 	dup2(OUT, childToParent[WRITE]);
		// 	//dup2(childToParent[WRITE], ERR);
		// 	close(childToParent[READ]);
		// 	close(childToParent[WRITE]);
		// 	close(server_socket);

		// 	// char buf[1024];
		// 	// int len = read(IN, buf, 1024);
		// 	// write(OUT, buf, len);

		// 	execl("/bin/gzip","gzip","-df",NULL);
		// } else {
		// 	//close(childToParent[WRITE]);
		// 	close(server_socket);
		// 	char buf[1024];
		// 	int len = read(childToParent[WRITE], buf, 1024);
		// 	printf("read len = %d\n", len);
		// 	fputs(buf,stdout);
		// 	close(childToParent[WRITE]);
		// 	close(childToParent[READ]);
		// }

		// bytes_sent = send(server_socket, packet, packet_len, 0);

		//close(ParentTochild[READ]);
		// dup2(server_socket,ParentTochild[WRITE]);
		// write(ParentTochild[WRITE],"abc",3);
	    //close(ParentTochild[WRITE]);
	    // close(server_socket);

		// if(read(ParentTochild[WRITE],recvline,1024) > 0) {
		// 	fputs(recvline, stdout);
		// }
			// perror("Server terminated ");
		// fputs(recvline,stdout);


		// if((childpid = fork()) == 0)
		// {
		// 	fputs("Child Process\n",stdout);
		// 	// close(server_socket);
		// 	close(ParentTochild[WRITE]);
		// 	dup2(ParentTochild[READ],stdin);
		// 	close(ParentTochild[READ]);
		// 	exit(0);
		// }

		break;

		//ack_len = recv(server_socket, ack, max_len, 0);
		bytes_total += bytes_sent;
	}
	DEBUG_PRINT("Transfered %d bytes\n", bytes_total);
	DEBUG_PRINT("Elapsed time %f\n", end_timer());
	DEBUG_PRINT("Elapsed time %f\n", end_timeofday());

	/* 7. shutdown */
	close(server_socket);
}
