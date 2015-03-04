#include <stdio.h>
void main(int argc, char* argv[]) {
  
FILE* in=fopen(argv[1],"r");
FILE* data=fopen("tdata.img","w");
FILE* oob=fopen("toob.img","w");

if (in == 0) return;

unsigned char buf[6600];
int i,j;

for(j=0;j<64;j++) printf("%02x ",j);
printf("\n");
for(;;) {
  printf("\n");
    fread(buf,1,2048,in);
    if (feof(in)) return;
    fwrite(buf,1,2048,data);
    fread(buf,1,64,in);
    if (feof(in)) return;
    fwrite(buf,1,64,oob);
    for(j=0;j<64;j++) printf("%02x ",buf[j]);
}  
  

}
