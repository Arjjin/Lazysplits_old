#pragma once

#include <obs-module.h>
#include <graphics\effect.h>
extern "C" {
#include <graphics\image-file.h>
}
#include <util\threading.h>

#include <opencv2\core.hpp>

#include <string>

namespace lazysplits {

class ls_source_calibration{
	public:
		ls_source_calibration();
		~ls_source_calibration();

		void lock_mutex();
		void unlock_mutex();

		void set_source( obs_source_t* source);

		bool try_set_image( const char* path );
		bool image_loaded();
		bool tex_loaded();
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

		cv::Rect calibrate_rect( const cv::Rect& rect );

	private:
		obs_source_t* calib_source;

		pthread_mutex_t calib_mutex;

		float opacity;
		float offset_x;
		float offset_y;
		float scale_x;
		float scale_y;
	
		std::string calib_img_path;
		gs_image_file_t* calib_img;
		gs_texture_t* calib_tex;
		gs_effect_t* calib_effect;

};

}//namespace lazysplits