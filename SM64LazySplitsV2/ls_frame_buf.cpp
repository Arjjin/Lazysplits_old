#include <obs-module.h>

#include "ls_frame_buf.h"

using namespace lazysplits;

ls_frame_buf::ls_frame_buf(){
	frame_limit = 30;
	frame_count = 0;
	pthread_mutex_init( &frame_buf_mutex, NULL );
	circlebuf_init(&buf);
}

ls_frame_buf::~ls_frame_buf(){
	blog( LOG_INFO, "[lazysplits] destroying ls_frame_buf" );
	pthread_mutex_destroy( &frame_buf_mutex );
	circlebuf_free(&buf);
}

pthread_mutex_t* ls_frame_buf::get_mutex(){
	return &frame_buf_mutex;
}

bool ls_frame_buf::is_frames(){
	return ( frame_count > 0 ) ? true : false;
}

int ls_frame_buf::get_num_frames(){
	return frame_count;
}

bool ls_frame_buf::try_push_frame( obs_source_frame* frame ){
	if( frame_count < frame_limit ){
		blog( LOG_INFO, "[lazysplits] pushing elem %i", frame_count );
		circlebuf_push_back( &buf, &frame, sizeof(struct obs_source_frame*) );
		frame_count++;
		return true;
	}
	else{
		blog( LOG_INFO, "[lazysplits] frame buffer full" );
		return false;
	}
}

obs_source_frame* ls_frame_buf::pop_frame(){
	obs_source_frame* frame;
	if( is_frames() ){
		circlebuf_pop_front( &buf, &frame, sizeof(struct obs_source_frame*) );

		blog( LOG_INFO, "[lazysplits] popping elem %i, frame is %s", frame_count, (frame) ? "good" : "bad" );
		frame_count--;
	}
	else{ blog( LOG_WARNING, "pop_frame() called when frame buffer empty!" ); }
	return frame;
}