#pragma once

#include <obs-module.h>
#include <opencv2\core.hpp>

namespace lazysplits{
	cv::Mat OBS_2_CV_BGR( obs_source_frame* frame );
	cv::Mat OBS_2_CV_BGR( obs_source_frame* frame, cv::Rect bounds );
} //namespace lazysplits