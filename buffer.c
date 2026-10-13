#include "buffer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

char *kill_ring = NULL;

// --- UNDO HISTORY STACK ---
#define UNDO_LIMIT 100
Buffer *undo_stack[UNDO_LIMIT];
int undo_count = 0;

// Helper to deeply copy a buffer
Buffer *buffer_copy(Buffer *buf) {
    Buffer *copy = malloc(sizeof(Buffer));
    copy->num_lines = buf->num_lines;
    copy->cx = buf->cx;
    copy->cy = buf->cy;
    copy->filename = buf->filename ? strdup(buf->filename) : NULL;
    copy->lines = malloc(sizeof(Line *) * buf->num_lines);
    for (int i = 0; i < buf->num_lines; i++) {
        copy->lines[i] = malloc(sizeof(Line));
        copy->lines[i]->capacity = buf->lines[i]->capacity;
        copy->lines[i]->length = buf->lines[i]->length;
        copy->lines[i]->chars = malloc(buf->lines[i]->capacity);
        memcpy(copy->lines[i]->chars, 
        buf->lines[i]->chars, 
        buf->lines[i]->capacity);
    }
    return copy;
}

void buffer_save_history(Buffer *buf) {
    // If stack is full, discard the oldest state
    if (undo_count >= UNDO_LIMIT) {
        buffer_free(undo_stack[0]);
        memmove(&undo_stack[0], 
        	&undo_stack[1], 
        	sizeof(Buffer *) * (UNDO_LIMIT - 1));
        undo_count--;
    }
    undo_stack[undo_count++] = buffer_copy(buf);
}

// --- Helper Function: Create a new empty line ---
Line *line_create() {
    Line *line = malloc(sizeof(Line));
    line->capacity = 16; // Start with room for 16 characters
    line->length = 0;
    line->chars = malloc(line->capacity);
    line->chars[0] = '\0'; // Null-terminate it so it's a valid C string
    return line;
}

// --- Buffer Creation and Destruction ---

Buffer *buffer_create() {
    Buffer *buf = malloc(sizeof(Buffer));
    
    // Start with space for 16 lines
    buf->lines = malloc(sizeof(Line *) * 16);
    buf->lines[0] = line_create(); // Create the first empty line
    buf->num_lines = 1;
    
    buf->cx = 0; // Cursor starts at column 0
    buf->cy = 0; // Cursor starts at row 0
    buf->filename = NULL;
    buf->mark_x = 0;
    buf->mark_y = 0;
    buf->mark_active = 0;
    buf->dirty = 0;
    
    return buf;
}

void buffer_free(Buffer *buf) {
    // Free every single line's string, then the line struct itself
    for (int i = 0; i < buf->num_lines; i++) {
        free(buf->lines[i]->chars);
        free(buf->lines[i]);
    }
    free(buf->lines);
    if(buf->filename) free(buf->filename);
    free(buf);
}

// --- Text Manipulation ---

void buffer_insert_char(Buffer *buf, char c) {
    buffer_save_history(buf);
    Line *line = buf->lines[buf->cy];
    
    if (line->length >= line->capacity - 1) {
        line->capacity *= 2;
        line->chars = realloc(line->chars, line->capacity);
    }
    
    memmove(&line->chars[buf->cx + 1], 
    	    &line->chars[buf->cx], 
    	    line->length - buf->cx);
    line->chars[buf->cx] = c;
    line->length++;
    
    // FIX: Add the null terminator!
    line->chars[line->length] = '\0';
    
    buf->cx++;
    buf->dirty = 1;
}

// --- Cursor Movement ---

void buffer_move_cursor(Buffer *buf, char direction) {
    Line *line = buf->lines[buf->cy];
    
    switch (direction) {
        case 'L': // Left
            if (buf->cx > 0) {
                buf->cx--;
            }else if (buf->cy > 0) {
                buf->cy--;
                buf->cx = buf->lines[buf->cy]->length;
            }
            break;
            
        case 'R': // Right
            // Don't move past the end of the line
            if (buf->cx < line->length) {
                buf->cx++;
            }else if (buf->cy < buf->num_lines -1) {
                buf->cy++;
                buf->cx = 0;
            }
            break;
            
        case 'U': // Up
            if (buf->cy > 0) {
                buf->cy--;
                // If the new line is shorter than the current cursor X position,
                // move the cursor to the end of the new line.
                if (buf->cx > buf->lines[buf->cy]->length) {
                    buf->cx = buf->lines[buf->cy]->length;
                }
            }
            break;
            
        case 'D': // Down
            if (buf->cy < buf->num_lines - 1) {
                buf->cy++;
                if (buf->cx > buf->lines[buf->cy]->length) {
                    buf->cx = buf->lines[buf->cy]->length;
                }
            }
            break;
    }
}

void buffer_delete_char(Buffer *buf){
    buffer_save_history(buf);
    Line *line = buf->lines[buf->cy];

    if(buf->cx == 0 && buf->cy > 0){
        Line *prev_line = buf->lines[buf->cy - 1];
        int new_cx = prev_line->length;
        int total_length = prev_line->length + line->length;

        if(total_length >= prev_line->capacity){
            prev_line->capacity = total_length + 1;
            prev_line->chars = realloc(prev_line->chars, prev_line->capacity);
        }

        memcpy(prev_line->chars + prev_line->length, line->chars, line->length);
        prev_line->length = total_length;
        prev_line->chars[prev_line->length] = '\0';
        free(line->chars);
        free(line);

        memmove(&buf->lines[buf->cy], &buf->lines[buf->cy + 1],
                sizeof(Line *) * (buf->num_lines - buf->cy -1));
        buf->num_lines --;
        buf->cy --;
        buf->cx = new_cx;
    } else if(buf->cx > 0){
        memmove(&line->chars[buf->cx - 1],
                &line->chars[buf->cx],
                line->length - buf->cx);
        line->length--;
        buf->cx--;
        line->chars[line->length] = '0';
        buf->dirty = 1;
    }
}

void buffer_insert_newline(Buffer *buf, int auto_indent) {
    Line *line = buf->lines[buf->cy];
    
    // 1. Calculate total indentation of the current line
    int indent_len = 0;
    if (auto_indent) {
        while (indent_len < line->length 
               && (line->chars[indent_len] == ' ' 
               || line->chars[indent_len] == '\t')) {
            indent_len++;
        }
    }
    
    // 2. Figure out how much indent to ACTUALLY add to the new line.
    // If the cursor is inside the indentation, we only indent up to the cursor.
    // If the cursor is at the beginning, we don't add extra indent (the text already has it!)
    int add_indent = indent_len;
    if (auto_indent && buf->cx < indent_len) {
        add_indent = buf->cx;
    } else if (buf->cx == 0) {
        add_indent = 0;
    }
    
    Line *new_line = line_create();
    
    int text_to_move = line->length - buf->cx;
    int total_new_len = add_indent + text_to_move;
    
    // Ensure the new line has enough capacity
    if (total_new_len >= new_line->capacity) {
        new_line->capacity = total_new_len + 1;
        new_line->chars = realloc(new_line->chars, new_line->capacity);
    }
    
    // 3. Copy the exact amount of indentation we calculated
    if (add_indent > 0) {
        memcpy(new_line->chars, line->chars, add_indent);
    }
    
    // 4. Copy the text that needs to move down to the next line
    if (text_to_move > 0) {
        memcpy(new_line->chars + add_indent, &line->chars[buf->cx], text_to_move);
    }
    
    new_line->length = total_new_len;
    new_line->chars[new_line->length] = '\0';
    
    // 5. Truncate the old line where the cursor is
    line->length = buf->cx;
    line->chars[line->length] = '\0';
    
    // 6. Shift lines down and insert the new line
    buf->lines = realloc(buf->lines, sizeof(Line *) * (buf->num_lines + 1));
    memmove(&buf->lines[buf->cy + 2], &buf->lines[buf->cy + 1], 
            sizeof(Line *) * (buf->num_lines - buf->cy - 1));
            
    buf->lines[buf->cy + 1] = new_line;
    buf->num_lines++;
    
    // 7. Move cursor to the new line, exactly at the end of the new indent
    buf->cy++;
    buf->cx = add_indent;
    buf->dirty = 1;
}

void buffer_move_cursor_home(Buffer *buf){
    buf->cx = 0;
}

void buffer_move_cursor_end(Buffer *buf){
    buf->cx = buf->lines[buf->cy]->length;
}

void buffer_kill_line(Buffer *buf){
    buffer_save_history(buf);
    Line *line = buf->lines[buf->cy];

    if(buf->cx < line->length){
        if(kill_ring) free(kill_ring);
        int kill_len = line->length - buf->cx;
        kill_ring = malloc(kill_len + 1);
        memcpy(kill_ring, &line->chars[buf->cx], kill_len);
        kill_ring[kill_len] = '\0';

        line->length = buf->cx;
        line->chars[line->length] = '\0';
    } else if(buf->cy < buf->num_lines -1){
        if(kill_ring) free(kill_ring);
        kill_ring = malloc(2);
        kill_ring[0] = '\n';
        kill_ring[1] = '\0';
        buf->cy++;
        buf->cx = 0;
        buffer_delete_char(buf);
    }
}

void buffer_yank(Buffer *buf) {
    if (!kill_ring) return;
    
    // Calculate length safely using a standard C function
    int kill_len = 0;
    while (kill_ring[kill_len] != '\0' && kill_len < 100000) {
        kill_len++;
    }
    
    for (int i = 0; i < kill_len; i++) {
        if (kill_ring[i] == '\n') {
            buffer_insert_newline(buf, 0);
        } else {
            buffer_insert_char(buf, kill_ring[i]);
        }
    }
}

void buffer_delete_forward(Buffer *buf) {
    buffer_save_history(buf);
    Line *line = buf->lines[buf->cy];
    
    if (buf->cx < line->length) {
        memmove(&line->chars[buf->cx], 
        	&line->chars[buf->cx + 1], 
        	line->length - buf->cx - 1);
        line->length--;
        
        // FIX: Add the null terminator!
        line->chars[line->length] = '\0';
        buf->dirty = 1;
    } else if (buf->cy < buf->num_lines - 1) {
        buf->cy++;
        buf->cx = 0;
        buffer_delete_char(buf);
    }
}

int buffer_save(Buffer *buf){
    if(!buf->filename) return 0;

    FILE *f = fopen(buf->filename, "w");
    if(!f) return -1;

    for (int i = 0; i < buf->num_lines; i++) {
        fwrite(buf->lines[i]->chars, 1, buf->lines[i]->length, f);
        if(i < buf->num_lines -1){
            fputc('\n', f);
        }
    }
    fclose(f);
    buf->dirty = 0;
    return buf->num_lines;
}

void buffer_open(Buffer *buf, const char *filename) {
    // Save the filename
    if (buf->filename) free(buf->filename);
    buf->filename = malloc(strlen(filename) + 1);
    strcpy(buf->filename, filename);
    
    // Clear the current buffer
    for (int i = 0; i < buf->num_lines; i++) {
        free(buf->lines[i]->chars);
        free(buf->lines[i]);
    }
    free(buf->lines);
    
    buf->lines = malloc(sizeof(Line *) * 16);
    buf->lines[0] = line_create();
    buf->num_lines = 1;
    buf->cx = 0;
    buf->cy = 0;
    
    FILE *f = fopen(filename, "r");
    if (!f) return; // File doesn't exist yet. That's fine.
    
    int c;
    Line *current = buf->lines[0];
    
    // Read file character by character!
    while ((c = fgetc(f)) != EOF) {
        if (c == '\n') {
            // Create a new empty line
            buf->lines = realloc(buf->lines, sizeof(Line *) * (buf->num_lines + 1));
            buf->lines[buf->num_lines] = line_create();
            buf->num_lines++;
            current = buf->lines[buf->num_lines - 1];
        } else if (c != '\r') {
            // Append character to current line
            if (current->length >= current->capacity - 1) {
                current->capacity *= 2;
                current->chars = realloc(current->chars, current->capacity);
            }
            current->chars[current->length++] = c;
            current->chars[current->length] = '\0';
        }
    }
    fclose(f);
}

int buffer_search(Buffer *buf, const char *query, int start_cx, int start_cy,
                  int *match_cx, int *match_cy){
    if(!query || query[0] == '\0') return 0;

    for (int y = start_cy; y < buf->num_lines; y++) {
        Line *line = buf->lines[y];
        int search_start = (y == start_cy) ? start_cx : 0;

        if(search_start < line->length){
            char *found = strstr(line->chars + search_start, query);
            if(found){
                *match_cx = found - line->chars;
                *match_cy = y;
                return 1;
            }
        }
    }
    for (int y = 0; y <= start_cy; y++) {
        Line *line = buf->lines[y];
        char *search_ptr = line->chars;
        while ((search_ptr = strstr(search_ptr, query)) != NULL) {
            int match_pos = search_ptr - line->chars;
            if(y == start_cy && match_pos >= start_cx){
                break;
            }
            *match_cx = match_pos;
            *match_cy = y;
            return 1;
            search_ptr++;
        }
    }
    return 0;
}

void buffer_replace_at_cursor(Buffer *buf, int match_len,
                              const char *replace_str){
    Line *line = buf->lines[buf->cy];
    int replace_len = strlen(replace_str);
    int new_length = line->length - match_len + replace_len;

    if(new_length >= line->capacity){
        while(new_length>= line->capacity) line->capacity *=2;
        line->chars = realloc(line->chars, line->capacity);
    }

    char *match_end = &line->chars[buf->cx + match_len];
    char *new_match_end = &line->chars[buf->cx + replace_len];
    int remaining_len = line->length - (buf->cx + match_len);

    if(remaining_len > 0){
        memmove(new_match_end, match_end, remaining_len);
    }

    memcpy(&line->chars[buf->cx], replace_str, replace_len);
    line->length = new_length;
    line->chars[line->length] = '\0';
    buf->cx += replace_len;
}

// Helper to extract text between mark and cursor
// Helper to extract text between mark and cursor
char *buffer_extract_region(Buffer *buf, int sy, int sx, int ey, int ex) {
    // 1. Strictly clamp boundaries to valid line lengths
    if (sy < 0) sy = 0;
    if (ey < 0) ey = 0;
    if (sy >= buf->num_lines) sy = buf->num_lines - 1;
    if (ey >= buf->num_lines) ey = buf->num_lines - 1;
    
    int len_sy = buf->lines[sy]->length;
    int len_ey = buf->lines[ey]->length;
    
    if (sx < 0) sx = 0;
    if (ex < 0) ex = 0;
    if (sx > len_sy) sx = len_sy;
    if (ex > len_ey) ex = len_ey;
    
    // 2. Safely calculate total length needed
    int len = 0;
    if (sy == ey) {
        len = ex - sx;
    } else {
        len += len_sy - sx; // Rest of first line
        for (int i = sy + 1; i < ey; i++) {
            len += buf->lines[i]->length + 1; // +1 for \n
        }
        len += ex; // Start of last line up to cursor
        len++; // +1 for \n between first and second line
    }
    
    if (len < 0) len = 0;
    
    // Allocate memory initialized to zero
    char *text = calloc(len + 1, 1);
    int pos = 0;
    
    // 3. Copy character by character safely
    if (sy == ey) {
        for (int i = sx; i < ex; i++) {
            text[pos++] = buf->lines[sy]->chars[i];
        }
    } else {
        // Copy first line part
        for (int i = sx; i < len_sy; i++) {
            text[pos++] = buf->lines[sy]->chars[i];
        }
        text[pos++] = '\n';
        
        // Copy middle lines
        for (int i = sy + 1; i < ey; i++) {
            int line_len = buf->lines[i]->length;
            for (int j = 0; j < line_len; j++) {
                text[pos++] = buf->lines[i]->chars[j];
            }
            text[pos++] = '\n';
        }
        
        // Copy last line part
        for (int i = 0; i < ex; i++) {
            text[pos++] = buf->lines[ey]->chars[i];
        }
    }
    
    text[pos] = '\0';
    return text;
}

void buffer_set_mark(Buffer *buf) {
    buf->mark_x = buf->cx;
    buf->mark_y = buf->cy;
    buf->mark_active = 1;
}

void buffer_kill_region(Buffer *buf) {
    buffer_save_history(buf);
    if (!buf->mark_active) return;
    
    int sy, ey, sx, ex;
    if (buf->cy < buf->mark_y) {
        sy = buf->cy; ey = buf->mark_y;
        sx = buf->cx; ex = buf->mark_x;
    } else if (buf->cy > buf->mark_y) {
        sy = buf->mark_y; ey = buf->cy;
        sx = buf->mark_x; ex = buf->cx;
    } else {
        sy = ey = buf->cy;
        if (buf->cx < buf->mark_x) {
            sx = buf->cx; ex = buf->mark_x;
        } else {
            sx = buf->mark_x; ex = buf->cx;
        }
    }
    
    // CLAMP Y coordinates to prevent reading garbage memory!
    if (sy < 0) sy = 0;
    if (ey < 0) ey = 0;
    if (sy >= buf->num_lines) sy = buf->num_lines - 1;
    if (ey >= buf->num_lines) ey = buf->num_lines - 1;
    
    // CLAMP X coordinates
    if (sx > buf->lines[sy]->length) sx = buf->lines[sy]->length;
    if (ex > buf->lines[ey]->length) ex = buf->lines[ey]->length;
    
    if (kill_ring) free(kill_ring);
    kill_ring = buffer_extract_region(buf, sy, sx, ey, ex);
    
    // Delete the region
    if (sy == ey) {
        Line *line = buf->lines[sy];
        memmove(&line->chars[sx], &line->chars[ex], line->length - ex);
        line->length -= (ex - sx);
        line->chars[line->length] = '\0';
    } else {
        Line *start_line = buf->lines[sy];
        Line *end_line = buf->lines[ey];
        int remaining = end_line->length - ex;
        if (remaining > 0) {
            if (start_line->length + remaining >= start_line->capacity) {
                start_line->capacity = start_line->length + remaining + 1;
                start_line->chars = realloc(start_line->chars, 
                				  start_line->capacity);
            }
            memcpy(&start_line->chars[sx], &end_line->chars[ex], remaining);
            start_line->length = sx + remaining;
            start_line->chars[start_line->length] = '\0';
        } else {
            start_line->length = sx;
            start_line->chars[start_line->length] = '\0';
        }
        for (int i = sy + 1; i <= ey; i++) {
            free(buf->lines[i]->chars);
            free(buf->lines[i]);
        }
        memmove(&buf->lines[sy + 1], 
        	&buf->lines[ey + 1], 
        	sizeof(Line *) * (buf->num_lines - ey - 1));
        buf->num_lines -= (ey - sy);
    }
    
    buf->cx = sx;
    buf->cy = sy;
    buf->mark_active = 0;
}

void buffer_copy_region(Buffer *buf) {
    if (!buf->mark_active) return;
    
    int sy, ey, sx, ex;
    if (buf->cy < buf->mark_y) {
        sy = buf->cy; ey = buf->mark_y;
        sx = buf->cx; ex = buf->mark_x;
    } else if (buf->cy > buf->mark_y) {
        sy = buf->mark_y; ey = buf->cy;
        sx = buf->mark_x; ex = buf->cx;
    } else {
        sy = ey = buf->cy;
        if (buf->cx < buf->mark_x) {
            sx = buf->cx; ex = buf->mark_x;
        } else {
            sx = buf->mark_x; ex = buf->cx;
        }
    }
    
    // CLAMP Y coordinates to prevent reading garbage memory!
    if (sy < 0) sy = 0;
    if (ey < 0) ey = 0;
    if (sy >= buf->num_lines) sy = buf->num_lines - 1;
    if (ey >= buf->num_lines) ey = buf->num_lines - 1;
    
    // CLAMP X coordinates
    if (sx > buf->lines[sy]->length) sx = buf->lines[sy]->length;
    if (ex > buf->lines[ey]->length) ex = buf->lines[ey]->length;
    
    if (kill_ring) free(kill_ring);
    kill_ring = buffer_extract_region(buf, sy, sx, ey, ex);
    buf->mark_active = 0;
}

void buffer_exchange_point_mark(Buffer *buf) {
    if (!buf->mark_active) return;
    int temp_x = buf->cx;
    int temp_y = buf->cy;
    buf->cx = buf->mark_x;
    buf->cy = buf->mark_y;
    buf->mark_x = temp_x;
    buf->mark_y = temp_y;
}

void buffer_undo(Buffer *buf) {
    if (undo_count == 0) return; // Nothing to undo
    
    // Pop the last state off the stack
    Buffer *prev = undo_stack[--undo_count];
    
    // Restore the lines
    for (int i = 0; i < buf->num_lines; i++) {
        free(buf->lines[i]->chars);
        free(buf->lines[i]);
    }
    free(buf->lines);
    
    buf->num_lines = prev->num_lines;
    buf->lines = prev->lines; // Steal the lines array from the copy
    
    // Restore cursor
    buf->cx = prev->cx;
    buf->cy = prev->cy;
    
    // Free the snapshot struct (but not the lines, we stole them!)
    if (prev->filename) free(prev->filename);
    free(prev);
    
    // Make sure cursor isn't out of bounds after undo
    if (buf->cy >= buf->num_lines) buf->cy = buf->num_lines - 1;
    if (buf->cx > buf->lines[buf->cy]->length){
    	buf->cx = buf->lines[buf->cy]->length;
    }
}
void buffer_move_word_forward(Buffer *buf) {
    Line *line = buf->lines[buf->cy];
    
    // 1. Skip current word
    while (buf->cx < line->length 
    	   && !isspace((unsigned char)line->chars[buf->cx])) {
        buf->cx++;
    }
    // 2. Skip spaces until next word
    while (buf->cx < line->length && isspace((unsigned char)line->chars[buf->cx])) {
        buf->cx++;
    }
    
    // 3. If we reached the end of the line, wrap to the next line (like C-f does)
    if (buf->cx == line->length && buf->cy < buf->num_lines - 1) {
        buf->cy++;
        buf->cx = 0;
    }
}

void buffer_move_word_backward(Buffer *buf) {
    Line *line = buf->lines[buf->cy];
    
    // 1. If at the beginning of a line, wrap to previous line (like C-b does)
    if (buf->cx == 0 && buf->cy > 0) {
        buf->cy--;
        buf->cx = buf->lines[buf->cy]->length;
        return; // Stop here so next press starts on the previous line
    }
    
    // 2. Step back one character to enter the previous word/space
    if (buf->cx > 0) buf->cx--;
    
    // 3. Skip spaces backwards
    while (buf->cx > 0 && isspace((unsigned char)line->chars[buf->cx])) {
        buf->cx--;
    }
    
    // 4. Skip word backwards until we hit a space or the beginning
    while (buf->cx > 0 && !isspace((unsigned char)line->chars[buf->cx - 1])) {
        buf->cx--;
    }
}
