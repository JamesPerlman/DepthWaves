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

#include "GL_base.h"
#include "Smart_Utils.h"
#include "AEFX_SuiteHelper.h"

#include <thread>
#include <atomic>
#include <map>
#include <mutex>
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
		glEnableVertexAttribArray(SizeSlot);
		glBindBuffer(GL_ARRAY_BUFFER, vertBuffer);
		glVertexAttribPointer(PositionSlot, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), 0);
		glVertexAttribPointer(ColorSlot, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(4 * sizeof(gl::GLfloat)));
		glVertexAttribPointer(SizeSlot, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(8 * sizeof(gl::GLfloat)));
		glDrawArrays(GL_POINTS, 0, numVerts);
		glDisableVertexAttribArray(PositionSlot);
		glDisableVertexAttribArray(ColorSlot);
		glDisableVertexAttribArray(SizeSlot);
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

		multiplier16bitOut = 1.0f;
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

	PF_Err GetCameraTransformations(
		PF_InData *in_data,
		A_long layerTime,
		vmath::Vector3 *cameraPosition,
		vmath::Vector3 *cameraOrientation,
		vmath::Vector3 *cameraRotation
	) {

		PF_Err err = PF_Err_NONE;

		A_Time				compCurrentTime;
		AEGP_LayerH			cameraLayer;
		AEGP_StreamVal2		cameraPositionStreamValue, cameraOrientationStreamValue,
			cameraRotationXStreamValue, cameraRotationYStreamValue, cameraRotationZStreamValue;

		AEGP_SuiteHandler	suites(in_data->pica_basicP);

		ERR(suites.PFInterfaceSuite1()->AEGP_ConvertEffectToCompTime(
			in_data->effect_ref,
			layerTime,
			in_data->time_scale,
			&compCurrentTime));

		ERR(suites.PFInterfaceSuite1()->AEGP_GetEffectCamera(
			in_data->effect_ref,
			&compCurrentTime,
			&cameraLayer));

		ERR(suites.StreamSuite5()->AEGP_GetLayerStreamValue(
			cameraLayer,
			AEGP_LayerStream_POSITION,
			AEGP_LTimeMode_CompTime,
			&compCurrentTime,
			false,
			&cameraPositionStreamValue,
			NULL));

		ERR(suites.StreamSuite5()->AEGP_GetLayerStreamValue(
			cameraLayer,
			AEGP_LayerStream_ROTATE_X,
			AEGP_LTimeMode_CompTime,
			&compCurrentTime,
			false,
			&cameraRotationXStreamValue,
			NULL));

		ERR(suites.StreamSuite5()->AEGP_GetLayerStreamValue(
			cameraLayer,
			AEGP_LayerStream_ROTATE_Y,
			AEGP_LTimeMode_CompTime,
			&compCurrentTime,
			false,
			&cameraRotationYStreamValue,
			NULL));

		ERR(suites.StreamSuite5()->AEGP_GetLayerStreamValue(
			cameraLayer,
			AEGP_LayerStream_ROTATE_Z,
			AEGP_LTimeMode_CompTime,
			&compCurrentTime,
			false,
			&cameraRotationZStreamValue,
			NULL));

		ERR(suites.StreamSuite5()->AEGP_GetLayerStreamValue(
			cameraLayer,
			AEGP_LayerStream_ORIENTATION,
			AEGP_LTimeMode_CompTime,
			&compCurrentTime,
			false,
			&cameraOrientationStreamValue,
			NULL));

		*cameraPosition = vmath::Vector3(
			(float)cameraPositionStreamValue.three_d.x,
			(float)cameraPositionStreamValue.three_d.y,
			(float)cameraPositionStreamValue.three_d.z
		);

		*cameraOrientation = vmath::Vector3(
			(float)(cameraOrientationStreamValue.three_d.x * PF_RAD_PER_DEGREE),
			(float)(cameraOrientationStreamValue.three_d.y * PF_RAD_PER_DEGREE),
			(float)(cameraOrientationStreamValue.three_d.z * PF_RAD_PER_DEGREE)
		);

		*cameraRotation = vmath::Vector3(
			(float)(cameraRotationXStreamValue.one_d * PF_RAD_PER_DEGREE),
			(float)(cameraRotationYStreamValue.one_d * PF_RAD_PER_DEGREE),
			(float)(cameraRotationZStreamValue.one_d * PF_RAD_PER_DEGREE)
		);

		return err;
	}

	PF_Err GetSceneInfo(
		PF_InData *in_data,
		PF_FpLong maxDepth,
		CameraTransform *cameraTransform,
		vmath::Matrix4 *waveTransformMatrix
	) {

		PF_Err err = PF_Err_NONE;

		A_Matrix4			camera_matrix4;
		A_short				image_plane_widthL = 0, image_plane_heightL = 0;
		A_FpLong			focal_lengthF = 0;
		A_Time				comp_timeT = { 0, 1 };

		AEGP_SuiteHandler	suites(in_data->pica_basicP);

		vmath::Vector3		cameraPosition;
		vmath::Vector3		cameraRotation;
		vmath::Vector3		cameraOrientation;

		ERR(suites.PFInterfaceSuite1()->AEGP_GetEffectCameraMatrix(
			in_data->effect_ref,
			&comp_timeT,
			&camera_matrix4,
			&focal_lengthF,
			&image_plane_widthL,
			&image_plane_heightL
		));

		ERR(GetCameraTransformations(
			in_data,
			in_data->current_time,
			&cameraPosition,
			&cameraOrientation,
			&cameraRotation
		));

		vmath::Vector3 fieldOfView(
			2.f * atan2f(0.5f * (float)image_plane_widthL, (float)focal_lengthF),
			2.f * atan2f(0.5f * (float)image_plane_heightL, (float)focal_lengthF),
			1.f
		);

		*waveTransformMatrix = vmath::Matrix4::rotationZYX(-cameraRotation) * vmath::Matrix4::rotationZYX(-cameraOrientation) * vmath::Matrix4::translation(-cameraPosition);
		*cameraTransform = CameraTransform(
			cameraPosition,
			cameraRotation,
			fieldOfView,
			(float)focal_lengthF,
			1.f,
			(float)maxDepth
		);

		return err;
	}

	PF_Err GetWaves(
		PF_InData *in_data,
		vmath::Matrix4 waveTransformMatrix,
		std::vector<Wave> &waves
	) {
		PF_KeyIndex			emitterNumKeyframes = 0;
		A_long				keyTime = 0;
		A_u_long			keyTimeScale = 0;
		AEGP_SuiteHandler	suites(in_data->pica_basicP);

		std::vector<Impulse> impulses;

		A_u_long timeScale = in_data->time_scale;
		A_long timeStep = in_data->time_step;

		PF_Err err = PF_Err_NONE;
		bool wasEmitting = false;

		ERR(suites.ParamUtilsSuite3()->PF_GetKeyframeCount(
			in_data->effect_ref,
			DepthWaves_EMITTER_IMPULSE,
			&emitterNumKeyframes
		));

		for (int i = 0; i < emitterNumKeyframes; ++i) {

			PF_ParamDef emitterImpulse_param;

			AEFX_CLR_STRUCT(emitterImpulse_param);

			ERR(suites.ParamUtilsSuite3()->PF_CheckoutKeyframe(
				in_data->effect_ref,
				DepthWaves_EMITTER_IMPULSE,
				i,
				&keyTime,
				&keyTimeScale,
				&emitterImpulse_param
			));

			//keyframeCheckouts.push_back(emitterImpulse_param);

			bool isEmitting = emitterImpulse_param.u.bd.value;

			if (isEmitting != wasEmitting) {
				if (isEmitting == true) {
					// emitter goes from off -> on: add a new impulse
					impulses.emplace_back(keyTime, 0);
				}
				else {
					// emitter goes from on -> off: end latest impulse
					impulses.back().endTime = keyTime;
				}
			}

			// make sure last impulse ends at last keyTime
			if (impulses.size() > 0) {
				impulses.back().endTime = keyTime;
			}

			ERR(suites.ParamUtilsSuite3()->PF_CheckinKeyframe(
				in_data->effect_ref,
				&emitterImpulse_param
			));
		}

		// generate waves from impulses
		for (Impulse &impulse : impulses) {

			// Skip zero-duration impulses
			if (impulse.startTime == impulse.endTime) {
				continue;
			}

			PF_FpLong now = (PF_FpLong)in_data->current_time / (PF_FpLong)timeScale;
			PF_FpLong impulseStart = (PF_FpLong)impulse.startTime / (PF_FpLong)timeScale;
			PF_FpLong impulseEnd = (PF_FpLong)impulse.endTime / (PF_FpLong)timeScale;

			PF_FpLong timeFromStart = now - impulseStart;
			PF_FpLong timeFromEnd = now - impulseEnd;

			if (timeFromStart > 0.0) {

				PF_ParamDef emitterPosition_param,
					waveBlockSizeMultiplier_param,
					waveDisplacement_param,
					waveDisplacementDirection_param,
					waveColor_param,
					waveColorMix_param,
					waveVelocity_param,
					waveDecay_param;

				PF_FpLong waveDisplacement,
					waveVelocity,
					waveDecay,
					waveColorMix;

				vmath::Vector3 waveEmitterPosition,
					waveDisplacementDirection;

				AEFX_CLR_STRUCT(emitterPosition_param);
				ERR(PF_CHECKOUT_PARAM(in_data,
					DepthWaves_EMITTER_POSITION,
					impulse.startTime,
					timeStep,
					timeScale,
					&emitterPosition_param));

				AEFX_CLR_STRUCT(waveDisplacement_param);
				ERR(PF_CHECKOUT_PARAM(in_data,
					DepthWaves_WAVE_DISPLACEMENT,
					impulse.startTime,
					timeStep,
					timeScale,
					&waveDisplacement_param));

				AEFX_CLR_STRUCT(waveBlockSizeMultiplier_param);
				ERR(PF_CHECKOUT_PARAM(in_data,
					DepthWaves_WAVE_BLOCK_SIZE_MULTIPLIER,
					impulse.startTime,
					timeStep,
					timeScale,
					&waveBlockSizeMultiplier_param));

				AEFX_CLR_STRUCT(waveDisplacementDirection_param);
				ERR(PF_CHECKOUT_PARAM(in_data,
					DepthWaves_WAVE_DISPLACEMENT_DIRECTION,
					impulse.startTime,
					timeStep,
					timeScale,
					&waveDisplacementDirection_param));

				AEFX_CLR_STRUCT(waveColor_param);
				ERR(PF_CHECKOUT_PARAM(in_data,
					DepthWaves_WAVE_COLOR,
					impulse.startTime,
					timeStep,
					timeScale,
					&waveColor_param));

				AEFX_CLR_STRUCT(waveColorMix_param);
				ERR(PF_CHECKOUT_PARAM(in_data,
					DepthWaves_WAVE_COLOR_MIX,
					impulse.startTime,
					timeStep,
					timeScale,
					&waveColorMix_param));

				AEFX_CLR_STRUCT(waveVelocity_param);
				ERR(PF_CHECKOUT_PARAM(in_data,
					DepthWaves_WAVE_VELOCITY,
					impulse.startTime,
					timeStep,
					timeScale,
					&waveVelocity_param));

				AEFX_CLR_STRUCT(waveDecay_param);
				ERR(PF_CHECKOUT_PARAM(in_data,
					DepthWaves_WAVE_DECAY,
					impulse.startTime,
					timeStep,
					timeScale,
					&waveDecay_param));

				waveVelocity = waveVelocity_param.u.fs_d.value;
				waveDecay = waveDecay_param.u.fs_d.value;

				PF_FpLong amplitude = pow(waveDecay, timeFromEnd);

				waveDisplacement = amplitude * waveDisplacement_param.u.fs_d.value;
				waveColorMix = amplitude * waveColorMix_param.u.fs_d.value;

				PF_FpLong outerRadius = waveVelocity * timeFromStart;
				PF_FpLong innerRadius = timeFromEnd <= 0.0 ? 0.0 : waveVelocity * timeFromEnd;
				PF_FpLong waveAmplitude = pow(waveDecay, (timeFromStart + timeFromEnd) * 0.5);
				PF_FpLong waveBlockSizeMultiplier = MIX(1.0, waveBlockSizeMultiplier_param.u.fs_d.value, waveAmplitude);

				// Sometimes point3ds come in at half their expected value.  These downsample values seem to correlate when that does happen.
				float sx = (float)in_data->downsample_x.den / (float)in_data->downsample_x.num;
				float sy = (float)in_data->downsample_y.den / (float)in_data->downsample_y.num;
				float sz = sy;

				waveEmitterPosition = vmath::Vector3(
					(float)emitterPosition_param.u.point3d_d.x_value * sx,
					(float)emitterPosition_param.u.point3d_d.y_value * sy,
					(float)emitterPosition_param.u.point3d_d.z_value * sz
				);

				waveDisplacementDirection = vmath::Vector3(
					(float)waveDisplacementDirection_param.u.point3d_d.x_value * sx,
					(float)waveDisplacementDirection_param.u.point3d_d.y_value * sy,
					(float)waveDisplacementDirection_param.u.point3d_d.z_value * sz
				);

				vmath::Vector4 transformedPosition = waveTransformMatrix * vmath::Vector4(waveEmitterPosition, 1.f);
				vmath::Vector4 transformedDisplacementDirection = waveTransformMatrix * vmath::Vector4(waveDisplacementDirection, 1.f) - waveTransformMatrix * vmath::Vector4(0.f, 0.f, 0.f, 1.f);

				// Negated components to transform to openGL coordinate space
				gl::GLfloat wavePosition[4] = {
					(gl::GLfloat)transformedPosition.getX(),
					(gl::GLfloat)transformedPosition.getY(),
					-(gl::GLfloat)transformedPosition.getZ(),
					1.f
				};

				gl::GLfloat waveDisplacementVector[4] = {
					(gl::GLfloat)transformedDisplacementDirection.getX(),
					(gl::GLfloat)transformedDisplacementDirection.getY(),
					(gl::GLfloat)transformedDisplacementDirection.getZ(),
					(gl::GLfloat)waveDisplacement
				};

				gl::GLfloat waveColor[4] = {
					(float)waveColor_param.u.cd.value.red / 255.f,
					(float)waveColor_param.u.cd.value.green / 255.f,
					(float)waveColor_param.u.cd.value.blue / 255.f,
					(float)waveColor_param.u.cd.value.alpha / 255.f
				};

				Wave wave(
					wavePosition,
					waveDisplacementVector,
					waveColor,
					(gl::GLfloat)waveBlockSizeMultiplier,
					(gl::GLfloat)waveColorMix,
					(gl::GLfloat)outerRadius,
					(gl::GLfloat)innerRadius,
					(gl::GLfloat)timeFromStart
				);

				waves.push_back(wave);

				ERR(PF_CHECKIN_PARAM(in_data, &emitterPosition_param));
				ERR(PF_CHECKIN_PARAM(in_data, &waveDisplacement_param));
				ERR(PF_CHECKIN_PARAM(in_data, &waveBlockSizeMultiplier_param));
				ERR(PF_CHECKIN_PARAM(in_data, &waveDisplacementDirection_param));
				ERR(PF_CHECKIN_PARAM(in_data, &waveColor_param));
				ERR(PF_CHECKIN_PARAM(in_data, &waveColorMix_param));
				ERR(PF_CHECKIN_PARAM(in_data, &waveVelocity_param));
				ERR(PF_CHECKIN_PARAM(in_data, &waveDecay_param));
			}
		}
		return err;
	}

	void ComputeParticles(
		const AESDK_OpenGL::AESDK_OpenGL_EffectRenderDataPtr& renderContext,
		gl::GLuint colorLayerTexture,
		gl::GLuint depthLayerTexture,
		DepthWavesInfo *info
	) {
		GLuint program = renderContext->computeShaderProgram;
		glUseProgram(program);

		glBindImageTexture(0, colorLayerTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
		glBindImageTexture(1, depthLayerTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);

		GLuint u;
		u = glGetUniformLocation(program, "minDepth");
		glUniform1f(u, (gl::GLfloat)info->minDepth);

		u = glGetUniformLocation(program, "maxDepth");
		glUniform1f(u, (gl::GLfloat)info->maxDepth);

		u = glGetUniformLocation(program, "nearBlockSize");
		glUniform1f(u, (gl::GLfloat)info->nearBlockSize);

		u = glGetUniformLocation(program, "farBlockSize");
		glUniform1f(u, (gl::GLfloat)info->farBlockSize);

		u = glGetUniformLocation(program, "cameraFov");
		glUniform2fv(u, 1, (gl::GLfloat*)&info->cameraTransform.fov);

		u = glGetUniformLocation(program, "waveCount");
		glUniform1i(u, info->numWaves);

		u = glGetUniformLocation(program, "colorizeWaves");
		glUniform1i(u, (gl::GLint)info->colorizeWaves);

		u = glGetUniformLocation(program, "colorCycleRadius");
		glUniform1f(u, (gl::GLfloat)info->colorCycleRadius);

		glDispatchCompute(info->numBlocksX, info->numBlocksY, 1);
		
		glUseProgram(0);
	}

	void RenderGL(const AESDK_OpenGL::AESDK_OpenGL_EffectRenderDataPtr& renderContext,
				  gl::GLuint inputFrameTexture,
				  A_long widthL,
				  A_long heightL,
				  DepthWavesInfo *info,
				  float multiplier16bit)
	{

		gl::GLuint program = renderContext->visualShaderProgram;
		GLuint u;

		glEnable(GL_DEPTH_TEST);

		// send uniforms to shader
		glUseProgram(program);

		u = glGetUniformLocation(program, "modelViewProjectionMatrix");
		glUniformMatrix4fv(u, 1, GL_TRUE, (gl::GLfloat*)&info->cameraTransform.projectionMatrix);

		u = glGetUniformLocation(program, "nearBlockSize");
		glUniform1f(u, (gl::GLfloat)info->nearBlockSize);

		u = glGetUniformLocation(program, "farBlockSize");
		glUniform1f(u, (gl::GLfloat)info->farBlockSize);

		u = glGetUniformLocation(program, "multiplier16bit");
		glUniform1f(u, multiplier16bit);

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
	
	out_data->out_flags2 = PF_OutFlag2_FLOAT_COLOR_AWARE
						| PF_OutFlag2_SUPPORTS_SMART_RENDER
						| PF_OutFlag2_I_MIX_GUID_DEPENDENCIES;
	
	PF_Err err = PF_Err_NONE;
	try
	{
		// always restore back AE's own OGL context
		SaveRestoreOGLContext oSavedContext;
		AEGP_SuiteHandler suites(in_data->pica_basicP);

		AEFX_SuiteScoper<PF_HandleSuite1> handleSuite = AEFX_SuiteScoper<PF_HandleSuite1>(
			in_data,
			kPFHandleSuite,
			kPFHandleSuiteVersion1,
			out_data
		);
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

	// Emitter Impulse
	PF_ADD_CHECKBOXX(
		STR(StrID_Emitter_Impulse_Switch_Name),
		DepthWaves_EMITTER_IMPULSE_DEFAULT,
		PF_ParamFlag_RESERVED1,
		EMITTER_IMPULSE_DISK_ID
	);

	AEFX_CLR_STRUCT(def);

	// Emitter Position
	PF_ADD_POINT_3D(
		STR(StrID_Emitter_Position_Point_Name),
		0,
		0,
		0,
		EMITTER_POSITION_DISK_ID
	);

	AEFX_CLR_STRUCT(def);

	// Min Depth
	PF_ADD_FLOAT_SLIDERX(
		STR(StrID_Min_Depth_Slider_Name),
		DepthWaves_DEPTH_SLIDER_MIN,
		DepthWaves_DEPTH_SLIDER_MAX,
		DepthWaves_DEPTH_SLIDER_MIN,
		DepthWaves_DEPTH_SLIDER_MAX,
		DepthWaves_MIN_DEPTH_DEFAULT,
		PF_Precision_THOUSANDTHS,
		PF_ValueDisplayFlag_NONE,
		PF_ParamFlag_RESERVED1,
		MIN_DEPTH_DISK_ID
	);

	AEFX_CLR_STRUCT(def);

	// Max Depth
	PF_ADD_FLOAT_SLIDERX(
		STR(StrID_Max_Depth_Slider_Name),
		DepthWaves_DEPTH_SLIDER_MIN,
		DepthWaves_DEPTH_SLIDER_MAX,
		DepthWaves_DEPTH_SLIDER_MIN,
		DepthWaves_DEPTH_SLIDER_MAX,
		DepthWaves_MIN_DEPTH_DEFAULT,
		PF_Precision_THOUSANDTHS,
		PF_ValueDisplayFlag_NONE,
		PF_ParamFlag_RESERVED1,
		MAX_DEPTH_DISK_ID
	);

	AEFX_CLR_STRUCT(def);

	// Near Block Size
	PF_ADD_FLOAT_SLIDERX(
		STR(StrID_Near_Block_Size_Slider_Name),
		DepthWaves_BLOCK_SIZE_SLIDER_MIN,
		DepthWaves_BLOCK_SIZE_SLIDER_MAX,
		DepthWaves_BLOCK_SIZE_SLIDER_MIN,
		DepthWaves_BLOCK_SIZE_SLIDER_MAX,
		DepthWaves_MIN_BLOCK_SIZE_DEFAULT,
		PF_Precision_THOUSANDTHS,
		PF_ValueDisplayFlag_NONE,
		PF_ParamFlag_RESERVED1,
		NEAR_BLOCK_SIZE_DISK_ID
	);

	AEFX_CLR_STRUCT(def);

	// Far Block Size
	PF_ADD_FLOAT_SLIDERX(
		STR(StrID_Far_Block_Size_Slider_Name),
		DepthWaves_BLOCK_SIZE_SLIDER_MIN,
		DepthWaves_BLOCK_SIZE_SLIDER_MAX,
		DepthWaves_BLOCK_SIZE_SLIDER_MIN,
		DepthWaves_BLOCK_SIZE_SLIDER_MAX,
		DepthWaves_MAX_BLOCK_SIZE_DEFAULT,
		PF_Precision_THOUSANDTHS,
		PF_ValueDisplayFlag_NONE,
		PF_ParamFlag_RESERVED1,
		NEAR_BLOCK_SIZE_DISK_ID
	);

	AEFX_CLR_STRUCT(def);

	// Wave Block Size Multiplier
	PF_ADD_FLOAT_SLIDERX(
		STR(StrID_Wave_Block_Size_Multiplier_Slider_Name),
		DepthWaves_WAVE_DISPLACEMENT_SLIDER_MIN,
		DepthWaves_WAVE_DISPLACEMENT_SLIDER_MAX,
		DepthWaves_WAVE_DISPLACEMENT_SLIDER_MIN,
		DepthWaves_WAVE_DISPLACEMENT_SLIDER_MAX,
		DepthWaves_WAVE_DISPLACEMENT_DEFAULT,
		PF_Precision_TENTHS,
		PF_ValueDisplayFlag_NONE,
		PF_ParamFlag_RESERVED1,
		WAVE_BLOCK_SIZE_MULTIPLIER_DISK_ID
	);

	AEFX_CLR_STRUCT(def);

	// Wave Displacement
	PF_ADD_FLOAT_SLIDERX(
		STR(StrID_Wave_Displacement_Slider_Name),
		DepthWaves_WAVE_DISPLACEMENT_SLIDER_MIN,
		DepthWaves_WAVE_DISPLACEMENT_SLIDER_MAX,
		DepthWaves_WAVE_DISPLACEMENT_SLIDER_MIN,
		DepthWaves_WAVE_DISPLACEMENT_SLIDER_MAX,
		DepthWaves_WAVE_DISPLACEMENT_DEFAULT,
		PF_Precision_TENTHS,
		PF_ValueDisplayFlag_NONE,
		PF_ParamFlag_RESERVED1,
		WAVE_DISPLACEMENT_DISK_ID
	);

	AEFX_CLR_STRUCT(def);

	// Displacement Direction
	PF_ADD_POINT_3D(
		STR(StrID_Wave_Displacement_Direction_Name),
		0,
		0,
		0,
		WAVE_DISPLACEMENT_DIRECTION_DISK_ID
	);

	AEFX_CLR_STRUCT(def);

	// Wave Color
	PF_ADD_COLOR(
		STR(StrID_Wave_Color_Name),
		0,
		0,
		0,
		WAVE_COLOR_DISK_ID
	);

	AEFX_CLR_STRUCT(def);

	// Wave Color Mix
	PF_ADD_FLOAT_SLIDERX(
		STR(StrID_Wave_Color_Mix_Slider_Name),
		DepthWaves_WAVE_COLOR_MIX_SLIDER_MIN,
		DepthWaves_WAVE_COLOR_MIX_SLIDER_MAX,
		DepthWaves_WAVE_COLOR_MIX_SLIDER_MIN,
		DepthWaves_WAVE_COLOR_MIX_SLIDER_MAX,
		DepthWaves_WAVE_COLOR_MIX_DEFAULT,
		PF_Precision_TENTHS,
		PF_ValueDisplayFlag_NONE,
		PF_ParamFlag_RESERVED1,
		WAVE_VELOCITY_DISK_ID
	);

	// Wave Velocity
	PF_ADD_FLOAT_SLIDERX(
		STR(StrID_Wave_Velocity_Slider_Name),
		DepthWaves_WAVE_VELOCITY_SLIDER_MIN,
		DepthWaves_WAVE_VELOCITY_SLIDER_MAX,
		DepthWaves_WAVE_VELOCITY_SLIDER_MIN,
		DepthWaves_WAVE_VELOCITY_SLIDER_MAX,
		DepthWaves_WAVE_VELOCITY_DEFAULT,
		PF_Precision_TENTHS,
		PF_ValueDisplayFlag_NONE,
		PF_ParamFlag_RESERVED1,
		WAVE_VELOCITY_DISK_ID
	);

	AEFX_CLR_STRUCT(def);

	// Wave Decay
	PF_ADD_FLOAT_SLIDERX(
		STR(StrID_Wave_Decay_Slider_Name),
		DepthWaves_WAVE_DECAY_SLIDER_MIN,
		DepthWaves_WAVE_DECAY_SLIDER_MAX,
		DepthWaves_WAVE_DECAY_SLIDER_MIN,
		DepthWaves_WAVE_DECAY_SLIDER_MAX,
		DepthWaves_WAVE_DECAY_DEFAULT,
		PF_Precision_THOUSANDTHS,
		PF_ValueDisplayFlag_NONE,
		PF_ParamFlag_RESERVED1,
		WAVE_DECAY_DISK_ID
	);

	AEFX_CLR_STRUCT(def);

	// Colorize Waves Checkbox
	PF_ADD_CHECKBOXX(
		STR(StrID_Colorize_Waves_Checkbox_Name),
		DepthWaves_EMITTER_IMPULSE_DEFAULT,
		PF_ParamFlag_RESERVED1,
		COLORIZE_WAVES_DISK_ID
	);

	AEFX_CLR_STRUCT(def);

	// Colorize Waves Cycle Radius
	PF_ADD_FLOAT_SLIDERX(
		STR(StrID_Colorize_Waves_Cycle_Radius_Slider_Name),
		DepthWaves_COLORIZE_WAVES_CYCLE_RADIUS_SLIDER_MIN,
		DepthWaves_COLORIZE_WAVES_CYCLE_RADIUS_SLIDER_MAX,
		DepthWaves_COLORIZE_WAVES_CYCLE_RADIUS_SLIDER_MIN,
		DepthWaves_COLORIZE_WAVES_CYCLE_RADIUS_SLIDER_MAX,
		DepthWaves_COLORIZE_WAVES_CYCLE_RADIUS_DEFAULT,
		PF_Precision_TENTHS,
		PF_ValueDisplayFlag_NONE,
		PF_ParamFlag_RESERVED1,
		COLORIZE_WAVES_CYCLE_RADIUS_DISK_ID
	);

	AEFX_CLR_STRUCT(def);

	// Num Blocks X
	PF_ADD_FLOAT_SLIDERX(
		STR(StrID_Num_Blocks_X_Name),
		DepthWaves_NUM_BLOCKS_SLIDER_MIN,
		DepthWaves_NUM_BLOCKS_SLIDER_MAX,
		DepthWaves_NUM_BLOCKS_SLIDER_MIN,
		DepthWaves_NUM_BLOCKS_SLIDER_MAX,
		DepthWaves_NUM_BLOCKS_DEFAULT,
		PF_Precision_INTEGER,
		PF_ValueDisplayFlag_NONE,
		PF_ParamFlag_RESERVED1,
		NUM_BLOCKS_X_DISK_ID
	);

	AEFX_CLR_STRUCT(def);

	// Num Blocks Y
	PF_ADD_FLOAT_SLIDERX(
		STR(StrID_Num_Blocks_Y_Name),
		DepthWaves_NUM_BLOCKS_SLIDER_MIN,
		DepthWaves_NUM_BLOCKS_SLIDER_MAX,
		DepthWaves_NUM_BLOCKS_SLIDER_MIN,
		DepthWaves_NUM_BLOCKS_SLIDER_MAX,
		DepthWaves_NUM_BLOCKS_DEFAULT,
		PF_Precision_INTEGER,
		PF_ValueDisplayFlag_NONE,
		PF_ParamFlag_RESERVED1,
		NUM_BLOCKS_Y_DISK_ID
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

static void
DisposePreRenderData(
	void *pre_render_dataPV)
{
	if (pre_render_dataPV) {
		DepthWavesInfo *info = reinterpret_cast<DepthWavesInfo *>(pre_render_dataPV);
		free(info->waves);
		free(info);
	}
}

static PF_Err
PreRender(
	PF_InData				*in_data,
	PF_OutData				*out_data,
	PF_PreRenderExtra		*extra)
{
	PF_Err	err = PF_Err_NONE,
			err2 = PF_Err_NONE;
	
	PF_CheckoutResult in_result;
	PF_CheckoutResult depthMapLayer_result;

	AEGP_SuiteHandler	suites(in_data->pica_basicP);

	PF_RenderRequest req = extra->input->output_request;

	PF_ParamDef minDepth_param,
		maxDepth_param,
		nearBlockSize_param,
		farBlockSize_param,
		numBlocksX_param,
		numBlocksY_param,
		colorizeWaves_param,
		colorizeWavesCycleRadius_param;

	PF_FpLong nearBlockSize, farBlockSize,
		minDepth, maxDepth,
		colorizeWavesCycleRadius;

	A_Boolean colorizeWaves;

	A_long numBlocksX, numBlocksY;

	std::vector<Wave>waves;

	vmath::Matrix4 waveTransformMatrix;
	CameraTransform cameraTransform;

	ERR(extra->cb->checkout_layer(
		in_data->effect_ref,
		DepthWaves_INPUT,
		DepthWaves_INPUT,
		&req,
		in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&in_result));

	ERR(extra->cb->checkout_layer(
		in_data->effect_ref,
		DepthWaves_DEPTHMAP_LAYER,
		DepthWaves_DEPTHMAP_LAYER,
		&req,
		in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&depthMapLayer_result));

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

	AEFX_CLR_STRUCT(nearBlockSize_param);

	ERR(PF_CHECKOUT_PARAM(in_data,
		DepthWaves_NEAR_BLOCK_SIZE,
		in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&nearBlockSize_param));

	AEFX_CLR_STRUCT(farBlockSize_param);

	ERR(PF_CHECKOUT_PARAM(in_data,
		DepthWaves_FAR_BLOCK_SIZE,
		in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&farBlockSize_param));

	AEFX_CLR_STRUCT(numBlocksX_param);

	ERR(PF_CHECKOUT_PARAM(in_data,
		DepthWaves_NUM_BLOCKS_X,
		in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&numBlocksX_param));

	AEFX_CLR_STRUCT(numBlocksY_param);

	ERR(PF_CHECKOUT_PARAM(in_data,
		DepthWaves_NUM_BLOCKS_Y,
		in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&numBlocksY_param));

	AEFX_CLR_STRUCT(colorizeWaves_param);

	ERR(PF_CHECKOUT_PARAM(in_data,
		DepthWaves_COLORIZE_WAVES,
		in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&colorizeWaves_param));

	AEFX_CLR_STRUCT(colorizeWavesCycleRadius_param);

	ERR(PF_CHECKOUT_PARAM(in_data,
		DepthWaves_COLORIZE_WAVES_CYCLE_RADIUS,
		in_data->current_time,
		in_data->time_step,
		in_data->time_scale,
		&colorizeWavesCycleRadius_param));


	if (!err) {
		// other params
		DepthWavesInfo info;

		nearBlockSize = nearBlockSize_param.u.fs_d.value;
		farBlockSize = farBlockSize_param.u.fs_d.value;
		minDepth = minDepth_param.u.fs_d.value;
		maxDepth = maxDepth_param.u.fs_d.value;
		numBlocksX = (A_long)numBlocksX_param.u.fs_d.value;
		numBlocksY = (A_long)numBlocksY_param.u.fs_d.value;
		colorizeWaves = colorizeWaves_param.u.bd.value;
		colorizeWavesCycleRadius = colorizeWavesCycleRadius_param.u.fs_d.value;

		ERR(GetSceneInfo(
			in_data,
			maxDepth,
			&cameraTransform,
			&waveTransformMatrix
		));

		ERR(GetWaves(
			in_data,
			waveTransformMatrix,
			waves
		));
		
		DepthWavesInfo *infoP = reinterpret_cast<DepthWavesInfo*>(malloc(sizeof(DepthWavesInfo)));

		if (infoP) {
			infoP->minDepth = minDepth;
			infoP->maxDepth = maxDepth;
			infoP->nearBlockSize = nearBlockSize;
			infoP->farBlockSize = farBlockSize;
			infoP->numBlocksX = numBlocksX;
			infoP->numBlocksY = numBlocksY;
			infoP->cameraTransform = cameraTransform;
			infoP->numWaves = waves.size();
			infoP->colorizeWaves = colorizeWaves;
			infoP->colorCycleRadius = colorizeWavesCycleRadius;

			if (infoP->numWaves) {
				infoP->waves = (Wave*)malloc(waves.size() * sizeof(Wave));
				memcpy(infoP->waves, waves.data(), waves.size() * sizeof(Wave));
			} else {
				infoP->waves = NULL;
			}

			extra->output->pre_render_data = infoP;
			extra->output->delete_pre_render_data_func = DisposePreRenderData;

			UnionLRect(&in_result.result_rect, &extra->output->result_rect);
			UnionLRect(&in_result.max_result_rect, &extra->output->max_result_rect);
			
		} else {
			err = PF_Err_OUT_OF_MEMORY;
		}
	}

	if (extra->cb->GuidMixInPtr) {
		if (waves.size() > 0) {
			ERR(extra->cb->GuidMixInPtr(in_data->effect_ref, waves.size() * sizeof(Wave), reinterpret_cast<void *>(&waves[0])));
		} else {
			ERR(extra->cb->GuidMixInPtr(in_data->effect_ref, 0, NULL));
		}
	}

	ERR(PF_CHECKIN_PARAM(in_data, &minDepth_param));
	ERR(PF_CHECKIN_PARAM(in_data, &maxDepth_param));
	ERR(PF_CHECKIN_PARAM(in_data, &nearBlockSize_param));
	ERR(PF_CHECKIN_PARAM(in_data, &farBlockSize_param));
	ERR(PF_CHECKIN_PARAM(in_data, &numBlocksX_param));
	ERR(PF_CHECKIN_PARAM(in_data, &numBlocksY_param));
	ERR(PF_CHECKIN_PARAM(in_data, &colorizeWaves_param));
	ERR(PF_CHECKIN_PARAM(in_data, &colorizeWavesCycleRadius_param));
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

	AEGP_SuiteHandler	suites(in_data->pica_basicP);


	ERR((extra->cb->checkout_layer_pixels(in_data->effect_ref, DepthWaves_INPUT, &input_worldP)));

	ERR((extra->cb->checkout_layer_pixels(in_data->effect_ref, DepthWaves_DEPTHMAP_LAYER, &depth_worldP)));

	ERR(extra->cb->checkout_output(in_data->effect_ref, &output_worldP));
	
	ERR(AEFX_AcquireSuite(in_data,
		out_data,
		kPFWorldSuite,
		kPFWorldSuiteVersion2,
		"Couldn't load suite.",
		(void**)&wsP));

	DepthWavesInfo *info = reinterpret_cast<DepthWavesInfo*>(extra->input->pre_render_data);

	if (!err && info){
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
			AESDK_OpenGL_InitResources(*renderContext.get(), widthL, heightL, info->numBlocksX, info->numBlocksY, info->waves, info->numWaves, S_ResourcePath);

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
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			
			/*** Compute Particles ***/

			if (info->numBlocksX * info->numBlocksY > 0) {
				ComputeParticles(
					renderContext,
					colorTexture,
					depthTexture,
					info
				);

				Vertex *verts = new Vertex[info->numBlocksX * info->numBlocksY];
				glGetNamedBufferSubData(renderContext->vertBuffer, 0, info->numBlocksX * info->numBlocksY * sizeof(Vertex), verts);

				Wave *waveBuf = new Wave[info->numWaves];
				glGetNamedBufferSubData(renderContext->waveBuffer, 0, info->numWaves * sizeof(Wave), waveBuf);

				delete[] waveBuf;
				delete[] verts;
				RenderGL(
					renderContext,
					renderContext->mOutputFrameTexture,
					widthL, heightL,
					info,
					multiplier16bit
				);
			}
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
		"DepthWaves v1", // Name
		"JPERL DepthWaves v1", // Match Name
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
