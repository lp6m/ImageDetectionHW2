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

bgr_weight = unscaled_weight[0:192].reshape(8,8,3)
hsv_weight = unscaled_weight[192:384].reshape(8,8,3)
hog_weight = unscaled_weight[384:].reshape(3,7,4,9);

code += "const weight WeightData[WINDOW_BLOCKNUM_H][WINDOW_BLOCKNUM_W] = {\n"
for by in range(3):
    code += "{"
    for bx in range(7):
        code += "{"
        for part in range(4):
            code += "{"
            for bin_index in range(9):
                code += "{:.8g}".format(hog_weight[by][bx][part][bin_index])
                if bin_index != 8:
                	code += ", "
            code += "}"
            if part != 3:
                code += ", "
        code += "}"
        if bx != 6:
            code += ", \n"
    code += "}"
    if by != 2:
        code += ", \n"
code += "\n};"

code += "\n"

code += "const pixweight WeightData[4][8] = {\n"
for y in range(4):
    code += "{"
    for x in range(8):
        code += "{"
        code += "{" + "{:.8g}".format(bgr_weight[2*y][x][0]) + ", " + "{:.8g}".format(bgr_weight[2*y][x][1]) + ", " +  "{:.8g}".format(bgr_weight[2*y][x][2]) + "}, "
        code += "{" + "{:.8g}".format(bgr_weight[2*y][x][0]) + ", " + "{:.8g}".format(bgr_weight[2*y][x][1]) + ", " +  "{:.8g}".format(bgr_weight[2*y][x][2]) + "}, "
        code += "{" + "{:.8g}".format(hsv_weight[2*y+1][x][0]) + ", " + "{:.8g}".format(hsv_weight[2*y+1][x][1]) + ", " + "{:.8g}".format(hsv_weight[2*y+1][x][2]) + "}, "
        code += "{" + "{:.8g}".format(hsv_weight[2*y+1][x][0]) + ", " + "{:.8g}".format(hsv_weight[2*y+1][x][1]) + ", " + "{:.8g}".format(hsv_weight[2*y+1][x][2]) + "}"
        code += "}"
        if(x != 7):
            code += ", \n"
    code += "}\n"
    if y != 3:
        code += ", \n"
code += "\n};"
with open('./output/weights.h', mode='w') as f:
    f.write(code)
