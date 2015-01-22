
#define nand_cmd 0x1b400000
#define nand_addr0 0x1b400004
#define nand_addr1 0x1b400008
#define nand_cs    0x1b40000c
#define nand_exec  0x1b400010
#define nand_status 0x1b400014
#define nand_cfg1  0x1b400024
#define sector_buf 0x1b400100

// число страниц в 1 блоке
#define ppb 64

void dump(char buffer[],int len,long base);
int send_cmd(unsigned char* incmdbuf, int blen, unsigned char* iobuf);
int open_port(char* devname);
int memread(char* membuf,int adr, int len);
int mempeek(int adr);
int mempoke(int adr, int data);
int flash_read(int adr0, int adr1);

