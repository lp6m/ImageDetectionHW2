############################################################
## This file is generated automatically by Vivado HLS.
## Please DO NOT edit it.
## Copyright (C) 1986-2017 Xilinx, Inc. All Rights Reserved.
############################################################
open_project ap_fixed_convert
set_top main
add_files ap_fixed_convert/main.cpp
add_files -tb ap_fixed_convert/main_tb.cpp
add_files -tb ap_fixed_convert/weights.h
open_solution "solution1"
set_part {xqku115-rlf1924-2-i} -tool vivado
create_clock -period 10 -name default
#source "./ap_fixed_convert/solution1/directives.tcl"
csim_design -compiler gcc
csynth_design
cosim_design
export_design -format ip_catalog
