# util  
## ap_fixed_convert  
This utility works on Vivado HLS.  
It expresses the trained weight in 32-bit fixed point and convert it to the value interpreted as 32-bit unsigned int notation.  
- main.cpp  
 dummy code. This Vivado HLS project is just conversion utility.  
- main_tb.cpp  
 simple conversion code  
- weight.h  
 declare trained weight array genarated by `python/myestimator.py`  

The generated JSON file is like the following. JSON file is used in  `sw/` project to set trained parameter to BRAM. 
```
{"bgrhsv_w1" : [286925, 414125, ...],
"bgrhsv_w2" : [4294678382, 4294426082, ...],
"bgrhsv_w3" : [136128, 4294683817, ...],
"hog_w1" : [535309, 4293005267, ...],
"hog_w2" : [93835, 4293342059, ...],
"hog_w3" : [4288671221, 4293277452, ...}
```