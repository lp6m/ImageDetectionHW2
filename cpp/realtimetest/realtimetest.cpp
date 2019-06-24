#include <iostream>
#include <cmath>
#include <vector>
#include <iomanip>

#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>

#include "feature.h"
#include "weights.h"

using namespace std;
using namespace cv;

float thresh = 0.85;

double predict(int y, int x, double* dst){
  double dotprodcut = 0;
  for(int i = 0; i < FEATURESIZE; i++) dotprodcut += dst[i] * unscaled_weight[i];
  double svm_output = dotprodcut + unscaled_bias; 
  double proba = 1.0/(1.0+exp(-1*svm_output));
  return proba;
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
vector<pair<pair<int,int>,int>> predictRectFrame(cv::Mat inputimg, int window_height, int sy = 0, int sx = 0){
  vector<pair<pair<int,int>,int>> rst;
  // float scaling = 32.0 / window_height;
  cv::Mat frame_copy = inputimg.clone();
  for(int y = 0; y <= 240 - 32; y+=8){
    for(int x = 0; x <= 320 - 64; x+=8){
      double dst[FEATURESIZE];
      for(int i = 0; i < FEATURESIZE; i++) dst[i] = 0;
      // Mat window_gray_img = gray(cv::Rect(x, y, 64, 32));
      Mat window_bgr_img = inputimg(cv::Rect(x, y, 64, 32));
      getFeature(window_bgr_img, dst, true);
      double proba = predict(y, x, dst);
      
      if(proba > thresh){
        cout << window_height << " " << proba << endl;
        int detected_y = (int)((float)y * window_height / 32.0) + sy;
        int detected_x = (int)((float)x * window_height / 32.0) + sx;
        rst.push_back(make_pair(make_pair(detected_y, detected_x), window_height));
        // cout << y << " " << x << " " << proba << endl;
        // rectangle(frame_copy, Point(x, y), Point(x + 64, y + 32), Scalar(0,0,200), 2); //x,y //Scaler = B,G,R
        // cv::putText(frame_copy, to_string(proba), cv::Point(x+5,y+5), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255,0,0), 1, CV_AA);
      }
    }
  }
  return rst;
}

vector<pair<pair<int,int>,int>> processRectFrame(cv::Mat original_img, int window_height, int sy = 0, int sx = 0){
  if(window_height <= 32){
    //crop and predict
    Mat cropped_img = original_img(cv::Rect(sx, sy, 320, 240));
    return predictRectFrame(cropped_img, window_height, sy, sx);
  }else if(window_height <= 64){
    cv::Mat resized_img;
    float scaling = 32.0/window_height;
    resize(original_img, resized_img, cv::Size(), scaling, scaling);
    Mat cropped_img = resized_img(cv::Rect(0, 0, 320, 240));
    return predictRectFrame(cropped_img, window_height);
  }else {
    Mat shrinked_img = getShrinkFrame(original_img, window_height);
    return predictRectFrame(shrinked_img, window_height);
  }
}

void putRectangle(vector<pair<pair<int,int>,int>> window_candidates, Mat frame_image){
  for(int i = 0; i < window_candidates.size(); i++){
    int sy = window_candidates[i].first.first;
    int sx = window_candidates[i].first.second;
    int window_height = window_candidates[i].second;
    rectangle(frame_image, Point(sx, sy), Point(sx + window_height*2, sy + window_height), Scalar(0,0,200), 2); //x,y //Scaler = B,G,R
  }
}

int main(void){
  /*cv::Mat img = cv::imread("frame.png");
  cv::Mat res = processFrame(img, 80);
  cv::imshow("res", res);
  cv::waitKey(0);*/
  
  cv::VideoCapture cap(1);
  if(!cap.isOpened()){
    cout << "failed" << endl;
    return -1;
  }
  // std::chrono::system_clock::time_point  t1, t2, t3, t4, t5, t6, t7;
  while(1){
    // t1 = std::chrono::system_clock::now();
    cv::Mat frame;
    cap >> frame; // get a new frame from camera
    cv::Mat res = frame.clone();
    //auto candidates = processRectFrame(frame, 32, 240, 160);
    auto candidates2 = processRectFrame(frame, 80);
    //auto candidates3 = processRectFrame(frame, 70);
    auto candidates4 = processRectFrame(frame, 60);
    auto candidates5 = processRectFrame(frame, 50);

    //putRectangle(candidates, res);
    putRectangle(candidates2, res);
    //putRectangle(candidates3, res);
    putRectangle(candidates4, res);
    putRectangle(candidates5, res);
    cv::imshow("result", res);
    // cv::imwrite("result.png", frame_copy);
    cv::waitKey(1);
  }
}
