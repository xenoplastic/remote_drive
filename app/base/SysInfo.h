/**
 * @brief lijun
 * 获取系统资源信息
 */

#pragma once

#define BUFFSIZE 512
#define WAIT_SECOND 1

#include <sys/sysinfo.h>
#include <sys/time.h>
#include <stdint.h>

typedef struct _CPU_PACKED
{
  char name[16];
  unsigned int user;   //用户模式
  unsigned int nice;   //低优先级的用户模式
  unsigned int system; //内核模式
  unsigned int idle;   //空闲处理器时间
} CPU_OCCUPY;

/*主机的状态信息结构体*/
typedef struct _HOST_STATE
{
  int hour;
  int minute;
  double cpu_used;
  double mem_used;
} HOST_STATE;

/*收发数据包结构体*/
typedef struct _RTX_BYTES
{
  long int tx_bytes;
  long int rx_bytes;
  struct timeval rtx_time;
} RTX_BYTES;

/*网卡设备信息结构体*/
typedef struct _NET_INTERFACE
{
  char name[16];                /*网络接口名称*/
  char ip[16];                  /*网口IP*/
  double kDownSpeed;            /*下行速度*/
  double kUploadSpeed;          /*上行速度*/
  char mac[13];                 /*网口MAC地址*/
  RTX_BYTES rtx0_cnt;
  RTX_BYTES rtx1_cnt;
  struct _NET_INTERFACE *next;  /*链表指针*/
} NET_INTERFACE;

/**
 * @description: 获取某个时刻的CPU负载
 * @param {CPU_OCCUPY} *cpu
 * @return {*}
 * @author: Ming
 */
bool get_cpuoccupy(CPU_OCCUPY *cpu);

/**
 * @description: 计算CPU占用率
 * @param {CPU_OCCUPY} *c1
 * @param {CPU_OCCUPY} *c2
 * @return {*}
 * @author: Ming
 */
double cal_occupy(CPU_OCCUPY *c1, CPU_OCCUPY *c2);

/**
 * @description: 获取内存占用率
 * @param {double} *mem_used
 * @return {*}
 */
bool get_mem_usage(double& mbTotalMemory, double& mbFreeMemory);

/**
 * @description: 获取内存占用率
 * @param {double} *cpu_used
 * @return {*}
 * @author: Ming
 */
bool get_cpu_usage(double *cpu_used);

/**
 * @description: 获取网卡数量和IP地址
 * @param {NET_INTERFACE} *
 * @param {int} *n 本机网卡数量
 * @return {*}
 */
bool get_interface_info(NET_INTERFACE **net, int *n);

/**
 * @description: 显示本机活动网络接口
 * @param {NET_INTERFACE} *p_net
 * @return {*}
 */
void show_netinterfaces(NET_INTERFACE *p_net, int n);

/**
 * @description: 获取网卡当前时刻的收发包信息
 * @param {char} *name
 * @param {RTX_BYTES} *rtx
 * @return {*}
 */
bool get_rtx_bytes(char *name, RTX_BYTES *rtx);

/**
 * @description: rtx结构体拷贝--disable
 * @param {RTX_BYTES} *dest
 * @param {RTX_BYTES} *src
 * @return {*}
 */
void rtx_bytes_copy(RTX_BYTES *dest, RTX_BYTES *src);

/**
 * @description: 获取网卡数量和IP地址
 * @param {NET_INTERFACE} *
 * @param {int} *n 本机网卡数量
 * @return {*}
 */
bool get_interface_info(NET_INTERFACE **net, int *n);

/**
 * @description: 计算网卡的上下行网速
 * @param {double} *u_speed
 * @param {double} *d_speed
 * @param {unsignedchar} *level
 * @param {RTX_BYTES} *rtx0
 * @param {RTX_BYTES} *rtx1
 * @return {*}
 */
void cal_netinterface_speed(
        double *kUploadspeed,
        double *lDownspeed,
        RTX_BYTES *rtx0,
        RTX_BYTES *rtx1
        );

/**
 * @description: 获取主机网卡速度信息
 * @param {NET_INTERFACE} *p_net
 * @return {*}
 */
bool get_network_speed(NET_INTERFACE *p_net);

/**
 * @description: 获取disk信息
 * @param {NET_INTERFACE} *p_net
 * @return {*}
 */
bool get_disk_info(uint64_t& totalsize, uint64_t& freeDisk);

enum SysInfoType
{
    memory          = 0x0001,
    cpu             = 0x0002,
    disk            = 0x0004,
    net             = 0x0008,
    all             = memory | cpu | disk | net
};

struct SysData
{
    int sysInfoType           = 0;
    double cpuPercent         = 0.f;
    double mbMemoryTotal       = 0.f;
    double mbMemoryFree        = 0.f;
    uint64_t mbDiskTotal      = 0;
    uint64_t mbDiskFree       = 0;
    double kUploadNetSpeed    = 0.f;
    double kDownNetSpeed      = 0.f;
};

#include <functional>
#include <atomic>
#include <thread>

using SYS_MINITOR_CALLBACK = std::function<void(const SysData&)>;

class SysInfo
{
public:
    SysInfo();
    ~SysInfo();

    void StartMinitor(SysInfoType sysInfoType, uint16_t secReadPeriod, SYS_MINITOR_CALLBACK callback);
    void StopMinitor();
    bool IsMonitoring();
    float GetCPU(unsigned pid);
    unsigned GetMemory(unsigned pid);
    unsigned GetVirtualMemory(unsigned pid);

private:
    SYS_MINITOR_CALLBACK m_callback;
    std::atomic_bool m_running;
    std::thread m_netThread;
    std::thread m_hostCoreThread;
    SysInfoType m_sysInfoType;
};
