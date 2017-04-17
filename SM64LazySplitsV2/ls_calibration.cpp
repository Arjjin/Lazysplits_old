#include "ls_calibration.h"

#include <obs-module.h>


using namespace lazysplits;

ls_source_calibration::ls_source_calibration(){
	pthread_mutex_init( &calib_mutex, NULL );
	calib_img = static_cast<gs_image_file_t*>( bzalloc( sizeof(gs_image_file_t) ) ); 
	calib_effect = static_cast<gs_effect_t*>( bzalloc( sizeof(gs_effect_t) ) );

	calib_img_path = "null";

	opacity = 1.0F;
	offset_x = 0.0F;
	offset_y = 0.0F;
	scale_x = 1.0F;
	scale_y = 1.0F;
}

ls_source_calibration::~ls_source_calibration(){
	pthread_mutex_destroy(&calib_mutex);

	obs_enter_graphics();
	gs_effect_destroy(calib_effect);
	gs_image_file_free(calib_img);
	obs_leave_graphics();

	bfree(calib_img);
}

void ls_source_calibration::lock_mutex(){ pthread_mutex_lock(&calib_mutex); }

void ls_source_calibration::unlock_mutex(){ pthread_mutex_unlock(&calib_mutex); }

bool ls_source_calibration::try_set_image( const char* path ){
	//check if path is empty/duplicate
	if( *path && strcmp( path, calib_img_path.c_str() ) != 0 ){
		obs_enter_graphics();
		gs_image_file_free(calib_img);
		obs_leave_graphics();
		
		gs_image_file_init( calib_img, path );
		
		obs_enter_graphics();
		gs_image_file_init_texture(calib_img);
		calib_tex = calib_img->texture;
		obs_leave_graphics();

		calib_img_path = path;

		blog( LOG_INFO, "[lazysplits] set_image() : %s", path );
		return true;
	}
	return false;
}

bool ls_source_calibration::image_loaded(){ return (calib_img) ? calib_img->loaded : false; }

bool ls_source_calibration::tex_loaded(){ return calib_tex != NULL; }

gs_texture_t* ls_source_calibration::get_tex(){ return calib_tex; }

void ls_source_calibration::set_effect( const char* path ){
	obs_enter_graphics();
	calib_effect = gs_effect_create_from_file( path, NULL );
	obs_leave_graphics();
}

bool ls_source_calibration::effect_loaded(){ return calib_effect != NULL; }

gs_effect_t* ls_source_calibration::get_effect(){ return calib_effect; }

void ls_source_calibration::set_opacity( float val ){ opacity = val; }
void ls_source_calibration::set_offset_x( float val ){ offset_x = val; }
void ls_source_calibration::set_offset_y( float val ){ offset_y = val; }
void ls_source_calibration::set_scale_x( float val ){ scale_x = val; }
void ls_source_calibration::set_scale_y( float val ){ scale_y = val; }
		
float ls_source_calibration::get_opacity(){ return opacity; }
float ls_source_calibration::get_offset_x(){ return offset_x; }
float ls_source_calibration::get_offset_y(){ return offset_y; }
float ls_source_calibration::get_scale_x(){ return scale_x; }
float ls_source_calibration::get_scale_y(){ return scale_y; }