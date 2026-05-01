// glibc 2.38+ C23 compatibility shim for UE5's CentOS 7 sysroot.
// DCMTK defines _DEFAULT_SOURCE which causes glibc to redirect standard
// functions to __isoc23_* variants. These symbols get baked into the .a
// files but don't exist in UE5's sysroot. This shim forwards them back
// to the standard versions.
//
// Build WITHOUT _DEFAULT_SOURCE to avoid the same redirect:
//   clang -std=c17 -fPIC -c isoc23_compat.c -o isoc23_compat.o
//   ar rcs libisoc23_compat.a isoc23_compat.o

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

int __isoc23_sscanf(const char *str, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = vsscanf(str, fmt, ap);
    va_end(ap);
    return ret;
}

long __isoc23_strtol(const char *nptr, char **endptr, int base) {
    return strtol(nptr, endptr, base);
}
