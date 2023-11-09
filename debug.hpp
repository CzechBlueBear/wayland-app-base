#pragma once
#include <string>

#define complain(message) do { do_complain(__PRETTY_FUNCTION__, message); } while(0);
#define info(message) do { do_info(__PRETTY_FUNCTION__, message); } while(0);

void do_complain(char const* where, char const* message);
void do_complain(char const* where, std::string message);
void do_info(char const* where, char const* message);
void do_info(char const* where, std::string message);