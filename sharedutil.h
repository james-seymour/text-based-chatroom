#ifndef SHARED_H
#define SHARED_H
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#define MAX_BUF 512
#define NUM_CLIENT_STATS 3

/* The ErrorCodes enum holds the specified exit codes for the client or server
 * to use whenever exiting.
 */
enum ErrorCodes {
    NORMAL = 0, USAGE = 1, COMMS = 2, KICKED = 3, FAILAUTH = 4
};

/* The HashedCommands enum holds the integer hashes for any of the valid 
 * messages sent to/from the client/server. The hash function used to generate
 * these hash values is the hash_input function, which is described in this
 * header file below.
 */
enum HashedCommands {
    WHO = 1078, NAME_TAKEN = 2213043, AUTH = 2844, MSG = 1013, KICK = 2958, 
    LIST = 3042, SAY = 1031, ENTER = 8740, LEAVE = 8931, NAME = 2991 
};

/* The Client datastructure holds all necessary variables for a client that 
 * connects to the server to function. The datastructure can be used on either
 * the clientside or the serverside to store information about a specific
 * client.
 *
 * name: A unique identifying name for the client, either given explicitly on 
 *  the clientside with command line arguments, or copied from the client on 
 *  the serverside.
 *
 * authString: A unique authString given to the client in its authfile 
 *  on the clientside only.
 *
 * writeLock: A lock that can be used when sending a message to this client, 
 *  to ensure mutual exclusion over other clients that also might want to send 
 *  this client a message.
 *
 * readHandle: A stdio file pointer that this client can use to read messages.
 *  On the clientside, the client can use this handle to read messages from the
 *  server. 
 *  On the serverside, the client in the server can use this to read messages
 *  from the client.
 *
 * writeHandle: A stdio file pointer that this client can use to send messages.
 *  On the clientside, the client can use this handle to send messages to the
 *  server. 
 *  On the serverside, the client in the server can use this to send messages
 *  to the client.
 *
 * next: A pointer to the "next" Client struct in the Client linked list. For
 *  any client instances on the clientside, this will never be updated. On the
 *  serverside, if this Client is not the last client in the client list, then
 *  this will point to the "next" Client struct instance in the list.
 *
 * stats: Stores the statistics of the messages sent by this client to the
 *  server. stats can be iterated through to access all statistics required, 
 *  and can be indexed using the Stats enumeration in the server.h header file.
 *  NOTE: the client only stores the STAT_SAY, STAT_KICK and STAT_LIST stats.
 *
 * isCommunicating: A flag that can be used serverside to ensure that
 *  even if the client is still sending messages, these messages are never
 *  processed on the serverside.
 */
typedef struct Client {
    char* name;
    char* authString;

    sem_t* writeLock;
    FILE* readHandle;
    FILE* writeHandle;

    struct Client* next;

    volatile int* stats;

    volatile int isCommunicating;
} Client;

/* The create_lock function initialises a lock which uses semaphores to ensure 
 * mutual exclusion between threads. The lock is initialised with only 1 thread
 * able to take the lock at any given time.
 *
 * Parameters:
 *      lock - A pointer to a newly allocated block of memory which will be 
 *          populated with the lock.
 * Returns:
 *      (sem_t)* - A pointer to the initialised lock.
 */
sem_t* create_lock(sem_t* lock);

/* The take_lock function blocks until it is able to take the lock it is given.
 *
 * Parameters:
 *      lock - A pointer to an initialised semaphore lock.
 */
void take_lock(sem_t* lock);

/* The release_lock function posts on the given lock, to allow other threads
 * access to the lock.
 *
 * Parameters:
 *      lock - A pointer to an initialised semaphore lock.
 */
void release_lock(sem_t* lock);

/* The hash_input function uses a simple hash algorithm to hash a string into
 * a unique integer, by using the ASCII values of the characters in the string.
 *
 * Parameters:
 *      input - A string of any length
 *
 * Returns:
 *      (int) - A hash value of the given input string. "Accepted" hash values
 *      for the client and server can be found in the HashedCommands enum above
 */
int hash_input(char* input);

/* The send_message function sends a message to/from a client. Any unrecognised
 * characters (ASCII value < 32), will be converted to '?' characters before 
 * sending. 
 *
 * Parameters:
 *      client - A client instance with valid read/write handles.
 *      message - A message to send from the client to the server, 
 *      or vice versa.
 *
 * Returns:
 *      (int) 0 - if a message was not specified.
 *      (int) 1 - if the message was successfully sent.
 */
int send_message(Client* client, char* message);

/* The receive_message function receives a message to/from a client. 
 *
 * Parameters:
 *      client - A client instance with valid read/write handles
 *      message - A message to receive from the client, either on clientside
 *      or serverside
 *
 * Returns:
 *      (int) 0 - if the client/server has reached EOF, or there is an error
 *          with the file handles.
 *      (int) 1 - if the message was successfully received.
 */
int receive_message(Client* client, char* buffer);

/* The handle_server_message function takes a message sent from the server,
 * parses the message, and displays this message to the user as per the spec.
 * This function can be used serverside whenever a client broadcasts a message
 * to all other clients, to echo all client messages to the server's stdout.
 * 
 * Parameters:
 *      message - A message sent from the server to the client
 */
int handle_server_message(char* message);

/* The setup_client function initialises all the necessary variables used in a 
 * Client struct datastructure. The client is initialised on the heap so that
 * both server/user threads have access to it on the clientside, and so that 
 * every other client also has access to this client on the serverside.
 * 
 * A description of all of the variables initialised in this function can be 
 * found in the declaration of the Client struct above.
 *
 * Parameters:
 *      socket - A socket file descriptor which can be opened into a read
 *          and write handle for the client to the server
 *      name - The name given to the client on startup
 *      authString - The authentication string given to the client on startup
 * 
 * Returns:
 *      (Client*) - A pointer to this client's instance which has just been 
 *          initialised.
 */
Client* setup_client(int socket, char* name, char* authString);

/* The free_client function frees all allocated memory given to a client
 * instance and takes it off the heap. This is necessary so that there are no
 * memory leaks possible when multiple clients join and leave the server.
 *
 * Parameters:
 *      client - An instance of a client that will have its memory cleaned up.
 */
void free_client(Client* client);
#endif
