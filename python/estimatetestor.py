# -*- coding: utf-8 -*-
from tkinter import *
from PIL import Image
import os
import glob
import cv2
import numpy as np
import vdtools

parentdir = '../video'
inputdir = 'itweek_train'
# outputdir = 'output'
imgname_prefix = 'image_'
# csvlogfilename = 'crop.csv'


_canvas_width = 640
_canvas_height = 640
_crop_width = 64
_crop_height = 32

#----------------------------------------------------------------------
class MainWindow():

    #----------------
    def __init__(self, main):
        # warning : image must be smaller than canvas size.
        # create image canvas 
        self.canvas = Canvas(main, width=_canvas_width, height=_canvas_height)
        self.canvas.grid(row=0, column=0, columnspan=2, rowspan=4)

        # load image list
        # image file name must be 'prefix + number(0000-9999) .png'
        self.file_list = []
        for i in range(10000):
            filepath = './' + parentdir + '/' + inputdir + '/' + imgname_prefix + ('%04d' % i) + '.png'
            # outputpath = './' + parentdir + '/' + outputdir + '/' + imgname_prefix + ('%04d' % i) + '.png'
            if os.path.exists(filepath):
                self.file_list.append((i, filepath))

        # load cropping frame information if csv file exists
        # self.crop_frame_dic = {}
        # self.csvfilepath = './' + parentdir + '/' + csvlogfilename
        # if os.path.exists(self.csvfilepath):
        #     with open(self.csvfilepath) as f:
        #         for line in f:
        #             elements = line.split(',')
        #             #filename, sx, sy, ex, ey
        #             self.crop_frame_dic[elements[0]] = (int(elements[1]), int(elements[2]), int(elements[3]), int(elements[4]))

        # load first image
        self.now_image_index = 0
        self.nowimage = PhotoImage(file=self.file_list[0][1])
        self.image_on_canvas = self.canvas.create_image(0, 0, anchor=NW, image=self.nowimage)
        self.cvimage = cv2.imread(self.file_list[0][1])

        # set callback function for key and mouse events
        self.canvas.bind('<Right>', self.right_key_pressed)
        self.canvas.bind('<Left>', self.left_key_pressed)
        self.canvas.bind('<space>', self.space_key_pressed)
        self.canvas.bind("<B1-Motion>", self.mouse_lclick_moving)
        self.canvas.bind("<Button-1>", self.mouse_lbuton_pressed)
        self.canvas.bind("<ButtonRelease-1>", self.mouse_lbutton_released)
        #  mouse event with Windows OS
        root.bind("<MouseWheel>", self.mouse_wheel_moving)
        #  mouse event with Linux OS
        root.bind("<Button-4>", self.mouse_wheel_moving)
        root.bind("<Button-5>", self.mouse_wheel_moving)

        self.canvas.focus_set() #need to recive key events

        # set initial value of crop frame
        self.cropframe_width = 128
        self.cropframe_height = self.cropframe_width * _crop_height / _crop_width
        self.cropframe_centerposx = _canvas_width/2
        self.cropframe_centerposy = _canvas_height/2

        # draw crop frame 
        sx, sy, ex, ey = self.__getCropFrameCoordinate()
        self.crop_rectangle = self.canvas.create_rectangle(sx, sy, ex, ey, outline="blue", width=3)

        # filename label
        self.filenamelabel = Label(self.canvas, bg="black",fg="green",text=self.file_list[0][1],borderwidth=0,font=("",20))
        self.filenamelabel.place(anchor=NW)

        #result label
        self.resultlabel = Label(self.canvas, bg="black",fg="red",text="",borderwidth=0,font=("",20))
        self.resultlabel.place(relx=0.0, rely=1.0,anchor=SW)

        self.__imageRefresh()

        #new vdtool instance
        self.finder = vdtools.WindowFinder()
    #----------------
    def right_key_pressed(self, event):
        # open next image
        self.now_image_index += 1
        if self.now_image_index >= len(self.file_list): 
            self.now_image_index = 0

        self.__imageRefresh()

    def left_key_pressed(self, event):
        # open previous image
        self.now_image_index -= 1
        if self.now_image_index < 0: 
            self.now_image_index = len(self.file_list) - 1

        self.__imageRefresh()

    def space_key_pressed(self, event):
        pass

    def mouse_lclick_moving(self, event):
        self.cropframe_centerposx = event.x
        self.cropframe_centerposy = event.y
        self.__cropRectangleRefresh()

    def mouse_lbuton_pressed(self, event):
        pass
    def mouse_lbutton_released(self, event):
        sx, sy, ex, ey = self.__getCropFrameCoordinate()
        proba = self.__doEstimate(sx, sy, ex, ey)
        self.resultlabel["text"] = str(proba)

            
    def mouse_wheel_moving(self, event):
        delta = 2
        delta_width = delta
        delta_height = delta * _crop_height / _crop_width 
        # respond to Linux or Windows wheel event
        if event.num == 5 or event.delta == -120:
            # mouse wheel down
            self.cropframe_width = max(0, self.cropframe_width - delta_width)
            self.cropframe_height = max(0, self.cropframe_height - delta_height)
        if event.num == 4 or event.delta == 120:
            # mouse wheel up
            self.cropframe_width += delta_width
            self.cropframe_height += delta_height
        self.__cropRectangleRefresh()

    #----------------
    def __imageRefresh(self):
        #change image
        self.nowimage = PhotoImage(file=self.file_list[self.now_image_index][1])
        self.canvas.itemconfig(self.image_on_canvas,image=self.nowimage)

        #check whether cropped image coordinate information already exists
        #ignore wheter cropped image file exists
        inputfilepath = self.file_list[self.now_image_index][1]
        imagefilename = os.path.basename(inputfilepath)
        
        #update filename label
        self.filenamelabel["text"] = inputfilepath
        self.resultlabel["text"] = "";
        self.cvimage = cv2.imread(inputfilepath)
    def __cropRectangleRefresh(self):
        sx, sy, ex, ey = self.__getCropFrameCoordinate()
        self.canvas.coords(self.crop_rectangle, sx, sy, ex, ey)

    def __saveCropImage(self):
        pass
        # im = Image.open(self.file_list[self.now_image_index][1])
        # sx, sy, ex, ey = self.__getCropFrameCoordinate()
        # im_crop = im.crop((sx, sy, ex, ey))
        # im_crop_resized  = im_crop.resize((_crop_width, _crop_height))
        # im_crop_resized.save(self.file_list[self.now_image_index][2], quality=95)

    def __addCropDic(self):
        pass
        # originalfilename = os.path.basename(self.file_list[self.now_image_index][1])
        # sx, sy, ex, ey = self.__getCropFrameCoordinate()
        # self.crop_frame_dic[originalfilename] = (sx, sy, ex, ey)

    def __saveCropInfoToCSV(self):
        pass
        # with open(self.csvfilepath, mode='w') as f:
        #     for filename, coordinate in self.crop_frame_dic.items():
        #         f.write(','.join([filename, str(coordinate[0]), str(coordinate[1]), str(coordinate[2]), str(coordinate[3])]) + "\n")
        # print("csv saved")

    def __getCropFrameCoordinate(self):
        sx = self.cropframe_centerposx - self.cropframe_width/2
        sy = self.cropframe_centerposy - self.cropframe_height/2
        ex = self.cropframe_centerposx + self.cropframe_width/2
        ey = self.cropframe_centerposy + self.cropframe_height/2
        return sx, sy, ex, ey

    def __doEstimate(self, sx, sy, ex, ey):
        test_crop = self.cvimage[int(sy):int(ey), int(sx):int(ex)]
        proba = self.finder.predictoneimage(test_crop)
        return proba


#----------------------------------------------------------------------

root = Tk()
MainWindow(root)
root.mainloop()