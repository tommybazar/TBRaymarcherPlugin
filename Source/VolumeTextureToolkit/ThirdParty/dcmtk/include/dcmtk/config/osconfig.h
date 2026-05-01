// Platform-switching header for DCMTK osconfig
// Selects the appropriate platform-specific configuration

#if defined(_WIN32) || defined(_WIN64)
    #include "osconfig_win64.h"
#elif defined(__linux__)
    #include "osconfig_linux.h"
#else
    #error "Unsupported platform for DCMTK"
#endif
