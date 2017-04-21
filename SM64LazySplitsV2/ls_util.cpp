#include "ls_util.h"

#include <util\threading.h>

#include <opencv2\highgui\highgui.hpp>
#include <opencv2\imgproc\imgproc.hpp>

using namespace lazysplits;

cv::Mat lazysplits::OBS_2_CV_BGR( obs_source_frame* frame ){
	int frame_width = frame->width;
	int frame_height = frame->height;
	cv::Mat BGR;

	if( frame->format == VIDEO_FORMAT_I420 ){
		cv::Mat yuv_raw( frame_height*1.5F, frame_width, CV_8UC1, frame->data[0] );
		cv::cvtColor( yuv_raw, BGR, CV_YUV2BGR_I420 );
	}
	else if( frame->format == VIDEO_FORMAT_NV12 ){
		cv::Mat yuv_raw( frame_height*1.5F, frame_width, CV_8UC1, frame->data[0] );
		cv::cvtColor( yuv_raw, BGR, CV_YUV2BGR_NV12 );
	}
	else if( frame->format == VIDEO_FORMAT_YVYU ){
		cv::Mat yuv_raw( frame_height, frame_width, CV_8UC2, frame->data[0] );
		cv::cvtColor( yuv_raw, BGR, CV_YUV2BGR_YVYU );
	}
	else if( frame->format == VIDEO_FORMAT_YUY2 ){
		cv::Mat yuv_raw( frame_height, frame_width, CV_8UC2, frame->data[0] );
		cv::cvtColor( yuv_raw, BGR, CV_YUV2BGR_YUY2 );
	}
	else if( frame->format == VIDEO_FORMAT_UYVY ){
		cv::Mat yuv_raw( frame_height, frame_width, CV_8UC2, frame->data[0] );
		cv::cvtColor( yuv_raw, BGR, CV_YUV2BGR_UYVY );
	}
	else if( frame->format == VIDEO_FORMAT_RGBA ){
		BGR = cv::Scalar( 0 );
	}
	else if( frame->format == VIDEO_FORMAT_BGRA ){
		BGR = cv::Scalar( 0 );
	}
	else if( frame->format == VIDEO_FORMAT_BGRX ){
		BGR = cv::Scalar( 0 );
	}
	else if( frame->format == VIDEO_FORMAT_Y800 ){
		BGR = cv::Scalar( 0 );
	}
	else if( frame->format == VIDEO_FORMAT_NONE ){
		BGR = cv::Scalar( 255 );
	}
	else{
		BGR = cv::Scalar( 0 );
	}

	return BGR;
}


cv::Mat lazysplits::OBS_2_CV_BGR( obs_source_frame* frame, cv::Rect& bounds ){
	cv::Mat BGR = OBS_2_CV_BGR(frame);
	BGR = BGR(bounds);

	return BGR;
}