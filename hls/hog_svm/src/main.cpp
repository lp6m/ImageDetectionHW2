#include "hls_video.h"
#include <iostream>
#include <string.h>
#include <hls_stream.h>
#include <ap_axi_sdata.h>

#define IMAGE_WIDTH 640
#define IMAGE_HEIGHT 480
#define WINDOW_WIDTH 64
#define WINDOW_HEIGHT 32
#define CELL_SIZE 8
#define BLOCK_SIZE 2
#define HIST_BIN_NUM 9

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

typedef hls::stream<ap_axis<32,1,1,1> > block_out;
void cell_histogram_generate(hls::stream<ap_axis<32,1,1,1> >& magstream, hls::stream<ap_axis<32,1,1,1> >& binstream,
		block_out& bottom0, block_out& bottom1, block_out& bottom2, block_out& bottom3, block_out& bottom4, block_out& bottom5, block_out& bottom6, block_out& bottom7, block_out& bottom8,
		block_out& upper0, block_out& upper1, block_out& upper2, block_out& upper3, block_out& upper4, block_out& upper5, block_out& upper6, block_out& upper7, block_out& upper8){

	hls::LineBuffer<CELL_SIZE, IMAGE_WIDTH/CELL_SIZE, int> linebufs[HIST_BIN_NUM];
	hls::LineBuffer<2, IMAGE_WIDTH/CELL_SIZE, int> cellbuf[HIST_BIN_NUM];
#pragma HLS ARRAY_PARTITION variable=linebufs complete dim=1
#pragma HLS ARRAY PARTITION variable=cellbuf complete dim=1
	int vote_counter[HIST_BIN_NUM] = {0};
#pragma HLS ARRAY_PARTITION variable=vote_counter complete dim=1
	loop_y:for(int y = 0; y < IMAGE_HEIGHT; y++){
		loop_winx:for(int winx = 0; winx < IMAGE_WIDTH / CELL_SIZE; winx++){
			//int output_count = 9;
			loop_cell_index:for(int cell_index = 0; cell_index < CELL_SIZE; cell_index++){
#pragma HLS PIPELINE II=1
				ap_axis<32,1,1,1> mag, bin;
				magstream >> mag;
				binstream >> bin;
				vote_counter[(int)bin.data] += mag.data;
				if(cell_index == 7){
					loop_updatelinebuf:for(int i = 0; i < HIST_BIN_NUM; i++){
						linebufs[i].shift_pixels_up(winx);
						linebufs[i].insert_bottom_row(vote_counter[i], winx);
					}
					if(y%8 == 7){
						loop_cellbuf_calc:for(int bin_index = 0; bin_index < HIST_BIN_NUM; bin_index++){
							int sum_of_cell = linebufs[bin_index].getval(0, winx) + linebufs[bin_index].getval(1, winx) + linebufs[bin_index].getval(2, winx) + linebufs[bin_index].getval(3, winx) + linebufs[bin_index].getval(4, winx) + linebufs[bin_index].getval(5, winx) + linebufs[bin_index].getval(6, winx) + linebufs[bin_index].getval(7, winx);//sum_of_cell[bin_index] =
							cellbuf[bin_index].shift_pixels_up(winx);
							cellbuf[bin_index].insert_bottom_row(sum_of_cell, winx);
							if(y >= 15){
								ap_axis<32,1,1,1> upper, bottom;
								upper.data = cellbuf[bin_index].getval(0, winx);
								bottom.data = cellbuf[bin_index].getval(1, winx);
								if(bin_index == 0){
									upper0 << upper;
									bottom0 << bottom;
								}else if(bin_index == 1){
									upper1 << upper;
									bottom1 << bottom;
								}else if(bin_index == 2){
									upper2 << upper;
									bottom2 << bottom;
								}else if(bin_index == 3){
									upper3 << upper;
									bottom3 << bottom;
								}else if(bin_index == 4){
									upper4 << upper;
									bottom4 << bottom;
								}else if(bin_index == 5){
									upper5 << upper;
									bottom5 << bottom;
								}else if(bin_index == 6){
									upper6 << upper;
									bottom6 << bottom;
								}else if(bin_index == 7){
									upper7 << upper;
									bottom7 << bottom;
								}else{//if(bin_index == 8)
									upper8 << upper;
									bottom8 << bottom;
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

void hog_svm(hls::stream<ap_axis<8,1,1,1> >& instream,
		block_out& bottom0, block_out& bottom1, block_out& bottom2, block_out& bottom3, block_out& bottom4, block_out& bottom5, block_out& bottom6, block_out& bottom7, block_out& bottom8,
		block_out& upper0, block_out& upper1, block_out& upper2, block_out& upper3, block_out& upper4, block_out& upper5, block_out& upper6, block_out& upper7, block_out& upper8){
	hls::stream<ap_axis<32,1,1,1> > magstream;
	hls::stream<ap_axis<32,1,1,1> > binstream;
#pragma HLS INTERFACE axis port=instream
#pragma HLS INTERFACE axis port=bottom0
#pragma HLS INTERFACE axis port=bottom1
#pragma HLS INTERFACE axis port=bottom2
#pragma HLS INTERFACE axis port=bottom3
#pragma HLS INTERFACE axis port=bottom4
#pragma HLS INTERFACE axis port=bottom5
#pragma HLS INTERFACE axis port=bottom6
#pragma HLS INTERFACE axis port=bottom7
#pragma HLS INTERFACE axis port=bottom8
#pragma HLS INTERFACE axis port=upper0
#pragma HLS INTERFACE axis port=upper1
#pragma HLS INTERFACE axis port=upper2
#pragma HLS INTERFACE axis port=upper3
#pragma HLS INTERFACE axis port=upper4
#pragma HLS INTERFACE axis port=upper5
#pragma HLS INTERFACE axis port=upper6
#pragma HLS INTERFACE axis port=upper7
#pragma HLS INTERFACE axis port=upper8

#pragma HLS DATAFLOW
	compute_mag_and_bin(instream, magstream, binstream);
	cell_histogram_generate(magstream, binstream, bottom0, bottom1, bottom2, bottom3, bottom4, bottom5, bottom6, bottom7, bottom8,
			upper0, upper1, upper2, upper3, upper4, upper5, upper6, upper7, upper8);

	//normalizeBlock(sum_of_block, descriptor, normalized);
	//memcpy(feature, normalized, HISTOGRAMSIZE * sizeof(unsigned short));
}
