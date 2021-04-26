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
//#include <errno.h>  // debug

#define SOCKET_INIT_FAIL -1
#define SERVER_FAIL -2
// appends the newline character to the end of a char_buff
#define APPEND_NEWLINE(buff) cb_append(buff, "\n")

// global variables
static game_server *SERVER;
pthread_mutex_t lock;
struct game* CURR_GAME;

void init_server() {
    if (SERVER == NULL) {
        SERVER = calloc(1, sizeof(struct game_server));
    } else {
        printf("Server already started");
    }
}

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

    /*
     * Handles a client connecting to the game server. Locks the other thread from
     * executing whenever a command is called. (erring on the safe side)
     */
    pthread_mutex_lock(&lock);
    CURR_GAME == NULL ? game_init() : printf("");
    pthread_mutex_unlock(&lock);
    long player = (long) arg;   // retrieve the player from the weird api
    long other_player = player ? 0 : 1;     // grab the other player
    int player_turn_status = !player ? PLAYER_0_TURN : PLAYER_1_TURN;   // set each individual's status
    int other_player_status = player ? PLAYER_0_TURN : PLAYER_1_TURN;
    struct char_buff* buffer = cb_create(5000); // create a buffer that we can work with
    /* CONSTANT STRINGS */
    const char* prompt = "battleBit (? for help) > ";
    const char* help_str = "? - show help\nload <string> - load a ship layout file for the given player\n"
                     "show - shows the board for the given player\n"
                     "fire [0-7] [0-7] - fire at the given position\n"
                     "say <string> - Send the string to all players as part of a chat\n"
                     "reset - reset the game\n"
                     "exit - quit the server\n";
    while(1) {
        /* Setup variables, update game and reset the buffer */
        CURR_GAME = game_get_current();
        cb_reset(buffer);
        char* command = NULL;
        send(SERVER->player_sockets[player], prompt, strlen(prompt), 0);
        recv(SERVER->player_sockets[player], buffer->buffer, buffer->size, 0);
        /* parse user input */
        command = cb_tokenize(buffer, " \n\r");   // note parsing on \r -> all
                                                                // messages sent over server include \r at the end
        if (command) {
            if (strcmp(command, "exit") == 0) {
            // disconnects the player from the server
                pthread_mutex_lock(&lock);
                send(SERVER->player_sockets[player], "Goodbye!\n", strlen("Goodbye!\n"), 0);
                close(SERVER->player_sockets[player]);  // close the connection
                char_buff* temp = cb_create(50);
                cb_append(temp, "\nOther player disconnected from the server. Goodbye!\n");
                send(SERVER->player_sockets[other_player], temp->buffer, temp->size, 0);
                cb_free(temp);
                close(SERVER->player_sockets[other_player]);
                pthread_mutex_unlock(&lock);
                break;

            } else if (strcmp(command, "?") == 0) {

            // prints off the help menu to the player
                pthread_mutex_lock(&lock);
                send(SERVER->player_sockets[player], help_str, strlen(help_str), 0);
                pthread_mutex_unlock(&lock);

            } else if (strcmp(command, "show") == 0) {

            // displays the hits/misses board and ships board to the player
                pthread_mutex_lock(&lock);
                struct char_buff *boardBuffer = cb_create(2000);
                repl_print_board(CURR_GAME, (int) player, boardBuffer);
                send(SERVER->player_sockets[player], boardBuffer->buffer, boardBuffer->size, 0);
                cb_free(boardBuffer);
                pthread_mutex_unlock(&lock);

            } else if (strcmp(command, "say") == 0) {

                pthread_mutex_lock(&lock);
                char* next_str = cb_next_token(buffer); // advance so we don't get the "say" portion of the message
                struct char_buff *say_buff = cb_create(2000); // allocate say_buff
                char identity[16];
                sprintf(identity, "Player %ld says: ", player);
                APPEND_NEWLINE(say_buff);
                cb_append(say_buff, identity);
                while (next_str /*&& (say_buff->size + strlen(next_str)) < say_buff->size*/) { // while our args aren't null and the string will fit in say_buff
                    cb_append(say_buff, next_str); // append the word onto the say buffer
                    cb_append(say_buff, " "); // add a space between the words
                    next_str = cb_next_token(buffer); // advance command
                }
                APPEND_NEWLINE(say_buff);
                cb_append(say_buff, prompt); // include the prompt so we don't get lots of text clutter
                send(SERVER->player_sockets[other_player], say_buff->buffer, say_buff->size, 0); // sends the message
                printf("%s", say_buff->buffer);
                cb_free(say_buff);
                pthread_mutex_unlock(&lock);

            }else if (strcmp(command, "reset") == 0) {

            // resets the game
                pthread_mutex_lock(&lock);
                game_init();
                pthread_mutex_unlock(&lock);

            } else if (strcmp(command, "load") == 0) {

            // loads a board into the player's board
                pthread_mutex_lock(&lock);
                char *arg1 = cb_next_token(buffer);     // retrieve args
                game_load_board(CURR_GAME, (int) player, arg1);
                if (CURR_GAME->players[0].ships != 0 && CURR_GAME->players[1].ships != 0) {
                    char_buff* load_buff = cb_create(500);
                    cb_append(load_buff, "\nAll Player Boards Loaded.\nPlayer 0 Turn.\n");
                    send(SERVER->player_sockets[player], &load_buff->buffer[1], load_buff->size-1, 0);
                    cb_append(load_buff, prompt);
                    send(SERVER->player_sockets[other_player], load_buff->buffer, load_buff->size, 0);
                    printf(load_buff->buffer);
                    cb_free(load_buff);
                }
                else if (CURR_GAME->players[other_player].ships == 0) {
                    char str[40];
                    sprintf(str, "Waiting On Player %ld\n", other_player);
                    send(SERVER->player_sockets[player], str, strlen(str), 0);
                }
                pthread_mutex_unlock(&lock);

            } else if (strcmp(command, "fire") == 0) {

            // allows the player to fire on the other player IF it is their turn
                pthread_mutex_lock(&lock);
                char *arg1 = cb_next_token(buffer);     // retrieve args
                char *arg2 = cb_next_token(buffer);
                int x = atoi(arg1);
                int y = atoi(arg2);
                if (CURR_GAME->status == player_turn_status) {
                    if (x < 0 || x >= BOARD_DIMENSION || y < 0 || y >= BOARD_DIMENSION) {
                    // handles invalid coordinates
                        char_buff *invalid = cb_create(100);
                        sprintf(invalid->buffer, "Invalid coordinate %i %i\n", x, y);
                        send(SERVER->player_sockets[player], invalid->buffer,
                             invalid->size, 0);
                        cb_free(invalid);
                    } else {
                    // perform the shot calculations
                        // notify the players
                        char_buff* fire_buff = cb_create(500);
                        char tmp[40];
                        sprintf(tmp, "Player %ld fired at %i %i", player + 1, x, y);
                        cb_append(fire_buff, tmp);
                        // calculate whether on not it was a hit
                        int result = game_fire(CURR_GAME, (int) player, x, y);
                        if (result) {
                            cb_append(fire_buff, " - HIT");
                            // Check for wins
                            if (CURR_GAME->status == PLAYER_0_WINS) cb_append(fire_buff, " - PLAYER 0 WINS\n");
                            else if (CURR_GAME->status == PLAYER_1_WINS) cb_append(fire_buff, " - PLAYER 1 WINS\n");
                            else APPEND_NEWLINE(fire_buff);
                        }
                        else cb_append(fire_buff, " - MISS\n");
                        /*
                         * This code sends different messages based on who it's sending to, basically just handling
                         * weird newline and prompt stuff
                         */
                        send(SERVER->player_sockets[player], fire_buff->buffer, fire_buff->size, 0); // (fire_buff)
                        char_buff* tmp_buff = cb_create(500);
                        APPEND_NEWLINE(tmp_buff);
                        cb_append(tmp_buff, fire_buff->buffer); // \n(fire_buff)prompt
                        cb_append(tmp_buff, prompt);
                        send(SERVER->player_sockets[other_player], tmp_buff->buffer, tmp_buff->size, 0);
                        printf(tmp_buff->buffer);
                        CURR_GAME->status = other_player_status;
                        cb_free(fire_buff);
                        cb_free(tmp_buff);
                    }
                }
                else {
                // handle when it isn't their turn and they try to fire
                    if (SERVER->player_sockets[other_player] == 0) {
                        send(SERVER->player_sockets[player], "Game has not begun!\n",
                             strlen("Game has not begun!\n"), 0);
                    }
                    else {
                        char_buff* turn_buff = cb_create(50);
                        sprintf(turn_buff->buffer, "Player %ld turn.\n", other_player);
                        send(SERVER->player_sockets[player], turn_buff->buffer,
                             turn_buff->size, 0);
                        cb_free(turn_buff);
                    }
                }
                pthread_mutex_unlock(&lock);

            } else {
            // handles invalid input
                pthread_mutex_lock(&lock);
                struct char_buff *unknown_buff = cb_create(30);
                sprintf(unknown_buff->buffer, "Unknown Command: %s\n", command);
                send(SERVER->player_sockets[player], unknown_buff->buffer, unknown_buff->size, 0);
                cb_free(unknown_buff);
                pthread_mutex_unlock(&lock);
            }
        }
    }
    return NULL;
}

void server_broadcast(char_buff *msg) {
    // send message to all players
    printf(msg->buffer); // send to server
    send(SERVER->player_sockets[1], msg->buffer, msg->size, 0); // send to players
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
    int server_socket_fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if (server_socket_fd == -1) {
        perror("ERROR | Server socket initialization failed\n");
        exit(SOCKET_INIT_FAIL);
    }
    int yes = 1;
    setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    // ??????
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(BATTLEBIT_PORT);

    // Bind the socket
    if (bind(server_socket_fd, (struct sockaddr *) &server, sizeof(server)) < 0) {
        perror("ERROR | Server bind failed\n");
        exit(SERVER_FAIL);
    }
    else {
        listen(server_socket_fd, 2); // listen for incoming connections
        struct sockaddr_in client; socklen_t size_from_connect; int client_socket_fd;
        long requests = 0; // long because it is 8 bytes (same as void*, avoids weird casting issues)
        while ((client_socket_fd = accept(server_socket_fd,
                                          (struct sockaddr *) &client,
                                          &size_from_connect)) > 0 && requests < 2)   {
            SERVER->player_sockets[requests] = client_socket_fd; // keep track of the socket
            pthread_t player_thread; // starts a new thread
            pthread_create(&player_thread, NULL, handle_client_connect, (void*)requests);
            SERVER->player_threads[requests] = player_thread; // saves the thread
            requests++;
        }
        // wait for both of the player threads to terminate
        pthread_join(SERVER->player_threads[0], NULL);
        pthread_join(SERVER->player_threads[1], NULL);
    }
    return NULL;
}

int server_start() {
    // STEP 6 - using a pthread, run the run_server() function asynchronously, so you can still
    // interact with the game via the command line REPL
    init_server();
    pthread_create(&SERVER->server_thread, NULL, run_server, NULL);
    return 0;
}
