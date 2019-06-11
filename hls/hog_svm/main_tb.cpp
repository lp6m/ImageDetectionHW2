#include "/opt/Xilinx/Vivado/2018.2/include/gmp.h"
#include <iostream>
#include <cmath>
#include <string.h>
#include <vector>
#include <ap_fixed.h>
#include <ap_axi_sdata.h>
#include "weight.h"
#include <opencv2/opencv.hpp>
#include <hls_stream.h>
#include <sstream>
#include "consts.h"

using namespace std;
using namespace cv;

void hog_svm(hls::stream<ap_axiu<32,1,1,1> >& instream, hls::stream<ap_axiu<32,1,1,1> >& outstream,
		pixweight bgrhsv_w1[8], pixweight bgrhsv_w2[8], pixweight bgrhsv_w3[8], pixweight bgrhsv_w4[8],
		hogweight hog_w1[7], hogweight hog_w2[7], hogweight hog_w3[7], ap_fixed_point bias);

string tostr(double val){
	std::stringstream ss;
	ss << val;
	std::string str = ss.str();
	return str;
}

pixweight bound_bgrhsv_w1[8], bound_bgrhsv_w2[8], bound_bgrhsv_w3[8], bound_bgrhsv_w4[8];
hogweight bound_hog_w1[7], bound_hog_w2[7], bound_hog_w3[7];

void prepare_bound_input(){
	for(int i = 0; i < 7; i++){
		for(int j = 0; j < 9; j++){
			int index = (i*9+j)*4;
			ap_uint<128> tmp = 0;
			tmp = tmp | ((ap_uint<128>)unbound_hog_w1[index]);
			tmp = tmp | ((ap_uint<128>)unbound_hog_w1[index+1] << 32);
			tmp = tmp | ((ap_uint<128>)unbound_hog_w1[index+2] << 64);
			tmp = tmp | ((ap_uint<128>)unbound_hog_w1[index+3] << 96);
			bound_hog_w1[i].weightval[j] = tmp;
		}
	}
	for(int i = 0; i < 7; i++){
		for(int j = 0; j < 9; j++){
			int index = (i*9+j)*4;
			ap_uint<128> tmp = 0;
			tmp = tmp | ((ap_uint<128>)unbound_hog_w2[index]);
			tmp = tmp | ((ap_uint<128>)unbound_hog_w2[index+1] << 32);
			tmp = tmp | ((ap_uint<128>)unbound_hog_w2[index+2] << 64);
			tmp = tmp | ((ap_uint<128>)unbound_hog_w2[index+3] << 96);
			bound_hog_w2[i].weightval[j] = tmp;
		}
	}
	for(int i = 0; i < 7; i++){
		for(int j = 0; j < 9; j++){
			int index = (i*9+j)*4;
			ap_uint<128> tmp = 0;
			tmp = tmp | ((ap_uint<128>)unbound_hog_w3[index]);
			tmp = tmp | ((ap_uint<128>)unbound_hog_w3[index+1] << 32);
			tmp = tmp | ((ap_uint<128>)unbound_hog_w3[index+2] << 64);
			tmp = tmp | ((ap_uint<128>)unbound_hog_w3[index+3] << 96);
			bound_hog_w3[i].weightval[j] = tmp;
		}
	}
	for(int i = 0; i < 8; i++){
		for(int j = 0; j < 3; j++){
			int index = (i*3+j)*4;
			ap_uint<128> tmp = 0;
			tmp = tmp | ((ap_uint<128>)unbound_bgrhsv_w1[index]);
			tmp = tmp | ((ap_uint<128>)unbound_bgrhsv_w1[index+1] << 32);
			tmp = tmp | ((ap_uint<128>)unbound_bgrhsv_w1[index+2] << 64);
			tmp = tmp | ((ap_uint<128>)unbound_bgrhsv_w1[index+3] << 96);
			bound_bgrhsv_w1[i].weight[j] = tmp;
		}
	}
	for(int i = 0; i < 8; i++){
		for(int j = 0; j < 3; j++){
			int index = (i*3+j)*4;
			ap_uint<128> tmp = 0;
			//bbgr ubgr bhsv uhsv
			tmp = tmp | ((ap_uint<128>)unbound_bgrhsv_w2[index]);//uhsv
			tmp = tmp | ((ap_uint<128>)unbound_bgrhsv_w2[index+1] << 32);//bhsv
			tmp = tmp | ((ap_uint<128>)unbound_bgrhsv_w2[index+2] << 64);//ubgr
			tmp = tmp | ((ap_uint<128>)unbound_bgrhsv_w2[index+3] << 96);//bbgr
			bound_bgrhsv_w2[i].weight[j] = tmp;
		}
	}

	for(int i = 0; i < 8; i++){
		for(int j = 0; j < 3; j++){
			int index = (i*3+j)*4;
			ap_uint<128> tmp = 0;
			tmp = tmp | ((ap_uint<128>)unbound_bgrhsv_w3[index]);
			tmp = tmp | ((ap_uint<128>)unbound_bgrhsv_w3[index+1] << 32);
			tmp = tmp | ((ap_uint<128>)unbound_bgrhsv_w3[index+2] << 64);
			tmp = tmp | ((ap_uint<128>)unbound_bgrhsv_w3[index+3] << 96);
			bound_bgrhsv_w3[i].weight[j] = tmp;
		}
	}
}
int main(){
	prepare_bound_input();

	cv::Mat img = cv::imread("frame.png");
	cv::Mat bgra_img;
	cv::cvtColor(img, bgra_img, CV_BGR2BGRA);
	cv::Mat frame_copy = img.clone();
	hls::stream<ap_axiu<32,1,1,1> > instream;

	//Input Value Preparation
	for(int y = 0; y < 240; y++){
		for(int x = 0; x < 320; x++){
			cv::Vec<unsigned char, 4> pix = bgra_img.ptr<cv::Vec4b>(y)[x];
			int pix_int[3] = {pix[0], pix[1], pix[2]};
			ap_axiu<32,1,1,1> in;
			in.data = (pix_int[0]| (pix_int[1] << 8) | pix_int[2] << 16);
			instream << in;
		}
	}

	//Execute HW
	hls::stream<ap_axiu<32,1,1,1>> resultstream;
	double thresh = 0.70;
	ap_fixed_point bias = -1.8334937;
	hog_svm(instream, resultstream, bound_bgrhsv_w1, bound_bgrhsv_w2, bound_bgrhsv_w3, bound_bgrhsv_w4, bound_hog_w1, bound_hog_w2, bound_hog_w3, bias);
	int cnt = 0;
	while(!resultstream.empty()){
		int y = (cnt / 33) * 8;
		int x = (cnt % 33) * 8;
		ap_uint<32> data = resultstream.read().data;
		union{
			int ival;
			float oval;
		}converter;
		converter.ival = data;
		float rst = converter.oval;
		float proba = 1.0/(1.0 + exp(-1 * rst));
		if(proba > 0.70){
			cout << fixed << setprecision(10) << rst << " " << proba*100.0 << endl;
			rectangle(frame_copy, Point(x, y), Point(x + 64, y + 32), Scalar(0,0,200), 2); //x,y //Scaler = B,G,R
			cv::putText(frame_copy, tostr(proba * 100.0), cv::Point(x,y-5), 5, 0.5, cv::Scalar(255.0,0,0), 1, CV_AA, false);
		}
		cnt++;
	}
	imwrite("result.png", frame_copy);
	//imshow("result", frame_copy);
	bool err = false;
	if(!err) return 0;
	else return -1;
}
