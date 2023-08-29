#ifndef SOLITAIRE_CARDS
#define SOLITAIRE_CARDS

#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

typedef enum {
    HEARTS, DIAMONDS, CLUBS, SPADES
} Suite;
typedef enum {
    NO_RANK, ACE, RANK2, RANK3, RANK4, RANK5, RANK6, RANK7, RANK8, RANK9, RANK10, JACK, QUEEN, KING
} Rank;
typedef enum {
    NO_HIGHLIGHT, HIGHLIGHTED, SOURCE
} Highlight;
typedef enum {
    TABLEAU, WASTE, STOCK, FOUNDATION
} CardLocation;
typedef enum {
    NO_ACTION, UP, RIGHT, DOWN, LEFT, CONFIRM, CANCEL, QUIT
} Action;

typedef struct {
    bool active;
    CardLocation location;
    int column;
    int row;
} CardPos;
typedef struct {
    bool visible;
    Highlight highlight;
    Suite suite;
    Rank rank;
} Card;
typedef struct {
    Card tableau[7][64];
    Card foundation[4];
    Card waste[64];
    Card stock[64];
    CardPos selected;
    CardPos moving;
} Game;

Game *create_game();
void reset_game(Game *game);
void reset_selected(Game *game);
void update_display(Game *game);
void update_visible(Game *game);
bool can_stack(Card card, Card above, bool is_foundation);
void clear_highlight(Game *game);
void highlight_source(Game *game);
int highlight_stackable(Card *card, bool tableau, Game *game);
Card *get_waste_top(Game *game, bool no_rank);
Card *get_stock_top(Game *game, bool no_rank);
Card *get_card(CardPos pos, Game *game, bool no_rank);
int get_amount_stacked_cards(CardPos pos, Game *game);
bool is_opposite_color(Suite suite1, Suite suite2);
bool get_suite_color(Suite suite);
char *get_rank_str(Rank rank);
char *get_suite_str(Suite suite);
bool move_card(Game *game);
bool is_same_pos(CardPos a, CardPos b);
bool handle_action(Action direction, Game *game);

#ifdef SOLITAIRE_CARDS_IMPLEMENTATION

Game *create_game() {
    // create memory for game, all game data is stored in this one memory buffer
    Game *game = malloc(sizeof(Game));
    if (!game) return NULL;

    // initialize stuff
    reset_game(game);
    return game;
}

void reset_game(Game *game) {
    reset_selected(game);

    Card cards[52] = {};
    for (int i = 0, suite = 0; suite < 4; ++suite) {
        for (int rank = 1; rank <= 13; ++rank, ++i) {
            // initialize card deck
            cards[i].rank = rank;
            cards[i].suite = suite;
            cards[i].visible = false;
            cards[i].highlight = NO_HIGHLIGHT;
        }
    }

    // shuffle card deck
    srand(time(NULL));
    for (int i = 51; i > 0; --i) {
        int j = rand() % (i + 1);
        Card temp = cards[i];
        cards[i] = cards[j];
        cards[j] = temp;
    }

    // clear foundation cards (4 piles of each suite)
    for (int i = 0; i < 4; ++i) {
        game->foundation[i].rank = NO_RANK;
        game->foundation[i].visible = true;
        game->foundation[i].highlight = NO_HIGHLIGHT;
    }

    // clear stock cards and waste cards (stock cards are not visible)
    for (int i = 0; i < 64; ++i) {
        game->stock[i].rank = NO_RANK;
        game->stock[i].visible = false;
        game->stock[i].highlight = NO_HIGHLIGHT;
        game->waste[i].rank = NO_RANK;
        game->waste[i].visible = false;
        game->waste[i].highlight = NO_HIGHLIGHT;
    }

    // put cards in tableau (main game area, 7 columns)
    int i = 0;
    for (int column = 0; column < 7; ++column) {
        for (int row = 0; row < 64; ++row) {
            game->tableau[column][row].rank = NO_RANK;
            game->tableau[column][row].visible = false;
            game->tableau[column][row].highlight = NO_HIGHLIGHT;
            if (row <= column) {
                game->tableau[column][row] = cards[i++];
            }
        }
    }

    // put the rest of the cards in the stock card pile
    for (int j = 0; i < 52; ++i, ++j) {
        game->stock[j] = cards[i];
    }

    update_display(game);
}

void update_display(Game *game) {
    // stuff to make the game work
    update_visible(game);
    clear_highlight(game);
    if (game->moving.active) {
        Card *card = get_card(game->moving, game, false);
        if (!card) {
            game->moving.active = false;
        } else {
            highlight_stackable(card, game->moving.location == TABLEAU, game);
        }
    }
    highlight_source(game);
}

void update_visible(Game *game) {
    // make the top card of each tableau column visible
    for (int column = 0; column < 7; ++column) {
        for (int row = 0; row < 64; ++row) {
            if (game->tableau[column][row].rank == NO_RANK) {
                if (row > 0) game->tableau[column][row - 1].visible = true;
                break;
            }
        }
    }
}

bool can_stack(Card card, Card above, bool is_foundation) {
    // can a card stack on another card?
    if (is_foundation) {
        // must be same suite and stack on the rank below it
        if (above.rank == NO_RANK)
            return card.rank == ACE;
        return card.suite == above.suite && card.rank == above.rank + 1;
    }
    // king is top of a column, other cards stack on a card with the opposite suite color and the rank above it, e.g 2 of clubs stacks on 3 of diamonds
    return (above.rank == NO_RANK && card.rank == KING) || (is_opposite_color(card.suite, above.suite) && above.rank == card.rank + 1);
}

void clear_highlight(Game *game) {
    // clears the "highlight"
    for (int i = 0; i < 4; ++i) {
        game->foundation[i].highlight = NO_HIGHLIGHT;
    }
    for (int i = 0; i < 64; ++i) {
        game->stock[i].highlight = NO_HIGHLIGHT;
        game->waste[i].highlight = NO_HIGHLIGHT;
    }
    for (int column = 0; column < 7; ++column) {
        for (int row = 0; row < 64; ++row) {
            game->tableau[column][row].highlight = NO_HIGHLIGHT;
        }
    }
}

void highlight_source(Game *game) {
    // highlight the card we are moving (source)
    if (!game->moving.active) return;
    Card *card = get_card(game->moving, game, true);
    if (!card) return;
    card->highlight = SOURCE;
}

int highlight_stackable(Card *card, bool tableau, Game *game) {
    // highlights cards that a card can be stacked on as "highlighted", which the action function uses
    clear_highlight(game);
    int count_stackable = 0;

    // if we are on the tableau and the next card is not empty, don't highlight moving to foundation
    // we cannot move more than one card at a time to the foundation
    if (!tableau || card[1].rank == NO_RANK) {
        for (int i = 0; i < 4; ++i) {
            bool stack = can_stack(*card, game->foundation[i], true);
            if (stack) {
                // TODO: instantly move card to the foundation if only the foundation cards are highlighted and no other cards
                // there is no need to select which foundation to move to
                ++count_stackable;
                game->foundation[i].highlight = HIGHLIGHTED;
            }
        }
    }

    for (int column = 0; column < 7; ++column) {
        for (int row = 0; row < 64; ++row) {
            if (game->tableau[column][row].rank == NO_RANK) {
                if (row > 0) {
                    if (game->tableau[column][row - 1].visible) {
                        bool stack = can_stack(*card, game->tableau[column][row - 1], false);
                        if (stack) {
                            ++count_stackable;
                            game->tableau[column][row - 1].highlight = HIGHLIGHTED;
                        }
                    }
                } else if (card->rank == KING) {
                    ++count_stackable;
                    game->tableau[column][row].highlight = HIGHLIGHTED;
                }
                break;
            }
        }
    }

    return count_stackable;
}

Card *get_waste_top(Game *game, bool no_rank) {
    // get waste cards
    // will return NULL if there is no card there (has NO_RANK) and if the no-rank argument is set to false
    // otherwise will return the first "non-existent" card if no-rank is true
    Card *card = NULL;
    for (int i = 0; i < 64; ++i) {
        if (game->waste[i].rank == NO_RANK) {
            if (no_rank && !card) return &game->waste[i];
            return card;
        }
        game->waste[i].visible = true;
        card = &game->waste[i];
    }
    return card;
}

Card *get_stock_top(Game *game, bool no_rank) {
    // get stock cards
    // will return NULL if there is no card there (has NO_RANK) and if the no-rank argument is set to false
    // otherwise will return the first "non-existent" card if no-rank is true
    Card *card = NULL;
    for (int i = 0; i < 64; ++i) {
        if (game->stock[i].rank == NO_RANK) {
            if (no_rank && !card) return &game->stock[i];
            return card;
        }
        game->stock[i].visible = true;
        card = &game->stock[i];
    }
    return card;
}

Card *get_card(CardPos pos, Game *game, bool no_rank) {
    // get a pointer to the card that a CardPos references
    // will return NULL if there is no card there (has NO_RANK) and if the no-rank argument is set to false
    if (!pos.active) return NULL;
    Card *card = NULL;
    switch (pos.location) {
        case TABLEAU:
            if (pos.column >= 0 && pos.column < 7 && pos.row >= 0 && pos.row < 64)
                card = &game->tableau[pos.column][pos.row];
            if (!no_rank && card && card->rank == NO_RANK) return NULL;
            break;
        case WASTE:
            card = get_waste_top(game, no_rank);
            break;
        case FOUNDATION:
            if (pos.column >= 0 && pos.column < 4)
                card = &game->foundation[pos.column];
            break;
        case STOCK:
            card = get_stock_top(game, no_rank);
            break;
    }
    return card;
}

int get_amount_stacked_cards(CardPos pos, Game *game) {
    // gets how many cards are stacked on top of the specified card
    // only works for cards in the tableau
    if (!pos.active) return 0;
    if (pos.location != TABLEAU) return 1;
    if (pos.column < 0 || pos.column >= 7 || pos.row < 0 || pos.row >= 64) return 0;
    int i = 0;
    for (int row = pos.row; row < 64; ++row, ++i) {
        if (game->tableau[pos.column][row].rank == NO_RANK) return i;
    }
    return i;
}

bool is_opposite_color(Suite suite1, Suite suite2) {
    return get_suite_color(suite1) != get_suite_color(suite2);
}

bool get_suite_color(Suite suite) {
    return suite & 2;
}

char *get_rank_str(Rank rank) {
    switch (rank) {
        case ACE:
            return "A";
        case RANK2:
            return "2";
        case RANK3:
            return "3";
        case RANK4:
            return "4";
        case RANK5:
            return "5";
        case RANK6:
            return "6";
        case RANK7:
            return "7";
        case RANK8:
            return "8";
        case RANK9:
            return "9";
        case RANK10:
            return "10";
        case JACK:
            return "J";
        case QUEEN:
            return "Q";
        case KING:
            return "K";
        default:
            return "";
    }
}

char *get_suite_str(Suite suite) {
    // \u doesn't work for some reason
    switch (suite) {
        case HEARTS:
            return "\xf3\xb0\xa3\x90 ";
        case DIAMONDS:
            return "\xf3\xb0\xa3\x8f ";
        case CLUBS:
            return "\xf3\xb0\xa3\x8e ";
        case SPADES:
            return "\xf3\xb0\xa3\x91 ";
    }
    return "";
}

void fix_selected_tableau(Game *game) {
    // moves the selected card to the nearest visible card in the tableau
    // what's the point of selecting a card that is flipped over? /rh
    if (!game->selected.active || game->selected.location != TABLEAU) return;
    Card *card_dir = NULL;
    while (true) {
        card_dir = get_card(game->selected, game, false);
        if (!card_dir) {
            if (game->selected.row == 0) {
                break;
            } else {
                --game->selected.row;
            }
        } else if (!card_dir->visible) {
            ++game->selected.row;
        } else if (card_dir->visible) break;
    }
}

void reset_selected(Game *game) {
    // reset the selected card back to the first column in the tableau
    game->selected.active = true;
    game->selected.location = TABLEAU;
    game->selected.column = 0;
    game->selected.row = 0;
    game->moving.active = false;
    fix_selected_tableau(game);
}

bool move_card(Game *game) {
    // main part of the game lol

    // don't move if there is no selected card
    if (!game->selected.active || !game->moving.active) return false;

    // card stack we're moving
    Card *source_card = get_card(game->moving, game, false);
    if (!source_card) return false;

    CardPos destination = game->selected;

    // where we're moving it to
    Card *orig_destination_card = get_card(destination, game, true);
    Card *destination_card = orig_destination_card;
    if (!destination_card) return false;
    if (destination_card->rank != NO_RANK && destination.location == TABLEAU) {
        // select the one on top of it, so we don't replace replaces
        ++destination.row;
        destination_card = get_card(destination, game, true);
        if (!destination_card || destination_card->rank != NO_RANK) return false;
    }

    if (!can_stack(*source_card, *orig_destination_card, destination.location == FOUNDATION)) return false;

    int amount = get_amount_stacked_cards(game->moving, game);
    if (amount < 1) return false;

    if (destination.location == TABLEAU) {
        // check to see the card stack can move
        CardPos last_destination = destination;
        last_destination.row += amount - 1;
        Card *last_destination_card = get_card(last_destination, game, true);
        if (!last_destination_card || last_destination_card->rank != NO_RANK) return false; // continue if there is no card there but not out of bounds

        // another check
        CardPos last_source = game->moving;
        last_source.row += amount - 1;
        Card *last_source_card = get_card(last_source, game, false);
        if (!last_source_card) return false; // continue if there is a card there

        if (last_source.location == TABLEAU) {
            // and another
            ++last_source.row;
            last_source_card = get_card(last_source, game, true);
            if (!last_source_card || last_source_card->rank != NO_RANK) return false; // continue if there is no card there but not out of bounds
        } else if (last_source.location == FOUNDATION) {
            if (source_card->rank == NO_RANK) return false;
        }

        memcpy(destination_card, source_card, sizeof(Card) * amount);

        if (last_source.location == FOUNDATION) {
            --source_card->rank; // decrease the rank on foundation
        } else {
            for (int i = 0; i < amount; ++i) {
                source_card = get_card(game->moving, game, false);
                if (!source_card) break;
                source_card->rank = NO_RANK;
                ++game->moving.row;
            }
        }
    } else if (destination.location == FOUNDATION) {
        if (game->moving.location == TABLEAU) {
            CardPos above = game->moving;
            ++above.row;

            Card *above_card = get_card(above, game, false);
            if (above_card) return false;
        }

        *destination_card = *source_card;
        source_card->rank = NO_RANK;
    } else return false;

    // finish up
    game->selected = destination;
    game->moving.active = false;
    update_visible(game);
    clear_highlight(game);
    fix_selected_tableau(game);
    return true;
}

bool is_same_pos(CardPos a, CardPos b) {
    return a.active && b.active && a.column == b.column && a.row == b.row && a.location == b.location;
}

bool handle_action(Action direction, Game *game) {
    // handle key presses
    update_visible(game);
    switch (direction) {
        case UP:
        case RIGHT:
        case DOWN:
        case LEFT:
            // handle this later in the code
            break;

        case CONFIRM:
            if (game->selected.active) {
                if (game->selected.location == STOCK) {
                    if (game->moving.active) return false;

                    Card *card = get_stock_top(game, false);
                    if (!card) {
                        for (int i = 0;; ++i) {
                            Card *waste_card = get_waste_top(game, false);
                            if (!waste_card) break;
                            game->stock[i] = *waste_card;
                            waste_card->rank = NO_RANK;
                        }
                    } else {
                        for (int i = 0; i < 64; ++i) {
                            if (game->waste[i].rank == NO_RANK) {
                                if (i < 63) game->waste[i + 1].rank = NO_RANK;
                                card->visible = true;
                                game->waste[i] = *card;
                                card->rank = NO_RANK;
                                break;
                            }
                        }
                        game->selected.location = WASTE;
                    }
                    return true;
                }

                Card *card = get_card(game->selected, game, false);
                if (card) {
                    if (game->moving.active) {
                        if (is_same_pos(game->moving, game->selected)) {
                            game->moving.active = false;
                            return true;
                        }
                        if (card->highlight == HIGHLIGHTED) {
                            return move_card(game);
                        }
                    } else {
                        int count = highlight_stackable(card, game->selected.location == TABLEAU, game);
                        if (count < 1) return false;
                        highlight_source(game);
                        game->moving = game->selected;
                        game->moving.active = true;
                        if (count == 1) {
                            for (int column = 0; column < 7; ++column) {
                                for (int row = 0; row < 64; ++row) {
                                    if (game->tableau[column][row].highlight == HIGHLIGHTED) {
                                        game->selected = (CardPos) {true, TABLEAU, column, row};
                                        return move_card(game);
                                    }
                                    if (game->tableau[column][row].rank == NO_RANK) break;
                                }
                            }
                        }
                    }
                }
                return false;
            }
            return false;

        case CANCEL:
            if (game->moving.active) {
                game->moving.active = false;
                return true;
            }
            return false;

        default:
            return false;
    }

    // return if there is no card selected
    if (!game->selected.active) return NULL;
    Card *card_dir = NULL;
    CardPos pos_dir;

    // cba to comment all of this, it basically selects the card in said direction of the previously selected card
    switch (game->selected.location) {
        case TABLEAU:
            switch (direction) {
                case UP:
                    pos_dir = game->selected;
                    --pos_dir.row;
                    card_dir = get_card(pos_dir, game, false);
                    if (game->selected.row <= 0 || !card_dir || !card_dir->visible) {
                        game->selected.row = 0;
                        if (game->selected.column >= 4) {
                            if (game->waste[0].rank != NO_RANK) {
                                game->selected.column = 0;
                                game->selected.location = WASTE;
                            } else if (game->selected.column == 4) {
                                game->selected.column = 3;
                                game->selected.location = FOUNDATION;
                            } else if (game->selected.column >= 6) {
                                game->selected.column = 0;
                                game->selected.location = STOCK;
                            }
                        } else {
                            game->selected.location = FOUNDATION;
                        }
                        fix_selected_tableau(game);
                        return true;
                    }
                    --game->selected.row;
                    return true;
                case RIGHT:
                    if (game->selected.column >= 6) {
                        game->selected.row = 0;
                        game->selected.column = 0;
                        game->selected.location = STOCK;
                    } else {
                        ++game->selected.column;
                        fix_selected_tableau(game);
                    }
                    return true;
                case DOWN:
                    pos_dir = game->selected;
                    ++pos_dir.row;
                    card_dir = get_card(pos_dir, game, false);
                    if (card_dir && card_dir->visible) {
                        game->selected = pos_dir;
                        return true;
                    }
                    return false;
                case LEFT:
                    if (game->selected.column > 0) {
                        --game->selected.column;
                        fix_selected_tableau(game);
                    }
                    return true;
                default:
                    return false;
            }
        case FOUNDATION:
            game->selected.row = 0;
            switch (direction) {
                case UP:
                    return false;
                case RIGHT:
                    if (game->selected.column >= 3) {
                        game->selected.column = 0;
                        if (game->waste[0].rank != NO_RANK) {
                            game->selected.location = WASTE;
                        } else {
                            game->selected.location = STOCK;
                        }
                        return true;
                    }
                    ++game->selected.column;
                    return true;
                case DOWN:
                    game->selected.location = TABLEAU;
                    fix_selected_tableau(game);
                    return true;
                case LEFT:
                    if (game->selected.column <= 0) return false;
                    --game->selected.column;
                    return true;
                default:
                    return false;
            }
        case WASTE:
        case STOCK:
            game->selected.row = 0;
            switch (direction) {
                case UP:
                    return false;
                case RIGHT:
                    if (game->selected.location == WASTE) {
                        game->selected.column = 3;
                        game->selected.location = STOCK;
                        return true;
                    }
                    return false;
                case DOWN:
                    game->selected.column = game->selected.location == WASTE ? 5 : 6;
                    game->selected.location = TABLEAU;
                    fix_selected_tableau(game);
                    return true;
                case LEFT:
                    if (game->waste[0].rank == NO_RANK || game->selected.location == WASTE) {
                        game->selected.column = 3;
                        game->selected.location = FOUNDATION;
                    } else {
                        game->selected.column = 0;
                        game->selected.location = WASTE;
                    }
                    return true;
                default:
                    return false;
            }
    }
    return false;
}

#endif
#endif
