/**
  ******************************************************************************
  * @FileName			    RemoteTask.cpp
  * @Description            Reveive the data from remote and switch the mode(FSM)
  * @author                 Steve Young
  * @note
  ******************************************************************************
  *
  * Copyright (c) 2021 Team JiaoLong-ShanghaiJiaoTong University
  * All rights reserved.
  *
  ******************************************************************************
**/

//#include <RemoteTask.h>

#include "includes.h"
#include "MotorTask.h"

void RemoteRecv::GetRemoteSwitchAction(RemoteSwitch_t *sw, uint8_t val) {
    static uint32_t switch_cnt = 0;

    sw->switch_value_raw = val;
    sw->switch_value_buf[sw->buf_index] = sw->switch_value_raw;

    //value1 value2的值其实是一样的
    //value1高4位始终为0
    sw->switch_value1 = (sw->switch_value_buf[sw->buf_last_index] << 2) |
                        (sw->switch_value_buf[sw->buf_index]);

    sw->buf_end_index = (sw->buf_index + 1) % REMOTE_SWITCH_VALUE_BUF_DEEP;

    sw->switch_value2 = (sw->switch_value_buf[sw->buf_end_index] << 4) | sw->switch_value1;

    //如果两次数据一样，即没有更新数据，拨杆不动
    if (sw->switch_value_buf[sw->buf_index] == sw->switch_value_buf[sw->buf_last_index]) {
        switch_cnt++;
    } else {
        switch_cnt = 0;
    }
    //如果拨杆维持了一定时间，即连续来了40帧一样的数据，则把拨杆数据写入switch_long_value
    if (switch_cnt >= 40) {
        sw->switch_long_value = sw->switch_value_buf[sw->buf_index];
    }
    //指向下一个缓冲区
    sw->buf_last_index = sw->buf_index;
    sw->buf_index++;
    if (sw->buf_index == REMOTE_SWITCH_VALUE_BUF_DEEP) {
        sw->buf_index = 0;
    }
}

void RemoteRecv::RemoteDataProcess(uint8_t *pData) {
    IWDG_counter = 0;

    if (pData == NULL) return;

    //遥控器 4*11bits摇杆   2*2bits开关 共48位，占用6字节
    //16位无符号，低11位有效
    RC_CtrlData.rc.ch0 = (*(uint32_t*)&pData[0]>>(00-(0*8)))&0x7FF;  //保持32-(22-(x*16))>11即可
    RC_CtrlData.rc.ch1 = (*(uint32_t*)&pData[0]>>(11-(0*8)))&0x7FF;
    RC_CtrlData.rc.ch2 = (*(uint32_t*)&pData[2]>>(22-(2*8)))&0x7FF;
    RC_CtrlData.rc.ch3 = (*(uint32_t*)&pData[2]>>(33-(2*8)))&0x7FF;
    //8位无符号，低2位有效
    RC_CtrlData.rc.s1  = (*(uint32_t*)&pData[5]>>(44-(5*8)))&0x03; // 档位开关
    RC_CtrlData.rc.s2  = (*(uint32_t*)&pData[5]>>(46-(5*8)))&0x03; // 档位开关
    for(int i=0; i<10; i++)
    {
			RC_CtrlData.rc.info[i]=pData[i+6]; // 使用手机外设时
    }
    RC_CtrlData.rc.dial = (*(uint32_t*)&pData[16]>>(128-(16*8)))&0xFFFF;
		

    //要 8 Bytes
    RC_CtrlData.mouse.x = ((int16_t) pData[6]) | ((int16_t) pData[7] << 8);
    RC_CtrlData.mouse.y = ((int16_t) pData[8]) | ((int16_t) pData[9] << 8);
    RC_CtrlData.mouse.z = ((int16_t) pData[10]) | ((int16_t) pData[11] << 8);

    RC_CtrlData.mouse.press_l = pData[12];
    RC_CtrlData.mouse.press_r = pData[13];

    //要 2 Bytes = 16 bits 每一位应一
    RC_CtrlData.key.v = ((int16_t) pData[14]) | ((int16_t) pData[15] << 8);

    //状态
    if (RC_CtrlData.rc.s2 == 1) inputMode = REMOTE_INPUT;
    else if (RC_CtrlData.rc.s2 == 3) inputMode = KEY_MOUSE_INPUT;
    else inputMode = STOP;

    if (RC_CtrlData.rc.s1 == 1) functionMode = UPPER_POS;
    else if (RC_CtrlData.rc.s1 == 3) functionMode = MIDDLE_POS;
    else functionMode = LOWER_POS;

    GetRemoteSwitchAction(&g_switch1, RC_CtrlData.rc.s1);
    RemoteKeyMouseControlLoop();
}

void RemoteRecv::RemoteDataRecv() {
    rc_cnt++;
    if (rx_free == 1) {
        if ((rc_cnt <= 17) && (rc_first_frame == 1)) {
            RemoteDataProcess(rc_data);
            HAL_UART_AbortReceive(&RC_UART);
            rx_free = 0;
            while (HAL_UART_Receive_DMA(&RC_UART, rc_data, 18) != HAL_OK);
            rc_cnt = 0;
        } else {
            if (rc_first_frame == 0) {
                //workState = PREPARE_STATE;
                HAL_UART_AbortReceive(&RC_UART);
                while (HAL_UART_Receive_DMA(&RC_UART, rc_data, 18) != HAL_OK);
                rc_cnt = 0;
                rc_first_frame = 1;
            }
        }
        rc_update = 0;
    }
}

void Remote::RemoteDataGet() {
    RemoteDataRecv();
    channel.rrow = (RC_CtrlData.rc.ch0 - (int16_t) REMOTE_CONTROLLER_STICK_OFFSET);
    channel.rcol = (RC_CtrlData.rc.ch1 - (int16_t) REMOTE_CONTROLLER_STICK_OFFSET);
    channel.lrow = (RC_CtrlData.rc.ch2 - (int16_t) REMOTE_CONTROLLER_STICK_OFFSET);
    channel.lcol = (RC_CtrlData.rc.ch3 - (int16_t) REMOTE_CONTROLLER_STICK_OFFSET);
    mouse = RC_CtrlData.mouse;
    key = RC_CtrlData.key;
}

uint16_t FSMCount = 0;

void Remote::WorkStateFSM() {
#ifndef BOARD_SLAVE
    uint8_t allMotorsRx = 1;
    for (auto &motor : Motor::motors) {
        for (auto &j : motor) {
            if (j != nullptr && j->everRx != 1) {
                allMotorsRx = 0;
            }
        }
    }
    switch (workState) {
        case PREPARE_STATE: {
            if (inputMode == STOP) workState = STOP_STATE;
            if (prepare_time < 500 && IMU::BSP_IMU.GetData().InitFinish == 1) prepare_time++;//&& allMotorsRx == 1
            if (prepare_time >= 500) {
                workState = NORMAL_STATE;
                prepare_time = 0;
            }
            FSMCount = (FSMCount + 1) % 2000;
            if(FSMCount == 0) {
                //if (HAL_CAN_Receive_IT(&hcan1, CAN_FIFO0) != HAL_OK) Error_Handler();
                //if (HAL_CAN_Receive_IT(&hcan2, CAN_FIFO0) != HAL_OK) Error_Handler();
            }

            for (auto &motor : Motor::motors) {
                for (auto &j : motor) {

                }
            }
        }
            break;
        case NORMAL_STATE: {
            if (inputMode == STOP) workState = STOP_STATE;

            if (functionMode == MIDDLE_POS) workState = ADDITIONAL_STATE_ONE;
            if (functionMode == LOWER_POS) workState = ADDITIONAL_STATE_TWO;
        }
            break;
        case ADDITIONAL_STATE_ONE: {
            if (inputMode == STOP) workState = STOP_STATE;

            if (functionMode == UPPER_POS) workState = NORMAL_STATE;
            if (functionMode == LOWER_POS) workState = ADDITIONAL_STATE_TWO;
        }
            break;
        case ADDITIONAL_STATE_TWO: {
            if (inputMode == STOP) workState = STOP_STATE;

            if (functionMode == UPPER_POS) workState = NORMAL_STATE;
            if (functionMode == MIDDLE_POS) workState = ADDITIONAL_STATE_ONE;
        }
            break;
        case STOP_STATE: {
            if (inputMode == REMOTE_INPUT || inputMode == KEY_MOUSE_INPUT) {
                workState = PREPARE_STATE;
            }
            for (int i = 0; i < 2; i++) {
                for (int j = 0; j < 8; j++) {
                    if(Motor::motors[i][j] != nullptr){}
                        Motor::motors[i][j]->StopReset();
                }
            }
        }
            break;
        default:
            break;
    }
#else
    WorkState = RxWorkState;
#endif
}

void Remote::IWDG_Handle() {
#ifdef BOARD_SLAVE
    HAL_IWDG_Refresh(&hiwdg);
#endif
    if (IWDG_counter < 1000) IWDG_counter++;
    if (IWDG_counter > 100) {
        workState = STOP_STATE;
        inputMode = STOP;

    }
    if (IWDG_counter < 100) HAL_IWDG_Refresh(&hiwdg);
}

void Remote::Handle() {
    IWDG_Handle();
    RemoteDataGet();
    WorkStateFSM();
}

Remote Remote::remote;
