#ifndef TERM_H
#define TERM_H

enum EditorKey {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    META_X,
    META_W,
    META_PERCENT,
    META_BANG,
    META_F,
    META_B,
    META_V,
    META_SLASH
};

void term_init();
void term_restore();
void term_clear_screen();
void term_cursor_pos(int x, int y);
int term_read_key();
void term_get_size(int *rows, int *cols);
void term_hide_cursor();
void term_show_cursor();
void term_clear_line();
void term_enter_alt_screen();
void term_exit_alt_screen();

#endif
