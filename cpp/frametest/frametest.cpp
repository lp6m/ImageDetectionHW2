#include <iomanip>
#include <iostream>
#include <cmath>
#include <chrono>
#include <vector>


#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>

#include "feature.h"
#include "weights.h"

using namespace std;
using namespace cv;

double predict(int y, int x, double* dst){
  double dotprodcut = 0;
  for(int i = 0; i < FEATURESIZE; i++) dotprodcut += dst[i] * unscaled_weight[i];
  double svm_output = dotprodcut + unscaled_bias; 
  double proba = 1.0/(1.0+exp(-1*svm_output));
  return proba;
}

int main(void){
  
  cv::Mat img = cv::imread("frame.png");
  cv::Mat frame_copy = img.clone();
  std::chrono::system_clock::time_point  t1, t2;
  t1 = std::chrono::system_clock::now();

  cv::Mat gray;
  // cv::cvtColor(img, gray, CV_BGR2GRAY);
  double max_proba = -1;
  int max_sy, max_sx;
  cv::Mat max_proba_img;
  int hitcnt = 0;
  int cnt = 0;
  for(int y = 0; y <= 240 - 32; y+=8){
    for(int x = 0; x <= 320 - 64; x+=8){
      cnt++;
      double dst[FEATURESIZE];
      for(int i = 0; i < FEATURESIZE; i++) dst[i] = 0;
      // Mat window_gray_img = gray(cv::Rect(x, y, 64, 32));
      Mat window_bgr_img = img(cv::Rect(x, y, 64, 32));
      getFeature(window_bgr_img, dst, true);
      double proba = predict(y, x, dst);
      // cout << rst << endl;
      if(max_proba < proba){
        max_proba = proba;
        max_sx = x;
        max_sy = y;
        max_proba_img = window_bgr_img.clone();
      }
      //cout << proba << endl;
      if(proba > 0.80){
        //cout << hitcnt << " " << y << " " << x << " " << proba << endl;
        rectangle(frame_copy, Point(x, y), Point(x + 64, y + 32), Scalar(0,0,200), 2); //x,y //Scaler = B,G,R
        cv::putText(frame_copy, to_string(proba), cv::Point(x+5,y+5), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255,0,0), 1, CV_AA);

        hitcnt++;

        cv::imwrite("hit/hit_" + to_string(hitcnt) + ".png", window_bgr_img);
      }
    }
  }
  cout << cnt << endl;
  // getFeature(img, dst, false);
  // double rst = predict(dst);
  // cout << fixed << setprecision(10) << max_proba << endl;
  // cout << fixed << max_sy << " " << max_sx << endl;
  // cv::imshow("result", frame_copy);
  // cv::imwrite("result.png", frame_copy);
  t2 = std::chrono::system_clock::now();
  double elapsed = (double)std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count();

  cout << "elapsed:" << elapsed << "[milisec]" << endl;
  cout << "fps:" << 1000.0/elapsed << "[fps]" << endl;
  // cv::waitKey(0);
}
