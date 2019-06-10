#include <iostream>
#include <iomanip>
#include <cmath>
#include <chrono>
#include <vector>

#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>

#include "dma_simple.h"

using namespace std;
using namespace cv;

void *dma_regs;
void *hls_regs;
struct udmabuf intake_buf;
struct udmabuf outlet_buf;

int hw_setup(){
  int uio1_fd = open("/dev/uio1", O_RDWR);
  hls_regs = mmap(NULL, 0x1000, PROT_READ|PROT_WRITE, MAP_SHARED, uio1_fd, 0);

  int uio2_fd = open("/dev/uio2", O_RDWR);
  dma_regs = mmap(NULL, 0x1000, PROT_READ|PROT_WRITE, MAP_SHARED, uio2_fd, 0);
  printf("mmap end\n");

  if(udmabuf_open(&intake_buf, "udmabuf0") == -1){
    printf("udmabuf_open failed\n");
    return -1;
  }
  if(udmabuf_open(&outlet_buf, "udmabuf1") == -1){
    printf("udmabuf_open failed\n");
    return -1;
  }
  printf("open end / reset start\n");
  dma_reset(dma_regs);
  printf("dma reset end\n");
  regs_write32(hls_regs, 0x81);
  return 1;
}

void predict(cv::Mat bgr_img){
  std::chrono::system_clock::time_point  t1, t2;
  t1 = std::chrono::system_clock::now();

  cv::Mat bgra_img;
  cv::cvtColor(bgr_img, bgra_img, CV_BGR2BGRA);
  //copy image pixel value to input buffer
  for(int y = 0; y < 480; y++)  {
    memcpy(intake_buf.buf + 640 * y * 4, bgra_img.data + bgra_img.step * y, bgra_img.cols * 4 * sizeof(unsigned char));
  }
  /*for(int y = 0; y < 480; y++){
    for(int x = 0; x < 640; x++){
      cv::Vec<unsigned char, 3> pix = img.ptr<cv::Vec3b>(y)[x];
      int index = y * 640 + x;
      ((unsigned char*)intake_buf.buf)[index*3] = pix[0];
      ((unsigned char*)intake_buf.buf)[index*3+1] = pix[1];
      ((unsigned char*)intake_buf.buf)[index*3+2] = pix[2];
    }
    }*/
  
  printf("dma_setup start\n");
  dma_setup(dma_regs, intake_buf.phys_addr, outlet_buf.phys_addr);
  printf("dma_setup end\n");

  printf("dma_outlet start\n");
  dma_outlet_start(dma_regs, 57*73*4);
  printf("dma_outlet start end\n");
  printf("dma_intake start\n");
  dma_intake_start(dma_regs, 640*480*4);
  printf("dma_intake start end\n");

  printf("wait start\n");
  dma_wait_irq(dma_regs);
  printf("wait end\n");
  dma_clear_status(dma_regs);
  printf("dma_clear_status end\n");

  for(int y = 0; y < 57; y++){
    for(int x = 0; x < 73; x++){
      float rst = ((float*) (outlet_buf.buf))[y*73+x];
      if(rst > 0.70){
        cout << "y: " << y << " x: " << x << " " << rst << endl;
      }
    }
  }

  t2 = std::chrono::system_clock::now();
  double elapsed = (double)std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count();
  cout << "elapsed:" << elapsed << "[milisec]" << endl;
  cout << "fps:" << 1000.0/elapsed << "[fps]" << endl;
}

int main(){
  if(hw_setup() < 0){
    cerr << "hw_setup() failed" << endl;
    return -1;
  }
  cout << "hw_setup() finished" << endl;
  cv::Mat frame = cv::imread("frame.png");
  predict(frame);
  return 0;
}
