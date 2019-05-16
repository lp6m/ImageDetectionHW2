import matplotlib.image as mpimg
import matplotlib.pyplot as plt
import numpy as np
import cv2
from skimage.feature import hog
from sklearn.svm import LinearSVC
from sklearn.ensemble import AdaBoostClassifier, RandomForestClassifier
from sklearn.metrics import recall_score
from sklearn.preprocessing import StandardScaler
from sklearn.model_selection import train_test_split
from sklearn.utils import shuffle

from scipy.ndimage.measurements import label
import math

import glob
import pickle
import time
import myhog

from sklearn import svm
from sklearn.externals import joblib
# from sklearn import cross_validation
# from sklearn.cross_validation import train_test_split
from sklearn.model_selection import GridSearchCV

class WindowFinder(object):
    """Finds windows in an image that contain a car."""
    def __init__(self):
        
        ### Hyperparameters, if changed ->(load_saved = False) If
        ### the classifier is changes load_feaures can be True

        self.load_saved     = True# Loads classifier and scaler
        self.load_features  = False # Loads saved features (to train new classifier)

        self.spatial_size   = (8, 8)
        self.spatial_feat   = True # Spatial features on or off
        self.hist_feat      = False # Histogram features on or off
        self.hog_feat       = True # HOG features on or off

        self.window_range_minx = 0.15
        self.window_range_maxx = 0.85
        self.window_range_miny = 0.0
        self.window_range_maxy = 0.6

        # The locations of all the data.
        self.notred_data_folders = ['../data/fpt/not_red_shukai', '../data/fpt/not_red_signal', '../data/fpt/not_red_wall', '../data/fpt/not_red_wall2', '../data/not_red_from_itweek']
        self.red_data_folders = ['../data/fordate', '../data/fpt/red_shukai', '../data/fpt/red_shukai2', '../data/fpt/red_not_pittiri', '../data/fpt/fpt_red_siro_wall']
        # self.notred_data_folders = ['../data/fpt/not_red_shukai', '../data/fpt/not_red_signal']
        # self.red_data_folders = ['../data/fordate', '../data/fpt/red_shukai']
        
        ######Classifiers                            
        self.pred_thresh = 0.65 #Increase to decrease likelihood of detection.
        
        ###### Variable for Classifier and Feature Scaler ##########
        # self.untrained_clf = RandomForestClassifier(n_estimators=100, max_features = 2, min_samples_leaf = 4,max_depth = 25)
        tuned_parameters = [{'C': [0.1, 0.5, 1.0, 5.0, 10.0, 50.0, 100.0]}]
        # tuned_parameters = [{'C': [0.1]}]
        self.grid_search = GridSearchCV(svm.LinearSVC(), tuned_parameters, cv=5)

        self.trained_clf, self.scaler = self.__get_classifier_and_scaler()

        self.windows_list = []
        self.__make_windows()
        ###### Variables for CNN ##########

        # print('Loading Neural Network...')
        # self.nn = load_model('models/keras(32x32).h5')
        # self.nn_train_size = (32,32) # size of training data used for CNN
        # self.nn.summary()
        # print('Neural Network Loaded.')



    def __get_classifier_and_scaler(self):
        """
        Gets the classifier and scaler needed for the rest of the operations. Loads from cache if 
        load_saved is set to true.
        """
        if self.load_saved:
            print('Loading saved classifier and scaler...')
            clf = pickle.load( open( "./cache/clf.p", "rb" ) )
            # print('%s loaded...' % self.untrained_clf.__class__.__name__)
            scaler = pickle.load(open( "./cache/scaler.p", "rb" ))
            print(clf.get_params())
            # np.set_printoptions(precision=3)
            np.set_printoptions(suppress=True)	
            np.set_printoptions(precision=6, floatmode='fixed')
        else:
            # Split up data into randomized training and test sets
            print('Training...')
            # print('Training a %s...' % self.untrained_clf.__class__.__name__)
            rand_state = np.random.randint(0, 100)
            
            notred_features, red_features, filenames = self.__get_features()
            scaled_X, y, scaler = self.__get_scaled_X_y(notred_features, red_features)

            test_size = 0.05
            X_train, X_test, y_train, y_test = train_test_split(
                scaled_X, y, test_size=test_size, random_state=rand_state)

            gscv = self.grid_search
            # Check the training time for the SVC
            t=time.time()
            gscv.fit(X_train, y_train)
            t2 = time.time()
            print(round(t2-t, 2), 'Seconds to train CLF...')
            # Extract best estimator
            clf = gscv.best_estimator_
            print('Grid Search is finished, search result of  C =', clf.C)

            # Check the score of the SVC
            # preds = clf.predict_proba(X_test)
            # preds = clf.decision_function(X_test)
            preds = 1/(1+(np.exp(-1*clf.decision_function(X_test))))
            print(preds)
            #get filename
            test_filenames = shuffle(filenames, random_state=rand_state)[len(scaled_X) - len(preds):]
            print(len(test_filenames), len(preds))
            for i, proba in enumerate(preds):
                correct = False
                ans = -1
                ans_proba = 0
                if proba < 0.5:
                    ans = 0
                    ans_proba = (1.0-proba)*100.0
                else:
                    ans = 1
                    ans_proba = proba * 100.0
                if ans == y_test[i]:
                    correct = True
                if correct == False:
                    print('\033[31mproba = {}, predict = {}, correct = {}, testcase = {}\033[0m'.format(round(ans_proba, 3), ans, correct, test_filenames[i]))
                else:
                    print('proba = {}, predict = {}, correct = {}, testcase = {}'.format(round(ans_proba, 3), ans, correct, test_filenames[i]))
            # print('Test Recall of CLF = ', round(recall_score(y_test, preds), 4))
            # print('Test Recall of CLF = ', round(precision_score(y_test, preds), 4))
            # Check the prediction time for a single sample
            t=time.time()

            print('Pickling classifier and scaler...')
            pickle.dump( clf, open( "./cache/clf.p", "wb" ) )
            pickle.dump( scaler, open( "./cache/scaler.p", "wb" ) )

        return clf, scaler
           
    def __get_features(self):
        """
        Gets features either by loading them from cache, or by extracting them from the data.
        """   
        if self.load_features:
            print('Loading saved features...')
            car_features, notcar_features = pickle.load( open( "./cache/features.p", "rb" ) )
            
        else: 
            # print("Extracting features from %s samples..." % self.sample_size)          
            print("Extracting features...")          

            notreds = []
            reds = []
            filenames = []

            for folder in self.notred_data_folders:
                image_paths =glob.glob(folder+'/*')
                for path in image_paths:
                    notreds.append(path)
            for folder in self.red_data_folders:
                image_paths =glob.glob(folder+'/*')
                for path in image_paths:
                    reds.append(path)

            filenames.extend(notreds)
            filenames.extend(reds)
            # notreds = notreds[0:self.sample_size]
            # reds =  reds[0:self.sample_size]


            start = time.clock()
            notred_features = self.__extract_features(notreds)
            red_features = self.__extract_features(reds)

            end = time.clock()
            print("Running time : %s seconds" % (end - start))
            
            print('Pickling features...')
            pickle.dump((notred_features, red_features), open( "./cache/features.p", "wb" ))
            
        return notred_features, red_features, filenames

    def __extract_features(self, imgs):
        """
        Extract features from image files.
        """
        
        # Create a list to append feature vectors to
        features = []
        # Iterate through the list of images
        for file in imgs:
            # Read in each one by one
            # image = mpimg.imread(file)
            image = cv2.imread(file)
            # Get features for one image
            file_features = self.__single_img_features(image)
            #Append to master list
            features.append(file_features)
        # Return list of feature vectors
        return features
   

    def __single_img_features(self, img):

        """
        Define a function to extract features from a single image window
        This function is very similar to extract_features()
        just for a single image rather than list of images
        Define a function to extract features from a single image window
        This function is very similar to extract_features()
        just for a single image rather than list of images
        """
        #1) Define an empty list to receive features
        img_features = []
        #2) Apply color conversion if other than 'RGB'

        hls = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)# convert it to HLS
        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
   
        #3) Compute spatial features if flag is set
        if self.spatial_feat == True:
            spatial_hls = self.__bin_spatial(hls)
            spatial_rgb = self.__bin_spatial(img)

            img_features.append(spatial_hls)
            img_features.append(spatial_rgb)

        #5) Compute histogram features if flag is set
        if self.hist_feat == True:
            hist_features_hls = self.__color_hist(hls)
            hist_features_rgb = self.__color_hist(img)
            #6) Append features to list
            img_features.append(hist_features_hls)
            img_features.append(hist_features_rgb)
        #7) Compute HOG features if flag is set
        if self.hog_feat == True:

            hog_features = self.__get_hog_features(gray)
            #8) Append features to list
            img_features.append(hog_features)

        #9) Return concatenated array of img_features
        return np.concatenate(img_features)

    def singleimgfeatures(self, img):
        return self.__single_img_features(img)

    def __get_scaled_X_y(self, notred_features, red_features):
        X = np.vstack((notred_features, red_features)).astype(np.float64)                        
        # Fit a per-column scaler
        X_scaler = StandardScaler().fit(X)
        # Apply the scaler to X
        scaled_X = X_scaler.transform(X)

        # Define the labels vector
        y = np.hstack((np.zeros(len(notred_features)), np.ones(len(red_features))))

        # Pickle scaler parameter
        mean = np.nanmean(np.array(X), axis=0)
        std = np.nanstd(np.array(X), axis=0)
        pickle.dump(mean, open( "./cache/scaler_mean.p", "wb"))
        pickle.dump(std, open( "./cache/scaler_std.p", "wb"))

        return scaled_X, y, X_scaler

    # Define a function to return HOG features and visualization
    def __get_hog_features(self, img, vis=False, feature_vec=True):
        return myhog.myhog(img)


    # Define a function to compute binned color features  
    def __bin_spatial(self, img):
        features = cv2.resize(img, self.spatial_size, cv2.INTER_LINEAR).ravel()
        return features

    # Define a function to compute color histogram features 
    # NEED TO CHANGE bins_range if reading .png files with mpimg!
    def __color_hist(self, img, bins_range=(0, 256)):
        channel1_hist = np.histogram(img[:,:,0], bins=[0, 21, 42, 64, 85,106, 128, 149, 170, 192, 213, 234, 256])
        channel2_hist = np.histogram(img[:,:,1], bins=[0, 21, 42, 64, 85,106, 128, 149, 170, 192, 213, 234, 256])
        channel3_hist = np.histogram(img[:,:,2], bins=[0, 21, 42, 64, 85,106, 128, 149, 170, 192, 213, 234, 256])
        hist_features = np.concatenate((channel1_hist[0], channel2_hist[0], channel3_hist[0]))
        return hist_features

    # Define a function to extract features from a list of images
    # Have this function call bin_spatial() and color_hist()
    def predictoneimage(self, img):
        test_image = cv2.resize(img, (64, 32), cv2.INTER_LINEAR)
        features = self.__single_img_features(test_image)
        test_features = self.scaler.transform(np.array(features).reshape(1, -1))
        bias = self.trained_clf.intercept_
        dot = np.dot(self.trained_clf.coef_[0], test_features[0]) 
        rst = dot + bias
        sigmoided_rst = 1/(1+np.exp(-1*rst))
        print(dot.shape)
        print("rst:", rst, "sigmoid:", sigmoided_rst)
        # prediction = self.trained_clf.predict_proba(test_features)[:,1]
        prediction = 1/(1+(np.exp(-1*self.trained_clf.decision_function(test_features))))
        print(prediction)
        return prediction

    def __classify_windows(self, img, windows):
        """
        Define a function you will pass an image 
        and the list of windows to be searched (output of slide_windows())
        """

        #1) Create an empty list to receive positive detection windows
        on_windows = []
        probas = []

        #2) Iterate over all windows in the list
        rtime = 0.0
        ftime = 0.0
        ttime = 0.0
        ptime = 0.0
        for i,window in enumerate(windows):

            
            ######### Classifier HOG Feature Prediction #########
            t1 = time.time()
            test_img = cv2.resize(img[window[0][1]:window[1][1], window[0][0]:window[1][0]], (64, 32), cv2.INTER_NEAREST)
            # cv2.imwrite('resize/resized' + str(i) + '.png', test_img)
            # cv2.waitKey(0)
            t2 = time.time()
            features = self.__single_img_features(test_img)
            t3 = time.time()
            test_features = self.scaler.transform(np.array(features).reshape(1, -1))
            t4 = time.time()
            # pickle.dump(test_features, open( "./test_features.p", "wb" ) )
            #[[1,2..968]]
            #extract 2nd probability
            proba = self.trained_clf.predict_proba(test_features)
            # print(proba)
            prediction = self.trained_clf.predict_proba(test_features)[:,1]
            # pickle.dump(prediction, open( "./prediction.p", "wb" ) )
            t5 = time.time()
            
            ftime += (t3 - t2)
            ptime += (t5 - t4)
            ## SVC prediction
            # prediction = self.trained_clf.predict(test_features)


            ######### Neural Network Predicion ########
            # test_img = cv2.resize(img[window[0][1]:window[1][1], window[0][0]:window[1][0]],
            #                       self.nn_train_size)
            # test_img = self.__normalize_image(test_img)
            # test_img = np.reshape(test_img, (1,self.nn_train_size[0],self.nn_train_size[1],3))
            # prediction = self.nn.predict_classes(test_img, verbose=0)


            if prediction >= self.pred_thresh:
                on_windows.append(window)
                probas.append(prediction)

        print('feature time : ', ftime)
        print('prediction time : ', ptime)
        #8) Return windows for positive detections
        # print("Number of hot windows:", len(on_windows))
        # print("Number of windows:", len(windows))
        return on_windows, probas



    def __normalize_image(self, img):

        img = img.astype(np.float32)
        # Normalize image
        img = img / 255.0 - 0.5
        return img

    def __visualise_searchgrid_and_hot(self, img, windows, hot_windows, ax=None):
        """
        Draws the search grid and the hot windows.
        """

        # print('Hot Windows...', hot_windows)
        search_grid_img = self.__draw_boxes(img, windows, color=(0, 0, 255), thick=6)                    
        hot_window_img = self.__draw_boxes(img, hot_windows, color=(0, 0, 255), thick=6)                    

        f, (ax1, ax2) = plt.subplots(1, 2, figsize=(10,6))
        f.tight_layout()
        ax1.imshow(search_grid_img)
        ax1.set_title('Search Grid')
        ax2.imshow(hot_window_img)
        ax2.set_title('Hot Boxes')

        plt.show()

        return

    # Define a function to draw bounding boxes
    def __draw_boxes(self, img, bboxes, color=(0, 0, 255), thick=6):
        """Draws boxes on image from a list of windows"""

        # Make a copy of the image
        imcopy = np.copy(img)
        # Iterate through the bounding boxes
        for bbox in bboxes:
            # Draw a rectangle given bbox coordinates
            cv2.rectangle(imcopy, bbox[0], bbox[1], color, thick)
        # Return the image copy with boxes drawn
        return imcopy

    
    def __slide_windows(self, x_start_stop,
                                y_start_stop, xy_window,
                                xy_overlap,
                                visualise=False):
        xspan = x_start_stop[1] - x_start_stop[0]
        yspan = y_start_stop[1] - y_start_stop[0]
        # Compute the number of pixels per step in x/y
        nx_pix_per_step = np.int(xy_window[0]*(1 - xy_overlap[0]))
        ny_pix_per_step = np.int(xy_window[1]*(1 - xy_overlap[1]))
        # Compute the number of windows in x/y
        nx_windows = np.int(xspan/nx_pix_per_step) - 1
        ny_windows = np.int(yspan/ny_pix_per_step) - 1
        # Initialize a list to append window positions to
        window_list = []
        for ys in range(ny_windows):
            for xs in range(nx_windows):
                # Calculate window position
                startx = xs*nx_pix_per_step + x_start_stop[0]
                endx = startx + xy_window[0]
                starty = ys*ny_pix_per_step + y_start_stop[0]
                endy = starty + xy_window[1]
                
                # Append window position to list
                window_list.append(((startx, starty), (endx, endy)))
        print(nx_windows, ny_windows, xspan, yspan)
        return window_list

    def __make_windows(self):
        
        # define the minimum window size
        x_min =[640*self.window_range_minx, 640*self.window_range_maxx] #minimum window slide area sx, ex
        y_min =[480*self.window_range_miny, 480*self.window_range_maxy] #minimum window slise area sx, ey
        xy_min = (80, 40)

        # define the maxium window size
        x_max = x_min#[300, 1280]
        y_max = y_min#[400, 700]
        xy_max = (200, 100)
        # intermedian windows
        n = 4 # the number of total window sizes
        x = []
        y = []
        xy =[]
        # chose the intermediate sizes by interpolation.
        x_range_w = x_max[0] - x_min[0]
        y_range_h = y_max[0] - y_min[0]
        for i in range(n):
            #sx, ex
            x_start_stop =[int(x_min[0] + i*(x_max[0]-x_min[0])/(n-1)), 
                           int(x_min[1] + i*(x_max[1]-x_min[1])/(n-1))]
            #sy, ey
            y_start_stop =[int(y_min[0] + i*(y_max[0]-y_min[0])/(n-1)), 
                           int(y_min[1] + i*(y_max[1]-y_min[1])/(n-1))]
            #window width height
            xy_window    =[int(xy_min[0] + i*(xy_max[0]-xy_min[0])/(n-1)), 
                           int(xy_min[1] + i*(xy_max[1]-xy_min[1])/(n-1))]
            x.append(x_start_stop)
            y.append(y_start_stop)
            xy.append(xy_window)
        print(x)
        print(y)
        print(xy)

        windows1 = self.__slide_windows(x_start_stop= x[0], y_start_stop = y[0], 
                            xy_window= xy[0], xy_overlap=(0.5, 0.5))
        windows2 = self.__slide_windows(x_start_stop= x[1], y_start_stop = y[1], 
                            xy_window= xy[1], xy_overlap=(0.75, 0.5))
        windows3 = self.__slide_windows(x_start_stop= x[2], y_start_stop = y[2], 
                            xy_window= xy[2], xy_overlap=(0.75, 0.5))
        windows4 = self.__slide_windows(x_start_stop= x[3], y_start_stop = y[3], 
                            xy_window= xy[3], xy_overlap=(0.75, 0.5))
        self.windows_list = list(windows1 + windows2 + windows3 + windows4)
        print(len(windows1), len(windows2), len(windows3), len(windows4))
        pickle.dump( self.windows_list, open( "./cache/windows_list.p", "wb" ) )
        print("window_candidate :  {}".format(len(self.windows_list)))

    def get_hot_windows(self, img, visualise=False):
        def check_red_ratio(image):
            hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV_FULL)
            h = hsv[:, :, 0]
            s = hsv[:, :, 1]
            mask = np.zeros(h.shape, dtype=np.bool)
            mask[((h < 20) | (h > 200)) & (s > 128)] = True

            redgaso = np.count_nonzero(mask == True)
            ratio = round(float(redgaso*100.0/mask.size), 3)
            return ratio

        #remove arienai candidate
        t1 = time.time()
        rf_windows = []
        for window in self.windows_list:
            resized = cv2.resize(img[window[0][1]:window[1][1], window[0][0]:window[1][0]], (64, 32), cv2.INTER_LINEAR)
            if(check_red_ratio(resized) > 2.00):
                rf_windows.append(window)

        print('candidate %d -> %d' % (len(self.windows_list), len(rf_windows)))
        t2 = time.time()
        print('remove arienai time: ', t2-t1)
        hot_windows, probas = self.__classify_windows(img, rf_windows)
        # hot_windows = self.__classify_windows(img, self.windows_list)

        t3 = time.time()
        print('get_hot_windows() iteration time: ', t3 - t1)
       
        if visualise:
            window_img = self.__draw_boxes(img, hot_windows, color=(0, 0, 255), thick=6)                    


            plt.figure(figsize=(10,6))
            plt.imshow(window_img)
            plt.tight_layout()
            plt.show()
            # return window_img
            

        return hot_windows, probas