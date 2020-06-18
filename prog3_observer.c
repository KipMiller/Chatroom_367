/* Program 3 observer 
 * Michael Montgomery | Chris Miller 
 * CSCI 367 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/*------------------------------------------------------------------------
* Program: Observer (based on the original demo client code provided in class) 
*
* Purpose: An observer connects to the server and affiliates itself with a client,
*           it will see every message (private and public) that the client sends/receives.
*
* Syntax: ./prog3_oberver server_address server_port
*
* server_address - name of a computer on which server is executing
* server_port    - protocol port number server is using
*
*------------------------------------------------------------------------
*/
int main( int argc, char **argv) {
	struct hostent *ptrh; /* pointer to a host table entry */
	struct protoent *ptrp; /* pointer to a protocol table entry */
	struct sockaddr_in sad; /* structure to hold an IP address */
	int sd; /* socket descriptor */
	uint16_t port; /* protocol port number */
	char *host; /* pointer to host name */
	int n; /* number of characters read */
	char buf[1000]; /* buffer for data from the server */

	memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
	sad.sin_family = AF_INET; /* set family to Internet */

	if( argc != 3 ) {
		fprintf(stderr,"Error: Wrong number of arguments\n");
		fprintf(stderr,"usage:\n");
		fprintf(stderr,"./client server_address server_port\n");
		exit(EXIT_FAILURE);
	}
	port = atoi(argv[2]); /* convert to binary */
	if (port >= 1024 && port < 65535) /* test for legal value */
		sad.sin_port = htons((u_short)port);
	else {
		fprintf(stderr,"Error: bad port number %s\n",argv[2]);
		exit(EXIT_FAILURE);
	}
	host = argv[1]; /* if host argument specified */

	/* Convert host name to equivalent IP address and copy to sad. */
	ptrh = gethostbyname(host);
	if ( ptrh == NULL ) {
		fprintf(stderr,"Error: Invalid host: %s\n", host);
		exit(EXIT_FAILURE);
	}
	memcpy(&sad.sin_addr, ptrh->h_addr, ptrh->h_length);

	/* Map TCP transport protocol name to protocol number. */
	if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
		fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
		exit(EXIT_FAILURE);
	}
	/* Create a socket. */
	sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (sd < 0) {
		fprintf(stderr, "Error: Socket creation failed\n");
		exit(EXIT_FAILURE);
	}
	/* Connect the socket to the specified server. You have to pass correct parameters to the connect function.*/
	if (connect(sd, (struct sockaddr *) &sad, sizeof(sad)) < 0) {
		fprintf(stderr,"connect failed\n");
		exit(EXIT_FAILURE);
	}
	
	// Receive a 'Y' or 'N' for if the server is full or not
	char status;
        n = recv(sd, &status, sizeof(status), 0);
        if(n < 0){
            printf("ERROR");
            exit(EXIT_FAILURE);
        }
	if(status == 'N'){
            printf("Server full...\n");
            close(sd);
            exit(EXIT_FAILURE);
        }
	status = ' ';
	while(status != 'Y'){

        printf("Enter a username (1-10 characters): ");
        char username[sizeof(buf)];
        fgets(username, sizeof(username), stdin);
        while(strlen(username)-1 == 0 || strlen(username)-1 > 10){// if they enter a name of incorrect length 
            printf("Invalid username: Does not fit parameters (1-10 characters)\n");
            printf("Please enter new username: ");// keep prompting them until they enter correct length name
            fgets(username, sizeof(username), stdin);
        }
        
        uint8_t nameLen = strlen(username)-1;// get rid of trailing newline character
        send(sd, &nameLen, sizeof(nameLen), 0);// Send the length of the username
        send(sd, &username, sizeof(username), 0);// Send the username
        
        // Receive I Y T 
        n = recv(sd, &status, sizeof(status), 0);
        if(status == 'N'){
            printf("Username not taken, disconnecting observer\n");
            exit(EXIT_FAILURE);
        } else if(status == 'T'){
            printf("Username already has an observer (timer reset)\n");
        } else {
            printf("Username valid: Now entering chatroom as an observer...\n");
        }
    }

	for(;;){// infinite loop of: prompt for a message, send message to the server
        uint16_t msgLen; 
        n = recv(sd, &msgLen, sizeof(msgLen), 0);
        char message[msgLen];
        n = recv(sd, &message, sizeof(message), 0);//Recv message from valid participant
        if(n < 0){
            printf("ERROR");
            exit(EXIT_FAILURE);
        }
        printf("%s", message);
	}
	
	close(sd);
	exit(EXIT_SUCCESS);
}

