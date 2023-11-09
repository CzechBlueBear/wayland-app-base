#include "debug.hpp"
#include <cstdio>

void do_complain(char const* where, char const* message) {
    fputs("error: ", stderr);
    fputs(where, stderr);
    fputs(": ", stderr);
    fputs(message, stderr);
    fputc('\n', stderr);
}

void do_info(char const* where, char const* message) {
    fputs("info: ", stderr);
    fputs(where, stderr);
    fputs(": ", stderr);
    fputs(message, stderr);
    fputc('\n', stderr);
}
