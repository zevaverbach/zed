#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <termios.h>

#include <unistd.h>

#define EDITOR_CAPACITY 1024

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

void editor_recompute_lines(Editor *e) {
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

size_t editor_current_line(const Editor *e) {
    assert(e->cursor <= e->data_count);
    for (size_t i = 0; i < e->lines_count; ++i) {
        if (
            e->lines[i].begin <= e->cursor 
            && 
            e->cursor <= e->lines[i].end
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

static Editor editor = {0};

int editor_start_interactive(Editor *e) {
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

        } else {
            int x = fgetc(stdin);
            switch (x) {
                case 'q': {
                    quit = true;
                } break;
                case 'h': {
                    if (e->cursor > 0) {
                        e->cursor = e->cursor - 1;
                    }
                } break;
                case 'l': {
                    if (e->cursor < EDITOR_CAPACITY) {
                        e->cursor += 1;
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

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: zed <input.txt>\n");
        fprintf(stderr, "ERROR: no input file was provided\n");
        return 1;
    }

    const char *file_path = argv[1];
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

    editor.data_count = fread(editor.data, 1, EDITOR_CAPACITY, f);

    fclose(f);

    editor_recompute_lines(&editor);

    return editor_start_interactive(&editor);
}
