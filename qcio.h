void dump(char buffer[],int len,long base);
int send_cmd(unsigned char* incmdbuf, int blen, unsigned char* iobuf);
int open_port(char* devname);
int memread(char* membuf,int adr, int len);
int mempeek(int adr);
int mempoke(int adr, int data);
int flash_read(int adr0, int adr1);



