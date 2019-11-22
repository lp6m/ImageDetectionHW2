import cv2
from skimage.feature import hog
import numpy as np
import math

INPUT_WIDTH = 64
INPUT_HEIGHT = 32 
CELL_SIZE = 8
BLOCK_SIZE = 2
CELL_NUM_WIDTH = (INPUT_WIDTH/CELL_SIZE)
CELL_NUM_HEIGHT = (INPUT_HEIGHT/CELL_SIZE)
BLOCK_NUM_WIDTH = (CELL_NUM_WIDTH-1)
BLOCK_NUM_HEIGHT = (CELL_NUM_HEIGHT-1)


def _hog_channel_gradient(channel):
    g_row = np.empty(channel.shape, dtype=np.int16)
    g_row[0, :] = 0
    g_row[-1, :] = 0
    g_row[1:-1, :] = channel[2:, :] - channel[:-2, :]
    g_col = np.empty(channel.shape, dtype=np.int16)
    g_col[:, 0] = 0
    g_col[:, -1] = 0
    g_col[:, 1:-1] = channel[:, 2:] - channel[:, :-2]
    return g_row, g_col

def approx_distance(dx, dy):
    min = 0
    max = 0
    if(dx < 0):
        dx = -dx;
    if(dy < 0):
        dy = -dy;

    if(dx < dy):
        min = dx;
        max = dy;
    else:
        min = dy;
        max = dx;
    

    return ((( max << 8 ) + ( max << 3 ) - ( max << 4 ) - ( max << 1 ) +
                ( min << 7 ) - ( min << 5 ) + ( min << 3 ) - ( min << 1 )) >> 8 )

lut0 = [0,0,0,1,1,1,2,2,2,3,3,3,4,4,5,5,5,6,6,6,7,7,7,8,8,9,9,9,10,10,10,11,11,11,12,12,13,13,13,14,14,14,15,15,15,16,16,17,17,17,18,18,18,19,19,19,20,20,21,21,21,22,22,22,23,23,23,24,24,25,25,25,26,26,26,27,27,27,28,28,29,29,29,30,30,30,31,31,31,32,32,33,33,33,34,34,34,35,35,35,36,36,37,37,37,38,38,38,39,39,39,40,40,41,41,41,42,42,42,43,43,43,44,44,45,45,45,46,46,46,47,47,47,48,48,49,49,49,50,50,50,51,51,51,52,52,53,53,53,54,54,54,55,55,55,56,56,57,57,57,58,58,58,59,59,59,60,60,61,61,61,62,62,62,63,63,63,64,64,65,65,65,66,66,66,67,67,67,68,68,69,69,69,70,70,70,71,71,71,72,72,73,73,73,74,74,74,75,75,75,76,76,77,77,77,78,78,78,79,79,79,80,80,81,81,81,82,82,82,83,83,83,84,84,85,85,85,86,86,86,87,87,87,88,88,89,89,89,90,90,90,91,91,91,92,92]
lut1 = [0,0,1,2,3,4,5,5,6,7,8,9,10,10,11,12,13,14,15,15,16,17,18,19,20,20,21,22,23,24,25,26,26,27,28,29,30,31,31,32,33,34,35,36,36,37,38,39,40,41,41,42,43,44,45,46,46,47,48,49,50,51,52,52,53,54,55,56,57,57,58,59,60,61,62,62,63,64,65,66,67,67,68,69,70,71,72,72,73,74,75,76,77,78,78,79,80,81,82,83,83,84,85,86,87,88,88,89,90,91,92,93,93,94,95,96,97,98,98,99,100,101,102,103,104,104,105,106,107,108,109,109,110,111,112,113,114,114,115,116,117,118,119,119,120,121,122,123,124,124,125,126,127,128,129,130,130,131,132,133,134,135,135,136,137,138,139,140,140,141,142,143,144,145,145,146,147,148,149,150,150,151,152,153,154,155,156,156,157,158,159,160,161,161,162,163,164,165,166,166,167,168,169,170,171,171,172,173,174,175,176,177,177,178,179,180,181,182,182,183,184,185,186,187,187,188,189,190,191,192,192,193,194,195,196,197,197,198,199,200,201,202,203,203,204,205,206,207,208,208,209,210,211,212,213,213]
lut2 = [0,1,3,5,6,8,10,12,13,15,17,19,20,22,24,25,27,29,31,32,34,36,38,39,41,43,44,46,48,50,51,53,55,57,58,60,62,63,65,67,69,70,72,74,76,77,79,81,83,84,86,88,89,91,93,95,96,98,100,102,103,105,107,108,110,112,114,115,117,119,121,122,124,126,127,129,131,133,134,136,138,140,141,143,145,147,148,150,152,153,155,157,159,160,162,164,166,167,169,171,172,174,176,178,179,181,183,185,186,188,190,191,193,195,197,198,200,202,204,205,207,209,210,212,214,216,217,219,221,223,224,226,228,230,231,233,235,236,238,240,242,243,245,247,249,250,252,254,255,257,259,261,262,264,266,268,269,271,273,274,276,278,280,281,283,285,287,288,290,292,294,295,297,299,300,302,304,306,307,309,311,313,314,316,318,319,321,323,325,326,328,330,332,333,335,337,338,340,342,344,345,347,349,351,352,354,356,358,359,361,363,364,366,368,370,371,373,375,377,378,380,382,383,385,387,389,390,392,394,396,397,399,401,402,404,406,408,409,411,413,415,416,418,420,421,423,425,427,428,430,432,434,435,437,439,441]
lut3 = [0,5,11,17,22,28,34,39,45,51,56,62,68,73,79,85,90,96,102,107,113,119,124,130,136,141,147,153,158,164,170,175,181,187,192,198,204,209,215,221,226,232,238,243,249,255,260,266,272,277,283,289,294,300,306,311,317,323,328,334,340,345,351,357,362,368,374,379,385,391,396,402,408,413,419,425,430,436,442,447,453,459,464,470,476,481,487,493,498,504,510,515,521,527,532,538,544,549,555,561,566,572,578,584,589,595,601,606,612,618,623,629,635,640,646,652,657,663,669,674,680,686,691,697,703,708,714,720,725,731,737,742,748,754,759,765,771,776,782,788,793,799,805,810,816,822,827,833,839,844,850,856,861,867,873,878,884,890,895,901,907,912,918,924,929,935,941,946,952,958,963,969,975,980,986,992,997,1003,1009,1014,1020,1026,1031,1037,1043,1048,1054,1060,1065,1071,1077,1082,1088,1094,1099,1105,1111,1116,1122,1128,1133,1139,1145,1150,1156,1162,1168,1173,1179,1185,1190,1196,1202,1207,1213,1219,1224,1230,1236,1241,1247,1253,1258,1264,1270,1275,1281,1287,1292,1298,1304,1309,1315,1321,1326,1332,1338,1343,1349,1355,1360,1366,1372,1377,1383,1389,1394,1400,1406,1411,1417,1423,1428,1434,1440,1445]

def myhog(input_image):
    sum_of_block = [0] * BLOCK_NUM_WIDTH * BLOCK_NUM_HEIGHT
    descriptor = [0] * BLOCK_NUM_WIDTH * BLOCK_NUM_HEIGHT * BLOCK_SIZE * BLOCK_SIZE * 9
    dst = [0] * BLOCK_NUM_WIDTH * BLOCK_NUM_HEIGHT * BLOCK_SIZE * BLOCK_SIZE * 9 
    gxlist = []

    binarray = [0] * (INPUT_WIDTH*INPUT_HEIGHT)
    magarray = [0] * (INPUT_WIDTH*INPUT_HEIGHT)
    image = cv2.resize(input_image, (INPUT_WIDTH, INPUT_HEIGHT))
    image = image.astype(np.int16)

    for y in range(INPUT_HEIGHT):
        for x in range(INPUT_WIDTH):
            mag = 0
            bin_index = 0
            if y == 0 or y == (INPUT_HEIGHT-1) or x == 0 or x == (INPUT_WIDTH-1):
                bin_index = 0
                mag = 0
            else:
                Gx = image[y][x+1] - image[y][x-1]
                Gy = image[y+1][x] - image[y-1][x]
                a = np.array([image[y][x+1], image[y+1][x]])
                b = np.array([image[y][x-1], image[y-1][x]])

                mag = np.linalg.norm(a-b)
                #bin_index = np.int((np.rad2deg(np.arctan2(Gx, Gy)) % 180) // 20) scikit-learn mode
                if((Gx >= 0 and Gy >= 0) or (Gx <= 0 and Gy <= 0)):
                    if (abs(Gy) < lut0[abs(Gx)]):
                        bin_index = 0
                    elif (abs(Gy) < lut1[abs(Gx)]):
                        bin_index = 1
                    elif (abs(Gy) < lut2[abs(Gx)]):
                        bin_index = 2
                    elif (abs(Gy) < lut3[abs(Gx)]):
                        bin_index = 3
                    else:
                        bin_index = 4
                else:
                    if (abs(Gy) <  lut0[abs(Gx)]):
                        bin_index = 4
                    elif (abs(Gy) <  lut1[abs(Gx)]):
                        bin_index = 5
                    elif (abs(Gy) <  lut2[abs(Gx)]):
                        bin_index = 6
                    elif (abs(Gy) <  lut3[abs(Gx)]):
                        bin_index = 7
                    else:
                        bin_index = 8
            binarray[y*INPUT_WIDTH+x] = bin_index
            magarray[y*INPUT_WIDTH+x] = mag

    for y in range(BLOCK_NUM_HEIGHT):
        for x in range(BLOCK_NUM_WIDTH):
            for yy in range(CELL_SIZE*BLOCK_SIZE):
                for xx in range(CELL_SIZE*BLOCK_SIZE):
                    ny = CELL_SIZE * y + yy
                    nx = CELL_SIZE * x + xx
                    bin_index = binarray[ny * INPUT_WIDTH + nx]
                    cell_index = (yy // CELL_SIZE) * BLOCK_SIZE + (xx // CELL_SIZE);
                    block_index = y * BLOCK_NUM_WIDTH + x;
                    descrip_index = block_index * BLOCK_SIZE * BLOCK_SIZE * 9 + cell_index * 9 + bin_index;

                    descriptor[descrip_index] += magarray[ny * INPUT_WIDTH + nx];
                    sum_of_block[block_index] += magarray[ny * INPUT_WIDTH + nx];

    average = 0
    # blkSize = 36
    for by in range(BLOCK_NUM_HEIGHT):
        for bx in range(BLOCK_NUM_WIDTH):
            blkIdx = by * BLOCK_NUM_WIDTH + bx
            for i in range(BLOCK_SIZE*BLOCK_SIZE*9):
                index = blkIdx * (BLOCK_SIZE*BLOCK_SIZE*9) + i
                if sum_of_block[blkIdx] == 0 :
                    dst[index] = 0
                else:
                    dst[index] = np.sqrt(descriptor[index] / sum_of_block[blkIdx])
                    # dst[index] = descriptor[index] / (sum_of_block[blkIdx] + 1e-5)
                    

    return np.array(dst)
    
# np.set_printoptions(linewidth=100)
# img = cv2.imread("input.png")
# gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
# print(myhog(gray))