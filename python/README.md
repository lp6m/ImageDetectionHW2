###Python
Python3 and Scikit-learn are used to train object, and extracted trained classifier paramet

####vdtools.py
MainClass, this refers [https://github.com/t-lanigan/vehicle-detection-and-tracking](https://github.com/t-lanigan/vehicle-detection-and-tracking) repository.  
This file contains many unnecessary methods for now.  
This file is module, so this file is not executed alone.  

###myhog.py
This file is module, so this file is not executed alone.
In this repo, scikit-learn HOG function is not used. This module extracts L-1 sqrt normalized HOG feature.

####train.py
Train the object.  
set `self.load_saved = True` in vdtools.py.


###estimatetestor.py
You can check model performance on Tkinter GUI  

|key              |behavior                  | 
|-----------------|--------------------------| 
|<-               | move to previous image   | 
|->               | move to next image       |  
|mouse scroll up  | make a rectangle bigger  | 
|mouse scroll down| make a rectanle smaller  |  
|drag mouse       | move a rectangle |


###myestimator.py
Extract SVM parameter from trained model, and estimation is conducted without scikit-learn.  
Extracted parameter is exported in C++ code form to ./output/weights.h  
```
double unscaled_weight[1140] = {
                               0.00026532167, 0.00010114468, 0.00025412301, 0.00089063402,
                               ...
}
double unscaled_bias = -6.6535004;
```

