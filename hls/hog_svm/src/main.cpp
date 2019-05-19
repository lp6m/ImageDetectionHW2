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


//mag minimum:0 maximum:sqrt(255*255+255*255) < 2^9
//integer bit needs 9 + 1(sign) = 10bit
typedef ap_fixed<18,10,AP_RND> magnitude_fixed;

void compute_mag_and_bin(hls::stream<ap_axis<8,1,1,1> >& instream, hls::stream<magnitude_fixed >& magstream, hls::stream<ap_uint<4> >& binstream){
	ap_axis<8,1,1,1> in;
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

			instream >> in;
			//these operation will be executed in parallel.
			linebuf.shift_pixels_up(x);
			linebuf.insert_bottom_row(in.data, x);
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
void svm_classification(hls::stream<ap_fixed9_float>& upperleft, hls::stream<ap_fixed9_float>& upperright, hls::stream<ap_fixed9_float>& bottomleft, hls::stream<ap_fixed9_float>& bottomright,
		hls::stream<ap_fixed_float>& resultstream, hls::stream<ap_axis<8,1,1,1> >& ystream, hls::stream<ap_axis<8,1,1,1> >& xstream){
	weight WeightData[WINDOW_BLOCKNUM_H][WINDOW_BLOCKNUM_W] = {
	{{{-0.27702861, 0.60882872, 0.77489772, -0.57971451, 1.1351337, -0.61708925, -0.037168136, 0.53348561, 0.65899799}, {-0.2304792, 0.15729401, -0.38586537, 1.0745021, 0.37144029, 0.26018033, -0.050108985, 0.50318494, 0.19848787}, {-0.81186583, 0.090699661, -0.69320837, 0.3581912, -0.22366082, -0.00046446369, 0.020652174, 0.066666796, 0.72009897}, {-0.11970606, 0.16936678, 0.49433372, -0.41093125, -0.25566695, 0.41232198, -0.22603478, -0.30878159, 0.5752175}},
	{{0.025700067, 0.55524942, -0.14299036, 1.1606084, 0.65674398, 0.26819078, -0.62530728, 0.047398622, -0.1328025}, {-0.96653118, -0.99550293, 0.029158756, 0.60217548, 0.1142082, 0.15795023, 0.5264729, 0.58868957, 0.74495914}, {-0.21356984, 0.18473744, 0.78924964, -0.4198517, -0.26577343, 0.23037056, -0.22418226, -0.4438898, 0.46172914}, {-0.21188348, -0.024238361, 0.5134554, -0.023279752, 0.36486243, 0.97045258, 0.22966675, -0.098204999, 0.26555553}},
	{{-0.86914489, -0.68238714, 0.026812079, 0.41011881, 0.057557923, 0.26800468, 0.24119578, 0.3414049, 0.6902127}, {-0.28180391, -0.55422329, 0.37444749, 0.10904583, -0.46401356, 0.49501922, -0.22026527, -0.79844701, 0.24561922}, {-0.31483978, -0.20492422, 0.38120321, -0.0136952, 0.13366862, 0.82982646, 0.19849744, -0.43043436, 0.28016748}, {0.38495892, -0.055314627, 0.37917025, -0.12441661, 0.24382124, 0.48965262, 0.31991869, -0.080944209, 0.42775918}},
	{{-0.24671259, -0.012490305, 0.59119726, 0.54870899, -0.14582407, 0.43749451, -0.1957042, -0.47959878, 0.7652754}, {-0.77971012, 0.068167356, 0.22010137, -0.28520843, 0.12596349, 0.43234721, -0.18261237, -0.91296345, 0.59792026}, {0.52231893, 0.10873035, 0.23189917, 0.33637669, 0.18480004, 0.37732502, 0.20249588, -0.0086822237, 0.42405191}, {0.14519455, 0.58410151, -0.48353986, 0.33736452, 0.15643579, 0.56669449, 0.30621269, 0.11954056, 0.35308405}},
	{{-0.41700489, 0.43662467, 0.01164503, -0.79628251, 0.20602757, 0.73133087, -0.21961537, -0.84576873, 0.71220664}, {0.016172465, 0.080416432, -0.0014595037, 0.38547646, 0.50455541, -0.070570844, 0.92615698, -0.40507349, 0.57797506}, {0.28772145, 1.1379891, -0.16899795, 0.26234811, -0.047884443, 0.31540193, -0.09030899, -0.1386055, 0.061466535}, {0.30002874, 0.3748508, 1.0799594, -0.178928, 0.28508135, 0.31442179, 0.69212965, -0.34006707, -0.086685334}},
	{{-0.27969151, -0.20775641, -0.11083242, 0.21389093, 0.46324219, -0.51753321, 0.51174946, -0.47594278, 0.56434511}, {-0.76210136, -0.55888976, -0.73619471, 0.088034475, -0.75506386, -0.23101256, -0.19797806, -0.3629989, 0.10285923}, {0.22232903, 0.19699354, 1.1235911, 0.04239838, 0.18618405, 0.20667088, 0.22342196, -0.17847417, -0.084844314}, {0.82994545, 0.86637083, 0.79220726, 0.34035208, 0.87545105, 0.69480061, 0.96528777, 0.51438682, 0.57698045}},
	{{-0.84012493, -0.39109452, -0.49328681, 0.075806932, -1.0200621, -0.37038699, -0.13468987, -0.3954281, -0.095477797}, {-2.6873869, 1.3384302, 0.42602405, 0.19305884, 0.059088147, 0.51419599, -0.026306977, -0.59619403, 0.21839868}, {0.51190524, 0.86711813, 1.0035063, 0.65906713, 0.8361062, 0.33776363, 0.86298273, 0.478098, 0.71702999}, {0.55562994, -0.42534472, -0.1631459, -0.6406547, 0.20347042, 0.69544903, 1.7594445, 0.27672169, 0.41676805}}},
	{{{-0.026565011, 0.8750027, 0.21319598, 0.45462325, -0.17950526, -0.18613147, -0.53005273, -0.52325662, 0.68785114}, {0.063024414, 0.64865514, 1.022538, -0.40963255, -0.077097115, 0.17893047, -0.052994053, -0.46129413, 0.51845943}, {0.23440801, -0.15624654, -0.0067885032, 0.11268131, 0.97924908, 0.17584953, 0.51239629, 0.74171771, 0.09752271}, {0.20597678, 0.39502654, 0.40717249, 0.16875216, -0.3626527, -0.079479627, 0.60823399, -0.1211186, 0.36074692}},
	{{-0.081084726, 0.21061735, 0.53397405, -0.70642196, -0.19027283, 0.40136058, -0.15184119, -0.45998149, 0.33319134}, {0.32961565, -0.11731105, 0.14184209, -0.61769355, 0.74055756, 0.98278503, 0.41457567, -0.25431528, 0.16394702}, {0.20218627, 0.089478814, -0.16113657, -0.34105227, -0.23109162, 0.23156054, 0.61959063, 0.076230109, 0.09632748}, {-0.24881096, -0.20862664, 0.039224784, 0.54311276, -0.033572687, 0.055324484, -0.19684684, -0.40374573, 0.27092075}},
	{{0.375982, -0.072703436, 0.36800475, -0.34038849, 0.70672804, 1.2860142, 0.65728211, -0.42856792, 0.025150893}, {0.63502534, 0.1422673, 0.49842448, -0.38521487, 0.30287861, 0.44065762, 0.28560239, -0.011097929, 0.46351148}, {-1.0033978, -0.82056151, -0.77426304, 0.060839325, 0.069293353, 0.34755052, 0.56671999, 0.34718724, 0.45202454}, {-0.25124857, -0.30847623, 0.47957693, -0.35061047, 0.21305469, -0.14469981, 0.33012084, -0.31366777, 0.1174494}},
	{{0.90244197, 0.26415254, 0.26486262, 0.21520949, 0.52106953, 0.89206695, 0.7112813, 0.23528755, 0.27022265}, {0.37931878, 0.96805914, -0.57568438, 0.15180537, 0.32653507, 0.59538149, 0.34913614, 0.0013011896, 0.22434735}, {-0.25821259, -0.39824002, 1.0617368, -0.0052754369, 0.16161773, 0.22708221, 0.5766827, -0.047858989, 0.06390307}, {-0.44883223, 0.81818747, 0.0087344773, 0.29438361, 0.26548172, -0.80206375, -0.17507186, 0.19135255, 0.77115767}},
	{{0.14376092, 1.3076891, -0.22560989, 0.03874272, 0.33688028, 0.024086497, -0.11127855, 0.015224355, -0.059854957}, {0.74046974, 0.050317658, 0.71731345, -0.35621582, 0.88144578, 0.34302006, 0.49848935, -0.08149386, -0.18364422}, {-0.61092334, -0.039563839, -0.64805246, -0.73209886, -0.1612037, -0.1824376, -0.10095392, -0.54863707, -0.13006024}, {0.84035913, -0.1074547, 0.095771397, 0.065293077, 0.50295166, 0.76087729, -0.05915887, 0.0038541875, 0.17693993}},
	{{-0.10783917, -1.395354, -0.18819719, -0.91640697, 0.10561303, -0.87915558, -1.2311914, -0.8063915, -0.53489989}, {0.40550872, 0.31034574, 0.13466076, 0.15388423, 0.42449091, -0.62185287, -0.1069148, -0.24735412, 0.091001952}, {0.078088304, -1.4804614, -0.47730564, -0.85410218, -0.73027194, 0.28144448, -0.7827631, -1.0725579, -0.87395905}, {0.019227432, -0.36397524, 0.039364162, -1.189656, 0.42376989, 0.029999473, 0.97409512, 0.28772906, 0.11587147}},
	{{0.20056804, -0.26722339, -0.42421664, -0.38231162, 0.48156817, -0.16695242, 0.43342902, -0.28646052, -0.034916129}, {0.25049893, -1.0733647, -0.48631756, -0.49391604, -0.50135032, -0.040424458, 1.0258481, -0.62519932, 0.21244732}, {0.33072464, 0.013948886, 0.57503137, -0.71442171, 0.32876287, -0.52405946, 0.26531818, -0.032614129, -0.049290842}, {0.29821244, 0.55137431, -0.081307618, -2.3686619, -0.86137179, -0.98107756, -0.97629564, -0.40670394, -0.8485431}}},
	{{{0.6214841, -0.26896073, -0.1465191, 0.079095399, 1.5062982, 0.084564116, 0.20235011, 0.37424373, 0.029640964}, {0.16488969, -0.060275283, -0.0043007629, -0.16993278, -0.37384607, 0.12352943, 0.85481896, -0.035967501, -0.053911601}, {-0.40461548, -0.9585113, -0.30067559, 1.140472, -0.49255279, 0.38920071, 1.8597437, -0.19081892, -0.29259476}, {-0.23485883, 0.10457873, 1.5402637, -0.12268229, 0.67514796, -0.45597952, 0.53631412, -0.25938066, 0.11603961}},
	{{-0.16532986, -0.28776586, -0.30216771, -0.13983241, -0.23488818, 0.26841173, 0.67579952, 0.2075759, 0.10079492}, {0.2883067, -0.076254561, -0.056714445, -0.12717646, -0.25804559, 0.10767514, 0.094400795, -0.58775883, 0.35738747}, {0.30955558, 0.11727025, 1.5156538, -0.33047703, 0.9563396, -0.36071855, 0.71598153, -0.0018377865, 0.26591552}, {0.21674342, 0.059564024, 0.070069545, -0.79517899, 0.36173561, -0.98804456, 0.023132508, -0.37981175, 0.6817025}},
	{{0.064290062, -0.032293471, -0.091723149, 0.16920939, 0.39532108, 0.92078631, 1.3558296, 0.28348178, 0.86695562}, {0.26121262, 0.4401242, 0.99947631, -0.15200249, 0.75073983, 0.15087219, 0.44446802, -0.48152652, 0.23903341}, {0.44827476, 0.63191367, 0.73132405, -0.15659082, 0.52343435, -0.39743259, 0.80255174, 0.085470138, 0.36742239}, {-0.083730254, -0.49241272, -0.11393205, 0.79072976, 0.3507508, -0.77723122, 0.63394345, -0.87268678, 0.61178516}},
	{{0.34102065, 0.39300976, 1.2152561, -0.010222636, 1.0280432, 0.51924918, 0.80043923, -0.075764857, 0.4653466}, {-0.028802604, 1.5028069, 0.13099841, 0.42172255, 0.69861489, 0.18687582, 0.34924764, 0.32517003, 0.68760069}, {0.076066524, -0.11743453, 0.41831777, 1.3341999, 0.19940946, -0.10386014, 0.96476401, -0.52698667, 0.41861975}, {-0.59498568, -0.22380025, -1.7230965, -0.16611817, 0.40480869, -0.52719617, 0.09220452, 0.73069318, 1.077759}},
	{{-0.13308773, 0.95736562, -0.1212927, -0.28634779, 0.40505793, 0.46279891, 0.30133646, 0.18380982, 0.32466724}, {1.297843, 0.48267698, 0.95107169, 0.94341752, 1.2225781, 0.63840273, 0.4601062, 0.37791788, 0.64735413}, {-0.12596486, -0.1281331, -1.9312925, -0.26744321, 0.12684679, -0.099555644, 0.26884079, 0.31270983, 0.66459285}, {0.4119147, 0.31075679, 1.5324757, 1.1849294, 0.23011497, 0.39824328, -0.029211714, 0.33448819, 0.74007796}},
	{{0.36431529, -0.76619006, 0.19175927, 0.21671878, 0.29059059, 0.13743621, -0.018456065, -0.29732754, 0.29029415}, {0.63100797, 0.22476678, 0.80325931, -0.15971009, 1.548191, 0.38844737, 1.0856287, 1.0905757, 1.0661546}, {0.18293959, 0.16563473, 1.2804299, 0.48590165, -0.72035689, 0.23467517, -0.42360673, -0.77477641, -0.075293888}, {-0.71484734, -1.4952735, 0.1655765, 0.36127044, 0.054662283, -0.5566214, 0.32951196, 0.36611205, 0.2143739}},
	{{0.68814351, 0.33994408, 0.98631133, 0.20109481, 1.0035688, -0.14433747, 0.56124644, 0.62519471, 0.8047775}, {0.62384013, 1.2266169, 0.5431755, -1.4693138, -0.080789101, -0.47382196, -0.68793498, -0.20996721, 0.12546701}, {-0.30741481, -0.97833445, 0.64061856, 0.45745884, -0.27755774, -0.85114379, -0.11401231, -0.58619251, -0.10015404}, {-0.51423913, 0.012935767, 1.4442867, -0.14993207, -1.356337, -0.21873557, -1.7991753, -1.2349689, -0.6561743}}}
	};


#pragma HLS ARRAY_PARTITION variable=WeightData complete dim=1
	ap_fixed_float PartialSum[WINDOW_BLOCKNUM_H][WINDOW_NUM_W];
#pragma HLS ARRAY_PARTITION variable=PartialSum complete dim=1
	for(int i = 0; i < WINDOW_BLOCKNUM_H; i++){
		for(int j = 0; j < WINDOW_NUM_W; j++) PartialSum[i][j] = 0;
	}
	ap_fixed_float bias = -14.786592;
	for(int y = 0; y < BLOCK_NUM_H; y++){
		for(int x = 0; x < BLOCK_NUM_W; x++){
			ap_fixed9_float ul = upperleft.read();
			ap_fixed9_float ur = upperright.read();
			ap_fixed9_float bl = bottomleft.read();
			ap_fixed9_float br = bottomright.read();
			for(int winx = 0; winx < WINDOW_NUM_W; winx++){
#pragma HLS PIPELINE II=1
				bool inside_window = (winx <= x && x <= winx + (WINDOW_BLOCKNUM_W - 1));
				if(inside_window){
					int block_index_x = x - winx;

					//block_index_y indicates where (ul,ur,bl,br) is located in the window in y axis.
					for(int block_index_y = 0; block_index_y < WINDOW_BLOCKNUM_H; block_index_y++){
						int block_start_y = y - block_index_y;
						if(0 <= block_start_y && block_start_y <= (BLOCK_NUM_H - WINDOW_BLOCKNUM_H)){
							int partial_sum_index_y = (y - block_index_y) % WINDOW_BLOCKNUM_H;

							weight w = WeightData[block_index_y][block_index_x];
							ap_fixed_float tmp_partial_sum = multiply_accum(w.upperleft, ul) + multiply_accum(w.upperright, ur)
									+ multiply_accum(w.bottomleft, bl) + multiply_accum(w.bottomright, br);
							PartialSum[partial_sum_index_y][winx] += tmp_partial_sum;

							bool window_completed = (block_index_x == (WINDOW_BLOCKNUM_W - 1) && block_index_y == (WINDOW_BLOCKNUM_H - 1));
							if(window_completed){
								ap_fixed_float allsum = PartialSum[partial_sum_index_y][winx] + bias;
								PartialSum[partial_sum_index_y][winx] = 0;
								ap_axis<8,1,1,1> ap_y, ap_x;
								ap_y.data = block_start_y;
								ap_x.data = winx;
								resultstream << allsum;
								ystream << ap_y;
								xstream << ap_x;
							}
						}
					}
				}
			}
		}
	}

}

void hog_svm(hls::stream<ap_axis<8,1,1,1> >& instream, hls::stream<ap_fixed_float>& resultstream, hls::stream<ap_axis<8,1,1,1> >& ystream, hls::stream<ap_axis<8,1,1,1> >& xstream){
	hls::stream<magnitude_fixed > magstream;
	hls::stream<ap_uint<4> > binstream;
	hls::stream<blockpart_fixed_9 > bottom, upper;
	hls::stream<ap_fixed9_float > ul_out, ur_out, bl_out, br_out;
#pragma HLS INTERFACE axis port=instream
#pragma HLS INTERFACE axis port=resultstream
#pragma HLS INTERFACE axis port=ystream
#pragma HLS INTERFACE axis port=xstream


#pragma HLS DATAFLOW
	compute_mag_and_bin(instream, magstream, binstream);
	cell_histogram_generate(magstream, binstream, bottom, upper);
	block_histogram_normalization(bottom, upper, ul_out, ur_out, bl_out, br_out);
	svm_classification(ul_out, ur_out, bl_out, br_out, resultstream, ystream, xstream);

}
