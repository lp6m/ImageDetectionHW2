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

float THRESH;
float BIAS;
bool LOG_MODE;

struct window_rect{
  int sy;
  int sx;
  int ey;
  int ex;
};

window_rect shukai_waku, cross_waku;

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
  regs_write32(hls_regs, 0x81);
  return 1;
}

void writebram(unsigned int* target, string array_name, json11::Json json){
  int cnt = 0;
  for(auto &k: json[array_name].array_items()){
    target[cnt++] = k.number_value();
  }
}
int json_setup(){
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

  writebram(hog_w1, "hog_w1", json);
  writebram(hog_w2, "hog_w2", json);
  writebram(hog_w3, "hog_w3", json);
  writebram(bgrhsv_w1, "bgrhsv_w1", json);
  writebram(bgrhsv_w2, "bgrhsv_w2", json);
  writebram(bgrhsv_w3, "bgrhsv_w3", json);
  writebram(bgrhsv_w4, "bgrhsv_w4", json);

  THRESH = json["thresh"].number_value();
  BIAS = json["bias"].number_value();
  LOG_MODE = (json["log_mode"].int_value() == 1);

  shukai_waku.sy = json["shukai_sy"].int_value();
  shukai_waku.sx = json["shukai_sx"].int_value();
  shukai_waku.ey = json["shukai_ey"].int_value();
  shukai_waku.ex = json["shukai_ex"].int_value();

  cross_waku.sy = json["cross_sy"].int_value();
  cross_waku.sx = json["cross_sx"].int_value();
  cross_waku.ey = json["cross_ey"].int_value();
  cross_waku.ex = json["cross_ex"].int_value();
  
  return 1;
}

cv::Mat getShrinkFrame(cv::Mat original_img, int window_height){
  //shrink whole frame
  float scaling = 32.0/window_height;
  // cv::Mat resized_img(cv::Size(640.0*scaling, 480.0*scaling), CV_8UC3);
  cv::Mat resized_img;
  resize(original_img, resized_img, cv::Size(), scaling, scaling);
  //pase scaled image to 320x240 canvas
  cv::Mat scaled_frame(cv::Size(320, 240), CV_8UC3);
  cv::Mat roi_dst = scaled_frame(cv::Rect(0, 0, 640*scaling, 480*scaling));
  resized_img.copyTo(roi_dst);
  return scaled_frame;
}

//input: 320x240pix
//output: ((sy,sx), window_height)
vector<pair<pair<int,int>,int>> predictRectFrame(cv::Mat inputimg, int window_height, window_rect waku, int sy = 0, int sx = 0){
  vector<pair<pair<int,int>,int>> rst;

  cv::Mat bgra_img;
  cv::cvtColor(inputimg, bgra_img, CV_BGR2BGRA);
  //copy image pixel value to input buffer
  for(int y = 0; y < 240; y++)  {
    memcpy(intake_buf.buf + 320 * y * 4, bgra_img.data + bgra_img.step * y, bgra_img.cols * 4 * sizeof(unsigned char));
  }
  
  dma_setup(dma_regs, intake_buf.phys_addr, outlet_buf.phys_addr);
  dma_outlet_start(dma_regs, 27*33*4);
  dma_intake_start(dma_regs, 320*240*4);
  dma_wait_irq(dma_regs);
  dma_clear_status(dma_regs);

  for(int y = 0; y < 27; y++){
    for(int x = 0; x < 33; x++){
      int index = y * 33 + x;
      float hwrst = ((float*) (outlet_buf.buf))[index];
      hwrst += BIAS;
      // float proba = 1.0/(1.0+exp(-rst));
      if(hwrst > THRESH){
        int detected_y = (int)((float)y * 8 * window_height / 32.0) + sy;
        int detected_x = (int)((float)x * 8 * window_height / 32.0) + sx;
        //inside window rect
        if(waku.sy <= detected_y && waku.sx <= detected_x && detected_y + window_height <= waku.ey && detected_x + window_height*2 <= waku.ex){
          float proba = 1.0/(1.0+exp(-hwrst));
          if(LOG_MODE) cout << window_height << " " << proba << " " << detected_y << " " << detected_x << endl;
          rst.push_back(make_pair(make_pair(detected_y, detected_x), window_height));
        }
      }
    }
  }
  return rst;
}

vector<pair<pair<int,int>,int>> processRectFrame(cv::Mat original_img, int window_height, window_rect waku, int sy = 0, int sx = 0){
  if(window_height <= 32){
    //crop and predict
    Mat cropped_img = original_img(cv::Rect(sx, sy, 320, 240));
    return predictRectFrame(cropped_img, window_height, waku, sy, sx);
  }else if(window_height <= 64){
    cv::Mat resized_img;
    float scaling = 32.0/window_height;
    resize(original_img, resized_img, cv::Size(), scaling, scaling);
    Mat cropped_img = resized_img(cv::Rect(0, 0, 320, 240));
    return predictRectFrame(cropped_img, window_height, waku);
  }else {
    Mat shrinked_img = getShrinkFrame(original_img, window_height);
    return predictRectFrame(shrinked_img, window_height, waku);
  }
}

int main(int argc, char *argv[]){
  int mode = atoi(argv[1]);

  if(json_setup() < 0){
    cout << "json setup failed" << endl;
    return -1;
  }
  if(hw_setup() < 0){
    cout << "hw_setup() failed" << endl;
    return -1;
  }
  cout << "hw_setup() finished" << endl;
  
  cv::VideoCapture cap(0);
  if(!cap.isOpened()){
    cout << "failed" << endl;
    return -1;
  }

  std::chrono::system_clock::time_point  t1, t2;
  while(1){
    t1 = std::chrono::system_clock::now();
    
    cv::Mat frame;
    cap >> frame; // get a new frame from camera
    cv::Mat res = frame.clone();

    window_rect waku;
    if(mode == 0) waku = shukai_waku;
    else waku = cross_waku;

    auto candidates = processRectFrame(frame, 32, waku, 240, 160);
    auto candidates2 = processRectFrame(frame, 80, waku);
    auto candidates3 = processRectFrame(frame, 60, waku);
   
    t2 = std::chrono::system_clock::now();
    double elapsed = (double)std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count();
    if(LOG_MODE) cout << "elapsed:" << elapsed << "[milisec]" << endl;
    if(LOG_MODE) cout << "fps:" << 1000.0/elapsed << "[fps]" << endl;
  }
  return 0;
}
