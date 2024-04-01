#include <iostream>
#include <cstdlib>
#include <csignal>
#include <termios.h>
#include <string>
#include <sstream>

typedef enum {
    PLAYER_1,
    PLAYER_2
} player_t;

typedef enum {
    PLAYING,
    WIN,
    DRAFT,
    RESET,
    HALT,
    EXIT
} game_status_t;

static struct termios default_attributes;
static unsigned int tui_total_height;
static player_t current_player;
static game_status_t game_status;
static int cursor_col, cursor_row, cursor_row_pre;
std::string player_status, key_string;
char game_board[3][3];

void tui_set_input_mode (void) {
    struct termios tattr;

    if (!isatty(STDIN_FILENO)) {
        std::cerr <<  "Not a terminal" << std::endl;
        exit (EXIT_FAILURE);
    }

    tcgetattr(STDIN_FILENO, &default_attributes);
    tcgetattr(STDIN_FILENO, &tattr);
    tattr.c_lflag &= ~(ICANON|ECHO);
    tattr.c_cc[VMIN] = 1;
    tattr.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &tattr);
}

void tui_reset_input_mode (void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &default_attributes);
}

void game_sig_handler(int signo) {
    signal(signo, SIG_IGN);
    if ((signo == SIGINT) || (signo == SIGQUIT)) {
        game_status = EXIT;
        exit(0);
    }
}

void game_clear_board(void) {
    std::cout << "\033[" << (cursor_row_pre*2)+1 << "F\033[0K";
    cursor_row_pre = cursor_row;
}

void game_player_status_msg() {
    std::ostringstream current_player_string;
    key_string.assign("\033[0J");

    if(game_status == PLAYING) {
        player_status = std::string("Player ");
        current_player_string << current_player+1;
        player_status += current_player_string.str();
        player_status += "\033[0J";
    }
    else if (game_status == WIN) {
        unsigned int player_won = ((current_player == PLAYER_1) ? PLAYER_2 : PLAYER_1);
        player_status = std::string("Player ");
        current_player_string << player_won+1;
        player_status += current_player_string.str();
        player_status += " Wins";
        game_status = HALT;
    }
    else if (game_status == DRAFT) {
        player_status.assign("Draft\033[0J");
        key_string.assign("Press `r` to restart\033[0J");
        game_status = HALT;
    }
}

void game_print_board(void) {
    game_player_status_msg();
    std::cout << "┌───┬───┬───┐\n│ "
              << game_board[0][0] << " │ "
              << game_board[0][1] << " │ " 
              << game_board[0][2] << " │ " << player_status << "\n";

    std::cout << "├───┼───┼───┤ " << key_string << "\n│ "
              << game_board[1][0] << " │ "
              << game_board[1][1] << " │ " 
              << game_board[1][2] << " │\n";

    std::cout << "├───┼───┼───┤\n│ "
              << game_board[2][0] << " │ "
              << game_board[2][1] << " │ " 
              << game_board[2][2] << " │\n"
              << "└───┴───┴───┘\n";
}

void game_board_set_cursor(void) {
    if(cursor_row)
        std::cout << "\033[" << cursor_row*2 << "B";

    std::cout << "\033[0G\033[" << (cursor_col*4)+2 << "C";
}

void game_move_cursor_right(void) {
    cursor_col = (cursor_col == 2) ? 0 : cursor_col + 1;
}

void game_move_cursor_left(void) {
    cursor_col = (cursor_col > 0) ? cursor_col-1 : 2;
}

void game_move_cursor_up(void) {
    cursor_row_pre = cursor_row;
    cursor_row = (cursor_row > 0) ? cursor_row-1 : 2;
}
 
void game_move_cursor_down(void) {
    cursor_row_pre = cursor_row;
    cursor_row = ((cursor_row + 1) > 2) ? 0 : cursor_row+1;
}

void game_set_at_cursor_pos(void) {
    if(game_board[cursor_row][cursor_col] == ' ') {
        char aux = ((current_player == PLAYER_1) ? 'X' : 'O');
        game_board[cursor_row][cursor_col] = aux;
        current_player = ((current_player == PLAYER_1) ? PLAYER_2 : PLAYER_1);
    }
}

void game_redraw_board(void) {
    game_clear_board();
    game_print_board();
    std::cout << "\033[" << tui_total_height-1 << "A";
    game_board_set_cursor();
}

void game_tui_cleanup() {
    tui_reset_input_mode();
    std::cout << "\033[" << tui_total_height-(cursor_row*2)-1 << "E";
}

void game_tui_setup() {
    tui_set_input_mode();
    signal(SIGINT,  game_sig_handler);
    signal(SIGQUIT, game_sig_handler);
    signal(SIGTSTP, SIG_IGN);
    atexit(game_tui_cleanup);
    std::cout << "\033[4 q"; // Blinking block cursor
}

void game_initialize_board() {
    for(size_t i = 0; i < 3; ++i) {
        for(size_t j = 0; j < 3; ++j) {
            game_board[i][j] = ' ';
        }
    }
}

void game_check_status() {
    char aux;
    size_t i, j, occ_x, occ_o;
    if(game_status != PLAYING)
        return;

    // Horizontal line
    for(i = occ_x = occ_o = 0; i < 3; ++i) {
        aux = game_board[cursor_row][i];
        if(aux == 'X') {
            occ_x++;
            occ_o = 0;
        }
        else if (aux == 'O'){
            occ_x = 0;
            occ_o++;
        }
        else {
            occ_x = occ_o = 0;
        }
    }
    if ((occ_o == 3) || (occ_x == 3)) {
        game_status = WIN;
    }

    // Vertical line
    for(i = occ_x = occ_o = 0; i < 3; ++i) {
        aux = game_board[i][cursor_col];
        if(aux == 'X') {
            occ_x++;
            occ_o = 0;
        }
        else if (aux == 'O'){
            occ_x = 0;
            occ_o++;
        }
        else {
            occ_x = occ_o = 0;
        }
    }
    if ((occ_o == 3) || (occ_x == 3)) {
        game_status = WIN;
    }

    // Reverse diagonal line (\)
    for(i = occ_x = occ_o = 0; i < 3; ++i) {
        aux = game_board[i][i];
        if(aux == 'X') {
            occ_x++;
            occ_o = 0;
        }
        else if (aux == 'O'){
            occ_x = 0;
            occ_o++;
        }
        else {
            occ_x = occ_o = 0;
        }
    }
    if ((occ_o == 3) || (occ_x == 3)) {
        game_status = WIN;
    }

    // Forward diagonal line (/)
    for(i = occ_x = occ_o = 0; i < 3; ++i) {
        aux = game_board[2-i][i];
        if(aux == 'X') {
            occ_x++;
            occ_o = 0;
        }
        else if (aux == 'O'){
            occ_x = 0;
            occ_o++;
        }
        else {
            occ_x = occ_o = 0;
        }
    }
    if ((occ_o == 3) || (occ_x == 3)) {
        game_status = WIN;
    }

    for(i = 0; i < 3; ++i)
        for(j = 0; j < 3; ++j)
            if(game_board[i][j] == ' ')
                goto end;;

end:
    if((i == j) && (i == 3))
        game_status = DRAFT;
}

void game_key_handler(void) {
    char cmd;

    std::cin.get(cmd);
    switch(cmd) {
    case 'w':
    case 'k':
        game_move_cursor_up();
        break;
    case 's':
    case 'j':
        game_move_cursor_down();
        break;
    case  10: // Enter
    case ' ':
        if(game_status == PLAYING)
            game_set_at_cursor_pos();
        break;
    case 'l':
    case 'd':
        game_move_cursor_right();
        break;
    case 'h':
    case 'a':
        game_move_cursor_left();
        break;
    case 'r':
        if(game_status == HALT)
            game_status = RESET;
        break;
    case 'q':
        game_status = EXIT;
        break;
    case 27: { // Escape codes (i.e. arrow keys)
        std::cin.get(cmd); // skip '['
        std::cin.get(cmd);
        switch (cmd) {
        case 'A': // up
            game_move_cursor_up();
            break;
        case 'B': // down
            game_move_cursor_down();
            break;
        case 'C': // right
            game_move_cursor_right();
            break;
        case 'D': // left
            game_move_cursor_left();
            break;
        }
        break;
        }
    }
}

int main (void) {
    tui_total_height = 7;
    cursor_row = cursor_row_pre = cursor_col = 0;
    game_status = PLAYING;
    current_player = PLAYER_1;

    game_tui_setup();
    game_initialize_board();
    game_print_board();
    std::cout << "\033[" << tui_total_height-1 << "A";
    std::cout << "\033[" << ((cursor_col*4)+2) << "C";

    game_move_cursor_right();
    game_redraw_board();
    game_move_cursor_down();
    game_redraw_board();

    while(game_status != EXIT) {
        game_key_handler();
        if(game_status == RESET) {
            game_status = PLAYING;
            current_player = PLAYER_1;
            cursor_col = cursor_row = 1;
            game_initialize_board();
        }
        game_check_status();
        game_redraw_board();
    }

    return 0;
}
