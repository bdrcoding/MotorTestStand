/* Cross-platform serial logger with Windows GUI.
 * Windows: x86_64-w64-mingw32-gcc -O2 -Wall -Wextra serial_logger.c -o serial_logger.exe -mwindows -lcomctl32
 * Linux:   gcc -O2 -Wall -Wextra serial_logger.c -o serial_logger
 * Serial:  115200 8-N-1
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <sys/time.h>
#endif

#define LINE_BUFFER_SIZE 1024

#ifdef _WIN32
#define WM_LOG_SAMPLE   (WM_APP+1)
#define WM_LOG_FINISHED (WM_APP+2)
#define IDC_PORT 1001
#define IDC_REFRESH 1002
#define IDC_START 1003
#define IDC_STOP 1004
#define IDC_STATUS 1005
#define IDC_SPEED 1006
#define IDC_CURRENT 1007
#define IDC_VOLTAGE 1008
#define IDC_LOAD 1009
#define IDC_SAMPLES 1010
#define IDC_ELAPSED 1011
#define IDC_CSV 1012

typedef struct { float speed,current,voltage,load; } Sample;
static HWND hwnd_main, combo_port, btn_refresh, btn_start, btn_stop;
static HWND lab_status,lab_speed,lab_current,lab_voltage,lab_load,lab_samples,lab_elapsed,lab_csv;
static HANDLE logger_thread=NULL;
static volatile LONG logger_running=0;
static unsigned long long logger_start_ms=0, sample_count=0;
static HFONT gui_font=NULL;

static void timestamp(char *b,size_t n){ SYSTEMTIME s; GetLocalTime(&s); snprintf(b,n,"%04d-%02d-%02d %02d:%02d:%02d.%03d",s.wYear,s.wMonth,s.wDay,s.wHour,s.wMinute,s.wSecond,s.wMilliseconds); }
static void csv_name(char *b,size_t n){ SYSTEMTIME s; GetLocalTime(&s); snprintf(b,n,"serial_log_%04d-%02d-%02d_%02d-%02d-%02d.csv",s.wYear,s.wMonth,s.wDay,s.wHour,s.wMinute,s.wSecond); }
static int parse_telemetry(const char *s,float *sp,float *cur,float *vol,float *load){ float ignored; return sscanf(s," T:%f S:%f I:%f V:%f L:%f",&ignored,sp,cur,vol,load)==5; }

static int open_serial(HANDLE *h,const char *port){
    char dev[64]; snprintf(dev,sizeof(dev),"%s%s",strncmp(port,"\\\\.\\",4)==0?"":"\\\\.\\",port);
    *h=CreateFileA(dev,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
    if(*h==INVALID_HANDLE_VALUE)return 0;
    DCB d={0}; d.DCBlength=sizeof(d);
    if(!GetCommState(*h,&d)){CloseHandle(*h);*h=INVALID_HANDLE_VALUE;return 0;}
    d.BaudRate=CBR_115200; d.ByteSize=8; d.Parity=NOPARITY; d.StopBits=ONESTOPBIT;
    d.fBinary=TRUE; d.fParity=FALSE;
    d.fOutxCtsFlow=FALSE; d.fOutxDsrFlow=FALSE; d.fDsrSensitivity=FALSE; d.fRtsControl=RTS_CONTROL_DISABLE;
    d.fOutX=FALSE; d.fInX=FALSE; d.fTXContinueOnXoff=TRUE; d.fDtrControl=DTR_CONTROL_DISABLE;
    d.fErrorChar=FALSE; d.fNull=FALSE; d.fAbortOnError=FALSE;
    if(!SetCommState(*h,&d)){CloseHandle(*h);*h=INVALID_HANDLE_VALUE;return 0;}
    EscapeCommFunction(*h,CLRDTR); EscapeCommFunction(*h,CLRRTS);
    COMMTIMEOUTS t={0}; t.ReadIntervalTimeout=MAXDWORD; t.ReadTotalTimeoutMultiplier=0; t.ReadTotalTimeoutConstant=50; t.WriteTotalTimeoutConstant=50;
    if(!SetCommTimeouts(*h,&t)){CloseHandle(*h);*h=INVALID_HANDLE_VALUE;return 0;}
    SetupComm(*h,4096,4096); PurgeComm(*h,PURGE_RXCLEAR|PURGE_TXCLEAR);
    return 1;
}
static int read_byte(HANDLE h,unsigned char *b){ DWORD n=0; if(!ReadFile(h,b,1,&n,NULL)){DWORD e=GetLastError(); if(e==ERROR_OPERATION_ABORTED)return 0; return -1;} return n?1:0; }

static int make_logs_dir(char *out,size_t n){
    char exe[MAX_PATH]; DWORD len=GetModuleFileNameA(NULL,exe,sizeof(exe)); if(!len||len>=sizeof(exe))return 0;
    char *p=strrchr(exe,'\\'); if(!p)return 0; *p='\0'; snprintf(out,n,"%s\\Logs",exe);
    if (CreateDirectoryA(out, NULL) || GetLastError() == ERROR_ALREADY_EXISTS) {
        return 1;
    }
    return 0;
}
static void setfont(HWND h){if(gui_font)SendMessageA(h,WM_SETFONT,(WPARAM)gui_font,TRUE);}
static HWND label(const char *s,int x,int y,int w,int h){HWND q=CreateWindowA("STATIC",s,WS_VISIBLE|WS_CHILD,x,y,w,h,hwnd_main,NULL,GetModuleHandleA(NULL),NULL);setfont(q);return q;}

static void refresh_ports(void){
    char old[64]={0}; int idx=(int)SendMessageA(combo_port,CB_GETCURSEL,0,0);
    if(idx!=CB_ERR)SendMessageA(combo_port,CB_GETLBTEXT,idx,(LPARAM)old);
    SendMessageA(combo_port,CB_RESETCONTENT,0,0);
    for(int n=1;n<=256;n++){
        char name[32],dev[64];snprintf(name,sizeof(name),"COM%d",n);snprintf(dev,sizeof(dev),"\\\\.\\%s",name);
        HANDLE h=CreateFileA(dev,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
        DWORD e=GetLastError(); if(h!=INVALID_HANDLE_VALUE){CloseHandle(h);e=0;}
        if(!e||e==ERROR_ACCESS_DENIED)SendMessageA(combo_port,CB_ADDSTRING,0,(LPARAM)name);
    }
    int count=(int)SendMessageA(combo_port,CB_GETCOUNT,0,0),pick=0;
    for(int i=0;i<count;i++){char s[64];SendMessageA(combo_port,CB_GETLBTEXT,i,(LPARAM)s);if(!strcmp(s,old)){pick=i;break;}}
    if(count)SendMessageA(combo_port,CB_SETCURSEL,pick,0);
    SetWindowTextA(lab_status,count?"Ready":"No COM ports found");
}
static void controls(int logging){EnableWindow(combo_port,!logging);EnableWindow(btn_refresh,!logging);EnableWindow(btn_start,!logging);EnableWindow(btn_stop,logging);}

static DWORD WINAPI logger_thread_proc(LPVOID arg){
    char port[64];strncpy(port,(char*)arg,sizeof(port)-1);port[sizeof(port)-1]='\0';free(arg);
    HANDLE serial=INVALID_HANDLE_VALUE; if(!open_serial(&serial,port)){PostMessageA(hwnd_main,WM_LOG_FINISHED,1,0);return 0;}
    char dir[MAX_PATH],name[256],path[MAX_PATH]; if(!make_logs_dir(dir,sizeof(dir))){CloseHandle(serial);PostMessageA(hwnd_main,WM_LOG_FINISHED,2,0);return 0;}
    csv_name(name,sizeof(name));snprintf(path,sizeof(path),"%s\\%s",dir,name);
    FILE *csv=fopen(path,"w"); if(!csv){CloseHandle(serial);PostMessageA(hwnd_main,WM_LOG_FINISHED,3,0);return 0;}
    fprintf(csv,"Timestamp,Speed,Current,Voltage,Load\n");fflush(csv);
    logger_start_ms=GetTickCount();sample_count=0;InterlockedExchange(&logger_running,1);PostMessageA(hwnd_main,WM_LOG_SAMPLE,0,0);
    char line[LINE_BUFFER_SIZE];size_t len=0;
    while(InterlockedCompareExchange(&logger_running,0,0)){
        unsigned char b;int r=read_byte(serial,&b);if(r<0)break;if(!r)continue;
        if(b=='\n'){
            line[len]='\0';if(len&&line[len-1]=='\r')line[len-1]='\0';
            float sp,cur,vol,load;if(parse_telemetry(line,&sp,&cur,&vol,&load)){
                char ts[64];timestamp(ts,sizeof(ts));fprintf(csv,"%s,%.3f,%.3f,%.3f,%.3f\n",ts,sp,cur,vol,load);fflush(csv);
                Sample *s=(Sample*)malloc(sizeof(*s));if(s){s->speed=sp;s->current=cur;s->voltage=vol;s->load=load;sample_count++;PostMessageA(hwnd_main,WM_LOG_SAMPLE,(WPARAM)s,1);}
            }len=0;
        }else if(len<LINE_BUFFER_SIZE-1)line[len++]=(char)b;else len=0;
    }
    fclose(csv);CloseHandle(serial);InterlockedExchange(&logger_running,0);
    char *final=(char*)malloc(strlen(path)+1);if(final)strcpy(final,path);PostMessageA(hwnd_main,WM_LOG_FINISHED,0,(LPARAM)final);return 0;
}

static void start_logging(void){
    if(InterlockedCompareExchange(&logger_running,0,0))return;
    int idx=(int)SendMessageA(combo_port,CB_GETCURSEL,0,0);if(idx==CB_ERR){MessageBoxA(hwnd_main,"Please select a COM port first.","No COM Port",MB_OK|MB_ICONWARNING);return;}
    char *port=(char*)malloc(64);if(!port)return;SendMessageA(combo_port,CB_GETLBTEXT,idx,(LPARAM)port);
    sample_count=0;SetWindowTextA(lab_status,"Starting...");SetWindowTextA(lab_speed,"Speed: ---");SetWindowTextA(lab_current,"Current: ---");SetWindowTextA(lab_voltage,"Voltage: ---");SetWindowTextA(lab_load,"Load: ---");SetWindowTextA(lab_samples,"Samples: 0");SetWindowTextA(lab_elapsed,"Elapsed: 00:00:00");SetWindowTextA(lab_csv,"CSV: Logs\\...");controls(1);
    logger_thread=CreateThread(NULL,0,logger_thread_proc,port,0,NULL);if(!logger_thread){free(port);controls(0);SetWindowTextA(lab_status,"Failed to start");MessageBoxA(hwnd_main,"Could not start the logging thread.","Error",MB_OK|MB_ICONERROR);}
}
static void stop_logging(void){
    if(!logger_thread)return;InterlockedExchange(&logger_running,0);WaitForSingleObject(logger_thread,INFINITE);CloseHandle(logger_thread);logger_thread=NULL;controls(0);SetWindowTextA(lab_status,"Stopped");
}
static void update_elapsed(void){
    if (!InterlockedCompareExchange(&logger_running, 0, 0)) {
        return;
    }
    unsigned long long sec = (GetTickCount() - logger_start_ms) / 1000ULL;
    char s[64];
    snprintf(s, sizeof(s), "Elapsed: %02llu:%02llu:%02llu",
             sec / 3600, (sec / 60) % 60, sec % 60);
    SetWindowTextA(lab_elapsed, s);
}

static LRESULT CALLBACK wndproc(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_CREATE:
        gui_font=(HFONT)GetStockObject(DEFAULT_GUI_FONT);
        label("COM Port:",20,20,75,24);
        combo_port=CreateWindowA("COMBOBOX","",WS_VISIBLE|WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,95,17,150,200,h,(HMENU)IDC_PORT,GetModuleHandleA(NULL),NULL);setfont(combo_port);
        btn_refresh=CreateWindowA("BUTTON","Refresh Ports",WS_VISIBLE|WS_CHILD|WS_TABSTOP,255,17,120,28,h,(HMENU)IDC_REFRESH,GetModuleHandleA(NULL),NULL);setfont(btn_refresh);
        btn_start=CreateWindowA("BUTTON","START LOGGING",WS_VISIBLE|WS_CHILD|WS_TABSTOP,20,65,170,40,h,(HMENU)IDC_START,GetModuleHandleA(NULL),NULL);setfont(btn_start);
        btn_stop=CreateWindowA("BUTTON","STOP",WS_VISIBLE|WS_CHILD|WS_TABSTOP|WS_DISABLED,205,65,170,40,h,(HMENU)IDC_STOP,GetModuleHandleA(NULL),NULL);setfont(btn_stop);
        label("Status:",20,125,75,24);lab_status=label("Ready",95,125,280,24);
        label("Speed:",20,165,75,24);lab_speed=label("Speed: ---",95,165,130,24);label("Voltage:",205,165,75,24);lab_voltage=label("Voltage: ---",280,165,100,24);
        label("Current:",20,200,75,24);lab_current=label("Current: ---",95,200,130,24);label("Load:",205,200,75,24);lab_load=label("Load: ---",280,200,100,24);
        lab_samples=label("Samples: 0",20,245,170,24);lab_elapsed=label("Elapsed: 00:00:00",205,245,175,24);lab_csv=label("CSV: Logs\\...",20,280,370,40);
        refresh_ports();SetTimer(h,1,500,NULL);return 0;
    case WM_COMMAND:
        if(LOWORD(w)==IDC_REFRESH&&HIWORD(w)==BN_CLICKED){refresh_ports();return 0;}
        if(LOWORD(w)==IDC_START&&HIWORD(w)==BN_CLICKED){start_logging();return 0;}
        if(LOWORD(w)==IDC_STOP&&HIWORD(w)==BN_CLICKED){stop_logging();return 0;}return 0;
    case WM_TIMER:if(w==1)update_elapsed();return 0;
    case WM_LOG_SAMPLE:
        if(l==1&&w){Sample*s=(Sample*)w;char t[64];snprintf(t,sizeof(t),"Speed: %.3f",s->speed);SetWindowTextA(lab_speed,t);snprintf(t,sizeof(t),"Current: %.3f",s->current);SetWindowTextA(lab_current,t);snprintf(t,sizeof(t),"Voltage: %.3f",s->voltage);SetWindowTextA(lab_voltage,t);snprintf(t,sizeof(t),"Load: %.3f",s->load);SetWindowTextA(lab_load,t);snprintf(t,sizeof(t),"Samples: %llu",sample_count);SetWindowTextA(lab_samples,t);SetWindowTextA(lab_status,"Logging");free(s);}else SetWindowTextA(lab_status,"Logging");return 0;
    case WM_LOG_FINISHED:{char *p=(char*)l;controls(0);if(logger_thread){CloseHandle(logger_thread);logger_thread=NULL;}if(w==1){SetWindowTextA(lab_status,"Could not open COM port");MessageBoxA(h,"Could not open the selected COM port.\n\nMake sure no other serial monitor is using it.","Serial Port Error",MB_OK|MB_ICONERROR);}else if(w==2){SetWindowTextA(lab_status,"Could not create Logs folder");MessageBoxA(h,"Could not create the Logs folder beside the executable.","Logging Error",MB_OK|MB_ICONERROR);}else if(w==3){SetWindowTextA(lab_status,"Could not create CSV");MessageBoxA(h,"Could not create the CSV file.","Logging Error",MB_OK|MB_ICONERROR);}else{SetWindowTextA(lab_status,"Logging stopped");if(p){char t[MAX_PATH+8];snprintf(t,sizeof(t),"CSV: %s",p);SetWindowTextA(lab_csv,t);free(p);}}return 0;}
    case WM_CLOSE:if(logger_thread)stop_logging();DestroyWindow(h);return 0;
    case WM_DESTROY:KillTimer(h,1);PostQuitMessage(0);return 0;
    }
    return DefWindowProcA(h,m,w,l);
}
int WINAPI WinMain(HINSTANCE inst,HINSTANCE prev,LPSTR cmd,int show){(void)prev;(void)cmd;INITCOMMONCONTROLSEX ic={sizeof(ic),ICC_STANDARD_CLASSES};InitCommonControlsEx(&ic);WNDCLASSA wc={0};wc.lpfnWndProc=wndproc;wc.hInstance=inst;wc.lpszClassName="ThrustStandLogger";wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);if(!RegisterClassA(&wc))return 1;hwnd_main=CreateWindowA("ThrustStandLogger","Thrust Stand Logger",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,420,370,NULL,NULL,inst,NULL);if(!hwnd_main)return 1;ShowWindow(hwnd_main,show);UpdateWindow(hwnd_main);MSG msg;while(GetMessageA(&msg,NULL,0,0)>0){TranslateMessage(&msg);DispatchMessageA(&msg);}return (int)msg.wParam;}

#else
/* Linux command-line implementation */
static volatile int running=1;static int serial_fd=-1;
static void signal_handler(int s){(void)s;running=0;}
static void timestamp(char*b,size_t n){time_t now=time(NULL);struct tm t;localtime_r(&now,&t);struct timeval tv;gettimeofday(&tv,NULL);snprintf(b,n,"%04d-%02d-%02d %02d:%02d:%02d.%03ld",t.tm_year+1900,t.tm_mon+1,t.tm_mday,t.tm_hour,t.tm_min,t.tm_sec,tv.tv_usec/1000);}
static void csv_name(char*b,size_t n){time_t now=time(NULL);struct tm t;localtime_r(&now,&t);snprintf(b,n,"serial_log_%04d-%02d-%02d_%02d-%02d-%02d.csv",t.tm_year+1900,t.tm_mon+1,t.tm_mday,t.tm_hour,t.tm_min,t.tm_sec);}
static int open_linux(const char*p){serial_fd=open(p,O_RDWR|O_NOCTTY);if(serial_fd<0){fprintf(stderr,"ERROR: %s\n",strerror(errno));return 0;}struct termios tty={0};if(tcgetattr(serial_fd,&tty)){close(serial_fd);serial_fd=-1;return 0;}cfmakeraw(&tty);cfsetispeed(&tty,B115200);cfsetospeed(&tty,B115200);tty.c_cflag&=~CSIZE;tty.c_cflag|=CS8;tty.c_cflag&=~PARENB;tty.c_cflag&=~CSTOPB;tty.c_cflag&=~CRTSCTS;tty.c_cflag|=CREAD|CLOCAL;tty.c_cc[VMIN]=0;tty.c_cc[VTIME]=1;if(tcsetattr(serial_fd,TCSANOW,&tty)){close(serial_fd);serial_fd=-1;return 0;}tcflush(serial_fd,TCIOFLUSH);return 1;}
int main(int argc,char**argv){const char*port=NULL;for(int i=1;i<argc;i++){if(!strcmp(argv[i],"-port")||!strcmp(argv[i],"--port")){if(i+1>=argc)return 1;port=argv[++i];}else if(!strcmp(argv[i],"-h")||!strcmp(argv[i],"--help")){printf("Usage: %s -port PORT\n",argv[0]);return 0;}else return 1;}if(!port){fprintf(stderr,"Usage: %s -port /dev/ttyUSB1\n",argv[0]);return 1;}signal(SIGINT,signal_handler);signal(SIGTERM,signal_handler);printf("Opening serial port: %s\n",port);if(!open_linux(port))return 1;printf("Serial port opened successfully.\nSerial settings: 115200 8-N-1\nWaiting for telemetry...\n");char fn[256];csv_name(fn,sizeof(fn));FILE*csv=fopen(fn,"w");if(!csv){close(serial_fd);return 1;}fprintf(csv,"Timestamp,Speed,Current,Voltage,Load\n");fflush(csv);printf("Logging to: %s\nPress Ctrl+C to stop.\n\n",fn);char line[LINE_BUFFER_SIZE];size_t len=0;while(running){unsigned char b;ssize_t r=read(serial_fd,&b,1);if(r<0){if(errno==EINTR)continue;break;}if(!r)continue;if(b=='\n'){line[len]='\0';if(len&&line[len-1]=='\r')line[len-1]='\0';float sp,cur,vol,load,ignored;if(sscanf(line," T:%f S:%f I:%f V:%f L:%f",&ignored,&sp,&cur,&vol,&load)==5){char ts[64];timestamp(ts,sizeof(ts));fprintf(csv,"%s,%.3f,%.3f,%.3f,%.3f\n",ts,sp,cur,vol,load);fflush(csv);printf("%s | Speed: %.3f | Current: %.3f | Voltage: %.3f | Load: %.3f\n",ts,sp,cur,vol,load);}len=0;}else if(len<LINE_BUFFER_SIZE-1)line[len++]=(char)b;else len=0;}fclose(csv);close(serial_fd);printf("\nStopping logger...\nCSV saved to: %s\n",fn);return 0;}
#endif