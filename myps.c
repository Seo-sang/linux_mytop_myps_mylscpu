#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>
#include <pwd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <signal.h>
#include <utmp.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <curses.h>
#include <stdbool.h>
#include <math.h>
#include <limits.h>


#define MAX 1024
#define ll long long

typedef struct { //option들 설정
	bool a; //모든 process 출력
	bool e; //커널 ㅍ ㅡ로세스를 제외한 모든 프로세스 출력
	bool f; //풀 포맷으로 출력
	bool l; //긴 포맷으로 출력
	bool p; //특정 PID 지정
	bool r; //현재 실행중인 process 출력
	bool u; //프로세스 소유자 기준으로 출력
	bool x; //데몬 프로세스처럼 터미널에 종속되지 않는 프로세스 출력
} opt;

typedef struct {
	int F;
	int S;
	char USER[MAX];
	int UUID;
	int PID;
	int PPID;
	int C;
	int PRI;
	char NI[MAX];
	char ADDR[MAX];
	ll SZ;
	char WCHAN[MAX];
	double CPU;
	double MEM;
	int VSZ;
	int RSS;
	char TTY[MAX];
	char STAT[MAX];
	ll START;
	ll TIME;
	char COMMAND[MAX];
} proc;

opt option; //option들
proc procs[MAX]; //process 정보들

int main(int argc, char argv[][]) {

	for(int i = 1; i < argc; i++) { //옵션 설정
		int idx = 0;
		while(argv[i][idx] != '\0') {
			if(argv[i][idx] == 'a') option.a = TRUE;
			if(argv[i][idx] == 'e') option.e = TRUE;
			if(argv[i][idx] == 'f') option.f = TRUE;
			if(argv[i][idx] == 'l') option.l = TRUE;
			if(argv[i][idx] == 'p') option.p = TRUE;
			if(argv[i][idx] == 'r') option.r = TRUE;
			if(argv[i][idx] == 'u') option.u = TRUE;
			if(argv[i][idx] == 'x') option.x = TRUE;
		}
	}


}
