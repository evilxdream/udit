#include "display.h"
#include "buffer.h"
#include "term.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_WINDOWS 2
Window windows[MAX_WINDOWS];
int num_windows = 1;
int active_window = 0;
int split_mode = 0;
char current_message[256];

struct abuf {
    char *b;
    int len;
    int cap;
};

#define ABUF_INIT {NULL, 0}

void ab_append(struct abuf *ab, const char *s, int len){
    if(ab->len + len > ab->cap){
        while (ab->len + len > ab->cap) {
            ab->cap = ab->cap ? ab->cap * 2 : 1024;
        }
        ab->b = realloc(ab->b, ab->cap);
    }
    memcpy(&ab->b[ab->len], s, len);
    ab->len += len;
}

void ab_free(struct abuf *ab){
    free(ab->b);
}

void display_set_message(const char *msg){
    strncpy(current_message, msg, sizeof(current_message) -1);
}

void display_init(Buffer *buf) {
    num_windows = 1;
    active_window = 0;
    windows[0].row_offset = 0;
    windows[0].col_offset = 0;
    windows[0].buf = buf;
}

void display_reset_scroll() {
    windows[active_window].row_offset = 0;
    windows[active_window].col_offset = 0;
}

Buffer *display_split(int mode) {
    if (num_windows >= MAX_WINDOWS) return windows[active_window].buf;
    num_windows = 2;
    split_mode = mode;
    windows[1].row_offset = 0;
    windows[1].col_offset = 0;
    windows[1].buf = windows[active_window].buf;
    active_window = 1;
    return windows[1].buf;
}

void display_unsplit() {
    num_windows = 1;
    split_mode = 0;
    active_window = 0;
}

Buffer *display_next_window() {
    active_window = (active_window + 1) % num_windows;
    return windows[active_window].buf;
}

int display_get_active() { return active_window; }
Window *display_get_windows(int *count) { *count = num_windows; return windows; }

// Calculate visual column (Tabs count as 8 spaces)
int get_visual_cx(Buffer *buf) {
    int visual_cx = 0;
    Line *line = buf->lines[buf->cy];
    for (int i = 0; i < buf->cx; i++) {
        if (line->chars[i] == '\t') visual_cx += (8 - (visual_cx % 8));
        else visual_cx++;
    }
    return visual_cx;
}

// --- NEW HELPER: Renders a single line with visual spaces instead of real tabs ---
void render_line(Buffer *buf, int file_row, Window *cw, struct abuf *ab) {
    if (file_row >= buf->num_lines) return;
    
    Line *line = buf->lines[file_row];
    if (line->length == 0) return;
    
    // Helper macro to draw a character visually (converting tabs to spaces)
    #define DRAW_CHAR(c, is_highlighted) \
        if (is_highlighted) ab_append(ab, "\x1b[7m", 4); \
        if (c == '\t') { \
            int tab_width = 8 - (visual_x % 8); \
            for (int s = 0; s < tab_width; s++) ab_append(ab, " ", 1); \
        } else { \
            ab_append(ab, &c, 1); \
        } \
        if (is_highlighted) ab_append(ab, "\x1b[0m", 4);

    // 1. No region active
    if (!buf->mark_active) {
        int visual_x = 0;
        for (int i = 0; i < line->length; i++) {
            char c = line->chars[i];
            int char_width = (c == '\t') ? (8 - (visual_x % 8)) : 1;
            
            if (visual_x + char_width > cw->col_offset && visual_x < cw->col_offset + cw->width) {
                if (visual_x >= cw->col_offset) {
                    DRAW_CHAR(c, 0);
                } else {
                    // Partially visible on the left edge
                    for (int s = 0; s < char_width; s++) {
                        if (visual_x + s >= cw->col_offset) ab_append(ab, " ", 1);
                    }
                }
            }
            visual_x += char_width;
            if (visual_x >= cw->col_offset + cw->width) break;
        }
        return;
    }

    // 2. Region is active
    int sy, ey, sx, ex;
    if (buf->cy < buf->mark_y) { sy = buf->cy; ey = buf->mark_y; sx = buf->cx; ex = buf->mark_x; }
    else if (buf->cy > buf->mark_y) { sy = buf->mark_y; ey = buf->cy; sx = buf->mark_x; ex = buf->cx; }
    else { sy = ey = buf->cy; if (buf->cx < buf->mark_x) { sx = buf->cx; ex = buf->mark_x; } else { sx = buf->mark_x; ex = buf->cx; } }

    if (file_row < sy || file_row > ey) {
        // Outside selection
        int visual_x = 0;
        for (int i = 0; i < line->length; i++) {
            char c = line->chars[i];
            int char_width = (c == '\t') ? (8 - (visual_x % 8)) : 1;
            if (visual_x + char_width > cw->col_offset && visual_x < cw->col_offset + cw->width) {
                if (visual_x >= cw->col_offset) {
                    DRAW_CHAR(c, 0);
                } else {
                    for (int s = 0; s < char_width; s++) {
                        if (visual_x + s >= cw->col_offset) ab_append(ab, " ", 1);
                    }
                }
            }
            visual_x += char_width;
            if (visual_x >= cw->col_offset + cw->width) break;
        }
        return;
    }

    // Line is inside selection
    int highlight_start_byte = 0;
    int highlight_end_byte = line->length;
    int curr_visual = 0;
    for (int i = 0; i <= line->length; i++) {
        if (file_row == sy && curr_visual == sx) highlight_start_byte = i;
        if (file_row == ey && curr_visual == ex) highlight_end_byte = i;
        if (i < line->length) {
            char c = line->chars[i];
            curr_visual += (c == '\t') ? (8 - (curr_visual % 8)) : 1;
        }
    }

    int visual_x = 0;
    for (int i = 0; i < line->length; i++) {
        char c = line->chars[i];
        int char_width = (c == '\t') ? (8 - (visual_x % 8)) : 1;
        
        if (visual_x + char_width > cw->col_offset && visual_x < cw->col_offset + cw->width) {
            int is_highlighted = (i >= highlight_start_byte && i < highlight_end_byte);
            
            if (visual_x >= cw->col_offset) {
                DRAW_CHAR(c, is_highlighted);
            } else {
                // Partially visible on left edge (never highlighted if it's the first char of a split)
                for (int s = 0; s < char_width; s++) {
                    if (visual_x + s >= cw->col_offset) {
                        if (is_highlighted) ab_append(ab, "\x1b[7m", 4);
                        ab_append(ab, " ", 1);
                        if (is_highlighted) ab_append(ab, "\x1b[0m", 4);
                    }
                }
            }
        }
        visual_x += char_width;
        if (visual_x >= cw->col_offset + cw->width) break;
    }
}

void display_draw(Buffer *buf) {
    int screen_rows, screen_cols;
    term_get_size(&screen_rows, &screen_cols);

    // Setup window boundaries
    if (num_windows == 1) {
        windows[0].x = 0; windows[0].y = 0;
        windows[0].width = screen_cols; windows[0].height = screen_rows - 2;
    } else if(split_mode == 1){
        int half = screen_cols / 2;
        windows[0].x = 0; windows[0].y = 0;
        windows[0].width = half; windows[0].height = screen_rows - 2;
        
        windows[1].x = half + 1; windows[1].y = 0;
        windows[1].width = screen_cols - half - 1; 
        windows[1].height = screen_rows - 2;
    }else if (split_mode == 2) {
        int half_h = (screen_rows - 2) / 2;
        windows[0].x = 0; windows[0].y = 0;
        windows[0].width = screen_cols; windows[0].height = half_h;
        windows[1].x = 0; windows[1].y = half_h + 1;
        windows[1].width = screen_cols;
        windows[1].height = (screen_rows - 2) - half_h - 1;
    }

    // Update scrolling based on the ACTIVE window
    Window *aw = &windows[active_window];
    if (buf->cy < aw->row_offset) aw->row_offset = buf->cy;
    if (buf->cy >= aw->row_offset + aw->height) {
        aw->row_offset = buf->cy - aw->height + 1;
    }
    
    int v_cx = get_visual_cx(buf);
    if (v_cx < aw->col_offset) aw->col_offset = v_cx;
    if (v_cx >= aw->col_offset + aw->width) aw->col_offset = v_cx - aw->width + 1;
    
    static int last_row_offset = -1;
    static int last_col_offset = -1;
    static int last_num_windows = -1;
    static int last_cx = -1;
    static int last_cy = -1;
    
    int cursor_moved = (buf->cx != last_cx || buf->cy != last_cy);
    
    int must_redraw = buf->needs_redraw ||
                      aw->row_offset != last_row_offset ||
                      aw->col_offset != last_col_offset ||
                      num_windows != last_num_windows ||
                      (buf->mark_active && cursor_moved);
    
    struct abuf ab = ABUF_INIT;

    //term_hide_cursor();
    ab_append(&ab, "\x1b[?25l", 6);
    
    if(must_redraw){
        char buf_temp[32];
        int len = snprintf(buf_temp, sizeof(buf_temp), "\x1b[H");
        ab_append(&ab, buf_temp, len);
        
        if (num_windows == 2) {
            if(split_mode == 1){
                for (int y = 0; y < screen_rows - 2; y++) {
                    len = snprintf(buf_temp, sizeof(buf_temp),
                    "\x1b[%d;%dH",
                    y + 1, (screen_cols / 2) + 1);
                    ab_append(&ab, buf_temp, len);
                    ab_append(&ab, "|", 1);
                }
            }else {
                int half_h = (screen_rows - 2) / 2;
                len = snprintf(buf_temp, sizeof(buf_temp), "\x1b[%d;%dH", 1, 1);
                ab_append(&ab, buf_temp, len);
                for (int x = 0; x < screen_cols; x++) {
                    ab_append(&ab, "-", 1);
                }
            }
        }
        for (int w = 0; w < num_windows; w++) {
            Window *cw = &windows[w];
            for (int y = 0; y < cw->height; y++) {
                len = snprintf(buf_temp,
                               sizeof(buf_temp), "\x1b[%d;%dH",
                               cw->y + y + 1, cw->x + 1);
                ab_append(&ab, buf_temp, len); // Clear line
                ab_append(&ab, "\x1b[K", 3);
                render_line(windows[w].buf, y + cw->row_offset, cw, &ab); 
            }
        }
        buf->needs_redraw = 0;
        last_row_offset = aw->row_offset;
        last_col_offset = aw->col_offset;
        last_num_windows = num_windows;
        last_cx = buf->cx;
        last_cy = buf->cy;
    }
    
    // Draw Status Line
    char buf_temp[32];
    int len = snprintf(buf_temp, sizeof(buf_temp), "\x1b[%d;%dH", screen_rows - 1, 1);
    ab_append(&ab, buf_temp, len);
    ab_append(&ab, "\x1b[7m", 4);
    char status[120];
    int status_len = snprintf(status, sizeof(status), 
    			"%s File: %s -- Line: %d -- Col: %d -- Win: %d", 
                        buf->dirty ? "**" : "  ", 
                        buf->filename ? buf->filename : "[None]", 
                        buf->cy + 1, buf->cx + 1, active_window + 1);
    ab_append(&ab, status, status_len);
    for(int i = status_len; i < screen_cols; i++){
        ab_append(&ab, " ", 1);
    }
    ab_append(&ab, "\x1b[0m", 4);
    
    //term_cursor_pos(0, screen_rows - 1);
    //term_clear_line();
    len = snprintf(buf_temp, sizeof(buf_temp), "\x1b[%d;%dH", screen_rows, 1);
    ab_append(&ab, buf_temp, len);
    ab_append(&ab, "\x1b[K", 3);
    if(strlen(current_message) > 0){
        ab_append(&ab, current_message, strlen(current_message));
    }

    // Move cursor to active window
    //term_cursor_pos(v_cx - aw->col_offset + aw->x, 
    //		    buf->cy - aw->row_offset + aw->y);
    len = snprintf(buf_temp, sizeof(buf_temp), "\x1b[%d;%dH",
                   (buf->cy -aw->row_offset + aw->y) + 1,
                   (v_cx - aw->col_offset + aw->x)+ 1);
    ab_append(&ab, buf_temp, len);
    //term_show_cursor();
    ab_append(&ab, "\x1b[?25h", 6);
    write(STDOUT_FILENO, ab.b, ab.len);
    ab_free(&ab);
}
