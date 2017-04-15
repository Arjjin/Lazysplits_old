#pragma once

#include "cv_frame_buf.h"

#include <util\threading.h>
#include <atomic>

namespace lazysplits{

class cv_thread_handler{
	public:
		cv_thread_handler();
		~cv_thread_handler();

		static void* cv_thread_frame_proc( void* data );

		void cv_thread_init( cv_frame_buf* frame_buf );
		void cv_thread_wake();
		void cv_thread_terminate();
			
		bool cv_thread_is_live();
		bool cv_thread_is_sleeping();

	private:
		pthread_t thread;
		pthread_mutex_t thread_mutex;
			
		std::atomic_bool thread_is_live;
		std::atomic_bool thread_is_sleeping;
		std::atomic_bool thread_should_wake;
		std::atomic_bool thread_should_terminate;

		pthread_cond_t COND_CV_THREAD_WAKE;
		pthread_cond_t COND_CV_THREAD_STOPPED;

		struct thread_data{
			pthread_mutex_t* thread_mutex;
			std::atomic_bool* thread_is_live;
			std::atomic_bool* thread_is_sleeping;
			std::atomic_bool* thread_should_wake;
			std::atomic_bool* thread_should_terminate;

			pthread_cond_t* COND_CV_THREAD_WAKE;
			pthread_cond_t* COND_CV_THREAD_STOPPED;

			cv_frame_buf* frame_buf;
		};
};

} //namespace lazysplits