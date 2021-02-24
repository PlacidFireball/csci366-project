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
#include<arpa/inet.h>    //inet_addr
#include<unistd.h>    //write

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

void* handle_client_connect(void* player) {
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
    struct char_buff* buffer = cb_create(2000);
    char* prompt = "battleBit ? for help";
    send(SERVER->player_threads[player], prompt, strlen(prompt), 0);
    recv(SERVER->server_thread, buffer->buffer, 2000 , 0);

}

void server_broadcast(char_buff *msg) {
    // send message to all players
    send(SERVER->player_sockets[1], msg->buffer, msg->size, 0);
    send(SERVER->player_sockets[0], msg->buffer, msg->size, 0);
}

void* run_server() {
    /*// STEP 7 - implement the server code to put this on the network.
    // Here you will need to initalize a server socket and wait for incoming connections.
    //
    // When a connection occurs, store the corresponding new client socket in the SERVER.player_sockets array
    // as the corresponding player position.
    //
    // You will then create a thread running handle_client_connect, passing the player number out
    // so they can interact with the server asynchronously*/
    int server_socket_fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if (server_socket_fd == -1) {
        exit(SOCKET_INIT_FAIL);
    }
    int yes = 1;
    setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in server;

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(BATTLEBIT_PORT);

    if (bind(server_socket_fd, (struct sockaddr *) &server, sizeof(server)) < 0) {
        // bind failed
        exit(SERVER_FAIL);
    }
    else {
        // bind success, listen for incoming connections
        listen(server_socket_fd, 3);
        struct sockaddr_in client;
        socklen_t size_from_connect;
        int client_socket_fd;
        int requests = 0;
        while ((client_socket_fd = accept(server_socket_fd,
                                          (struct sockaddr *) &client,
                                          &size_from_connect)) > 0) {
            // on connect, add the socket to the player_sockets array
            SERVER->player_sockets[requests++] = client_socket_fd;
        }
        pthread_t player_thread0;
        pthread_create(&player_thread0, NULL, handle_client_connect, &SERVER->player_sockets[0]);
        SERVER->player_threads[0] = player_thread0;
        pthread_t player_thread1;
        pthread_create(&player_thread1, NULL, handle_client_connect, &SERVER->player_sockets[1]);
        SERVER->player_threads[1] = player_thread1;

    }
}

int server_start() {
    // STEP 6 - using a pthread, run the run_server() function asynchronously, so you can still
    // interact with the game via the command line REPL
    pthread_t server_thread;
    pthread_create(&server_thread, NULL, run_server, NULL);
    SERVER->server_thread = server_thread;
}
