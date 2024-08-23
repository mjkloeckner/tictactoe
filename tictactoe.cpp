#include <iostream>
#include <cstdlib>
#include <csignal>
#include <termios.h>
#include <string>
#include <sstream>

#define  TUI_TOTAL_HEIGHT  7

typedef enum {
    PLAYER_1 = 1,
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
static player_t current_player;
static game_status_t game_status;
static int cursor_col, cursor_row, cursor_row_pre;
std::string player_status, key_string;
char game_board[3][3];

void tui_set_input_mode(void) {
    struct termios tattr;

    if (!isatty(STDIN_FILENO)) {
        std::cerr <<  "Error: not a terminal" << std::endl;
        exit (EXIT_FAILURE);
    }

    tcgetattr(STDIN_FILENO, &default_attributes);
    tcgetattr(STDIN_FILENO, &tattr);
    tattr.c_lflag &= ~(ICANON|ECHO);
    tattr.c_cc[VMIN] = 1;
    tattr.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &tattr);
}

void tui_reset_input_mode(void) {
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

void game_player_status_msg(void) {
    key_string.assign("\033[0J");

    switch(game_status) {
    case PLAYING:
        player_status.assign("Player ");
        player_status += (std::ostringstream() << current_player).str();
        player_status += "\033[0J";
        break;
    case WIN:
        player_status.assign("Player ");
        player_status += (std::ostringstream() << (current_player^3)).str();
        player_status += " Wins";
        key_string.assign("Press `r` to restart\033[0J");
        game_status = HALT;
        break;
    case DRAFT:
        player_status.assign("Draft\033[0J");
        game_status = HALT;
    case HALT:
        key_string.assign("Press `r` to restart\033[0J");
        break;
    default:
        break;
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

    std::cout << "\033[" << TUI_TOTAL_HEIGHT-1 << "A";
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
        game_board[cursor_row][cursor_col] = ((current_player == PLAYER_1) ? 'X' : 'O');
        current_player = ((current_player == PLAYER_1) ? PLAYER_2 : PLAYER_1);
    }
}

void game_redraw_board(void) {
    game_clear_board();
    game_print_board();
    game_board_set_cursor();
}

void game_tui_cleanup(void) {
    tui_reset_input_mode();
    std::cout << "\033[" << TUI_TOTAL_HEIGHT-(cursor_row*2)-1 << "E";
    std::cout << "\033[0 q"; // block cursor shape
}

void game_tui_setup(void) {
    tui_set_input_mode();
    signal(SIGINT,  game_sig_handler);
    signal(SIGQUIT, game_sig_handler);
    signal(SIGTSTP, SIG_IGN);
    atexit(game_tui_cleanup);
    std::cout << "\033[4 q"; // underline cursor shape
}

void game_initialize_board(void) {
    game_status = PLAYING;
    cursor_col = cursor_row = 1;
    current_player = PLAYER_1;
    for(size_t i = 0; i < 3; ++i) {
        for(size_t j = 0; j < 3; ++j) {
            game_board[i][j] = ' ';
        }
    }
}

void game_check_status(void) {
    char first;
    size_t i, j, h, v, d, rd, white_space;

    if(game_status != PLAYING)
        return;

    for(i = white_space = 0; i < 3; ++i) {
        for(j = h = v = d = rd = 0; j < 3; ++j) {
            if((first = game_board[i][0]) != ' ') { // horizontal (-)
                if(game_board[i][j] == first) {
                    if((h++) == 2) {
                        game_status = WIN;
                        return;
                    }
                }
            }
            if((first = game_board[0][i]) != ' ') { // vertical (|)
                if(game_board[j][i] == first) {
                    if((v++) == 2) {
                        game_status = WIN;
                        return;
                    }
                }
            }
            if((first = game_board[0][0]) != ' ') { // diagonal (/)
                if(game_board[j][j] == first) {
                    if((d++) == 2) {
                        game_status = WIN;
                        return;
                    }
                }
            }
            if((first = game_board[0][2]) != ' ') { // reverse diagonal (\)
                if(game_board[2-j][j] == first) {
                    if((rd++) == 2) {
                        game_status = WIN;
                        return;
                    }
                }
            }
            if(game_board[i][j] == ' ') {
                white_space++;
            }
        }
    }
    if((i == 3) && (j == 3) && (white_space == 0))
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

int main(void) {
    cursor_row = cursor_col = cursor_row_pre = 0;

    game_tui_setup();
    game_initialize_board();
    game_print_board();
    std::cout << "\033[" << ((cursor_col*4)+2) << "C";
    game_redraw_board();

    while(game_status != EXIT) {
        game_key_handler();
        game_check_status();
        if(game_status == RESET)
            game_initialize_board();
        game_redraw_board();
    }

    return 0;
}
