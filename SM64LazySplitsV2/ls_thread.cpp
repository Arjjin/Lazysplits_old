#include "ls_thread.h"

#include "ls_util.h"
#include <opencv2\highgui\highgui.hpp>

#include <sstream>

#include "SM64_constants.h"

using namespace lazysplits;

ls_thread_handler::ls_thread_handler(){
	pthread_mutex_init( &thread_mutex, NULL );
	pthread_cond_init( &COND_LS_THREAD_WAKE, NULL );
	pthread_cond_init( &COND_LS_THREAD_STOPPED, NULL );
	thread_is_live = false;
	thread_is_sleeping = false;
	thread_should_wake = false;
	thread_should_terminate = false;
}

ls_thread_handler::~ls_thread_handler(){
	blog( LOG_INFO, "[lazysplits] destroying ls_thread_handler" );
	if( ls_thread_is_live() ){ ls_thread_terminate(); }

	pthread_mutex_destroy(&thread_mutex);
	pthread_cond_destroy( &COND_LS_THREAD_WAKE );
	pthread_cond_destroy(&COND_LS_THREAD_STOPPED);
}

void* ls_thread_handler::ls_thread_frame_proc( void* data ){
	thread_data* t_data = static_cast<thread_data*>(data);

	pthread_mutex_lock(t_data->thread_mutex);
	*t_data->thread_is_live = true;
	pthread_mutex_unlock(t_data->thread_mutex);
	
	int im_num = 0;
	//cv compression params
	std::vector<int> compression_params;
	compression_params.push_back(CV_IMWRITE_PNG_COMPRESSION);
	compression_params.push_back(1);

	cv::Rect star_crop;
	//main thread loop
	while(true){
		//check if thread is signalled to terminate
		pthread_mutex_lock(t_data->thread_mutex);
		if( *t_data->thread_should_terminate ){
			pthread_mutex_unlock(t_data->thread_mutex);
			break;
		}
		pthread_mutex_unlock(t_data->thread_mutex);

		//frame buffer loop
		while(true){
			if( star_crop.size().area() <= 1 ){ star_crop = t_data->calib->calibrate_rect( cv::Rect( SM64_STAR_COUNT_1x1_X, SM64_STAR_COUNTER_Y, SM64_STAR_COUNT_1x1_WIDTH, SM64_STAR_COUNTER_HEIGHT ) ); }
			//check if thread is signalled to terminate
			pthread_mutex_lock(t_data->thread_mutex);
			if( *t_data->thread_should_terminate ){
				pthread_mutex_unlock(t_data->thread_mutex);
				break;
			}
			pthread_mutex_unlock(t_data->thread_mutex);

			//check if frames are available
			obs_source_frame* frame;
			pthread_mutex_lock( t_data->frame_buf->get_mutex() );
			if(  t_data->frame_buf->is_frames() ){
				frame = t_data->frame_buf->pop_frame();
			}
			else{
				pthread_mutex_unlock( t_data->frame_buf->get_mutex() );
				break;
			}
			pthread_mutex_unlock( t_data->frame_buf->get_mutex() );

			if( frame ){
				cv::Mat BGR_frame = OBS_2_CV_BGR( frame, star_crop );
				if(BGR_frame.data){
					std::stringstream ss;
					ss << "img/cap" << im_num << ".png";
					cv::imwrite( ss.str(), BGR_frame, compression_params );
					im_num++;
				}
			}
		}

		//no more frames, sleep until signalled to wake
		pthread_mutex_lock(t_data->thread_mutex);
		if( *t_data->thread_should_terminate ){
			pthread_mutex_unlock(t_data->thread_mutex);
			break;
		}
		*t_data->thread_is_sleeping = true;
		blog( LOG_INFO, "[lazysplits] thread sleeping" );
		while( !*t_data->thread_should_wake ){
			pthread_cond_wait( t_data->COND_LS_THREAD_WAKE, t_data->thread_mutex );
		}
		*t_data->thread_is_sleeping = false;
		*t_data->thread_should_wake = false;
		blog( LOG_INFO, "[lazysplits] thread woke" );
		pthread_mutex_unlock(t_data->thread_mutex);

	}
	
	pthread_cond_signal(t_data->COND_LS_THREAD_STOPPED);

	pthread_mutex_lock(t_data->thread_mutex);
	*t_data->thread_is_live = false;
	pthread_mutex_unlock(t_data->thread_mutex);
	
	blog( LOG_INFO, "[lazysplits] exiting thread" );
	return NULL;
}

void ls_thread_handler::ls_thread_init( ls_frame_buf* frame_buf, ls_source_calibration* calib ){

	//thread data structure
	thread_data* t_data = new thread_data;
	t_data->thread_mutex = &thread_mutex;
	t_data->thread_is_live = &thread_is_live;
	t_data->thread_is_sleeping = &thread_is_sleeping;
	t_data->thread_should_wake = &thread_should_wake;
	t_data->thread_should_terminate = &thread_should_terminate;
	t_data->COND_LS_THREAD_WAKE = &COND_LS_THREAD_WAKE;
	t_data->COND_LS_THREAD_STOPPED = &COND_LS_THREAD_STOPPED;
	t_data->frame_buf = frame_buf;
	t_data->calib = calib;

	int t = pthread_create( &thread, NULL, ls_thread_frame_proc, static_cast<void*>(t_data) );
	if( t != 0 ){ blog( LOG_ERROR, "[lazysplits] pthread error : %i", t ); }
	else{ blog( LOG_INFO, "[lazysplits] thread created"); }
}

void ls_thread_handler::ls_thread_wake(){
	pthread_mutex_lock(&thread_mutex);
	blog( LOG_INFO, "[lazysplits] waking thread" );
	thread_should_wake = true;
	pthread_cond_signal(&COND_LS_THREAD_WAKE);
	pthread_mutex_unlock(&thread_mutex);
}

void ls_thread_handler::ls_thread_terminate(){
	//if thread is sleeping, wake it first
	if( ls_thread_is_sleeping() ){ 
		blog( LOG_INFO, "[lazysplits] cv_thread_terminate(), trying to wake thread" );
		ls_thread_wake();
	}

	pthread_mutex_lock(&thread_mutex);
	blog( LOG_INFO, "[lazysplits] terminating thread" );
	thread_should_terminate = true;
	//wait for thread to signal back that it's closing
	while( thread_is_live ){
		pthread_cond_wait( &COND_LS_THREAD_STOPPED, &thread_mutex );
	}
	pthread_mutex_unlock(&thread_mutex);
}

bool ls_thread_handler::ls_thread_is_live(){
	bool b_out;

	pthread_mutex_lock(&thread_mutex);
	b_out = thread_is_live;
	pthread_mutex_unlock(&thread_mutex);
	
	return b_out;
}

bool ls_thread_handler::ls_thread_is_sleeping(){
	bool b_out;

	pthread_mutex_lock(&thread_mutex);
	b_out = thread_is_sleeping;
	pthread_mutex_unlock(&thread_mutex);

	return b_out;
}