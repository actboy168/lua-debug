#pragma once

#include <string>
#include <unistd.h>

bool injectdll(pid_t pid, const std::string& dll, const char* entry = 0);
