#include "dma_simple.h"

int udmabuf_open(struct udmabuf* udmabuf, const char* name){
  char file_name[1024];
  int fd;
  unsigned char attr[1024];

  strcpy(udmabuf->name, name);
  udmabuf->file = -1;
  sprintf(file_name, "/sys/class/udmabuf/%s/phys_addr", name);
  if ((fd = open(file_name, O_RDONLY)) == -1){
    printf("Can not open %s\n", file_name);
    return(-1);
  }
  read(fd, (void*)attr, 1024);
  sscanf(attr, "%x", &udmabuf->phys_addr);
  close(fd);

  sprintf(file_name, "/sys/class/udmabuf/%s/size", name);
  if((fd = open(file_name, O_RDONLY)) == -1){
    printf("Can not open %s\n", file_name);
    return(-1);
  }
  read(fd, (void*)attr, 1024);
  sscanf(attr, "%d", &udmabuf->buf_size);
  close(fd);
  sprintf(file_name, "/dev/%s", name);
  if((udmabuf->file = open(file_name, O_RDWR|O_SYNC)) == -1){
    printf("Can not open %s\n", file_name);
    return(-1);
  }
  udmabuf->buf = mmap(NULL, udmabuf->buf_size, PROT_READ|PROT_WRITE, MAP_SHARED, udmabuf->file, 0);
  udmabuf->debug_vma = 0;
  udmabuf->sync_mode = 1;

  return 0;
}

int udmabuf_close(struct udmabuf* udmabuf){
  if(udmabuf->file < 0) return -1;

  close(udmabuf->file);
  udmabuf->file = -1;
  return 0;
}
