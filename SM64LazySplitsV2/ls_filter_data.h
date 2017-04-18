#pragma once

#include <obs-module.h>

#include "ls_frame_buf.h"
#include "ls_thread.h"
#include "ls_calibration.h"

namespace lazysplits{

struct ls_filter_data {
	ls_filter_data();
	~ls_filter_data();

	obs_source_t* context;

	int polling_ms;

	int cv_frame_skip;
	long cv_frame_count;
	
	//circular frame buffer
	ls_frame_buf* frame_buf;

	//thread
	ls_thread_handler* thread;

	//calibration stuff
	ls_source_calibration* calib;
	bool is_calib;
};

} //namespace lazysplits