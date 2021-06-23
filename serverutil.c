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
#include <signal.h>
#include "server.h"
#include "sharedutil.h"
#include "serverutil.h"

int setup_server_connection(char* port) {
    // Setup correct address information
    struct addrinfo* ai = 0;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    int err;
    if ((err = getaddrinfo("localhost", port, &hints, &ai))) {
        freeaddrinfo(ai);
        return 0;
    }

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (bind(serverSocket, 
            (struct sockaddr*) ai->ai_addr, sizeof(struct sockaddr))) {
        return 0;
    }

    struct sockaddr_in ad;
    memset(&ad, 0, sizeof(struct sockaddr_in));
    socklen_t len = sizeof(struct sockaddr_in);
    if (getsockname(serverSocket, (struct sockaddr*) &ad, &len)) {
        return 0;
    }

    if (listen(serverSocket, INF)) {
        return 0;
    }
    
    fprintf(stderr, "%u\n", ntohs(ad.sin_port));
    freeaddrinfo(ai);
    return serverSocket;
}

Server* setup_server_instance(char* authString) {
    
    Server* server = malloc(sizeof(Server));
    
    // Give server the authstring. This should never be updated
    server->authString = authString; 
    
    // Setup clientList lock which locks on any updating of the client list
    server->clientAccess = create_lock(malloc(sizeof(sem_t)));
    
    // Initialise server stats and stats lock and give to server
    server->statsAccess = create_lock(malloc(sizeof(sem_t)));
    server->stats = calloc(NUM_SERVER_STATS, sizeof(int));

    return server;
}

void initialise_sighup_handler(Server* server) {
    // 
    pthread_t* signalThread = malloc(sizeof(pthread_t));
    sigset_t* signalMask = malloc(sizeof(sigset_t));
    sigemptyset(signalMask);
    sigaddset(signalMask, SIGHUP);
    pthread_sigmask(SIG_BLOCK, signalMask, NULL);
 
    SignalHandler* handler = malloc(sizeof(SignalHandler));
    handler->signalMask = signalMask;
    handler->server = server;
    pthread_create(signalThread, NULL, sigholdup_handler, handler);
    pthread_detach(*signalThread);
}

void* sigholdup_handler(void* args) {
    SignalHandler* handler = (SignalHandler*) args;
    Server* server = handler->server;
    int signal;
    while (!sigwait(handler->signalMask, &signal)) {       
        print_server_stats(server);
    }
    pthread_exit(0);
}

Client* add_client(Client* clientList, Client* newClient) {
    // If there has not been a client added yet, add the clientList
    // Otherwise, find the last client in the list and add the client there
    if (clientList == NULL) {
        clientList = newClient;
        return clientList;
    }

    Client* currentClient = clientList; 
    while (currentClient != NULL) {
        // If the new client's name comes after in the alphabet, then
        // if there is not another client in the list, add and return,
        // otherwise set to the next client and iterate.
        if (strcasecmp(newClient->name, currentClient->name) > 0) {
            if (currentClient->next == NULL) {
                currentClient->next = newClient;
                return clientList;
            } else {
                currentClient = currentClient->next;
            }
        } else {
            Client* previousClient = 
                    get_previous_client(clientList, currentClient->name);
            if (previousClient != NULL) {
                previousClient->next = newClient;
            } else {
                clientList = newClient;
            }
            newClient->next = currentClient;
            return clientList;
        }
    }    
    return clientList;
}

Client* get_client(Client* clientList, char* name) {    
    
    Client* currentClient = clientList;

    // Iterate through all clients, checking by name for any matches to return
    while (currentClient != NULL) {
        if (!strcmp(currentClient->name, name)) {
            return currentClient;
        }
        currentClient = currentClient->next;
    }

    return NULL;
}

Client* get_previous_client(Client* clientList, char* name) {
    
    Client* currentClient = clientList;
    Client* previousClient = NULL;
    
    // Iterate through all clients, saving a pointer to the previous client
    while (currentClient != NULL) {
        if (!strcmp(currentClient->name, name)) {
            return previousClient;
        }
        previousClient = currentClient;
        currentClient = currentClient->next;
    }

    return NULL;
}

Client* remove_client(Client* clientList, char* name) {
    
    if (name == NULL) {
        return clientList;   
    }

    // Get necessary clients
    Client* clientPrevious = get_previous_client(clientList, name);
    Client* clientToRemove = get_client(clientList, name);

    // If the client in the list does not exist, do nothing
    if (clientToRemove == NULL) {
        return clientList;
    }

    // If we are at the first client in the list
    if (clientPrevious == NULL) {
        // Copy a pointer to the first client, move the head forward by one
        // and then free
        Client* tempClient = clientList;
        clientList = clientList->next;
        free_client(tempClient);
    
    // Otherwise, we can simply link up the previous clients and the
    // next clients ends and free the client in the middle
    } else {
        clientPrevious->next = clientToRemove->next;
        free_client(clientToRemove);
    }

    return clientList;
}

void add_to_server_stats(Server* server, int statCode) {
    // If stat code in range, increment stat counter
    if (statCode >= 0 && statCode < NUM_SERVER_STATS) {
        take_lock(server->statsAccess);
        server->stats[statCode]++;
        release_lock(server->statsAccess);
    }
}

void add_to_client_stats(Client* client, int statCode) {
    // If stat code in range, increment stat counter
    if (statCode >= 0 && statCode < NUM_CLIENT_STATS) {
        client->stats[statCode]++;
    }
}

void sigpipe_handler(int code) {
    ;
}

void print_server_stats(Server* server) {
    
    take_lock(server->clientAccess);

    // Iterate through all connected clients and print out statistics
    Client* currentClient = server->clientList;
    fprintf(stderr, "@CLIENTS@\n");
    while (currentClient != NULL) {
        volatile int* clientStats = currentClient->stats;
        fprintf(stderr, "%s:SAY:%d:KICK:%d:LIST:%d\n", 
                currentClient->name, clientStats[STAT_SAY], 
                clientStats[STAT_KICK], clientStats[STAT_LIST]);
        
        currentClient = currentClient->next;
    }
    release_lock(server->clientAccess);

    take_lock(server->statsAccess);

    // Then, print out all cumulative server stats
    volatile int* serverStats = server->stats;
    fprintf(stderr, "@SERVER@\n");
    fprintf(stderr, "server:AUTH:%d:NAME:%d:SAY:%d:KICK:%d:LIST:%d:LEAVE:%d\n",
            serverStats[STAT_AUTH], serverStats[STAT_NAME], 
            serverStats[STAT_SAY], serverStats[STAT_KICK], 
            serverStats[STAT_LIST], serverStats[STAT_LEAVE]);
 
    release_lock(server->statsAccess);
}
