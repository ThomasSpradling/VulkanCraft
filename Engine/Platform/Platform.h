#pragma once

#if defined(_WIN32)
#define PLATFORM_WINDOWS
#elif defined(__APPLE__)
#define PLATFORM_APPLE
#else
#define PLATFORM_UNIX
#endif
