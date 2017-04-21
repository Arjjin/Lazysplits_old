#include "ls_calibration.h"

#include "SM64_constants.h"


using namespace lazysplits;

ls_source_calibration::ls_source_calibration()
{
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
	blog( LOG_INFO, "[lazysplits] destroying ls_source_calibration" );
	pthread_mutex_destroy(&calib_mutex);

	obs_enter_graphics();
	gs_effect_destroy(calib_effect);
	gs_image_file_free(calib_img);
	obs_leave_graphics();

	bfree(calib_img);
}

void ls_source_calibration::set_source( obs_source_t* source){ calib_source = source; }

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

cv::Rect ls_source_calibration::calibrate_rect( const cv::Rect& rect ){
	float calib_img_width = calib_img->cx * scale_x;
	float calib_img_height = calib_img->cy * scale_y;
	float source_width, source_height, calib_img_x, calib_img_y;
	if( calib_source ){
		blog( LOG_WARNING, "[lazysplits] ls_source_calibration::transform_rect() : no calib_source!" );
		source_width = obs_source_get_width(calib_source);
		source_height = obs_source_get_height(calib_source);
	}
	else{
		source_width = calib_img_width;
		source_height = calib_img_height;
	}
	//take into account calibration uses scale centering
	calib_img_x = (source_width - calib_img_width) * 0.5F;
	calib_img_y = (source_height - calib_img_height) * 0.5F;
	//add calibration offsets
	calib_img_x += source_width * offset_x;
	calib_img_y += source_height * offset_y;

	cv::Rect out_rect(
		( rect.x * scale_x ) + calib_img_x,
		( rect.y * scale_y ) + calib_img_y,
		rect.width*scale_x,
		rect.height*scale_y
	);
	
	//handle out of bounds/wacky offset and scale
	out_rect.x = ( out_rect.x < 0.0F ) ? 0.0F : ( out_rect.x > source_width ) ? source_width : out_rect.x;
	out_rect.y = ( out_rect.y < 0.0F ) ? 0.0F : ( out_rect.y > source_width ) ? source_width : out_rect.y;
	if( out_rect.x + out_rect.width > source_width ){ out_rect.width -= ( out_rect.width - source_width - 1.0F ); }
	if( out_rect.y + out_rect.height > source_height ){ out_rect.height -= ( out_rect.height - source_height - 1.0F ); }

	return out_rect;
}