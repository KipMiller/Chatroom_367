/* Program 3 server 
 * Michael Montgomery | Chris Miller 
 * CSCI 367 
 */
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <math.h>

#define BACKLOG 6 /* size of request queue */
#define LIMIT 255 /* maximum number of participants / observers */ 
int visits = 0; /* counts client connections */
int num_observers = 0;

/*------------------------------------------------------------------------
* Program: Server (based on the original demo server code provided in class) 
*
* Purpose: Support 255 participants, receive messages from participants and 
*           send private messages between participants. Also establish 
*           observers (up to 255 concurrently) which affiliate to a participant.
*
* Syntax: ./program3_server participant_port observer_port
*
* participant_port - the port on which the server will listen for participants (uint16_t)
* observer_port - the port on which the server listens for observers (uint16_t)
*
*------------------------------------------------------------------------
*/

// Function that will return 0 if the name is invalid, 1 if it is.
int isValid(char username[]){
	if(strlen(username) > 10){
		printf("Name is too long (1-10 characters)\n");
		return 0;
	}
	for(int i = 0; i < strlen(username); i++){
		if(!((username[i] >= 'a' && username[i] <= 'z') || (username[i] >= 'A' && username[i] <= 'Z') || (username[i] >= '0' && username[i] <= '0') || username[i] == '_')){
			return 0;
		}
	}
	return 1;
}

// Structure to hold the userNames of each user and 
// store them in an array of the type user. 
struct user 
{
	char userName[10];
	char status;
	int observer;
	time_t enterTime;
	time_t startTime; 
	time_t endTime;
  // Y = username is valid (not taken and valid characters)
  // T = Username already taken (resets timer) 
  // I = username is invalid (does not reset timer)
};

struct user users[LIMIT];// Index of the array storing usernames is the SD
struct user observer[LIMIT];
int client_sockets[LIMIT];// keeps track of every client SD
int o_sockets[LIMIT];

// Function to check if a username is used or not, if it is not in the used 
// list return 1, else return 0.
int isName(char username[]){
	for(int i = 0; i < LIMIT; i++){
		if(strcmp(users[i].userName,username) == 0){
			return 0;// if it IS a name, return 0
		}  
	}
	return 1;// if it is NOT a name, return 1
}

// Function to check if a username already has an observer or not 
// returns 0 if there is no observer, -1 if there is an observer and 1 
// if the name is not found in the list
int isObserver(char username[]){
	for(int i = 0; i < LIMIT; i++){
		if(strcmp(users[i].userName,username) == 0){
				if(users[i].observer == 0){
					users[i].observer = 1;
					return 0;// if it IS a name, return 0
				} else {
					return -1;
				}
		}  
	}
	return 1;// if it is NOT a name, return 1
}	

// function to remove the observer flag from a participant
void removeObserver(char username[]){
	for(int i = 0; i < LIMIT; i++){
		if(strcmp(users[i].userName,username) == 0){
			users[i].observer = 0;
		}  
	}
}


// Function to send a message to every observer
void send_All(char* msg, fd_set readfds){ 
	int osd;
	uint16_t msgLen = strlen(msg);
	char message[msgLen];
	strcpy(message, msg);// Copy the message into a new variable of appropriate size 

	for(int i = 0; i < LIMIT; i++){
		osd = o_sockets[i];
		if(FD_ISSET(osd, &readfds)){
			send(osd, &msgLen, sizeof(msgLen), 0);// send the length of the message
			send(osd, &message, sizeof(message), 0);// send the message
		}
	}
}

// Function that ONLY sends a message to both the target and sender's observer
void private_send(char* msg, char senderName[], char targName[], fd_set readfds){
	int osd;
	uint16_t msgLen = strlen(msg);
	char message[msgLen];
	strcpy(message, msg);// Copy the message into a new variable of appropriate size 
  
	for(int i = 0; i < LIMIT; i++){
		osd = o_sockets[i];
		if(FD_ISSET(osd, &readfds)){
			if(strcmp(observer[osd].userName, senderName) == 0 || strcmp(observer[osd].userName, targName) == 0){// send to sender and recipient
				send(osd, &msgLen, sizeof(msgLen), 0);// send the length of the message
				send(osd, &message, sizeof(message), 0);// send the message
			}
		}
	}
}

int main(int argc, char **argv) {
	struct protoent *ptrp; /* pointer to a protocol table entry */
	struct sockaddr_in sad; /* structure to hold server's address */
	struct sockaddr_in cad; /* structure to hold client's address */
	int SD = 0;
	int mainSD = -1;
	int mainOSD = -1;
	int newSD, newOSD;
	uint16_t port; /* protocol port number */
	socklen_t alen; /* length of address */
	int optval = 1; /* boolean value when we set socket option */
	char buf[1000]; /* buffer for string the server sends */
	int max_sd, max_osd; // the highest valued SD in the client sockets list

	fd_set readfds;
	int ready = 0;
	int n;
	int sd, osd;

	for(int i =0; i < LIMIT; i++){
		client_sockets[i] = 0;
		o_sockets[i] = 0;
	}
	if( argc != 3 ) {
		fprintf(stderr,"Error: Wrong number of arguments\n");
		fprintf(stderr,"usage:\n");
		fprintf(stderr,"./server participant_port observer_port\n");
		exit(EXIT_FAILURE);
	}
	memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
	sad.sin_family = AF_INET;// Set socket family to AF_INET
	sad.sin_addr.s_addr = INADDR_ANY;// Set local IP address to listen to all IP addresses this server can assume

	/* Map TCP transport protocol name to protocol number */
	if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
		fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
		exit(EXIT_FAILURE);
	}

	for(int i = 1; i < 3; i++){// Establish all the ports to be connected to
		port = atoi(argv[i]); /* convert argument to binary */
		if (port >= 1024 && port < 65535) { /* test for illegal value */
		// set port number. The data type is u_short
		sad.sin_port = htons(port); 
		} else { /* print error message and exit */
			fprintf(stderr,"Error: Bad port number %s\n",argv[i]);
			exit(EXIT_FAILURE);
		}
		/*  Create a socket with AF_INET as domain, protocol type as SOCK_STREAM, and protocol as ptrp->p_proto. This call returns a socket descriptor named sd. */
		if((SD = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
			perror("Socket");
			printf("Failed to create the socket \n");
			abort();
		}
		/* Allow reuse of port - avoid "Bind failed" issues */
		if( setsockopt(SD, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 ) {
			fprintf(stderr, "Error Setting socket option failed\n");
			exit(EXIT_FAILURE);
		}
		/*  Bind a local address to the socket. For this, you need to pass correct parameters to the bind function. */
		if(bind(SD, (struct sockaddr *) &sad, sizeof(sad)) < 0){
			perror("Bind");
			printf("Cannot bind socket to address \n");
			abort();
		}
		/*  Specify size of request queue. Listen take 2 parameters -- socket descriptor and QLEN, which has been set at the top of this code. */
		if(listen(SD, BACKLOG) < 0) {
			fprintf(stderr,"Error: Listen failed\n");
			exit(EXIT_FAILURE);
		} 
		if( i == 1){
			mainSD = SD;
		} else if( i == 2){
			mainOSD = SD;
		}
	}

	printf("Server Running...\n");
	/* Main server loop - accept and handle requests */
	while (1) { 
		FD_ZERO(&readfds);
		FD_SET(mainSD, &readfds);
		FD_SET(mainOSD, &readfds);

		max_sd = mainSD;// The current highest participant socket number 
		for(int i = 0; i < LIMIT; i++){
			sd = client_sockets[i];
			if(sd > max_sd){// update the highest socket number 
				max_sd = sd;
			}
			if(sd > 0){// Add non zero sockets to readfds 
				FD_SET(sd, &readfds);
			}
		}

		max_osd = mainOSD;// the current highest observer socket number
		for(int i = 0; i < LIMIT; i++){
			osd = o_sockets[i];
			if(osd > max_sd){// update the highest socket number 
				max_osd = osd;
			}
			if(osd > 0){// Add non zero sockets to readfds 
				FD_SET(osd, &readfds);
			}
		}

		int maxfd;
		if ( max_osd > max_sd ) {
			maxfd = max_osd;
		} else {
			maxfd = max_sd;
		}
		ready = select(maxfd+1, &readfds, NULL, NULL, NULL);// Check if any sockets are ready
		if (ready == -1 && errno == EINTR)
			continue;
		if (ready == -1) {
			perror("select()");
			exit(EXIT_FAILURE);
		}

		// If the main socket is ready, then a new user is attempting to join
		if(FD_ISSET(mainSD, &readfds)){// PARTICIPANT
			printf("New Participant joining! \n");
			newSD = accept(mainSD, (struct sockaddr *) &cad, &alen);
			if(newSD < 0){
				perror("accept()");
				exit(EXIT_FAILURE);
			}

			visits++;// number of current participants
			char status;
			if(visits == LIMIT){// server full
				status = 'N';
				send(newSD, &status, sizeof(status),0);
				printf("Server full...\n");
				close(newSD);// close that socket
				visits--;// decrement total number of participants
				break;
			} else {// server vacant
				status = 'Y';
				send(newSD, &status, sizeof(status), 0); 
			}
			for(int i = 0; i < LIMIT; i++){
				if(client_sockets[i] == 0){//if there is room in the socket list
					client_sockets[i] = newSD;// add the new socket descriptor to our list of sockets 
					users[maxfd+1].observer = 0;
					users[maxfd+1].enterTime = 60;// initialize their timer (?)
					users[maxfd+1].startTime = time(NULL);// start their timer for entering a name
					break;// Leave this loop and head to the established participants section
				}
			}						
		}

		if(FD_ISSET(mainOSD, &readfds)){// OBSERVER
			printf("New Observer Joining!\n");
			newOSD = accept(mainOSD, (struct sockaddr *) &cad, &alen);
			if(newOSD < 0){
				perror("accept()");
				exit(EXIT_FAILURE);
			}

			num_observers++;
			char status;
			if(num_observers == LIMIT){// server full
				status = 'N';
				send(newOSD, &status, sizeof(status),0);
				printf("Server Full...\n");
				close(newOSD);// close that socket
				num_observers--;// decrement total number of participants
			break;
			} else {// server vacant
				status = 'Y';
				send(newOSD, &status, sizeof(status), 0); 
			}
			for(int i = 0; i < LIMIT; i++){
				if(o_sockets[i] == 0){//if there is room in the socket list
					o_sockets[i] = newOSD;// add the new socket descriptor to our list of sockets 
					observer[maxfd+1].enterTime = 60;
					observer[maxfd+1].startTime = time(NULL);
					break;// Leave this loop and head to the established participants section
				}
			}	
		}

		for(int i = 0; i < LIMIT; i++){// Loop through the sockets either getting a username or a message
			sd = client_sockets[i];
			osd = o_sockets[i];

			if(FD_ISSET(sd, &readfds)){// PARTICIPANT
				if(users[sd].status != 'Y'){// If they are not yet a valid participant 

					uint8_t nameLen;
					n = (recv(sd, &nameLen, sizeof(nameLen), 0));// receive the participant's username length
					if(n == 0){
						perror("Error: Client disconnected\n");
						close(sd);// close that socket
						FD_CLR(sd, &readfds);
						client_sockets[i] = 0;
						visits--;// decrement total number of participants
						break;
					}

					char username[nameLen];
					n = (recv(sd, &username, sizeof(username), 0));// receive the participant's username  

					users[sd].endTime = time(NULL);// TODO Check how long they took to enter their username here
					users[sd].enterTime -= ( users[sd].endTime - users[sd].startTime);// minus how long they took to enter a name from their total time
					if(users[sd].enterTime <= 0){
						printf("User Took Too long to enter name...\n");
						strcpy(users[sd].userName, "");
						users[sd].status = ' ';
						users[sd].enterTime = 60;
						close(sd);// close that socket
						FD_CLR(sd, &readfds);
						client_sockets[i] = 0;
						visits--;// decrement total number of participants
						break;
					} 
					username[nameLen]= '\0';
					char wordStatus;

					if(isValid(username) == 0){
						printf("Username is invalid!\n");
						users[sd].startTime = time(NULL);// keep track of when they started entering a new name
						wordStatus = 'I';
						users[sd].status = 'I';
						send(sd, &wordStatus, sizeof(wordStatus),0);//SEND I

					} else if(isName( username) == 1){// check if name is in used names list or not
						strcpy(users[sd].userName, username);
						wordStatus = 'Y';
						users[sd].status = 'Y';
						send(sd, &wordStatus, sizeof(wordStatus),0);// SEND Y
						char joined[100];
						sprintf(joined, "User %s has joined \n", username);// TODO: Send to observers 
						send_All(joined, readfds);
					} else {
						printf("Username = '%s'", username);
						printf("Username taken (Resetting timer) \n");
						users[sd].enterTime = 60;// Reset their timer to 60 seconds
						users[sd].startTime = time(NULL);// keep track of when they started entering a new name
						wordStatus = 'T';
						users[sd].status = 'T';
						send(sd, &wordStatus, sizeof(wordStatus),0);//SEND T
						break;
					}

				}else if (users[sd].status == 'Y'){// if they are a valid participant
					uint16_t msgLen;
					n = recv(sd, &msgLen, sizeof(msgLen), 0);// receive the length of the message
					if(n < 0){
						printf("Participant disconnected...\n");
						char disconnect[100];
						sprintf(disconnect, "User %s has left \n", users[sd].userName);// TODO: Send to observers 
						send_All(disconnect, readfds);

						strcpy(users[sd].userName, "");
						users[sd].status = ' ';
						users[sd].enterTime = 60;
						close(sd);// close that socket
						FD_CLR(sd, &readfds);
						client_sockets[i] = 0;
						visits--;// decrement total number of participants
						break;
					}
					n = recv(sd, &buf, sizeof(buf), 0);//Recv message from valid participant

					if(buf[0] == '@'){// private message
						char pMessage[1000];
						int spaces = 11 - strlen(users[sd].userName);
						sprintf(pMessage, "-%*c%s: %s", spaces, ' ', users[sd].userName, buf);// Format the message

						char target[11];
						int j = 0;
						for(int i = 1; buf[i] != ' '; i++){// Isolate the recipient's name
							target[j] = buf[i];
							j++;
						}
						target[j] = '\0';
						printf("target = '%s'\n", target);
						if(isName(target) == 0){// if the recipient is a valid user 
							printf("Sending private message between %s and %s \n", users[sd].userName, target);
							private_send(pMessage, users[sd].userName, target, readfds);
						} else {// if recipient is NOT valid, send an error message
							char error[100];
							sprintf(error, "Warning: user %s doesn't exist...\n", target);
							private_send(error, users[sd].userName,users[sd].userName, readfds);// only send the error to the sender
						}   
					} else {// Public message 
						char message[1000];
						int spaces = 11 - strlen(users[sd].userName);
						sprintf(message, ">%*c%s: %s", spaces, ' ', users[sd].userName, buf);// Format the message
						send_All(message, readfds);// send message to all observer
					}
				}
			}
			if(FD_ISSET(osd, &readfds)){// OBSERVER
				if(observer[osd].status != 'Y'){
					uint8_t nameLen;
					n = (recv(osd, &nameLen, sizeof(nameLen), 0));// receive the participant's username length
					if(n == 0){
						printf("Observer disconnected...\n");
						strcpy(observer[osd].userName, "");
						observer[osd].status = ' ';
						observer[osd].enterTime = 60;
						close(osd);// close that socket
						FD_CLR(osd, &readfds);
						o_sockets[i] = 0;
						num_observers--;// decrement total number of participants
						break;
					}
					char username[nameLen];
					n = (recv(osd, &username, sizeof(username), 0));// receive the participant's username  

					observer[osd].endTime = time(NULL);// TODO Check how long they took to enter their username here
					observer[osd].enterTime -= ( observer[osd].endTime - observer[osd].startTime);// minus how long they took to enter a name from their total time
					if(observer[osd].enterTime <= 0){
						printf("User Took Too long to enter name...\n");
						strcpy(observer[osd].userName, "");
						observer[osd].status = ' ';
						observer[osd].enterTime = 60;
						close(osd);// close that socket
						FD_CLR(osd, &readfds);
						o_sockets[i] = 0;
						num_observers--;
					} else { 
						username[nameLen]= '\0';
						char wordStatus;

						if(isValid(username) == 0){// if observer enters an incorrect name,  disconnect them
							printf("Invalid username, disconnecting...\n");
							wordStatus = 'N';
							observer[osd].status = 'N';
							send(osd, &wordStatus, sizeof(wordStatus),0);//SEND N
							close(osd);
							FD_CLR(osd, &readfds);
							o_sockets[i] = 0;
							strcpy(observer[osd].userName, "");
							observer[osd].status = ' ';
							observer[osd].enterTime = 60;
							num_observers--;// decrement total number of participants
							break;

						} else if(isObserver( username) == 0){// check if name is in used names list or not (if it IS in the list, observer is established)
							wordStatus = 'Y';
							observer[osd].status = 'Y';
							strcpy(observer[osd].userName, username);
							send(osd, &wordStatus, sizeof(wordStatus),0);// SEND Y

							char oJoined[] = "A new observer has joined\n";
							send_All(oJoined, readfds);  
						} else if(isObserver (username) == -1){ 
							printf("Username taken (Resetting timer) \n");
							wordStatus = 'T';
							observer[osd].status = 'T';
							observer[osd].enterTime = 60;// reset their timer
							send(osd, &wordStatus, sizeof(wordStatus),0);//SEND T
						} else {
							printf("Invalid username, disconnecting...\n");
							wordStatus = 'N';
							observer[osd].status = 'N';
							send(osd, &wordStatus, sizeof(wordStatus),0);//SEND N
							close(osd);
							FD_CLR(osd, &readfds);
							o_sockets[i] = 0;
							num_observers--;// decrement total number of participants
							break;
						}
					} 
				}
			}
		}// End of socket loop
	}// End of server endless while loop	
}

