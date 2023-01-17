#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <termios.h>

#include <unistd.h>

#define EDITOR_CAPACITY (10*1024)

typedef struct {
    size_t begin;
    size_t end;
} Line;

typedef struct {
    char data[EDITOR_CAPACITY];
    size_t data_count;
    Line lines[EDITOR_CAPACITY + 10];
    size_t lines_count;
    size_t cursor;
} Editor;

void editor_compute_lines(Editor *e) {
    // populate `e` with entries for `e->lines` and `e->data_count`
    e->lines_count = 0;
    size_t begin = 0;
    for (size_t i = 0; i < e->data_count; ++i) {
        if (e->data[i] == '\n') {
            e->lines[e->lines_count].begin = begin;
            e->lines[e->lines_count].end = i;
            e->lines_count += 1;
            begin = i + 1;
        }
    }
    e->lines[e->lines_count].begin = begin;
    e->lines[e->lines_count].end = e->data_count;
    e->lines_count += 1;
}

void editor_insert_char(Editor *e, char x) {
    if (e->data_count < EDITOR_CAPACITY) {
        memmove(&e->data[e->cursor + 1], &e->data[e->cursor], e->data_count - e->cursor);
        e->data[e->cursor] = x;
        e->cursor += 1;
        e->data_count += 1;
        editor_compute_lines(e);
    }
}

size_t editor_current_line(const Editor *e) {
    assert(e->cursor <= e->data_count);
    Line line;
    for (size_t i = 0; i < e->lines_count; ++i) {
        line = e->lines[i];
        if (
            line.begin <= e->cursor 
            && 
            e->cursor <= line.end
        ) return i;
    }
    return 0;
}

void editor_rerender(const Editor *e) {
    printf("\033[2J\033[H"); // clear screen and go to 0, 0
    fwrite(e->data, 1, e->data_count, stdout);
    size_t line = editor_current_line(e);
    // position the cursor
    printf(
        "\033[%zu;%zuH", // '%zu' is a specifier for a `size_t`
        line + 1, 
        e->cursor - e->lines[line].begin + 1
    );
}

int editor_start_interactive(Editor *e, const char * filepath) {
    if (!isatty(0)) {
        fprintf(stderr, "Please run in the terminal!\n");
        return 1;
    }

    struct termios term; // stores the state of the terminal
    if (tcgetattr(0, &term)) {
        fprintf(
            stderr, 
            "ERROR: could not save the state of the terminal: %s\n", 
            strerror(errno)
        );
        return 1;
    }

    term.c_lflag = term.c_lflag & ~ECHO; // equivalent of &= ~ECHO
    term.c_lflag = term.c_lflag & ~ICANON;

    if (tcsetattr(0, 0, &term)) {
        fprintf(
            stderr, 
            "ERROR: could not update the state of the terminal: %s\n", 
            strerror(errno)
        );
        return 1;
    }

    bool quit = false;
    bool insert = false;
    
    while (!quit && !feof(stdin)) {
        editor_rerender(e);
        if (insert) {
            int i = fgetc(stdin);
            switch (i) {
                case 27: {
                    insert = false;
                    FILE *f = fopen(filepath, "wb");
                    fwrite(e->data, 1, e->data_count, f);
                } break;
                default: {
                    editor_insert_char(e, i);
                }
            }
        } else {
            int x = fgetc(stdin);
            switch (x) {
                case 'i': {
                    insert = true;
                } break;
                case 'q': {
                    quit = true;
                } break;
                case 'h': {
                    if (e->cursor > 0) {
                        e->cursor = e->cursor - 1;
                    }
                } break;
                case 'l': {
                    if (e->cursor < e->data_count - 1) {
                        e->cursor += 1;
                    }
                } break;
                case 'j': {
                    size_t current_line = editor_current_line(e);
                    if (current_line < e->lines_count - 1) {
                        size_t current_col = e->cursor - e->lines[current_line].begin;
                        e->cursor = e->lines[current_line + 1].begin + current_col;
                        if (e->cursor > e->lines[current_line + 1].end) {
                            e->cursor = e->lines[current_line + 1].end;
                        }
                    }
                } break;
                case 'k': {
                    size_t current_line = editor_current_line(e);
                    if (current_line > 0) {
                        size_t current_col = e->cursor - e->lines[current_line].begin;
                        e->cursor = e->lines[current_line - 1].begin + current_col;
                        if (e->cursor > e->lines[current_line - 1].end) {
                            e->cursor = e->lines[current_line - 1].end;
                        }
                    }
                } break;
            }
        }
    }

    printf("\033[2J");

    term.c_lflag |= ECHO;
    tcsetattr(0, 0, &term);
    return 0;

}

static Editor editor;

int main(int num_args, char **args) {
    if (num_args < 2) {
        fprintf(stderr, "Usage: zed <input.txt>\n");
        fprintf(stderr, "ERROR: no input file was provided\n");
        return 1;
    }

    const char *file_path = args[1];
    FILE *f = fopen(file_path, "rb");
    if (f == NULL) {
        fprintf(
            stderr, 
            "ERROR: could not open file %s: %s\n", 
            file_path, 
            strerror(errno)
        );
        return 1;
    }

    // load contents of file into `editor.data`
    // assign length of file to `editor.data_count`
    editor.data_count = fread(editor.data, 1, EDITOR_CAPACITY, f);

    fclose(f);
    editor_compute_lines(&editor);

    return editor_start_interactive(&editor, file_path);
    return 0;
}
