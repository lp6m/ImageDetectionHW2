#include <vector>
//#include "consts.h"
using namespace std;

//void lite_hog(cv::Mat img, double* dst, bool approx_mode, int descriptor[HISTOGRAMSIZE]);
extern void cell_histogram_host(hls::stream<ap_axis<32,1,1,1> >& magstream, hls::stream<ap_axis<32,1,1,1> >& binstream);

extern vector<int> maglist;
extern vector<int> binlist;

extern vector<int> upper_sw_rst;
extern vector<int> bottom_sw_rst;
