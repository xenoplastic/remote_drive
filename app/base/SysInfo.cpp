#include <string.h>
#include <stdio.h>
#include <unistd.h>

//多线程处理库
#include <pthread.h>
//网络监控部分
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/vfs.h>

#include "SysInfo.h"

#define VMRSS_LINE 17
#define VMSIZE_LINE 13
#define PROCESS_ITEM 14

typedef struct {
    unsigned long user;
    unsigned long nice;
    unsigned long system;
    unsigned long idle;
}Total_Cpu_Occupy_t;

typedef struct {
	unsigned int pid;
	unsigned long utime;  //user time
	unsigned long stime;  //kernel time
	unsigned long cutime; //all user time
	unsigned long cstime; //all dead time
}Proc_Cpu_Occupy_t;

//获取第N项开始的指针
const char* get_items(const char*buffer ,unsigned int item){

    const char *p =buffer;

    int len = strlen(buffer);
    int count = 0;

    for (int i=0; i<len;i++){
        if (' ' == *p){
            count ++;
            if(count == item -1){
                p++;
                break;
            }
        }
        p++;
    }

    return p;
}

//获取总的CPU时间
unsigned long get_cpu_total_occupy(){

    FILE *fd;
    char buff[1024]={0};
    Total_Cpu_Occupy_t t;

    fd =fopen("/proc/stat","r");
    if (nullptr == fd){
        return 0;
    }

    fgets(buff,sizeof(buff),fd);
    char name[64]={0};
    sscanf(buff,"%s %ld %ld %ld %ld",name,&t.user,&t.nice,&t.system,&t.idle);
    fclose(fd);

    return (t.user + t.nice + t.system + t.idle);
}

//获取进程的CPU时间
unsigned long get_cpu_proc_occupy(unsigned int pid){

    char file_name[64]={0};
    Proc_Cpu_Occupy_t t;
    FILE *fd;
    char line_buff[1024]={0};
    sprintf(file_name,"/proc/%d/stat",pid);

    fd = fopen(file_name,"r");
    if(nullptr == fd){
        return 0;
    }

    fgets(line_buff,sizeof(line_buff),fd);

    sscanf(line_buff,"%u",&t.pid);
    const char *q =get_items(line_buff,PROCESS_ITEM);
    sscanf(q,"%ld %ld %ld %ld",&t.utime,&t.stime,&t.cutime,&t.cstime);
    fclose(fd);

    return (t.utime + t.stime + t.cutime + t.cstime);
}

//获取CPU占用率
float get_proc_cpu(unsigned int pid){

    unsigned long totalcputime1,totalcputime2;
    unsigned long procputime1,procputime2;

    totalcputime1=get_cpu_total_occupy();
    procputime1=get_cpu_proc_occupy(pid);

    usleep(200000);

    totalcputime2=get_cpu_total_occupy();
    procputime2=get_cpu_proc_occupy(pid);

    float pcpu = 0.0;
    if(0 != totalcputime2-totalcputime1){
        pcpu=100.0 * (procputime2-procputime1)/(totalcputime2-totalcputime1);
    }

    return pcpu;
}

//获取进程占用内存
unsigned int get_proc_mem(unsigned int pid){

    char file_name[64]={0};
    FILE *fd;
    char line_buff[512]={0};
    sprintf(file_name,"/proc/%d/status",pid);

    fd =fopen(file_name,"r");
    if(nullptr == fd){
        return 0;
    }

    char name[64];
    int vmrss;
    for (int i=0; i<VMRSS_LINE-1;i++){
        fgets(line_buff,sizeof(line_buff),fd);
    }

    fgets(line_buff,sizeof(line_buff),fd);
    sscanf(line_buff,"%s %d",name,&vmrss);
    fclose(fd);

    return vmrss;
}

//获取进程占用虚拟内存
unsigned int get_proc_virtualmem(unsigned int pid){

    char file_name[64]={0};
    FILE *fd;
    char line_buff[512]={0};
    sprintf(file_name,"/proc/%d/status",pid);

    fd =fopen(file_name,"r");
    if(nullptr == fd){
        return 0;
    }

    char name[64];
    int vmsize;
    for (int i=0; i<VMSIZE_LINE-1;i++){
        fgets(line_buff,sizeof(line_buff),fd);
    }

    fgets(line_buff,sizeof(line_buff),fd);
    sscanf(line_buff,"%s %d",name,&vmsize);
    fclose(fd);

    return vmsize;
}

//进程本身
int get_pid(const char* process_name, const char* user = nullptr)
{
    if(user == nullptr){
        user = getlogin();
    }

    char cmd[512];
    if (user){
        sprintf(cmd, "pgrep %s -u %s", process_name, user);
    }

    FILE *pstr = popen(cmd,"r");

    if(pstr == nullptr){
        return 0;
    }

    char buff[512];
    ::memset(buff, 0, sizeof(buff));
    if(NULL == fgets(buff, 512, pstr)){
        return 0;
    }

    return atoi(buff);
}

bool get_cpuoccupy(CPU_OCCUPY *cpu)
{
    char buff[BUFFSIZE];
    FILE *fd;

    ///proc/stat 包含了系统启动以来的许多关于kernel和系统的统计信息，
    //其中包括CPU运行情况、中断统计、启动时间、上下文切换次数、
    //运行中的进程等等信息
    fd = fopen("/proc/stat", "r");
    if (fd == nullptr)
    {
        perror("open /proc/stat failed\n");
        return false;
    }

    auto ret = fgets(buff, sizeof(buff), fd);
    if(!ret)
        return false;

    sscanf(buff, "%s %u %u %u %u", cpu->name, &cpu->user, &cpu->nice,
           &cpu->system, &cpu->idle);
    fclose(fd);
    return true;
}

double cal_occupy(CPU_OCCUPY *c1, CPU_OCCUPY *c2)
{
    double t1, t2;
    double id, sd;
    double cpu_used;

    t1 = (double)(c1->user + c1->nice + c1->system + c1->idle);
    t2 = (double)(c2->user + c2->nice + c2->system + c2->idle);

    id = (double)(c2->user - c1->user);
    sd = (double)(c2->system - c1->system);

    cpu_used = (100 * (id + sd) / (t2 - t1));
    return cpu_used;
}

bool get_mem_usage(double& mbTotalMemory, double& mbFreeMemory)
{
    FILE *fd;
    fd = fopen("/proc/meminfo", "r");
    if (fd == nullptr) {
        perror("open /proc/meminfo failed\n");
        return false;
    }

    size_t bytes_read;
    char *line = NULL;
    int index = 0;
    int avimem = 0;
    while (getline(&line, &bytes_read, fd) != -1)
    {
        if (++index <= 2)
        {
            continue;
        }
        if (strstr(line, "MemAvailable") != NULL)
        {
            sscanf(line, "%*s%d%*s", &avimem);
            break;
        }
    }
    fclose(fd);

    /*sysinfo系统相关信息的结构体*/
    struct sysinfo info;
    if(sysinfo(&info))
        return false;

    mbTotalMemory= info.totalram / 1024.0f / 1024.0f;
    mbFreeMemory = avimem / 1024.0f;

    return true;
}

bool get_cpu_usage(double *cpu_used)
{
    CPU_OCCUPY cpu_0, cpu_1;
    if(!get_cpuoccupy(&cpu_0))
        return false;

    sleep(WAIT_SECOND);

    if(!get_cpuoccupy(&cpu_1))
        return false;

    *cpu_used = cal_occupy(&cpu_0, &cpu_1);
    return true;
}

bool get_interface_info(NET_INTERFACE *net, int *n)
{
    int fd;
    int num = 0;
    struct ifreq buf[16];
    struct ifconf ifc;

    NET_INTERFACE *p_temp = NULL;
    net->next = nullptr;

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        close(fd);
        return false;
    }

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = (caddr_t)buf;

    if (ioctl(fd, SIOCGIFCONF, (char *)&ifc) != 0)
        return false;

    // get interface nums
    num = ifc.ifc_len / sizeof(struct ifreq);
    *n = num;
    // find all interfaces;
    while (num-- > 0) {
        // get interface name
        strcpy(net->name, buf[num].ifr_name);

        // get the ipaddress of the interface
        if (!(ioctl(fd, SIOCGIFADDR, (char *)&buf[num])))
        {

            memset(net->ip, 0, 16);
            strcpy(net->ip, inet_ntoa(((struct sockaddr_in *)(&buf[num].ifr_addr))->sin_addr));
        }

        // get the mac of this interface
        if (!ioctl(fd, SIOCGIFHWADDR, (char *)(&buf[num])))
        {

            memset(net->mac, 0, 13);

            snprintf(net->mac, 13, "%02x%02x%02x%02x%02x%02x",
                     (unsigned char)buf[num].ifr_hwaddr.sa_data[0],
                    (unsigned char)buf[num].ifr_hwaddr.sa_data[1],
                    (unsigned char)buf[num].ifr_hwaddr.sa_data[2],
                    (unsigned char)buf[num].ifr_hwaddr.sa_data[3],
                    (unsigned char)buf[num].ifr_hwaddr.sa_data[4],
                    (unsigned char)buf[num].ifr_hwaddr.sa_data[5]);
        }
        if (num >= 1)
        {
            p_temp = (NET_INTERFACE *)malloc(sizeof(NET_INTERFACE));
            memset(p_temp, 0, sizeof(NET_INTERFACE));
            net->next = p_temp;
            net = p_temp;
        }
    }
    return true;
}

bool get_rtx_bytes(char *name, RTX_BYTES *rtx) {
    char *line = nullptr;
    size_t bytes_read;

    char str1[32];
    char str2[32];

    int i = 0;
    /*文件/proc/net/dev*/
    FILE *netDevFile = nullptr;
    netDevFile = fopen("/proc/net/dev", "r");
    if (!netDevFile) {
        return false;
    }

    //获取时间
    gettimeofday(&rtx->rtx_time, nullptr);
    //从第三行开始读取网络接口数据
    while (getline(&line, &bytes_read, netDevFile) != -1)
    {
        if ((++i) <= 2)
            continue;
        if (strstr(line, name) != nullptr)
        {
            memset(str1, 0x0, 32);
            memset(str2, 0x0, 32);

            sscanf(line, "%*s%s%*s%*s%*s%*s%*s%*s%*s%s", str1, str2);

            rtx->tx_bytes = atol(str2);
            rtx->rx_bytes = atol(str1);
        }
    }
    fclose(netDevFile);
    return true;
}

void rtx_bytes_copy(RTX_BYTES *dest, RTX_BYTES *src)
{
    dest->rx_bytes = src->rx_bytes;
    dest->tx_bytes = src->tx_bytes;
    dest->rtx_time = src->rtx_time;
    src->rx_bytes = 0;
    src->tx_bytes = 0;
}

bool get_network_speed(NET_INTERFACE *p_net)
{
    RTX_BYTES rtx0, rtx1;
    NET_INTERFACE *temp = p_net;
    while (temp != nullptr)
    {
        if(strcasecmp(temp->name, "lo") == 0 || strcasecmp(temp->name, "docker0") == 0)
        {
            temp = temp->next;
            continue;
        }
        if(!get_rtx_bytes(temp->name, &rtx0))
            return false;
        temp->rtx0_cnt.tx_bytes = rtx0.tx_bytes;
        temp->rtx0_cnt.rx_bytes = rtx0.rx_bytes;
        temp->rtx0_cnt.rtx_time = rtx0.rtx_time;
        temp = temp->next;
    }

    sleep(WAIT_SECOND);

    temp = p_net;
    while (temp != nullptr)
    {
        if(strcasecmp(temp->name, "lo") == 0 || strcasecmp(temp->name, "docker0") == 0)
        {
            temp = temp->next;
            continue;
        }
        if(!get_rtx_bytes(temp->name, &rtx1))
            return false;
        temp->rtx1_cnt.tx_bytes = rtx1.tx_bytes;
        temp->rtx1_cnt.rx_bytes = rtx1.rx_bytes;
        temp->rtx1_cnt.rtx_time = rtx1.rtx_time;
        temp = temp->next;
    }

    temp = p_net;
    while (temp != NULL)
    {
        if(strcasecmp(temp->name, "lo") == 0 || strcasecmp(temp->name, "docker0") == 0)
        {
            temp = temp->next;
            continue;
        }
        cal_netinterface_speed(
                    &temp->kUploadSpeed,
                    &temp->kDownSpeed,
                    (&temp->rtx0_cnt),
                    (&temp->rtx1_cnt)
                    );
        temp = temp->next;
    }
    return true;
}

void cal_netinterface_speed(
        double *kUploadSpeed,
        double *kDownSpeed,
        RTX_BYTES *rtx0,
        RTX_BYTES *rtx1
        )
{
    long int time_lapse;
    time_lapse = (rtx1->rtx_time.tv_sec * 1000 + rtx1->rtx_time.tv_usec / 1000) -
            (rtx0->rtx_time.tv_sec * 1000 + rtx0->rtx_time.tv_usec / 1000);

    *kDownSpeed = (rtx1->rx_bytes - rtx0->rx_bytes) * 1.0 /
            (1024 * time_lapse * 1.0 / 1000);
    *kUploadSpeed = (rtx1->tx_bytes - rtx0->tx_bytes) * 1.0 /
            (1024 * time_lapse * 1.0 / 1000);
}

bool get_disk_info(uint64_t& totalsize, uint64_t& freeDisk)
{
    struct statfs diskInfo;
    if(statfs("/",&diskInfo) != 0)
        return false;
    unsigned long blocksize = diskInfo.f_bsize;// 每个block里面包含的字节数
    totalsize = blocksize * diskInfo.f_blocks;//总的字节数
    totalsize >>= 20;// 1024*1024 =1MB  换算成MB单位

    freeDisk = diskInfo.f_bfree*blocksize; //再计算下剩余的空间大小
    freeDisk >>= 20;// 1024*1024 =1MB  换算成MB单位
    return true;
}

SysInfo::SysInfo() :
    m_running(false)
{

}

SysInfo::~SysInfo()
{
    StopMinitor();
}

void SysInfo::StartMinitor(SysInfoType sysInfoType,uint16_t secReadPeriod, SYS_MINITOR_CALLBACK callback)
{
    if(m_running)
        return;
    else
        StopMinitor();

    m_callback = callback;
    m_running = true;
    m_sysInfoType = sysInfoType;
    m_hostCoreThread = std::thread([=]
    {
        NET_INTERFACE* netInterface;
        int nums = 0;
        netInterface = (NET_INTERFACE *)malloc(sizeof(NET_INTERFACE));
        get_interface_info(netInterface, &nums);
        NET_INTERFACE *temp;
        while(m_running)
        {
            SysData sysData;
            sysData.sysInfoType = m_sysInfoType;
            if(m_sysInfoType & SysInfoType::cpu)
            {
                get_cpu_usage(&sysData.cpuPercent);
            }
            if(m_sysInfoType & SysInfoType::disk)
            {
                get_disk_info(sysData.mbDiskTotal, sysData.mbDiskFree);
            }
            if(m_sysInfoType & SysInfoType::memory)
            {
                get_mem_usage(sysData.mbMemoryTotal, sysData.mbMemoryFree);
            }
            if(m_sysInfoType & SysInfoType::net)
            {
                if(get_network_speed(netInterface))
                {
                    temp = netInterface;
                    while (temp != nullptr)
                    {
                        sysData.kUploadNetSpeed += temp->kUploadSpeed;
                        sysData.kDownNetSpeed += temp->kDownSpeed;
                        temp = temp->next;
                    }
                }
            }
            callback(sysData);
            sleep(secReadPeriod);
        }

        NET_INTERFACE* pos;
        temp = netInterface;
        while (temp != nullptr)
        {
            pos = temp->next;
            free(temp);
            temp = pos;
        }
    });
}

void SysInfo::StopMinitor()
{
    m_running = false;
    if(m_hostCoreThread.joinable())
        m_hostCoreThread.join();
}

bool SysInfo::IsMonitoring()
{
    return m_running;
}

float SysInfo::GetCPU(unsigned pid)
{
    return get_proc_cpu(pid);
}

unsigned SysInfo::GetMemory(unsigned pid)
{
    return get_proc_mem(pid);
}

unsigned SysInfo::GetVirtualMemory(unsigned pid)
{
    return get_proc_virtualmem(pid);
}
