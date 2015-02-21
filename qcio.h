
extern unsigned int nand_cmd;    // 0x1b400000
#define nand_addr0 nand_cmd+4
#define nand_addr1 nand_cmd+8
#define nand_cs    nand_cmd+0xc
#define nand_exec  nand_cmd+0x10
#define nand_status nand_cmd+0x14
#define nand_cfg0  nand_cmd+0x20
#define nand_cfg1  nand_cmd+0x24
#define sector_buf nand_cmd+0x100


#define ppb 64             // число страниц в 1 блоке
#define maxblock 0x800     // Общее число блоков флешки

void dump(char buffer[],int len,long base);
int send_cmd(unsigned char* incmdbuf, int blen, unsigned char* iobuf);
int send_cmd_base(unsigned char* incmdbuf, int blen, unsigned char* iobuf, int prefixflag);
int open_port(char* devname);
int memread(char* membuf,int adr, int len);
int mempeek(int adr);
int mempoke(int adr, int data);
int flash_read(int block, int page, int sect);
void setaddr(int block, int page);
void nandwait();
void hello();
void load_ptable(char* ptable);
extern int siofd;
void port_timeout(int timeout);
