#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <termios.h>

#include <unistd.h>

#define ZED_CAPACITY 1024

typedef struct {
    char data[ZED_CAPACITY];
    size_t count;
} Editor;

static Editor editor = {0};

int main(int argc, char **argv)
{
    if (!isatty(0)) {
        fprintf(stderr, "Please run in the terminal!\n");
        return 1;
    }

    if (argc < 2) {
        fprintf(stderr, "Usage: zed <input.txt>\n");
        fprintf(stderr, "ERROR: no input file was provided\n");
        return 1;
    }

    // stores the state of the terminal
    struct termios term;
    if (tcgetattr(0, &term)) {
        fprintf(stderr, "ERROR: could not save the state of the terminal\n");
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

    editor.count = fread(editor.data, 1, ZED_CAPACITY, f);
    fwrite(editor.data, 1, editor.count, stdout);

    printf("Input file: %s\n", file_path);

    fclose(f);

    return 0;
}
