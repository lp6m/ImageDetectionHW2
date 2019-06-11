struct hogweight{
	ap_uint<128> weightval[9];
};

struct pixweight{
	ap_uint<128> weight[3];
};

typedef ap_fixed<32, 10> ap_fixed_point;
typedef ap_fixed<64, 20> accum_fixed;

