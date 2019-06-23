#include <iostream>
#include <iomanip>
#include <iterator>
#include <cmath>
#include <chrono>
#include <fstream>
#include <vector>

#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <unistd.h>

#include "json11.hpp"
#include "dma_simple.h"
using namespace std;
using namespace cv;

void *dma_regs;
void *hls_regs;
struct udmabuf intake_buf;
struct udmabuf outlet_buf;
float BIAS;
float THRESH;

unsigned int *assignToPhysicalUInt(unsigned long address,unsigned int size){
	int devmem = open("/dev/mem", O_RDWR | O_SYNC);
	off_t PageOffset = (off_t) address % getpagesize();
	off_t PageAddress = (off_t) (address - PageOffset);
	return (unsigned int *) mmap(0, size*sizeof(unsigned int), PROT_READ|PROT_WRITE, MAP_SHARED, devmem, PageAddress);
}

int hw_setup(){
  int uio1_fd = open("/dev/uio1", O_RDWR);
  hls_regs = mmap(NULL, 0x10000, PROT_READ|PROT_WRITE, MAP_SHARED, uio1_fd, 0);

  int uio2_fd = open("/dev/uio2", O_RDWR);
  dma_regs = mmap(NULL, 0x1000, PROT_READ|PROT_WRITE, MAP_SHARED, uio2_fd, 0);

  if(udmabuf_open(&intake_buf, "udmabuf0") == -1){
    printf("udmabuf_open failed\n");
    return -1;
  }
  if(udmabuf_open(&outlet_buf, "udmabuf1") == -1){
    printf("udmabuf_open failed\n");
    return -1;
  }
  dma_reset(dma_regs);
  
  regs_write32(hls_regs, 0x01); //start
  regs_write32(hls_regs, 0x80); //enable autorestart

  return 1;
}

void writebram(unsigned int* target, string array_name, json11::Json json){
  int cnt = 0;
  for(auto &k: json[array_name].array_items()){
    target[cnt++] = k.number_value();
  }
}
int param_setup(){
  unsigned int* bgrhsv_w1 = assignToPhysicalUInt(0xA0000000, 24*4);
  unsigned int* bgrhsv_w2 = assignToPhysicalUInt(0xA0004000, 24*4);
  unsigned int* bgrhsv_w3 = assignToPhysicalUInt(0xA0008000, 24*4);
  unsigned int* bgrhsv_w4 = assignToPhysicalUInt(0xA000C000, 24*4);
  unsigned int* hog_w1 = assignToPhysicalUInt(0xA0010000, 63*4);
  unsigned int* hog_w2 = assignToPhysicalUInt(0xA0014000, 63*4);
  unsigned int* hog_w3 = assignToPhysicalUInt(0xA0018000, 63*4);

  ifstream ifs("weights.json");
  if(ifs.fail()){
    cerr << "json file load failed" << endl;
    return -1;
  }
  std::istreambuf_iterator<char> it(ifs);
  std::istreambuf_iterator<char> last;
  std::string str(it, last);
  string err;
  auto json = json11::Json::parse(str, err);
  if(err != ""){
    cerr << "json file parse error: " << err << endl;
    return -1;
  }

  BIAS = json["bias"].number_value();
  THRESH = json["thresh"].number_value();
  writebram(hog_w1, "hog_w1", json);
  writebram(hog_w2, "hog_w2", json);
  writebram(hog_w3, "hog_w3", json);
  writebram(bgrhsv_w1, "bgrhsv_w1", json);
  writebram(bgrhsv_w2, "bgrhsv_w2", json);
  writebram(bgrhsv_w3, "bgrhsv_w3", json);
  writebram(bgrhsv_w4, "bgrhsv_w4", json);

  return 1;
}

cv::Mat  predict(cv::Mat bgr_img){
  std::chrono::system_clock::time_point  t1, t2;
  t1 = std::chrono::system_clock::now();
  cv::Mat result_frame = bgr_img.clone();
  cv::Mat bgra_img;
  cv::cvtColor(bgr_img, bgra_img, CV_BGR2BGRA);
  cv::Mat bgra_quarter_img(bgra_img, cv::Rect(0, 0, 320, 240));
  //copy image pixel value to dma input buffer
  for(int y = 0; y < 240; y++)  {
    memcpy(intake_buf.buf + 320 * y * 4, bgra_quarter_img.data + bgra_quarter_img.step * y, bgra_quarter_img.cols * 4 * sizeof(unsigned char));
  }
  
  dma_setup(dma_regs, intake_buf.phys_addr, outlet_buf.phys_addr);
  dma_outlet_start(dma_regs, 27*33*4);
  dma_intake_start(dma_regs, 320*240*4);
  dma_wait_irq(dma_regs);
  dma_clear_status(dma_regs);
  for(int y = 0; y < 27; y++){
    for(int x = 0; x < 33; x++){
      int index = y * 33 + x;
      float rst = ((float*) (outlet_buf.buf))[index] + BIAS;
      if(rst >= THRESH){
	//sigmoid function
	float proba = 1.0/(1.0+exp(-rst));
	cout << fixed << setprecision(10) << rst << " " << proba << endl;
	int yy = y * 8;
	int xx = x * 8;
	rectangle(result_frame, Point(xx, yy), Point(xx + 64, yy + 32), Scalar(0,0,200), 2); //x,y //Scaler = B,G,R
        cv::putText(result_frame, to_string(proba), cv::Point(xx+5,yy+5), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255,0,0), 1, CV_AA);
      }
    }
  }

  t2 = std::chrono::system_clock::now();
  double elapsed = (double)std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count();
  cout << "elapsed:" << elapsed << "[milisec]" << endl;
  cout << "fps:" << 1000.0/elapsed << "[fps]" << endl;
  return result_frame;
}

int main(){
  if(param_setup() < 0){
    cout << "param setup failed" << endl;
    return -1;
  }
  if(hw_setup() < 0){
    cout << "hw_setup() failed" << endl;
    return -1;
  }
  cv::Mat frame = cv::imread("frame.png");
  cv::Mat result_frame = predict(frame);
  cv::imwrite("result.png", result_frame);
  return 0;
}
