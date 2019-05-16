############################################################
## This file is generated automatically by Vivado HLS.
## Please DO NOT edit it.
## Copyright (C) 1986-2017 Xilinx, Inc. All Rights Reserved.
############################################################
open_project hog_svm
set_top hog_svm
add_files hog_svm/src/frame.png
add_files hog_svm/src/main.cpp
add_files -tb hog_svm/src/consts.h
add_files -tb hog_svm/src/hog_host.cpp
add_files -tb hog_svm/src/hog_host.h
add_files -tb hog_svm/src/main_tb.cpp
open_solution "solution1"
set_part {xczu3eg-sfva625-1-e}
create_clock -period 10 -name default
#source "./hog_svm/solution1/directives.tcl"
csim_design -compiler gcc
csynth_design
cosim_design
export_design -format ip_catalog
