#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h> 
#include <arpa/inet.h>
#include <stdbool.h>
#include "header.h"

int main(int argc, char *argv[]) {
	struct sockaddr_in serv_addr;
	struct sockaddr_in to_station;
	struct hostent *server;

	fd_set read_fds;
	fd_set tmp_fds;
	
	int sockfd, sockfd_udp, n, size;	
	bool open_session = false, open_transfer = false;
	char last_card_no[7], last_command[20];

	int fdmax, j;
	char buffer[BUFLEN], client_log[20];;

	if (argc < 3) {
		fprintf(stderr,"Usage %s server_address server_port\n", argv[0]);
		exit(0);
	} 

	// Deschid fisierul client_log
	sprintf(client_log, "%s%d%s", "client-", getpid(), ".log");
	FILE* log_file = fopen(client_log, "w");
	
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);
	
	// Creez socket TCP
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) 
		 error("-10 : Eroare la apel socket");

	// Setare struct sockaddr_in pentru a specifica unde trimit datele
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[2]));
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	size = sizeof(serv_addr);

	if (connect(sockfd,(struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) 
		 error("-10 : Eroare la apel connect");   

	// Creez socket UDP
	sockfd_udp = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd_udp < 0) 
		 error("-10 : Eroare la apel socket");

	// Setare struct sockaddr_in pentru a specifica unde trimit datele
	uint32_t port = htonl(atoi(argv[2]));
	to_station.sin_family = AF_INET;
	to_station.sin_port = port;
	struct in_addr myaddr;
	inet_aton(argv[1], &myaddr);
	to_station.sin_addr = myaddr;
	
	// Legare proprietati de socket
	if (bind(sockfd_udp, (struct sockaddr*) &to_station, sizeof(to_station))< 0)
        error("-10 : Eroare la apel bind udp");

	FD_SET(0, &read_fds);
	FD_SET(sockfd, &read_fds);
	FD_SET(sockfd_udp, &read_fds);
	fdmax = sockfd_udp;

	while(1){   // Citesc de la tastatura
		tmp_fds = read_fds;

		if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1) 
			error("-10 : Eroare la apel select");
		
		for (j = 0; j <= fdmax; j++) {
			// Citesc comenzi 
			if (FD_ISSET(j, &tmp_fds) && j == 0) {  
				memset(buffer, 0, BUFLEN);
				fgets(buffer, BUFLEN-1, stdin);
				fwrite(buffer, 1, strlen(buffer), log_file);
				buffer[strlen(buffer) - 1] = '\0';

				if (strcmp(buffer, "quit") == 0) {
					n = send(sockfd, buffer, strlen(buffer), 0);
					close(sockfd);
					close(sockfd_udp);
					fclose(log_file);
					return 0;
				}
				// Se incearca o noua conexiune login in timp ce alta este deschisa
				if (strstr(buffer, "login") != NULL && open_session == true) {
					sprintf(buffer, "IBANK> -2 : Sesiune deja deschisa\n");
					printf("%s", buffer);
					fwrite(buffer, 1, strlen(buffer), log_file);
				}
				// Vrem sa deblocam ultimul card introdus
				else if (strstr(buffer, "unlock") != NULL) {
					sprintf(buffer, "unlock %s", last_card_no);
					// Trimit comanda catre server pe socketul UDP
					if (n = sendto(sockfd_udp, buffer, strlen(buffer), 0,
						(struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
	                    error("-10 : Eroare la apel sendto");
	                }
				} 
				// Trimitem parola secreta in cazul in care ultima comanda 
				// trimisa catre server a fost "unlock"
				else if (strcmp(last_command, "unlock") == 0) {
					if (n = sendto(sockfd_udp, buffer, strlen(buffer), 0,
						(struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
	                    error("-10 : Eroare la apel sendto");
	                }
				}
				// Se incearca executarea unei comenzi dar nu exista nicio 
				// sesiune deschisa
				else if (strstr(buffer, "login") == NULL 
						&& strstr(buffer, "quit") == NULL 
						&& open_transfer == false
						&& open_session == false) {
					sprintf(buffer,"IBANK> -1 : Clientul nu este autentificat\n");
					printf("%s", buffer);
					fwrite(buffer, 1, strlen(buffer), log_file);
				} 
				// Se trimite orice comanda diferita de "unlock" catre server
				// pe socketul TCP
				else if (strstr(buffer, "unlock") == NULL) {
					if (strstr(buffer, "logout") != NULL) {
						open_transfer = false;
						open_session = false;
					} 
					else if (strstr(buffer, "login") != NULL) {
						char* aux = (char*) malloc(strlen(buffer));
						strcpy(aux, buffer);
						char* token = strtok(aux, " ");
						token = strtok(NULL, " ");
						strcpy(last_card_no, token);
					} 
					n = send(sockfd, buffer, strlen(buffer), 0);
				} 
			}
			// Primesc raspuns de la server de pe socket TCP
			if (FD_ISSET(j, &tmp_fds) && j == sockfd) {
				memset(buffer, 0, BUFLEN);
				n = recv(sockfd, buffer, sizeof(buffer), 0);
				if (n == 0)
				  	return 1;
				else {
				  	if (strstr(buffer,"Welcome") != NULL) {
				  		open_session = true;
				  	}
				  	if (strstr(buffer,"Transfer") != NULL) {
				  		open_transfer = true;
				  	}
				  	if (strstr(buffer,"Transfer realizat cu succes") != NULL
				  		&& strstr(buffer,"-9 : Operatie anulata") != NULL) {
				  		open_transfer = false;
				  	}
				  	printf("%s", buffer);
				  	fwrite(buffer, 1, strlen(buffer), log_file);
				}
			}
			// Primesc raspuns de la server de pe socket UDP
			if (FD_ISSET(j, &tmp_fds) && j == sockfd_udp) {
				memset(buffer, 0, BUFLEN);

                n = recvfrom(sockfd_udp, buffer, BUFLEN, 0,
                                (struct sockaddr *) &serv_addr, &size);
                if (n > 0) {
                	if (strstr(buffer, "Trimite parola secreta") != NULL) {
				  		strcpy(last_command, "unlock");
				  	} 
				  	else {
				  		strcpy(last_command, "");
				  	}
                	printf("%s", buffer);
                	fwrite(buffer, 1, strlen(buffer), log_file);
                }
			}
		}
	}
	close(sockfd);
	close(sockfd_udp);
	fclose(log_file);
	return 0;
}

