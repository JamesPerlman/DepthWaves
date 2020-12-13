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

#pragma once

typedef enum {
	StrID_NONE, 
	StrID_Name,
	StrID_Description,
	StrID_DepthMap_Layer_Name,
	StrID_Emitter_Impulse_Switch_Name,
	StrID_Emitter_Position_Point_Name,
	StrID_Min_Depth_Slider_Name,
	StrID_Max_Depth_Slider_Name,
	StrID_Scale_Blocks_With_Depth_Name,
	StrID_Near_Block_Size_Slider_Name,
	StrID_Far_Block_Size_Slider_Name,
	StrID_Wave_Block_Size_Multiplier_Slider_Name,
	StrID_Wave_Displacement_Slider_Name,
	StrID_Wave_Displacement_Direction_Name,
	StrID_Wave_Brightness_Slider_Name,
	StrID_Wave_Velocity_Slider_Name,
	StrID_Wave_Decay_Slider_Name,
	StrID_Num_Blocks_X_Name,
	StrID_Num_Blocks_Y_Name,
	StrID_NUMTYPES
} StrIDType;
