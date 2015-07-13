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
static int printf(const char* format, ...)
{
    static char ostr[2048];
    static wchar_t wstr[2048];
    va_list args;

    va_start(args, format);
    vsprintf(ostr, format, args);
    va_end(args);
    MultiByteToWideChar(CP_UTF8, 0, ostr, -1, wstr, 2048);
    WideCharToMultiByte(CP_OEMCP, 0, wstr, -1, ostr, 2048, NULL, NULL);
    return printf_s("%s", ostr);
}
#endif

extern unsigned int nand_cmd;    // 0x1b400000
extern unsigned int spp;
extern unsigned int pagesize;
extern unsigned int sectorsize;
extern unsigned int maxblock;    // Общее число блоков флешки
extern unsigned int oobsize;
extern unsigned int bad_loader;

// тип чипcета:
//-----------------------------------
//  0 - (MDM9x15, по умолчанию), 
//  1 - (MDM8200)
//  2 - (MDM9x00), 
//  3 - (MDM9x25)
extern unsigned int chip_type; 


// #define nand_addr0 nand_cmd+4
// #define nand_addr1 nand_cmd+8
// #define nand_cs    nand_cmd+0xc
// #define nand_exec  nand_cmd+0x10
// #define nand_status nand_cmd+0x14
// #define nand_cfg0  nand_cmd+0x20
// #define nand_cfg1  nand_cmd+0x24
// #define nand_ecc_cfg  nand_cmd+0x28
// #define NAND_FLASH_READ_ID nand_cmd+0x40
// #define sector_buf nand_cmd+0x100

extern unsigned int nand_addr0;
extern unsigned int nand_addr1;
extern unsigned int nand_cs;   
extern unsigned int nand_exec; 
extern unsigned int nand_status;
extern unsigned int nand_cfg0;  
extern unsigned int nand_cfg1;  
extern unsigned int nand_ecc_cfg;
extern unsigned int NAND_FLASH_READ_ID; 
extern unsigned int sector_buf;

extern unsigned int nc_stop,nc_read,nc_readall,nc_program,nc_programall,nc_erase,nc_identify;


#define ppb 64             // число страниц в 1 блоке

void ttyflush();
void dump(unsigned char buffer[],unsigned int len,unsigned int base);
int send_cmd(unsigned char* incmdbuf, int blen, unsigned char* iobuf);
int send_cmd_np(unsigned char* incmdbuf, int blen, unsigned char* iobuf);
int send_cmd_base(unsigned char* incmdbuf, int blen, unsigned char* iobuf, int prefixflag);
int open_port(char* devname);
int memread(char* membuf,int adr, int len);
int memwrite(unsigned int adr, unsigned char* buf, unsigned int len);
unsigned int mempeek(int adr);
int mempoke(int adr, int data);
int flash_read(int block, int page, int sect);
void setaddr(int block, int page);
void nandwait();
void hello(int mode);
void load_ptable(unsigned char* ptable);
extern int siofd;
void port_timeout(int timeout);
int send_cmd_massdata(unsigned char* incmdbuf, int blen, unsigned char* iobuf, unsigned int datalen);
unsigned int send_unframed_buf(char* outcmdbuf, unsigned int outlen, int prefixflag);
unsigned int receive_reply(char* iobuf, int masslen);
void show_errpacket(char* descr, char* pktbuf, int len);
int qclose(int errmode);
void block_erase(int block);
void get_flash_config();
void nand_reset();
int dload_sahara();
void disable_bam();
void close_port();
void define_chipset(char* arg);
int test_zero(unsigned char* buf, int len);
void set_chipset(unsigned int c);
unsigned char* get_chipname();
int identify_chipset();
int test_loader();
void exec_nand(int cmd);
#endif