############################################################
## This file is generated automatically by Vivado HLS.
## Please DO NOT edit it.
## Copyright (C) 1986-2018 Xilinx, Inc. All Rights Reserved.
############################################################
open_project hog_svm
set_top hog_svm
add_files hog_svm/blue.png
add_files hog_svm/consts.h
add_files hog_svm/frame.png
add_files hog_svm/main.cpp
add_files -tb hog_svm/main_tb.cpp
add_files -tb hog_svm/weight.h
open_solution "solution1"
set_part {xczu3eg-sbva484-1-e} -tool vivado
create_clock -period 9 -name default
#source "./hog_svm/solution1/directives.tcl"
csim_design
csynth_design
cosim_design -trace_level all -rtl vhdl
export_design -rtl verilog -format ip_catalog
