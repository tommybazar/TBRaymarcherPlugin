// Platform-switching header for DCMTK arith

#if defined(_WIN32) || defined(_WIN64)
    #include "arith_win64.h"
#elif defined(__linux__)
    #include "arith_linux.h"
#else
    #error "Unsupported platform for DCMTK"
#endif
