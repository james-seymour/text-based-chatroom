#ifndef SERVERUTIL_H
#define SERVERUTIL_H
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include "sharedutil.h"
#include "server.h"
#define NUM_SERVER_STATS 6

/* The setup_server_connection function sets up a server on the localhost
 * using IPv4 with the TCP protocol. 
 *
 * It first creates a socket file descriptor on the specified port 
 * as a communication end-point, and then attaches the localhost address 
 * to this socket. The server then listens on this port to indicate 
 * willingness to accept connections.
 *
 * If an ephemeral port is used (either not specified or specified as 0),
 * then the server will output the free port it is given by the kernel to 
 * stderr.
 *
 * If any of the above connection steps fail, then the server exits with a 
 * communications error.
 *
 * Parameters:
 *      port - the port number which the server should listen on
 *
 * Returns:
 *      (int) 0 - if the server could not setup a connection
 *      (int) 1 - if the server successfully setup a connection.
 */
int setup_server_connection(char* port);

/* The setup_server_instance function initiliases the main server datastructure
 * that is used by the server to keep track of all necessary variables. The
 * server is initiliased on the heap so that every client thread has access
 * to its variables. A description of all of the variables in the server struct
 * can be found in the server.h header file.
 *
 * Parameters:
 *      authString - The auth string given in the authfile when the server is 
 *          created.
 * Returns:
 *      (Server*) - A pointer to the main server datastructure which has just
 *          been initiliased.
 */
Server* setup_server_instance(char* authString);

/* The initialise_sighup_handler function creates a pthread signal mask which
 * blocks on SIGHUP when sigwait is called. It then creates a dedicated signal
 * handling thread which given this signal mask, and an instance of the server.
 * The signal handling thread is run with the sigholdup_handler routine, and is
 * run in a detached state so that pthread memory is freed on exit.
 *
 * Parameters: 
 *      server - An instance of the main server datastructure
 */
void initialise_sighup_handler(Server* server);

/* The sigholdup_handler function is the main routine for the SIGHUP signal
 * handling thread. 
 *
 * If sigwait(3) does not return an error, then the thread simply blocks until 
 * the process is sent a SIGHUP. When a SIGHUP is received, this thread will 
 * print the server's statistics and then wait for another SIGHUP. 
 *
 * If there is while error calling sigwait(3), then the thread exits.
 *
 * Parameters:
 *      args - A signal handling datastructure containing a pointer to the
 *          server and to a signalmask 
 */
void* sigholdup_handler(void* args);

/* The add_client function adds a client instance to the main linked list of 
 * client instances held in the server. The client list is iterated through,
 * checking at each client whether the new client's name is after the current
 * client's name in the alphabet. When the position to place the new client
 * in the list is found alphabetically, then the client is inserted into the
 * linked list at that position.
 *
 * Parameters:
 *      clientList - A pointer to the head of the server's client list
 *      newClient - A pointer to a new client instance that should be inserted
 *          into the client list
 * Returns:
 *      (Client*) - An updated pointer to the head of the client list, which
 *          the server can then update to save the client list changes
 */
Client* add_client(Client* clientList, Client* newClient);

/* The get_client function finds a client by its unique name in the 
 * client list.
 *
 * Parameters:
 *      clientList - A pointer to the head of the server's client list.
 *      name - A unique (not null) name to search for in the client list.
 *
 * Returns:
 *      (Client*) - A pointer to the client in the client list matching the 
 *          given name
 *      NULL - there is no client in the client list matching the given name 
 */
Client* get_client(Client* clientList, char* name);

/* The get_previous_client function finds the client in the client list 
 * preceeding the client specified by a name. This is a necessary helper
 * function when adding/removing clients, as a doubly linked list is not used.
 *
 * Parameters:
 *      clientList - A pointer to the head of the server's client list.
 *      name - A unique (not null) name to search for in the client list.
 * 
 * Returns:
 *      (Client*) - A pointer to the client in the client list which is
 *          directly preceeding the client which is matched by a given name.
 *      
 *      NULL - If the matched client has no previous client (i.e it is the
 *          first client in the client list)
 */
Client* get_previous_client(Client* clientList, char* name);

/* The remove_client function removes a client specified by a name from the
 * server's client list. The client to remove is found and its allocated 
 * memory is freed. The hanging ends of the clients either side of the client 
 * that has been removed are then linked back up.
 *
 * Parameters:
 *      clientList - A pointer to the head of the server's client list.
 *      name - A unique (not null) name to search for and remove a client from
 *          the list, if the client exists.
 *
 * Returns:
 *      (Client*) - An updated pointer to the head of the client list, which
 *          the server can then update to save the client list changes.
 */
Client* remove_client(Client* clientList, char* name);

/* The add_to_server_stats function increments a statistic in the server by 1
 * if a valid statCode index is given (see Stats enumeration in server.h 
 * for valid codes). Otherwise, it does nothing.
 *
 * Parameters:
 *      server - The main server datastructure
 *      statCode - The index at which the server stat should be incremented
 */
void add_to_server_stats(Server* server, int statCode);

/* The add_to_client_stats function increments a statistic in a client by 1
 * if a valid statCode index is given (see Stats enumeration in server.h 
 * for valid codes). Otherwise, it does nothing.
 *
 * Parameters:
 *      server - The main server datastructure
 *      statCode - The index at which the server stat should be incremented
 */
void add_to_client_stats(Client* client, int statCode);

/* The print_server_stats function grabs all currently connected client stats,
 * and the cumulative server stats, formats them, and displays them 
 * on the server's stdout.
 *
 * Parameters:
 *      server - The main server datastructure
 */
void print_server_stats(Server* server);

/* Empty function to ignore SIGPIPE signals received by disconnecting clients.
 *
 * Parameters:
 *      code - The code for the signal sent to the running server process
 *
 */
void sigpipe_handler(int code);
#endif
