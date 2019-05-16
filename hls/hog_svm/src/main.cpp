#include "hls_video.h"
#include <iostream>
#include <string.h>
#include <hls_stream.h>
#include <ap_axi_sdata.h>

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

void compute_mag_and_bin(hls::stream<ap_axis<8,1,1,1> >& instream, hls::stream<ap_axis<32,1,1,1> >& magstream, hls::stream<ap_axis<32,1,1,1> >& binstream){
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
			int mag = isedge ? 0 : approx_distance(Gx, Gy);
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
			ap_axis<32,1,1,1> m,b;
			m.data = mag;
			b.data = bin_index;
			magstream << m;
			binstream << b;
		}
	}
}

template<int W, int U, int TI, int TD>
    struct ap_int9_axis{
        struct data {
            ap_int<W> data0;
            ap_int<W> data1;
            ap_int<W> data2;
            ap_int<W> data3;
            ap_int<W> data4;
            ap_int<W> data5;
            ap_int<W> data6;
            ap_int<W> data7;
            ap_int<W> data8;
        } data;
        ap_uint<(W+7)/8> keep;
        ap_uint<(W+7)/8> strb;
        ap_uint<U>       user;
        ap_uint<1>       last;
        ap_uint<TI>      id;
        ap_uint<TD>      dest;
    };

typedef hls::stream<ap_axis<32,1,1,1> > block_out;
void cell_histogram_generate(hls::stream<ap_axis<32,1,1,1> >& magstream, hls::stream<ap_axis<32,1,1,1> >& binstream,
		hls::stream<ap_int9_axis<32,1,1,1> >& bottom, hls::stream<ap_int9_axis<32,1,1,1> >& upper){


	hls::LineBuffer<CELL_SIZE, IMAGE_WIDTH/CELL_SIZE, int> linebufs[HIST_BIN_NUM];
	hls::LineBuffer<2, IMAGE_WIDTH/CELL_SIZE, int> cellbuf[HIST_BIN_NUM];
#pragma HLS ARRAY_PARTITION variable=linebufs complete dim=1
#pragma HLS ARRAY PARTITION variable=cellbuf complete dim=1
	int vote_counter[HIST_BIN_NUM] = {0};
#pragma HLS ARRAY_PARTITION variable=vote_counter complete dim=1
	loop_y:for(int y = 0; y < IMAGE_HEIGHT; y++){
		loop_winx:for(int winx = 0; winx < IMAGE_WIDTH / CELL_SIZE; winx++){
			loop_cell_index:for(int cell_index = 0; cell_index < CELL_SIZE; cell_index++){
#pragma HLS PIPELINE II=1
				ap_axis<32,1,1,1> mag, bin;
				magstream >> mag;
				binstream >> bin;
				vote_counter[(int)bin.data] += mag.data;
				if(cell_index == (CELL_SIZE - 1)){
					loop_updatelinebuf:for(int i = 0; i < HIST_BIN_NUM; i++){
						linebufs[i].shift_pixels_up(winx);
						linebufs[i].insert_bottom_row(vote_counter[i], winx);
					}
					if(y%CELL_SIZE == (CELL_SIZE - 1)){
						ap_int9_axis<32,1,1,1> out_upper, out_bottom;
						loop_cellbuf_calc:for(int bin_index = 0; bin_index < HIST_BIN_NUM; bin_index++){
							int sum_of_cell = linebufs[bin_index].getval(0, winx) + linebufs[bin_index].getval(1, winx) + linebufs[bin_index].getval(2, winx) + linebufs[bin_index].getval(3, winx) + linebufs[bin_index].getval(4, winx) + linebufs[bin_index].getval(5, winx) + linebufs[bin_index].getval(6, winx) + linebufs[bin_index].getval(7, winx);//sum_of_cell[bin_index] =
							cellbuf[bin_index].shift_pixels_up(winx);
							cellbuf[bin_index].insert_bottom_row(sum_of_cell, winx);
							if(y >= CELL_SIZE * BLOCK_SIZE - 1){
								//ap_axis<32,1,1,1> upper, bottom;
								int upper_d, bottom_d;
								upper_d = cellbuf[bin_index].getval(0, winx);
								bottom_d = cellbuf[bin_index].getval(1, winx);
								if(bin_index == 0){
									out_upper.data.data0 = upper_d;
									out_bottom.data.data0 = bottom_d;
								}else if(bin_index == 1){
									out_upper.data.data1 = upper_d;
									out_bottom.data.data1 = bottom_d;
								}else if(bin_index == 2){
									out_upper.data.data2 = upper_d;
									out_bottom.data.data2 = bottom_d;
								}else if(bin_index == 3){
									out_upper.data.data3 = upper_d;
									out_bottom.data.data4 = bottom_d;
								}else if(bin_index == 4){
									out_upper.data.data4 = upper_d;
									out_bottom.data.data4 = bottom_d;
								}else if(bin_index == 5){
									out_upper.data.data5 = upper_d;
									out_bottom.data.data5 = bottom_d;
								}else if(bin_index == 6){
									out_upper.data.data6 = upper_d;
									out_bottom.data.data6 = bottom_d;
								}else if(bin_index == 7){
									out_upper.data.data7 = upper_d;
									out_bottom.data.data7 = bottom_d;
								}else{//if(bin_index == 8)
									out_upper.data.data8 = upper_d;
									out_bottom.data.data8 = bottom_d;
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

void block_histogram_normalization(hls::stream<ap_int9_axis<32,1,1,1> >& bottom, hls::stream<ap_int9_axis<32,1,1,1> >& upper,
		hls::stream<ap_int9_axis<32,1,1,1> >& ul_out, hls::stream<ap_int9_axis<32,1,1,1> >& ur_out, hls::stream<ap_int9_axis<32,1,1,1> >& bl_out, hls::stream<ap_int9_axis<32,1,1,1> >& br_out){

	hls::LineBuffer<2, 1, int> bottomfifo[HIST_BIN_NUM], upperfifo[HIST_BIN_NUM];
	int partial_block_sum;
#pragma HLS ARRAY_PARTITION variable=bottomfifo complete dim=1
#pragma HLS ARRAY PARTITION variable=upperfifo complete dim=1
	for(int y = 0; y < (IMAGE_HEIGHT / CELL_SIZE - BLOCK_SIZE + 1); y++){//59
		for(int x = 0; x < (IMAGE_WIDTH / CELL_SIZE); x++){//80
			ap_int9_axis<32,1,1,1> bottom_in = bottom.read();
			ap_int9_axis<32,1,1,1> upper_in = upper.read();
			ap_int9_axis<32,1,1,1> ul, ur, bl, br;
			for(int bin_index = 0; bin_index < HIST_BIN_NUM; bin_index++){
#pragma HLS PIPELINE II=1
				int b, u;
				if(bin_index == 0) b = bottom_in.data.data0;
				if(bin_index == 0) u = upper_in.data.data0;
				if(bin_index == 1) b = bottom_in.data.data1;
				if(bin_index == 1) u = upper_in.data.data1;
				if(bin_index == 2) b = bottom_in.data.data2;
				if(bin_index == 2) u = upper_in.data.data2;
				if(bin_index == 3) b = bottom_in.data.data3;
				if(bin_index == 3) u = upper_in.data.data3;
				if(bin_index == 4) b = bottom_in.data.data4;
				if(bin_index == 4) u = upper_in.data.data4;
				if(bin_index == 5) b = bottom_in.data.data5;
				if(bin_index == 5) u = upper_in.data.data5;
				if(bin_index == 6) b = bottom_in.data.data6;
				if(bin_index == 6) u = upper_in.data.data6;
				if(bin_index == 7) b = bottom_in.data.data7;
				if(bin_index == 7) u = upper_in.data.data7;
				if(bin_index == 8) b = bottom_in.data.data8;
				if(bin_index == 8) u = upper_in.data.data8;
				bottomfifo[bin_index].shift_pixels_up(0);
				bottomfifo[bin_index].insert_bottom(b, 0);
				upperfifo[bin_index].shift_pixels_up(0);
				upperfifo[bin_index].insert_bottom(u, 0);

				int partial_block_new_sum = b + u;
				int block_sum = partial_block_new_sum + partial_block_sum;
				partial_block_sum = partial_block_new_sum;

				bool sum_of_block_completed = (x >= 1);
				if(sum_of_block_completed){
					//59*79
					//normalization
					int un_upperleft = upperfifo[bin_index].getval(0, 0);
					int un_upperright = upperfifo[bin_index].getval(1, 0);
					int un_bottomleft = bottomfifo[bin_index].getval(0, 0);
					int un_bottomright = bottomfifo[bin_index].getval(1, 0);
					int upperleft = block_sum == 0 ? 0 : sqrt((un_upperleft / block_sum) << 1);
					int upperright = block_sum == 0 ? 0 : sqrt((un_upperright / block_sum) << 1);
					int bottomleft = block_sum == 0 ? 0 : sqrt((un_bottomleft / block_sum) << 1);
					int bottomright = block_sum == 0 ? 0 : sqrt((un_bottomright / block_sum) << 1);

					switch(bin_index){
					case 0:
						ul.data.data0 = upperleft; ur.data.data0 = upperright;
						bl.data.data0 = bottomleft; br.data.data0 = bottomright;
						break;
					case 1:
						ul.data.data1 = upperleft; ur.data.data1 = upperright;
						bl.data.data1 = bottomleft; br.data.data1 = bottomright;
						break;
					case 2:
						ul.data.data2 = upperleft; ur.data.data2 = upperright;
						bl.data.data2 = bottomleft; br.data.data2 = bottomright;
						break;
					case 3:
						ul.data.data3 = upperleft; ur.data.data3 = upperright;
						bl.data.data3 = bottomleft; br.data.data3 = bottomright;
						break;
					case 4:
						ul.data.data4 = upperleft; ur.data.data4 = upperright;
						bl.data.data4 = bottomleft; br.data.data4 = bottomright;
						break;
					case 5:
						ul.data.data5 = upperleft; ur.data.data5 = upperright;
						bl.data.data5 = bottomleft; br.data.data5 = bottomright;
						break;
					case 6:
						ul.data.data6 = upperleft; ur.data.data6 = upperright;
						bl.data.data6 = bottomleft; br.data.data6 = bottomright;
						break;
					case 7:
						ul.data.data7 = upperleft; ur.data.data7 = upperright;
						bl.data.data7 = bottomleft; br.data.data7 = bottomright;
						break;
					case 8:
						ul.data.data8 = upperleft; ur.data.data8 = upperright;
						bl.data.data8 = bottomleft; br.data.data8 = bottomright;
						break;
					}
				}
			}
			ul_out << ul;
			ur_out << ur;
			bl_out << bl;
			br_out << br;
		}
	}
}
struct histdata{
	int data[9];
};
struct weight{
	histdata upperleft;
	histdata upperright;
	histdata bottomleft;
	histdata bottomright;
};

int multiply_accum(histdata weights, ap_int9_axis<32,1,1,1> features){
	return weights.data[0] * features.data.data0 + weights.data[1] * features.data.data1 + weights.data[2] * features.data.data2
			+ weights.data[3] * features.data.data3 + weights.data[4] * features.data.data4 + weights.data[5] * features.data.data5
			+ weights.data[6] * features.data.data6 + weights.data[7] * features.data.data7 + weights.data[8] * features.data.data8;
}
void svm_classification(hls::stream<ap_int9_axis<32,1,1,1> >& upperleft, hls::stream<ap_int9_axis<32,1,1,1> >& upperright, hls::stream<ap_int9_axis<32,1,1,1> >& bottomleft, hls::stream<ap_int9_axis<32,1,1,1> >& bottomright,
		hls::stream<ap_axis<8,1,1,1> >& resultstream, hls::stream<ap_axis<8,1,1,1> >& ystream, hls::stream<ap_axis<8,1,1,1> >& xstream){
	weight WeightData[WINDOW_BLOCKNUM_H][WINDOW_BLOCKNUM_W];
#pragma HLS ARRAY_PARTITION variable=WeightData complete dim=1
	int PartialSum[WINDOW_BLOCKNUM_H][WINDOW_NUM_W];
#pragma HLS ARRAY_PARTITION variable=PartialSum complete dim=1

	int bias = 2;
	for(int y = 0; y < BLOCK_NUM_H; y++){
		for(int x = 0; x < (IMAGE_WIDTH / CELL_SIZE); x++){
			ap_int9_axis<32,1,1,1> ul = upperleft.read();
			ap_int9_axis<32,1,1,1> ur = upperright.read();
			ap_int9_axis<32,1,1,1> bl = bottomleft.read();
			ap_int9_axis<32,1,1,1> br = bottomright.read();
			for(int winx = 0; winx < WINDOW_NUM_W; winx++){
#pragma HLS PIPELINE II=1
				bool inside_window = (winx <= x && x <= winx + (WINDOW_BLOCKNUM_W - 1));
				if(inside_window){
					int block_index_x = x - winx;
					bool window_completed = ((x - winx) == (WINDOW_BLOCKNUM_W - 1) && y >= (WINDOW_BLOCKNUM_H - 1));

					//calc partial sum and update
					int sum_of_vertical_partial_sum = 0;
					for(int block_index_y = 0; block_index_y < WINDOW_BLOCKNUM_H; block_index_y++){
						weight w = WeightData[block_index_y][block_index_x];
						int tmp_partial_sum = multiply_accum(w.upperleft, ul) + multiply_accum(w.upperright, ur)
								+ multiply_accum(w.bottomleft, bl) + multiply_accum(w.bottomright, br);
						int old_partial_sum = PartialSum[block_index_y][winx];
						int new_partial_sum = old_partial_sum + tmp_partial_sum;
						PartialSum[block_index_y][winx] = new_partial_sum;
						sum_of_vertical_partial_sum += new_partial_sum;
					}
					//if window is completed, calculate sum of vertical partial sum, and plus bias
					if(window_completed){
						int result = ((sum_of_vertical_partial_sum + bias));
						ap_axis<8,1,1,1> ap_r, ap_y, ap_x;
						ap_r.data = result;
						ap_y.data = y - 2;
						ap_x.data = winx;
						resultstream << ap_r;
						ystream << ap_y;
						xstream << ap_x;
					}

				}
			}
		}
	}

}

void hog_svm(hls::stream<ap_axis<8,1,1,1> >& instream, hls::stream<ap_axis<8,1,1,1> >& result, hls::stream<ap_axis<8,1,1,1> >& ystream, hls::stream<ap_axis<8,1,1,1> >& xstream){
	hls::stream<ap_axis<32,1,1,1> > magstream;
	hls::stream<ap_axis<32,1,1,1> > binstream;
	hls::stream<ap_int9_axis<32,1,1,1> > bottom, upper;
	hls::stream<ap_int9_axis<32,1,1,1> > ul_out, ur_out, bl_out, br_out;
#pragma HLS INTERFACE axis port=instream
#pragma HLS INTERFACE axis port=result
#pragma HLS INTERFACE axis port=ystream
#pragma HLS INTERFACE axis port=xstream


#pragma HLS DATAFLOW
	compute_mag_and_bin(instream, magstream, binstream);
	cell_histogram_generate(magstream, binstream, bottom, upper);
	block_histogram_normalization(bottom, upper, ul_out, ur_out, bl_out, br_out);
	svm_classification(ul_out, ur_out, bl_out, br_out, result, ystream, xstream);
}
