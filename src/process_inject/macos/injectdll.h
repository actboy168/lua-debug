#pragma once

#include <unistd.h>

#include <string>

bool injectdll(pid_t pid, const std::string& dll, const char* entry = 0);
