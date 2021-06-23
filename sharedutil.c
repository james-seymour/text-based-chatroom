#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include "sharedutil.h"

sem_t* create_lock(sem_t* lock) {
    sem_init(lock, 0, 1);
    return lock;
}

void take_lock(sem_t* lock) {
    sem_wait(lock);
}

void release_lock(sem_t* lock) {
    sem_post(lock);
}

int hash_input(char* input) {
    int hash = 0;
    for (int i = 0; i < strlen(input); i++) {
        // Multiply hash by 2, then add previous hash and ASCII value of input
        // This guarantees a unique hash combination
        hash = ((hash << 1) + hash + input[i]);
    }
    return hash;
}

int send_message(Client* client, char* message) {
    
    if (message == NULL) {
        return 0;
    }
    
    take_lock(client->writeLock);
    
    // Update any bad characters in the message with a '?' char
    for (int i = 0; i < strlen(message); i++) {
        if (message[i] < 32 && message[i] != '\n') {
            message[i] = '?';
        }
    }

    // If the message can still be sent, send it.
    if (!ferror(client->writeHandle)) {
        fprintf(client->writeHandle, "%s\n", message);
        fflush(client->writeHandle);
    }

    release_lock(client->writeLock);
    return 1;
}

int receive_message(Client* client, char* buffer) {
    // If there are any file handle errors or EOF is reached, return 0
    if (!ferror(client->readHandle)) {
        if (fgets(buffer, MAX_BUF - 1, client->readHandle)) {
            strtok(buffer, "\n");
            return 1;
        }
    }
    return 0;
}

int handle_server_message(char* message) {
    
    // Thread safe strtok is not used as message buffers are never shared
    // between threads 
    char* command = strtok(message, ":");
    char* optArg1 = strtok(NULL, ":");
    char* optArg2 = strtok(NULL, "\n");
    int hashCommand = hash_input(command);

    // Outputs readable message from command if command is valid
    switch (hashCommand) {
        case ENTER:
            fprintf(stdout, "(%s has entered the chat)\n", optArg1);
            break;
        case LEAVE:
            fprintf(stdout, "(%s has left the chat)\n", optArg1);
            break;
        case MSG:
            fprintf(stdout, "%s: %s\n", optArg1, optArg2);
            break;
        case KICK: 
            fprintf(stderr, "Kicked\n");
            return KICKED;
        case LIST:
            fprintf(stdout, "(current chatters: %s)\n", optArg1);
            break;
        default:
            break;
    }

    fflush(stdout);
    return 0;
}

Client* setup_client(int socket, char* name, char* authString) {
    Client* client = malloc(sizeof(Client));
    client->next = NULL; 

    // Create pointers to the socket file handles and give to thread
    int extraSocket = dup(socket);
    client->readHandle = fdopen(socket, "r");
    client->writeHandle = fdopen(extraSocket, "w");
    
    // Initialise writing lock and give to thread
    client->writeLock = create_lock(malloc(sizeof(sem_t)));
 
    // If the client gives a name on startup (clientside only), then give it
    // this name.
    client->name = malloc(sizeof(char) * MAX_BUF);
    if (name != NULL) {
        client->name = strcpy(realloc(client->name, 
                sizeof(char) * (strlen(name) + 3)), name);
    }
    client->authString = 
            strcpy(malloc(sizeof(char) * strlen(authString) + 1), authString);
    
    // Initialise client stats and give to thread
    client->stats = calloc(NUM_CLIENT_STATS, sizeof(int));

    // Set the initial status of the client to be communicating
    client->isCommunicating = 1;

    return client;
}

void free_client(Client* client) {
    // If the client instance exists (which it always should), free all
    // allocated variables in the client
    if (client != NULL) {
        fclose(client->readHandle);
        fclose(client->writeHandle);
        free(client->name);
        free(client->authString);
        free(client->writeLock);
        free((int*) client->stats);
        free(client);   
    }
}
