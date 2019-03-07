#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdbool.h>
#include "header.h"


int main(int argc, char *argv[]) {

    int sockfd, sockfd_udp, newsockfd, portno, clilen;
    char buffer[BUFLEN], last_command[20];
    struct sockaddr_in serv_addr, cli_addr;
    int n, i, j, socket_idx, N;
    int size;
    
    User* last_user = (User*) malloc(sizeof(User));

	srand(time(NULL));

	fd_set read_fds;	//multimea de citire folosita in select()
	fd_set tmp_fds;	    //multime folosita temporar 
	int fdmax;		    //valoare maxima file descriptor din multimea read_fds

	if (argc < 3) {
		fprintf(stderr,"Usage : %s port users_data_file\n", argv[0]);
		exit(1);
	}

	FILE* in_file = fopen(argv[2], "r");
	fscanf(in_file, "%d", &N);
	User* users = (User*)malloc(sizeof(User) * N); 

	initialize_users(users, in_file, N);

	for (i = 0; i < N; i++) {
		printf("%s %s %s %s %s %.2lf\n", users[i].last_name, 
						users[i].first_name, users[i].card_no,
						users[i].pin_code, users[i].password,
						users[i].sold);
	}
	// Golim multimea de descriptori de citire (read_fds) si multimea tmp_fds 
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	// Creez socket TCP
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) 
		error("-10 : Eroare la apel socket");

	// Setare struct sockaddr_in pentru a specifica unde trimit datele
	portno = atoi(argv[1]);

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;	// foloseste adresa IP a masinii
	serv_addr.sin_port = htons(portno);
	size = sizeof(serv_addr);

	// Legare proprietati de socket
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0) 
		error("-10 : Eroare la apel bind");

	listen(sockfd, MAX_CLIENTS);
	
	// Creez socket UDP
	sockfd_udp = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_udp < 0)
        error("-10 : Eroare la apel socket");

    // Legare proprietati de socket
    if (bind(sockfd_udp, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("-10 : Eroare la apel bind");


	// Adaugam noul file descriptor (socketul pe care se asculta conexiuni) 
	// in multimea read_fds
   	FD_SET(STDIN_FILENO, &read_fds);
   	FD_SET(sockfd, &read_fds);
	FD_SET(sockfd_udp, &read_fds);
    fdmax = sockfd_udp;
 
	while (1) {
		tmp_fds = read_fds; 
		if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1) 
			error("-10 : Eroare la apel select");
		
		for(socket_idx = 0; socket_idx <= fdmax; socket_idx++) {
			User cur_user;
			// A venit ceva pe socketul inactiv(cel cu listen) = o noua 
			// conexiune actiunea serverului: accept()
			if (FD_ISSET(socket_idx, &tmp_fds) && socket_idx == sockfd) {
				clilen = sizeof(cli_addr);
				if ((newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr,
										 &clilen)) == -1) {
					error("-10 : Eroare la apel accept");
				} 
				else {
					// Adaug noul socket intors de accept() la multimea 
					// descriptorilor de citire
					FD_SET(newsockfd, &read_fds);
					if (newsockfd > fdmax) { 
						fdmax = newsockfd;
					}
				}
				printf("Noua conexiune de la %s, port %d, socket_client %d\n", 
												inet_ntoa(cli_addr.sin_addr), 
												ntohs(cli_addr.sin_port), 
												newsockfd);
			}	
			// Citesc de la tastatura
			else if (FD_ISSET(socket_idx, &tmp_fds) 
						&& socket_idx == STDIN_FILENO) {
				scanf("%s", buffer);
				if (strcmp(buffer, "quit") == 0) {
					close(sockfd);
				    close(sockfd_udp);
				   	fclose(in_file);	
				    return 0; 
				}
			} 
			// Primesc date de la socketul UDP
			else if (FD_ISSET(socket_idx, &tmp_fds) 
						&& socket_idx == sockfd_udp) { 
				
				int user_idx;
                memset(buffer, 0, BUFLEN);
                n = recvfrom(sockfd_udp, buffer, BUFLEN, 0, 
                			(struct sockaddr *) &serv_addr, &size);

                if (n > 0) {
                	execute_unlock(users, buffer, &user_idx, N);					
					if (n = sendto(sockfd_udp, buffer, strlen(buffer),
                                          0, (struct sockaddr *) &serv_addr,
                                          sizeof(serv_addr)) < 0) {
                        error("-10 : Eroare la apel sendto");
                    }
                }
            } 
            // Primesc date de la socketi TCP
			else if (FD_ISSET(socket_idx, &tmp_fds)){
				
				memset(buffer, 0, BUFLEN);
				if ((n = recv(socket_idx, buffer, sizeof(buffer), 0)) <= 0) {
					if (n == 0) {
						//conexiunea s-a inchis
						printf("selectserver: socket %d hung up\n", socket_idx);
					} else {
						error("-10 : Eroare la apel recv");
					}
					close(socket_idx); 
					FD_CLR(socket_idx, &read_fds);
				} 
				else { //recv intoarce >0
					printf ("Am primit de la clientul de pe socketul ");
					printf ("%d, mesajul: %s\n", socket_idx, buffer);
					
					if (!strcmp(buffer, "quit")) {
						for (i = 0; i < N; i++) {
							// Deconectam userul corepunzator clientului 
							// de la care am primit quit
							if (users[i].client == socket_idx) {
								users[i].trials = 0;
								users[i].open_session = false;
								users[i].client = -1;
								break;
							} 
						}
						close(socket_idx); 
						FD_CLR(socket_idx, &read_fds);	
						continue;				
					}
					char* command = strtok(buffer, " ");
					// Executam comada de login 
					if (strcmp(command, "login") == 0) {
						char* card_no = strtok(NULL, " ");
						char* pin = strtok(NULL, " ");

						int user_idx = get_user_idx(users, card_no, N);
						execute_login(users, user_idx, buffer, pin, &cur_user,
										socket_idx, last_user, N);						
					}
					// Executam comada de logout si restam valorile pentru user
					else if (strcmp(command, "logout") == 0) {
						int user_idx = get_user_idx(users, cur_user.card_no, N);
						users[user_idx].trials = 0;
						users[user_idx].open_session = false;
						users[user_idx].client = -1;
						sprintf(buffer, "IBANK> Clientul a fost deconectat\n");
					}
					// Afisam sold-ul pentru userul logat  
					else if (strcmp(command, "listsold") == 0) {
						int user_idx = get_user_idx(users, cur_user.card_no, N);
						sprintf(buffer, "IBANK> %.2lf\n", users[user_idx].sold);	
					} 
					else if (strcmp(command, "transfer") == 0) {
						char* card_no = strtok(NULL, " ");
						char* token = strtok(NULL, " ");
						double sum = atof(token);
						// Verificam daca transferul se poate realiza
						int user_idx = get_user_idx(users, card_no, N);
						int cur_user_idx = get_user_idx(users, cur_user.card_no, N);
						verify_transfer(users, user_idx, buffer, sum, 
										cur_user_idx, last_command);
					}
					else if (strcmp(last_command, "transfer") == 0) {
						// In functie de raspunsul primit de la client 
						// finalizam transferul
						finalize_transfer(users, command, last_command, N,
											socket_idx);					
					}
					send_message(buffer, socket_idx); 				
				}
			}
		}
    }

    close(sockfd);
    close(sockfd_udp);
   	fclose(in_file);	
    return 0; 
}