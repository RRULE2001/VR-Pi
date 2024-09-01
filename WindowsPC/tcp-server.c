/**
 * @file server.c
 * @brief This program emulates as EUI-64 server that accepts connections from clients, displays
 * basic information about the connections, and supplies EUI-64 values back to the clients.
 * 
 * The server is executed in a command window on a Windows based system.
 * 
 * Kettering University CE-480, Phase 4
 * 
 * @authors Brenden Newman, Richard Rule
 * @date 11 December 2023
*/

/* Include(s) */
#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>
#include <stdlib.h> 
#include <time.h>
#include <synchapi.h>

/* Define(s) */
#define MSGRECVLENGTH   100     /*!< Receive buffer size in bytes. */
#define MSGSENDLENGTH   100     /*!< Transmit buffer size in bytes. */
#define SERVER_TIMEOUT  60      /*!< Idle timeout value in seconds. */

/**
 * @enum e_error
 * @brief List of return codes for the application.
*/
typedef enum e_error {
    ERROR_WINSOCK_INIT_FAILURE = -8,
    ERROR_WINSOCK_SOCKET_CREATE_FAILURE,
    ERROR_WINSOCK_SOCKET_CONNECT_FAILURE,
    ERROR_WINSOCK_SOCKET_SEND_FAILURE,
    ERROR_WINSOCK_SOCKET_BIND_FAILURE,
    ERROR_INPUT_ARGUMENT,
    ERROR_MEMORY_ALLOCATION,
    IDLE_TIMEOUT,
    SUCCESS_OK,
} ErrorCodes;

/**
 * @struct s_thread_t
 * @brief Link list of threads. Contains identification information and encapsulates data
 * to pass into the thread.
*/
typedef struct s_thread_t {
    int tid;                    /*!< Numerical ID of the thread. Used for human reference. */
    void *data;                 /*!< Pointer to a block of memory containg data to be passed into the thread. */
    uintptr_t handle;           /*!< OS assigned thread handle - address used to keep track of thread. */
    struct s_thread_t *next;    /*!< Pointer to the next thread in the list. */
} thread_t;

/**
 * @struct s_connection_t
 * @brief Contains information pertaining to a socket connection. Used for persistent connections
 * and to pass data to a thread for the designated connection.
*/
typedef struct s_connection_t {
    SOCKET comm;                        /*!< Holds the client socket information for the connection. */
    struct sockaddr_in addr;            /*!< Holds the client addressing informaiton. */
    char recvBuffer[MSGRECVLENGTH];     /*!< Receive buffer for the connection. */
    char transBuffer[MSGSENDLENGTH];    /*!< Transmit buffer for the connection. */
} connection_t;

/* Global(s) */
thread_t *g_head = NULL;                    /*!< Points to the beginning of the list of threads. */
thread_t *g_newThread = NULL;               /*!< Empty thread pointer for adding an item to the list. */
thread_t *g_oldThread = NULL;               /*!< Empty thread pointer for removing an item from the list. */
CHAR g_ipv4addr[INET_ADDRSTRLEN] = { "192.168.1.219" };   /*!< Global array for storing an IPv4 string. */
BOOL g_timeout = FALSE;                     /*!< Indicates when the server has timed out and should shut down. */

/* Function Prototype(s) */
void cleanup();
int createServer(SOCKET *server, int port);
void _callback(void *pThread);
void _idle(void *pThread);
void messageHandler(char *msg, char *buffer);

/**
 * @fn int main(int, char*[])
 * @brief The entry point of the program. It processes any input arguments and handles
 * the client connections on a multithreaded basis.
 * @param[in] argc  An interger containing the amount of input arguments
 * @param[in] argv  A character pointer to the array of input arguments
 * 
 * @retval An integer indiciating the success of the function
*/
int main(int argc, char* argv[])
{
    WSADATA wsa;
    int result = SUCCESS_OK;
    SOCKET server = INVALID_SOCKET;
    int tcpPort = 50000;
    int id = 0;

    // Initialize the Winsock library for socket operations
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        printf("main: Failed to initialize Winsock library - error code: %d\n", WSAGetLastError());
        return ERROR_WINSOCK_INIT_FAILURE;
    }

    // Check for input arguments
    if (argc > 3)
    {
        
        if (strcmp(argv[2], "-p") == 0 || strcmp(argv[2], "-P") == 0)
        {
            // Convert the input argument to an integer
            tcpPort = atoi(argv[3]);
            // If the input is an invalid integer or outside the valid port number bounds then report an error and exit
            if ((tcpPort == 0 && strcmp(argv[3], "0") == 0) || (tcpPort < 49152 || tcpPort > 65535))
            {
                printf("main: Input argument must be a valid dynamic TCP port number.\n");
                WSACleanup();
                return ERROR_INPUT_ARGUMENT;
            }
        }
    }

    // Create and bind the server
    result = createServer(&server, tcpPort);
    if (result != SUCCESS_OK)
    {
        printf("main: Failed to initialize server. Exiting program...\n");
        WSACleanup();
        return result;
    }

    // Start listening for clients and allow a maximum number of connections
    if (listen(server, SOMAXCONN) == SOCKET_ERROR)
    {
        printf("main: listen failed with error - %d", WSAGetLastError());
        closesocket(server);
        WSACleanup();
        return ERROR_WINSOCK_SOCKET_CONNECT_FAILURE;
    }
    int sockaddrlen = sizeof(struct sockaddr_in);
    connection_t conn, *temp = NULL;
    thread_t *thread = NULL;

    thread_t *idledemon = (thread_t*)malloc(sizeof(thread_t));
    if (idledemon == NULL)
    {
        // Memory allocation failed so free all threads if any were created
        printf("main: Failed to allocate memory block - %d Bytes\n", sizeof(thread_t));
        closesocket(server);
        WSACleanup();
        return ERROR_MEMORY_ALLOCATION;
    } else {
        connection_t *temp = (connection_t *)idledemon->data;
        temp = (connection_t*)malloc(sizeof(connection_t));
        if (temp == NULL)
        {
            printf("main: Failed to allocate memory block - %d Bytes\n", sizeof(thread_t));
            closesocket(server);
            WSACleanup();
            return ERROR_MEMORY_ALLOCATION;
        }
        temp->comm = server;
        idledemon->handle = _beginthread(_idle, 0, idledemon); // Start the thread with default stack size
    }

    while (1)
    {
        memset(&conn, 0, sizeof(connection_t));
        // Block until a new client is detected
        conn.comm = accept(server, (struct sockaddr*)&conn.addr, &sockaddrlen);
        if (conn.comm == INVALID_SOCKET && !g_timeout)
        {
            printf("main: Accpet failed with error code - %d\n", WSAGetLastError());
            continue;
        } else if (conn.comm == INVALID_SOCKET && g_timeout) {
            printf("main: Server has been idle for an extended period. Shutting down...\n");
            WaitForSingleObject((HANDLE)idledemon->handle, INFINITE); // Wait for the thread to finish
            free(idledemon);
            server == INVALID_SOCKET;
            break;
        }

        // Allocate a new thread block
        if ((thread = (thread_t*)malloc(sizeof(thread_t))) == NULL)
        {
            // Memory allocation failed so free all threads if any were created
            printf("main: Failed to allocate memory block - %d Bytes\n", sizeof(thread_t));
            result = ERROR_MEMORY_ALLOCATION;
            break;
        } else {
            thread->next = g_head; // Add new thread to beginning of the list
            g_head = thread; // Set the head to the new thread
            thread->tid = id++;
            // Allocate a block of memory for the data to be used in the thread
            if ((thread->data = (void*)malloc(sizeof(connection_t))) == NULL)
            {
                // Memory allocation failed so free all threads if any were created
                printf("main: Failed to allocate memory block - %d Bytes\n", sizeof(connection_t));
                result = ERROR_MEMORY_ALLOCATION;
                break;
            } else {
                // Copy the socket information to the thread
                temp = (connection_t*)thread->data;
                memcpy(temp, &conn, sizeof(connection_t));
                sprintf(temp->transBuffer, "You are connection %d", thread->tid);
                if (send(temp->comm, temp->transBuffer, (int)strlen(temp->transBuffer), 0) < 0)
                {
                    printf("main: Error transmitting on connection %d", thread->tid);
                    result = ERROR_WINSOCK_SOCKET_SEND_FAILURE;
                    break;
                }
                thread->handle = _beginthread(_callback, 0, thread); // Start the thread with default stack size
            }
        }

        temp = (connection_t*)g_head->data;
        // Print connection information to console
        InetNtop(AF_INET, &(temp->addr.sin_addr), (PSTR)g_ipv4addr, INET_ADDRSTRLEN);
        printf("Connected to client....\n\tIP Address: %s\n\tPort: %d\n", g_ipv4addr, ntohs(temp->addr.sin_port));
    }

    printf("main: Terminating server...\n");
    cleanup();
    if (server != INVALID_SOCKET)
        closesocket(server);
    WSACleanup();
    return result;
}

/**
 * @fn void cleanup()
 * @brief Cleanup function for clearing any allocated memory and closing connections.
 * @retval None.
*/
void cleanup()
{
    while(g_head != NULL)
    {
        WaitForSingleObject((HANDLE)g_head->handle, INFINITE); // Wait for the thread to finish
        free(g_head->data); // Free the data block
        // Delete the head from the list
        g_oldThread = g_head;
        g_head = g_head->next;
        free(g_oldThread); // Free the thread
    }
}

/**
 * @fn int createServer(SOCKET *, struct sockaddr_in *, int)
 * @brief Opens and connects and socket to be used as a server for accepting open connections
 * on any address.
 * @param[in,out] server        A SOCKET pointer containing the server ID.
 * @param[in] port              An integer containing the port number to use for the server.
 * 
 * @retval Returns zero if there were no errors and the server was created successfully.
*/
int createServer(SOCKET *server, int port)
{
    int result = SUCCESS_OK;
    struct sockaddr_in serverAddr = { 0 };

    // Check if input socket is already created
    if (*server != INVALID_SOCKET)
        closesocket(*server);

    // Create socket using IPv4 addressing and TCP socket streaming
    *server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (*server == INVALID_SOCKET)
    {
        printf("createConnection: Failed to create socket - %d\n", WSAGetLastError());
        result = ERROR_WINSOCK_SOCKET_CONNECT_FAILURE;
    } else {
        // Define connection source information
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);

        if (SOCKET_ERROR == bind(*server, (SOCKADDR*)&serverAddr, sizeof(serverAddr)))
        {
            printf("createConnection: Failed to bind socket\n");
            result = ERROR_WINSOCK_SOCKET_BIND_FAILURE;
        } else {
            inet_ntop(AF_INET, &(serverAddr.sin_addr), (LPSTR)g_ipv4addr, INET_ADDRSTRLEN);
            printf("Server established.... Listening on\n\tIP Address: %s\n\tPort: %d\n", g_ipv4addr, port);
        }
    }

    return result;
}

/**
 * @fn void _callback(void *)
 * @brief Callback function for a thread. Handles the communication for a connection
 * established on the respective thread.
 * @param[in] pData A pointer to the memory block containing the thread information and data
 * 
 * @retval None.
*/
void _callback(void *pData)
{
    int bytesRecv = 0;
    thread_t *thread = (thread_t*)pData; // Convert the input memory block to the thread block
    connection_t *data = (connection_t*)thread->data; // Convert the data block to the connection data
    srand((int)time(NULL)); // Generate a random seed for the connection

    while(thread->data != NULL)
    {
        memset(data->recvBuffer, 0, MSGRECVLENGTH); // Empty the receive buffer
        bytesRecv = recv(data->comm, data->recvBuffer, MSGRECVLENGTH-1, 0); // Block until a message is received
        if ((bytesRecv == 1) && (data->recvBuffer[0] == 'Q')) // Close the connection
        {
            printf("Connection %d: closing connection...\n", thread->tid);
            closesocket(data->comm);
            free(data);
            data = NULL;
            break;
        }
        else if ((bytesRecv == 1) && (data->recvBuffer[0] == 'R')) // Send a random EUI-64
        {
            // Generate a random EUI-64
            char value = 0;
            for (int i = 0; i < 20; i++)
            {
                if (i == 4 || i == 9 || i == 14)
                    data->transBuffer[i] = ':';
                else {
                    value = rand() % 16; // Generate a random decimal digit between 0-15
                    if (value >= 0 && value <= 9)
                        value += 0x30; // Convert to decimal character
                    else
                        value += 0x57; // Convert to hex character
                    data->transBuffer[i] = value;
                }
            }

            // Transmit the randomly generated EUI-64
            if (send(data->comm, data->transBuffer, (int)strlen(data->transBuffer), 0) < 0)
            {
                printf("Connection %d: Failed to send message\n", thread->tid);
            }
        }
        else if (bytesRecv > 0) // Non-command received
        {
            // Print out the received message
            printf("Connection  %d: \n", thread->tid);
            printf("Message Length: %d \n",bytesRecv);
            printf("Message Received: %s \n", data->recvBuffer);
            
            messageHandler(data->recvBuffer, data->transBuffer);
            if (send(data->comm, data->transBuffer, (int)strlen(data->transBuffer), 0) < 0)
            {
                printf("Connection %d: Failed to send message\n", thread->tid);
            }
        }
    }

    // Remove thread from list
    if (thread == g_head)
    {
        g_head = thread->next;
    } else {
        thread_t *prevThread = g_head;
        while (prevThread->next != thread) { prevThread = prevThread->next; }
        prevThread->next = thread->next;
    }
    free(thread);
}

/**
 * @fn void _idle(void *)
 * @brief Callback function for the idle thread. Keeps track of connections and thread count
 * to see how long the server has been idle. If left idle for an extended period, inities
 * shutdown sequence.
 * 
 * NOTE: Safety precaution for memory leakage to ensure the program is shut down properly.
 * @param[in] pData A pointer to the memory block containing the thread information and data
 * 
 * @retval None.
*/
void _idle(void *pThread)
{
    thread_t *idledemon = (thread_t*)pThread;
    connection_t *data = (connection_t*)idledemon->data;
    char term = 0;
    while(!g_timeout)
    {
        scanf("%c", &term);
        g_timeout = TRUE;
        closesocket(data->comm);
        free(data);
    }
}

/**
 * @fn void messageHandler(char*, char*)
 * @brief Converts a MAC Address string to a EUI-64 string
 * @param[in] msg       A character pointer to the string containing the MAC Address.
 * @param[out] buffer   A character pointer to a buffer to store the EUI-64 string.
 * 
 * @retval None.
*/
void messageHandler(char *msg, char* buffer)
{
    memset(buffer, 0, MSGSENDLENGTH); // clears out transmit buffer
    strcpy(buffer, msg); // copies input buffer to output buffer
}
