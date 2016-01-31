typedef int int32;
typedef unsigned char uint8;
#ifndef __INCLUDE_H__
    #define __INCLUDE_H__

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
    #endif
    #include "qcio.h"
#endif