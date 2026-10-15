#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "term.h"
#include "buffer.h"
#include "display.h"
#include <string.h>
#include <dirent.h>
#include <signal.h>

#define MAX_BUFFER 20
Buffer *all_buffers[MAX_BUFFER];
int num_buffers = 0;

void tab_complete_file(char *buf, int *len) {
    // Static variables remember their values between Tab presses!
    static char matches[100][256];
    static int match_count = 0;
    static int cycle_idx = -1;

    // 1. Check if the user is pressing Tab again on a completed match
    int is_cycling = 0;
    if (*len > 0) {
        for (int i = 0; i < match_count; i++) {
            if (strcmp(buf, matches[i]) == 0) {
                is_cycling = 1;
                // Advance to the next match in the list (wrap around if at the end)
                cycle_idx = (i + 1) % match_count;
                break;
            }
        }
    }

    // 2. If we are not cycling, we need to recalculate the matches
    if (!is_cycling) {
        cycle_idx = -1;

        // Expand ~ to home directory, ensuring a trailing slash for correct prefix splitting
        char full_path[512];
        if (buf[0] == '~') {
            char *home = getenv("HOME");
            if (home) {
                snprintf(full_path, sizeof(full_path), "%s/%s", home, buf + 1);
            } else {
                strncpy(full_path, buf, sizeof(full_path));
            }
        } else {
            strncpy(full_path, buf, sizeof(full_path));
        }

        // Split into directory and prefix
        char dir[512] = "";
        char prefix[256] = "";
        char *last_slash = strrchr(full_path, '/');

        if (last_slash) {
            int dir_len = last_slash - full_path + 1;
            strncpy(dir, full_path, dir_len);
            dir[dir_len] = '\0';
            strcpy(prefix, last_slash + 1);
        } else {
            strcpy(prefix, full_path);
        }

        // Capture user_typed_dir (to preserve ~ and relative paths in the prompt)
        char user_dir[512] = "";
        char *user_last_slash = strrchr(buf, '/');
        if (user_last_slash) {
            int user_dir_len = user_last_slash - buf + 1;
            strncpy(user_dir, buf, user_dir_len);
            user_dir[user_dir_len] = '\0';
        } else if (buf[0] == '~') {
            strcpy(user_dir, "~/");
        }

        DIR *d = opendir(strlen(dir) > 0 ? dir : ".");
        if (!d) return;

        struct dirent *entry;
        match_count = 0;

        while ((entry = readdir(d)) != NULL && match_count < 100) {
            if (strncmp(entry->d_name, prefix, strlen(prefix)) == 0) {
                int is_dir = (entry->d_type == DT_DIR);
                if (is_dir) {
                    snprintf(matches[match_count], 256, 
                    	     "%s%s/", user_dir, entry->d_name);
                } else {
                    snprintf(matches[match_count], 256, 
                    	     "%s%s", user_dir, entry->d_name);
                }
                match_count++;
            }
        }
        closedir(d);

        if (match_count == 0) return;

        // Start at the first match
        cycle_idx = 0;
    }

    // 3. Apply the selected match to the prompt buffer
    if (match_count > 0 && cycle_idx >= 0) {
        strcpy(buf, matches[cycle_idx]);
        *len = strlen(buf);
    }
}

char *expand_path(const char *path){
    if(path[0] == '~'){
        char *home = getenv("HOME");
        if(home){
            char *full_path = malloc(strlen(home) + strlen(path));
            sprintf(full_path, "%s%s", home, path + 1);
            return full_path;
        }
    }
    char *copy = malloc(strlen(path) + 1);
    strcpy(copy, path);
    return copy;
}

// The list of commands supported by M-x
char *editor_commands[] = {
    "save-buffer", "find-file", "goto-line", 
    "kill-line", "yank", "quit", "shell-command"
};
int num_commands = 7;

void tab_complete_command(char *buf, int *len) {
    static char matches[20][64];
    static int match_count = 0;
    static int cycle_idx = -1;

    // 1. Check if we are already cycling
    int is_cycling = 0;
    for (int i = 0; i < match_count; i++) {
        if (strcmp(buf, matches[i]) == 0) {
            is_cycling = 1;
            cycle_idx = (i + 1) % match_count;
            break;
        }
    }

    // 2. If not cycling, search the command list
    if (!is_cycling) {
        cycle_idx = -1;
        match_count = 0;
        
        for (int i = 0; i < num_commands && match_count < 20; i++) {
            if (strncmp(editor_commands[i], buf, *len) == 0) {
                strcpy(matches[match_count], editor_commands[i]);
                match_count++;
            }
        }
        if (match_count == 0) return;
        cycle_idx = 0;
    }

    // 3. Apply the match to the prompt buffer
    if (match_count > 0 && cycle_idx >= 0) {
        strcpy(buf, matches[cycle_idx]);
        *len = strlen(buf);
    }
}

void tab_complete_buffer(char *buf, int *len){
	static int cycle_idx = -1;
	for(int i = 0; i < num_buffers; i++){
		if(all_buffers[i] && strcmp(buf, all_buffers[i]->filename) == 0){
			cycle_idx = (i + 1) % num_buffers;
			while(!all_buffers[cycle_idx]){
				cycle_idx = (cycle_idx + 1) % num_buffers;
			}
			strcpy(buf, all_buffers[cycle_idx]->filename);
			*len = strlen(buf);
			return;
		}
	}
	cycle_idx = -1;
	for(int i = 0; i < num_buffers; i++){
		if(all_buffers[i] 
		   && strncmp(all_buffers[i]->filename, buf, *len) == 0){
		   	cycle_idx = i;
		   	break;
		}
	}
	if(cycle_idx != -1){
		strcpy(buf, all_buffers[cycle_idx]->filename);
		*len = strlen(buf);
	}
}

char *editor_prompt(const char *promt_msg, int tab_enabled){
    char *buf = malloc(256);
    int len = 0;
    buf[0] = '\0';

    while (1) {
        int rows, cols;
        term_get_size(&rows, &cols);
        term_cursor_pos(0, rows -1);
        term_clear_line();

        char temp[512];
        int tlen = snprintf(temp, sizeof(temp), "%s%s", promt_msg, buf);
        write(STDOUT_FILENO, temp, tlen);

        int c = term_read_key();
        if(c == 13){
            return buf;
        }else if (c == 17 || c ==27 || c==7) {
            free(buf);
            return NULL;
        }else if (c == 127 || c == 8) {
            if(len > 0){ len --; buf[len] = '\0';}
        }else if (c == 9 && tab_enabled == 1) {
            tab_complete_file(buf, &len);
        }else if (c == 9 && tab_enabled == 2) {
            tab_complete_command(buf, &len);
        }else if (c == 9 && tab_enabled == 3){
            tab_complete_buffer(buf, &len);
        }else if (c > 32 && c <= 126) {
            buf[len++] = c;
            buf[len] = '\0';
        }
    }
}

int show_message(const char *msg){
    int rows, cols;
    term_get_size(&rows, &cols);
    term_cursor_pos(0, rows - 1);
    term_clear_line();
    
    write(STDOUT_FILENO, msg, strlen(msg));
    return term_read_key();
}

void flash_message(const char *msg){
    int rows, cols;
    term_get_size(&rows, &cols);
    term_cursor_pos(0, rows - 1);
    term_clear_line();
    write(STDOUT_FILENO, msg, strlen(msg));
}

int confirm_discard(){
    int rows, cols;
    term_get_size(&rows, &cols);
    term_cursor_pos(0, rows -1);
    term_clear_line();
    write(STDOUT_FILENO, "Dscard unsaved changes? (y or n)", 33);
    while(1){
        int c = term_read_key();
        if(c == 'y') return 1;
        if(c == 'n'|| c == 27 || c == 7) return 0;
    }
}

void editor_open_file(Buffer **buf_ptr) {
    char *fn = editor_prompt("Open file: ", 1);
    if (!fn) return;
    
    char *path = expand_path(fn);
    
    Buffer *new_buf = buffer_create();
    buffer_open(new_buf, path);
    
    all_buffers[num_buffers++] = new_buf;
    
    // 1. Check if the current buffer is shared by another window
    int w_count;
    Window *wins = display_get_windows(&w_count);
    int aw = display_get_active();
    wins[aw].buf = new_buf;
    
    //int shared = 0;
    //for (int i = 0; i < w_count; i++) {
    //    if (i != aw && wins[i].buf == *buf_ptr) shared = 1;
    //}
    *buf_ptr = new_buf;
    
    // 2. Create a brand new buffer for the new file
    //Buffer *old_buf = *buf_ptr;
    //*buf_ptr = buffer_create();
    //buffer_open(*buf_ptr, path);
    
    // 3. Point the current window to the new buffer
    //wins[aw].buf = *buf_ptr;
    
    // 4. If the old buffer was not shared, destroy it to prevent memory leaks!
    //if (!shared) buffer_free(old_buf);
    
    free(path);
    free(fn);
    display_reset_scroll();
}

void handle_shell_command() {
    char *cmd = editor_prompt("Shell command: ", 0);
    if (!cmd) return; // User cancelled

    // Run the command and open a pipe to read its output
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        show_message("Error: Failed to run command");
        free(cmd);
        return;
    }

    // Read the output into a buffer
    char output[4096];
    size_t nread = fread(output, 1, sizeof(output) - 1, fp);
    output[nread] = '\0';
    pclose(fp);
    free(cmd);

    // Display the output on the bottom line
    int rows, cols;
    term_get_size(&rows, &cols);
    term_cursor_pos(0, rows - 1);
    term_clear_line();

    // If the output is huge, only print what fits on one line for now
    // (We replace newlines with spaces so it doesn't mess up the screen)
    for (int i = 0; i < nread; i++) {
        if (output[i] == '\n') output[i] = ' ';
    }
    
    int len = strlen(output);
    if (len > cols) len = cols;
    write(STDOUT_FILENO, output, len);

    // Wait for any keypress before returning to the editor
    term_read_key();
}

void handle_switch_buffer(Buffer **buf_ptr){
	char *name = editor_prompt("Switch to buffer: ", 3);
	if(!name) return;
	
	for(int i = 0; i < num_buffers; i++){
		if(all_buffers[i] && strcmp(all_buffers[i]->filename, name) == 0){
			int w_count; Window *wins = display_get_windows(&w_count);
			int aw = display_get_active();
			wins[aw].buf = all_buffers[i];
			*buf_ptr = all_buffers[i];
			display_reset_scroll();
			free(name);
			return;
		}
	}
	show_message("Buffer not found.");
	free(name);
}

void handle_kill_buffer(Buffer **buf_ptr){
    char *name = editor_prompt("kill buffer (default: current): ", 3);
    if(!name) return;
    if(strlen(name) == 0){
        free(name);
        name = strdup((*buf_ptr)->filename);
    }
    
    int killed_active = 0;
    for(int i = 0; i < num_buffers; i++){
        if(all_buffers[i] && strcmp(all_buffers[i]->filename, name) == 0){
            if(all_buffers[i]->dirty){
                if(!confirm_discard()){ free(name); return; }
            }
            int w_count;
            Window *wins = display_get_windows(&w_count);
            int aw = display_get_active();
            if(wins[aw].buf == all_buffers[i]) killed_active = 1;
            
            buffer_free(all_buffers[i]);
            all_buffers[i] = NULL;
            
            if(killed_active){
                for(int j = 0; j < num_buffers; j++){
                    if(all_buffers[j]){
                    	wins[aw].buf = all_buffers[j];
                    	*buf_ptr = all_buffers[j];
                    	display_reset_scroll();
                    	break;
                    }
                }
            }
            free(name);
            return;
        }
    }
    show_message("Bufer not found.");
    free(name);
}

Buffer *handle_ctrl_x(Buffer *buf, int *running) {
    int next_key = term_read_key();
    if (next_key == 7) { flash_message("Quit"); return buf; }

    if (next_key == 19) { // Ctrl-S (Save)
        // ... (keep your save logic exactly the same)
        
        if(!buf->dirty){
            flash_message("No changes need to be saved.");
            return buf;
        }
        
        if(!buf->filename){
            char *fn = editor_prompt("Save as: ", 1);
            if(fn){
                buf->filename = expand_path(fn);
                free(fn);
            }else {
                return buf;
            }
        }
        
        int lines_saved = buffer_save(buf);
        if(lines_saved >= 0){
            char msg[256];
            snprintf(msg, sizeof(msg), 
            	     "Wrote %s (%d lines)", buf->filename, lines_saved);
            int msg_key = show_message(msg);
            if(msg_key = 24){
                int cmd_key = term_read_key();
                if(cmd_key == 3) *running = 0;
            }
        }else {
            show_message("Error saving file!");
        }
    } 
    else if (next_key == 6) { // Ctrl-F (Open)
        if (buf->dirty) {
            if (!confirm_discard()) return buf;
        }
        // Call our new multi-buffer open function!
        // We pass &buf because we need to update the pointer in main.c
        editor_open_file(&buf); 
    } 
    else if (next_key == '3') { // C-x 3 (Split)
        buf = display_split(1); // Update main.c's buf pointer to the new window's buffer
    }
    else if (next_key == '2') buf = display_split(2);
    else if (next_key == '1') display_unsplit();
    else if (next_key == 'o') { // C-x o (Other window)
        buf = display_next_window(); // Update buf to the other window's buffer!
    }
    else if (next_key == 'b') handle_switch_buffer(&buf);
    else if (next_key == 'k') handle_kill_buffer(&buf);
    else if (next_key == 24) buffer_exchange_point_mark(buf);
    else if (next_key == 3) { // C-x C-c (Quit)
        if (!buf->dirty || confirm_discard()) *running = 0;
    }
    return buf;
}

void handle_meta_x(Buffer *buf, int *running){
    char *cmd = editor_prompt("M-x ", 2);
    if(!cmd) return;

    if(strcmp(cmd, "save-buffer") == 0){
        handle_ctrl_x(buf, running);
        if(!buf->filename){
            char *fn = editor_prompt("Save as: ", 1);
            if(fn){ buf->filename = expand_path(fn); free(fn);}
            else { free(cmd); return;}
        }
        
        if(!buf->dirty){
            flash_message("No changes need to be saved.");
            free(cmd);
            return;
        }
        
        int lines_saved = buffer_save(buf);
        if(lines_saved >= 0){
            char msg[256];
            snprintf(msg, sizeof(msg), "Wrote %s (%d lines)",
                     buf->filename, lines_saved);
            int msg_key = show_message(msg);
            if(msg_key = 24){
                int cmd_key = term_read_key();
                if(cmd_key == 3) *running = 0;
            }
            show_message(msg);
        }else{
            show_message("Error saving file!");
        }
    }
    else if (strcmp(cmd, "find-file") == 0) {
        if(buf->dirty){
            if(!confirm_discard()){ free(cmd); return; }
        }
        char *fn = editor_prompt("Open file: ", 1);
        if(fn){
            char *path = expand_path(fn);
            buffer_open(buf, path);
            free(path); free(fn);
            display_reset_scroll();
        }
    }
    else if (strcmp(cmd, "quit") == 0) {
        if(!buf->dirty || confirm_discard()) *running = 0;
    }
    else if (strcmp(cmd, "shell-command") ==  0) {
        handle_shell_command();
    }
    else if (strcmp(cmd, "goto-line") == 0) {
        char *line_str = editor_prompt("Goto line: ", 0);
        if(line_str){
            int line_num = atoi(line_str);
            if(line_num > 0 && line_num <= buf->num_lines){
                buf->cy = line_num - 1; buf->cx = 0;
                display_reset_scroll();
            }
            free(line_str);
        }
    }
    else if(strcmp(cmd, "kill-line") == 0) buffer_kill_line(buf);
    else if(strcmp(cmd, "yank") == 0) buffer_yank(buf);
    free(cmd);
}

// Global variable to remember the last search
char last_search[256] = {0};

void handle_inc_search(Buffer *buf) {
    char query[256] = {0};
    int qlen = 0;
    
    int saved_cx = buf->cx;
    int saved_cy = buf->cy;

    while (1) {
        int rows, cols;
        term_get_size(&rows, &cols);
        
        display_draw(buf);
        term_cursor_pos(0, rows - 1);
        term_clear_line();
        
        char prompt_msg[512];
        int plen = snprintf(prompt_msg, sizeof(prompt_msg), "I-search: %s", query);
        write(STDOUT_FILENO, prompt_msg, plen);
        
        int c = term_read_key();

        if (c == 13) { // Enter: Confirm search
            strcpy(last_search, query); // Save the query for next time!
            return; 
        }
        
        if (c == 7 || c == 27) { // Ctrl-G or ESC: Cancel
            buf->cx = saved_cx;
            buf->cy = saved_cy;
            return; 
        }
        
        if (c == 19) { // Ctrl-s pressed INSIDE the search!
            if (qlen == 0 && strlen(last_search) > 0) {
                // If prompt is empty, load the last search!
                strcpy(query, last_search);
                qlen = strlen(query);
            }

            if (qlen > 0) {
                // If already searching, search again from the current match!
                saved_cx = buf->cx + 1;
                saved_cy = buf->cy;
            }
            // Skip the normal character insertion below and go straight to searching
            goto perform_search;
        }
        
        if (c == 127 || c == 8) { // Backspace
            if (qlen > 0) {
                qlen--;
                query[qlen] = '\0';
            }
            continue;
        }
        
        if (c >= 32 && c <= 126) { // Normal character
            query[qlen++] = c;
            query[qlen] = '\0';
        }

perform_search:
        // Perform the search
        if (qlen > 0) {
            int mx, my;
            if (buffer_search(buf, query, saved_cx, saved_cy, &mx, &my)) {
                buf->cx = mx;
                buf->cy = my;
            }
        } else {
            buf->cx = saved_cx;
            buf->cy = saved_cy;
        }
    }
}

void handle_inc_qreplace(Buffer *buf) {
    char *search_str = editor_prompt("Query replace: ", 0);
    if (!search_str) return;

    char *replace_str = editor_prompt("Replace with: ", 0);
    if (!replace_str) { 
        free(search_str); 
        return; 
    }

    int search_cx = buf->cx;
    int search_cy = buf->cy;

    while (1) {
        int mx, my;
        if (!buffer_search(buf, search_str, search_cx, search_cy, &mx, &my)) {
            goto cleanup; // Done! Go free memory.
        }
        
        buf->cx = mx;
        buf->cy = my;
        search_cx = mx + 1;
        search_cy = my;
        
        display_draw(buf);
        
        int rows, cols;
        term_get_size(&rows, &cols);
        term_cursor_pos(0, rows - 1);
        term_clear_line();
        
        char msg[256];
        int mlen = snprintf(msg, sizeof(msg), 
        		    "Query replace %s with %s? (y/n/!): ", 
        		    search_str, replace_str);
        write(STDOUT_FILENO, msg, mlen);

        int c = term_read_key();

        if (c == 'y') {
            buffer_replace_at_cursor(buf, strlen(search_str), replace_str);
            search_cx = buf->cx;
        } else if (c == 'n') {
            // Do nothing, loop will find the next match
        } else if (c == '!') {
            int replace_count = 0;
            while (1) {
                buffer_replace_at_cursor(buf, strlen(search_str), replace_str);
                replace_count++;
                search_cx = buf->cx;
                
                if (!buffer_search(buf, search_str, 
                    search_cx, search_cy, &mx, &my)) break; // Exit inner loop
                
                buf->cx = mx; buf->cy = my;
                search_cx = mx + 1; search_cy = my;
            }
            
            // Show the completion message!
            display_draw(buf);
            int rows2, cols2;
            term_get_size(&rows2, &cols2);
            term_cursor_pos(0, rows2 - 1);
            term_clear_line();
            char done_msg[256];
            int dlen = snprintf(done_msg, sizeof(done_msg), 
            		     "Replaced %d occurrences.", replace_count);
            write(STDOUT_FILENO, done_msg, dlen);
            term_read_key(); // Wait for keypress
            
            goto cleanup; // Done! Go free memory.
        } else if (c == 27 || c == 7) {
            goto cleanup; // Cancel! Go free memory.
        }
    }

cleanup:
    // This guarantees we never leak memory, no matter how the function exits
    free(search_str);
    free(replace_str);
}

void resume_editor(int sig){
    term_init();
    term_enter_alt_screen();
    term_clear_screen();
}

int main(int argc, char *argv[]) {
    term_init();
    term_enter_alt_screen();
    term_clear_screen();
    
    Buffer *buf = buffer_create();
    buf->filename = strdup("*scratch*");
    all_buffers[0] = buf;
    num_buffers = 1;
    
    display_init(buf);

    if (argc >= 2) {
        buffer_open(buf, argv[1]);
        display_reset_scroll();
    }

    int running = 1; // Use a running flag instead of goto

    signal(SIGCONT, resume_editor);
    while (running) {
        display_draw(buf);
        int key = term_read_key();

        switch (key) {
            case 7: { // Ctrl-G
                if (buf->mark_active) { buf->mark_active = 0; flash_message("Quit"); }
                break;
            }
            case 24: buf = handle_ctrl_x(buf, &running); break;  // DELEGATE!
            case META_X: handle_meta_x(buf, &running); break; // DELEGATE!
            case 19: handle_inc_search(buf); break; // DELEGATE!
            case META_PERCENT: handle_inc_qreplace(buf); break; // DELEGATE!

            // --- Emacs Movement ---
            case 1:  buffer_move_cursor_home(buf); break;
            case 5:  buffer_move_cursor_end(buf); break;
            case 16: buffer_move_cursor(buf, 'U'); break;
            case 14: buffer_move_cursor(buf, 'D'); break;
            case 2:  buffer_move_cursor(buf, 'L'); break;
            case 6:  buffer_move_cursor(buf, 'R'); break;
            case 9:  buffer_insert_char(buf, '\t'); break;
            
            // --- Emacs Kill Ring ---
            case 11: buffer_kill_line(buf); break;
            case 23: buffer_kill_region(buf); break;
            case META_W: buffer_copy_region(buf); break;
            case META_BANG: handle_shell_command(); break;
            case META_F: buffer_move_word_forward(buf); break;
            case META_B: buffer_move_word_backward(buf); break;
            case 25: buffer_yank(buf); break;
            case 31: buffer_undo(buf); break;
            case 0:  buffer_set_mark(buf); break;

            // --- Arrow Keys ---
            case ARROW_UP:    buffer_move_cursor(buf, 'U'); break;
            case ARROW_DOWN:  buffer_move_cursor(buf, 'D'); break;
            case ARROW_LEFT:  buffer_move_cursor(buf, 'L'); break;
            case ARROW_RIGHT: buffer_move_cursor(buf, 'R'); break;
            
            case 127: case 8: buffer_delete_char(buf); break;
            case 4:  buffer_delete_forward(buf); break;
            case 13: buffer_insert_newline(buf, 1); break;
            case 17: { // Ctrl-Q (Quoted insert)
                int literal_key = term_read_key();
                if (literal_key == 9 || (literal_key >= 32 && literal_key <= 126))
                    buffer_insert_char(buf, literal_key);
                break;
            }
            case 26:
                {
                    term_exit_alt_screen();
                    term_restore();
                    printf("\n[1]+ Stoped  udit\n");
                    fflush(stdout);
                    raise(SIGTSTP);
                    break;
                }
            case 22: {
            	int rows, cols;
            	term_get_size(&rows, &cols);
            	buf->cy += (rows - 2);
            	if(buf->cy >= buf->num_lines) buf->cy = buf->num_lines - 1;
            	if(buf->cx > buf->lines[buf->cy]->length){
            	    buf->cx = buf->lines[buf->cy]->length;
            	}
            	break;
            }
            case META_V: {
            	int rows, cols;
            	term_get_size(&rows, &cols);
            	buf->cy -= (rows - 2);
            	if(buf->cy < 0) buf->cy = 0;
            	if(buf->cx > buf->lines[buf->cy]->length){
            	    buf->cx = buf->lines[buf->cy]->length;
            	}
            	break;
            }
            default:
                if (key >= 32 && key <= 126) buffer_insert_char(buf, key);
                break;
        }
    }

    buffer_free(buf);
    term_clear_screen();
    term_exit_alt_screen();
    return 0;
}
