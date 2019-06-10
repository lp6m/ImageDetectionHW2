### Python
Python3 and Scikit-learn are used to train object, and extracted trained classifier parameter.  

#### vdtools.py
MainClass, this refers [https://github.com/t-lanigan/vehicle-detection-and-tracking](https://github.com/t-lanigan/vehicle-detection-and-tracking) repository.  
This file is module, so this file is not executed alone.  

#### myhog.py
This file is module, so this file is not executed alone.  
In this repo, scikit-learn HOG function library is not used. This module extracts L1 normalized HOG feature.

#### train.py
Train the object.  
Set `self.load_saved = True` in vdtools.py to train.
Trained data is saved in `./cache` directory.

#### estimatetestor.py
You can check model performance on Tkinter GUI  

|key              |behavior                  | 
|-----------------|--------------------------| 
|<-               | move to previous image   | 
|->               | move to next image       |  
|mouse scroll up  | make a rectangle bigger  | 
|mouse scroll down| make a rectanle smaller  |  
|drag mouse       | move a rectangle |


#### myestimator.py
Extract SVM parameter from cached trained model in `./cache` directory, and estimation is conducted without scikit-learn.  
Extracted parameter is exported in C++ code form to `./output/weights.h`    
The trained SVM weight is inverse-scaled. In `vdtools.py`, scaling is applied to extracted feature before input to SVM.  
Perform inverse-scaling on weights and biases to remove the necessity to perform scaling on extracted feature on HW.  

```
double unscaled_weight[1140] = {
                               0.00026532167, 0.00010114468, 0.00025412301, 0.00089063402,
                               ...
}
double unscaled_bias = -6.6535004;
```

