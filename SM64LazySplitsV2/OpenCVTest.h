#pragma once

#include <obs.hpp>

#include <opencv2\core\core.hpp>


class OpenCVTestClass {
	public :
		OpenCVTestClass();
		cv::Mat OBS_2_CV_BGR( obs_source_frame* frame )const;
		cv::Mat getTarget( cv::Mat &img, cv::Rect &bounds )const;
};

struct CVTestThreadInfo{
	int frame_num;
	struct obs_source_frame* frame;
};