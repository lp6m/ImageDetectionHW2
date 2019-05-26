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


typedef ap_fixed<32,10,AP_RND> ap_fixed_float;
//typedef ap_fixed<64,20,AP_RND> ap_fixed_float;
struct bgr{
	unsigned char b,g,r;
};
