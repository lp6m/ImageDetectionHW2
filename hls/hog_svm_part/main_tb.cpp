#include "/opt/Xilinx/Vivado/2018.2/include/gmp.h"
#include <iostream>
#include <cmath>
#include <string.h>
#include <vector>
#include <ap_fixed.h>
#include <ap_axi_sdata.h>
#include <opencv2/opencv.hpp>
#include <hls_stream.h>

#include <sstream>

using namespace std;
using namespace cv;

void hog_svm_part(hls::stream<ap_axiu<32,1,1,1> >& instream, hls::stream<ap_axiu<32,1,1,1> >& outstream);

string tostr(double val){
	std::stringstream ss;
	ss << val;
	std::string str = ss.str();
	return str;
}
void ap_fixed_playground(){
//test ap_fixed_behavior
	ap_fixed<24,20> a = 28087;
	ap_fixed<24,20> b = 65536;

	ap_fixed<64,20> aa = 28087;
	ap_fixed<64,20> bb = 65536;

	const ap_fixed<64,20> eps2 = 1e-10;

	ap_fixed<32,5> c = 13.0;
	ap_fixed<32,5> cdiv = (13.0 / 256.0);
	ap_fixed<32,5> cdiv2 = (c >> 8);
	cout << fixed << setprecision(10) << cdiv << endl;
	cout << fixed << setprecision(10) << cdiv2 << endl;

	ap_fixed<32,10> test = 0.0057410165;
	cout << (test << 2) << endl;
}
int main(){
	//ap_fixed_playground();

	cv::Mat img = cv::imread("frame.png");
	cv::Mat bgra_img;
	cv::cvtColor(img, bgra_img, CV_BGR2BGRA);
	cv::Mat frame_copy = img.clone();
	//hls::stream<bgr> instream;
	hls::stream<ap_axiu<32,1,1,1> > instream;

	//Input Value Preparation
	for(int y = 0; y < 240; y++){
		for(int x = 0; x < 320; x++){
			cv::Vec<unsigned char, 4> pix = bgra_img.ptr<cv::Vec4b>(y)[x];
			//bgr in = bgr{pix[0], pix[1], pix[2]};
			//instream << in;
			int pix_int[3] = {pix[0], pix[1], pix[2]};
			ap_axiu<32,1,1,1> in;
			in.data = (pix_int[0]| (pix_int[1] << 8) | pix_int[2] << 16);
			instream << in;
		}
	}

	//Execute HW
	hls::stream<ap_axiu<32,1,1,1>> resultstream;
	//hls::stream<ap_int<8> > ystream;
	//hls::stream<ap_int<8> > xstream;
	double thresh = 0.70;
	hog_svm_part(instream, resultstream);
	while(!resultstream.empty()){
		ap_uint<32> data = resultstream.read().data;
				union{
					int ival;
					float oval;
				}converter;
				converter.ival = data;
				float rst = converter.oval;
				float proba = 1.0/(1.0 + exp(-1 * rst));
				if(proba > 0.80) cout << fixed << setprecision(10) << rst << " " << proba*100.0 << endl;
	}

	bool err = false;
	if(!err) return 0;
	else return -1;
}
