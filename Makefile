TARGET = udit

CC = gcc

CFLAGS = -Wall -Wextra -g

SRC = main.c term.c buffer.c display.c wordset.c

OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

run: $(TARGET)
	./$(TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) /usr/bin/$(TARGET)
	
uninstall:
	rm -f /usr/bin/$(TARGET)