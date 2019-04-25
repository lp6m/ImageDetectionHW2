#include <iostream>
#include <cmath>
#include <string.h>
#include <vector>

#include <ap_axi_sdata.h>
#include <opencv2/opencv.hpp>
#include <hls_stream.h>
#include <hls_video.h>

#include "consts.h"
#include "hog_host.h"

using namespace std;
using namespace cv;

typedef hls::stream<ap_axis<32,1,1,1> > block_out;
void compute_mag_and_bin(hls::stream<ap_axis<8,1,1,1> >& instream, hls::stream<ap_axis<32,1,1,1> >& magstream, hls::stream<ap_axis<32,1,1,1> >& binstream);
void cell_histogram_generate(hls::stream<ap_axis<32,1,1,1> >& magstream, hls::stream<ap_axis<32,1,1,1> >& binstream,
		block_out& bottom0, block_out& bottom1, block_out& bottom2, block_out& bottom3, block_out& bottom4, block_out& bottom5, block_out& bottom6, block_out& bottom7, block_out& bottom8,
		block_out& upper0, block_out& upper1, block_out& upper2, block_out& upper3, block_out& upper4, block_out& upper5, block_out& upper6, block_out& upper7, block_out& upper8);

int main(){

	cv::Mat img = cv::imread("frame.png");
	cv::Mat resized_img;
	cv::resize(img, resized_img, cv::Size(64, 32));
	cv::Mat gray;
	cv::cvtColor(resized_img, gray, CV_RGB2GRAY);

	//unsigned char image_buffer[32*64];
	hls::stream<ap_axis<8,1,1,1> > instream, instream_host;
	unsigned char image_buffer2[32][64];
	double sw_ans[HISTOGRAMSIZE] = {0};
	int sw_unnormalized_descriptor[HISTOGRAMSIZE];
	unsigned short hw_ans[HISTOGRAMSIZE]={0};

	//Input Value Preparation
	ap_axis<8,1,1,1> in, in2;
	for(int y = 0; y < 480; y++){
		for(int x = 0; x < 640; x++){
			in.data = gray.ptr<uchar>(y)[x];
			in2.data = gray.ptr<uchar>(y)[x];
			instream << in;
			instream_host << in2;
		}
	}

	//Running SW and HW implementation
	hls::stream<ap_axis<32,1,1,1> > magstream, binstream, magstream_host, binstream_host;

	//magnitude and binindex are generated from HW implementation
	compute_mag_and_bin(instream, magstream, binstream);
	compute_mag_and_bin(instream_host, magstream_host, binstream_host);

	hls::stream<ap_axis<32,1,1,1> > bottom0, bottom1, bottom2, bottom3, bottom4, bottom5, bottom6, bottom7, bottom8, upper0, upper1, upper2, upper3, upper4, upper5, upper6, upper7, upper8;
	//cell histogram generation by HW implementation
	cell_histogram_generate(magstream, binstream, bottom0, bottom1, bottom2, bottom3, bottom4, bottom5, bottom6, bottom7, bottom8, upper0, upper1, upper2, upper3, upper4, upper5, upper6, upper7, upper8);
	//cell histgoram generation by SW implementation
	cell_histogram_host(magstream_host, binstream_host);

	//validation
	bool err = false;
	for(int i = 0; i < 59*80*9; i++){
		int hw_bottom, hw_upper;
		if(i % 9 == 0) hw_bottom = bottom0.read().data;
		if(i % 9 == 1) hw_bottom = bottom1.read().data;
		if(i % 9 == 2) hw_bottom = bottom2.read().data;
		if(i % 9 == 3) hw_bottom = bottom3.read().data;
		if(i % 9 == 4) hw_bottom = bottom4.read().data;
		if(i % 9 == 5) hw_bottom = bottom5.read().data;
		if(i % 9 == 6) hw_bottom = bottom6.read().data;
		if(i % 9 == 7) hw_bottom = bottom7.read().data;
		if(i % 9 == 8) hw_bottom = bottom8.read().data;
		if(i % 9 == 0) hw_upper = upper0.read().data;
		if(i % 9 == 1) hw_upper = upper1.read().data;
		if(i % 9 == 2) hw_upper = upper2.read().data;
		if(i % 9 == 3) hw_upper = upper3.read().data;
		if(i % 9 == 4) hw_upper = upper4.read().data;
		if(i % 9 == 5) hw_upper = upper5.read().data;
		if(i % 9 == 6) hw_upper = upper6.read().data;
		if(i % 9 == 7) hw_upper = upper7.read().data;
		if(i % 9 == 8) hw_upper = upper8.read().data;

		if(upper_sw_rst[i] != hw_upper || bottom_sw_rst[i] != hw_bottom){
			cout << upper_sw_rst[i] << " " << hw_upper << " " << bottom_sw_rst[i] << " " << hw_bottom << endl;
			err = true;
		}
	}
	while(!bottom0.empty()){
		int hw_bottom = bottom0.read().data;
		cout << "un" << endl;
	}

	if(!err) return 0;
	else return -1;
}
