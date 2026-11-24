// Minimal SDL2 stand-in: the Vircon32 dev tools only use SDL to find
// their own executable's directory (the compiler looks for include/ there).
#pragma once
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <climits>
#if defined(__APPLE__)
#include <mach-o/dyld.h>   // _NSGetExecutablePath: macOS has no /proc/self/exe
#include <cstdint>
#endif
inline int SDL_Init(unsigned) { return 0; }
inline void SDL_Quit() {}
inline void SDL_free(void* p) { std::free(p); }
inline char* SDL_GetBasePath() {
    char buf[PATH_MAX];
#if defined(__APPLE__)
    char raw[PATH_MAX]; uint32_t size = sizeof(raw);
    if (_NSGetExecutablePath(raw, &size) != 0 || realpath(raw, buf) == NULL) return strdup("./");
#else
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf)-1);
    if (n < 0) return strdup("./");
    buf[n] = 0;
#endif
    char* slash = strrchr(buf, '/'); if (slash) slash[1] = 0;
    return strdup(buf);
}
