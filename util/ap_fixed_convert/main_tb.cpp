#include <string.h>
#include <vector>
#include "weights.h"
#include <cmath>
#include <iomanip>
#include <sstream>

using namespace std;

unsigned int convFixedToUint(ap_fixed<32, 10> val){
	unsigned int res = 0;
	string str = val.to_string(2, false);
	str = str.substr(2, str.length() - 2);
	unsigned int tmp = 1;
	for(int i = str.length() - 1; i >= 0; i--){
		if(str[i] == '.') continue;

		if(str[i] == '1'){
			res += tmp;
		}
		tmp = tmp << 1;
	}
	return res;
}

float reverseUIntRepresentToFloat(unsigned int val){
	float res = 0;
	for(int i = 31; i >= 0; i--){
		unsigned int tmp = 1 << i;
		//cout << tmp << endl;
		if(val > tmp){
			if(i == 31) res -= 512;
			else if(i >= 22) res += (1 << (i - 22));
			else res += pow(0.5, (22 - i));
			val -= tmp;
		}
	}
	return res;
}

string to_string(unsigned int val){
	stringstream ss;
	ss << val;
	return ss.str();
}

string toJson(string name, vector<unsigned int> arr, bool commadisable = false){
	string rst = "";
	rst += "\"" + name + "\" : [";
	for(int i = 0; i < 63; i++){
		rst += to_string(arr[i]);
		if(i != 62) rst += ", ";
	}
	if(commadisable) rst += "]\n";
	else rst += "],\n";
	return rst;
}

struct bgr{
  double u_b;
  double u_g;
  double u_r;

  double b_b;
  double b_g;
  double b_r;
};

struct hsv{
  ap_fixed<32,10> u_h;
  ap_fixed<32,10> u_s;
  ap_fixed<32,10> u_v;

  ap_fixed<32,10> b_h;
  ap_fixed<32,10> b_s;
  ap_fixed<32,10> b_v;
};

struct hogblock{
	ap_fixed<32,10> ul[9];
	ap_fixed<32,10> ur[9];
	ap_fixed<32,10> bl[9];
	ap_fixed<32,10> br[9];
};
bgr bgrweight[32];
hsv hsvweight[32];
hogblock hogweight[21];

string joinstr(vector<string> arr){
	string rst = "";
	for(int i = 0; i < arr.size(); i++){
		rst += arr[i];
		if(i != arr.size() - 1) rst += ", ";
	}
	return rst;
}

int main(){
	int cnt = 0;
	for(int y = 0; y < 8; y++){
		for(int x = 0; x < 8; x++){
			if(y % 2 == 0){
				hsvweight[(y/2)*8+x].u_h = unscaled_weight[cnt++];
				hsvweight[(y/2)*8+x].u_s = unscaled_weight[cnt++];
				hsvweight[(y/2)*8+x].u_v = unscaled_weight[cnt++];
			}else{
				hsvweight[(y/2)*8+x].b_h = unscaled_weight[cnt++];
				hsvweight[(y/2)*8+x].b_s = unscaled_weight[cnt++];
				hsvweight[(y/2)*8+x].b_v = unscaled_weight[cnt++];
			}

		}
	}

	for(int y = 0; y < 8; y++){
		for(int x = 0; x < 8; x++){
			if(y % 2 == 0){
				bgrweight[(y/2)*8+x].u_b = unscaled_weight[cnt++];
				bgrweight[(y/2)*8+x].u_g = unscaled_weight[cnt++];
				bgrweight[(y/2)*8+x].u_r = unscaled_weight[cnt++];
			}else{
				bgrweight[(y/2)*8+x].b_b = unscaled_weight[cnt++];
				bgrweight[(y/2)*8+x].b_g = unscaled_weight[cnt++];
				bgrweight[(y/2)*8+x].b_r = unscaled_weight[cnt++];
			}

		}
	}

	for(int y = 0; y < 3; y++){
		for(int x = 0; x < 7; x++){
			int index = y * 7 + x;
			//ul, ur, bl, br
			for(int blocki = 0; blocki < 4; blocki++){
				for(int bini = 0; bini < 9; bini++){
					if(blocki == 0) hogweight[index].ul[bini] = unscaled_weight[cnt++];
					if(blocki == 1) hogweight[index].ur[bini] = unscaled_weight[cnt++];
					if(blocki == 2) hogweight[index].bl[bini] = unscaled_weight[cnt++];
					if(blocki == 3) hogweight[index].br[bini] = unscaled_weight[cnt++];
				}
			}
		}
	}

	vector<string> strs;

	string jsonstr = "";
	jsonstr += "{";
	//bgrhsv_w1
	strs.clear();
	jsonstr += "\"bgrhsv_w1\" : [";
	for(int i = 0; i < 8; i++){
		strs.push_back(to_string(convFixedToUint(bgrweight[i].b_b)));
		strs.push_back(to_string(convFixedToUint(bgrweight[i].u_b)));
		strs.push_back(to_string(convFixedToUint(hsvweight[i].b_h)));
		strs.push_back(to_string(convFixedToUint(hsvweight[i].u_h)));


		strs.push_back(to_string(convFixedToUint(bgrweight[i].b_g)));
		strs.push_back(to_string(convFixedToUint(bgrweight[i].u_g)));
		strs.push_back(to_string(convFixedToUint(hsvweight[i].b_s)));
		strs.push_back(to_string(convFixedToUint(hsvweight[i].u_s)));

		strs.push_back(to_string(convFixedToUint(bgrweight[i].b_r)));
		strs.push_back(to_string(convFixedToUint(bgrweight[i].u_r)));
		strs.push_back(to_string(convFixedToUint(hsvweight[i].b_v)));
		strs.push_back(to_string(convFixedToUint(hsvweight[i].u_v)));
	}
	jsonstr += joinstr(strs);
	jsonstr += "],\n";

	//bgrhsv_w2
	strs.clear();
	jsonstr += "\"bgrhsv_w2\" : [";
	for(int i = 8; i < 16; i++){
		strs.push_back(to_string(convFixedToUint(bgrweight[i].b_b)));
		strs.push_back(to_string(convFixedToUint(bgrweight[i].u_b)));
		strs.push_back(to_string(convFixedToUint(hsvweight[i].b_h)));
		strs.push_back(to_string(convFixedToUint(hsvweight[i].u_h)));


		strs.push_back(to_string(convFixedToUint(bgrweight[i].b_g)));
		strs.push_back(to_string(convFixedToUint(bgrweight[i].u_g)));
		strs.push_back(to_string(convFixedToUint(hsvweight[i].b_s)));
		strs.push_back(to_string(convFixedToUint(hsvweight[i].u_s)));

		strs.push_back(to_string(convFixedToUint(bgrweight[i].b_r)));
		strs.push_back(to_string(convFixedToUint(bgrweight[i].u_r)));
		strs.push_back(to_string(convFixedToUint(hsvweight[i].b_v)));
		strs.push_back(to_string(convFixedToUint(hsvweight[i].u_v)));
	}
	jsonstr += joinstr(strs);
	jsonstr += "],\n";

	//bgrhsv_w3
	strs.clear();
	jsonstr += "\"bgrhsv_w3\" : [";
	for(int i = 16; i < 24; i++){
		strs.push_back(to_string(convFixedToUint(bgrweight[i].b_b)));
		strs.push_back(to_string(convFixedToUint(bgrweight[i].u_b)));
		strs.push_back(to_string(convFixedToUint(hsvweight[i].b_h)));
		strs.push_back(to_string(convFixedToUint(hsvweight[i].u_h)));


		strs.push_back(to_string(convFixedToUint(bgrweight[i].b_g)));
		strs.push_back(to_string(convFixedToUint(bgrweight[i].u_g)));
		strs.push_back(to_string(convFixedToUint(hsvweight[i].b_s)));
		strs.push_back(to_string(convFixedToUint(hsvweight[i].u_s)));

		strs.push_back(to_string(convFixedToUint(bgrweight[i].b_r)));
		strs.push_back(to_string(convFixedToUint(bgrweight[i].u_r)));
		strs.push_back(to_string(convFixedToUint(hsvweight[i].b_v)));
		strs.push_back(to_string(convFixedToUint(hsvweight[i].u_v)));
	}
	jsonstr += joinstr(strs);
	jsonstr += "],\n";

	//bgrhsv_w4
	strs.clear();
	jsonstr += "\"bgrhsv_w4\" : [";
	for(int i = 24; i < 32; i++){
		strs.push_back(to_string(convFixedToUint(bgrweight[i].b_b)));
		strs.push_back(to_string(convFixedToUint(bgrweight[i].u_b)));
		strs.push_back(to_string(convFixedToUint(hsvweight[i].b_h)));
		strs.push_back(to_string(convFixedToUint(hsvweight[i].u_h)));


		strs.push_back(to_string(convFixedToUint(bgrweight[i].b_g)));
		strs.push_back(to_string(convFixedToUint(bgrweight[i].u_g)));
		strs.push_back(to_string(convFixedToUint(hsvweight[i].b_s)));
		strs.push_back(to_string(convFixedToUint(hsvweight[i].u_s)));

		strs.push_back(to_string(convFixedToUint(bgrweight[i].b_r)));
		strs.push_back(to_string(convFixedToUint(bgrweight[i].u_r)));
		strs.push_back(to_string(convFixedToUint(hsvweight[i].b_v)));
		strs.push_back(to_string(convFixedToUint(hsvweight[i].u_v)));
	}
	jsonstr += joinstr(strs);
	jsonstr += "],\n";

	//hog_w1
	strs.clear();
	jsonstr += "\"hog_w1\" : [";
	for(int i = 0; i < 7; i++){
		for(int j = 0; j < 9; j++){
			strs.push_back(to_string(convFixedToUint(hogweight[i].br[j])));
			strs.push_back(to_string(convFixedToUint(hogweight[i].bl[j])));
			strs.push_back(to_string(convFixedToUint(hogweight[i].ur[j])));
			strs.push_back(to_string(convFixedToUint(hogweight[i].ul[j])));
		}
	}
	jsonstr += joinstr(strs);
	jsonstr += "],\n";

	//hog_w1
	strs.clear();
	jsonstr += "\"hog_w2\" : [";
	for(int i = 7; i < 14; i++){
		for(int j = 0; j < 9; j++){
			strs.push_back(to_string(convFixedToUint(hogweight[i].br[j])));
			strs.push_back(to_string(convFixedToUint(hogweight[i].bl[j])));
			strs.push_back(to_string(convFixedToUint(hogweight[i].ur[j])));
			strs.push_back(to_string(convFixedToUint(hogweight[i].ul[j])));
		}
	}
	jsonstr += joinstr(strs);
	jsonstr += "],\n";

	//hog_w1
	strs.clear();
	jsonstr += "\"hog_w3\" : [";
	for(int i = 14; i < 21; i++){
		for(int j = 0; j < 9; j++){
			strs.push_back(to_string(convFixedToUint(hogweight[i].br[j])));
			strs.push_back(to_string(convFixedToUint(hogweight[i].bl[j])));
			strs.push_back(to_string(convFixedToUint(hogweight[i].ur[j])));
			strs.push_back(to_string(convFixedToUint(hogweight[i].ul[j])));
		}
	}
	jsonstr += joinstr(strs);
	jsonstr += "]\n";

	//end
	jsonstr += "}";

	cout << jsonstr << endl;
}
