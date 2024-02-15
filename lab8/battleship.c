#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>

int fd = 0;

/**
 * Flag to indicate if the user wants to forfeit the game.
 * Ensures that the program correctly notices the change made by the
 * signal handler to this variable during its execution. 
*/
volatile sig_atomic_t forfeitFlag = 0;

#define FORFEIT 127
#define BACKLOG 1
#define SEND_RETRY_LIMIT 3
#define GRID_WIDTH 4
#define GRID_SIZE (GRID_WIDTH * GRID_WIDTH)
#define PORT_MIN 1024
#define PORT_MAX 65535
#define PORT_DEFAULT 5001
#define LOCALHOST "127.0.0.1"
#define BOAT_NUM 3
#define CLEAR_INPUT_BUFFER() \
    do { \
        int c; \
        while ((c = getchar()) != '\n' && c != EOF); \
    } while (0)

typedef enum
{
    DEFAULT = 0, BOAT, MISSED_SHOT, SUNKEN_BOAT
}gridState;

typedef enum
{
    SERVER_LISTEN = 0,
    SERVER_CLIENT_CONNECT,
    CLIENT_CONNECTING,
    CLIENT_CONNECTED,
    BOAT_PLACE,
    OPPONENT_WAIT,
    ATTACK_CELL,
    USER_ATTACK,
    OPPONENT_ATTACK,
    MISS,
    HIT,
    USER_GRID,
    OPPONENT_GRID,
    USER_WON,
    USER_LOST,
    INV_CELL_ATTACKED,
    INV_CELL_VAL,
    INV_BOAT_STACK,
    USER_FORFEIT,
    OPPONENT_FORFEIT,
    SERV_SOCK_FAIL,
    CLI_SOCK_FAIL,
    SERV_BIND_FAIL,
    SERV_LISTEN_FAIL,
    SERV_CLI_CONN_FAIL,
    CLI_CONN_FAIL,
    CLI_IP_RET_FAIL,
    RECV_FAIL,
    RECV_CONN_CLOSED,
    SEND_FAIL,
    INV_IP_ADDR,
    INV_PORT,
    INV_PORT_RANGE,
    SEND_NULL_BUFFER,
    READ_NULL_BUFFER,
    GRID_NULL_ERR,
    GRID_INV_SYMBOL,
    ARG_IP_ERR,
    ARG_PORT_ERR,
    UNKNOWN_ARG_ERR,
    PROG_USAGE
}stringsIdx;

const char *strings[] = {
        "Server listening on port",
        "Client connected",
        "Connecting to server",
        "Connected to server successfully",
        "Please enter 3 grid cells to place your boats into",
        "Waiting for opponent...",
        "Please enter a grid cell to attack your opponent",
        "You attacked",
        "Your opponent attacked",
        "MISS",
        "HIT",
        "User grid",
        "Opponent grid",
        "YOU WON ^_^",
        "YOU LOST T_T",
        "Invalid ! You already attacked this cell",
        "Invalid ! Please enter a grid cell index between 0-F",
        "Invalid ! You cannot place 2 boats on the same grid cell",
        "You forfeited the game",
        "Your opponent forfeited the game",
        "Server socket creation failed",
        "Client socket creation failed",
        "Server: binding failed",
        "Server: listen failure",
        "Server: client connection failed",
        "Client: Connection failed",
        "Client: error occurred ip address conversion",
        "recv failed",
        "No bytes received, the connection was closed",
        "Send failed",
        "Invalid IP address: NULL pointer provided.",
        "Invalid port: ",
        "Port must be between 1024 and 65535.",
        "Error: Attempted to send data from a NULL buffer.",
        "Error: Attempted to read data into a NULL buffer.",
        "Error: Attempted to print a NULL grid.",
        "Error: Invalid symbol in grid.",
        "Error: -a option requires an IP address.",
        "Error: -p option requires a port number.",
        "Error: Unknown argument:",
        "Usage: [program_name] [-s] [-a ip_address] [-p port]"
};

/**
 * @brief Forfeits the game when receiving SIGSTP (Ctrl+Z) signal
 *
 * Signal routine to override default behaviour of SIGSTP. When this routine is called due to the reception of SIGSTP (Ctrl+Z in the shell) it sends a message on the socket to warn that the user wants to forfeit the game and then closes the socket file descriptor before exiting
*/
void quitGame(int sig)
{
    if (sig == SIGTSTP) {
        forfeitFlag = 1;
    }
}

/**
 * @brief Converts a single hexadecimal character to its decimal value
 *
 * The hexadecimal digit may be lowercase or uppercase
 *
 * @param digit the hexadecimal digit to convert
 *
 * @returns The decimal value of the hexadecimal digit given or -1 if the given digit isn't hexadecimal
*/
int charToHex(const int digit) {
    if(digit >= '0' && digit <= '9') return digit - '0';

    int lowerDigit = tolower(digit);
    if(lowerDigit >= 'a' && lowerDigit <= 'f') return 10 + (lowerDigit - 'a');

    return -1;
}

/**
 * @brief Recovers a hexadecimal digit representing a grid cell from the user and returns its decimal value
 *
 * Only the first input hexadecimal digit is returned. All other input characters are safely discarded. If the given hex digit is invalid, an error message is displayed and a new input is required
 *
 * @returns The decimal value of the selected grid cell
*/
int promptCell() {

    while (1) {
        int hexValue = charToHex(getchar());

        if (forfeitFlag) {
            return FORFEIT;
        }

        if (hexValue != -1) {
            CLEAR_INPUT_BUFFER();
            return hexValue; // Return the decimal value of the hex digit
        }

        printf("%s\n", strings[INV_CELL_VAL]);
        CLEAR_INPUT_BUFFER();
    }
}

/**
 * @brief Prints a grid
 *
 * Prints the given board with the following pattern and symbols:
 * ~ ~ ~ 0
 * ~ o ~ ~
 * ~ 0 ~ o
 * ~ ~ ~ X
 *
 * ~: Default (water)
 * 0: Boat
 * o: Missed shot
 * X: Sunken boat
*/
void printBoard(const char *grid) {
    if (grid == NULL) {
        printf("%s\n", strings[GRID_NULL_ERR]);
    }
    
    const char symbols[] = "~0oX";

    for (int i = 0; i < GRID_SIZE; i++) {
        char gridSymbol = grid[i];

        // Ensure gridSymbol is within the valid range for symbols
        if (gridSymbol < DEFAULT || gridSymbol > SUNKEN_BOAT) {
            printf("%s: %c\n", strings[GRID_INV_SYMBOL], gridSymbol);
            return;
        }

        printf("%c ", symbols[gridSymbol]);

        if ((i + 1) % GRID_WIDTH == 0) {
            printf("\n");
        }
    }
}

/**
 * @brief Places boats on the user's grid.
 *
 * Allows the user to place a specified number of boats on the grid, validating each placement.
 *
 * @param grid The grid where boats will be placed.
 * @param boatNum The number of boats to place.
 */
void placeBoatsOnGrid(char *grid, int boatNum) {
    printf("%s\n", strings[BOAT_PLACE]);

    int boatPlacements = 0;
    while (boatPlacements < boatNum) {
        int cell = promptCell();

        // Check if the cell is not already occupied
        if (grid[cell] != DEFAULT) {
            printf("%s\n", strings[INV_BOAT_STACK]);
        } else {
            grid[cell] = BOAT;
            boatPlacements++;
        }
    }

    printBoard(grid);
}

/**
 * @brief Recovers size bytes from the socket_fd file descriptor and stores them in the buf buffer
 * 
 * The recv call is blocking, except if interrupted by a signal. In that case, the function returns -1.
 * This allows the possibility to the defender to forfeit while waiting for the opponent's attack.
 *
 * @returns -1 if an error occurred, 0 otherwise
*/
int socketRecv(const int sock_fd, char *buf, const size_t size) {

    if (buf == NULL) {
        printf("%s\n", strings[READ_NULL_BUFFER]);
        return -1;
    }

    size_t totalBytesRead = 0;
    ssize_t bytesRead;

    // Exits when the total number of bytes read = requested size (partial reads handled)
    while (totalBytesRead < size) {
        bytesRead = recv(sock_fd, buf + totalBytesRead, size - totalBytesRead, 0);

        // The recv call was interrupted by a signal (probably SIGTSTP => forfeit)
        if(errno == EINTR){
            return -1;
        }

        if (bytesRead == -1) {
            printf("%s\n", strings[RECV_FAIL]);
            return -1;
        }

        if (bytesRead == 0) {
            printf("%s\n", strings[RECV_CONN_CLOSED]);
            return -1;
        }

        totalBytesRead += bytesRead;
    }

    return 0;
}

/**
 * @brief Sends size bytes from the buf buffer to the sock_fd file descriptor
 *
 * @returns -1 if an error occurred, 0 otherwise
*/
int socketSend(const int sock_fd, char *buf, const size_t size) {
    
    if (buf == NULL) {
    printf("%s\n", strings[SEND_NULL_BUFFER]);
    return -1;
}

    // handle partial writes (when send writes less than size bytes)
    size_t totalBytesSent = 0;
    ssize_t bytesSent;
    int retryCount = 0;

    while (totalBytesSent < size) {
        bytesSent = send(sock_fd, buf + totalBytesSent, size - totalBytesSent, 0);

        // Tries to send again if the send failed (may be due to signal interruption)
        if (bytesSent == -1) {
            if (++retryCount > SEND_RETRY_LIMIT) {
                printf("%s %s %d %s\n", strings[SEND_FAIL], "after", SEND_RETRY_LIMIT, "retries, aborting.");
                return -1;
            }
            continue;
        }

        totalBytesSent += bytesSent;
        retryCount = 0; // Reset retry count on successful send
    }

    return 0;
}

/**
 * @brief Creates a server that waits for a client before returning
 *
 * The server is created and then waits for a client to connect with accept. Once a client is connected, the server's file descriptor is closed and the client's file descriptor is returned
 *
 * @returns -1 if an error occurred, the client's file descriptor as returned by accept otherwise
*/
int server_create(const int port) {
    int sock_fd, conn_fd;
    struct sockaddr_in serv_addr;

    // socket create and verification
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        printf("%s\n", strings[SERV_SOCK_FAIL]);
        return -1;
    }

    // Forcefully attaching socket to the port
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    // Binding newly created socket to given IP and verification
    if(bind(sock_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0){
        printf("%s\n", strings[SERV_BIND_FAIL]);
        close(sock_fd);
        return -1;
    }

    // Now server is ready to listen (only 1 client)
    if ((listen(sock_fd, BACKLOG)) != 0) {
        printf("%s\n", strings[SERV_LISTEN_FAIL]);
        close(sock_fd);
        return -1;
    }
    printf("%s %d...\n", strings[SERVER_LISTEN], port);

    conn_fd = accept(sock_fd, (struct sockaddr *) NULL, NULL);

    // connection established, no need for server socket anymore
    close(sock_fd);

    if (conn_fd < 0) {
        printf("%s\n",strings[SERV_CLI_CONN_FAIL]);
        exit(-1);
    }

    printf("%s\n", strings[SERVER_CLIENT_CONNECT]);
    return conn_fd;
}

/**
 * @brief Creates a client that connects to a server before returning
 *
 * @returns -1 if an error occurred, the file descriptor of the server as returned by connect otherwise
*/
int client_connect(const char *ip_addr, const int port) {
    int sock_fd;
    struct sockaddr_in serv_addr;

    // Check IP address validity
    if (ip_addr == NULL) {
        printf("%s\n", strings[INV_IP_ADDR]);
        return -1;
    }

    // Check port validity
    if (port < PORT_MIN || port > PORT_MAX) {
        printf("%s%d %s\n", strings[INV_PORT], port, strings[INV_PORT_RANGE]);
        return -1;
    }

    printf("%s %s:%d...\n", strings[CLIENT_CONNECTING], ip_addr, port);

    // socket creation and verification
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("%s\n", strings[CLI_SOCK_FAIL]);
        return -1;
    }

    // Initialize the server address
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // Convert IPv4 address from text to binary form
    if (inet_pton(AF_INET, ip_addr, &serv_addr.sin_addr) <= 0) {
        printf("%s\n", strings[CLI_IP_RET_FAIL]);
        close(sock_fd);
        return -1;
    }

    // connect the client socket to server socket
    if (connect(sock_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        printf("%s\n", strings[CLI_CONN_FAIL]);
        close(sock_fd);
        return -1;
    }

    printf("%s\n", strings[CLIENT_CONNECTED]);
    return sock_fd;
}

/**
 * @brief Checks if the opponent forfeited the game
 *
 * @returns FORFEIT if the opponent forfeited, 0 otherwise
*/
int checkOpponentForfeit(char data) {
    if(data == FORFEIT){
        printf("%s: %s\n", strings[OPPONENT_FORFEIT], strings[USER_WON]);
        return FORFEIT;
    }
    return 0;
}

/**
 * @brief Main function of the program
 *
 * Parses the command line arguments and initializes the game according to the role (server or client). Then runs the game until the user forfeits or wins/loses
 *
 * @returns EXIT_SUCCESS if the program exited successfully, EXIT_FAILURE otherwise
*/
int main(int argc, char const *argv[]){

    char userGrid[GRID_SIZE], advGrid[GRID_SIZE];
    const char *ip_addr = LOCALHOST;
    int server = 0, port = PORT_DEFAULT;

    for (size_t i = 1; i < argc; i++) {

        // Check if the argument is "-s" (server mode)
        if (strcmp(argv[i], "-s") == 0) {
            server = 1;
        }
        // Check if the argument is "-a" (IP address)
        else if (strcmp(argv[i], "-a") == 0) {

            // Ensure there's another argument after "-a"
            if (i + 1 < argc) {
                ip_addr = argv[++i];
            } else {
                printf("%s\n", strings[ARG_IP_ERR]);
                return -1;
            }
        }
        // Check if the argument is "-p" (port number)
        else if (strcmp(argv[i], "-p") == 0) {

            // Ensure there's another argument after "-p"
            if (i + 1 < argc) {
                port = atoi(argv[++i]);
            } else {
                printf("%s\n", strings[ARG_PORT_ERR]);
                return -1;
            }
        }
        // If the argument is unknown, print usage instructions
        else {
            printf("%s %s\n",strings[UNKNOWN_ARG_ERR], argv[i]);
            printf("%s\n", strings[PROG_USAGE]);
            return -1;
        }
    }

    // Initialize game boards
    memset(userGrid, DEFAULT, sizeof(userGrid));
    memset(advGrid, DEFAULT, sizeof(advGrid));

    // Place boats on the user's grid
    placeBoatsOnGrid(userGrid, BOAT_NUM);

    // Init socket communication according to role
    if (server)
        fd = server_create(port);
    else
        fd = client_connect(ip_addr, port);

    if (fd == -1)
        return EXIT_FAILURE;

    // Register signal handler respecting the POSIX standard
    struct sigaction sa;
    sa.sa_handler = quitGame;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTSTP, &sa, NULL);

    // Run game according to role
    int userBoats = BOAT_NUM, advBoats = BOAT_NUM;
    char data = 0, usrInput = 0;
    int firstIter = server; //Server plays first

    while(!forfeitFlag){
        if(firstIter % 2){ //Here the player (attacker) sends it's choice

            printf("%s\n", strings[ATTACK_CELL]);
            usrInput = promptCell();

            // The attacker has decided to forfeit => send message to the opponent and exit
            if (usrInput == FORFEIT) {
                socketSend(fd, &usrInput, sizeof(usrInput));
                printf("%s: %s\n", strings[USER_FORFEIT], strings[USER_LOST]);
                break;
            }

            // Check if the user input is valid
            if(advGrid[usrInput] != DEFAULT){
                printf("%s\n", strings[INV_CELL_ATTACKED]);
                continue;
            }

            // Send the choice to the opponent
            data = usrInput;
            if(socketSend(fd, &data, sizeof(data)) == -1){
                // failure during send, exit
                close(fd);
                return EXIT_FAILURE;
            }

            //Receive the result from the opponent as gridState value
            if(socketRecv(fd, &data, sizeof(data)) == -1){
                // failure during recv, exit
                close(fd);
                return EXIT_FAILURE;
            }

            if(checkOpponentForfeit(data) == FORFEIT){
                break;
            }

            if(data == BOAT){
                printf("%s %X: %s\n", strings[USER_ATTACK], usrInput, strings[HIT]);
                fflush(stdout);
                advGrid[usrInput] = SUNKEN_BOAT;
                advBoats--;
            } else {
                printf("%s %X: %s\n", strings[USER_ATTACK], usrInput, strings[MISS]);
                fflush(stdout);
                advGrid[usrInput] = MISSED_SHOT;
            }

            printf("%s\n", strings[OPPONENT_GRID]);
            printBoard(advGrid);

            if(advBoats == 0){
                printf("%s\n", strings[USER_WON]);
                break;
            }

        } else { //Here the player (defender) receive the opponent's choice

            printf("%s\n", strings[OPPONENT_WAIT]);
            fflush(stdout);

            // Wait for the server to send the choice and check if an error occurred
            if (socketRecv(fd, &data, sizeof(data)) == -1) {
                // Check for signal interruption (defender forfeit during waiting)
                if (forfeitFlag) {
                    char forfeit = FORFEIT;
                    socketSend(fd, &forfeit, sizeof(forfeit));
                    printf("%s: %s\n", strings[USER_FORFEIT], strings[USER_LOST]);
                    break;
                } else{
                    // Another error occurred, exit
                    close(fd);
                    return EXIT_FAILURE;
                }
            }

            if(checkOpponentForfeit(data) == FORFEIT){
                break;
            }

            // check the opponent choice
            if(userGrid[data] == BOAT){
                printf("%s %X: %s\n", strings[OPPONENT_ATTACK], data, strings[HIT]);
                fflush(stdout);
                userGrid[data] = SUNKEN_BOAT;
                userBoats--;
                data = BOAT;
            } else {
                printf("%s %X: %s\n", strings[OPPONENT_ATTACK], data, strings[MISS]);
                fflush(stdout);
                userGrid[data] = MISSED_SHOT;
                data = MISSED_SHOT;
            }

            // send back the result as gridState value
            socketSend(fd, &data, sizeof(data));

            //Display the opponent attack
            printf("%s\n", strings[USER_GRID]);
            printBoard(userGrid);

            if(userBoats == 0){
                printf("%s\n", strings[USER_LOST]);
                break;
            }
        }

        //Change the player turn
        firstIter++;
    }

    close(fd);
    return EXIT_SUCCESS;
}