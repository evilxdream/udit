udit

A minimalist, terminal based text editor inspired by uemacs.

LAUNCHING

COMMAND			ACTION

udit			open editor with *scratch* buffer
myemacs <filename>	Open the editor and load a specific file

MOVEMENT

KEY			ACTION

C-p / UP		Move cursor up one line
C-n / DOWN		Move cursor down one line
C-b / LEFT		Move cursor left one character
C-f / RIGHT		Move cursor right one character
C-a			Move cursor to the beginning of the line
C-e			Move cursor to the end of the line
M-f			Move cursor forward to the end of the next word
M-b			Move cursor backward to the start of the previous word
C-v			Page Down (move down by one screen)
M-v			Page Up (move up by one screen)

EDITING

KEY			ACTION

Enter			Insert a newline (with smart auto-indentation)
Tab			Insert a real Unix Tab character (\t)
Backspace / C-h		Delete the character before the cursor
C-d			Delete the character under the cursor (forward delete)
C-q + Key		Quoted insert (insert a literal Tab or any key literally)
C-/			Undo

KILL RING

KEY			ACTION

C-SPC / Ctrl-2		Set the mark (begin text selection/highlighting)
C-w			Kill (Cut) the selected region
M-w			Copy the selected region (deactivates mark)
C-k			Kill (Cut) from the cursor to the end of the line
C-y			Yank (Paste) the last killed/copied text
C-x C-x			Exchange the cursor and the mark (jump to the other end of selection)

SEARCH & REPLACE

KEY			ACTION

C-s			Incremental Search forward. (Press C-s again to repeat last search)
M-%			Query Replace. Prompts for search string, then replacement

FILES & BUFFERS

KEY			ACTION

C-x C-f			Find/Open a file (Supports ~ and Tab autocomplete)
C-x C-s			Save the current buffer
C-x b			Switch to another open buffer (Tab to cycle)
C-x k			Kill (close) a buffer

WINDOW MANAGEMENT

KEY			ACTION

C-x 2			Split window horizontally (top and bottom)
C-x 3			Split window vertically (side-by-side)
C-x 1			Unsplit (close all windows except the active one)
C-x o			Move cursor to the "other" window

SYSTEM & SHELL

KEY			ACTION

M-!			Run a shell command (e.g., ls -la or gcc main.c)
C-z			Suspend the editor to the shell (type fg to resume)
C-x C-c			Quit the editor (prompts to save unsaved changes)
C-g			Universal abort/Cancel (cancels commands, clears mark, prints "Quit")

COMMAND PROMPT

M-x			Enter the command prompt (Tab to cycle autocomplete).

Available M-x Commands:
save-buffer (Saves the current file)
find-file (Opens a file)
goto-line (Prompts for a line number to jump to)
kill-line (Cuts to end of line)
yank (Pastes text)
shell-command (Runs a bash command)
quit (Exits the editor