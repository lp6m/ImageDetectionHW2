############################################################
## This file is generated automatically by Vivado HLS.
## Please DO NOT edit it.
## Copyright (C) 1986-2018 Xilinx, Inc. All Rights Reserved.
############################################################
open_project hog_svm
set_top hog_svm
add_files hog_svm/src/blue.png
add_files hog_svm/src/frame.png
add_files hog_svm/src/main.cpp
add_files -tb hog_svm/src/consts.h -cflags "-Wno-unknown-pragmas"
add_files -tb hog_svm/src/hog_host.cpp -cflags "-Wno-unknown-pragmas"
add_files -tb hog_svm/src/hog_host.h -cflags "-Wno-unknown-pragmas"
add_files -tb hog_svm/src/main_tb.cpp -cflags "-Wno-unknown-pragmas"
open_solution "solution1"
set_part {xczu3eg-sbva484-1-e}
create_clock -period 9.5 -name default
#source "./hog_svm/solution1/directives.tcl"
csim_design -compiler gcc
csynth_design
cosim_design
export_design -rtl vhdl -format ip_catalog
