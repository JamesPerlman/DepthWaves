/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2007 Adobe Systems Incorporated                       */
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

#include "DepthWaves.h"


typedef struct {
	unsigned long	index;
	char			str[256];
} TableString;


TableString		g_strs[StrID_NUMTYPES] = {
	StrID_NONE,										"",
	StrID_Name,										"DepthWaves v1",
	StrID_Description,								"Send waves through a voxel reconstruction from a color map and a depth map.",
	StrID_DepthMap_Layer_Name,						"Depth Map Layer",
	StrID_Emitter_Impulse_Switch_Name,				"Emitter Impulse",
	StrID_Emitter_Position_Point_Name,				"Emitter Position",
	StrID_Min_Depth_Slider_Name,					"Min Depth",
	StrID_Max_Depth_Slider_Name,					"Max Depth",
	StrID_Near_Block_Size_Slider_Name,				"Near Block Size",
	StrID_Far_Block_Size_Slider_Name,				"Far Block Size",
	StrID_Wave_Block_Size_Multiplier_Slider_Name,	"Wave Block Size Multiplier",
	StrID_Wave_Displacement_Slider_Name,			"Wave Maximum Displacement",
	StrID_Wave_Displacement_Direction_Name,			"Wave Displacement Direction",
	StrID_Wave_Brightness_Slider_Name,				"Wave Brightness",
	StrID_Wave_Velocity_Slider_Name,				"Wave Velocity",
	StrID_Wave_Decay_Slider_Name,					"Wave Decay",
	StrID_Num_Blocks_X_Name,						"Num Blocks (Horizontal)",
	StrID_Num_Blocks_Y_Name,						"Num Blocks (Vertical)"
};


char	*GetStringPtr(int strNum)
{
	return g_strs[strNum].str;
}

	