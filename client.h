#ifndef CLIENT_H
#define CLIENT_H
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include "sharedutil.h"

/* The connect_to_server function sets up a connection from the client to the
 * server on the localhost over IPv4 with the TCP protocol.
 *
 * Parameters:
 *      port - the port number specified by the user to listen on
 * 
 * Returns:
 *      (int) socketFD - a socket file descriptor returned from socket(2)
 *      (int) 0 - if the connection failed
 */
int connect_to_server(char* port);

/* The authenticate_client method authenticates the client process clientside.
 * Once a connection has been established on client startup, the client will
 * receive messages from the server, and will send back the client's given auth
 * string if asked.
 *
 * Parameters:
 *      client - The main instance of the client datastructure
 *
 * Returns:
 *      (int) 0 - if authentication was unsuccessful (the server stopped
 *          communicating with the client during authentication)
 *      (int) 1 - if authentication was successful (the client received OK from
 *          the server)
 */
int authenticate_client(Client* client);

/* The resolve_client_name function negotiates the client's name with the 
 * server clientside. The client will send its given name to the server
 * whenever the server asks for it at this stage. If the client is told that
 * its name has already been taken by a client already connected to the server,
 * then it will increment its "name counter", and update its name by appending
 * this counter. 
 *
 * On successful name negotiation, the client will copy down its
 * most recent name. If name negotiation is unsuccessful, then the client exits
 * immediately.
 *
 * Parameters:
 *      client - The main instance of the client datastructure
 *
 * Returns:
 *      (int) 1 - On successful name negotiation with the server
 */
int resolve_client_name(Client* client);

/* The listen_to_user function is the main routine for the thread which listens
 * to user input (which is created in main). This routine receives any input
 * from the user (on stdin), and handles this input for the user to see, 
 * before sending an appropriate response to the server. 
 *
 * On exit, this thread will stop the execution of the thread which listens to
 * server commands (in order to properly free resources).
 *
 * Parameters:
 *      args - The main instance of the client datastructure
 *
 * Returns:
 *      NULL - On exit
 */
void* listen_to_user(void* args);

/* The listen_to_server function is the main routine for the thread which
 * listen to server input (which is created in main). This routine receives
 * any message/command from the server and handles this input in a way that
 * the user can easily read.
 *
 * On exit, this thread will stop the execution of the thread which listens to
 * user input (in order to properly free resources).
 *
 * Parameters:
 *      args - The main instance of the client datastructure
 *
 * Returns:
 *      NULL - On exit
 */
void* listen_to_server(void* args);

/* The handle_user_message function parses any input received from the user 
 * through stdin, by updating the contents in the buffer it has been given.
 * 
 * If the user sends a verbatim command to the server (indicated by a
 * leading '*' character), then this will be sent to the server. Otherwise,
 * it is assumed that the user just wants to send a chat message to the other
 * clients in the server, so this message is properly formatted before sending.
 *
 * Parameters:
 *      message - A message buffer containing the raw user input
 *
 * Returns:
 *      (int) 0 - if any normal message was sent to the server
 *      (int) LEAVE - if the user has indicated that they want to leave on the
 *          clientside.
 */
int handle_user_message(char* message);

/* The client_exit function frees up all memory given to a client instance, and
 * exits the process with the supplied exitCode.
 *
 * Parameters:
 *      exitCode - The code which the process will exit with
 *      client - An instance of the client which should be freed before exit
 */
void client_exit(int exitCode, Client* client);
#endif
