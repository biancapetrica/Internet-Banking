#ifndef AUXILIARY
#define AUXILIARY

#define MAX_CLIENTS	99999
#define BUFLEN 256

typedef struct {
    char last_name[13];
    char first_name[13];
    char card_no[7];
    char pin_code[5];
    char password[9];
    double sold;
    int trials;
    bool open_session; // contul este deschis de un anumit client
    bool open_unlock;
    bool open_transfer;  // se realizeaza un transfer
    int transfer_to;	 // catre user-ul cu acest index in vectorul users 
    double transfer_sum; // cu suma 
    bool card_blocked;
    int client; // clientul care este logat pe acest cont
}User;

void error(char *msg) {
    perror(msg);
    exit(1);
}

void initialize_users(User* users, FILE* in_file, int N) {
	int i;
	for (i = 0; i < N; i++) {
		fscanf(in_file, "%s %s %s %s %s %lf",users[i].last_name, 
						users[i].first_name, users[i].card_no,
						users[i].pin_code, users[i].password,
						&users[i].sold);
		users[i].trials = 0;
		users[i].open_session = false;
		users[i].card_blocked = false;
		users[i].open_transfer = false;
		users[i].client = -1;
    	users[i].transfer_to = -1;
   		users[i].transfer_sum = 0;
   		users[i].open_unlock = false;
	}
}

int get_user_idx(User* users, char* card_no, int n){
	int i;
	for (i = 0; i < n; i++) {
		if (strcmp(users[i].card_no, card_no) == 0) {
			return i;
		} 
	}
	return -1;
}

// Trimit comanda catre server pe socketul TCP
void send_message(char* buffer, int socket_idx){
	int n;
	if (n = send(socket_idx, buffer, strlen(buffer), 0) < 0) {
		 error("-10 : Eroare la apel send");
	}
}

void execute_login(User* users, int user_idx, char* buffer, char* pin, 
					User* cur_user, int client, User* last_user, int n) {
	if (user_idx == -1) {
		sprintf(buffer, "IBANK> -4 : Numar card inexistent\n");
	} 
	else {
		if (users[user_idx].open_session == false
			&& users[user_idx].card_blocked == false) {
			if (users[user_idx].client != client) {
				users[user_idx].client = client;
				users[user_idx].trials = 0;
			} 
			// Se reseteaza contorul pentru nr de incercari al ultimului card 
			// cu care am incercat sa ne logam daca introducem un alt card 
			if (last_user != NULL 
				  && strcmp(last_user->card_no, users[user_idx].card_no) != 0) {
				int idx = get_user_idx(users, last_user->card_no, n);
				users[idx].trials = 0;
			}
		}
		if (users[user_idx].open_session == true) {
			// Exista o sesiune deschisa pentru acest numar de card
			sprintf(buffer, "IBANK> -2 : Sesiune deja deschisa\n");
		} 
		else if (users[user_idx].card_blocked == true) {
			// Cardul este blocat
			sprintf(buffer, "IBANK> -5 : Card blocat\n");
		} 
		else if (strcmp(users[user_idx].pin_code, pin) != 0) {
			// Cardul nu este blocat si nici nu exista o sesiune deschisa
			// pentru el, dar pinul introdus este gresit
			users[user_idx].trials++;
			if (users[user_idx].trials > 2) {
				users[user_idx].card_blocked = true;
				sprintf(buffer, "IBANK> -5 : Card blocat\n");
			} else {
				sprintf(buffer, "IBANK> -3 : Pin gresit\n");
			}
		} 
		else if (strcmp(users[user_idx].pin_code, pin) == 0) {
			// Cardul nu este blocat si nici nu exista o sesiune deschisa
			// pentru el, pinul introdus este corect si deschide o sesiune noua
			users[user_idx].open_session = true;
			users[user_idx].trials = 0;
			users[user_idx].client = client;
			*cur_user = users[user_idx];
			sprintf(buffer, "IBANK> Welcome %s %s\n", 
							users[user_idx].last_name, 
							users[user_idx].first_name);
		}
	}
	*last_user = users[user_idx];
}

void verify_transfer(User* users, int user_idx, char* buffer, double sum, 
					 int cur_user_idx, char* last_command) {
	if (user_idx == -1) {
		sprintf(buffer, "IBANK> -4 : Numar card inexistent\n");
		strcpy(last_command, "");
	} 
	else {
		if (sum > users[cur_user_idx].sold) {
			sprintf(buffer, "IBANK> -8 : Fonduri insuficiente\n");
			strcpy(last_command, "");
		} 
		else {
			// Setam aceste variabile pentru a sti dupa ce primim aprobarea de 
			// la client din ce cont dorim sa facem transferul si catre cine 
			users[cur_user_idx].open_transfer = true;
			users[cur_user_idx].transfer_sum = sum;
			users[cur_user_idx].transfer_to = user_idx;
			int temp = (int) sum;
			if (sum == temp) 
				sprintf(buffer, "IBANK> Transfer %d catre %s %s? [y/n]\n", temp, 
					users[user_idx].last_name, users[user_idx].first_name);
			else
				sprintf(buffer, "IBANK> Transfer %.2lf catre %s %s? [y/n]\n", 
												sum, users[user_idx].last_name, 
												users[user_idx].first_name);
			strcpy(last_command, "transfer");
		}
	}
}

void finalize_transfer(User* users, char* buffer, char* last_command, int N,
						int socket_idx) {
	int i = N;
	if (buffer[0] != 'y') {
		sprintf(buffer, "IBANK> -9 : Operatie anulata\n");
	} 
	else {
		for (i = 0; i < N; i++) {
			// Cautam sesiunea de transfer ce trebuie realizata
			if (users[i].client == socket_idx 
				&& users[i].open_transfer == true){
				users[i].sold -= users[i].transfer_sum;
				users[users[i].transfer_to].sold += users[i].transfer_sum;
				sprintf(buffer, "IBANK> Transfer realizat cu succes\n");
				break; 
			}
		}
	}
	if (i != N) {
		// Resetam contoarele
		users[i].open_transfer == false;
		users[i].transfer_sum = 0;
		users[i].transfer_to = -1;
	}
	strcpy(last_command, "");
}

void execute_unlock(User* users, char* buffer, int* user_idx, int N) {
	char* command = strtok(buffer, " ");

	if (strcmp(command, "unlock") == 0) {
		char* card_no = strtok(NULL, " ");
		// Caut user-ul cu card_no primit ca parametru
		*user_idx = get_user_idx(users, card_no, N);

		if (*user_idx == -1) {
			sprintf(buffer, "UNLOCK> -4 : Numar card inexistent\n");
		} 
		else {
			// Exista deja deschisa o sesiune de unlock pentru acest card
			if (users[*user_idx].open_unlock == true)
				sprintf(buffer, "UNLOCK> -7 : Deblocare esuata\n");
			// Cardul nu este blocat
			else if (users[*user_idx].card_blocked == false)
				sprintf(buffer, "UNLOCK> -6 : Operatie esuata\n");
			else {
				users[*user_idx].open_unlock = true;
				sprintf(buffer, "UNLOCK> Trimite parola secreta\n");
			}
		}
	} else {
		users[*user_idx].open_unlock = false; // Inchid sesiunea de unlock
		if (strcmp(users[*user_idx].password, buffer) == 0) {
			// Am primit parola secreta buna, deblochez cardul si resetez
			// numarul de incercari gresite  
        	users[*user_idx].card_blocked = false;
        	users[*user_idx].trials = 0;
        	sprintf(buffer, "UNLOCK> Card deblocat\n");	
        } 
        else { // Nu am primit parola secreta corecta
        	sprintf(buffer, "UNLOCK> -7 : Deblocare esuata\n");
        }
	}
} 
#endif