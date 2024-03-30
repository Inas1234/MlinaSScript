#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include <string.h>
#include <stdbool.h>
#include <termios.h>
#include <signal.h>
#include <ctype.h>

#define CTRL_KEY(k) ((k) & 0x1f)
#define SPACE 32
#define ENTER 13
#define ESCAPE 27
#define INITIAL_CAPACITY 1024

typedef enum {
    KEYWORD,
    NORMAL,
    STRING,
    COMMENT,
} SyntaxType;


const char * keywords[] = {
    "if", "else", "print", "printf", "cout", "void", "int", "float", "string", "char", "bool"
    "double", "long", "short", "for", "while", "do", "return", "break", "continue", "switch",
    "case", "default", "true", "false", "class", "struct",
};


struct termios orig_termios;

int max(int a, int b) {
    return a > b ? a : b;
}

int min(int a, int b) {
    return a < b ? a : b;
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

void drawLineWithSyntaxHighlighting(WINDOW *win, char *line, int y, int lineNumWidth) {
    char word[256] = {0}; 
    int word_len = 0;
    int x = lineNumWidth; 
    
    bool isWordChar = false;

    for (int i = 0; line[i] != '\0' || isWordChar; ++i) {
        char c = line[i];
        isWordChar = (isalnum(c) || c == '_'); 

        if (isWordChar) {
            word[word_len++] = c;
        }

        if (!isWordChar && word_len > 0) {
            word[word_len] = '\0'; 
            
            bool isKeyword = false;
            for (int k = 0; keywords[k] != NULL; k++) {
                if (strcmp(word, keywords[k]) == 0) {
                    isKeyword = true;
                    break;
                }
            }

            if (isKeyword) {
                wattron(win, COLOR_PAIR(KEYWORD));
            }

            mvwprintw(win, y, x, "%s", word);
            
            if (isKeyword) {
                wattroff(win, COLOR_PAIR(KEYWORD));
            }

            word_len = 0;

            x += strlen(word);
        }

        if (!isWordChar && c != '\0') {
            mvwaddch(win, y, x++, c);
        }
    }
}


void redrawWindow(WINDOW *win, TextBuffer *buffer, int scrollOffset) {
    werase(win); 
    int maxy, maxx;
    getmaxyx(win, maxy, maxx);
    for (int i = 0; i < maxy && (i + scrollOffset) < buffer->num_lines; i++) {
        int lineNum = i + scrollOffset + 1; 
        mvwprintw(win, i, 0, "%4d ", lineNum); 
        // char tempLine[INITIAL_CAPACITY];
        // strncpy(tempLine, buffer->lines[i + scrollOffset], INITIAL_CAPACITY-1);
        // tempLine[INITIAL_CAPACITY-1] = '\0';

        // drawLineWithSyntaxHighlighting(win, tempLine, i, maxx);
        mvwprintw(win, i, 6, "%s", buffer->lines[i + scrollOffset]); 
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

void disableRawMode(){
    endwin();
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {
            perror("tcsetattr");
    }
}

void handleSigInt(int sig) {}


void enableRawMode(){
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);

    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VINTR]    = 0;
    raw.c_cc[VQUIT]    = 0;
    raw.c_cc[VERASE]   = 0;
    raw.c_cc[VKILL]    = 0;
    raw.c_cc[VEOF]     = 0;
    raw.c_cc[VTIME]    = 0;
    raw.c_cc[VMIN]     = 1;
    raw.c_cc[VSWTC]    = 0;
    raw.c_cc[VSTART]   = 0;
    raw.c_cc[VSTOP]    = 0;
    raw.c_cc[VSUSP]    = 0;
    raw.c_cc[VEOL]     = 0;
    raw.c_cc[VREPRINT] = 0;
    raw.c_cc[VDISCARD] = 0;
    raw.c_cc[VWERASE]  = 0;
    raw.c_cc[VLNEXT]   = 0;
    raw.c_cc[VEOL2]    = 0;


    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    //signal(SIGINT, handleSigInt);

    initscr();
    keypad(stdscr, TRUE);
    cbreak();
    noecho();
    nonl();
}


char * readfile(char *filename){
    FILE *file = fopen(filename, "r");
    if (!file) {
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *buffer = malloc(length + 1);
    buffer[length] = '\0';
    fread(buffer, 1, length, file);
    fclose(file);
    return buffer;
}


void saveToFile(TextBuffer *buffer, char *filename) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        return;
    }
    for (int i = 0; i < buffer->num_lines; i++) {
        fprintf(file, "%s\n", buffer->lines[i]);
    }
    fclose(file);
}


void readIntoBuffer(TextBuffer *buffer, char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) return;

    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    int lineIndex = 0;

    while ((read = getline(&line, &len, file)) != -1) {
        if (read > 0 && line[read - 1] == '\n') {
            line[read - 1] = '\0';
            --read;
        }

        if (lineIndex >= buffer->capacity) {
            buffer->capacity *= 2;
            buffer->lines = realloc(buffer->lines, buffer->capacity * sizeof(char*));
            for (int i = lineIndex; i < buffer->capacity; i++) {
                buffer->lines[i] = malloc(INITIAL_CAPACITY * sizeof(char));
                buffer->lines[i][0] = '\0';
            }
        }

        if (lineIndex >= buffer->num_lines) {
            insertLine(buffer, lineIndex);
        }

        if (read > 0) {
            strncpy(buffer->lines[lineIndex], line, INITIAL_CAPACITY - 1);
            buffer->lines[lineIndex][INITIAL_CAPACITY - 1] = '\0'; 
        } else {
            buffer->lines[lineIndex][0] = '\0';
        }

        lineIndex++;
    }

    buffer->num_lines = lineIndex;

    free(line);
    fclose(file);
}



void searchInBuffer(TextBuffer *buffer, char *query, int *x, int *y, int *scrollOffset, int windowHeight) {
    for (int i = 0; i < buffer->num_lines; i++) {
        char *line = buffer->lines[i];
        char *found = strstr(line, query);
        if (found) {
            int lineIndex = i;
            *x = found - line; 
            *y = lineIndex - *scrollOffset; 

            if (*y >= windowHeight - 2) { 
                *scrollOffset += *y - (windowHeight - 2);
                *y = windowHeight - 2;
            } else if (*y < 0) {
                *scrollOffset += *y; 
                *y = 0; 
            }

            return;
        }
    }
}

int getNumberNextToc(char *line, int *index) {
    int num = 0;
    while (line[*index] >= '0' && line[*index] <= '9') {
        num = num * 10 + (line[*index] - '0');
        (*index)++;
    }
    return num;
}


void removeLine(TextBuffer *buffer, int y) {
    if (y >= buffer->num_lines) return; 
    for (int i = y; i < buffer->num_lines - 1; i++) {
        strcpy(buffer->lines[i], buffer->lines[i+1]);
    }
    buffer->num_lines--;
}



int main(int argc, char **argv){
    
    WINDOW *my_win;
    enableRawMode();
    start_color();
    init_pair(NORMAL, COLOR_WHITE, COLOR_BLUE);
    init_pair(KEYWORD, COLOR_BLUE, COLOR_BLUE);
    init_pair(STRING, COLOR_GREEN, COLOR_BLUE);
    init_pair(COMMENT, COLOR_YELLOW, COLOR_BLUE);
    init_pair(5, COLOR_BLACK, COLOR_WHITE); // New pair for status window background


    int c;

    TextBuffer *buffer = createTextBuffer();
    TextBuffer *copyBuffer = createTextBuffer();

    if (argc > 1) {
        readIntoBuffer(buffer, argv[1]);
    } else {
        insertLine(buffer, 0);
    }


    int x = 0;
    int y = 0;
    int scrollOffset = 0;
    bool editing = true;
    int width, height;
    getmaxyx(stdscr, height, width);

    my_win = create_newwin(height - 3, width - 2, y, x);
    int status_height = 1;
    int status_width = width; 
    int status_starty = height - 2;
    int status_startx = 0;

    WINDOW *status_win = create_newwin(status_height, status_width, status_starty, status_startx);
    wbkgd(status_win, COLOR_PAIR(5) | ' ');
    wrefresh(status_win);

    wmove(my_win, y, x);
    wrefresh(my_win);
    while(1){
        bool fileSaved = false;

        c = getch();

        int realY = y + scrollOffset;

        switch (c)
        {
        case ENTER:
            if (buffer->lines[realY][x] != '\0') {
                insertLine(buffer, realY + 1);

                strcpy(buffer->lines[realY + 1], buffer->lines[realY] + x);

                buffer->lines[realY][x] = '\0';
            } else {
                insertLine(buffer, realY + 1);
            }

            if (y >= height - 4) {
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
            if (y > 0 || scrollOffset > 0) {
                if (y > 0) {
                    y--;
                } else if (scrollOffset > 0) {
                    scrollOffset--;
                }
                if (x > strlen(buffer->lines[y + scrollOffset])) {
                    x = strlen(buffer->lines[y + scrollOffset]);
                }
            }
            break;
        case KEY_DOWN:
            if (realY < buffer->num_lines - 1) { 
                if (y < height - 4) {
                    y++;
                } else {
                    scrollOffset++; 
                }
                x = min(x, strlen(buffer->lines[realY + 1]));
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
        case CTRL_KEY('q'):
            disableRawMode();
            exit(0);
            break;
        case CTRL_KEY('s'):
            saveToFile(buffer, argv[1]);
            fileSaved = true;
            break;
        case CTRL_KEY('f'):
            {
                char query[100];
                echo(); 
                mvprintw(height - 1, 0, "Search: "); 
                getnstr(query, sizeof(query) - 1); 
                noecho();
                clear();

                searchInBuffer(buffer, query, &x, &y, &scrollOffset, height - 2); 
                
                mvprintw(height - 1, 0, " ");
                clrtoeol();
            }
            break;
        case ESCAPE:
        {
            char command[10];
            echo(); 
            mvprintw(height - 1, 0, ":");
            getnstr(command, sizeof(command) - 1);
            noecho();
            char* cmdPtr = command;

            if (command[0] >= '0' && command[0] <= '9') {
                int numLinesToCopy = atoi(cmdPtr); 
                while (isdigit((unsigned char)*cmdPtr)) cmdPtr++;
                if (*cmdPtr == 'c') {
                    freeTextBuffer(copyBuffer);
                    copyBuffer = createTextBuffer();
                    for (int i = 0; i < numLinesToCopy && (realY + i) < buffer->num_lines; i++) {
                        if (i >= copyBuffer->capacity) insertLine(copyBuffer, i);
                        strcpy(copyBuffer->lines[i], buffer->lines[realY + i]);
                    }
                    copyBuffer->num_lines = numLinesToCopy;
                }
                else if (*cmdPtr == 'd'){
                    freeTextBuffer(copyBuffer);
                    copyBuffer = createTextBuffer();
                    for (int i = 0; i < numLinesToCopy && (realY + i) < buffer->num_lines; i++) {
                        if (i >= copyBuffer->capacity) insertLine(copyBuffer, i);
                        strcpy(copyBuffer->lines[i], buffer->lines[realY + i]);
                    }
                    copyBuffer->num_lines = numLinesToCopy;
                    for (int i = 0; i < numLinesToCopy && (realY + i) < buffer->num_lines; i++) {
                        removeLine(buffer, realY);
                    }
                }
            } else if (*cmdPtr == 'p') {
                for (int i = 0; i < copyBuffer->num_lines; i++) {
                    insertLine(buffer, realY + i);
                    strcpy(buffer->lines[realY + i], copyBuffer->lines[i]);
                }
            }


            move(height - 1, 0);
            clrtoeol();
        }
        break;

        default:
            if (editing && c >= 32 && c <= 126) { 
                insertCharacter(buffer, realY, x, c);
                x++;
                if (x >= width - 2) { 
                    x = 0;
                    if (y >= height - 4) {
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



        werase(status_win);
        mvwhline(my_win, height - 3, 1, 0, width - 4);
        wrefresh(my_win); 
        char statusMessage[80];
        if (fileSaved) {
            snprintf(statusMessage, sizeof(statusMessage), "File saved successfully to %s", argv[1]);
        } else {
            snprintf(statusMessage, sizeof(statusMessage), "File: %s | Line: %d, Column: %d", argv[1], y + scrollOffset + 1, x + 1);
        }

        //int statusMsgStart = status_width - strlen(statusMessage) - 1; 

        mvwprintw(status_win, 0, 0, "%s", statusMessage);

        wrefresh(status_win);
        
        redrawWindow(my_win, buffer, scrollOffset);
        wmove(my_win, y, x + 6);
        wrefresh(my_win);


    }

    freeTextBuffer(buffer);
    freeTextBuffer(copyBuffer);
    delwin(status_win);
    endwin();

    return 0;
}