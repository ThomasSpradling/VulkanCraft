#pragma once

#define NOMINMAX
#include <Windows.h>
#include <string>

std::string WindowsErrorString(DWORD error_code);
