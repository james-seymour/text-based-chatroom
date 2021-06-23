#ifndef SERVER_H
#define SERVER_H
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include "sharedutil.h"
#define INF 1000000000
#define SECOND_IN_MS 100000

/* The Stats enum serves as an easy to read index for the statistics held
 * in the server. 
 */
enum Stats {
    STAT_SAY, STAT_KICK, STAT_LIST, STAT_AUTH, STAT_NAME, STAT_LEAVE
};

/* The Server datastructure is the overarching struct which holds all variables
 * that are necessary to run the server process.  
 *
 * serverSocket: A socket given to the server after connection (improve)
 * 
 * authString: A unique one line string that is required from all clients to
 *  enter the server.
 * 
 * clientAccess: A lock that should be used when accessing clientList,
 *  to ensures mutual exclusion between threads.
 * 
 * newClient: A pointer to the newest Client struct instance
 *  created by the server whenever a new client joins.
 * 
 * clientList: The head pointer to a linked list of Client struct instances
 *
 * statsAccess: A lock that should be used when accessing the server's stats,
 *  again to ensure mutual exclusion.
 * 
 * stats: Stores the statistics of the server's received messages. stats can be
 *  iterated through to access all statistics required, and can be indexed 
 *  using the Stats enumeration above.
 */
typedef struct Server {
    int serverSocket; 
    char* authString; 
    
    sem_t* clientAccess;
    struct Client* newClient;
    struct Client* clientList;

    sem_t* statsAccess;
    volatile int* stats;
} Server;

/* The SignalHandler datastructure allows access for a signal handling thread
 * to appropriately access server statistics. On a SIGHUP to the process,
 * a dedicated signal handling thread accesses the server and client statistics
 * through an instance of the server, and displays them.
 *
 * signalMask: A signal mask which allows the signal handling thread to block
 *  while waiting for a SIGHUP signal.
 *
 * server: The instance of the Server struct which the signal handling thread
 *  can pull statistics from.
 */
typedef struct SignalHandler {
    sigset_t* signalMask;
    Server* server;
} SignalHandler;

/* The initialise_client function allocates a new client instance, 
 * and updates the newest client in the server with this new instance.
 *
 * Then, a thread is created which calls the listen_to_client routine with an
 * instance of the server.
 * 
 * The new thread is set in a detached state, to ensure that if 
 * the thread exits (either from an error or if the user leaves), 
 * then resources allocated by pthread_create are reclaimed.
 *
 * Parameters:
 *      server - An instance of the main server datastructure.
 *      socket - A newly retrieved socket allowing connection between
 *          the client and the server
 */
void initialise_client(Server* server, int socket);

/* The listen_to_client function is the main routine for the client handling 
 * threads serverside. This routine first grabs the newest client it has been
 * allocated from the server, before immediately validating authentication and
 * name negotiation. 
 * 
 * If the client is authenticated, then this function will
 * listen to messages from the client, and appropriately handle them.
 * 
 * There is a delay of 100ms between parsing messages to rate limit client and
 * reduce spam.
 *
 * When the client leaves, other clients are notified of this, and the client
 * is removed from the server.
 *
 * Parameters:
 *      args - An instance of the main server datastructure
 *
 * Returns:
 *      NULL
 */
void* listen_to_client(void* args);

/* The validate_authentication function validates a client's authentication
 * string, by asking for the client's auth string and checking it against
 * the server's auth string. 
 *
 * If the client's authentication does not match, or the client does not send
 * an auth string in the correct format (AUTH:<auth_string>), then the client
 * is kicked. Otherwise, the client passes authentication.
 *
 * Parameters:
 *      server - An instance of the main server datastructure
 *      client - An instance of the client seeking authentication
 *
 * Returns:
 *      (int) 0 - if the client fails authentication
 *      (int) 1 - if the client passes authentication
 */
int validate_authentication(Server* server, Client* client);

/* The validate_client_name functions recursively validates whether a client's
 * name that it would like to be called is valid in the server. 
 * 
 * This functions first asks for the client's name, before checking it against
 * the names of any other clients already in the server. If there is a match, 
 * the client is told that name has already been taken, and the function is 
 * called again. Otherwise, the client's given name is saved to the client
 * instance.
 *
 * Parameters:
 *      server - An instance of the main server datastructure
 *      client - An instance of the client seeking name validation
 *
 * Returns:
 *      (int) 0 - if the client fails name negotiation
 *      (int) 1 - if the client passes name negotiation
 */
int validate_client_name(Server* server, Client* client);

/* The handle_client_message function asks for any general input from a
 * valid, connected client, and parses this message. 
 *
 * If the client wants to say
 * something to the chat, is requesting a list of all connected users, would
 * like to kick a user, or would like to leave, then the server handles this
 * appropriately. Otherwise, the input is ignored.
 *
 * Parameters:
 *      server - An instance of the main server datastructure
 *      client - An instance of a client which has sent a message to the server
 *      message - A message which was sent by the client to the server
 *
 * Returns:
 *      (int) 0 - if the client is no longer communicating with the server,
 *          but is still connected (can be the case if the client is kicked
 *          but there are still messages in its buffer).
 *      (int) 1 - if the client has sent any message.
 */
int handle_client_message(Server* server, Client* client, char* message);

/* The broadcast_to_clients function broadcasts a message to all valid, 
 * connected clients in the server. It also emits a readable version of the 
 * message to the server's stdout.
 *
 * Parameters:
 *      clientList - A pointer to the head of the client linked list
 *      message - A message to broadcast to all of the clients in the list
 */
void broadcast_to_clients(Server* clientList, char* message);

/* The kick_client function finds a client instance by a given name, and 
 * attempts to kick this client if this client is connected to the server.
 *
 * Once the client has been kicked, they are not allowed to communicate
 * with the server in the time it takes them to leave.
 *
 * Parameters:
 *      server - An instance of the main server datastructure
 *      name - The name of the client which has been requested to be kicked
 *
 */
void kick_client(Server* server, char* name);

/* The update_active_client_list function concatenates all of the valid,
 * connected clients' names into a buffer, and formats this into a valid
 * LIST message to send to the client which has requested it.
 *
 * Parameters:
 *      server - An instance of the main server datastructure
 *      messageBuffer - A buffer which can be updated with the new list
 *          so that the calling function can access it
 */
void update_active_client_list(Server* server, char* messageBuffer);
#endif
