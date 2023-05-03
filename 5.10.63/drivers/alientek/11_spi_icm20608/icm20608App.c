#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "sys/ioctl.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include <poll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
/***************************************************************
Copyright © ALIENTEK Co., Ltd. 1998-2029. All rights reserved.
文件名        : icm20608App.c
作者          : 左忠凯
版本           : V1.0
描述           : icm20608设备测试APP。
其他           : 无
使用方法     ：./icm20608App /dev/icm20608
论坛            : www.openedv.com
日志           : 初版V1.0 2019/9/20 左忠凯创建
***************************************************************/

/*
 * @description        : main主程序
 * @param - argc     : argv数组元素个数
 * @param - argv     : 具体参数
 * @return             : 0 成功;其他 失败
 */
int main(int argc, char *argv[])
{
    int fd;
    char *filename;
    //signed int databuf[7];
    unsigned char data[14];
    signed int gyro_x_adc, gyro_y_adc, gyro_z_adc;
    signed int accel_x_adc, accel_y_adc, accel_z_adc;
    signed int temp_adc;

    float gyro_x_act, gyro_y_act, gyro_z_act;
    float accel_x_act, accel_y_act, accel_z_act;
    float temp_act;

    int ret = 0;

    if (argc != 2) {
        printf("Error Usage!\r\n");
        return -1;
    }

    filename = argv[1];
    fd = open(filename, O_RDWR);
    if(fd < 0) {
        printf("can't open file %s\r\n", filename);
        return -1;
    }

    while (1) {
        ret = read(fd, data, sizeof(data));
        if(ret == 0) {             /* 数据读取成功 */
            gyro_x_adc = data[8]<<8  | data[9];
            gyro_y_adc = data[10]<<8 | data[11];
            gyro_z_adc = data[12]<<8 | data[13];
            accel_x_adc = data[0]<<8  | data[1];
            accel_y_adc = data[2]<<8  | data[3];
            accel_z_adc = data[4]<<8  | data[5];
            temp_adc = data[6]<<8 | data[7];
            /* 计算实际值 */
            gyro_x_act = (float)(gyro_x_adc)  / 16.4;
            gyro_y_act = (float)(gyro_y_adc)  / 16.4;
            gyro_z_act = (float)(gyro_z_adc)  / 16.4;
            accel_x_act = (float)(accel_x_adc) / 2048;
            accel_y_act = (float)(accel_y_adc) / 2048;
            accel_z_act = (float)(accel_z_adc) / 2048;
            temp_act = ((float)(temp_adc) - 25 ) / 326.8 + 25;

            printf("sensor raw value:\n");
            printf("gx=%d,gy=%d,gz=%d;\n", gyro_x_adc, gyro_y_adc, gyro_z_adc);
            printf("ax=%d,ay=%d,az=%d;\n", accel_x_adc, accel_y_adc, accel_z_adc);
            printf("temp=%d\r\n", temp_adc);
            printf("sensor calucate value:\n");
            printf("gro: gx=%.2f°/S,gy=%.2f°/S,gz=%.2f°/S;\n", gyro_x_act, gyro_y_act, gyro_z_act);
            printf("accel: ax=%.2fg,ay=%.2fg,az=%.2fg;\n", accel_x_act, accel_y_act, accel_z_act);
            printf("temperature = %.2f°C\r\n\n", temp_act);
        }
        usleep(1000000); /*100ms */
    }
    close(fd);    /* 关闭文件 */
    return 0;
}

