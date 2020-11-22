/**
  ******************************************************************************
  * @FileName			    LeverControlTask.cpp
  * @Description            Control the robot by the lever on the remote
  * @author                 Steve Young
  * @note
  ******************************************************************************
  *
  * Copyright (c) 2021 Team JiaoLong-ShanghaiJiaoTong University
  * All rights reserved.
  *
  ******************************************************************************
**/

#include "includes.h"

#define USE_CHASSIS_FOLLOW
uint16_t flag = 1000;

void Remote::LeverControl() {
    static WorkState_e lastWorkState = NORMAL_STATE;
    if (workState <= 0) return;
    
		__HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_3,1000);
	  __HAL_TIM_SET_COMPARE(&htim5,TIM_CHANNEL_3,500); 
	
     if (workState == NORMAL_STATE) {
				if (channel.lcol>400){																		//open gate1 to put small balls
						__HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_3,500);
				}
				else{                           //open gate2 to put big balls
						__HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_3,1000);
				}
				
				if (channel.rcol>400){                                    //open the outside gate
						__HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_4,1500);
				}
				else{                                                     //close the outside gate
						__HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_4,500);
				}
				/*
				if(times>0){
					uint8_t t = 5;
					//int t = RC_CtrlData.rc.info[1];
					int sendNumber(t);
					times--;
				}
				*/
				//sendNumber(RC_CtrlData.rc.info[1]);
				
				if(flag < 1000){
						//sendNumber(RC_CtrlData.rc.info[1]);
						if(flag%20 == 0){
								sendNumber(0);
						}
						flag = flag + 1;
				}
				
				//useless
				//Additional::additional.setTargetAngle(150); //to pass the hill
    } 
		else if (workState == ADDITIONAL_STATE_ONE) {
			/*
				if(RC_CtrlData.rc.info[0]== 0x02){
						flag = true;
				}
			*/
			flag = 0;
			
				//times = 50;
				__HAL_TIM_SET_COMPARE(&htim5,TIM_CHANNEL_3,500);
				Chassis::chassis.SetVelocity(channel.rcol, channel.rrow, channel.lrow * 5);   //control car driving
				Additional::additional.setTargetAngle(150); //adjust the car position to fit clamp
    }
		else if (workState == ADDITIONAL_STATE_TWO) {
				//still useless
			/*
				if(flag_rc) {
						sendNumber(vsMsg);
						flag_rc = false;
				}
			*/
    }
    lastWorkState = workState;
}
