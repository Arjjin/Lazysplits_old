#pragma once

#include "ls_frame_buf.h"

#include <util\threading.h>
#include <atomic>

namespace lazysplits{

class ls_thread_handler{
	public:
		ls_thread_handler();
		~ls_thread_handler();

		static void* ls_thread_frame_proc( void* data );

		void ls_thread_init( ls_frame_buf* frame_buf );
		void ls_thread_wake();
		void ls_thread_terminate();
			
		bool ls_thread_is_live();
		bool ls_thread_is_sleeping();

	private:
		pthread_t thread;
		pthread_mutex_t thread_mutex;
			
		std::atomic_bool thread_is_live;
		std::atomic_bool thread_is_sleeping;
		std::atomic_bool thread_should_wake;
		std::atomic_bool thread_should_terminate;

		pthread_cond_t COND_LS_THREAD_WAKE;
		pthread_cond_t COND_LS_THREAD_STOPPED;

		struct thread_data{
			pthread_mutex_t* thread_mutex;
			std::atomic_bool* thread_is_live;
			std::atomic_bool* thread_is_sleeping;
			std::atomic_bool* thread_should_wake;
			std::atomic_bool* thread_should_terminate;

			pthread_cond_t* COND_LS_THREAD_WAKE;
			pthread_cond_t* COND_LS_THREAD_STOPPED;

			ls_frame_buf* frame_buf;
		};
};

} //namespace lazysplits