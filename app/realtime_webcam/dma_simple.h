#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>

#define DMA_INTAKE_DMACR  (0x0000)
#define DMA_INTAKE_DMASR  (0x0004)
#define DMA_INTAKE_SA     (0x0018)
#define DMA_INTAKE_LENGTH (0x0028)

#define DMA_OUTLET_DMACR  (0x0030)
#define DMA_OUTLET_DMASR  (0x0034)
#define DMA_OUTLET_DA     (0x0048)
#define DMA_OUTLET_LENGTH (0x0058)

#define DMA_CR_RS         (1u<<0)
#define DMA_CR_RESET      (1u<<2)

#define DMA_SR_HALTED     (1u<<0)
#define DMA_SR_IDLE       (1u<<1)
#define DMA_SR_IOC_Irq    (1u<<12)
#define DMA_SR_ERR_Irq    (1u<<14)

static inline uint32_t regs_read32(void* addr){
  volatile uint32_t* regs_addr = (uint32_t*)(addr);
  return *regs_addr;
}

static inline void regs_write32(void* addr, uint32_t data){
  volatile uint32_t* regs_addr = (uint32_t*)(addr);
  *regs_addr = data;
}

static inline void dma_reset(void* regs){
  regs_write32(regs + DMA_INTAKE_DMACR, DMA_CR_RESET);
  while(regs_read32(regs + DMA_INTAKE_DMACR) & DMA_CR_RESET);
  regs_write32(regs + DMA_OUTLET_DMASR, DMA_CR_RESET);
  while(regs_read32(regs + DMA_OUTLET_DMACR) & DMA_CR_RESET);
}

static inline void dma_setup(void* regs, unsigned long src_addr, unsigned long dst_addr){
  regs_write32(regs + DMA_OUTLET_DMACR, DMA_CR_RS);
  regs_write32(regs + DMA_OUTLET_DA, dst_addr);
  regs_write32(regs + DMA_INTAKE_DMACR, DMA_CR_RS);
  regs_write32(regs + DMA_INTAKE_SA, src_addr);
}

static inline void dma_intake_start(void* regs, unsigned int xfer_size){
  regs_write32(regs + DMA_INTAKE_LENGTH, xfer_size);
}

static inline void dma_outlet_start(void* regs, unsigned int xfer_size){
  regs_write32(regs + DMA_OUTLET_LENGTH, xfer_size);
}

static inline void dma_wait_irq(void* regs){
  while(~regs_read32(regs + DMA_INTAKE_DMASR) & DMA_SR_IOC_Irq);
  while(~regs_read32(regs + DMA_OUTLET_DMASR) & DMA_SR_IOC_Irq);
}

static inline void dma_clear_status(void* regs){
  regs_write32(regs + DMA_INTAKE_DMASR, DMA_SR_IOC_Irq | DMA_SR_ERR_Irq);
  regs_write32(regs + DMA_OUTLET_DMASR, DMA_SR_IOC_Irq | DMA_SR_ERR_Irq);
}

struct udmabuf{
  char name[128];
  int file;
  unsigned char* buf;
  unsigned int buf_size;
  unsigned long phys_addr;
  unsigned long debug_vma;
  unsigned long sync_mode;
};

int udmabuf_open(struct udmabuf* udmabuf, const char* name);

int udmabuf_close(struct udmabuf* udmabuf);
