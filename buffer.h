#ifndef BUFFER_H
#define BUFFER_H

// Represents a single line of text
typedef struct {
    char *chars;    // The actual string data
    int length;      // How many characters are in this line
    int capacity;    // How much memory is allocated
} Line;

// Represents the entire file being edited
typedef struct {
    Line **lines;    // Array of pointers to Lines
    int num_lines;   // How many lines exist
    int cx, cy;      // Cursor X (column) and Y (row)
    char *filename;
    int mark_x, mark_y;
    int mark_active;
    int dirty;
} Buffer;

Buffer *buffer_create();
void buffer_free(Buffer *buf);
void buffer_insert_char(Buffer *buf, char c);
void buffer_move_cursor(Buffer *buf, char direction);
void buffer_delete_char(Buffer *buf);
void buffer_insert_newline(Buffer *buf, int auto_indent);
void buffer_move_cursor_home(Buffer *buf);
void buffer_move_cursor_end(Buffer *buf);
void buffer_kill_line(Buffer *buf);
void buffer_yank(Buffer *buf);
void buffer_delete_forward(Buffer *buf);
int buffer_save(Buffer *buf);
void buffer_open(Buffer *buf, const char *filename);
int buffer_search(Buffer *buf, const char *query,
                  int start_cx, int start_cy, int *match_cx, int *match_cy);
void buffer_replace_at_cursor(Buffer *buf, int match_len,
                              const char *replace_str);
void buffer_set_mark(Buffer *buf);
void buffer_kill_region(Buffer *buf);
void buffer_copy_region(Buffer *buf);
void buffer_exchange_point_mark(Buffer *buf);
void buffer_save_history(Buffer *buf);
void buffer_undo(Buffer *buf);
void buffer_move_word_forward(Buffer *buf);
void buffer_move_word_backward(Buffer *buf);

#endif
