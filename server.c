#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <netinet/tcp.h>
#include <math.h>
#include "helpers.h"

// contine informatii despre un abonat (client TCP)
struct subscriber{
	char id[11]; 	// id-ul abonatului
	int sf[100];	// retine tipurile de abonare (tipul de abonare pe o pozitie x este pentru topicul de pe pozitia x)
	char topics[100][50]; // retine topicurile la care este abonat clientul
	int topic_number;	// numarul de topicuri la care este abonat
	char missed_msg[100][2000]; // retine mesajele pe care trebuia sa le primeasca dupa reconectare
	int missed_msg_number; // numarul de mesaje pe care trebuie sa le primeasca la reconectare
	int sock_number; // retine socket-ul pentru comunicarea dintre client si servere; -1 = deconectat
};

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	int sockfd, sock_udp, newsockfd, portno;
	struct sockaddr_in serv_addr, cli_addr;
	int ret, r, s;
	socklen_t clilen;
	
	char buffer[BUFLEN];

	struct subscriber subscribers[MAX_CLIENTS]; // retine toti abonatii
	
	for (int i = 0; i < MAX_CLIENTS; i++) {
		// se initializeaza numarul de topicuri cu -1 pentru a stii daca pe o anumita pozitie se afla un client sau nu
		subscribers[i].topic_number = -1;
	}
	
	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea read_fds

	if (argc < 2) {
		usage(argv[0]);
	}

	// se goleste multimea de descriptori de citire (read_fds) si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	sockfd = socket(AF_INET, SOCK_STREAM, 0); // socket pentru tcp
	DIE(sockfd < 0, "socket");

	sock_udp = socket(AF_INET, SOCK_DGRAM, 0); // socket pentru udp
	DIE(sock_udp < 0, "socket");

	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");

	// se completeaza informatiile despre socketii tcp si udp 
	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	// se asociaza socketii de TCP si UDP cu portul ales
	ret = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind");

	ret = bind(sock_udp, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind");

	// socket-ul de TCP devine pasiv
	ret = listen(sockfd, MAX_CLIENTS);
	DIE(ret < 0, "listen");

	// se adauga noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
	FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(sockfd, &read_fds);
	FD_SET(sock_udp, &read_fds);
	
	// se calculeaza fdmax
	if (sockfd > sock_udp) {
		fdmax = sockfd;
	} else {
		fdmax = sock_udp;
	}


	bool not_exit = true;
	// se face loop pana cand se primeste comanda de exit
	while (not_exit) {
		tmp_fds = read_fds; 
		
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		for (int i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				if (i == STDIN_FILENO) {
					// se primeste comanda de la tastatura
					memset(buffer, 0, BUFLEN);
					fgets(buffer, BUFLEN, stdin);

					// se verifica comanda de exit
					if (strncmp(buffer, "exit", 4) == 0) {
						not_exit = false;
						break;
					}
				} else if (i == sockfd) {
					// a venit o cerere de conexiune pe socket-ul inactiv

					// server-ul accepta conexiunea
					clilen = sizeof(cli_addr);
					newsockfd = accept(i, (struct sockaddr *)&cli_addr, &clilen);
					DIE(newsockfd < 0, "accept");

					// se dezativeaza algoritmul lui Neagle
					int flag = 1;
					ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
					DIE(ret < 0, "Neagle");

					// noul socket este adaugat in multime
					FD_SET(newsockfd, &read_fds);
					// se recalculeaza fdmax
					if (newsockfd > fdmax) {
						fdmax = newsockfd;
					}

					// se primeste id-ul clientului
					memset(buffer, 0, BUFLEN);
					r = recv(newsockfd, buffer, BUFLEN, 0);
					DIE(r < 0, "recv");

					// se cauta in vector prima pozitie libera din vector
					for (int j = 0; j < MAX_CLIENTS; j++) {
						if (subscribers[j].topic_number == -1) {
							
							// s-a gasit o pozitie libera din vector si nu exista alti clienti cu acelasi id
							printf("New client %s connected from %s:%d\n", buffer, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
							// se initializeaza datele despre client
							strcpy(subscribers[j].id, buffer);
							subscribers[j].sock_number = newsockfd;
							subscribers[j].topic_number = 0;
							subscribers[j].missed_msg_number = 0;
							break;
						} else {

							if (strcmp(subscribers[j].id, buffer) == 0) {
								
								// s-a gasit un client cu acelasi id
								if (subscribers[j].sock_number != -1) {
									
									// clientul este activ
									printf("Client %s already connected.\n", buffer);
									close(newsockfd);
									FD_CLR(newsockfd, &read_fds);
									break;

								} else {

									// s-a gasit un client cu acelasi id care este dezactivat

									printf("New client %s connected from %s:%d\n", buffer, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

									// se activeaza clientul
									subscribers[j].sock_number = newsockfd;

									if (subscribers[j].missed_msg_number > 0) {

										// se trimit mesajele generate atunci cand clientul era dezactivat
										for (int k = 0; k < subscribers[j].missed_msg_number; k++) {
											memset(buffer, 0, BUFLEN);
			 								memcpy(buffer, subscribers[j].missed_msg[k], sizeof(subscribers[j].missed_msg[k]));
											s = send(newsockfd, buffer, BUFLEN, 0);
											DIE(s < 0, "send");
										}
										for (int k = 0; k < subscribers[j].missed_msg_number; k++) {
											memset(subscribers[j].missed_msg[k], 0, 2000);
										}

										subscribers[j].missed_msg_number = 0;
									}

									break;
								}
							}

						}
					} 

				} else if (i == sock_udp) {

				// s-a primit mesaj pe socket-ul de UDP

			 	clilen = sizeof(cli_addr);
			 	memset(buffer, 0, BUFLEN);
			 	memset((char *) &cli_addr, 0, clilen);
			 	r = recvfrom(i, buffer, BUFLEN, 0, (struct sockaddr *)&cli_addr, &clilen);
			 	DIE(r < 0, "recv");

			 	// se extrage topicul
			 	char topic[50];
			 	memset(topic, 0, 50);
			 	strncpy(topic, buffer, 50);

			 	char message[2000];
			 	memset(message, 0, sizeof(message));
			 	
			 	// se completeaza mesajul cu adresa ip, portul si topicul
			 	strcpy(message, inet_ntoa(cli_addr.sin_addr));
			 	message[strlen(message)] = ':';
			 	char port_str[11];
			 	snprintf(port_str, sizeof(port_str), "%d", ntohs(cli_addr.sin_port));
			 	strcat(message, port_str);
			 	strcat(message, " - ");
			 	strcat(message, topic);
			 	strcat(message, " - ");

			 	// se extrage tipul de date
			 	int type = (int8_t)buffer[50];

			 	if (type == 0) {

			 		// tip de date INT
			 		strcat(message, "INT - ");
					int sign = buffer[51];
			 		int value;
					memcpy(&value, buffer + 52, sizeof(value));
			 		value = ntohl(value);
					if (sign) {
			 			value = value * (-1);
			 		}
					
					char str[50];
					memset(str, 0, 50);
					snprintf(str, sizeof(str), "%d", value);
			 		strcat(message, str);

			 	} else if (type == 1) {

			 		// tip de date SHORT REAL
			 		strcat(message, "SHORT_REAL - ");
					uint16_t payload;
			 		memcpy(&payload, buffer + 51, sizeof(payload));
			 		float value = (float) ntohs(payload) / 100;
					
					char str[50];
					memset(str, 0, 50);
					snprintf(str, sizeof(str), "%.2f", value);
			 		strcat(message, str);

			 	} else if (type == 2) {

			 		// tip de date FLOAT
			 		strcat(message, "FLOAT - ");

			 		int sign = buffer[51];
			 		uint8_t exp = buffer[56];
					int base;
			 		memcpy(&base, buffer + 52, sizeof(base));			 		
			 		base = ntohl(base);
					float val = base;
					base = 1;
					while(exp > 0) {
			 			exp--;
			 			base = base * 10;
			 		}
			 		
			 		val = val / base;

			 		if (sign) {
			 			val = val * (-1);
			 		}

			 		char str[50];
			 		memset(str, 0, 50);
					snprintf(str, sizeof(str), "%lf", val);
			 		strcat(message, str);

			 	} else if (type == 3) {

			 		// tip de date STRING
			 		strcat(message, "STRING - ");

			 		char value[1500];
			 		memset(value, 0, 1499);
			 		memcpy(value, buffer + 51, 1499);
					strcat(message, value);
				}

				message[strlen(message)] = '\0';

			 	memset(buffer, 0, BUFLEN);
			 	memcpy(buffer, message, sizeof(message));

			 	// se parcurg clientii
			 	for (int j = 0; j < MAX_CLIENTS; j++) {
			 		if (subscribers[j].topic_number != -1) {

			 			// se verifica daca este activ
			 			if (subscribers[j].sock_number != -1) {

			 				// se parcurg topicurile la care clientul este abonat
			 				for (int k = 0; k < subscribers[j].topic_number; k++) {
			 					if (strncmp(topic, subscribers[j].topics[k], strlen(topic)) == 0) {
			 						// se trimite mesajul
			 						s = send(subscribers[j].sock_number, buffer, BUFLEN, 0);
			 						DIE(s < 0, "send");
			 						break;
			 					}
			 				}

			 			} else  {

			 				// clientul este inactiv
							for (int k = 0; k < subscribers[j].topic_number; k++) {
			 					if (strncmp(topic, subscribers[j].topics[k], strlen(topic)) == 0 && subscribers[j].sf[k] == 1) {
			 						// clientul este abonat la un topic cu SF = 1 si se salveaza mesajul pentru a il trimite la reconectare
			 						int curr = subscribers[j].missed_msg_number;
			 						memset(subscribers[j].missed_msg[curr], 0, 2000);
			 						memcpy(subscribers[j].missed_msg[curr], buffer, 2000);
			 						subscribers[j].missed_msg_number++;
			 					}
			 				}
			 			}
			 		}
			 	}

			} else {
				// a venit o comanda de la clientul TCP

				memset(buffer, 0, BUFLEN);
				r = recv(i, buffer, BUFLEN, 0); 
				DIE(r < 0, "recv");
				if (r == 0) {

					// clientul s-a dezactivat
					// se afla ce client s-a dezactivat
					for (int j = 0; j < MAX_CLIENTS; j++) {
						if (i == subscribers[j].sock_number) {
							printf("Client %s disconnected.\n", subscribers[j].id);
							subscribers[j].sock_number = -1; // il setez ca deconectat
							// inchid socket-ul si il scot din multime
							close(i);
							FD_CLR(i, &read_fds);
							break;
						}
					}

				} else {

					// se extrage comanda
					char command[12];
					memset(command, 0, sizeof(command));
					memcpy(command, buffer, sizeof(command) - 1);

					if (strncmp(command, "subscribe", 9) == 0) {

						// comanda de subscribe
						// aflu clientul care a trimis comanda
						for (int j = 0; j < MAX_CLIENTS; j++) {
							if (subscribers[j].sock_number == i) {
								
								// extrag topicul la care clientul vrea sa se aboneze
								char subscribed_topic[50];
								memset(subscribed_topic, 0, 50);
								strncpy(subscribed_topic, buffer + 10, strlen(buffer) - 9 - 2); 
								subscribed_topic[strlen(subscribed_topic) - 1] = '\0';
								int topic_idx = subscribers[j].topic_number;

								for (int k = 0; k < subscribers[j].topic_number; k++) {
									
									// se verifica daca e abonat la topicul respectiv
									if (strncmp(subscribers[j].topics[k], subscribed_topic, strlen(subscribed_topic)) == 0) {
										topic_idx = k; // salvez pozitia pe care se afla topicul
										break;
									}
								}
								if (topic_idx == subscribers[j].topic_number) {
									// clientul se aboneaza la topic
									strncpy(subscribers[j].topics[topic_idx], subscribed_topic, sizeof(subscribed_topic));
									subscribers[j].topics[topic_idx][strlen(subscribers[j].topics[topic_idx])] = '\0';
									subscribers[j].sf[topic_idx] = atoi(strrchr(buffer, ' '));
									subscribers[j].topic_number++;
								} else {
									// clientul s-a abonat la topic si isi schimba tipul de abonare
									subscribers[j].sf[topic_idx] = atoi(strrchr(buffer, ' '));
								}

								break;
							}
						}

					} else if (strncmp(command, "unsubscribe", 11) == 0) {

						// comanda de unsubscribe
						// aflu clientul care a trimis comanda 
						for (int j = 0; j < MAX_CLIENTS; j++) {
							if (subscribers[j].sock_number == i) {
								if (subscribers[j].topic_number > 0) {

									// extrag topicul la care se dezaboneaza
									char unsubscribed_topic[50];
									memset(unsubscribed_topic, 0, 50);
									strncpy(unsubscribed_topic, buffer + 12, strlen(buffer) - 11);
									
									// aflu pozitia topicului din vector
									int topic_idx = -1;
									for (int k = 0; k < subscribers[j].topic_number; k++) {
										if (strcmp(unsubscribed_topic, subscribers[j].topics[k]) == 0) {
											topic_idx = k;
											break;
										}
									}

									if (topic_idx != -1) {

										// se elimina topicul 
										for (int k = topic_idx + 1; k < subscribers[j].topic_number; k++) {
											subscribers[j].sf[k - 1] = subscribers[j].sf[k];
											strcpy(subscribers[j].topics[k - 1], subscribers[j].topics[k]);
										}
										subscribers[j].topic_number--;
									}
								}
							}
						}
					}
				}
			}
		}
	}
	
	}
	
	close(sock_udp);
	close(sockfd);
	return 0;
}
