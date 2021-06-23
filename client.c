#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <semaphore.h>
#include <netdb.h>
#include "client.h"
#include "sharedutil.h"

int main(int argc, char* argv[]) {

    // Grab client name and auth string
    char* name = argv[1];

    FILE* authFilePath = fopen(argv[2], "r");
    if (argc != 4 || authFilePath == NULL) {
        fprintf(stderr, "Usage: client name authfile port\n");
        client_exit(USAGE, NULL);
    }

    char auth[MAX_BUF] = "AUTH:";
    char authBuffer[MAX_BUF];
    strcat(auth, strtok(fgets(authBuffer, MAX_BUF - 1, authFilePath), "\n"));
    fclose(authFilePath);
    
    // Establish connection with server
    char* port = argv[3];
    int socket = connect_to_server(port); 
    if (!socket) {
        fprintf(stderr, "Communications error\n");
        client_exit(COMMS, NULL);
    }  
    
    // Setup client's datastructure instance
    Client* client = setup_client(socket, name, auth);

    // Authenticate client and negotiate names with the server
    if (!authenticate_client(client) || !resolve_client_name(client)) {
        fprintf(stderr, "Authentication error\n");
        client_exit(FAILAUTH, client);
    }

    pthread_t serverTid;
    // Setup a thread to listen to server messages and handle them clientside. 
    pthread_create(&serverTid, 0, listen_to_server, (void*) client);

    pthread_t userTid;
    // Setup another thread to listen to user input and send back to server.
    pthread_create(&userTid, 0, listen_to_user, (void*) client);
   
    // If threads do not exit, wait for both to return so main does not exit
    pthread_join(serverTid, NULL);
    pthread_join(userTid, NULL);

    client_exit(NORMAL, client);
    return 1;
}

int connect_to_server(char* port) {
    // Setup address information storage variables
    struct addrinfo* ai = NULL;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    // Client connects to localhost, so get address info of host on the port
    getaddrinfo("localhost", port, &hints, &ai);
        
    // Open socket and connect to server
    int socketFD = socket(AF_INET, SOCK_STREAM, 0);
    int connection = connect(socketFD, ai->ai_addr, sizeof(struct sockaddr));
    
    // Free address information before returning whether connection established
    freeaddrinfo(ai);
    if (connection < 0) {
        return 0;
    } else {
        return socketFD;
    }
}

int authenticate_client(Client* client) {
    char buffer[MAX_BUF];
    while (1) {

        int response = receive_message(client, buffer);
        if (!response) {
            fprintf(stderr, "Communications error\n");
            client_exit(COMMS, NULL);

        } else if (!strcmp(buffer, "AUTH:")) {
            send_message(client, client->authString);
        
        } else if (!strcmp(buffer, "OK:")) {
            return 1;
        }
    }
    return 0;
}

int resolve_client_name(Client* client) {
    int nameCounter = -1;
    char buffer[MAX_BUF];
    char nameBuffer[strlen(client->name) + 3];
    
    while (1) {

        int response = receive_message(client, buffer);
        if (!response) {
            fprintf(stderr, "Communications error\n");
            client_exit(COMMS, NULL);
        }

        if (!strcmp(buffer, "WHO:")) {
            if (nameCounter < 0) {
                sprintf(nameBuffer, "NAME:%s", client->name);
            } else {
                sprintf(nameBuffer, "NAME:%s%d", client->name, nameCounter);
            }
            send_message(client, nameBuffer);

        } else if (!strcmp(buffer, "NAME_TAKEN:")) {
            nameCounter++;

        } else if (!strcmp(buffer, "OK:")) {
            return 1;
        }
    }

    // Update the client's name if it has changed
    if (nameCounter >= 0) {
        sprintf(client->name, "%s%d", client->name, nameCounter);
    }

    return 1;
}

void* listen_to_user(void* args) {
    Client* client = (Client*) args;
    
    char buffer[MAX_BUF];
    // Receive messages from user, parse, and send message back to server
    while (fgets(buffer, MAX_BUF - 1, stdin)) {
        int response = handle_user_message(buffer);
        send_message(client, buffer);
        if (response == LEAVE) {
            client_exit(NORMAL, client);    
        }
    }

    return NULL;
}

void* listen_to_server(void* args) {
    Client* client = (Client*) args;
    
    char buffer[MAX_BUF];
    // Receive messages from the server, parse, and output to user
    while (receive_message(client, buffer)) {
        int response = handle_server_message(buffer); 
        if (response == KICKED) {
            client_exit(KICKED, client); 
        }
    }

    // If the client reaches EOF from the server, exit with a COMMS error.
    client_exit(COMMS, client);
    return NULL;
}

int handle_user_message(char* message) {
    char messageCopy[strlen(message) + 1];
    strcpy(messageCopy, message);
    
    if ((int) messageCopy[0] != '*') {
        // Client has sent a message (SAY command)
        strtok(messageCopy, "\n");
        if (messageCopy == NULL) {
            sprintf(message, "SAY: ");
        } else {
            sprintf(message, "SAY:%s", messageCopy);
        }
    
    } else {
        // Client has sent a command to the server 
        // If and only if this command is leave, return leave and exit
        //char* command = strtok(messageCopy, "*");
        sprintf(message, "%s", strtok(messageCopy + 1, "\n"));
        if (hash_input(messageCopy + 1) == LEAVE) {
            return LEAVE;
        }
    }

    return 0;
}

void client_exit(int exitCode, Client* client) {
    free_client(client);
    exit(exitCode);
}
