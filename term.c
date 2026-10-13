#include "term.h"
#include <asm-generic/ioctls.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <poll.h>

struct termios orig_termios;

void term_get_size(int *rows, int *cols){
    struct winsize ws;
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0){
        *rows = 24;
        *cols = 80;
    } else {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
    }
}

void term_hide_cursor(){
    write(STDOUT_FILENO, "\x1b[?25l", 6);
}

void term_show_cursor(){
    write(STDOUT_FILENO, "\x1b[?25h", 6);
}

void term_clear_line(){
    write(STDOUT_FILENO, "\x1b[K", 3);
}

void term_restore(){
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void term_init(){
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(term_restore);
    struct termios raw = orig_termios;

    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_oflag &= ~(OPOST);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void term_clear_screen(){
    write(STDOUT_FILENO, "\x1b[2J\x1b[H", 7);
}

void term_cursor_pos(int x, int y){
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", y + 1, x + 1);
    write(STDOUT_FILENO, buf, len);
}

int term_read_key(){
    char c;
    int nread = read(STDIN_FILENO, &c, 1);
    if(nread != 1) return -1;

    if(c != '\x1b'){
        return c;
    }

    struct pollfd pfd = {STDIN_FILENO, POLLIN, 0};
    if(poll(&pfd, 1, 10) <= 0){
        return '\x1b';
    }

    char seq[2];
    if(read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    //if(read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

    if(seq[0] == '['){
        if(read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
        switch (seq[1]) {
            case 'A': return ARROW_UP;
            case 'B': return ARROW_DOWN;
            case 'C': return ARROW_RIGHT;
            case 'D': return ARROW_LEFT;
        }
    }

    if(seq[0] == 'x'){
        return META_X;
    }
    if(seq[0] == 'w') return META_W;
    if(seq[0] == '%'){
        return META_PERCENT;
    }
    if(seq[0] == '!') return META_BANG;
    if(seq[0] == 'f') return META_F;
    if(seq[0] == 'b') return META_B;
    if(seq[0] == 'v') return META_V;
    return '\x1b';
}

void term_enter_alt_screen(){
    write(STDOUT_FILENO, "\x1b[?1049h", 8);
}

void term_exit_alt_screen(){
    write(STDOUT_FILENO, "\x1b[?104l", 8);
}
