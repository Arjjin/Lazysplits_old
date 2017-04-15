#pragma once

#include <graphics\effect.h>
extern "C" {
#include <graphics\image-file.h>
}
#include <util\threading.h>

namespace lazysplits {

class cv_source_calibration{
	public:
		cv_source_calibration();
		~cv_source_calibration();

		void lock_mutex();
		void unlock_mutex();

		void set_image( const char* path );
		bool image_loaded();
		gs_texture_t* get_tex();

		void set_effect( const char* path );
		bool effect_loaded();
		gs_effect_t* get_effect();
		
		void set_opacity( float val );
		void set_offset_x( float val );
		void set_offset_y( float val );
		void set_scale_x( float val );
		void set_scale_y( float val );
		
		float get_opacity();
		float get_offset_x();
		float get_offset_y();
		float get_scale_x();
		float get_scale_y();

	private:
		pthread_mutex_t calib_mutex;

		float opacity;
		float offset_x;
		float offset_y;
		float scale_x;
		float scale_y;
	
		gs_image_file_t* calib_img;
		gs_effect_t* calib_effect;
		gs_texture_t* calib_tex;

};

}//namespace lazysplits