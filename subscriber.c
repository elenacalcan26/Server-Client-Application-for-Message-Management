#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>
#include <netinet/tcp.h>
#include "helpers.h"

void usage(char *file)
{
	fprintf(stderr, "Usage: %s id server_address server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	int sockfd, s, ret, r;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];

	if (argc < 4) {
		usage(argv[0]);
	}


	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea read_fds

	// se goleste multimea de descriptori de citire (read_fds) si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	// se creeaza socket-ul TCP pentru comunicarea cu serevr-ul
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	// se completeaza informatii despre socket-ul TCP
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");

	s = send(sockfd, argv[1], sizeof(argv[1]), 0);
	DIE(s < 0, "send");	
	
	// se dezactiveaza algoritmul lui Nagle
	int flag = 1;
	ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
	DIE(ret < 0, "Nagle");
	
	// se adauga in multime socket-ul pentru stdin si pentru server
	FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(sockfd, &read_fds);
	fdmax = sockfd;

	bool not_exit = true;
	// se face loop pana cand se primeste comanda de exit
	while (not_exit) {

		tmp_fds = read_fds;
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(select < 0, "select");
		for (int i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				if (i == STDIN_FILENO) {

					// se primeste o comanda de la stdin
					memset(buffer, 0, BUFLEN);
					fgets(buffer, BUFLEN, stdin);

					if (strncmp(buffer, "exit", 4) == 0) {
						// comanda este de exit si anunta server-ul
						s = send(sockfd, NULL, 0, 0);

						// se intrerupe comunicatie intre client si server
						ret = shutdown(sockfd, SHUT_RDWR);
						DIE(ret < 0, "shutdown");
						not_exit = false;
						break;
					}
					
					// a primit o alta comanda
					char copy_command[BUFLEN];
					strcpy(copy_command, buffer);
					char *token;
					token = strtok(copy_command, " ");

					buffer[strlen(buffer) - 1] = '\0';

					// se verifica daca comanda pe care a primit-o subscriber-ul este corecta
					if (strncmp(token, "subscribe", 9) == 0) {
						token = strtok(NULL, " ");

						if (token == NULL) {
							fprintf(stderr, "Topic is missing\n");
							continue;

						} else {

							token = strtok(NULL, " ");

							if (token == NULL) {
								fprintf(stderr, "SF is missing\n");
								continue;
							
							} else if (atoi(token) != 0 && atoi(token) != 1) {
								fprintf(stderr, "SF must be 0 or 1\n");
								continue;
							} 

						}

					}  else if (strncmp(token, "unsubscribe", 11) == 0) {

						token = strtok(NULL, " ");

						if (token == NULL) {
							fprintf(stderr, "Topic is missing\n");
							continue;
						}

					} else {
						fprintf(stderr, "Wrong command\n");
						continue;	
					}

					// trimite comanda catre server
					s = send(sockfd, buffer, sizeof(buffer), 0);
					DIE(s < 0, "send");

					// se afiseaza mesajul de feedback in functie de comanda primita
					if (strcmp(token, "subscribe") == 0) {
						printf("Subscribed to topic.\n");
					} else {
						printf("Unsubscribed from topic.\n");
					}

				} else if (i == sockfd) {
					
					// primeste mesaj de la server

					memset(buffer, 0, BUFLEN);
					r = recv(sockfd, buffer, BUFLEN, 0);
					DIE(r < 0, "recv");

					if (r == 0) {
						// server-ul s-a inchis
						not_exit = false;
						break; 
					} 

					// se afiseaza mesajul primit
					printf("%s\n", buffer);
				}
			}
		}
	}

	close(sockfd);

	return 0;
}
