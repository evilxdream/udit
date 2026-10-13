#ifndef DISPLAY_H
#define DISPLAY_H
#include "buffer.h"

typedef struct{
    int x,y;
    int width, height;
    int row_offset;
    int col_offset;
    Buffer *buf;
}Window ;

void display_init(Buffer *buf);
void display_draw(Buffer *active_buf);
void display_reset_scroll();

Buffer *display_split(int mode);
void display_unsplit();
Buffer *display_next_window();
int display_get_active();
Window *display_get_windows(int *count);

#endif
