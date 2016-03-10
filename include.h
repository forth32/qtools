#ifndef __INCLUDE_H__
    #define __INCLUDE_H__

#include <stdint.h>
typedef int32_t int32;
typedef uint32_t uint32;
typedef uint8_t uint8;
typedef int8_t int8;
typedef int16_t int16;
typedef uint16_t uint16;

#ifdef WIN32
	#define PACKED_ON(name) __pragma(pack(push, 1)) struct name
	#define PACKED_OFF __pragma(pack(pop))
#else
	#define PACKED_ON(name) struct __attribute__ ((__packed__)) name
	#define PACKED_OFF
#endif

    #include <errno.h>
    #include <fcntl.h>
    #include <stdio.h>
    #include <string.h>
    #include <stdlib.h>
    #include <sys/types.h>
    #include <sys/stat.h>

    #ifdef WIN32
        #include "getopt.h"
        #ifndef NO_IO
            #include <io.h>
        #endif
        #include <windows.h>
    #else
        #include <getopt.h>
        #include <termios.h>
        #include <unistd.h>
        #include <readline/readline.h>
        #include <readline/history.h>
    #endif

    #ifdef WIN32
        #include "printf.h"
		#define S_IFMT  170000
		#define S_IFDIR 40000
		#define S_IFBLK 60000
		#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
		#define usleep(n) Sleep(n/1000)
    #endif
    #include "qcio.h"
    #include "ptable.h"
#endif