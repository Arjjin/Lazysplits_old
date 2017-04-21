#include <obs-module.h>

#include <graphics\vec2.h>

#include <opencv2\highgui\highgui.hpp>
#include <opencv2\imgproc\imgproc.hpp>

#include "SM64_constants.h"
#include "ls_filter_data.h"
#include "ls_util.h"

#include <string>

/* TODO : limit to one filter instance in OBS*/

#define SETTING_POLLING_MS "polling_ms"
#define SETTING_CV_FRAME_SKIP "cv_frame_skip"
#define SETTING_IS_OFFSET_CALIBRATION "is_offset_calibration"
#define SETTING_CALIB_IMAGE_PATH "calib_image_path"
#define SETTING_CALIB_IMAGE_OPACITY "calib_image_opacity"
#define SETTING_OFFSET_X "offset_x"
#define SETTING_OFFSET_Y "offset_y"
#define SETTING_SCALE_X "scale_x"
#define SETTING_SCALE_Y "scale_y"

#define TEXT_POLLING_MS "Polling time (ms)"
#define TEXT_CV_FRAME_SKIP "Analyze every (n) frames"
#define TEXT_IS_OFFSET_CALIBRATION "Source calibration"
#define TEXT_CALIB_IMAGE_PATH "Calibration image"
#define TEXT_CALIB_IMAGE_OPACITY "Calibration image opacity"
#define TEXT_OFFSET_X "Frame x-offset"
#define TEXT_OFFSET_Y "Frame y-offset"
#define TEXT_SCALE_X "Frame width scaling"
#define TEXT_SCALE_Y "Frame height scaling"

using namespace lazysplits;

static const char* ls_filter_name(void* type_data) {
    return "SM64 Lazy Splits";
}

static void ls_filter_update(void *data, obs_data_t *settings)
{
	struct ls_filter_data *filter = static_cast<ls_filter_data*>(data);

	filter->polling_ms = static_cast<uint64_t>( obs_data_get_int( settings, SETTING_POLLING_MS ) );
	filter->cv_frame_skip = static_cast<uint64_t>( obs_data_get_int( settings, SETTING_CV_FRAME_SKIP ) );
	
	filter->is_calib = obs_data_get_bool( settings, SETTING_IS_OFFSET_CALIBRATION );
	filter->calib->try_set_image( obs_data_get_string( settings, SETTING_CALIB_IMAGE_PATH ) );

	filter->calib->lock_mutex();
	filter->calib->set_opacity( obs_data_get_double( settings, SETTING_CALIB_IMAGE_OPACITY )/100.0F );
	filter->calib->set_offset_x( obs_data_get_double( settings, SETTING_OFFSET_X )/100.0F );
	filter->calib->set_offset_y( obs_data_get_double( settings, SETTING_OFFSET_Y )/100.0F );
	filter->calib->set_scale_x( obs_data_get_double( settings, SETTING_SCALE_X )/100.0F );
	filter->calib->set_scale_y( obs_data_get_double( settings, SETTING_SCALE_Y )/100.0F );
	filter->calib->unlock_mutex();

}

static void* ls_filter_create( obs_data_t* settings, obs_source_t* context )
{
	struct ls_filter_data *filter = new ls_filter_data;

	filter->context = context;
	filter->calib->set_source(context);
	filter->cv_frame_count = 1;
	
	char* effect_path =  obs_module_file("effect/calibration.effect");
	if( effect_path ){
		blog( LOG_INFO, "[lazysplits] creating calibration effect : %s", effect_path );
		filter->calib->set_effect(effect_path);
	}
	else{ blog( LOG_INFO, "[lazysplits] obs_module_file(\"effect\") is null" ); }
	bfree(effect_path);

	ls_filter_update( filter, settings );

	return filter;
}

static void ls_filter_destroy(void* data) {
    delete static_cast<ls_filter_data*>(data);
}

static void ls_filter_video_tick( void *data, float seconds ){
	struct ls_filter_data* filter = static_cast<ls_filter_data*>(data);

	//initialize ls thread
	if( !filter->thread->ls_thread_is_live() ){
		filter->thread->ls_thread_init( filter->frame_buf, filter->calib );
	}

	if( filter->cv_frame_count % 100 == 0){
		blog( LOG_INFO, "[lazysplits] framecount : %i, source width : %i, source height : %i, calib x : %f,calib y : %f, calib scale x : %f, calib scale y : %f, test : %f",
			filter->cv_frame_count,
			obs_source_get_width(filter->context),
			obs_source_get_height(filter->context),
			filter->calib->get_offset_x(),
			filter->calib->get_offset_y(),
			filter->calib->get_scale_x(),
			filter->calib->get_scale_y(),
			(float) obs_source_get_width(filter->context) - ( SM64_BASE_WIDTH * filter->calib->get_scale_x() )
		);
	}

	filter->cv_frame_count++;

}

static void ls_filter_render_video(void *data, gs_effect_t *effect){
	struct ls_filter_data* filter = static_cast<ls_filter_data*>(data);
	
	if( !filter->is_calib || !filter->calib->tex_loaded() || !filter->calib->effect_loaded() || !filter->calib->image_loaded() ){
		obs_source_skip_video_filter(filter->context);
		return;
	}

	if ( !obs_source_process_filter_begin( filter->context, GS_RGBA, OBS_ALLOW_DIRECT_RENDERING) ){
		return;
	}

	vec2 calib_offset, calib_scale;
	gs_eparam_t *param;

	filter->calib->lock_mutex();
	float calib_opacity = filter->calib->get_opacity();
	calib_offset.x = filter->calib->get_offset_x();
	calib_offset.y = filter->calib->get_offset_y();
	calib_scale.x = filter->calib->get_scale_x();
	calib_scale.y = filter->calib->get_scale_y();
	filter->calib->unlock_mutex();

	//aspect and scale correction
	float source_width = obs_source_get_width(filter->context);
	float source_height = obs_source_get_height(filter->context);
	float tex_width = gs_texture_get_width( filter->calib->get_tex() );
	float tex_height = gs_texture_get_height( filter->calib->get_tex() );
	float aspect_correction = (source_width/source_height) / (tex_width/tex_height);

	if( source_width >= source_height ){
		float scale_correction = tex_height/source_height;
		calib_scale.x = (calib_scale.x / aspect_correction) * scale_correction;
		calib_scale.y *= scale_correction;
	}
	else{
		float scale_correction = tex_width/source_width;
		calib_scale.y = (calib_scale.y / aspect_correction) * scale_correction;
		calib_scale.x *= scale_correction;
	}

	//scale centering
	float scale_dif_x = 1.0F - calib_scale.x;
	float scale_dif_y = 1.0F - calib_scale.y;
	calib_offset.x += scale_dif_x * 0.5F;
	calib_offset.y += scale_dif_y * 0.5F;

	param = gs_effect_get_param_by_name( filter->calib->get_effect(), "target" );
	gs_effect_set_texture( param, filter->calib->get_tex() );
	param = gs_effect_get_param_by_name( filter->calib->get_effect(), "target_opacity" );
	gs_effect_set_float( param, calib_opacity );
	param = gs_effect_get_param_by_name( filter->calib->get_effect(), "target_offset" );
	gs_effect_set_vec2( param, &calib_offset );
	param = gs_effect_get_param_by_name( filter->calib->get_effect(), "target_scale" );
	gs_effect_set_vec2( param, &calib_scale );

	obs_source_process_filter_end( filter->context, filter->calib->get_effect(), 0, 0 );
}

static struct obs_source_frame* ls_filter_video( void* data, struct obs_source_frame* frame)
{
	struct ls_filter_data* filter = static_cast<ls_filter_data*>(data);

	if( !filter->is_calib && filter->cv_frame_count%filter->cv_frame_skip == 0 ){
		if( frame && filter->frame_buf ){
			pthread_mutex_lock( filter->frame_buf->get_mutex() );
			bool frame_pushed = filter->frame_buf->try_push_frame(frame);
			pthread_mutex_unlock( filter->frame_buf->get_mutex() );
			if( frame_pushed && filter->thread->ls_thread_is_sleeping() ){ filter->thread->ls_thread_wake(); }
		}
	}

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


static obs_properties_t* ls_filter_properties(void* data)
{
	obs_properties_t *props = obs_properties_create();
	
	obs_properties_add_int_slider( props, SETTING_POLLING_MS, TEXT_POLLING_MS, 5, 1000, 5 );
	obs_properties_add_int_slider( props, SETTING_CV_FRAME_SKIP, TEXT_CV_FRAME_SKIP, 1, 30, 1 );
	
	obs_property_t *offset_calibration = obs_properties_add_bool( props, SETTING_IS_OFFSET_CALIBRATION, TEXT_IS_OFFSET_CALIBRATION );
	obs_property_set_modified_callback( offset_calibration, offset_calibration_toggle );
	
	/* TODO : narrow this down */
	std::string calib_img_path = obs_module_file("img");
	std::string calib_img_filter_str =  "All Image Files (*.bmp *.jpg *.jpeg *.tga *.gif *.png);;All Files (*.*)";


	obs_properties_add_path( props, SETTING_CALIB_IMAGE_PATH, TEXT_CALIB_IMAGE_PATH, OBS_PATH_FILE, calib_img_filter_str.c_str(), calib_img_path.c_str() );

	obs_properties_add_float_slider( props, SETTING_CALIB_IMAGE_OPACITY, TEXT_CALIB_IMAGE_OPACITY, 0.0, 100.0, 1.00 );
	obs_properties_add_float_slider( props, SETTING_OFFSET_X, TEXT_OFFSET_X, -50.0, 50.0, 0.05 );
	obs_properties_add_float_slider( props, SETTING_OFFSET_Y, TEXT_OFFSET_Y, -50.0, 50.0, 0.05 );
	obs_properties_add_float_slider( props, SETTING_SCALE_X, TEXT_SCALE_X, 20.0, 500.0, 0.05 );
	obs_properties_add_float_slider( props, SETTING_SCALE_Y, TEXT_SCALE_Y, 20.0, 500.0, 0.05 );

	return props;
}

static void ls_filter_defaults(obs_data_t *settings)
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

struct obs_source_info ls_filter = {
    /* ----------------------------------------------------------------- */
    /* Required implementation*/

    /* id                  */ "ls_filter",
    /* type                */ OBS_SOURCE_TYPE_FILTER,
    /* output_flags        */ OBS_SOURCE_VIDEO,
    /* get_name            */ ls_filter_name,
    /* create              */ ls_filter_create,
    /* destroy             */ ls_filter_destroy,
    /* get_width           */ 0,
    /* get_height          */ 0,

    /* ----------------------------------------------------------------- */
    /* Optional implementation */

    /* get_defaults        */ ls_filter_defaults,
    /* get_properties      */ ls_filter_properties,
    /* update              */ ls_filter_update,
    /* activate            */ 0,
    /* deactivate          */ 0,
    /* show                */ 0,
    /* hide                */ 0,
    /* video_tick          */ ls_filter_video_tick,
    /* video_render        */ ls_filter_render_video,
    /* filter_video        */ ls_filter_video,
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

MODULE_EXPORT const char* obs_module_name(void) {
    return "SM64 Lazy Splits";
}

OBS_DECLARE_MODULE()

MODULE_EXPORT bool obs_module_load(void) {
	obs_register_source(&ls_filter);

	return true;
}
