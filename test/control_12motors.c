#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#endif

#include "motor_protocol.h"

/* ==================== 跨平台串口操作 ==================== */
#ifdef _WIN32
typedef HANDLE SerialPort;

SerialPort serial_open(const char *port_name)
{
    char full_port[256];
    snprintf(full_port, sizeof(full_port), "\\\\.\\%s", port_name);
    
    HANDLE hSerial = CreateFileA(
        full_port,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hSerial == INVALID_HANDLE_VALUE) {
        printf("打开串口失败: %s\n", port_name);
        return INVALID_HANDLE_VALUE;
    }
    
    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    
    if (!GetCommState(hSerial, &dcbSerialParams)) {
        printf("获取串口状态失败\n");
        CloseHandle(hSerial);
        return INVALID_HANDLE_VALUE;
    }
    
    dcbSerialParams.BaudRate = CBR_115200;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;
    
    if (!SetCommState(hSerial, &dcbSerialParams)) {
        printf("设置串口参数失败\n");
        CloseHandle(hSerial);
        return INVALID_HANDLE_VALUE;
    }
    
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    
    if (!SetCommTimeouts(hSerial, &timeouts)) {
        printf("设置超时失败\n");
        CloseHandle(hSerial);
        return INVALID_HANDLE_VALUE;
    }
    
    return hSerial;
}

void serial_close(SerialPort port)
{
    if (port != INVALID_HANDLE_VALUE) {
        CloseHandle(port);
    }
}

int serial_write(SerialPort port, const uint8_t *data, size_t len)
{
    DWORD bytes_written;
    if (!WriteFile(port, data, (DWORD)len, &bytes_written, NULL)) {
        return -1;
    }
    return (int)bytes_written;
}

int serial_read(SerialPort port, uint8_t *data, size_t len)
{
    DWORD bytes_read;
    if (!ReadFile(port, data, (DWORD)len, &bytes_read, NULL)) {
        return -1;
    }
    return (int)bytes_read;
}

void sleep_ms(int ms)
{
    Sleep(ms);
}

#else
typedef int SerialPort;

SerialPort serial_open(const char *port_name)
{
    int fd = open(port_name, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1) {
        printf("打开串口失败: %s\n", port_name);
        return -1;
    }
    
    struct termios options;
    tcgetattr(fd, &options);
    
    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);
    
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_oflag &= ~OPOST;
    
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 10;
    
    tcsetattr(fd, TCSANOW, &options);
    
    return fd;
}

void serial_close(SerialPort port)
{
    if (port != -1) {
        close(port);
    }
}

int serial_write(SerialPort port, const uint8_t *data, size_t len)
{
    return (int)write(port, data, len);
}

int serial_read(SerialPort port, uint8_t *data, size_t len)
{
    return (int)read(port, data, len);
}

void sleep_ms(int ms)
{
    usleep(ms * 1000);
}
#endif

int receive_feedback(SerialPort port, MotorFeedback feedbacks[12])
{
    uint8_t data[USB_PKG_TOTAL_LEN];
    int bytes_read = serial_read(port, data, USB_PKG_TOTAL_LEN);
    
    if (bytes_read == USB_PKG_TOTAL_LEN && 
        data[0] == USB_PKG_HEADER && 
        data[111] == USB_PKG_FOOTER) {
        return feedback_pkg_parse(data, feedbacks);
    }
    
    return -1;
}

int main(int argc, char *argv[])
{
    const char *port_name = NULL;
    
    if (argc > 1) {
        port_name = argv[1];
    } else {
#ifdef _WIN32
        port_name = "COM3";
#else
        port_name = "/dev/ttyACM0";
#endif
        printf("使用默认串口: %s\n", port_name);
        printf("如需指定串口，请运行: %s <串口名>\n", argv[0]);
    }
    
    printf("============================================\n");
    printf("  12 电机控制程序 - C 语言版本\n");
    printf("  控制所有电机旋转 30 度\n");
    printf("============================================\n\n");
    
    SerialPort port = serial_open(port_name);
    if (port == (SerialPort)-1 || port == INVALID_HANDLE_VALUE) {
        printf("无法打开串口，程序退出\n");
        return 1;
    }
    
    printf("成功打开串口: %s\n", port_name);
    printf("注：CDC虚拟串口，波特率设置不影响实际速率\n\n");
    
    USBControlPkg pkg;
    uint8_t data[USB_PKG_TOTAL_LEN];
    MotorFeedback feedbacks[12];
    
    printf("[1/3] 设置所有电机到 0 度位置...\n");
    usb_control_pkg_init(&pkg);
    pkg.sequence = 1;
    usb_control_pkg_set_all_motors_position_deg(&pkg, 0.0f, 1.0f, 10.0f);
    usb_control_pkg_pack(&pkg, data);
    serial_write(port, data, USB_PKG_TOTAL_LEN);
    printf("  发送数据包，长度: %d 字节\n", USB_PKG_TOTAL_LEN);
    sleep_ms(1000);
    
    if (receive_feedback(port, feedbacks) == 0) {
        printf("  收到反馈 - 电机1位置: %.2f°\n", rad_to_deg(feedbacks[0].position));
    }
    
    printf("\n[2/3] 设置所有电机到 30 度位置...\n");
    usb_control_pkg_init(&pkg);
    pkg.sequence = 2;
    usb_control_pkg_set_all_motors_position_deg(&pkg, 30.0f, 1.0f, 10.0f);
    usb_control_pkg_pack(&pkg, data);
    serial_write(port, data, USB_PKG_TOTAL_LEN);
    printf("  发送数据包，长度: %d 字节\n", USB_PKG_TOTAL_LEN);
    sleep_ms(2000);
    
    if (receive_feedback(port, feedbacks) == 0) {
        printf("  收到反馈 - 电机1位置: %.2f°\n", rad_to_deg(feedbacks[0].position));
    }
    
    printf("\n[3/3] 设置所有电机回到 0 度位置...\n");
    usb_control_pkg_init(&pkg);
    pkg.sequence = 3;
    usb_control_pkg_set_all_motors_position_deg(&pkg, 0.0f, 1.0f, 10.0f);
    usb_control_pkg_pack(&pkg, data);
    serial_write(port, data, USB_PKG_TOTAL_LEN);
    printf("  发送数据包，长度: %d 字节\n", USB_PKG_TOTAL_LEN);
    sleep_ms(1000);
    
    if (receive_feedback(port, feedbacks) == 0) {
        printf("  收到反馈 - 电机1位置: %.2f°\n", rad_to_deg(feedbacks[0].position));
    }
    
    printf("\n============================================\n");
    printf("  控制完成！\n");
    printf("============================================\n");
    
    serial_close(port);
    return 0;
}
