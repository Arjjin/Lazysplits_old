#include <obs-module.h>
#include <graphics\effect.h>
extern "C" {
#include <graphics\image-file.h>
}
#include <graphics\vec2.h>
#include <util\dstr.h>
#include <util\threading.h>

#include <opencv2\highgui\highgui.hpp>
#include <opencv2\imgproc\imgproc.hpp>

#include "SM64_constants.h"
#include "cv_frame_buf.h"
#include "cv_thread.h"
#include "lazysplits_util.h"

#include <atomic>

/* TODO : limit to one filter instance in OBS*/

#define SETTING_POLLING_MS "polling_ms"
#define SETTING_CV_FRAME_SKIP "cv_frame_skip"
#define SETTING_IS_OFFSET_CALIBRATION "is_offset_calibration"
#define SETTING_CALIB_IMAGE_PATH "calib_image_path"
#define SETTING_CALIB_IMAGE_OPACITY "calib_image_opcaity"
#define SETTING_OFFSET_X "offset_x"
#define SETTING_OFFSET_Y "offset_y"
//#define SETTING_STAR_COUNT_CONSTRAIN_SCALE "starcount_constrain_scale"
#define SETTING_SCALE_X "scale_x"
#define SETTING_SCALE_Y "scale_y"

#define TEXT_POLLING_MS "Polling time (ms)"
#define TEXT_CV_FRAME_SKIP "Analyze every (n) frames"
#define TEXT_IS_OFFSET_CALIBRATION "Calibrate offset"
#define TEXT_CALIB_IMAGE_PATH "Calibration image"
#define TEXT_CALIB_IMAGE_OPACITY "Calibration image opacity"
#define TEXT_OFFSET_X "Frame x-offset"
#define TEXT_OFFSET_Y "Frame y-offset"
//#define TEXT_STAR_COUNT_CONSTRAIN_SCALE "Constrain scaling to aspect ratio"
#define TEXT_SCALE_X "Frame width scaling"
#define TEXT_SCALE_Y "Frame height scaling"

struct lazy_split_filter_data {
	obs_source_t* context;

	int polling_ms;

	int cv_frame_skip;
	long cv_frame_count;
	
	//circular frame buffer
	lazysplits::cv_frame_buf* frame_buf;

	//cv thread
	lazysplits::cv_thread_handler* thread_h;

	//cv calibration stuff
	bool is_calib;

	const char* calib_img_path;
	float calib_img_opacity;
	float calib_img_x;
	float calib_img_y;
	float calib_img_scale_x;
	float calib_img_scale_y;
	
	gs_image_file_t *calib_img;
	gs_effect_t *calib_effect;
	gs_texture_t *calib_target;
};

static const char* lazy_splits_filter_name(void* type_data) {
    return "SM64 Lazy Splits";
}

static void lazy_splits_filter_update(void *data, obs_data_t *settings)
{
	struct lazy_split_filter_data *filter = static_cast<lazy_split_filter_data*>(data);
	//LS_LOG( LOG_INFO, "image path :  %s", calib_img_path );
	filter->polling_ms = static_cast<uint64_t>( obs_data_get_int( settings, SETTING_POLLING_MS ) );
	filter->cv_frame_skip = static_cast<uint64_t>( obs_data_get_int( settings, SETTING_CV_FRAME_SKIP ) );
	
	filter->is_calib = obs_data_get_bool( settings, SETTING_IS_OFFSET_CALIBRATION );
	filter->calib_img_path = obs_data_get_string( settings, SETTING_CALIB_IMAGE_PATH );

	//if calibration image path exists, free and initialize an image from the path, and set our calib_image texture
	if( *filter->calib_img_path ){
		obs_enter_graphics();
		gs_image_file_free(filter->calib_img);
		obs_leave_graphics();
		
		gs_image_file_init( filter->calib_img, filter->calib_img_path );
		
		obs_enter_graphics();
		gs_image_file_init_texture(filter->calib_img);
		filter->calib_target = filter->calib_img->texture;
		obs_leave_graphics();
	}

	filter->calib_img_opacity = obs_data_get_double( settings, SETTING_CALIB_IMAGE_OPACITY );
	filter->calib_img_x = obs_data_get_double( settings, SETTING_OFFSET_X );
	filter->calib_img_y = obs_data_get_double( settings, SETTING_OFFSET_Y );
	filter->calib_img_scale_x = obs_data_get_double( settings, SETTING_SCALE_X );
	filter->calib_img_scale_y = obs_data_get_double( settings, SETTING_SCALE_Y );

}

static void* lazy_splits_filter_create( obs_data_t* settings, obs_source_t* context )
{
	struct lazy_split_filter_data *filter = new lazy_split_filter_data;
	filter->calib_img = static_cast<gs_image_file_t*>(
		bzalloc(sizeof(gs_image_file_t))
	); 
	filter->calib_effect = static_cast<gs_effect_t*>(
		bzalloc(sizeof(gs_effect_t))
	);
	
	/*
	struct lazy_split_filter_data *filter = static_cast<lazy_split_filter_data*>(
		bzalloc(sizeof(struct lazy_split_filter_data))
	);
	*/
	

	filter->context = context;
	filter->cv_frame_count = 1;

	/* TODO these need to be deleted */
	filter->frame_buf = new lazysplits::cv_frame_buf;
	filter->thread_h = new lazysplits::cv_thread_handler;
	
	char* img_path =  obs_module_file("img");
	char* effect_path =  obs_module_file("effect/calibration.effect");

	if( img_path ){filter->calib_img_path = img_path; }
	else{ blog( LOG_INFO, "[lazysplits] obs_module_file(\"img\") is null" ); }

	if( effect_path ){
		blog( LOG_INFO, "[lazysplits] creating calibration effect : %s", effect_path );
		obs_enter_graphics();
		filter->calib_effect = gs_effect_create_from_file(effect_path, NULL);
		obs_leave_graphics();
	}
	else{ blog( LOG_INFO, "[lazysplits] obs_module_file(\"effect\") is null" ); }

	lazy_splits_filter_update( filter, settings );

	bfree(img_path);
	bfree(effect_path);
	return filter;
}

static void lazy_splits_filter_destroy(void* data) {
	struct lazy_split_filter_data* filter = static_cast<lazy_split_filter_data*>(data);

	//kill cv thread if it's live
	if( filter->thread_h->cv_thread_is_live() ){
		filter->thread_h->cv_thread_terminate();
	}
	
	obs_enter_graphics();
	gs_effect_destroy(filter->calib_effect);
	gs_image_file_free(filter->calib_img);
	obs_leave_graphics();
	
	//bfree(filter);
	bfree(filter->calib_img);
	//delete filter->frame_buf;
	//delete filter->thread_h;
    delete static_cast<lazy_split_filter_data*>(data);
}

static void lazy_splits_filter_video_tick( void *data, float seconds ){
	struct lazy_split_filter_data* filter = static_cast<lazy_split_filter_data*>(data);

	//initialize cv thread
	if( !filter->thread_h->cv_thread_is_live() ){
		filter->thread_h->cv_thread_init( filter->frame_buf );
	}

	if( filter->cv_frame_count % 100 == 0){
		blog( LOG_INFO, "[lazysplits] framecount : %i, source width : %i, source height : %i",
			filter->cv_frame_count,
			obs_source_get_width(filter->context),
			obs_source_get_height(filter->context)
		);
	}

	filter->cv_frame_count++;

}

static void lazy_splits_filter_render_video(void *data, gs_effect_t *effect){
	struct lazy_split_filter_data* filter = static_cast<lazy_split_filter_data*>(data);

	if( !filter->is_calib || !filter->calib_target || !filter->calib_effect || !filter->calib_img->loaded ){
		obs_source_skip_video_filter(filter->context);
		return;
	}

	if ( !obs_source_process_filter_begin( filter->context, GS_RGBA, OBS_ALLOW_DIRECT_RENDERING) ){
		blog( LOG_INFO, "[lazysplits] !obs_source_process_filter_begin()" );
		return;
	}

	vec2 calib_offset, calib_scale;
	gs_eparam_t *param;

	float calib_opacity = filter->calib_img_opacity/100.0F;
	calib_offset.x = filter->calib_img_x/100.0F;
	calib_offset.y = filter->calib_img_y/100.0F;
	calib_scale.x = filter->calib_img_scale_x/100.0F;
	calib_scale.y = filter->calib_img_scale_y/100.0F;

	//scale centering
	float scale_dif_x = 1.0F - calib_scale.x;
	float scale_dif_y = 1.0F - calib_scale.y;
	calib_offset.x += scale_dif_x * 0.5F;
	calib_offset.y += scale_dif_y * 0.5F;
	
	param = gs_effect_get_param_by_name( filter->calib_effect, "target" );
	gs_effect_set_texture( param, filter->calib_target );
	param = gs_effect_get_param_by_name( filter->calib_effect, "target_opacity" );
	gs_effect_set_float( param, calib_opacity );
	param = gs_effect_get_param_by_name( filter->calib_effect, "target_offset" );
	gs_effect_set_vec2( param, &calib_offset );
	param = gs_effect_get_param_by_name( filter->calib_effect, "target_scale" );
	gs_effect_set_vec2( param, &calib_scale );
	
	obs_source_process_filter_end( filter->context, filter->calib_effect, 0, 0 );
	//obs_source_skip_video_filter(filter->context);
}

static struct obs_source_frame* lazy_splits_filter_video( void* data, struct obs_source_frame* frame)
{
	struct lazy_split_filter_data* filter = static_cast<lazy_split_filter_data*>(data);

	/*
	if(*filter->calib_img_path){
		cv::Mat img = cv::imread( filter->calib_img_path, CV_LOAD_IMAGE_COLOR );
		if( img.data ){
			cv::Size src_dims( frame->width, frame->height );

			cv::Mat img2, img3;
			cv::resize( img, img2, src_dims );
			cv::cvtColor( img2, img3, CV_BGR2YUV );

			blog( LOG_INFO, "whatever : %i", frame->format );

			//frame->data[0] = img.data;
			std::vector<int> compression_params;
			compression_params.push_back(CV_IMWRITE_PNG_COMPRESSION);
			compression_params.push_back(1);



			cv::imwrite( "img.png", img3, compression_params );
		}
	}
	*/

	/*
	if( filter->cv_frame_count%filter->cv_frame_skip == 0 ){
		if( frame && filter->frame_buf ){
			pthread_mutex_lock( filter->frame_buf->get_mutex() );
			if( !filter->frame_buf->try_push_frame(frame) ){
				blog( LOG_INFO, "[lazysplits] [%i] Frame buffer full", filter->cv_frame_count );
			}
			pthread_mutex_unlock( filter->frame_buf->get_mutex() );
			if( filter->thread_h->cv_thread_is_sleeping() ){ filter->thread_h->cv_thread_wake(); }
		}

		//cv::Mat test_mat = filter->CVTest->OBS_2_CV_BGR( frame );

		//std::vector<int> compression_params;
		//compression_params.push_back(CV_IMWRITE_PNG_COMPRESSION);
		//compression_params.push_back(1);


		//cv::imwrite( "cap.png", test_mat, compression_params );
	}
	*/

	return frame;
}

static bool offset_calibration_toggle( obs_properties_t *props, obs_property_t *p, obs_data_t *settings ){
	bool calib_toggle = obs_data_get_bool( settings, SETTING_IS_OFFSET_CALIBRATION );
	
	obs_property_set_visible( obs_properties_get( props, SETTING_CALIB_IMAGE_PATH ), calib_toggle );
	obs_property_set_visible( obs_properties_get( props, SETTING_CALIB_IMAGE_OPACITY ), calib_toggle );
	obs_property_set_visible( obs_properties_get( props, SETTING_OFFSET_X ), calib_toggle );
	obs_property_set_visible( obs_properties_get( props, SETTING_OFFSET_Y ), calib_toggle );
	obs_property_set_visible( obs_properties_get( props, SETTING_SCALE_X ), calib_toggle );
	obs_property_set_visible( obs_properties_get( props, SETTING_SCALE_Y ), calib_toggle );

	return true;
}

static obs_properties_t* lazy_splits_filter_properties(void* data)
{
	obs_properties_t *props = obs_properties_create();
	
	obs_properties_add_int_slider( props, SETTING_POLLING_MS, TEXT_POLLING_MS, 5, 1000, 5 );
	obs_properties_add_int_slider( props, SETTING_CV_FRAME_SKIP, TEXT_CV_FRAME_SKIP, 1, 30, 1 );
	
	obs_property_t *offset_calibration = obs_properties_add_bool( props, SETTING_IS_OFFSET_CALIBRATION, TEXT_IS_OFFSET_CALIBRATION );
	obs_property_set_modified_callback( offset_calibration, offset_calibration_toggle );
	
	char* calib_img_path = obs_module_file("img");
	/* TODO : narrow this down */
	char* calib_img_filter_str =  "All Image Files (*.bmp *.jpg *.jpeg *.tga *.gif *.png);;All Files (*.*)";


	obs_properties_add_path( props, SETTING_CALIB_IMAGE_PATH, TEXT_CALIB_IMAGE_PATH, OBS_PATH_FILE, calib_img_filter_str, calib_img_path );

	obs_properties_add_float_slider( props, SETTING_CALIB_IMAGE_OPACITY, TEXT_CALIB_IMAGE_OPACITY, 0.0, 100.0, 1.00 );
	obs_properties_add_float_slider( props, SETTING_OFFSET_X, TEXT_OFFSET_X, -50.0, 50.0, 0.05 );
	obs_properties_add_float_slider( props, SETTING_OFFSET_Y, TEXT_OFFSET_Y, -50.0, 50.0, 0.05 );
	obs_properties_add_float_slider( props, SETTING_SCALE_X, TEXT_SCALE_X, 50.0, 150.0, 0.05 );
	obs_properties_add_float_slider( props, SETTING_SCALE_Y, TEXT_SCALE_Y, 50.0, 150.0, 0.05 );

	//UNUSED_PARAMETER(data);
	bfree(calib_img_path);
	bfree(calib_img_filter_str);
	return props;
}

static void lazy_splits_filter_defaults(obs_data_t *settings)
{
	obs_data_set_default_int( settings, SETTING_POLLING_MS, 50 );
	obs_data_set_default_int( settings, SETTING_CV_FRAME_SKIP, 10 );

	obs_data_set_default_bool( settings, SETTING_IS_OFFSET_CALIBRATION, false );
	
	obs_data_set_default_double( settings, SETTING_CALIB_IMAGE_OPACITY, 100.0F );
	obs_data_set_default_double( settings, SETTING_OFFSET_X, 0.0F );
	obs_data_set_default_double( settings, SETTING_OFFSET_Y, 0.0F );
	obs_data_set_default_double( settings, SETTING_SCALE_X, 100.0F );
	obs_data_set_default_double( settings, SETTING_SCALE_Y, 100.0F );
}

struct obs_source_info lazy_splits_filter = {
    /* ----------------------------------------------------------------- */
    /* Required implementation*/

    /* id                  */ "lazy_splits_filter",
    /* type                */ OBS_SOURCE_TYPE_FILTER,
    /* output_flags        */ OBS_SOURCE_VIDEO,
    /* get_name            */ lazy_splits_filter_name,
    /* create              */ lazy_splits_filter_create,
    /* destroy             */ lazy_splits_filter_destroy,
    /* get_width           */ 0,
    /* get_height          */ 0,

    /* ----------------------------------------------------------------- */
    /* Optional implementation */

    /* get_defaults        */ lazy_splits_filter_defaults,
    /* get_properties      */ lazy_splits_filter_properties,
    /* update              */ lazy_splits_filter_update,
    /* activate            */ 0,
    /* deactivate          */ 0,
    /* show                */ 0,
    /* hide                */ 0,
    /* video_tick          */ lazy_splits_filter_video_tick,
    /* video_render        */ lazy_splits_filter_render_video,
    /* filter_video        */ lazy_splits_filter_video,
    /* filter_audio        */ 0,
    /* enum_active_sources */ 0,
    /* save                */ 0,
    /* load                */ 0,
    /* mouse_click         */ 0,
    /* mouse_move          */ 0,
    /* mouse_wheel         */ 0,
    /* focus               */ 0,
    /* key_click           */ 0,
    /* filter_remove       */ 0,
    /* type_data           */ 0,
    /* free_type_data      */ 0,
    /* audio_render        */ 0
};



//OBS_MODULE_USE_DEFAULT_LOCALE("sm64-lazy-splits", "en-US")

MODULE_EXPORT const char* obs_module_name(void) {
    return "SM64 Lazy Splits";
}

OBS_DECLARE_MODULE()

MODULE_EXPORT bool obs_module_load(void) {
	obs_register_source(&lazy_splits_filter);

	return true;
}