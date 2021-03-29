//
// Created by carson on 5/20/20.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "helper.h"

// STEP 9 - Synchronization: the GAME structure will be accessed by both players interacting
// asynchronously with the server.  Therefore the data must be protected to avoid race conditions.
// Add the appropriate synchronization needed to ensure a clean battle.

static game * GAME = NULL;

void game_init() {
    if (GAME) {
        free(GAME);
    }
    GAME = malloc(sizeof(game));
    GAME->status = CREATED;
    game_init_player_info(&GAME->players[0]);
    game_init_player_info(&GAME->players[1]);
}

void game_init_player_info(player_info *player_info) {
    player_info->ships = 0;
    player_info->hits = 0;
    player_info->shots = 0;
}

int game_fire(game *game, int player, int x, int y) {
    /*// Step 5 - This is the crux of the game.  You are going to take a shot from the given player and
    // update all the bit values that store our game state.
    //
    //  - You will need up update the players 'shots' value
    //  - you You will need to see if the shot hits a ship in the opponents ships value.  If so, record a hit in the
    //    current players hits field
    //  - If the shot was a hit, you need to flip the ships value to 0 at that position for the opponents ships field
    //
    //  If the opponents ships value is 0, they have no remaining ships, and you should set the game state to
    //  PLAYER_1_WINS or PLAYER_2_WINS depending on who won.*/
    if ((x<0 || x>7) || (y<0 || y>7)) return 0; // handle invalid firing
    struct player_info* from_player = &game->players[player]; // grab the shooting player
    struct player_info* to_player = NULL;
    switch (player) { // get the other player
        case 1: to_player = &game->players[0]; break;
        case 0: to_player = &game->players[1]; break;
        default: break;
    }
    if (!to_player || !from_player) return 0; // handle NULL player values
    game->status = player ? PLAYER_0_TURN : PLAYER_1_TURN; // update player turns
    unsigned long long shot = xy_to_bitval(x, y); // calculate the shot (to be used later)
    from_player->shots |= shot;
    if (to_player->ships & shot) { // if the shot hit
        from_player->hits |= shot; // set the from_player.hits at shot to 1
        to_player->ships = to_player->ships & (~shot); // delete that section of ship
        if (to_player->ships == 0ull && player == 0) // check for winners
            game->status = PLAYER_0_WINS;
        else if (to_player->ships == 0ull && player == 1)
            game->status = PLAYER_1_WINS;
        return 1;
    }
    return 0; // otherwise it wasn't a hit so move on
}

unsigned long long int xy_to_bitval(int x, int y) {
    /*// Step 1 - implement this function.  We are taking an x, y position
    // and using bitwise operators, converting that to an unsigned long long
    // with a 1 in the position corresponding to that x, y
    //
    // x:0, y:0 == 0b00000...0001 (the one is in the first position)
    // x:1, y: 0 == 0b00000...10 (the one is in the second position)
    // ....
    // x:0, y: 1 == 0b100000000 (the one is in the eighth position)
    //
    // you will need to use bitwise operators and some math to produce the right
    // value.*/
    if ((x < 0 || x > 7) || (y < 0 || y > 7)) { return 0ull; } // invalid coords
    return (unsigned long long) 1<<(8*y+x); // 8*y <- get correct row, x <- get correct column
}

struct game * game_get_current() {
    return GAME;
}

int game_load_board(struct game *game, int player, char * spec) {
    /*// Step 2 - implement this function.  Here you are taking a C
    // string that represents a layout of ships, then testing
    // to see if it is a valid layout (no off-the-board positions
    // and no overlapping ships)
    //

    // if it is valid, you should write the corresponding unsigned
    // long long value into the Game->players[player].ships data
    // slot and return 1
    //
    // if it is invalid, you should return -1K*/
    if (!spec) return -1; // handle empty spec
    int b_used = 0, c_used = 0, d_used = 0, p_used = 0, s_used = 0, curr_pos = 0; // usage variables
    char ship_type = ' '; int coord_x = 0; int coord_y = 0; int length = 0; // ship data
    while (curr_pos < 15) {
        sscanf(spec,"%c%1d%1d", &ship_type, &coord_x, &coord_y); // grab info about the ship
        spec += 3; // consume the string
        curr_pos += 3; // keep track of where we are in the string
        if (!strcmp(spec, "") && curr_pos < 15) return -1; // handle an incomplete spec
        /* Set ship length based on its type */
        switch (ship_type) {
            case 'B':
            case 'b':
                if (!b_used) { b_used = 1; }
                else return -1;
                length = 4;
                break;
            case 'C':
            case 'c':
                if (!c_used) { c_used = 1; }
                else return -1;
                length = 5;
                break;
            case 'D':
            case 'd':
                if (!d_used) { d_used = 1; }
                else return -1;
                length = 3;
                break;
            case 'P':
            case 'p':
                if (!p_used) { p_used = 1; }
                else return -1;
                length = 2;
                break;
            case 'S':
            case 's':
                if (!s_used) { s_used = 1; }
                else return -1;
                length = 3;
                break;
            default:
                length = 0;
                break;
        }
        int _return = (ship_type > 54 && ship_type < 91) ?
            add_ship_horizontal(&game->players[player], coord_x, coord_y, length) : // attempt to add a horizontal ship (uppercase letters)
        (ship_type > 96 && ship_type < 123) ?
            add_ship_vertical(&game->players[player], coord_x, coord_y, length) : -1; // attempt to add a vertical ship (lowercase letters)
        if (_return == -1) return -1;
    }
    if (game->players[0].ships != 0ull && game->players[1].ships != 0ull) // if both player fields have been set set the status to PLAYER_0_TURN
        game->status = PLAYER_0_TURN;
    return 1;
}

int add_ship_horizontal(player_info *player, int x, int y, int length) {
    /*// implement this as part of Step 2
    // returns 1 if the ship can be added, -1 if not
    // hint: this can be defined recursively*/
    length = length - 1;
    if (length == -1 && !xy_to_bitval(x, y) ^ player->ships) { return 1; } // check for successful ship add
    else if (xy_to_bitval(x+length, y) == 0ull) return -1;   // if the ship runs off the board
    else if (xy_to_bitval(x, y) & player->ships) return -1;     // if the ship overlaps with an already placed ship
    else {
        player->ships |= xy_to_bitval(x+length, y);          // set the x+length, y bit to be 1
        add_ship_horizontal(player, x, y, length);              // recursively call add_ship_horizontal until we're done
    }
}

int add_ship_vertical(player_info *player, int x, int y, int length) {
    /*// implement this as part of Step 2
    // returns 1 if the ship can be added, -1 if not
    // hint: this can be defined recursively*/
    length = length - 1;
    if (length == -1 && !xy_to_bitval(x, y) ^ player->ships) { return 1; } // check for successful ship add
    else if (xy_to_bitval(x, y+length) == 0ull) return -1;   // if the ship runs off the board
    else if (xy_to_bitval(x, y) & player->ships) return -1;     // if the ship overlaps with an already placed ship
    else {
        player->ships |= xy_to_bitval(x, y+length);          // set the x, y+length bit to be 1
        add_ship_vertical(player, x, y, length);                // recursively call add_ship_vertical until we're done
    }
}