#include "display.h"
#include "buffer.h"
#include "term.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define MAX_WINDOWS 2
Window windows[MAX_WINDOWS];
int num_windows = 1;
int active_window = 0;
int split_mode = 0;

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

// --- NEW HELPER: Renders a single line with region highlighting ---
void render_line(Buffer *buf, int file_row, Window *cw) {
    if (file_row >= buf->num_lines) return; // Empty line, already cleared
    
    Line *line = buf->lines[file_row];
    int len = line->length - cw->col_offset;
    if (len <= 0) return;
    if (len > cw->width) len = cw->width;
    
    // If no region is active, just draw it and return
    if (!buf->mark_active) {
        write(STDOUT_FILENO, &line->chars[cw->col_offset], len);
        return;
    }

    // Region is active, calculate bounds
    int sy, ey, sx, ex;
    if (buf->cy < buf->mark_y) { 
        sy = buf->cy; ey = buf->mark_y; 
        sx = buf->cx; ex = buf->mark_x; 
    }else if (buf->cy > buf->mark_y) { 
        sy = buf->mark_y; ey = buf->cy; 
        sx = buf->mark_x; ex = buf->cx; 
    }else { 
        sy = ey = buf->cy; 
        if (buf->cx < buf->mark_x) { 
            sx = buf->cx; ex = buf->mark_x; 
        } else { 
            sx = buf->mark_x; 
            ex = buf->cx; 
        } 
    }

    // If line is outside selection, draw normally and return
    if (file_row < sy || file_row > ey) {
        write(STDOUT_FILENO, &line->chars[cw->col_offset], len);
        return;
    }

    // Line is inside selection, calculate clamped bounds
    int vis_start = cw->col_offset;
    int vis_end = cw->col_offset + len;
    int draw_start = vis_start;
    int draw_end = vis_end;
    
    if (file_row == sy) draw_start = sx;
    if (file_row == ey) draw_end = ex;
    
    if (draw_start < vis_start) draw_start = vis_start;
    if (draw_start > vis_end) draw_start = vis_end;
    if (draw_end < vis_start) draw_end = vis_start;
    if (draw_end > vis_end) draw_end = vis_end;

    // Print the 3 parts
    if (vis_start < draw_start) write(STDOUT_FILENO, &line->chars[vis_start], draw_start - vis_start);
    
    if (draw_start < draw_end) {
        write(STDOUT_FILENO, "\x1b[7m", 4);
        write(STDOUT_FILENO, &line->chars[draw_start], draw_end - draw_start);
        write(STDOUT_FILENO, "\x1b[0m", 4);
    }
    
    if (draw_end < vis_end) {
        write(STDOUT_FILENO, &line->chars[draw_end], vis_end - draw_end);
    }
}

void display_draw(Buffer *buf) {
    int screen_rows, screen_cols;
    term_get_size(&screen_rows, &screen_cols);

    // Setup window boundaries
    if (num_windows == 1) {
        windows[0].x = 0; windows[0].y = 0;
        windows[0].width = screen_cols; windows[0].height = screen_rows - 1;
    } else if(split_mode == 1){
        int half = screen_cols / 2;
        windows[0].x = 0; windows[0].y = 0;
        windows[0].width = half; windows[0].height = screen_rows - 1;
        
        windows[1].x = half + 1; windows[1].y = 0;
        windows[1].width = screen_cols - half - 1; 
        windows[1].height = screen_rows - 1;
    }else if (split_mode == 2) {
        int half_h = (screen_rows - 1) / 2;
        windows[0].x = 0; windows[0].y = 0;
        windows[0].width = screen_cols; windows[0].height = half_h;
        windows[1].x = 0; windows[1].y = half_h + 1;
        windows[1].width = screen_cols;
        windows[1].height = (screen_rows - 1) - half_h - 1;
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

    term_hide_cursor();
    term_cursor_pos(0, 0);

    // Draw separator if 2 windows
    if (num_windows == 2) {
        if(split_mode == 1){
            for (int y = 0; y < screen_rows - 1; y++) {
                term_cursor_pos(screen_cols / 2, y);
                write(STDOUT_FILENO, "|", 1);
            }
        }else {
            int half_h = (screen_rows - 1) / 2;
            term_cursor_pos(0, half_h);
            for (int x = 0; x < screen_cols; x++) {
                write(STDOUT_FILENO, "-", 1);
            }
        }
    }

    // --- FLAT WINDOW DRAWING LOOP ---
    for (int w = 0; w < num_windows; w++) {
        Window *cw = &windows[w];
        char blank[cw->width + 1];
        memset(blank, ' ', cw->width);
        blank[cw->width] = '\0';

        for (int y = 0; y < cw->height; y++) {
            term_cursor_pos(cw->x, cw->y + y);
            write(STDOUT_FILENO, blank, cw->width); // Clear line
            term_cursor_pos(cw->x, cw->y + y);
            render_line(windows[w].buf, y + cw->row_offset, cw); 
        }
    }

    // Draw Status Line
    term_cursor_pos(0, screen_rows - 1);
    term_clear_line();
    write(STDOUT_FILENO, "\x1b[7m", 4);
    char status[120];
    int status_len = snprintf(status, sizeof(status), 
    			"%s File: %s -- Line: %d -- Col: %d -- Win: %d", 
                        buf->dirty ? "**" : "  ", 
                        buf->filename ? buf->filename : "[None]", 
                        buf->cy + 1, buf->cx + 1, active_window + 1);
    write(STDOUT_FILENO, status, status_len);
    for(int i = status_len; i < screen_cols; i++){
        write(STDOUT_FILENO, " ", 1);
    }
    write(STDOUT_FILENO, "\x1b[0m", 4);

    // Move cursor to active window
    term_cursor_pos(v_cx - aw->col_offset + aw->x, 
    		    buf->cy - aw->row_offset + aw->y);
    term_show_cursor();
}
