#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

static void repl() {
    char line[1024];
    for (;;) {
        printf("> ");

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        interpret(line);
    }
}

static char* readFile(const char *path) {
    FILE *file = fopen(path, "rb"); // Mode: "rb" (read binary) ensures that the OS doesn't "translate" anything (like '\n' to a line jump)
                                                  // and it also ensures that ftell, gets the name of all the bytes in the file
    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    fseek(file, 0L, SEEK_END); // Moves the file cursor to the end of the file
    size_t fileSize = ftell(file); // ftell asks the system "how many bytes away are we?" because we're at the end thanks to fseek
                                   // the number returned is the size of the file.
    rewind(file); // return the file cursor to the beginning of the file

    char *buffer = (char*)malloc(fileSize + 1); // Then we allocate memory in the heap for the whole file, + 1 byte for the null character (\0)
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file); // fread copies the bytes from the file to the buffer
                                                                                    // and bytesRead is how many bytes were read
                                                                                    // if everything goes well, bytesRead is the length of the buffer
    if (bytesRead < fileSize) {
        fprintf(stderr, "Cound not read file \"%s\".\n", path);
        exit(74);
    }
    buffer[bytesRead] = '\0'; // The last element of the buffer should always be the null character

    fclose(file);
    return buffer;
}

static void runFile(const char *path) {
    char *source = readFile(path);
    InterpretResult result = interpret(source);
    free(source);

    if (result == INTERPRET_COMPILE_ERROR) exit(65);
    if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

int main(int argc, char *argv[]) {
    initVM();

    if (argc == 1) { // This means the user only typed the name of the program (./cfer) -> we initiate the REPL
        repl();
    } else if (argc == 2) { // This means the user typed something else aside from the name of the program, and we assume it was the filepath to the .fer file
                            // -> we interpret the file
        runFile(argv[1]);
    } else { // We only need those 2 arguments, so if we pass more arguments we exit the program with an error
        fprintf(stderr, "Usage: cfer [path]\n");
        exit(64);
    }


    freeVM();
    return 0;
}
