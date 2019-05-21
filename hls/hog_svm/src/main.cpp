#include "hls_video.h"
#include <iostream>
#include <string.h>
#include <hls_stream.h>
#include <hls_math.h>
#include <ap_axi_sdata.h>
#include <ap_fixed.h>
#include <iostream>
#include <iomanip>
#include "consts.h"

using namespace std;

#define IMAGE_WIDTH 640
#define IMAGE_HEIGHT 480
#define WINDOW_BLOCKNUM_W 7
#define WINDOW_BLOCKNUM_H 3
#define CELL_SIZE 8
#define BLOCK_SIZE 2
#define HIST_BIN_NUM 9
#define WINDOW_NUM_W IMAGE_WIDTH / CELL_SIZE - WINDOW_BLOCKNUM_W
#define BLOCK_NUM_W IMAGE_WIDTH / CELL_SIZE - 1
#define BLOCK_NUM_H IMAGE_HEIGHT / CELL_SIZE - 1

inline int approx_distance(int dx, int dy){
	int min, max; //uint
	if(dx < 0) dx = -dx;
	if(dy < 0) dy = -dy;

	if(dx < dy){
		min = dx;
		max = dy;
	}else{
		min = dy;
		max = dx;
	}

	//coefficients equivalent to (123/128 * max) and (51/128*min)
	return ((( max << 8 ) + ( max << 3 ) - ( max << 4 ) - ( max << 1 ) +
	            ( min << 7 ) - ( min << 5 ) + ( min << 3 ) - ( min << 1 )) >> 8 );
}

struct bgr{
	unsigned char b,g,r;
};
void grayscale_and_resizing(hls::stream<bgr>& bgr_in, hls::stream<bgr>&upper_scaled_rgb, hls::stream<bgr>&bottom_scaled_rgb, hls::stream<ap_uint<8> >& gray_pix){
	//scaleBuffer[i][j]
	//i indicates the kind of location in (8,8) cell
	//  0(1,3) 1(1,4) 2(2,3) 3(2,5) 4(5,3) 5(5,4) 6(6,3)
	//j indicates the cell x index
	bgr scaleBuffer[7][IMAGE_WIDTH/8];

#pragma HLS ARRAY_PARTITION variable=scaleBuffer complete dim=1
	for(int y = 0; y < IMAGE_HEIGHT / 8; y++){
		for(int yy = 0; yy < 8; yy++){
			for(int x = 0; x < IMAGE_WIDTH / 8; x++){
				for(int xx = 0; xx < 8; xx++){
#pragma HLS PIPELINE II=1
					bgr pix = bgr_in.read();
					unsigned char gray = ((int)(299 * (int)pix.r + 587 * (int)pix.g + 114 * (int)pix.b) / 1000);
					gray_pix.write(gray);
					int yy2 = yy%4;
					if((yy2 == 1 || yy2 == 2) && (xx == 3 || xx == 4)){
						if(y == 6 && x == 4){
							//generate upper and bottom output
							bgr pix1, pix2, pix3, pix4, pix5, pix6, pix7, pix8;
							pix1 = scaleBuffer[0][x];
							pix2 = scaleBuffer[1][x];
							pix3 = scaleBuffer[2][x];
							pix4 = scaleBuffer[3][x];
							pix5 = scaleBuffer[4][x];
							pix6 = scaleBuffer[5][x];
							pix7 = scaleBuffer[6][x];
							pix8 = pix;
							int u_bsum = (int)pix1.b + (int)pix2.b + (int)pix3.b + (int)pix4.b;
							int u_gsum = (int)pix1.g + (int)pix2.g + (int)pix3.g + (int)pix4.g;
							int u_rsum = (int)pix1.r + (int)pix2.r + (int)pix3.r + (int)pix4.r;
							int b_bsum = (int)pix5.b + (int)pix6.b + (int)pix7.b + (int)pix8.b;
							int b_gsum = (int)pix5.g + (int)pix6.g + (int)pix7.g + (int)pix8.g;
							int b_rsum = (int)pix5.r + (int)pix6.r + (int)pix7.r + (int)pix8.r;
							bgr u_rst, b_rst;
							u_rst.b = u_bsum / 4 + (((u_bsum % 2) > 0) ? 1 : 0);//round
							u_rst.g = u_gsum / 4 + (((u_gsum % 2) > 0) ? 1 : 0);
							u_rst.r = u_rsum / 4 + (((u_rsum % 2) > 0) ? 1 : 0);
							b_rst.b = u_bsum / 4 + (((u_bsum % 2) > 0) ? 1 : 0);
							b_rst.g = u_gsum / 4 + (((u_gsum % 2) > 0) ? 1 : 0);
							b_rst.r = u_rsum / 4 + (((u_rsum % 2) > 0) ? 1 : 0);
							upper_scaled_rgb.write(u_rst);
							bottom_scaled_rgb.write(b_rst);
						}else{
							int bufindex = 4 * (y / 4) + yy2 * 2 + (xx - 3);
							scaleBuffer[bufindex][x] = pix;
						}
					}
				}
			}
		}
	}
}

void bgr2hsv(unsigned char bb, unsigned char gg, unsigned char rr, unsigned char* h, unsigned char* s, unsigned char* v){
	int b = (int)bb;
	int g = (int)gg;
	int r = (int)rr;
	int pix0_max = max(max(b, g), r);
	int pix0_min = min(min(b, g), r);
	int pix0_V = pix0_max;
	int pix0_S;
	int pix0_2H;
	if(pix0_max == pix0_min){
		pix0_2H = 0;
		pix0_S = 0;
	}else{
		if(pix0_max == r) pix0_2H = 60 * (g - b) / (pix0_V - pix0_min);
		else if(pix0_max == g) pix0_2H = 60 * (b - r) / (pix0_V - pix0_min) + 120;
		else pix0_2H = 60 * (r - g) / (pix0_V - pix0_min) + 240;

		if(pix0_2H > 360) pix0_2H = pix0_2H - 360;
		else if(pix0_2H  < 0) pix0_2H = pix0_2H + 360;

		pix0_S = (pix0_max - pix0_min) * 255 / pix0_max;
	}
	*h = pix0_2H / 2;
	*s = pix0_S;
	*v = pix0_V;
}

struct pixweight{
	ap_fixed_float upper_bgrweight[3];
	ap_fixed_float bottom_bgrweight[3];
	ap_fixed_float upper_hsvweight[3];
	ap_fixed_float bottom_hsvweight[3];
};

ap_fixed_float multiply_accum_bgr(ap_fixed_float weights[3], bgr features){
	ap_fixed_float bb = ap_fixed<64,20>(features.b / 255.0);
	ap_fixed_float gg = ap_fixed<64,20>(features.g / 255.0);
	ap_fixed_float rr = ap_fixed<64,20>(features.r / 255.0);
	return weights[0] * bb + weights[1] * gg + weights[2] * rr;
}
void bgr_hsv_svm_classification(hls::stream<bgr>& upper_scaled_in, hls::stream<bgr>& bottom_scaled_in, hls::stream<ap_fixed_float>& resultstream){
	ap_fixed_float PartialSum[4][80 - 8 + 1];
	const pixweight WeightData[4][8] = {
	{{{0.13471201, 0.083322162, 0.035062906}, {0.13471201, 0.083322162, 0.035062906}, {0.050302028, 0.041768948, -0.018623708}, {0.050302028, 0.041768948, -0.018623708}},
	{{0.12138333, -0.021585074, 0.026402816}, {0.12138333, -0.021585074, 0.026402816}, {0.023603471, 0.033578816, 0.0098683894}, {0.023603471, 0.033578816, 0.0098683894}},
	{{0.064547878, -0.036325294, -0.051299207}, {0.064547878, -0.036325294, -0.051299207}, {-0.015845041, -0.047933505, -0.029382994}, {-0.015845041, -0.047933505, -0.029382994}},
	{{-0.082272872, -0.037619732, -0.0032339732}, {-0.082272872, -0.037619732, -0.0032339732}, {-0.0045304722, -0.014916845, -0.0088652933}, {-0.0045304722, -0.014916845, -0.0088652933}},
	{{0.060039154, -0.15293596, -0.080378009}, {0.060039154, -0.15293596, -0.080378009}, {0.030134322, -0.011468256, 0.021366325}, {0.030134322, -0.011468256, 0.021366325}},
	{{0.061231119, 0.054127958, 0.030798207}, {0.061231119, 0.054127958, 0.030798207}, {0.042379168, 0.026543052, 0.10076567}, {0.042379168, 0.026543052, 0.10076567}},
	{{0.0029276586, 0.017786769, -0.01837371}, {0.0029276586, 0.017786769, -0.01837371}, {-0.16910963, -0.19076954, 0.051312153}, {-0.16910963, -0.19076954, 0.051312153}},
	{{-0.010471252, -0.10217662, -0.014071269}, {-0.010471252, -0.10217662, -0.014071269}, {-0.067393744, -0.096034793, -0.0059219049}, {-0.067393744, -0.096034793, -0.0059219049}}}
	,
	{{{-0.053935112, 0.0091040652, -0.031652548}, {-0.053935112, 0.0091040652, -0.031652548}, {-0.02003568, -0.020670034, -0.022160591}, {-0.02003568, -0.020670034, -0.022160591}},
	{{-0.020781729, -0.24581708, -0.071282291}, {-0.020781729, -0.24581708, -0.071282291}, {-0.097039813, -0.13357548, 0.084337575}, {-0.097039813, -0.13357548, 0.084337575}},
	{{-0.084402904, -0.11817464, 0.039444177}, {-0.084402904, -0.11817464, 0.039444177}, {-0.033581, -0.049768229, 0.18229063}, {-0.033581, -0.049768229, 0.18229063}},
	{{0.13687052, -0.13877358, 0.035004109}, {0.13687052, -0.13877358, 0.035004109}, {0.035067667, -0.069998808, 0.11097779}, {0.035067667, -0.069998808, 0.11097779}},
	{{0.15890596, -0.12908746, -0.036374915}, {0.15890596, -0.12908746, -0.036374915}, {-0.077920979, -0.32958032, -0.0065736498}, {-0.077920979, -0.32958032, -0.0065736498}},
	{{-0.1934103, -0.032097639, 0.11303854}, {-0.1934103, -0.032097639, 0.11303854}, {-0.00094657861, -0.043625967, 0.23625573}, {-0.00094657861, -0.043625967, 0.23625573}},
	{{-0.22198444, 0.19131014, 0.17004659}, {-0.22198444, 0.19131014, 0.17004659}, {-0.0033271789, 0.011490043, 0.32489379}, {-0.0033271789, 0.011490043, 0.32489379}},
	{{-0.16449135, 0.10548364, 0.067575418}, {-0.16449135, 0.10548364, 0.067575418}, {-0.13897258, -0.14510974, 0.068374273}, {-0.13897258, -0.14510974, 0.068374273}}}
	,
	{{{0.08470874, -0.03524777, -0.023765044}, {0.08470874, -0.03524777, -0.023765044}, {0.074593713, 0.058966379, 0.059484843}, {0.074593713, 0.058966379, 0.059484843}},
	{{-0.033169333, -0.25499266, -0.13677776}, {-0.033169333, -0.25499266, -0.13677776}, {-0.043610391, -0.068694957, -0.012626139}, {-0.043610391, -0.068694957, -0.012626139}},
	{{-0.13132883, -0.30579938, -0.13154223}, {-0.13132883, -0.30579938, -0.13154223}, {-0.016838394, -0.10008507, -0.022767541}, {-0.016838394, -0.10008507, -0.022767541}},
	{{0.16711428, -0.14871899, 0.043064104}, {0.16711428, -0.14871899, 0.043064104}, {-0.00097244005, -0.1720749, -0.049039706}, {-0.00097244005, -0.1720749, -0.049039706}},
	{{-0.028743729, -0.091273505, 0.034011042}, {-0.028743729, -0.091273505, 0.034011042}, {-0.11783557, -0.38750312, 0.064519567}, {-0.11783557, -0.38750312, 0.064519567}},
	{{0.0056306783, -0.014925917, 0.18519255}, {0.0056306783, -0.014925917, 0.18519255}, {-0.058837353, -0.15747198, 0.14471212}, {-0.058837353, -0.15747198, 0.14471212}},
	{{-0.10777391, 0.17725541, 0.23888486}, {-0.10777391, 0.17725541, 0.23888486}, {0.059702007, 0.016759473, 0.18784607}, {0.059702007, 0.016759473, 0.18784607}},
	{{-0.17712807, 0.087059749, 0.083511978}, {-0.17712807, 0.087059749, 0.083511978}, {0.0092453054, -0.070892581, 0.0043148652}, {0.0092453054, -0.070892581, 0.0043148652}}}
	,
	{{{0.13409783, 0.13689984, 0.0075472259}, {0.13409783, 0.13689984, 0.0075472259}, {0.093046917, 0.062794018, 0.027703842}, {0.093046917, 0.062794018, 0.027703842}},
	{{-0.041404445, -0.061232684, -0.09921005}, {-0.041404445, -0.061232684, -0.09921005}, {0.079139321, 0.047343812, 0.0083861466}, {0.079139321, 0.047343812, 0.0083861466}},
	{{0.026515395, -0.042265334, 0.014214927}, {0.026515395, -0.042265334, 0.014214927}, {0.091222795, 0.054607177, 0.014804954}, {0.091222795, 0.054607177, 0.014804954}},
	{{0.076428341, -0.098295236, 0.05998011}, {0.076428341, -0.098295236, 0.05998011}, {0.0068872233, -0.043541468, -0.084461526}, {0.0068872233, -0.043541468, -0.084461526}},
	{{0.0048971584, 0.088781691, 0.15474196}, {0.0048971584, 0.088781691, 0.15474196}, {-0.0022159363, -0.07038476, -0.026038157}, {-0.0022159363, -0.07038476, -0.026038157}},
	{{0.12206222, 0.087455524, 0.11157121}, {0.12206222, 0.087455524, 0.11157121}, {-0.09174724, -0.17449156, 0.023081852}, {-0.09174724, -0.17449156, 0.023081852}},
	{{0.011593439, 0.047574944, 0.080315934}, {0.011593439, 0.047574944, 0.080315934}, {-0.0063195126, -0.077341474, 0.060669956}, {-0.0063195126, -0.077341474, 0.060669956}},
	{{0.2145156, -0.081198541, 0.0079493893}, {0.2145156, -0.081198541, 0.0079493893}, {0.014578249, -0.031849439, -0.057506089}, {0.014578249, -0.031849439, -0.057506089}}}

	};
#pragma HLS RESOURCE variable=WeightData core=ROM_1P_BRAM
#pragma HLS ARRAY_PARTITION variable=WeightData complete dim=1
#pragma HLS ARRAY_PARTITION variable=PartialSum complete dim=1

	for(int y = 0; y < IMAGE_HEIGHT / 8; y++){
		for(int x = 0; x < IMAGE_WIDTH / 8; x++){
			bgr upper_bgr = upper_scaled_in.read();
			bgr bottom_bgr = bottom_scaled_in.read();
			//h->b s->g v->r
			bgr upper_hsv, bottom_hsv;
			bgr2hsv(upper_bgr.b, upper_bgr.g, upper_bgr.r, &upper_hsv.b, &upper_hsv.g, &upper_hsv.r);
			bgr2hsv(bottom_bgr.b, bottom_bgr.g, bottom_bgr.r, &bottom_hsv.b, &bottom_hsv.g, &bottom_hsv.r);

			for(int cell_index_x = 7; cell_index_x >= 0; cell_index_x--){
#pragma HLS PIPELINE II=1
				int winx = x - cell_index_x;
				bool inside_window = (cell_index_x <= x && x <= cell_index_x + 72);
				if(inside_window){
					for(int cell_index_y = 0; cell_index_y < 4; cell_index_y++){
						int cell_start_y = y - cell_index_y;
						if(0 <= cell_start_y && cell_start_y <= (IMAGE_HEIGHT / 8 - 4)){
							int partial_sum_index_y = (y - cell_index_y) % 4;
							pixweight w = WeightData[cell_index_y][cell_index_x];
	//#pragma HLS allocation instances=multiply_accum limit=2
							ap_fixed_float tmp_partial_sum = multiply_accum_bgr(w.upper_bgrweight, upper_bgr) + multiply_accum_bgr(w.bottom_bgrweight, bottom_bgr)
									+ multiply_accum_bgr(w.upper_hsvweight, upper_hsv) + multiply_accum_bgr(w.bottom_hsvweight, bottom_hsv);
							PartialSum[partial_sum_index_y][winx] += tmp_partial_sum;
							bool window_completed = (cell_index_x == 7 && cell_index_y == 3);
							if(window_completed){
								ap_fixed_float allsum = PartialSum[partial_sum_index_y][winx];
								resultstream.write(allsum);
								PartialSum[partial_sum_index_y][winx] = 0;
							}
						}
					}
				}
			}
		}
	}
}
//mag minimum:0 maximum:sqrt(255*255+255*255) < 2^9
//integer bit needs 9 + 1(sign) = 10bit
typedef ap_fixed<18,10,AP_RND> magnitude_fixed;

void compute_mag_and_bin(hls::stream<ap_uint<8> >& instream, hls::stream<magnitude_fixed >& magstream, hls::stream<ap_uint<4> >& binstream){
	//Lookup tables for tan(20)*Gx
	int lut0[256] = {0,0,0,1,1,1,2,2,2,3,3,3,4,4,5,5,5,6,6,6,7,7,7,8,8,9,9,9,10,10,10,11,11,11,12,12,13,13,13,14,14,14,15,15,15,16,16,17,17,17,18,18,18,19,19,19,20,20,21,21,21,22,22,22,23,23,23,24,24,25,25,25,26,26,26,27,27,27,28,28,29,29,29,30,30,30,31,31,31,32,32,33,33,33,34,34,34,35,35,35,36,36,37,37,37,38,38,38,39,39,39,40,40,41,41,41,42,42,42,43,43,43,44,44,45,45,45,46,46,46,47,47,47,48,48,49,49,49,50,50,50,51,51,51,52,52,53,53,53,54,54,54,55,55,55,56,56,57,57,57,58,58,58,59,59,59,60,60,61,61,61,62,62,62,63,63,63,64,64,65,65,65,66,66,66,67,67,67,68,68,69,69,69,70,70,70,71,71,71,72,72,73,73,73,74,74,74,75,75,75,76,76,77,77,77,78,78,78,79,79,79,80,80,81,81,81,82,82,82,83,83,83,84,84,85,85,85,86,86,86,87,87,87,88,88,89,89,89,90,90,90,91,91,91,92,92};
#pragma HLS RESOURCE variable=lut0 core=ROM_1P_BRAM
	//Lookup tables for tan(40)*Gx
	int lut1[256] = {0,0,1,2,3,4,5,5,6,7,8,9,10,10,11,12,13,14,15,15,16,17,18,19,20,20,21,22,23,24,25,26,26,27,28,29,30,31,31,32,33,34,35,36,36,37,38,39,40,41,41,42,43,44,45,46,46,47,48,49,50,51,52,52,53,54,55,56,57,57,58,59,60,61,62,62,63,64,65,66,67,67,68,69,70,71,72,72,73,74,75,76,77,78,78,79,80,81,82,83,83,84,85,86,87,88,88,89,90,91,92,93,93,94,95,96,97,98,98,99,100,101,102,103,104,104,105,106,107,108,109,109,110,111,112,113,114,114,115,116,117,118,119,119,120,121,122,123,124,124,125,126,127,128,129,130,130,131,132,133,134,135,135,136,137,138,139,140,140,141,142,143,144,145,145,146,147,148,149,150,150,151,152,153,154,155,156,156,157,158,159,160,161,161,162,163,164,165,166,166,167,168,169,170,171,171,172,173,174,175,176,177,177,178,179,180,181,182,182,183,184,185,186,187,187,188,189,190,191,192,192,193,194,195,196,197,197,198,199,200,201,202,203,203,204,205,206,207,208,208,209,210,211,212,213,213};
#pragma HLS RESOURCE variable=lut1 core=ROM_1P_BRAM
	//Lookup tables for tan(60)*Gx
	int lut2[256] = {0,1,3,5,6,8,10,12,13,15,17,19,20,22,24,25,27,29,31,32,34,36,38,39,41,43,44,46,48,50,51,53,55,57,58,60,62,63,65,67,69,70,72,74,76,77,79,81,83,84,86,88,89,91,93,95,96,98,100,102,103,105,107,108,110,112,114,115,117,119,121,122,124,126,127,129,131,133,134,136,138,140,141,143,145,147,148,150,152,153,155,157,159,160,162,164,166,167,169,171,172,174,176,178,179,181,183,185,186,188,190,191,193,195,197,198,200,202,204,205,207,209,210,212,214,216,217,219,221,223,224,226,228,230,231,233,235,236,238,240,242,243,245,247,249,250,252,254,255,257,259,261,262,264,266,268,269,271,273,274,276,278,280,281,283,285,287,288,290,292,294,295,297,299,300,302,304,306,307,309,311,313,314,316,318,319,321,323,325,326,328,330,332,333,335,337,338,340,342,344,345,347,349,351,352,354,356,358,359,361,363,364,366,368,370,371,373,375,377,378,380,382,383,385,387,389,390,392,394,396,397,399,401,402,404,406,408,409,411,413,415,416,418,420,421,423,425,427,428,430,432,434,435,437,439,441};
#pragma HLS RESOURCE variable=lut2 core=ROM_1P_BRAM
	//Lookup tables for tan(80)*Gx
	int lut3[256] = {0,5,11,17,22,28,34,39,45,51,56,62,68,73,79,85,90,96,102,107,113,119,124,130,136,141,147,153,158,164,170,175,181,187,192,198,204,209,215,221,226,232,238,243,249,255,260,266,272,277,283,289,294,300,306,311,317,323,328,334,340,345,351,357,362,368,374,379,385,391,396,402,408,413,419,425,430,436,442,447,453,459,464,470,476,481,487,493,498,504,510,515,521,527,532,538,544,549,555,561,566,572,578,584,589,595,601,606,612,618,623,629,635,640,646,652,657,663,669,674,680,686,691,697,703,708,714,720,725,731,737,742,748,754,759,765,771,776,782,788,793,799,805,810,816,822,827,833,839,844,850,856,861,867,873,878,884,890,895,901,907,912,918,924,929,935,941,946,952,958,963,969,975,980,986,992,997,1003,1009,1014,1020,1026,1031,1037,1043,1048,1054,1060,1065,1071,1077,1082,1088,1094,1099,1105,1111,1116,1122,1128,1133,1139,1145,1150,1156,1162,1168,1173,1179,1185,1190,1196,1202,1207,1213,1219,1224,1230,1236,1241,1247,1253,1258,1264,1270,1275,1281,1287,1292,1298,1304,1309,1315,1321,1326,1332,1338,1343,1349,1355,1360,1366,1372,1377,1383,1389,1394,1400,1406,1411,1417,1423,1428,1434,1440,1445};
#pragma HLS RESOURCE variable=lut3 core=ROM_1P_BRAM

	hls::LineBuffer<3, IMAGE_WIDTH, unsigned char> linebuf;
	hls::Window<3, 3, unsigned char> winbuf;

	//calculate magnitude using shift register
	loop_y:for(int y = 0; y < IMAGE_HEIGHT; y++){
		loop_x:for(int x = 0; x < IMAGE_WIDTH; x++){
#pragma HLS PIPELINE II=1
			bool isedge = (x < 2 || y < 2);

			unsigned char indata = instream.read();
			//these operation will be executed in parallel.
			linebuf.shift_pixels_up(x);
			linebuf.insert_bottom_row(indata, x);
			winbuf.shift_pixels_right();
			winbuf.insert_pixel(linebuf.getval(0, x), 0, 0);
			winbuf.insert_pixel(linebuf.getval(1, x), 1, 0);
			winbuf.insert_pixel(linebuf.getval(2, x), 2, 0);

			int Gx = isedge ? 0 : (int)winbuf.getval(1, 0) - (int)winbuf.getval(1, 2);
			int Gy = isedge ? 0 : (int)winbuf.getval(2, 1) - (int)winbuf.getval(0, 1);
			int square_sum = Gx * Gx + Gy * Gy;
			magnitude_fixed mag = (isedge||square_sum==0) ? 0 : approx_distance(Gx, Gy);//hls::sqrt(square_sum);
			int bin_index;
			if((Gx >= 0 && Gy >= 0) || (Gx <= 0 && Gy <= 0)){
				if 		(abs(Gy) < lut0[abs(Gx)]) 	{ bin_index = 0;
				}else if (abs(Gy) < lut1[abs(Gx)]) 	{ bin_index = 1;
				}else if (abs(Gy) < lut2[abs(Gx)]) 	{ bin_index = 2;
				}else if (abs(Gy) < lut3[abs(Gx)]) 	{ bin_index = 3;
				}else{ 								  bin_index = 4;
				}
			} else{
				if 		(abs(Gy) <  lut0[abs(Gx)])	{ bin_index = 4;
				}else if (abs(Gy) <  lut1[abs(Gx)]) { bin_index = 5;
				}else if (abs(Gy) <  lut2[abs(Gx)]) { bin_index = 6;
				}else if (abs(Gy) <  lut3[abs(Gx)]) { bin_index = 7;
				}else 								{ bin_index = 8;
				}
			}
			magstream.write(mag);
			binstream.write(bin_index);
		}
	}
}

struct ap_fixed9_float{
	ap_fixed_float data[9];
};

//bottom,upper minimum:0 maximum:sqrt(255*255+255*255)*8*8 < 2^15
//integer bit needs 15 + 1(sign) = 16bit
typedef ap_fixed<24,16,AP_RND> blockpart_fixed;
//8pix * 8pix = cellsize part of block
struct blockpart_fixed_9{
	blockpart_fixed data[HIST_BIN_NUM];
};

void cell_histogram_generate(hls::stream<magnitude_fixed>& magstream, hls::stream<ap_uint<4> >& binstream,
		hls::stream<blockpart_fixed_9>& bottom, hls::stream<blockpart_fixed_9>& upper){

	hls::LineBuffer<CELL_SIZE, IMAGE_WIDTH/CELL_SIZE, blockpart_fixed > linebufs[HIST_BIN_NUM];
	hls::LineBuffer<2, IMAGE_WIDTH/CELL_SIZE, blockpart_fixed > cellbuf[HIST_BIN_NUM];
#pragma HLS ARRAY_PARTITION variable=linebufs complete dim=1
#pragma HLS ARRAY PARTITION variable=cellbuf complete dim=1
	blockpart_fixed vote_counter[HIST_BIN_NUM];
	for(int i = 0; i < HIST_BIN_NUM; i++) vote_counter[i] = 0;
#pragma HLS ARRAY_PARTITION variable=vote_counter complete dim=1
	loop_y:for(int y = 0; y < IMAGE_HEIGHT; y++){//480
		loop_winx:for(int winx = 0; winx < IMAGE_WIDTH / CELL_SIZE; winx++){ //80
			loop_cell_index:for(int cell_index = 0; cell_index < CELL_SIZE; cell_index++){ //8
#pragma HLS PIPELINE II=1
				magnitude_fixed mag = magstream.read();
				ap_uint<4> bin = binstream.read();
				vote_counter[(int)bin] += mag;
				if(cell_index == (CELL_SIZE - 1)){
					loop_updatelinebuf:for(int i = 0; i < HIST_BIN_NUM; i++){
						linebufs[i].shift_pixels_up(winx);
						linebufs[i].insert_bottom_row(vote_counter[i], winx);
					}
					if(y%CELL_SIZE == (CELL_SIZE - 1)){
						blockpart_fixed_9 out_upper, out_bottom;
						loop_cellbuf_calc:for(int bin_index = 0; bin_index < HIST_BIN_NUM; bin_index++){
							blockpart_fixed sum_of_cell = linebufs[bin_index].getval(0, winx) + linebufs[bin_index].getval(1, winx) + linebufs[bin_index].getval(2, winx) + linebufs[bin_index].getval(3, winx) + linebufs[bin_index].getval(4, winx) + linebufs[bin_index].getval(5, winx) + linebufs[bin_index].getval(6, winx) + linebufs[bin_index].getval(7, winx);
							cellbuf[bin_index].shift_pixels_up(winx);
							cellbuf[bin_index].insert_bottom_row(sum_of_cell, winx);
							if(y >= CELL_SIZE * BLOCK_SIZE - 1){
								out_upper.data[bin_index] = cellbuf[bin_index].getval(0, winx);
								out_bottom.data[bin_index] = cellbuf[bin_index].getval(1, winx);
								if(bin_index == 8){
									bottom << out_bottom;
									upper << out_upper;
								}
							}
						}
					}
					//zeroing
					for(int i = 0; i < HIST_BIN_NUM; i++) vote_counter[i] = 0;
				}
			}
		}
	}
}

//bottom,upper minimum:0 maximum:sqrt(255*255+255*255)*16*16 < 2^17
//integer bit needs 17 + 1(sign) = 18bit
typedef ap_fixed<24,18,AP_RND> blocksum_fixed;

inline blocksum_fixed myabs(blocksum_fixed input){
	return ((input > 0) ? (blocksum_fixed)input : (blocksum_fixed)(-input));
}

void block_histogram_normalization(hls::stream<blockpart_fixed_9>& bottom, hls::stream<blockpart_fixed_9>& upper,
		hls::stream<ap_fixed9_float>& ul_out, hls::stream<ap_fixed9_float>& ur_out, hls::stream<ap_fixed9_float>& bl_out, hls::stream<ap_fixed9_float>& br_out){

	hls::LineBuffer<2, 1, blockpart_fixed> bottomfifo[HIST_BIN_NUM], upperfifo[HIST_BIN_NUM];
	blocksum_fixed  partial_old_block_sum;
#pragma HLS ARRAY_PARTITION variable=bottomfifo complete dim=1
#pragma HLS ARRAY PARTITION variable=upperfifo complete dim=1
	for(int y = 0; y < (IMAGE_HEIGHT / CELL_SIZE - BLOCK_SIZE + 1); y++){//59
		for(int x = 0; x < (IMAGE_WIDTH / CELL_SIZE); x++){//80
			blockpart_fixed_9 bottom_in = bottom.read();
			blockpart_fixed_9 upper_in = upper.read();
			ap_fixed9_float ul, ur, bl, br;

			blocksum_fixed partial_block_new_sum = 0;

			for(int bin_index = 0; bin_index < HIST_BIN_NUM; bin_index++){
#pragma HLS PIPELINE II=1
				blockpart_fixed b = bottom_in.data[bin_index];
				blockpart_fixed u = upper_in.data[bin_index];
				bottomfifo[bin_index].shift_pixels_up(0);
				bottomfifo[bin_index].insert_bottom_row(b, 0);
				upperfifo[bin_index].shift_pixels_up(0);
				upperfifo[bin_index].insert_bottom_row(u, 0);

				partial_block_new_sum += (b + u);
			}
			bool sum_of_block_completed = (x >= 1);
			if(sum_of_block_completed){
				blocksum_fixed block_sum = partial_block_new_sum + partial_old_block_sum;
				for(int bin_index = 0; bin_index < HIST_BIN_NUM; bin_index++){
	#pragma HLS PIPELINE II=1
					//59*79
					//normalization
					blockpart_fixed un_upperleft = upperfifo[bin_index].getval(0, 0);
					blockpart_fixed un_upperright = upperfifo[bin_index].getval(1, 0);
					blockpart_fixed un_bottomleft = bottomfifo[bin_index].getval(0, 0);
					blockpart_fixed un_bottomright = bottomfifo[bin_index].getval(1, 0);
					ap_fixed_float zero = 0;
					const ap_fixed<32,2> eps = 1e-10;
					const ap_fixed<64,20> eps2 = 1e-10;
					ap_fixed_float upperleft = zero;
					ap_fixed_float upperright = zero;
					ap_fixed_float bottomleft = zero;
					ap_fixed_float bottomright = zero;
#pragma HLS allocation instances=hls::sqrt limit=2
					if(un_upperleft > eps && block_sum > eps){
						ap_fixed<32,2> sqrt_target = (ap_fixed<64,20>)un_upperleft/(block_sum + eps2);
						upperleft = (ap_fixed_float)hls::sqrt(sqrt_target);
					}
					if(un_upperright > eps && block_sum > eps){
						ap_fixed<32,2> sqrt_target = (ap_fixed<64,20>)un_upperright/(block_sum + eps2);
						upperright = (ap_fixed_float)hls::sqrt(sqrt_target);
					}
					if(un_bottomleft > eps && block_sum > eps){
						ap_fixed<32,2> sqrt_target = (ap_fixed<64,20>)un_bottomleft/(block_sum + eps2);
						bottomleft = (ap_fixed_float)hls::sqrt(sqrt_target);
					}
					if(un_bottomright > eps && block_sum > eps){
						ap_fixed<32,2> sqrt_target = (ap_fixed<64,20>)un_bottomright/(block_sum + eps2);
						bottomright = (ap_fixed_float)hls::sqrt(sqrt_target);
					}
					ul.data[bin_index] = upperleft;
					ur.data[bin_index] = upperright;
					bl.data[bin_index] = bottomleft;
					br.data[bin_index] = bottomright;
				}
				ul_out << ul;
				ur_out << ur;
				bl_out << bl;
				br_out << br;

			}
			partial_old_block_sum = partial_block_new_sum;
		}
	}
}
struct histdata{
	ap_fixed_float data[9];
};
struct weight{
	histdata upperleft;
	histdata upperright;
	histdata bottomleft;
	histdata bottomright;
};

ap_fixed_float multiply_accum(histdata weights, ap_fixed9_float features){
	return weights.data[0] * features.data[0] + weights.data[1] * features.data[1] + weights.data[2] * features.data[2]
			+ weights.data[3] * features.data[3] + weights.data[4] * features.data[4] + weights.data[5] * features.data[5]
			+ weights.data[6] * features.data[6] + weights.data[7] * features.data[7] + weights.data[8] * features.data[8];
}
void hog_svm_classification(hls::stream<ap_fixed9_float>& upperleft, hls::stream<ap_fixed9_float>& upperright, hls::stream<ap_fixed9_float>& bottomleft, hls::stream<ap_fixed9_float>& bottomright,
		hls::stream<ap_fixed_float>& resultstream){
	const weight WeightData[WINDOW_BLOCKNUM_H][WINDOW_BLOCKNUM_W] = {
	{{{-0.11592449, 0.1182964, 0.2061952, -0.094230117, 0.18011853, -0.24861195, -0.31082461, -0.066229289, 0.14555717}, {-0.12177213, 0.1570581, -0.032505462, 0.20021335, 0.1206582, 0.054348659, -0.18054019, 0.099335802, 0.12794927}, {0.011623583, 0.11882419, 0.13402278, 0.076370048, -0.037424818, -0.031787151, 0.05145738, -0.002697307, 0.20921361}, {-0.11073844, 0.01213981, 0.19430859, -0.071810858, 0.0053566204, 0.065097385, -0.019606471, -0.043234665, 0.069365788}},
	{{-0.033270435, 0.19400932, 0.017387759, 0.22618451, 0.15820455, 0.057495381, -0.22403611, 0.104304, 0.10486948}, {-0.20971139, -0.32212354, 0.04168051, -0.067712064, -0.091495479, -0.080738092, 0.22655086, 0.17898326, 0.22516093}, {-0.033750991, 0.09249765, 0.28777511, -0.082558502, 0.050255984, 0.0064472982, -0.012990965, -0.025119048, 0.10155549}, {-0.053426633, -0.0043009026, 0.032369867, -0.07787733, 0.1385036, 0.22776495, 0.22142897, -0.03234516, 0.084754989}},
	{{-0.17756311, -0.26771382, 0.10880758, -0.043610169, -0.12460573, -0.11596455, 0.20511605, 0.19153252, 0.2344352}, {-0.049322715, -0.013816101, 0.30123311, 0.098238018, 0.027334136, 0.10921524, 0.036884754, -0.23700685, 0.17841826}, {-0.030869365, 0.0080292138, 0.047850581, -0.081402302, 0.047019091, 0.14645256, 0.12614477, -0.093407975, 0.098645451}, {0.028685553, 0.12183843, 0.089774209, -0.10263262, 0.12472778, 0.17387027, 0.0010053052, 0.066719498, 0.14304681}},
	{{-0.046341315, -0.034698674, 0.25037194, 0.096801329, 0.075023307, 0.097866379, 0.056280716, -0.23822489, 0.22711145}, {-0.25470352, -0.0043274761, 0.1475764, 0.067650486, 0.025088939, 0.0895277, 0.085504781, -0.11874947, 0.13796362}, {0.040475342, 0.15124149, 0.068934462, -0.047095526, 0.075147985, 0.14980075, -0.07385104, 0.0038458234, 0.11651169}, {0.03820062, 0.1504057, -0.066247694, 0.023653421, -0.098901847, 0.091094277, -0.064746702, -0.053473627, -0.010289872}},
	{{-0.20402853, 0.018143997, 0.12489888, 0.060133977, 0.064072119, 0.058608756, 0.071443014, -0.10420338, 0.16876624}, {-0.13852104, 0.031116787, -0.056394875, -0.1212645, 0.16968301, -0.016743024, 0.20346078, -0.17315607, 0.03153855}, {0.10365764, 0.25043992, -0.019321639, 0.086238004, -0.087303171, 0.083726665, -0.078406374, -0.056396723, -0.032906659}, {0.16924041, 0.018518957, 0.16796473, -0.031833464, 0.27287053, 0.18695973, 0.18575349, -0.037919585, 0.032404905}},
	{{-0.11043125, 0.062772848, -0.035236975, -0.11415272, 0.16267688, -0.058802518, 0.16481565, -0.21429743, 0.0089831929}, {-0.27706159, -0.14587425, -0.062903999, -0.011989971, -0.081880523, -0.037077481, 0.059175979, -0.29280887, 0.22868649}, {0.21065956, 0.034593258, 0.22702283, 0.044894871, 0.31647419, 0.13184745, 0.12006566, 0.020647034, 0.073490905}, {0.21542956, 0.086603005, 0.29356806, 0.10166705, 0.17163763, 0.17659553, 0.21793817, 0.069145632, 0.19382333}},
	{{-0.27922656, -0.096164605, -0.011492297, 0.069953521, -0.063638212, -0.07123322, 0.070743985, -0.27656053, 0.21136157}, {-0.49314024, -0.042300013, 0.3987719, -0.013132899, 0.016510116, -0.28521439, -0.13626125, -0.052740663, 0.0051108432}, {0.20346241, 0.13206138, 0.40381235, 0.21629524, 0.18654819, 0.15225299, 0.19957248, 0.090757601, 0.22191681}, {0.22852448, -0.18852693, -0.078566493, -0.20802071, 0.12555475, 0.22277216, 0.41118628, 0.14512701, 0.14367961}}},
	{{{0.044579429, 0.11069598, 0.12760938, 0.10290862, -0.066675714, -0.038146976, 0.010907334, -0.019563273, 0.18207468}, {-0.15240902, 0.047327045, 0.29505887, -0.13217798, -0.011373969, 0.011225929, -0.070696874, -0.082039084, 0.049284433}, {0.074811757, -0.036614263, 0.15708184, -0.076490253, 0.233618, 0.14698478, 0.097043869, 0.12501421, 0.15830543}, {0.14897214, 0.069984224, 0.054192798, 0.096248853, 0.012313457, 0.05908485, 0.18127155, 0.048440144, 0.012773975}},
	{{-0.062561827, 0.12218231, 0.26722583, -0.16642257, -0.012630438, 0.022347511, -0.052208225, -0.058796134, 0.06484724}, {0.040887304, 0.054654873, -0.004689234, -0.19006523, 0.10089168, 0.18608673, 0.16775235, -0.14496722, -0.0098464146}, {0.16359299, 0.072323227, 0.020182567, 0.08272619, 0.11782917, 0.071831522, 0.16863494, 0.1386057, 0.088875622}, {-0.070990287, 0.047628195, -0.03840939, 0.011993042, -0.0083895928, 0.051004934, 0.1068579, -0.21225895, 0.12609878}},
	{{0.093810061, 0.11103108, 0.0688409, -0.13438204, 0.088346813, 0.2083127, 0.18234842, -0.13985956, 0.03962744}, {0.10847143, 0.19840675, 0.11492917, -0.043109632, 0.14800956, 0.16668798, -0.048221908, 0.023015279, 0.1388581}, {-0.10965889, 0.038724309, -0.10743746, -0.034540034, 0.06500578, 0.084231563, 0.23577237, -0.032253555, 0.21582038}, {-0.13228925, -0.090934661, 0.18620389, -0.085093456, 0.056309023, 0.062901664, -0.20222026, 0.085966658, 0.028540048}},
	{{0.22000103, 0.25354488, 0.12966186, 0.055252747, 0.21309721, 0.26738918, -0.031928173, 0.0056788181, 0.16070531}, {0.14165958, 0.25727044, -0.010564129, 0.061951012, 0.0083970062, 0.1352847, 0.014516957, -0.046756229, 0.0084980845}, {-0.10626999, -0.086670025, 0.29568639, -0.016331814, 0.052554672, 0.094193347, -0.14343545, 0.10992583, 0.06746346}, {-0.02977118, 0.23640959, -0.084094683, -0.022456867, -0.046137313, -0.12064784, 0.0069437312, 0.0027500095, 0.16120056}},
	{{0.15779064, 0.27957822, -0.0093961446, 0.046511621, -0.051980856, 0.088774336, -0.071178255, -0.12141282, -0.098715439}, {0.24973519, 0.014921079, 0.17791351, -0.022003804, 0.31523096, 0.22933043, 0.14831697, 0.011705889, 0.013690324}, {-0.055370738, 0.13878398, -0.15200915, -0.082864144, -0.13249547, -0.043751727, 0.00046233218, -0.15234066, 0.050577356}, {0.068596523, 0.011578978, 0.0011043251, 0.0090517282, 0.21748779, 0.13495866, 0.09930035, -0.090354489, 0.15215495}},
	{{0.14451014, -0.20026734, 0.034737105, -0.15123429, 0.18365296, 0.059668555, -0.047036278, -0.079993124, -0.013823159}, {0.20919716, -0.029216715, 0.23516954, 0.025043176, 0.05605364, 0.0086847605, 0.069393715, -0.096355472, 0.13933716}, {0.005840083, -0.15837345, -0.051319259, -0.094430886, 0.039745943, 0.014278065, -0.019797748, -0.22331128, 0.059470367}, {0.1006123, -0.031277112, 0.2754353, -0.27811465, 0.04768145, -0.134654, 0.10674591, 0.13134071, 0.13014969}},
	{{0.22869302, -0.039301469, 0.21588374, 0.021360162, 0.10846941, 0.061234211, 0.10705291, -0.074340336, 0.14166819}, {0.12966969, -0.32814873, -0.091129323, -0.20544338, -0.0025871613, 0.059019296, 0.26206094, -0.017003698, 0.098202179}, {0.1704601, 0.066261076, 0.32287007, -0.26495409, -0.0064942404, -0.174833, -0.014579095, 0.046833495, 0.069886004}, {0.19939697, 0.065798265, 0.058469575, -0.2931888, -0.13443881, -0.21512615, -0.092547958, -0.064141267, -0.19902666}}},
	{{{0.11539914, -0.016138564, 0.12202647, -0.096539418, 0.3019073, 0.16736615, 0.1312227, 0.075984563, 0.19415406}, {0.18168976, 0.01287564, 0.046611026, 0.04748642, 0.027304215, 0.13794976, 0.2198293, 0.058760094, -0.0038417319}, {-0.30253939, -0.29695031, -0.16082153, 0.17212937, -0.19734152, 0.086858548, 0.28022801, 0.004133992, -0.055597559}, {-0.022072788, -0.012364921, 0.16851467, -0.032821561, 0.13592686, -0.0071104258, 0.14396486, -0.0098258573, 0.015577752}},
	{{0.18725773, 0.048212962, 0.071960706, 0.082832388, 0.11172193, 0.15016726, 0.23278672, 0.19054241, 0.086453839}, {0.036786848, 0.16157146, 0.042674974, -0.055943539, -0.018266032, 0.08527395, 0.1821655, -0.23950713, 0.11616048}, {0.00028257458, -0.0010320787, 0.29517542, 0.071288742, 0.20250402, 0.009959885, 0.12459319, 0.048263148, 0.025469543}, {0.13206687, 0.12356242, 0.080103418, -0.24020136, 0.1001829, -0.30751604, 0.0297378, -0.021043765, 0.11754154}},
	{{0.012841696, 0.17548713, 0.047270664, -0.020008671, 0.13895715, 0.20997449, 0.384735, -0.0099389079, 0.27769408}, {-0.0066516487, 0.087046539, 0.40818295, -0.0023725681, 0.1034266, 0.17022448, -0.095242119, 0.08091745, 0.033031528}, {0.19971025, 0.13433623, 0.15285781, -0.10100651, 0.16196728, -0.16462922, 0.18110956, 0.14338389, 0.16992045}, {0.1060539, 0.0057624527, -0.10776361, 0.10677553, 0.079593102, 0.055258178, 5.678708e-06, -0.03986419, 0.27728506}},
	{{0.0041562235, 0.077552474, 0.38455666, 0.058099734, 0.17776652, 0.2389959, -0.012758953, 0.17742698, 0.093823241}, {0.042574916, 0.34859266, -0.04108654, 0.057889751, 0.061263279, 0.051414502, 0.13577414, 0.091593779, 0.20638943}, {0.13212623, 0.068742223, -0.04688282, 0.24342886, 0.11953529, 0.12374953, 0.053028088, 0.031751836, 0.2843291}, {-0.039427695, -0.10233855, -0.46463407, -0.19808517, 0.15879985, -0.046028455, -0.098659666, 0.34429324, 0.34150136}},
	{{-0.032262662, 0.21173491, -0.13619052, -0.07371776, -0.066747189, 0.035209033, 0.08666089, -0.0045365509, 0.15785024}, {0.1495476, 0.19778502, 0.16356276, 0.12055646, 0.27210411, 0.149613, 0.080490789, -0.092089318, 0.15617547}, {-0.023751664, -0.10736633, -0.46603358, -0.28284845, 0.060828962, -0.11070217, -0.19989077, 0.17506338, 0.27994905}, {-0.25128567, -0.17945564, 0.25678048, 0.35129339, 0.21356892, -0.12556178, -0.20645757, 0.1519229, 0.31688105}},
	{{0.062723229, 0.030353401, 0.090875508, 0.039653161, 0.15178509, 0.027115715, 0.017800107, -0.13416678, 0.15821689}, {0.15373497, 0.082854438, 0.33213174, -0.15407718, 0.13382972, -0.091574278, 0.12127016, 0.21273168, 0.18468283}, {-0.2552168, -0.20210543, 0.19281782, 0.19138105, 0.041779978, -0.15424641, -0.24600728, -0.036643669, 0.19345768}, {-0.22468307, -0.38906813, 0.20046161, -0.13792487, -0.049714959, -0.25497507, 0.016836398, -0.072722928, 0.24236252}},
	{{0.26122991, 0.24504713, 0.35939792, -0.051026463, 0.15463105, -0.050092472, 0.11413047, 0.24189677, 0.15587059}, {0.22877413, 0.21274151, 0.19252402, -0.1062988, -0.047991764, -0.1444901, -0.061432939, 0.076043734, -0.090193633}, {-0.11139747, -0.21083, 0.35798376, -0.051648798, -0.047464313, -0.20259483, 0.0060244403, -0.09987777, 0.21099345}, {-0.085035579, 0.12808071, 0.46575593, -0.16074694, -0.089777959, -0.45468041, -0.53123911, -0.54021578, -0.11771554}}}
	};
#pragma HLS ARRAY_PARTITION variable=WeightData complete dim=1
#pragma HLS RESOURCE variable=WeightData core=ROM_1P_BRAM
	ap_fixed_float PartialSum[WINDOW_BLOCKNUM_H][WINDOW_NUM_W];
#pragma HLS ARRAY_PARTITION variable=PartialSum complete dim=1
#pragma HLS RESOURCE variable=PartialSum core=RAM_2P_BRAM

	for(int i = 0; i < WINDOW_BLOCKNUM_H; i++){
		for(int j = 0; j < WINDOW_NUM_W; j++){
#pragma HLS PIPELINE II=1
			PartialSum[i][j] = 0;
		}
	}
	ap_fixed_float bias = -14.786592;
	loop_y:for(int y = 0; y < BLOCK_NUM_H; y++){
		loop_x:for(int x = 0; x < BLOCK_NUM_W; x++){
			ap_fixed9_float ul = upperleft.read();
			ap_fixed9_float ur = upperright.read();
			ap_fixed9_float bl = bottomleft.read();
			ap_fixed9_float br = bottomright.read();
			for(int block_index_x = 6; block_index_x >= 0; block_index_x--){
#pragma HLS PIPELINE II=1
				bool inside_window = (block_index_x <= x && x <= block_index_x + 72);
				if(inside_window){
					int winx = x - block_index_x;
					//block_index_y indicates where (ul,ur,bl,br) is located in the window in y axis.
					loop_block_index_y:for(int block_index_y = 0; block_index_y < WINDOW_BLOCKNUM_H; block_index_y++){
						int block_start_y = y - block_index_y;
						if(0 <= block_start_y && block_start_y <= (BLOCK_NUM_H - WINDOW_BLOCKNUM_H)){
							int partial_sum_index_y = (y - block_index_y) % WINDOW_BLOCKNUM_H;

							weight w = WeightData[block_index_y][block_index_x];
//#pragma HLS allocation instances=multiply_accum limit=2
							ap_fixed_float tmp_partial_sum = multiply_accum(w.upperleft, ul) + multiply_accum(w.upperright, ur)
									+ multiply_accum(w.bottomleft, bl) + multiply_accum(w.bottomright, br);
							PartialSum[partial_sum_index_y][winx] += tmp_partial_sum;

							bool window_completed = (block_index_x == (WINDOW_BLOCKNUM_W - 1) && block_index_y == (WINDOW_BLOCKNUM_H - 1));
							if(window_completed){
								ap_fixed_float allsum = PartialSum[partial_sum_index_y][winx] + bias;
								PartialSum[partial_sum_index_y][winx] = 0;
								//ap_axis<8,1,1,1> ap_y, ap_x;
								//ap_y.data = block_start_y;
								//ap_x.data = winx;
								resultstream.write(allsum);
							}
						}
					}
				}
			}
		}
	}

}

void hog_svm(hls::stream<bgr>& instream, hls::stream<ap_fixed_float>& resultstream, hls::stream<ap_int<8> >& ystream, hls::stream<ap_int<8> >& xstream){
	hls::stream<bgr>upper_scaled_rgb, bottom_scaled_rgb;
	hls::stream<ap_uint<8> > gray_pix;
	hls::stream<magnitude_fixed > magstream;
	hls::stream<ap_uint<4> > binstream;
	hls::stream<blockpart_fixed_9 > bottom, upper;
	hls::stream<ap_fixed9_float > ul_out, ur_out, bl_out, br_out;
	hls::stream<ap_fixed_float> hog_resultstream, bgr_hsv_resultstream;
#pragma HLS INTERFACE axis port=instream
#pragma HLS INTERFACE axis port=resultstream
#pragma HLS INTERFACE axis port=ystream
#pragma HLS INTERFACE axis port=xstream

#pragma HLS DATAFLOW
	grayscale_and_resizing(instream, upper_scaled_rgb, bottom_scaled_rgb, gray_pix);
	compute_mag_and_bin(gray_pix, magstream, binstream);
	cell_histogram_generate(magstream, binstream, bottom, upper);
	block_histogram_normalization(bottom, upper, ul_out, ur_out, bl_out, br_out);
	hog_svm_classification(ul_out, ur_out, bl_out, br_out, hog_resultstream);
	bgr_hsv_svm_classification(upper_scaled_rgb, bottom_scaled_rgb, bgr_hsv_resultstream);
	for(int y = 0; y < 57; y++){
		for(int x = 0; x < 73; x++){
			ap_fixed_float rst1, rst2;
			rst1 = hog_resultstream.read();
			rst2 = bgr_hsv_resultstream.read();
			resultstream.write(rst1 + rst2);
			ystream.write(y);
			xstream.write(x);
		}
	}
}
