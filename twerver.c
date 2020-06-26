#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>
#include "socket.h"

#ifndef PORT
    #define PORT 53456
#endif

#define LISTEN_SIZE 5
#define WELCOME_MSG "Welcome to CSC209 Twitter! Enter your username: "
#define SEND_MSG "send"
#define SHOW_MSG "show"
#define FOLLOW_MSG "follow"
#define UNFOLLOW_MSG "unfollow"
#define BUF_SIZE 256
#define MSG_LIMIT 8
#define FOLLOW_LIMIT 5

#define FOLLOW_ERR "Please enter a valid username to follow"
#define QUIT_CMD "quit"
#define INVALID_CMD "Invalid command"

struct client {
    int fd;
    struct in_addr ipaddr;
    char username[BUF_SIZE];
    char message[MSG_LIMIT][BUF_SIZE];
    struct client *following[FOLLOW_LIMIT]; // Clients this user is following
    struct client *followers[FOLLOW_LIMIT]; // Clients who follow this user
    char inbuf[BUF_SIZE]; // Used to hold input from the client
    char *in_ptr; // A pointer into inbuf to help with partial reads
    struct client *next;
};


// Provided functions. 
void add_client(struct client **clients, int fd, struct in_addr addr);


void remove_client(struct client **clients, int fd);

// These are some of the function prototypes that we used in our solution 
// You are not required to write functions that match these prototypes, but
// you may find them helpful when thinking about operations in your program.

// Send the message in s to all clients in active_clients. 
void announce(struct client **active_clients, char *s);

// Move client c from new_clients list to active_clients list. 
void activate_client(struct client *c, 
    struct client **active_clients_ptr, struct client **new_clients_ptr);


// The set of socket descriptors for select to monitor.
// This is a global variable because we need to remove socket descriptors
// from allset when a write to a socket fails. 
fd_set allset;

//added functions
void unfollow(struct client *follower, struct client *active_clients, char *name);

void send_message(struct client *c, char *input, struct client **active_clients);

int write_to_client(int fd, char* msg);

/* Function: new_line
 * ------------------------
 * appends a network newline to a string
 *
 * s: pointer of string
 *
*/
void newline(char *s){

	strcat(s, "\r\n");
}

 /* Function: add_client
 * ------------------------
 * Create a new client, initialize it, and add it to the head of the linked
 * list.
 *
 * clients: linked list of clients to add to
 * fd: file descritor to add to client
 * addr: address of client connection
 * 
*/
void add_client(struct client **clients, int fd, struct in_addr addr) {
    struct client *p = malloc(sizeof(struct client));
    if (!p) {
        perror("malloc");
        exit(1);
    }

    printf("Adding client %s\n", inet_ntoa(addr));
    p->fd = fd;
    p->ipaddr = addr;
    p->username[0] = '\0';
    p->in_ptr = p->inbuf;
    p->inbuf[0] = '\0';
    p->next = *clients;

    // initialize messages to empty strings
    for (int i = 0; i < MSG_LIMIT; i++) {
        p->message[i][0] = '\0';
    }
    
    //TODO: added initilaizing following and followers
    //because otherwise the mem was being reused
    for (int i = 0; i < FOLLOW_LIMIT; i++){
    
    	p -> following[i] = NULL;
    	p -> followers[i] = NULL;
    }

    *clients = p;
}

/* Function: remove_client
 * ------------------------
 * removes client from linked list, removes client from all other clients
 * that are following it or it is following, and cleans up memory/connecitons.
 *
 * clients: pointer to linked list of clients
 * fd: file descriptor of client to remove
 * 
*/
void remove_client(struct client **clients, int fd) {
    struct client **p;

    for (p = clients; *p && (*p)->fd != fd; p = &(*p)->next)
        ;

    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        // Remove the client from other clients' following/followers lists
        
        struct client *q;
        
		for(q = *clients; q; q = q -> next){
		

			if(q -> username[0] != '\0'){
			
				//send goodbye message
				char msg[BUF_SIZE] = "\0";
				strcat(msg, "Goodbye ");
				strcat(msg, (*p) -> username);
				write_to_client(q -> fd, msg);
			}
			

		        		
			for(int i = 0; i < FOLLOW_LIMIT; i++){
			
				if(q -> followers[i] == *p){
				
					q -> followers[i] = NULL;
				}
				if(q -> following[i] == *p){
					
					printf("%s no longer following %s because they disconnected\r\n", q -> username, (*p) -> username);
					printf("%s no longer has %s as a follower\r\n", (*p) -> username, q -> username);					
					q -> following[i] = NULL;
				}
				
			}
		}

        // Remove the client
        struct client *t = (*p)->next;
        //TODO: move the disconnect somewhere else
        printf("Disconnect from %s\r\n", inet_ntoa((*p)->ipaddr));
        printf("Removing client %d %s\r\n", fd, inet_ntoa((*p)->ipaddr));
        FD_CLR((*p)->fd, &allset);
        close((*p)->fd);
        free(*p);
        *p = t;
    } else {
        fprintf(stderr, 
            "Trying to remove fd %d, but I don't know about it\n", fd);
    }
}

/* Function: activate_client
 * ------------------------
 * appends newline to message and write to a client
 * 
 * c: client to activate
 * active_clients_ptr: pointer to linked list of active_client
 * new_clients_ptr: pointer to linked list of new clients
 * 
*/
void activate_client(struct client *c, 
    struct client **active_clients_ptr, struct client **new_clients_ptr) {
    
    struct client **p;
    //iterate through new_clients until we find the client we're looking for
    for (p = new_clients_ptr; *p && (*p)->fd != c -> fd; p = &(*p)->next);
    //save the address of the client we're looking for
    struct client *u = *p;
    //save the address of the next new_client
    struct client *t = (*p)->next;
    //change the address of p to be it's next new client
    *p = t;
    //change the client's next to be the start of the active_clients
    u -> next = *active_clients_ptr;
    //change active_clients to start at the client
    *active_clients_ptr = u;
    
}

//TODO: should look for both \r and \n
///function to find a network newline and return index + 1
int find_network_newline(const char *buf, int n) {
	
	for(int i = 0; i < n; i ++){
		if(buf[i] == '\n'){
			return i + 1;
		}
	}
	
    return -1;
}
 
 /* Function: read_client
 * ------------------------
 * reads text from client and places in buf
 * 
 * c: client to read from
 * clients: client linked list that c is part of
 * buf: string to place read message in
 *
 * returns: 0 if newline is read, 1 if newline not read, returns 2 if fd is closed
 * 
*/
int read_client(struct client *c, struct client **clients, char *buf){
	
	char *inbuf = c -> inbuf;
	//position after string in inbuf
	char *after = inbuf + strlen(inbuf);
	int after_size = after - inbuf;
	//room left in inbuf
	int room = BUF_SIZE - after_size;
	//do the read
	int nbytes = read(c -> fd, after, room);
	fprintf(stdout, "[%d] Read %d bytes\r\n", c -> fd, nbytes);	
	//int buf_size = strlen(inbuf);
	int buf_size = after_size + nbytes;
	
	//if read closed then disconnect client
	if(nbytes == 0){
		
		remove_client(clients, c -> fd);
		return 2;
	}
	
	//error check
	if (nbytes == -1){
	
		perror("read");
		exit(1);
	}
	
	//place null terminator after what's read in, 
	//so we can calc length next time
	inbuf[after_size + nbytes] = '\0';
	int where;
	//if a line is read
	if((where = find_network_newline(inbuf, after_size + nbytes)) != -1){
		
		//put a null terminator where /r was
		(inbuf)[where -2] = '\0';
		//copy line out of inbuf
		strcpy(buf, inbuf);
		printf("[%d] Found newline: %s\r\n", c -> fd, buf);
		
		for(int i = 0; i < buf_size + where; i++){
			if(i < buf_size){
				inbuf[i] = inbuf[where + i];
			}
			else{
				inbuf[i] = '\0';
			}
		}
		
		return 0;	
	}
	
	return 1;
	
}

/* Function: write_to_client
 * ------------------------
 * appends newline to message and write to a client
 * 
 * fd: file descriptor
 * msg: string of message
 *
 * returns: 0  if seccessful, 1 if fd is closed
 * 
*/
int write_to_client(int fd, char* msg){
	
	char res[BUF_SIZE] = "\0";
	
	strcpy(res, msg);
	
	newline(res);
	
	int nbytes = write(fd, res, strlen(res));
	
	//if write fails return 1
	if(nbytes == 0){
		return 1;
	}
	
	//error check
	if(nbytes == -1){
		
		perror("write");
		exit(1);
	}
	
	return 0;
}
 
/* Function: announce
 * ------------------------
 * Send the message to all active clients
 * 
 * active_clients: pionter to active clients
 * s: message
 *
 * 
 */
void announce(struct client **active_clients, char *s){
	
	struct client *p;
	
	for(p = *active_clients; p; p = p -> next){
	
	if(write_to_client(p ->fd, s)){
		
		remove_client(active_clients, p -> fd);
	}
	
	}
	
}
 
 /* Function: client_by_name
 * ------------------------
 * finds a client by their username
 * 
 * c: pointer to client
 * name: name of client that's being searched for
 * 
 */
struct client *client_by_name(struct client *c, char *name){
	
	struct client *p;
	
	for(p = c; p; p = p -> next){
		if(!strcmp(p -> username, name)){
			return p;
		}
	}
	
	return NULL;

}

/*
 * adds a user as a follower
 *
 *
 */
 //TODO: make so that you can't have the same follower twice
 //TODO: large function
 
 /* Function: add_follower
 * ------------------------
 * adds client as a follower of another client by name
 * 
 * follower: pointer to client that will follow
 * active_clients: pointer to linked list of active clients
 * name: name of client that follower is aiming to follow
 * 
 */
void add_follower(struct client *follower, struct client **active_clients, char *name){
	//TODO: stop from being able to follow yourself?
	//get name of client to follow
	struct client *to_follow = client_by_name(*active_clients, name);
	
	//if client's name not found send error message
	if(!to_follow){
		//write(follower -> fd, FOLLOW_ERR, strlen(FOLLOW_ERR));
		if(write_to_client(follower -> fd, FOLLOW_ERR)){
			
			remove_client(active_clients, follower -> fd);
		}
		
		return;
	}
	
	int i;
	
	//find space for follower in follow_limit
	for(i = 0; i < FOLLOW_LIMIT; i++){
		
		if(!((follower -> following)[i])){
		
			//fprintf(stdout,"%s following %s\n", follower -> username, to_follow -> username);			
			break;	
		}
	}
	
	if(i == FOLLOW_LIMIT){
	//write(follower -> fd, "cannot follow as your maximum followers have been reached \n", 60);
	if(write_to_client(follower -> fd, "cannot follow as your maximum followers have been reached")){
	
		remove_client(active_clients, follower -> fd);
	}
	return;	
	}
	
	int j;
	
	//add to_follow to follower's following, add follower to to_follow's followers
	//if space is found
	for(j = 0; j < FOLLOW_LIMIT; j++){
		
		if(!((to_follow -> followers)[j])){
			follower -> following[i] = to_follow;
			fprintf(stdout,"%s is following %s\n", follower -> username, to_follow -> username);
			to_follow -> followers[j] = follower;	
			fprintf(stdout,"%s has %s as a follower\n", to_follow -> username, follower -> username);
			return;
		}
	}

	//write(follower -> fd, "cannot follow as their maximum followers have been reached \n", 60);		
	if(write_to_client(follower -> fd, "cannot follow as their maximum followers have been reached")){
	
		remove_client(active_clients, follower -> fd);
	}

}


 /* Function: unfollow
 * ------------------------
 * gets client to unfollow other clinet by name
 * 
 * follower: pointer to client that will unfollow
 * active_clients: pointer to linked list of active clients
 * name: name of client that follower is aiming to unfollow
 * 
 */
void unfollow(struct client *follower, struct client *active_clients, char *name){
	
	struct client *c;

	//find pointer to client that was being followed and remove
	for(int i = 0; i < FOLLOW_LIMIT; i++){
		
		//TODO: use variable to make more readable,
		if((follower -> following)[i]){
		
			if(!strcmp(follower -> following[i] -> username, name)){
				
				c = follower -> following[i];
				follower -> following[i] = NULL;
				fprintf(stdout,"%s unfollowed %s\r\n", follower -> username, c -> username);
				break;
			}
		}
	}
	
	//remove 
	for(int i = 0; i < FOLLOW_LIMIT; i++){
	
		if((c -> followers)[i]){
			
			if(!strcmp((c -> followers)[i] -> username, follower -> username)){
				
				c -> followers[i] = NULL;
			} 
		}
	
	}
	
	fprintf(stdout,"%s no longer has %s as a follower \r\n", c -> username, follower -> username);
}



 /* Function: show
 * ------------------------
 * send all the messages from clients that a client is following
 * 
 * c: client
 * active_clients: pointer to active clients
 * 
 */
void show(struct client *c, struct client **active_clients){

	for(int i = 0; i < FOLLOW_LIMIT; i++){
		
		if(c -> following[i]){
			
			for(int j = 0; j < MSG_LIMIT; j++){
			
				if(c -> following[i] -> message[j][0] != '\0'){
					
					//format message
					char msg[BUF_SIZE];
					msg[0] = '\0'; 
					strcpy(msg, c -> following[i] -> username);
					strcat(msg, " wrote: ");
					strcat(msg, c -> following[i] -> message[j]);
					
					//send message
					if(write_to_client(c -> fd, msg)){
					
						remove_client(active_clients, c -> fd);
					}
				}
			}
		}
	}

}

//TODO: large function
//TODO: edge case, message message send with no followers
//TODO: what is message is over 140 char
//TODO: make change size of message arrays to 142 ,.

/* Function: send_message
 * ------------------------
 * adds a message to a client's messages if there's space, and sends to all
 * clients that are following that client
 * 
 * c: client
 * active_clients: pointer to active clients
 * input: message to be added and sent
 * 
 */
void send_message(struct client *c, char *input, struct client **active_clients){
	
	int j;
	
	//check to see if there's messaging space. if there is add message
	for(j = 0; j < MSG_LIMIT; j++){
	
		if((c -> message[j][0]) == '\0'){
		
			strcpy(c -> message[j], input);
			break;
		}
	}

	//if not, don't send
	if(j == MSG_LIMIT){
	
		if(write_to_client(c -> fd, "message limit reached. cannot send")){
		
			remove_client(active_clients, c -> fd);
		}
		return;
	}
	
	char msg[BUF_SIZE] = "\0";
	
	//format message for sending
	strcpy(msg, c -> username);
	strcat(msg, ": ");
	strcat(msg, input);	
		
	//loop through followers and write message to them
	for(int i = 0; i < FOLLOW_LIMIT; i++){
		
		//if there's a follower
		if((c -> followers)[i]){
		
			if(write_to_client(c -> followers[i] -> fd, msg)){
			
				remove_client(active_clients, c -> followers[i] -> fd);
			}
		}
	}
	
}

/* Function: quit
 * ------------------------
 * adds a message to a client's messages if there's space, and sends to all
 * clients that are following that client
 * 
 * c: client
 * active_clients: pointer to active clients
 * input: message to be added and sent
 * 
 */
void quit(struct client *c, struct client **active_clients){
	
	
	//remove from following list of all followers
	for(int i = 0; i < FOLLOW_LIMIT; i++){
	
		if(c -> followers[i]){
			
			struct client *p = c -> followers[i];
			
			for(int j = 0; j < FOLLOW_LIMIT; j++){
			
				if(!strcmp(p -> following[j] -> username, c -> username)){
				
					p -> following[j] = NULL;
					break;
				} 
			
			}
			
		}
	
	}
	
	remove_client(active_clients, c -> fd);

}
 
/* Function: parse_input
 * ------------------------
 * takes input from client and calls correct command depending on input
 * 
 * c: client
 * active_clients: pointer to active clients
 * 
 */
void parse_input(struct client *c, struct client **active_clients){

	char input[BUF_SIZE];
	
	//read client inputs
	if(read_client(c, active_clients, input)){
	
		//partial input or disconnected
		return;
	}

	printf("%s: %s \r\n", c -> username, input);
	char *space = strchr(input, ' ');

	//if a space character is found
	if(space){
		
		//replace space with null termintor to recognize first command word
		*space = '\0';
		space++;

		
		//follow command
		if(!strcmp(input, FOLLOW_MSG)){
			
			add_follower(c, active_clients, space);
			return;
		}
		
		//unfollow command
		if(!strcmp(input, UNFOLLOW_MSG)){
			
			unfollow(c, *active_clients, space);
			return;
		}
		
		//send command
		if(!strcmp(input, SEND_MSG)){
			
			//erase command from argument
			strcpy(input, space);
			//send message
			send_message(c, input, active_clients);
			return;
		}
	
	}
	
	//if a space character is not found
	else{
	
		//show command
		if(!strcmp(input, SHOW_MSG)){
			
			show(c, active_clients);
			return;
		}
		
		//quit command
		if(!strcmp(input, QUIT_CMD)){
			
			quit(c, active_clients);
			return;
		}
	}
	
	//if we reach here, it's invalid
	printf("%s\r\n", INVALID_CMD);	

	if(write_to_client(c -> fd, INVALID_CMD)){
	
		remove_client(active_clients, c -> fd);
	}

}

int main (int argc, char **argv) {
    int clientfd, maxfd, nready;
    struct client *p;
    struct sockaddr_in q;
    fd_set rset;

    // If the server writes to a socket that has been closed, the SIGPIPE
    // signal is sent and the process is terminated. To prevent the server
    // from terminating, ignore the SIGPIPE signal. 
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGPIPE, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    // A list of active clients (who have already entered their names). 
    struct client *active_clients = NULL;

    // A list of clients who have not yet entered their names. This list is
    // kept separate from the list of active clients, because until a client
    // has entered their name, they should not issue commands or 
    // or receive announcements. 
    struct client *new_clients = NULL;

    struct sockaddr_in *server = init_server_addr(PORT);
    int listenfd = set_up_server_socket(server, LISTEN_SIZE);
    free(server);

    // Initialize allset and add listenfd to the set of file descriptors
    // passed into select 
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);

    // maxfd identifies how far into the set to search
    maxfd = listenfd;

    while (1) {
        // make a copy of the set before we pass it into select
        rset = allset;
			
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (nready == -1) {
            perror("select");
            exit(1);
        } else if (nready == 0) {
            continue;
        }

        // check if a new client is connecting
        if (FD_ISSET(listenfd, &rset)) {
            printf("A new client is connecting\n");
            clientfd = accept_connection(listenfd, &q);

            FD_SET(clientfd, &allset);
            if (clientfd > maxfd) {
                maxfd = clientfd;
            }
            printf("Connection from %s\n", inet_ntoa(q.sin_addr));
            add_client(&new_clients, clientfd, q.sin_addr);
            char *greeting = WELCOME_MSG;
            if (write(clientfd, greeting, strlen(greeting)) == -1) {
                fprintf(stderr, 
                    "Write to client %s failed\r\n", inet_ntoa(q.sin_addr));
                remove_client(&new_clients, clientfd);
            }
        }

        // Check which other socket descriptors have something ready to read.
        // The reason we iterate over the rset descriptors at the top level and
        // search through the two lists of clients each time is that it is
        // possible that a client will be removed in the middle of one of the
        // operations. This is also why we call break after handling the input.
        // If a client has been removed, the loop variables may no longer be 
        // valid.
        int cur_fd, handled;
        for (cur_fd = 0; cur_fd <= maxfd; cur_fd++) {
            if (FD_ISSET(cur_fd, &rset)) {
                handled = 0;

                // Check if any new clients are entering their names
                for (p = new_clients; p != NULL; p = p->next) {
                    if (cur_fd == p->fd) {
                        // handle input from a new client who has not yet
                        // entered an acceptable name
                         
                        char temp_name[BUF_SIZE];
                        
                        int nbytes = 0;
                        
                        if((nbytes = read_client(p, &new_clients, temp_name))){
                        	
                        	if(nbytes == 2){
                        		
                        		//client disconnected
                        		//remove_client(&new_clients, p -> fd);
                        	}
                        	
                        	break;
                        }
                        
                        //if name is already in active clients send msg to client and break
                        if(client_by_name(active_clients, temp_name)){
                        	//write(p -> fd, "username already exists. Please pick another\n", 45);
                        	if(write_to_client(p -> fd, "username already exists. Please pick another")){
                        	
                        		remove_client(&new_clients, p -> fd);
                        	}
                        	break;
                        }
                        
                        //else activate client
                        activate_client(p, &active_clients, &new_clients);
						
						//and add the name
                        strcpy(p -> username, temp_name);
                        
                        //TODO: should i initialize all arrays like this?
                        char msg[BUF_SIZE] = "\0";                        
                        
                        strcat(msg, temp_name);
                        
                        strcat(msg, " has just joined.");
                        
                        announce(&active_clients, msg);
                        
                        printf("%s has just joined.\r\n", p -> username);
                        handled = 1;
                        break;
                    }
                }

                if (!handled) {
                    // Check if this socket descriptor is an active client
                    for (p = active_clients; p != NULL; p = p->next) {
                        if (cur_fd == p->fd) {
                            //handle input from an active client
                            
                            parse_input(p, &active_clients);
                           
                            break;
                        }
                    }
                }
            }
        }
    }
    return 0;
}

/*

BUGS: //TODO: See bug in last scenaario, where "Rhys:Ibrahim got printed when Ibrahim left
Also, Goodbye ibrahim didn't get sent to anyone. actually issue may have been resolved.


*/

