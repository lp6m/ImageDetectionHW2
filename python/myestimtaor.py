import pickle
import graphviz
import numpy as np
from skimage.feature import hog
from sklearn.svm import LinearSVC
from sklearn.ensemble import AdaBoostClassifier, RandomForestClassifier
from sklearn.metrics import recall_score
from sklearn.preprocessing import StandardScaler
from sklearn.model_selection import train_test_split
from sklearn.utils import shuffle
import cv2
import vdtools

#load trained models and scaler
clf = pickle.load(open( './cache/clf.p', 'rb'))
weights = clf.coef_[0]
bias = clf.intercept_
scaler_mean = pickle.load(open( "./cache/scaler_mean.p", "rb" ))
scaler_std = pickle.load(open( "./cache/scaler_std.p", "rb" ))

#get geatures
img = cv2.imread("input.png")
feature = vdtools.WindowFinder().singleimgfeatures(img)

#1. estimate in same way as scikit-learn
scaled_feature = (feature - scaler_mean) / (scaler_std)
rst = np.dot(scaled_feature, weights) + bias
sigmoided_rst = 1/(1+np.exp(-1*rst))
print("estimate in same way as scikit-learn...")
print("rst", rst, "sigmoided_rst", sigmoided_rst)

#2. estimate by using unscaled weight
unscaled_weight = weights / scaler_std
unscaled_bias = bias + (- 1) * np.sum((scaler_mean * weights) / scaler_std)
rst2 = np.dot(feature, unscaled_weight) + unscaled_bias
sigmoided_rst2 = 1/(1+np.exp(-1*rst2))
print("estimate by using unscaled_weight")
print("rst2", rst2, "sigmoided_rst2", sigmoided_rst2)

#3. print unscaled weights and bias
weight_num = len(unscaled_weight)
code = ""
unscaled_weight_strarray = list(map(lambda val: "{:.8g}".format(val), unscaled_weight))

code += "double unscaled_weight[{}] = {{\n".format(weight_num)  + "{}\n}};\n".format(",\n".join(map(lambda list: " " * 31 + ", ".join(list), np.array(unscaled_weight_strarray).reshape(-1, 10))))
code += "double unscaled_bias = {:.8g};\n".format(unscaled_bias[0])
with open('./output/weights.h', mode='w') as f:
    f.write(code)
