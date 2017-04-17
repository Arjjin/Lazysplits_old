#pragma once

#include <obs-module.h>
#include <util\threading.h>
#include <util\circlebuf.h>

namespace lazysplits {

class ls_frame_buf{
	public :
		ls_frame_buf();
		~ls_frame_buf();

		pthread_mutex_t* get_mutex();
		/* TODO : implement this */
		//void lock_mutex();
		//void unlock_mutex();
		bool is_frames();
		int get_num_frames();
		bool try_push_frame( obs_source_frame* frame );
		obs_source_frame* pop_frame();

	private :
		circlebuf buf;
		pthread_mutex_t frame_buf_mutex;
		int frame_count;
		int frame_limit;
};

}	//namespace lazysplits