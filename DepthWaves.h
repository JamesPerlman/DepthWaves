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

/*******************
Important Notice 06/02/2020
OpenGL was deprecated on macOS 10.14 with Apple asking all developers to move to using the Metal framework.
As such we are recommending cross-platform plugins are no longer developed directly against OpenGL.
For GPU-based plugin development, please refer to the SDK_Invert_ProcAmp SDK sample on how to use GPU rendering with the plugin SDK.
This plugin is kept only for reference.
*******************/

/*
	DepthWaves.h
*/

#pragma once

#ifndef DepthWaves_H
#define DepthWaves_H

typedef unsigned char		u_char;
typedef unsigned short		u_short;
typedef unsigned short		u_int16;
typedef unsigned long		u_long;
typedef short int			int16;
typedef float				fpshort;

#define PF_TABLE_BITS	12
#define PF_TABLE_SZ_16	4096

#define PF_DEEP_COLOR_AWARE 1	// make sure we get 16bpc pixels; 
								// AE_Effect.h checks for this.
#include "AEConfig.h"

#ifdef AE_OS_WIN
	typedef unsigned short PixelType;
	#include <Windows.h>
#endif

#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "AE_EffectCBSuites.h"
#include "String_Utils.h"
#include "AE_GeneralPlug.h"
#include "AEFX_ChannelDepthTpl.h"
#include "AEGP_SuiteHandler.h"

#include "CameraTransform.hpp"
#include "Impulse.h"
#include "Wave.h"
#include "vmath.hpp"

#include <vector>

#include "DepthWaves_Strings.h"


/* Versioning information */

#define	MAJOR_VERSION	1
#define	MINOR_VERSION	0
#define	BUG_VERSION		0
#define	STAGE_VERSION	PF_Stage_DEVELOP
#define	BUILD_VERSION	1


/* Parameter defaults */

#define	DepthWaves_EMITTER_IMPULSE_DEFAULT					false
#define DepthWaves_MIN_DEPTH_DEFAULT						100.0
#define DepthWaves_MAX_DEPTH_DEFAULT						1000.0
#define DepthWaves_MIN_BLOCK_SIZE_DEFAULT					0.0
#define DepthWaves_MAX_BLOCK_SIZE_DEFAULT					0.0
#define DepthWaves_WAVE_BLOCK_SIZE_MULTIPLIER_DEFAULT		0.0
#define DepthWaves_WAVE_DISPLACEMENT_DEFAULT				100.0
#define DepthWaves_WAVE_COLOR_MIX_DEFAULT					0.0
#define DepthWaves_WAVE_VELOCITY_DEFAULT					100.0
#define DepthWaves_WAVE_DECAY_DEFAULT						0.95
#define DepthWaves_COLORIZE_WAVES_CHECKBOX_DEFAULT			false
#define DepthWaves_COLORIZE_WAVES_CYCLE_RADIUS_DEFAULT		0.0
#define DepthWaves_NUM_BLOCKS_DEFAULT						50

#define DepthWaves_BLOCK_SIZE_SLIDER_MIN					0.0000
#define DepthWaves_BLOCK_SIZE_SLIDER_MAX					1000.0
#define DepthWaves_DEPTH_SLIDER_MIN							0
#define DepthWaves_DEPTH_SLIDER_MAX							100000.0
#define DepthWaves_WAVE_BLOCK_SIZE_MULTIPLIER_MIN			0.0
#define DepthWaves_WAVE_BLOCK_SIZE_MULTIPLIER_MAX			100.0
#define DepthWaves_WAVE_DISPLACEMENT_SLIDER_MIN				-10000.0
#define DepthWaves_WAVE_DISPLACEMENT_SLIDER_MAX				10000.0
#define DepthWaves_WAVE_COLOR_MIX_SLIDER_MIN				0.0
#define DepthWaves_WAVE_COLOR_MIX_SLIDER_MAX				1.0
#define DepthWaves_WAVE_VELOCITY_SLIDER_MIN					0.0
#define DepthWaves_WAVE_VELOCITY_SLIDER_MAX					10000.0
#define DepthWaves_WAVE_DECAY_SLIDER_MIN					0.0
#define DepthWaves_WAVE_DECAY_SLIDER_MAX					1.0
#define DepthWaves_COLORIZE_WAVES_CYCLE_RADIUS_SLIDER_MIN	0.0
#define DepthWaves_COLORIZE_WAVES_CYCLE_RADIUS_SLIDER_MAX	100000.0
#define DepthWaves_NUM_BLOCKS_SLIDER_MIN					1
#define DepthWaves_NUM_BLOCKS_SLIDER_MAX					2000

enum {
	DepthWaves_INPUT = 0,
	DepthWaves_DEPTHMAP_LAYER,
	DepthWaves_EMITTER_IMPULSE,
	DepthWaves_EMITTER_POSITION,
	DepthWaves_MIN_DEPTH,
	DepthWaves_MAX_DEPTH,
	DepthWaves_NEAR_BLOCK_SIZE,
	DepthWaves_FAR_BLOCK_SIZE,
	DepthWaves_WAVE_BLOCK_SIZE_MULTIPLIER,
	DepthWaves_WAVE_DISPLACEMENT,
	DepthWaves_WAVE_DISPLACEMENT_DIRECTION,
	DepthWaves_WAVE_COLOR,
	DepthWaves_WAVE_COLOR_MIX,
	DepthWaves_WAVE_VELOCITY,
	DepthWaves_WAVE_DECAY,
	DepthWaves_COLORIZE_WAVES,
	DepthWaves_COLORIZE_WAVES_CYCLE_RADIUS,
	DepthWaves_NUM_BLOCKS_X,
	DepthWaves_NUM_BLOCKS_Y,
	DepthWaves_NUM_PARAMS
};

enum {
	DEPTHMAP_LAYER_DISK_ID = 1,
	EMITTER_IMPULSE_DISK_ID,
	EMITTER_POSITION_DISK_ID,
	MIN_DEPTH_DISK_ID,
	MAX_DEPTH_DISK_ID,
	NEAR_BLOCK_SIZE_DISK_ID,
	FAR_BLOCK_SIZE_DISK_ID,
	WAVE_BLOCK_SIZE_MULTIPLIER_DISK_ID,
	WAVE_DISPLACEMENT_DISK_ID,
	WAVE_DISPLACEMENT_DIRECTION_DISK_ID,
	WAVE_COLOR_DISK_ID,
	WAVE_COLOR_MIX_DISK_ID,
	WAVE_VELOCITY_DISK_ID,
	WAVE_DECAY_DISK_ID,
	COLORIZE_WAVES_DISK_ID,
	COLORIZE_WAVES_CYCLE_RADIUS_DISK_ID,
	NUM_BLOCKS_X_DISK_ID,
	NUM_BLOCKS_Y_DISK_ID
};

extern "C" {
	
	DllExport
	PF_Err 
	EffectMain(
		PF_Cmd			cmd,
		PF_InData		*in_data,
		PF_OutData		*out_data,
		PF_ParamDef		*params[],
		PF_LayerDef		*output,
		void			*extra);

}

typedef struct DepthWavesInfo {
	PF_FpLong minDepth, maxDepth;
	PF_FpLong nearBlockSize, farBlockSize;
	PF_FpLong colorCycleRadius;

	A_Boolean colorizeWaves;

	A_long numBlocksX;
	A_long numBlocksY;
	A_long numWaves;

	Wave *waves;

	CameraTransform cameraTransform;
} DepthWavesInfo, *DepthWavesInfoP, **DepthWavesInfoH;

//helper func
inline u_char AlphaLookup(u_int16 inValSu, u_int16 inMaxSu)
{
	fpshort normValFp = 1.0f - (inValSu)/static_cast<fpshort>(inMaxSu);
	return static_cast<u_char>(normValFp*normValFp*0.8f*255);
}

//error checking macro
#define CHECK(err) {PF_Err err1 = err; if (err1 != PF_Err_NONE ){ throw PF_Err(err1);}}

#define MIX(a, b, k) (a + (b - a) * k)

#endif // DepthWaves_H
