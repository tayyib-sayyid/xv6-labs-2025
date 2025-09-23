#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

void memdump(char *fmt, char *data);

int
main(int argc, char *argv[])
{
  if(argc == 1){
    printf("Example 1:\n");
    int a[2] = { 61810, 2025 };
    memdump("ii", (char*) a);
    
    printf("Example 2:\n");
    memdump("S", "a string");
    
    printf("Example 3:\n");
    char *s = "another";
    memdump("s", (char *) &s);

    struct sss {
      char *ptr;
      int num1;
      short num2;
      char byte;
      char bytes[8];
    } example;
    
    example.ptr = "hello";
    example.num1 = 1819438967;
    example.num2 = 100;
    example.byte = 'z';
    strcpy(example.bytes, "xyzzy");
    
    printf("Example 4:\n");
    memdump("pihcS", (char*) &example);
    
    printf("Example 5:\n");
    memdump("sccccc", (char*) &example);
  } else if(argc == 2){
    // format in argv[1], up to 512 bytes of data from standard input.
    char data[512];
    int n = 0;
    memset(data, '\0', sizeof(data));
    while(n < sizeof(data)){
      int nn = read(0, data + n, sizeof(data) - n);
      if(nn <= 0)
        break;
      n += nn;
    }
    memdump(argv[1], data);
  } else {
    printf("Usage: memdump [format]\n");
    exit(1);
  }
  exit(0);
}

void formatHex64(uint64 x) {
  int start = 0;

  for(int nib = 15; nib >= 0; nib--) { // nib short for nibble
    int val = (x >> (nib * 4)) & 0xF;

    if(val || start) {
      start = 1;
      char c = (val < 10) ? ('0' + val) : ('a' + (val - 10));
      write(1, &c, 1);
    }
  }

  if(!start){
    char z = '0';
    write(1, &z, 1);
  } 
  char nl = '\n';
  write(1, &nl, 1);
}

/*static void print_hex32(uint32 x) {
  int started = 0;
  for (int nib = 7; nib >= 0; nib--) {
    int v = (x >> (nib * 4)) & 0xF;
    if (v || started) {
      started = 1;
      char c = (v < 10) ? ('0' + v) : ('a' + (v - 10));
      write(1, &c, 1);
    }
  }
  if (!started) { char z = '0'; write(1, &z, 1); }
  char nl = '\n'; write(1, &nl, 1);
}*/

void
memdump(char *fmt, char *data)
{
  // Your code here.
  for (char *fp = fmt; *fp; fp++) {
    switch (*fp) {
      case 'i': {
        uint32 v = 0;
        v |= (uint32)(unsigned char)data[0];
        v |= (uint32)(unsigned char)data[1] << 8;
        v |= (uint32)(unsigned char)data[2] << 16;
        v |= (uint32)(unsigned char)data[3] << 24;
        printf("%d\n", (int)v);
        data += 4;
        break;
      }

      case 'p': {
        uint32 v = 0;
        for (int i = 7; i >= 0; i--) v = (v << 8) | (uint64)(unsigned char)data[i];
        formatHex64(v);
        data += 8;
        break;
      }

      case 'h': {
        uint32 v = 0;
        v |= (uint32)(unsigned char)data[0];
        v |= (uint32)(unsigned char)data[1] << 8;
        printf("%d\n", (int)(short)v);
        data += 2;
        break;
      }

      case 'c': {
        printf("%c\n", *data);
        data += 1;
        break;
      }

      case 's': {
        uint64 addr = 0;
        for (int i = 7; i >= 0; i--) addr = (addr << 8) | (uint64)(unsigned char)data[i];
        printf("%s\n", (char *)addr);
        data += 8;
        break;
      }

      case 'S': {
        printf("%s\n", (char *)data);
        return;
      }

      default:
        break;
    }
  }
}
