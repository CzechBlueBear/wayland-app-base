#include "debug.hpp"
#include <cstdio>
#include <cstring>

void do_complain(char const* where, char const* message) {
    fputs("error: ", stderr);
    fputs(where, stderr);
    fputs(": ", stderr);
    fputs(message, stderr);
    fputc('\n', stderr);
}

void do_complain(char const* where, std::string const& message) {
    fputs("error: ", stderr);
    fputs(where, stderr);
    fputs(": ", stderr);
    fputs(message.c_str(), stderr);
    fputc('\n', stderr);
}

void do_info(char const* where, char const* message) {
    fputs("info: ", stderr);
    fputs(where, stderr);
    fputs(": ", stderr);
    fputs(message, stderr);
    fputc('\n', stderr);
}

void do_info(char const* where, std::string const& message) {
    fputs("info: ", stderr);
    fputs(where, stderr);
    fputs(": ", stderr);
    fputs(message.c_str(), stderr);
    fputc('\n', stderr);
}

std::string errno_to_string() {
    int error_code = errno;

    // handle most frequent errors; strerror_r() does strange things
    switch (error_code) {
    case EACCES: return "EACCES"; break;
    case EAGAIN: return "EAGAIN"; break;
    case EBADF: return "EBADF"; break;
    case EPERM: return "EPERM"; break;
    case EINVAL: return "EINVAL"; break;
    case ELOOP: return "ELOOP"; break;
    case EIO: return "EIO"; break;
    case ENAMETOOLONG: return "ENAMETOOLONG"; break;
    case ENODEV: return "ENODEV"; break;
    case ENOMEM: return "ENOMEM"; break;
    case ENOTCONN: return "ENOTCONN"; break;
    case ENOTSUP: return "ENOTSUP"; break;
    case ENXIO: return "ENXIO"; break;
    case EOVERFLOW: return "EOVERFLOW"; break;
    }

    char buffer[1024];
    strerror_r(error_code, buffer, 1024);
    return std::string(buffer);
}
