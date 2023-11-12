#pragma once
#include <string>

#define complain(message) do { do_complain(__FUNCTION__, message); } while(0);
#define info(message) do { do_info(__FUNCTION__, message); } while(0);

void do_complain(char const* where, char const* message);
void do_complain(char const* where, std::string const& message);
void do_info(char const* where, char const* message);
void do_info(char const* where, std::string const& message);

std::string errno_to_string();