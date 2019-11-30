import cv2
import glob
import json
import math
import numpy as np
import pickle
import time

import myhog

import matplotlib.image as mpimg
import matplotlib.pyplot as plt
from scipy.ndimage.measurements import label
from skimage.feature import hog
from sklearn.svm import LinearSVC
from sklearn.ensemble import AdaBoostClassifier, RandomForestClassifier
from sklearn.metrics import recall_score
from sklearn.preprocessing import StandardScaler
from sklearn.model_selection import train_test_split
from sklearn.utils import shuffle
from sklearn import svm
from sklearn.externals import joblib
from sklearn.model_selection import GridSearchCV
from sklearn.metrics import make_scorer
from sklearn.metrics import f1_score

class WindowFinder(object):
    """Finds windows in an image that contain a car."""
    def __init__(self, cfgfilepath):


        jsonobject = None
        with open(cfgfilepath) as f:
            jobject = json.loads(f.read())
        self.spatial_size = (jobject["spatial_size"], jobject["spatial_size"])
        self.spatial_feat = jobject["spatial_feat"]# Spatial features on or off
        self.hog_feat = jobject["hog_feat"]# HOG features on or off

        ### Hyperparameters, if changed ->(load_saved = False) If
        ### the classifier is changes load_feaures can be True
        self.load_saved     = True # Histogram features on or off
        self.load_features  = False # Loads saved features (to train new classifier)

        # The locations of all the data.   
        self.negative_data_folders = jobject["negative_data_folders"]
        self.positive_data_folders = jobject["positive_data_folders"]

        self.svm_clf_name = jobject["svm_clf_name"]
        self.rf_clf_name = jobject["rf_clf_name"]
        self.svm_enable = jobject["svm_enable"]
        self.rf_enable = jobject["rf_enable"]

        self.scaler_name = jobject["scaler_name"]
        self.scaler_mean_name = jobject["scaler_mean_name"]
        self.scaler_std_name = jobject["scaler_std_name"]
        
        ######Classifiers                            
        self.pred_thresh = 0.65 #Increase to decrease likelihood of detection.
        
        ###### Variable for Classifier and Feature Scaler ##########
        #SVM
        tuned_parameters = [{'C': [0.1, 0.5, 1.0, 5.0, 10.0, 50.0, 100.0]}]
        self.svm_grid_search = GridSearchCV(svm.LinearSVC(max_iter = 1000000), tuned_parameters, cv=5)
        #RF
        forest_grid_param = {
            'n_estimators': [100],
            'max_features': [2, 4, 8, 'auto'],
            'max_depth': [25],
            'min_samples_leaf': [2, 4, 8]
        }
        f1_scoring = make_scorer(f1_score,  pos_label=1)
        self.forest_grid_search = GridSearchCV(RandomForestClassifier(random_state=0, n_jobs=4), forest_grid_param, scoring=f1_scoring, cv=4)

        self.trained_svm, self.trained_rf, self.scaler = self.__get_classifier_and_scaler()

    def __get_classifier_and_scaler(self):
        """
        Gets the classifier and scaler needed for the rest of the operations. Loads from cache if 
        load_saved is set to true.
        """
        if self.load_saved:
            print('Loading saved classifier and scaler...')
            svm_clf = pickle.load( open( "./cache/" + self.svm_clf_name, "rb")) if self.svm_enable else None
            rf_clf = pickle.load( open( "./cache/" + self.rf_clf_name, "rb")) if self.rf_enable else None
            scaler = pickle.load(open( "./cache/" + self.scaler_name, "rb"))
            
            np.set_printoptions(suppress=True)	
            np.set_printoptions(precision=6, floatmode='fixed')
        else:
            # Split up data into randomized training and test sets
            print('Training...')
            rand_state = np.random.randint(0, 100)
            
            notred_features, red_features, filenames = self.__get_features()
            scaled_X, y, scaler, mean, std = self.__get_scaled_X_y(notred_features, red_features)

            test_size = 0.05
            X_train, X_test, y_train, y_test = train_test_split(
                scaled_X, y, test_size=test_size, random_state=rand_state)
            # Check the training time for the SVC
            svm_clf = None
            if self.svm_enable:
                t=time.time()
                gscv = self.svm_grid_search
                gscv.fit(X_train, y_train)
                t2 = time.time()
                print(round(t2-t, 2), 'Seconds to train CLF...')
                # Extract best estimator
                svm_clf = gscv.best_estimator_
                print('Grid Search is finished, search result of  C =', svm_clf.C)
                self.__test_classifier(svm_clf, X_test, y_test, scaled_X, filenames, rand_state)
            
            rf_clf = None
            if self.rf_enable:
                t=time.time()
                gscv = self.forest_grid_search
                print(X_train, y_train)
                gscv.fit(X_train, y_train)
                # Extract best estimator
                rf_clf = gscv.best_estimator_
                print('Best parameters: {}'.format(gscv.best_params_))
                t2 = time.time()
                print(round(t2-t, 2), 'Seconds to train CLF...')
                self.__test_classifier(rf_clf, X_test, y_test, scaled_X, filenames, rand_state)

            
            
            print('Pickling classifier and scaler...')
            if self.svm_enable:
                pickle.dump(svm_clf, open( "./cache/" + self.svm_clf_name, "wb" )) 
            if self.rf_enable:
                pickle.dump(rf_clf, open( "./cache/" + self.rf_clf_name, "wb" )) 

            pickle.dump(scaler, open( "./cache/" + self.scaler_name, "wb" ) )
            pickle.dump(mean, open( "./cache/" + self.scaler_mean_name, "wb"))
            pickle.dump(std, open( "./cache/" + self.scaler_std_name, "wb"))

        return svm_clf, rf_clf, scaler
           
    def __test_classifier(self, clf, X_test, y_test, scaled_X, filenames, rand_state):
        # Check the score of the Classifier
        if isinstance(clf, LinearSVC):
            preds = 1/(1+(np.exp(-1*clf.decision_function(X_test))))
        elif isinstance(clf, RandomForestClassifier):
            preds = list(map(lambda x: x[1] / (x[0] + x[1]), clf.predict_proba(X_test)))
        else:
            pass
        #get filename
        test_filenames = shuffle(filenames, random_state=rand_state)[:len(preds)]
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
                print('\033[31mproba = {}, predict = {}, answer = {}, testcase = {}\033[0m'.format(round(ans_proba, 3), ans, int(y_test[i]), test_filenames[i]))
            else:
                print('proba = {}, predict = {}, answer = {}, testcase = {}'.format(round(ans_proba, 3), ans, int(y_test[i]), test_filenames[i]))

    def __get_features(self):
        """
        Gets features either by loading them from cache, or by extracting them from the data.
        """   
        if self.load_features:
            print('Loading saved features...')
            notred_features, red_features, filenames = pickle.load( open( "./cache/features.p", "rb" ) )
            
        else: 
            # print("Extracting features from %s samples..." % self.sample_size)          
            print("Extracting features...")          

            notreds = []
            reds = []
            filenames = []

            for folder in self.negative_data_folders:
                image_paths =glob.glob(folder+'/*')
                for path in image_paths:
                    notreds.append(path)
            for folder in self.positive_data_folders:
                image_paths =glob.glob(folder+'/*')
                for path in image_paths:
                    reds.append(path)

            #same input data num for RF
            #sample_size = min(len(notreds), len(reds))
            #notreds = notreds[0:sample_size]
            #reds =  reds[0:sample_size]

            filenames.extend(notreds)
            filenames.extend(reds)
            print("netative input data num : ", len(notreds))
            print("positive input data num : ", len(reds))
            start = time.clock()
            notred_features = self.__extract_features(notreds)
            red_features = self.__extract_features(reds)


            end = time.clock()
            print("Running time : %s seconds" % (end - start))
            
            print('Pickling features...')
            pickle.dump((notred_features, red_features, filenames), open( "./cache/features.p", "wb" ))
            
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

        # hls = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)# convert it to HLS
        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
   
        #3) Compute spatial features if flag is set
        if self.spatial_feat == True:
            # spatial_hls = self.__bin_spatial(hls)
            spatial_rgb = self.__bin_spatial(img)
            spatial_hsv = cv2.cvtColor(spatial_rgb, cv2.COLOR_BGR2HSV)
            img_features.append(spatial_hsv.ravel()/256)
            img_features.append(spatial_rgb.ravel()/256)

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

        return scaled_X, y, X_scaler, mean, std

    # Define a function to return HOG features and visualization
    def __get_hog_features(self, img, vis=False, feature_vec=True):
        return myhog.myhog(img)


    # Define a function to compute binned color features  
    def __bin_spatial(self, img):
        features = cv2.resize(img, self.spatial_size, cv2.INTER_LINEAR)
        return features

    # Define a function to extract features from a list of images
    # Have this function call bin_spatial() and color_hist()
    def predictoneimage(self, img):
        test_image = cv2.resize(img, (128, 64), cv2.INTER_LINEAR)
        features = self.__single_img_features(test_image)
        test_features = self.scaler.transform(np.array(features).reshape(1, -1))
        # prediction = self.trained_svm.predict_proba(test_features)[:,1]
        svm_prediction = 0
        rf_prediction = 0
        if self.svm_enable:
            bias = self.trained_svm.intercept_
            dot = np.dot(self.trained_svm.coef_[0], test_features[0]) 
            rst = dot + bias
            sigmoided_rst = 1/(1+np.exp(-1*rst))
            print(dot.shape)
            print("rst:", rst, "sigmoid:", sigmoided_rst)
            svm_prediction = 1/(1+(np.exp(-1*self.trained_svm.decision_function(test_features))))
        if self.rf_enable:
            rf_prediction = self.trained_rf.predict_proba(test_features)[:,1]
        return svm_prediction, rf_prediction
