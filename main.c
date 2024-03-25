#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include <string.h>
#include <stdbool.h>

#define CTRL_KEY(k) ((k) & 0x1f)

#define SPACE 32
#define ENTER 13

#define INITIAL_CAPACITY 1024

int max(int a, int b) {
    return a > b ? a : b;
}

typedef struct {
    char **lines; 
    int num_lines; 
    int capacity;
} TextBuffer;

TextBuffer *createTextBuffer() {
    TextBuffer *buffer = malloc(sizeof(TextBuffer));
    buffer->lines = malloc(INITIAL_CAPACITY * sizeof(char*));
    for (int i = 0; i < INITIAL_CAPACITY; i++) {
        buffer->lines[i] = malloc(INITIAL_CAPACITY * sizeof(char));
        buffer->lines[i][0] = '\0'; 
    }
    buffer->num_lines = 1; 
    buffer->capacity = INITIAL_CAPACITY;
    return buffer;
}


void freeTextBuffer(TextBuffer *buffer) {
    for (int i = 0; i < buffer->capacity; i++) {
        free(buffer->lines[i]);
    }
    free(buffer->lines);
    free(buffer);
}

void insertCharacter(TextBuffer *buffer, int y, int x, char c) {
    if (y >= buffer->num_lines) return; 
    int len = strlen(buffer->lines[y]);
    if (x > len) x = len; 
    
    for (int i = len; i >= x; i--) {
        buffer->lines[y][i+1] = buffer->lines[y][i];
    }
    buffer->lines[y][x] = c;

    buffer->lines[y][len+1] = '\0';
}

void insertLine(TextBuffer *buffer, int y){
    if (buffer->num_lines == buffer->capacity) {
        buffer->capacity *= 2;
        buffer->lines = realloc(buffer->lines, buffer->capacity * sizeof(char*));
        for (int i = buffer->num_lines; i < buffer->capacity; i++) {
            buffer->lines[i] = malloc(INITIAL_CAPACITY * sizeof(char));
            buffer->lines[i][0] = '\0'; 
        }
    }
    for (int i = buffer->num_lines; i > y; i--) {
        strcpy(buffer->lines[i], buffer->lines[i-1]);
    }
    buffer->lines[y][0] = '\0';
    buffer->num_lines++;

}

void redrawWindow(WINDOW *win, TextBuffer *buffer, int scrollOffset) {
    werase(win); 
    int maxy, maxx;
    getmaxyx(win, maxy, maxx);
    for (int i = 0; i < maxy && (i + scrollOffset) < buffer->num_lines; i++) {
        mvwprintw(win, i, 0, "%s",buffer->lines[i + scrollOffset]);
    }
    wrefresh(win);
}

void mergeLines(TextBuffer *buffer, int y) {
    if (y >= buffer->num_lines - 1) return; 
    int len = strlen(buffer->lines[y]);
    strcpy(buffer->lines[y] + len, buffer->lines[y+1]);
    for (int i = y+1; i < buffer->num_lines - 1; i++) {
        strcpy(buffer->lines[i], buffer->lines[i+1]);
    }
    buffer->num_lines--;
}

void removeCharacter(TextBuffer *buffer, int y, int x) {
    if (y >= buffer->num_lines) return; 
    int len = strlen(buffer->lines[y]);
    if (x >= len) return; 
    for (int i = x; i < len; i++) {
        buffer->lines[y][i] = buffer->lines[y][i+1];
    }
}

WINDOW *create_newwin(int height, int width, int starty, int startx){
    WINDOW *local_win;

    local_win = newwin(height, width, starty, startx);
    box(local_win, 0, 0);
    wrefresh(local_win);

    return local_win;
}

void enableRawMode(){
    initscr();
    keypad(stdscr, TRUE);
    cbreak();
    noecho();
    nonl();
}

int main(){
    
    WINDOW *my_win;
    enableRawMode();
    start_color();
    init_pair(1, COLOR_WHITE, COLOR_BLUE);


    int c;
    TextBuffer *buffer = createTextBuffer();
    int x = 0;
    int y = 0;
    int scrollOffset = 0;
    bool editing = true;
    int width, height;
    getmaxyx(stdscr, height, width);

    my_win = create_newwin(height - 2, width - 2, y, x);
    wmove(my_win, y, x);
    wrefresh(my_win);
    while(1){
        c = getch();

        int realY = y + scrollOffset;

        switch (c)
        {
        case ENTER:
            insertLine(buffer, realY + 1);
            if (y >= height - 3) {
                scrollOffset++;
            } else {
                y++;
            }
            x = 0;
            break;
        case SPACE:
            insertCharacter(buffer, realY, x++, ' ');
            break;
        case KEY_BACKSPACE:
            if (x > 0 || realY > 0) {
                if (x == 0) {
                    mergeLines(buffer, realY - 1);
                    y--;
                    x = strlen(buffer->lines[realY - 1]);
                    if (y < 0) {
                        y = 0;
                        if (scrollOffset > 0) scrollOffset--;
                    }
                } else {
                    removeCharacter(buffer, realY, --x);
                }
            }
            break;
        case KEY_UP:
             if (y > 0) {
                y--;
                if (y < scrollOffset) scrollOffset--;
                if (x > strlen(buffer->lines[y - scrollOffset])) {
                    x = strlen(buffer->lines[y - scrollOffset]);
                }
            }
            break;
        case KEY_DOWN:
            if (y < buffer->num_lines - 1) {
                y++;
                if (y - scrollOffset >= height - 2) scrollOffset++;
                if (x > strlen(buffer->lines[y - scrollOffset])) {
                    x = strlen(buffer->lines[y - scrollOffset]);
                }
            }
            break;
        case KEY_LEFT:
            if (x > 0)
                x--;
            break;
        case KEY_RIGHT:
            if (x < width)
                x++;
            break;
        default:
            if (editing && c >= 32 && c <= 126) { 
                insertCharacter(buffer, realY, x, c);
                x++;
                if (x >= width - 2) { 
                    x = 0;
                    if (y >= height - 3) {
                        scrollOffset++;
                    } else {
                        y++;
                    }
                    if (realY + 1 == buffer->num_lines) {
                        insertLine(buffer, buffer->num_lines);
                    }
                }
            }
            break;
        }
        attron(COLOR_PAIR(1));
        mvprintw(height - 1, 0 , "Line: %d, Column: %d", y + scrollOffset + 1, x + 1); // Use realY for line number
        clrtoeol();
        attroff(COLOR_PAIR(1));

        refresh();
        
        redrawWindow(my_win, buffer, scrollOffset);
        wmove(my_win, y, x);
        wrefresh(my_win);


    }
    freeTextBuffer(buffer);
    endwin();
    return 0;
}