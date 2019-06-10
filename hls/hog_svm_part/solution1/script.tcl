############################################################
## This file is generated automatically by Vivado HLS.
## Please DO NOT edit it.
## Copyright (C) 1986-2018 Xilinx, Inc. All Rights Reserved.
############################################################
open_project hog_svm_part
set_top hog_svm_part
add_files hog_svm_part/blue.png
add_files hog_svm_part/frame.png
add_files hog_svm_part/main.cpp
add_files -tb hog_svm_part/main_tb.cpp
open_solution "solution1"
set_part {xczu3eg-sbva484-1-e} -tool vivado
create_clock -period 9 -name default
#source "./hog_svm_part/solution1/directives.tcl"
csim_design
csynth_design
cosim_design -trace_level all -rtl vhdl
export_design -rtl verilog -format ip_catalog
