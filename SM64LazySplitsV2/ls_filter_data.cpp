#include "ls_filter_data.h"

using namespace lazysplits;

ls_filter_data::ls_filter_data(){
	blog( LOG_INFO, "[lazysplits] creating ls_filter_data" );
	frame_buf = new ls_frame_buf;
	thread = new ls_thread_handler;
	calib = new ls_source_calibration;
}

ls_filter_data::~ls_filter_data(){
	blog( LOG_INFO, "[lazysplits] destroying ls_filter_data" );
	delete thread;
	delete frame_buf;
	delete calib;
}