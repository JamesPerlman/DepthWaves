/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2007-2015 Adobe Systems Incorporated                  */
/* All Rights Reserved.                                            */
/*                                                                 */
/* NOTICE:  All information contained herein is, and remains the   */
/* property of Adobe Systems Incorporated and its suppliers, if    */
/* any.  The intellectual and technical concepts contained         */
/* herein are proprietary to Adobe Systems Incorporated and its    */
/* suppliers and may be covered by U.S. and Foreign Patents,       */
/* patents in process, and are protected by trade secret or        */
/* copyright law.  Dissemination of this information or            */
/* reproduction of this material is strictly forbidden unless      */
/* prior written permission is obtained from Adobe Systems         */
/* Incorporated.                                                   */
/*                                                                 */
/*******************************************************************/

/*	DepthWaves.cpp	

	This is a sample OpenGL plugin. The framework is done for you.
	Use it to create more funky effects.
	
	Revision History

	Version		Change													Engineer	Date
	=======		======													========	======
	1.0			Win and Mac versions use the same base files.			anindyar	7/4/2007
	1.1			Add OpenGL context switching to play nicely with
				AE's own OpenGL usage (thanks Brendan Bolles!)			zal			8/13/2012
	2.0			Completely re-written for OGL 3.3 and threads			aparente	9/30/2015
	2.1			Added new entry point									zal			9/15/2017

*/

#include "DepthWaves.h"

#include "CameraTransform.hpp"

#include "GL_base.h"
#include "Smart_Utils.h"
#include "AEFX_SuiteHelper.h"

#include <thread>
#include <atomic>
#include <map>
#include <mutex>
#include "vmath.hpp"
#include <assert.h>

using namespace AESDK_OpenGL;
using namespace gl45core;

#include "glbinding/gl45ext/gl.h"
#include <glbinding/gl/extension.h>

/* AESDK_OpenGL effect specific variables */

namespace {
	THREAD_LOCAL int t_thread = -1;

	std::atomic_int S_cnt;
	std::map<int, std::shared_ptr<AESDK_OpenGL::AESDK_OpenGL_EffectRenderData> > S_render_contexts;
	std::recursive_mutex S_mutex;

	AESDK_OpenGL::AESDK_OpenGL_EffectCommonDataPtr S_DepthWaves_EffectCommonData; //global context
	std::string S_ResourcePath;

	// - OpenGL resources are restricted per thread, mimicking the OGL driver
	// - The filter will eliminate all TLS (Thread Local Storage) at PF_Cmd_GLOBAL_SETDOWN
	AESDK_OpenGL::AESDK_OpenGL_EffectRenderDataPtr GetCurrentRenderContext()
	{
		S_mutex.lock();
		AESDK_OpenGL::AESDK_OpenGL_EffectRenderDataPtr result;

		if (t_thread == -1) {
			t_thread = S_cnt++;

			result.reset(new AESDK_OpenGL::AESDK_OpenGL_EffectRenderData());
			S_render_contexts[t_thread] = result;
		}
		else {
			result = S_render_contexts[t_thread];
		}
		S_mutex.unlock();
		return result;
	}

#ifdef AE_OS_WIN
	std::string get_string_from_wcs(const wchar_t* pcs)
	{
		int res = WideCharToMultiByte(CP_ACP, 0, pcs, -1, NULL, 0, NULL, NULL);

		std::auto_ptr<char> shared_pbuf(new char[res]);

		char *pbuf = shared_pbuf.get();

		res = WideCharToMultiByte(CP_ACP, 0, pcs, -1, pbuf, res, NULL, NULL);

		return std::string(pbuf);
	}
#endif

	void DrawVertices(GLuint vertBuffer, GLsizei numVerts)
	{
		glEnableVertexAttribArray(PositionSlot);
		glEnableVertexAttribArray(ColorSlot);
		glBindBuffer(GL_ARRAY_BUFFER, vertBuffer);
		glVertexAttribPointer(PositionSlot, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), 0);
		glVertexAttribPointer(ColorSlot, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4 * sizeof(float)));
		glDrawArrays(GL_POINTS, 0, numVerts);
		glDisableVertexAttribArray(PositionSlot);
		glDisableVertexAttribArray(ColorSlot);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	std::string GetResourcesPath(PF_InData		*in_data)
	{
		//initialize and compile the shader objects
		A_UTF16Char pluginFolderPath[AEFX_MAX_PATH];
		PF_GET_PLATFORM_DATA(PF_PlatData_EXE_FILE_PATH_W, &pluginFolderPath);

#ifdef AE_OS_WIN
		std::string resourcePath = get_string_from_wcs((wchar_t*)pluginFolderPath);
		std::string::size_type pos;
		//delete the plugin name
		pos = resourcePath.rfind("\\", resourcePath.length());
		resourcePath = resourcePath.substr(0, pos) + "\\";
#endif
#ifdef AE_OS_MAC
		NSUInteger length = 0;
		A_UTF16Char* tmp = pluginFolderPath;
		while (*tmp++ != 0) {
			++length;
		}
		NSString* newStr = [[NSString alloc] initWithCharacters:pluginFolderPath length : length];
		std::string resourcePath([newStr UTF8String]);
		resourcePath += "/Contents/Resources/";
#endif
		return resourcePath;
	}

	struct CopyPixelFloat_t {
		PF_PixelFloat	*floatBufferP;
		PF_EffectWorld	*input_worldP;
	};

	PF_Err
	CopyPixelFloatIn(
		void			*refcon,
		A_long			x,
		A_long			y,
		PF_PixelFloat	*inP,
		PF_PixelFloat	*)
	{
		CopyPixelFloat_t	*thiS = reinterpret_cast<CopyPixelFloat_t*>(refcon);
		PF_PixelFloat		*outP = thiS->floatBufferP + y * thiS->input_worldP->width + x;

		outP->red = inP->red;
		outP->green = inP->green;
		outP->blue = inP->blue;
		outP->alpha = inP->alpha;

		return PF_Err_NONE;
	}

	PF_Err
	CopyPixelFloatOut(
		void			*refcon,
		A_long			x,
		A_long			y,
		PF_PixelFloat	*,
		PF_PixelFloat	*outP)
	{
		CopyPixelFloat_t		*thiS = reinterpret_cast<CopyPixelFloat_t*>(refcon);
		const PF_PixelFloat		*inP = thiS->floatBufferP + y * thiS->input_worldP->width + x;

		outP->red = inP->red;
		outP->green = inP->green;
		outP->blue = inP->blue;
		outP->alpha = inP->alpha;

		return PF_Err_NONE;
	}


	gl::GLuint UploadTexture(GLuint					 unit,				// >>
							 AEGP_SuiteHandler&		suites,				// >>
							 PF_PixelFormat			format,				// >>
							 PF_EffectWorld			*input_worldP,		// >>
							 PF_EffectWorld			*output_worldP,		// >>
							 PF_InData				*in_data,			// >>
							 size_t& pixSizeOut,						// <<
							 gl::GLenum& glFmtOut,						// <<
							 float& multiplier16bitOut)					// <<

	{
		// - upload to texture memory
		// - we will convert on-the-fly from ARGB to RGBA, and also to pre-multiplied alpha,
		// using a fragment shader
#ifdef _DEBUG
		GLint nUnpackAlignment;
		::glGetIntegerv(GL_UNPACK_ALIGNMENT, &nUnpackAlignment);
		assert(nUnpackAlignment == 4);
#endif
		if (input_worldP == NULL) {
			return 0;
		}

		gl::GLuint texture;
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLint)GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLint)GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLint)GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (GLint)GL_CLAMP_TO_EDGE);

		glTexImage2D(GL_TEXTURE_2D, 0, (GLint)GL_RGBA32F, input_worldP->width, input_worldP->height, 0, GL_RGBA, GL_FLOAT, nullptr);

		switch (format)
		{
		case PF_PixelFormat_ARGB128:
		{
			glFmtOut = GL_FLOAT;
			pixSizeOut = sizeof(PF_PixelFloat);

			std::auto_ptr<PF_PixelFloat> bufferFloat(new PF_PixelFloat[input_worldP->width * input_worldP->height]);
			CopyPixelFloat_t refcon = { bufferFloat.get(), input_worldP };

			CHECK(suites.IterateFloatSuite1()->iterate(in_data,
				0,
				input_worldP->height,
				input_worldP,
				nullptr,
				reinterpret_cast<void*>(&refcon),
				CopyPixelFloatIn,
				output_worldP));

			glPixelStorei(GL_UNPACK_ROW_LENGTH, input_worldP->width);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, input_worldP->width, input_worldP->height, GL_RGBA, glFmtOut, bufferFloat.get());
			break;
		}

		case PF_PixelFormat_ARGB64:
		{
			glFmtOut = GL_UNSIGNED_SHORT;
			pixSizeOut = sizeof(PF_Pixel16);
			multiplier16bitOut = 65535.0f / 32768.0f;

			glPixelStorei(GL_UNPACK_ROW_LENGTH, input_worldP->rowbytes / sizeof(PF_Pixel16));
			PF_Pixel16 *pixelDataStart = NULL;
			PF_GET_PIXEL_DATA16(input_worldP, NULL, &pixelDataStart);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, input_worldP->width, input_worldP->height, GL_RGBA, glFmtOut, pixelDataStart);
			break;
		}

		case PF_PixelFormat_ARGB32:
		{

			glFmtOut = GL_UNSIGNED_BYTE;
			pixSizeOut = sizeof(PF_Pixel8);

			glPixelStorei(GL_UNPACK_ROW_LENGTH, input_worldP->rowbytes / sizeof(PF_Pixel8));
			PF_Pixel8 *pixelDataStart = NULL;
			PF_GET_PIXEL_DATA8(input_worldP, NULL, &pixelDataStart);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, input_worldP->width, input_worldP->height, GL_RGBA, glFmtOut, pixelDataStart);
			break;
		}

		default:
			CHECK(PF_Err_BAD_CALLBACK_PARAM);
			break;
		}

		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		glBindTexture(GL_TEXTURE_2D, 0);

		return texture;
	}

	void ReportIfErrorFramebuffer(PF_InData *in_data, PF_OutData *out_data)
	{
		// Check for errors...
		std::string error_msg;
		if ((error_msg = CheckFramebufferStatus()) != std::string("OK"))
		{
			out_data->out_flags |= PF_OutFlag_DISPLAY_ERROR_MESSAGE;
			PF_SPRINTF(out_data->return_msg, error_msg.c_str());
			CHECK(PF_Err_OUT_OF_MEMORY);
		}
	}

	void ComputeParticles(const AESDK_OpenGL::AESDK_OpenGL_EffectRenderDataPtr& renderContext,
						  gl::GLuint colorLayerTexture,
						  A_long colorWidthL,
						  A_long colorHeightL,
						  gl::GLuint depthLayerTexture,
						  A_long depthWidthL,
						  A_long depthHeightL,
						  float minDepth,
						  float maxDepth,
						  CameraTransform cameraTransform,
						  float multiplier16bit)
	{
		GLuint program = renderContext->computeShaderProgram;
		glUseProgram(program);

		glBindImageTexture(0, colorLayerTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
		glBindImageTexture(1, depthLayerTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, renderContext->vertBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, renderContext->waveBuffer);

		GLuint u;

		u = glGetUniformLocation(program, "minDepth");
		glUniform1f(u, minDepth);

		u = glGetUniformLocation(program, "maxDepth");
		glUniform1f(u, maxDepth);
		
		u = glGetUniformLocation(program, "cameraRot");
		glUniform3fv(u, 1, (gl::GLfloat*)&cameraTransform.rotation);

		u = glGetUniformLocation(program, "cameraPos");
		glUniform3fv(u, 1, (gl::GLfloat*)&cameraTransform.position);

		u = glGetUniformLocation(program, "cameraFov");
		glUniform2fv(u, 1, (gl::GLfloat*)&cameraTransform.fov);

		u = glGetUniformLocation(program, "waveCount");
		glUniform1i(u, 0);

		glDispatchCompute(colorWidthL, colorHeightL, 1);
		glUseProgram(0);

	}

	void RenderGL(const AESDK_OpenGL::AESDK_OpenGL_EffectRenderDataPtr& renderContext,
				  gl::GLuint inputFrameTexture,
				  A_long widthL,
				  A_long heightL,
				  PF_FpLong minBlockSize,
				  vmath::Matrix4 projectionMatrix,
				  float multiplier16bit)
	{

		gl::GLuint program = renderContext->visualShaderProgram;
		GLuint u;

		glEnable(GL_DEPTH_TEST);

		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		glBlendEquation(GL_FUNC_ADD);

		// send uniforms to shader
		glUseProgram(program);
		u = glGetUniformLocation(program, "size");
		glUniform1f(u, minBlockSize);

		u = glGetUniformLocation(program, "modelViewProjectionMatrix");
		glUniformMatrix4fv(u, 1, GL_TRUE, (gl::GLfloat*)&projectionMatrix);

		// render
		glBindVertexArray(renderContext->vao);

		DrawVertices(renderContext->vertBuffer, widthL * heightL);
		glBindVertexArray(0);

		glUseProgram(0);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);
	}

	void DownloadTexture(const AESDK_OpenGL::AESDK_OpenGL_EffectRenderDataPtr& renderContext,
						 AEGP_SuiteHandler&		suites,				// >>
						 PF_EffectWorld			*input_worldP,		// >>
						 PF_EffectWorld			*output_worldP,		// >>
						 PF_InData				*in_data,			// >>
						 PF_PixelFormat			format,				// >>
						 size_t					pixSize,			// >>
						 gl::GLenum				glFmt				// >>
						 )
	{
		//download from texture memory onto the same surface
		PF_Handle bufferH = NULL;
		bufferH = suites.HandleSuite1()->host_new_handle(((renderContext->mRenderBufferWidthSu * renderContext->mRenderBufferHeightSu)* pixSize));
		if (!bufferH) {
			CHECK(PF_Err_OUT_OF_MEMORY);
		}
		void *bufferP = suites.HandleSuite1()->host_lock_handle(bufferH);

		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glReadPixels(0, 0, renderContext->mRenderBufferWidthSu, renderContext->mRenderBufferHeightSu, GL_RGBA, glFmt, bufferP);

		switch (format)
		{
		case PF_PixelFormat_ARGB128:
		{
			PF_PixelFloat* bufferFloatP = reinterpret_cast<PF_PixelFloat*>(bufferP);
			CopyPixelFloat_t refcon = { bufferFloatP, input_worldP };

			CHECK(suites.IterateFloatSuite1()->iterate(in_data,
				0,
				input_worldP->height,
				input_worldP,
				nullptr,
				reinterpret_cast<void*>(&refcon),
				CopyPixelFloatOut,
				output_worldP));
			break;
		}

		case PF_PixelFormat_ARGB64:
		{
			PF_Pixel16* buffer16P = reinterpret_cast<PF_Pixel16*>(bufferP);

			//copy to output_worldP
			for (int y = 0; y < output_worldP->height; ++y)
			{
				PF_Pixel16 *pixelDataStart = NULL;
				PF_GET_PIXEL_DATA16(output_worldP, NULL, &pixelDataStart);
				::memcpy(pixelDataStart + (y * output_worldP->rowbytes / sizeof(PF_Pixel16)),
					buffer16P + (y * renderContext->mRenderBufferWidthSu),
					output_worldP->width * sizeof(PF_Pixel16));
			}
			break;
		}

		case PF_PixelFormat_ARGB32:
		{
			PF_Pixel8 *buffer8P = reinterpret_cast<PF_Pixel8*>(bufferP);

			//copy to output_worldP
			for (int y = 0; y < output_worldP->height; ++y)
			{
				PF_Pixel8 *pixelDataStart = NULL;
				PF_GET_PIXEL_DATA8(output_worldP, NULL, &pixelDataStart);
				::memcpy(pixelDataStart + (y * output_worldP->rowbytes / sizeof(PF_Pixel8)),
					buffer8P + (y * renderContext->mRenderBufferWidthSu),
					output_worldP->width * sizeof(PF_Pixel8));
			}
			break;
		}

		default:
			CHECK(PF_Err_BAD_CALLBACK_PARAM);
			break;
		}

		//clean the data after being copied
		suites.HandleSuite1()->host_unlock_handle(bufferH);
		suites.HandleSuite1()->host_dispose_handle(bufferH);
	}
} // anonymous namespace

static PF_Err 
About (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	
	suites.ANSICallbacksSuite1()->sprintf(	out_data->return_msg,
											"%s v%d.%d\r%s",
											STR(StrID_Name), 
											MAJOR_VERSION, 
											MINOR_VERSION, 
											STR(StrID_Description));
	return PF_Err_NONE;
}

static PF_Err
GlobalSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	out_data->my_version = PF_VERSION(	MAJOR_VERSION, 
										MINOR_VERSION,
										BUG_VERSION, 
										STAGE_VERSION, 
										BUILD_VERSION);

	out_data->out_flags = 	PF_OutFlag_DEEP_COLOR_AWARE;
	
	out_data->out_flags2 =	PF_OutFlag2_FLOAT_COLOR_AWARE	|
							PF_OutFlag2_SUPPORTS_SMART_RENDER;
	
	PF_Err err = PF_Err_NONE;
	try
	{
		// always restore back AE's own OGL context
		SaveRestoreOGLContext oSavedContext;
		AEGP_SuiteHandler suites(in_data->pica_basicP);

		//Now comes the OpenGL part - OS specific loading to start with
		S_DepthWaves_EffectCommonData.reset(new AESDK_OpenGL::AESDK_OpenGL_EffectCommonData());
		AESDK_OpenGL_Startup(*S_DepthWaves_EffectCommonData.get());
		
		S_ResourcePath = GetResourcesPath(in_data);
	}
	catch(PF_Err& thrown_err)
	{
		err = thrown_err;
	}
	catch (...)
	{
		err = PF_Err_OUT_OF_MEMORY;
	}

	return err;
}

static PF_Err 
ParamsSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err		err		= PF_Err_NONE;
	PF_ParamDef	def;	

	AEFX_CLR_STRUCT(def);

	PF_ADD_LAYER(
		STR(StrID_DepthMap_Layer_Name),
		PF_LayerDefault_NONE,
		DEPTHMAP_LAYER_DISK_ID
	);

	AEFX_CLR_STRUCT(def);

	PF_ADD_CHECKBOXX(
		STR(StrID_Emitter_Impulse_Switch_Name),
		DepthWaves_EMITTER_IMPULSE_DEFAULT,
		PF_ParamFlag_RESERVED1,
		EMITTER_IMPULSE_DISK_ID
	);

	AEFX_CLR_STRUCT(def);

	PF_ADD_POINT_3D(
		STR(StrID_Emitter_Position_Point_Name),
		0,
		0,
		0,
		EMITTER_POSITION_DISK_ID
	);

	AEFX_CLR_STRUCT(def);

	PF_ADD_FLOAT_SLIDERX(
		STR(StrID_Min_Depth_Slider_Name), 
		DepthWaves_DEPTH_SLIDER_MIN,
		DepthWaves_DEPTH_SLIDER_MAX, 
		DepthWaves_DEPTH_SLIDER_MIN, 
		DepthWaves_DEPTH_SLIDER_MAX, 
		DepthWaves_MIN_DEPTH_DEFAULT,
		PF_Precision_HUNDREDTHS,
		PF_ValueDisplayFlag_NONE,
		PF_ParamFlag_RESERVED1,
		MIN_DEPTH_DISK_ID
	);

	AEFX_CLR_STRUCT(def);

	PF_ADD_FLOAT_SLIDERX(
		STR(StrID_Max_Depth_Slider_Name),
		DepthWaves_DEPTH_SLIDER_MIN,
		DepthWaves_DEPTH_SLIDER_MAX,
		DepthWaves_DEPTH_SLIDER_MIN,
		DepthWaves_DEPTH_SLIDER_MAX,
		DepthWaves_MAX_DEPTH_DEFAULT,
		PF_Precision_HUNDREDTHS,
		PF_ValueDisplayFlag_NONE,
		PF_ParamFlag_RESERVED1,
		MAX_DEPTH_DISK_ID
	);

	AEFX_CLR_STRUCT(def);

	PF_ADD_FLOAT_SLIDERX(
		STR(StrID_Min_Block_Size_Slider_Name),
		DepthWaves_BLOCK_SIZE_SLIDER_MIN,
		DepthWaves_BLOCK_SIZE_SLIDER_MAX,
		DepthWaves_BLOCK_SIZE_SLIDER_MIN,
		DepthWaves_BLOCK_SIZE_SLIDER_MAX,
		DepthWaves_MIN_BLOCK_SIZE_DEFAULT,
		PF_Precision_HUNDREDTHS,
		PF_ValueDisplayFlag_NONE,
		PF_ParamFlag_RESERVED1,
		MIN_BLOCK_SIZE_DISK_ID
	);

	AEFX_CLR_STRUCT(def);
	
	PF_ADD_FLOAT_SLIDERX(
		STR(StrID_Max_Block_Size_Slider_Name),
		DepthWaves_BLOCK_SIZE_SLIDER_MIN,
		DepthWaves_BLOCK_SIZE_SLIDER_MAX,
		DepthWaves_BLOCK_SIZE_SLIDER_MIN,
		DepthWaves_BLOCK_SIZE_SLIDER_MAX,
		DepthWaves_MAX_BLOCK_SIZE_DEFAULT,
		PF_Precision_HUNDREDTHS,
		PF_ValueDisplayFlag_NONE,
		PF_ParamFlag_RESERVED1,
		MAX_BLOCK_SIZE_DISK_ID
	);

	AEFX_CLR_STRUCT(def);

	PF_ADD_FLOAT_SLIDERX(
		STR(StrID_Wave_Displacement_Slider_Name),
		DepthWaves_WAVE_DISPLACEMENT_SLIDER_MIN,
		DepthWaves_WAVE_DISPLACEMENT_SLIDER_MAX,
		DepthWaves_WAVE_DISPLACEMENT_SLIDER_MIN,
		DepthWaves_WAVE_DISPLACEMENT_SLIDER_MAX,
		DepthWaves_WAVE_DISPLACEMENT_DEFAULT,
		PF_Precision_HUNDREDTHS,
		PF_ValueDisplayFlag_NONE,
		PF_ParamFlag_RESERVED1,
		WAVE_DISPLACEMENT_DISK_ID
	);

	AEFX_CLR_STRUCT(def);

	PF_ADD_FLOAT_SLIDERX(
		STR(StrID_Wave_Velocity_Slider_Name),
		DepthWaves_WAVE_VELOCITY_SLIDER_MIN,
		DepthWaves_WAVE_VELOCITY_SLIDER_MAX,
		DepthWaves_WAVE_VELOCITY_SLIDER_MIN,
		DepthWaves_WAVE_VELOCITY_SLIDER_MAX,
		DepthWaves_WAVE_VELOCITY_DEFAULT,
		PF_Precision_HUNDREDTHS,
		PF_ValueDisplayFlag_NONE,
		PF_ParamFlag_CANNOT_TIME_VARY,
		WAVE_VELOCITY_DISK_ID
	);

	AEFX_CLR_STRUCT(def);

	PF_ADD_FLOAT_SLIDERX(
		STR(StrID_Wave_Decay_Slider_Name),
		DepthWaves_WAVE_DECAY_SLIDER_MIN,
		DepthWaves_WAVE_DECAY_SLIDER_MAX,
		DepthWaves_WAVE_DECAY_SLIDER_MIN,
		DepthWaves_WAVE_DECAY_SLIDER_MAX,
		DepthWaves_WAVE_DECAY_DEFAULT,
		PF_Precision_HUNDREDTHS,
		PF_ValueDisplayFlag_NONE,
		PF_ParamFlag_RESERVED1,
		WAVE_DECAY_DISK_ID
	);

	AEFX_CLR_STRUCT(def);
	
	out_data->num_params = DepthWaves_NUM_PARAMS;

	return err;
}


static PF_Err 
GlobalSetdown (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err			err			=	PF_Err_NONE;

	try
	{
		// always restore back AE's own OGL context
		SaveRestoreOGLContext oSavedContext;

		S_mutex.lock();
		S_render_contexts.clear();
		S_mutex.unlock();

		//OS specific unloading
		AESDK_OpenGL_Shutdown(*S_DepthWaves_EffectCommonData.get());
		S_DepthWaves_EffectCommonData.reset();
		S_ResourcePath.clear();

		if (in_data->sequence_data) {
			PF_DISPOSE_HANDLE(in_data->sequence_data);
			out_data->sequence_data = NULL;
		}
	}
	catch(PF_Err& thrown_err)
	{
		err = thrown_err;
	}
	catch (...)
	{
		err = PF_Err_OUT_OF_MEMORY;
	}

	return err;
}

static PF_Err
PreRender(
	PF_InData				*in_data,
	PF_OutData				*out_data,
	PF_PreRenderExtra		*extra)
{
	PF_Err	err = PF_Err_NONE,
			err2 = PF_Err_NONE;

	PF_ParamDef emitter_impulse_param,
				emitter_position_param,
				min_depth_param,
				max_depth_param,
				min_block_size_param,
				max_block_size_param,
				wave_displacement_param,
				wave_velocity_param,
				wave_decay_param;

	PF_CheckoutResult in_result;
	PF_CheckoutResult depthMapLayer_result;

	PF_RenderRequest req = extra->input->output_request;

	ERR(extra->cb->checkout_layer(in_data->effect_ref,
		DepthWaves_INPUT,
		DepthWaves_INPUT,
		&req,
		in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&in_result));

	ERR(extra->cb->checkout_layer(in_data->effect_ref,
		DepthWaves_DEPTHMAP_LAYER,
		DepthWaves_DEPTHMAP_LAYER,
		&req,
		in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&depthMapLayer_result));

	AEFX_CLR_STRUCT(emitter_impulse_param);

	ERR(PF_CHECKOUT_PARAM(
		in_data,
		DepthWaves_EMITTER_IMPULSE,
		in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&emitter_impulse_param));

	AEFX_CLR_STRUCT(emitter_position_param);

	ERR(PF_CHECKOUT_PARAM(
		in_data,
		DepthWaves_EMITTER_POSITION,
		in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&emitter_position_param));

	AEFX_CLR_STRUCT(min_depth_param);

	ERR(PF_CHECKOUT_PARAM(
		in_data,
		DepthWaves_MIN_DEPTH,
		in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&min_depth_param));

	AEFX_CLR_STRUCT(max_depth_param);

	ERR(PF_CHECKOUT_PARAM(
		in_data,
		DepthWaves_MAX_DEPTH,
		in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&max_depth_param));

	AEFX_CLR_STRUCT(min_block_size_param);

	ERR(PF_CHECKOUT_PARAM(
		in_data,
		DepthWaves_MIN_BLOCK_SIZE,
		in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&min_block_size_param));

	AEFX_CLR_STRUCT(max_block_size_param);

	ERR(PF_CHECKOUT_PARAM(
		in_data,
		DepthWaves_MAX_BLOCK_SIZE,
		in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&max_block_size_param));

	AEFX_CLR_STRUCT(wave_displacement_param);

	ERR(PF_CHECKOUT_PARAM(
		in_data,
		DepthWaves_WAVE_DISPLACEMENT,
		in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&wave_displacement_param));

	AEFX_CLR_STRUCT(wave_velocity_param);

	ERR(PF_CHECKOUT_PARAM(
		in_data,
		DepthWaves_WAVE_VELOCITY,
		in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&wave_velocity_param));

	AEFX_CLR_STRUCT(wave_decay_param);

	ERR(PF_CHECKOUT_PARAM(
		in_data,
		DepthWaves_WAVE_DECAY,
		in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&wave_decay_param));

	if (!err){
		UnionLRect(&in_result.result_rect, &extra->output->result_rect);
		UnionLRect(&in_result.max_result_rect, &extra->output->max_result_rect);
	}

	ERR2(PF_CHECKIN_PARAM(in_data, &emitter_impulse_param));
	ERR2(PF_CHECKIN_PARAM(in_data, &emitter_position_param));
	ERR2(PF_CHECKIN_PARAM(in_data, &min_depth_param));
	ERR2(PF_CHECKIN_PARAM(in_data, &max_depth_param));
	ERR2(PF_CHECKIN_PARAM(in_data, &min_block_size_param));
	ERR2(PF_CHECKIN_PARAM(in_data, &max_block_size_param));
	ERR2(PF_CHECKIN_PARAM(in_data, &wave_displacement_param));
	ERR2(PF_CHECKIN_PARAM(in_data, &wave_velocity_param));
	ERR2(PF_CHECKIN_PARAM(in_data, &wave_decay_param));

	return err;
}

static PF_Err
SmartRender(
	PF_InData				*in_data,
	PF_OutData				*out_data,
	PF_SmartRenderExtra		*extra)
{
	PF_Err				err = PF_Err_NONE,
						err2 = PF_Err_NONE;

	PF_EffectWorld		*input_worldP = NULL,
						*depth_worldP = NULL,
						*output_worldP = NULL;
	PF_WorldSuite2		*wsP = NULL;
	PF_PixelFormat		format = PF_PixelFormat_INVALID;
	PF_FpLong			minDepth, maxDepth,
						minBlockSize, maxBlockSize;

	A_Matrix4			camera_matrix4;
	A_short				image_plane_widthL = 0,
						image_plane_heightL = 0;
	A_FpLong			focal_lengthF = 0;
	A_Time				comp_timeT = { 0, 1 };
	AEGP_LayerH			camera_layerH = NULL;


	AEGP_SuiteHandler suites(in_data->pica_basicP);

	PF_ParamDef minDepth_param,
				maxDepth_param,
				minBlockSize_param;

	AEFX_CLR_STRUCT(minDepth_param);

	ERR(PF_CHECKOUT_PARAM(in_data,
		DepthWaves_MIN_DEPTH,
		in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&minDepth_param));


	AEFX_CLR_STRUCT(maxDepth_param);

	ERR(PF_CHECKOUT_PARAM(in_data,
		DepthWaves_MAX_DEPTH,
		in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&maxDepth_param));

	AEFX_CLR_STRUCT(minBlockSize_param);

	ERR(PF_CHECKOUT_PARAM(in_data,
		DepthWaves_MIN_BLOCK_SIZE,
		in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&minBlockSize_param));

	if (!err){
		minDepth = minDepth_param.u.fs_d.value;
		maxDepth = maxDepth_param.u.fs_d.value;
		minBlockSize = minBlockSize_param.u.fs_d.value;
	}

	ERR((extra->cb->checkout_layer_pixels(in_data->effect_ref, DepthWaves_INPUT, &input_worldP)));

	ERR((extra->cb->checkout_layer_pixels(in_data->effect_ref, DepthWaves_DEPTHMAP_LAYER, &depth_worldP)));

	ERR(extra->cb->checkout_output(in_data->effect_ref, &output_worldP));

	ERR(AEFX_AcquireSuite(in_data,
		out_data,
		kPFWorldSuite,
		kPFWorldSuiteVersion2,
		"Couldn't load suite.",
		(void**)&wsP));

	ERR(suites.PFInterfaceSuite1()->AEGP_ConvertEffectToCompTime(in_data->effect_ref,
		in_data->current_time,
		in_data->time_scale,
		&comp_timeT));

	ERR(suites.PFInterfaceSuite1()->AEGP_GetEffectCameraMatrix(in_data->effect_ref,
		&comp_timeT,
		&camera_matrix4,
		&focal_lengthF,
		&image_plane_widthL,
		&image_plane_heightL));

	if (!err){
		try
		{
			// always restore back AE's own OGL context
			SaveRestoreOGLContext oSavedContext;

			// our render specific context (one per thread)
			AESDK_OpenGL::AESDK_OpenGL_EffectRenderDataPtr renderContext = GetCurrentRenderContext();

			if (!renderContext->mInitialized) {
				//Now comes the OpenGL part - OS specific loading to start with
				AESDK_OpenGL_Startup(*renderContext.get(), S_DepthWaves_EffectCommonData.get());

				renderContext->mInitialized = true;
			}

			renderContext->SetPluginContext();
			
			// - Gremedy OpenGL debugger
			// - Example of using a OpenGL extension
			bool hasGremedy = renderContext->mExtensions.find(gl::GLextension::GL_GREMEDY_frame_terminator) != renderContext->mExtensions.end();

			A_long widthL = input_worldP->width;
			A_long heightL = input_worldP->height;

			//loading OpenGL resources
			AESDK_OpenGL_InitResources(*renderContext.get(), widthL, heightL, S_ResourcePath);

			CHECK(wsP->PF_GetPixelFormat(input_worldP, &format));

			// upload the input world to a texture
			size_t pixSize;
			gl::GLenum glFmt;
			float multiplier16bit;
			gl::GLuint colorTexture = UploadTexture(0, suites, format, input_worldP, output_worldP, in_data, pixSize, glFmt, multiplier16bit);
			gl::GLuint depthTexture = UploadTexture(1, suites, format, depth_worldP, output_worldP, in_data, pixSize, glFmt, multiplier16bit);
			
			// Set up the frame-buffer object just like a window.
			AESDK_OpenGL_MakeReadyToRender(*renderContext.get(), renderContext->mOutputFrameTexture);
			ReportIfErrorFramebuffer(in_data, out_data);

			glViewport(0, 0, widthL, heightL);
			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			glClear(GL_COLOR_BUFFER_BIT);
			
			/*** Compute Particles ***/
			// Get Camera Transform

			vmath::Vector3 position(0.f, 0.f, 0.f);
			vmath::Vector3 rotation(0.f, 0.f, 0.f);
			vmath::Vector3 fieldOfView(2.f * atan2f(0.5f * float(image_plane_widthL), focal_lengthF), 2.f * atan2f(0.5f * float(image_plane_heightL), focal_lengthF), 1.f);
			CameraTransform cameraTransform(position, rotation, fieldOfView, focal_lengthF, minDepth, maxDepth);
			
			ComputeParticles(renderContext, colorTexture, widthL, heightL, depthTexture, widthL, heightL, minDepth, maxDepth, cameraTransform, multiplier16bit);

			Vertex *data = new Vertex[widthL * heightL];
			glGetNamedBufferSubData(renderContext->vertBuffer, 0, widthL * heightL * sizeof(Vertex), data);

			RenderGL(renderContext, renderContext->mOutputFrameTexture, widthL, heightL, minBlockSize, cameraTransform.projectionMatrix, multiplier16bit);

			delete[] data;

			// - we toggle PBO textures (we use the PBO we just created as an input)
			// AESDK_OpenGL_MakeReadyToRender(*renderContext.get(), colorTexture);
			// ReportIfErrorFramebuffer(in_data, out_data);

			if (hasGremedy) {
				gl::glFrameTerminatorGREMEDY();
			}

			// - get back to CPU the result, and inside the output world
			DownloadTexture(renderContext, suites, input_worldP, output_worldP, in_data, format, pixSize, glFmt);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glBindTexture(GL_TEXTURE_2D, 0);
			glDeleteTextures(1, &colorTexture);
			glDeleteTextures(1, &depthTexture);
		}
		catch (PF_Err& thrown_err)
		{
			err = thrown_err;
		}
		catch (...)
		{
			err = PF_Err_OUT_OF_MEMORY;
		}
	}

	// If you have PF_ABORT or PF_PROG higher up, you must set
	// the AE context back before calling them, and then take it back again
	// if you want to call some more OpenGL.		
	ERR(PF_ABORT(in_data));

	ERR2(AEFX_ReleaseSuite(in_data,
		out_data,
		kPFWorldSuite,
		kPFWorldSuiteVersion2,
		"Couldn't release suite."));

	ERR2(PF_CHECKIN_PARAM(in_data, &minDepth_param));
	ERR2(PF_CHECKIN_PARAM(in_data, &maxDepth_param));
	ERR2(PF_CHECKIN_PARAM(in_data, &minBlockSize_param));
	
	ERR2(extra->cb->checkin_layer_pixels(in_data->effect_ref, DepthWaves_INPUT));
	ERR2(extra->cb->checkin_layer_pixels(in_data->effect_ref, DepthWaves_DEPTHMAP_LAYER));

	return err;
}


extern "C" DllExport
PF_Err PluginDataEntryFunction(
	PF_PluginDataPtr inPtr,
	PF_PluginDataCB inPluginDataCallBackPtr,
	SPBasicSuite* inSPBasicSuitePtr,
	const char* inHostName,
	const char* inHostVersion)
{
	PF_Err result = PF_Err_INVALID_CALLBACK;

	result = PF_REGISTER_EFFECT(
		inPtr,
		inPluginDataCallBackPtr,
		"DepthWaves", // Name
		"ADBE DepthWaves", // Match Name
		"jperl", // Category
		AE_RESERVED_INFO); // Reserved Info

	return result;
}


PF_Err
EffectMain(
	PF_Cmd			cmd,
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	void			*extra)
{

	/*******************
    Important Notice 06/02/2020
    OpenGL was deprecated on macOS 10.14 with Apple asking all developers to move to using the Metal framework.
    As such we are recommending cross-platform plugins are no longer developed directly against OpenGL.
    For GPU-based plugin development, please refer to the SDK_Invert_ProcAmp SDK sample on how to use GPU rendering with the plugin SDK.
    This plugin is kept only for reference.
    *******************/


	PF_Err		err = PF_Err_NONE;
	
	try {
		switch (cmd) {
			case PF_Cmd_ABOUT:
				err = About(in_data,
							out_data,
							params,
							output);
				break;
				
			case PF_Cmd_GLOBAL_SETUP:
				err = GlobalSetup(	in_data,
									out_data,
									params,
									output);
				break;
				
			case PF_Cmd_PARAMS_SETUP:
				err = ParamsSetup(	in_data,
									out_data,
									params,
									output);
				break;
				
			case PF_Cmd_GLOBAL_SETDOWN:
				err = GlobalSetdown(	in_data,
										out_data,
										params,
										output);
				break;

			case  PF_Cmd_SMART_PRE_RENDER:
				err = PreRender(in_data, out_data, reinterpret_cast<PF_PreRenderExtra*>(extra));
				break;

			case  PF_Cmd_SMART_RENDER:
				err = SmartRender(in_data, out_data, reinterpret_cast<PF_SmartRenderExtra*>(extra));
				break;
		}
	}
	catch(PF_Err &thrown_err){
		err = thrown_err;
	}
	return err;
}
