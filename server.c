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
#include "serverutil.h"
#include "sharedutil.h"

int main(int argc, char* argv[]) {

    // Grab Auth string
    FILE* authFilePath = fopen(argv[1], "r");
    if (authFilePath == NULL) {
        fprintf(stderr, "Usage: server authfile [port]\n");
        exit(USAGE);
    }
    char authBuffer[MAX_BUF];
    char* auth = strtok(fgets(authBuffer, MAX_BUF - 1, authFilePath), "\n");
    fclose(authFilePath);

    // Ignore sigpipe signals
    struct sigaction sa;
    sa.sa_handler = sigpipe_handler;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGPIPE, &sa, 0);

    // Setup server connection
    char* port = argv[2] ? argv[2] : "0";
    int serverSocket = setup_server_connection(port);
    if (!serverSocket) {
        fprintf(stderr, "Communications error\n");
        exit(COMMS);
    }

    Server* server = setup_server_instance(auth);
    initialise_sighup_handler(server);

    // Accept new client connections
    int clientSocket;
    while ((clientSocket = accept(serverSocket, 0, 0))) {
        initialise_client(server, clientSocket);
    }

    // We do not free server's fixed allocated memory in the server instance,
    // because it should never terminate unless it receives SIGQUIT/SIGKILL, 
    // in which case it will not free memory anyway.
    return 1;
}

void initialise_client(Server* server, int socket) {
    
    // Main server thread never needs to join on this thread because as soon
    // as the thread is finished reading, it exits
    take_lock(server->clientAccess);

    // Create a new client instance for the thread to use
    Client* newClient = setup_client(socket, NULL, server->authString);
    server->newClient = newClient;
        
    // Set thread in a detached state so that resources are freed on exit 
    pthread_t tid;
    pthread_create(&tid, 0, listen_to_client, server);
    pthread_detach(tid);

}

void* listen_to_client(void* args) {
    Server* server = (Server*) args;
    // Use the newClient pointer in server to find this thread's client
    Client* myClient = server->newClient;
    
    char buffer[MAX_BUF];
    // If auth invalid, simply exit the client thread
    if (!validate_authentication(server, myClient) || 
            !validate_client_name(server, myClient)) {
        
        free_client(myClient);
        release_lock(server->clientAccess);
        return NULL;
    
    } else {

        // If valid, then add the client to the server list alphabetically.
        server->clientList = add_client(server->clientList, myClient);
        sprintf(buffer, "ENTER:%s", myClient->name);
        broadcast_to_clients(server, buffer);
        release_lock(server->clientAccess);
    }

    // Main message loop
    while (receive_message(myClient, buffer)) {
        int response = handle_client_message(server, myClient, buffer);
        if (response == LEAVE) {
            break;
        }
        usleep(SECOND_IN_MS);
    }

    // Notify of this client's exit and remove client from the client list
    sprintf(buffer, "LEAVE:%s", myClient->name);
    take_lock(server->clientAccess);
    server->clientList = remove_client(server->clientList, myClient->name);
    broadcast_to_clients(server, buffer);
    release_lock(server->clientAccess);

    return NULL; 
}

int validate_authentication(Server* server, Client* client) {
   
    // Receive the authstring from the client
    char buffer[MAX_BUF];
    send_message(client, "AUTH:");
    receive_message(client, buffer);
    char* auth = strtok(buffer, ":");
    char* clientAuthString = strtok(NULL, "\n");
   
    // If a valid AUTH command, add to server stats
    if (hash_input(auth) == AUTH) {
        add_to_server_stats(server, STAT_AUTH);
    }

    // If the client has sent an AUTH string, then if the string matches,
    // allow the client into the server
    if (clientAuthString != NULL) {
        if (!strcmp(clientAuthString, server->authString)) {
            send_message(client, "OK:");
            return 1;
        } else {
            return 0;
        }
    } else {
        return 0;
    }

}

int validate_client_name(Server* server, Client* client) {
    
    char buffer[MAX_BUF];
    send_message(client, "WHO:");
    receive_message(client, buffer);
    char* name = strtok(buffer, ":");
    char* clientName = strtok(NULL, "\n");
    
    // If the client has sent an invalid input, reject authentication
    if (clientName == NULL || name == NULL || strcmp(name, "NAME")) {
        return 0;
    }

    add_to_server_stats(server, STAT_NAME);
    Client* currentClient = server->clientList;
   
    // Iterate through the currently connected clients and check for matching
    // names.
    while (currentClient != NULL) {
        if (!strcmp(clientName, currentClient->name)) {
            send_message(client, "NAME_TAKEN:");
            // Recursively call validate client to validate
            return validate_client_name(server, client);
        } else {
            currentClient = currentClient->next;
        } 
    }
   
    // If reached, we have successfully iterated through the client list
    // without finding a matching name, so we can copy down that name and allow
    // the client into the server
    client->name = strcpy(realloc(client->name, 
            sizeof(char) * (strlen(clientName) + 2)), clientName);
    send_message(client, "OK:");
    return 1;
}

int handle_client_message(Server* server, Client* client, char* message) {
   
    if (!client->isCommunicating) {
        return 0;
    }
    
    // Thread safe version of strtok is not used as buffers are never shared
    // between threads.
    char messageBuffer[MAX_BUF];
    char* command = strtok(message, ":");
    char* optArg1 = strtok(NULL, "\n");
    int hashCommand = hash_input(command);

    switch (hashCommand) {
        case SAY:
            add_to_client_stats(client, STAT_SAY);
            add_to_server_stats(server, STAT_SAY);
            sprintf(messageBuffer, "MSG:%s:%s", client->name, optArg1);
            take_lock(server->clientAccess);
            broadcast_to_clients(server, messageBuffer);
            release_lock(server->clientAccess);
            break;
        case KICK:
            add_to_client_stats(client, STAT_KICK);
            add_to_server_stats(server, STAT_KICK);
            kick_client(server, optArg1);
            break;
        case LIST:
            add_to_client_stats(client, STAT_LIST);
            add_to_server_stats(server, STAT_LIST);
            update_active_client_list(server, messageBuffer);
            send_message(client, messageBuffer);
            break;
        case LEAVE:
            add_to_server_stats(server, STAT_LEAVE);
            return LEAVE;
    }

    return 1;
}

void broadcast_to_clients(Server* server, char* message) {

    // Output the client's message to server's stdout
    char messageCopy[strlen(message) + 1];
    strcpy(messageCopy, message);
    handle_server_message(messageCopy);

    // Send the same message to all other clients (to handle clientside)
    Client* currentClient = server->clientList;
    while (currentClient != NULL) {
        if (currentClient->isCommunicating) {
            send_message(currentClient, message);
        }
        currentClient = currentClient->next;
    }

}

void kick_client(Server* server, char* name) {
    
    if (name != NULL) {
        // Grab client to kick
        take_lock(server->clientAccess);
        Client* clientToKick = get_client(server->clientList, name);
        release_lock(server->clientAccess);
        
        // If this client exists, kick client.
        if (clientToKick != NULL) {
            send_message(clientToKick, "KICK:");
            clientToKick->isCommunicating = 0;
        }
    }

}

void update_active_client_list(Server* server, char* messageBuffer) {
    
    take_lock(server->clientAccess);
    sprintf(messageBuffer, "LIST:");
   
    // Grab all client's names and add to a buffer
    Client* currentClient = server->clientList;
    while (currentClient->next != NULL) {
        strcat(messageBuffer, currentClient->name);
        strcat(messageBuffer, ",");
        currentClient = currentClient->next;
    }
    // Add final client name without comma
    strcat(messageBuffer, currentClient->name);
    release_lock(server->clientAccess);
}
