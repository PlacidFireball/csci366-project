//
// Created by carson on 5/20/20.
//

#include "stdio.h"
#include "stdlib.h"
#include "server.h"
#include "char_buff.h"
#include "game.h"
#include "repl.h"
#include "pthread.h"
#include<string.h>    //strlen
#include<sys/socket.h>
#include<sys/types.h>
#include<arpa/inet.h>    //inet_addr
#include<unistd.h>    //write
#include <errno.h>

#define SOCKET_INIT_FAIL -1
#define SERVER_FAIL -2

static game_server *SERVER;

void init_server() {
    if (SERVER == NULL) {
        SERVER = calloc(1, sizeof(struct game_server));
    } else {
        printf("Server already started");
    }
}

pthread_mutex_t lock;
void* handle_client_connect(void* arg) {
    // STEP 8 - This is the big one: you will need to re-implement the REPL code from
    // the repl.c file, but with a twist: you need to make sure that a player only
    // fires when the game is initialized and it is there turn.  They can broadcast
    // a message whenever, but they can't just shoot unless it is their turn.
    //
    // The commands will obviously not take a player argument, as with the system level
    // REPL, but they will be similar: load, fire, etc.
    //
    // You must broadcast informative messages after each shot (HIT! or MISS!) and let
    // the player print out their current board state at any time.
    //
    // This function will end up looking a lot like repl_execute_command, except you will
    // be working against network sockets rather than standard out, and you will need
    // to coordinate turns via the game::status field.
    long player = (long) arg; // retrieve player number from the garbo api
    // player is coming out as 1 for some reason on the first connection
    struct char_buff* buffer = cb_create(2000); // create a buffer that we can work with
    // default strings
    char* prompt = "battleBit (? for help) > ";
    char* help_str = "? - show help\nload <string> - load a ship layout file for the given player \\n"
                     "show - shows the board for the given player\n"
                     "fire [0-7] [0-7] - fire at the given position\n"
                     "say <string> - Send the string to all players as part of a chat\n"
                     "reset - reset the game\n"
                     "exit - quit the server\n";
    printf("Player init: %ld\n", player);
    while(1) {
        cb_reset(buffer);
        send(SERVER->player_sockets[player], prompt, strlen(prompt), 0);
        /*ssize_t bytes =*/recv(SERVER->player_sockets[player], buffer->buffer, buffer->size, 0);
        printf("RECV SUCCESS - received: %s", buffer->buffer);
        char *command = cb_tokenize(buffer, " \n");
        printf("command: %s<endstr>\n", command);
        if (command) {
            char *arg1 = cb_next_token(buffer);
            char *arg2 = cb_next_token(buffer);
            char *arg3 = cb_next_token(buffer);
            if (strcmp(command, "exit") == 0) {
                printf("Player %ld disconnect\n", player);
                send(SERVER->player_sockets[player], "Goodbye!\n", strlen("Goodbye!\n"), 0);
                close(SERVER->player_sockets[player]);
                break;
            } else if (strcmp(command, "?") == 0) {
                send(SERVER->player_sockets[player], help_str, strlen(help_str), 0);
            } else if (strcmp(command, "show") == 0) {

                struct char_buff *boardBuffer = cb_create(2000);
                repl_print_board(game_get_current(), (int) player, boardBuffer);
                send(SERVER->player_threads[player], boardBuffer->buffer, boardBuffer->size, 0);
                cb_free(boardBuffer);

            } else if (strcmp(command, "reset") == 0) {

                game_init();

            } else if (strcmp(command, "load") == 0) {

                game_load_board(game_get_current(), (int) player, arg1);

            } else if (strcmp(command, "fire") == 0) {
                int player = atoi(arg1);
                int x = atoi(arg2);
                int y = atoi(arg3);
                if (x < 0 || x >= BOARD_DIMENSION || y < 0 || y >= BOARD_DIMENSION) {
                    printf("Invalid coordinate: %i %i\n", x, y);
                } else {
                    printf("Player %i fired at %i %i\n", player + 1, x, y);
                    int result = game_fire(game_get_current(), player, x, y);
                    if (result) {
                        printf("  HIT!!!");
                    } else {
                        printf("  Miss");
                    }
                }
            } else {
                struct char_buff *unknown_buff = cb_create(30);
                sprintf(unknown_buff->buffer, "Unknown Command: %s\n", command);
                send(SERVER->player_sockets[player], unknown_buff->buffer, unknown_buff->size, 0);
                cb_free(unknown_buff);
            }
        }
    }
    return NULL;
}

void server_broadcast(char_buff *msg) {
    // send message to all players
    send(SERVER->player_sockets[1], msg->buffer, msg->size, 0);
    send(SERVER->player_sockets[0], msg->buffer, msg->size, 0);
}

void* run_server(void* arg) {
    /*// STEP 7 - implement the server code to put this on the network.
    // Here you will need to initalize a server socket and wait for incoming connections.
    //
    // When a connection occurs, store the corresponding new client socket in the SERVER.player_sockets array
    // as the corresponding player position.
    //
    // You will then create a thread running handle_client_connect, passing the player number out
    // so they can interact with the server asynchronously*/
    printf("Server init.\n");
    int server_socket_fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if (server_socket_fd == -1) {
        perror("ERROR | Server socket initialization failed\n");
        exit(SOCKET_INIT_FAIL);
    }
    SERVER->server_socket = server_socket_fd;
    int yes = 1;
    setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in server;

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(BATTLEBIT_PORT);

    printf("Server bind.\n");
    if (bind(server_socket_fd, (struct sockaddr *) &server, sizeof(server)) < 0) {
        // bind failed
        perror("ERROR | Server bind failed\n");
        exit(SERVER_FAIL);
    }
    else {
        // bind success, listen for incoming connections
        listen(server_socket_fd, 2);
        struct sockaddr_in client; socklen_t size_from_connect; int client_socket_fd;
        long requests = 0;
        while ((client_socket_fd = accept(server_socket_fd,
                                          (struct sockaddr *) &client,
                                          &size_from_connect)) > 0) {
            // on connect, add the socket to the player_sockets array
            SERVER->player_sockets[requests] = client_socket_fd;
            // initialize a thread for the player
            pthread_t player_thread;
            pthread_create(&player_thread, NULL, handle_client_connect, (void*)requests);
            SERVER->player_threads[requests] = player_thread;
            requests++;
        }
        pthread_join(SERVER->player_threads[0], NULL);
        pthread_join(SERVER->player_threads[1], NULL);
    }
    return NULL;
}

int server_start() {
    // STEP 6 - using a pthread, run the run_server() function asynchronously, so you can still
    // interact with the game via the command line REPL
    init_server();
    pthread_t server_thread;
    pthread_create(&server_thread, NULL, run_server, NULL);
    SERVER->server_thread = server_thread;
    pthread_join(server_thread, NULL);
    return 0;
}
