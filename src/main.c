#include <ncurses.h>
#include <stdbool.h>
#include <signal.h>
#include <assert.h>
#include <locale.h>
#include <string.h>

#define SOLITAIRE_CARDS_IMPLEMENTATION
#include "./cards.h"
#include "./colors.h"

bool running = true;

void quit() {
	running = false;
}

#define repeat(x) for (int repeat_ = 0; repeat_ < x; ++repeat_)

void render_dialog() {
    attron(COLOR_PAIR(COLOR_DIALOG));
    for (int i = 0; i <= 6; ++i) {
        move(i + 10, 20);
        if (i == 0 || i == 6) {
            repeat(40) printw(CHAR_DIALOG_BORDER);
        } else {
            repeat(40) printw(CHAR_DIALOG);
            move(i + 10, 20);
            printw(CHAR_DIALOG_BORDER);
            move(i + 10, 59);
            printw(CHAR_DIALOG_BORDER);
        }
    }
    attroff(COLOR_PAIR(COLOR_DIALOG));
}

bool size_too_small() {
    int win_x, win_y;
    getmaxyx(stdscr, win_y, win_x);
    return win_x < 84 || win_y < 45;
}

void render_size_dialog() {
    move(0, 0);
    int win_x, win_y;
    getmaxyx(stdscr, win_y, win_x);
    printw("Window size too small\n");
    printw("%ix%i\n", win_x, win_y);
}

void render_quit_dialog(bool quitting2) {
    if (size_too_small()) {
        move(2, 0);
        printw("Quit?\n");
        printw("%cCancel  %cQuit\n", quitting2 ? ' ' : '>', quitting2 ? '>' : ' ');
        return;
    }

    // renders the dialog for exiting the game

    render_dialog();

    attron(COLOR_PAIR(COLOR_DIALOG));
    move(12, 37);
    printw("Quit?");

    if (quitting2) {
        move(14, 30);
        printw("Cancel");

        attroff(COLOR_PAIR(COLOR_DIALOG));

        move(14, 45);
        attron(COLOR_PAIR(COLOR_DIALOG_SELECTED));
        printw("Quit");
        attroff(COLOR_PAIR(COLOR_DIALOG_SELECTED));
        move(14, 45);
    } else {
        move(14, 45);
        printw("Quit");

        attroff(COLOR_PAIR(COLOR_DIALOG));

        move(14, 30);
        attron(COLOR_PAIR(COLOR_DIALOG_SELECTED));
        printw("Cancel");
        attroff(COLOR_PAIR(COLOR_DIALOG_SELECTED));
        move(14, 30);
    }
}

void render_card_outline(Card card, int x, int y, bool is_selected, bool right_side_only) {
    // right_only only renders the right side of the card outline so highlight outline doesn't override the selected outline
    int color;
    if (card.highlight != NO_HIGHLIGHT || is_selected) {
        // render highlighted/selected outline
        color = is_selected ? COLOR_SELECTED : card.highlight == SOURCE ? COLOR_SOURCE : COLOR_HIGHLIGHTED;
        for (int y_ = -1; y_ <= 8; ++y_) {
            attron(COLOR_PAIR(color));
            move(y + y_, x - 1);
            if (!right_side_only && (y_ == -1 || y_ == 8)) {
                repeat(11) printw(is_selected ? CHAR_SELECT : CHAR_HIGHLIGHT);
            } else {
                if (!right_side_only) printw(is_selected ? CHAR_SELECT : CHAR_HIGHLIGHT);
                move(y + y_, x + 9);
                printw(is_selected ? CHAR_SELECT : CHAR_HIGHLIGHT);
            }
            attroff(COLOR_PAIR(color));
        }
    }
}

void render_card(Card card, CardPos pos, int x, int y, bool is_selected) {
    render_card_outline(card, x, y, is_selected, false);

    // if "missing" card (no card there on the tableau)
    if (card.rank == NO_RANK && pos.location == TABLEAU) return;

    int color;
    bool blank = (pos.location == STOCK || card.rank == NO_RANK || !card.visible);
    bool none = blank && pos.location == FOUNDATION; // blank outline for foundation card slot

    if (pos.location == STOCK) {
        if (card.rank == NO_RANK) {
            color = COLOR_STOCK_NONE;
            none = true; // and blank outline for empty stock
        } else {
            color = COLOR_STOCK;
        }
    } else if (blank) {
        color = COLOR_REGULAR;
    } else {
        color = get_suite_color(card.suite) ? COLOR_SUITE_BLACK : COLOR_SUITE_RED;
    }

    for (int y_ = 0; y_ <= 7; ++y_) {
        attron(COLOR_PAIR(color));
        move(y + y_, x);
        if (none) {
            repeat(9) printw(CHAR_NONE);
        } else if (y_ == 0 || y_ == 7) {
            if (blank) {
                repeat(9) printw(CHAR_CARD_BORDER_BLANK);
            } else {
                repeat(9) printw(CHAR_CARD_BORDER);
            }
        } else {
            if (blank) {
                repeat(9) printw(CHAR_CARD_BLANK);
            } else {
                repeat(9) printw(CHAR_CARD);
            }
            move(y + y_, x);
            if (blank) {
                printw(CHAR_CARD_BORDER_BLANK);
                move(y + y_, x + 8);
                printw(CHAR_CARD_BORDER_BLANK);
            } else {
                printw(CHAR_CARD_BORDER);
                move(y + y_, x + 8);
                printw(CHAR_CARD_BORDER);
            }
        }
        attroff(COLOR_PAIR(color));
    }

    if (blank) return;

    // rank and suite text
    char *rank_str = get_rank_str(card.rank);
    char *suite_str = get_suite_str(card.suite);

    move(y + 1, x + 2);
    attron(COLOR_PAIR(color));
    printw("%s", rank_str);
    attroff(COLOR_PAIR(color));

    move(y + 6, x + (strlen(rank_str) > 1 ? 5 : 6));
    attron(COLOR_PAIR(color));
    printw("%s", rank_str);
    attroff(COLOR_PAIR(color));

    move(y + 6, x + 2);
    attron(COLOR_PAIR(color));
    printw("%s", suite_str);
    attroff(COLOR_PAIR(color));

    move(y + 1, x + 6);
    attron(COLOR_PAIR(color));
    printw("%s", suite_str);
    attroff(COLOR_PAIR(color));
}

bool game_started = false;

void render(Game *game) {
    clear();

    if (size_too_small()) {
        render_size_dialog();
        return;
    }

    game_started = true;

    Card *selected_card = NULL;
    if (game->selected.active)
        selected_card = get_card(game->selected, game, false);
    int selected_x = 0, selected_y = 0, selected_y_off = 0;
    bool is_selected;

    // render foundation cards
    for (int x = 0; x < 4; ++x) {
        int x_ = x * 10 + 1, y_ = 1;
        if ((is_selected = (game->selected.location == FOUNDATION && game->selected.column == x))) {
            selected_x = x_, selected_y = y_;
        }
        render_card(game->foundation[x], (CardPos) { true, FOUNDATION, x, 0 }, x_, y_, is_selected);
    }

    int i;
    for (i = 0; i < 64; ++i) {
        if (game->waste[i].rank == NO_RANK) {
            break;
        }
    }
    // render last 3 waste cards
    for (int x = (i > 3 ? i - 3 : 0), j = 0; x < i; ++x, ++j) {
        int x_ = j * 6 + 47, y_ = 1;
        if ((is_selected = (game->selected.location == WASTE && x == i - 1))) {
            selected_x = x_, selected_y = y_;
        }
        render_card(game->waste[x], (CardPos) {true, WASTE, 0, 0 }, x_, y_, is_selected);
    }

    // render stock card
    is_selected = false;
    Card *card_ = get_stock_top(game, true);
    if (card_) {
        Card card = *card_;
        if ((is_selected = (game->selected.location == STOCK))) { selected_x = 71, selected_y = 1; }
        render_card(card, (CardPos) {true, STOCK, 0, 0 }, 71, 1, is_selected);
    }

    // render tableau
    for (int column = 0; column < 7; ++column) {
        bool prev_selected = false;
        for (int row = 0; row < 64; ++row) {
            int x_ = column * 10 + 1, y_ = row * 3 + 10;
            if ((is_selected = (game->selected.location == TABLEAU && game->selected.column == column && game->selected.row == row))) {
                selected_y_off = 0, selected_x = x_, selected_y = y_;
            } else if (row > 0 && game->tableau[column][row].rank != NO_RANK && prev_selected) {
                // move cursor up a bit if there is a card in the way
                selected_y_off = -1;
            }
            render_card(game->tableau[column][row], (CardPos) { true, TABLEAU, column, row }, x_, y_, is_selected);
            prev_selected = is_selected;
        }
    }

    if (selected_card) {
        render_card_outline(*selected_card, selected_x, selected_y, true, true);
    }

    move(selected_y + 3 + selected_y_off, selected_x + 4);
}

Game *game_instance;

int main() {
    // allow unicode characters
    setlocale(LC_ALL, "");

    // create game instance
    game_instance = create_game();
    assert(game_instance);

	initscr();

    if (!has_colors()) {
        printw("Color is not supported on this terminal.");
        endwin();
        return 0;
    }

    start_color();
    use_default_colors();

    // initialize colors
    init_pair(COLOR_REGULAR, COLOR_WHITE, COLOR_BLACK);
    init_pair(COLOR_SUITE_BLACK, COLOR_WHITE, COLOR_BLACK);
    init_pair(COLOR_SUITE_RED, COLOR_RED, COLOR_BLACK);
    init_pair(COLOR_STOCK, COLOR_WHITE, COLOR_BLACK);
    init_pair(COLOR_STOCK_NONE, COLOR_RED, COLOR_BLACK);
    init_pair(COLOR_SOURCE, COLOR_BLUE, COLOR_BLACK);
    init_pair(COLOR_HIGHLIGHTED, COLOR_YELLOW, COLOR_BLACK);
    init_pair(COLOR_SELECTED, COLOR_CYAN, COLOR_BLACK);
    init_pair(COLOR_DIALOG, COLOR_WHITE, COLOR_BLACK);
    init_pair(COLOR_DIALOG_SELECTED, COLOR_CYAN, COLOR_BLACK);

	raw();
	noecho();
	keypad(stdscr, TRUE);

	refresh();

    // set up signal handlers
	signal(SIGINT, quit);
	signal(SIGTERM, quit);
	signal(SIGQUIT, quit);
	signal(SIGHUP, quit);
	signal(SIGPIPE, quit);
	signal(SIGUSR1, quit);
	signal(SIGUSR2, quit);

    // render the game
    render(game_instance);

    bool quitting = false;
    bool quitting2 = false;
	while (running) {
        Action action = NO_ACTION;

        // handle key presses
		switch (getch()) {
			case ERR:
				break;

            case KEY_RESIZE:
                render(game_instance);
                break;

			case KEY_UP:
			case 'w':
			case 'W':
                action = UP;
				break;

            case KEY_RIGHT:
            case 'd':
            case 'D':
                action = RIGHT;
                break;

			case KEY_DOWN:
			case 's':
			case 'S':
                action = DOWN;
				break;

			case KEY_LEFT:
			case 'a':
			case 'A':
                action = LEFT;
				break;

			case 'q':
			case 'Q':
            case '\x1a': // ctrl+Z
            case '\x03': // ctrl+C
                action = QUIT;
				break;

            case '\x1b': // escape (ctrl+[)
                action = CANCEL;
                break;

            case '\x0d': // return (ctrl+M \r)
            case '\x0a': // enter (\n)
            case ' ': // space
                action = CONFIRM;
                break;

			default:
				break;
		}
		refresh();
        if (!running) break;
        if (action != NO_ACTION) {
            if (!quitting) {
                // show quit dialog
                if (action == QUIT) {
                    if (!game_started) {
                        running = false;
                    } else {
                        quitting = true;
                        quitting2 = false;
                    }
                }
                handle_action(action, game_instance);
                update_display(game_instance);
            }
            render(game_instance);
            if (quitting) {
                switch (action) {
                    // move quit dialog option
                    case LEFT:
                        quitting2 = false;
                        break;
                    case RIGHT:
                        quitting2 = true;
                        break;
                    case CONFIRM:
                        // confirm quit
                        if (quitting2) {
                            running = false;
                            break;
                        }
                    case CANCEL:
                        quitting = false;
                        break;
                    default:
                        break;
                }
                if (quitting)
                    render_quit_dialog(quitting2);
            }
        }

        refresh();
	}

    noraw();
    echo();
    keypad(stdscr, false);

	endwin();
	return 0;
}
