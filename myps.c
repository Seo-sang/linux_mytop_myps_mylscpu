#include <stdio.h>
#include <stdlib.h>
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
	bool e; //커널 프로세스를 제외한 모든 프로세스 출력
	bool f; //풀 포맷으로 출력
	//bool l; //긴 포맷으로 출력
	bool p; //특정 PID 지정
	bool r; //현재 실행중인 process 출력
	bool u; //프로세스 소유자 기준으로 출력
	bool x; //데몬 프로세스처럼 터미널에 종속되지 않는 프로세스 출력
	bool bar; //'-'가 있는지 여부
} opt;

typedef struct {
	//int F;
	//int S;
	char USER[MAX]; //*
	ll UID; //*
	ll PID; //*
	ll PPID; //*
	//int C;
	char PRI[MAX]; //*
	char NI[MAX]; //*
	//char ADDR[MAX];
	//ll SZ;
	//char WCHAN[MAX];
	double CPU; //*
	double MEM; //*
	ll VSZ; //*
	ll RSS; //*
	char TTY[MAX]; //*
	char STAT[MAX]; //*
	ll START; //*
	ll TIME; //*
	char COMMAND[MAX]; //*
	char CMD[MAX]; //*
	int ttyNr; //*
} proc;


//data들
int tasks, run, slp, stop, zombie;
double memtotal;
opt option; //option들
proc procs[MAX]; //process 정보들

ll get_value(const char* str) { //string으로부터 정수값 얻기
	ll ret = 0;
	for(int i = 0; i < MAX; i++)
		if(isdigit(str[i]))
			ret = ret *10 + ((ll) str[i] - '0');

	return ret;
}

double round(double a) { //소수 첫째자리에서 반올림하는 함수
	int tmp = a *10;
	double ret = (double)tmp / 10;

	return ret;
}

ll get_uptime() { //Uptime을 초단위로 바꾸는 함수
	char buffer[MAX];
	memset(buffer, 0, sizeof(buffer));
	
	int fd;
	if((fd = open("/proc/uptime", O_RDONLY)) < 0) {
		fprintf(stderr, "/proc/uptime file open error\n");
		exit(1);
	}

	read(fd, buffer, MAX);
	ll ret;

	int idx = 0;

	while(buffer[idx] != ' ') idx++;
	
	memset(buffer + idx, 0, sizeof(char) * (MAX - idx));

	ret = atoll(buffer);
	close(fd);

	return ret;
}

void get_mem_info() {
	int fd;

	if((fd = open("/proc/meminfo", O_RDONLY)) < 0) { //proc/meminfo 파일 열기
		fprintf(stderr, "/proc/meminfo file open error\n");
		exit(1);
	}
	char mem_tmp[MAX]; //파일로부터 읽어올 버퍼
	memset(mem_tmp, 0, MAX);
	if(read(fd, mem_tmp, MAX) == 0) { //proc/meminfo 파일 읽기
		fprintf(stderr, "/proc/meminfo file read error\n");
		exit(1);
	}

	close(fd);

	char mem[MAX][MAX];
	memset(mem, 0, sizeof(mem));
	int i = 0;
	char *ptr = strtok(mem_tmp, "\n"); //읽은 정보 파싱
	while(ptr != NULL) {
		strcpy(mem[i++], ptr);
		ptr = strtok(NULL, "\n"); //개행으로 정보 파싱
	}

	//파싱한 정보 int형으로 바꾸기
	int mtotal = get_value(mem[0]);

	//memtotal 얻기
	memtotal = (double)mtotal / 1024;
}

ll get_VmLck(int pid) {
	char path[MAX];
	char tmp[MAX];
	char status[MAX][MAX];
	memset(path, 0, MAX);
	memset(tmp, 0, MAX);
	memset(status, 0, sizeof(status));
	sprintf(path, "/proc/%d/status", pid);
	
	ll ret = 0;

	int fd;
	if((fd = open(path, O_RDONLY)) < 0) {
		fprintf(stderr, "%s file open errorn\n", path);
		exit(1);
	}
	if(read(fd, tmp, MAX) == 0) {
		fprintf(stderr, "%s file read error\n", path);
		exit(1);
	}
	close(fd);

	int i = 0;
	char *ptr = strtok(tmp, "\n"); //개행으로 정보 파싱
	while(ptr != NULL) {
		strcpy(status[i++], ptr);
		ptr = strtok(NULL, "\n");
	}
	
	for(int j = 0; j < i; j++) {
		if(!strncmp(status[j], "VmLck", 5)) { //VmLck를 찾은 경우
			ll ret = get_value(status[j]);
			break;
		}
	}

	return ret;
}


void get_tty(const char* path, int index) {
	/*
	if(access(path, F_OK) < 0) { //proc/pid/fd/0이 없을 경우
		DIR *dp;
		struct dirent *dentry;
		if((dp = opendir("/dev")) < 0) {
			fpritnf(stderr, "/dev directory open error\n");
			exit(1);
		}
		
		char devpath[MAX];
		while((dentry = readdir(dp)) != NULL) { //dev 디렉토리 읽기
			memset(devpath, 0, MAX);
			sprintf(devpath, "/dev/%s", dentry->d_name);

			struct stat statbuf;
			if(stat(devpath, &statbuf) < 0) {
				fprintf(stderr, "stat error\n");
				exit(1);
			}


		}
	}*/
	if(procs[index].ttyNr == 0) {
		strcpy(procs[index].TTY, "?");
	}
	else {
		int MAJOR = major(procs[index].ttyNr);
		int MINOR = minor(procs[index].ttyNr);
		char link_path[MAX];
		char link[MAX];
		memset(link_path, 0, MAX);
		memset(link, 0, MAX);
		sprintf(link_path, "/dev/char/%d:   -%d", MAJOR, MINOR);

		if(access(link_path, F_OK) != 0) {
			sprintf(procs[index].TTY, "pts/%d", MINOR);
		}
		else {
			readlink(link_path, link, MAX);
			strcpy(procs[index].TTY, link +3);
		}
	}
}


void get_proc_stat(const char* path, int index) {
	
	//USER읽기
	struct stat statbuf;
	stat(path, &statbuf);
	struct passwd *upasswd = getpwuid(statbuf.st_uid);
	strcpy(procs[index].USER, upasswd->pw_name);
	//UID읽기
	procs[index].UID = upasswd->pw_uid;

	int fd;
	char stat_tmp[MAX];
	char stat[MAX][MAX];
	memset(stat_tmp, 0, MAX);
	memset(stat, 0, sizeof(stat));

	if((fd = open(path, O_RDONLY)) < 0) {
		fprintf(stderr, "/proc/pid/stat file open error\n");
		exit(1);
	}
	if(read(fd, stat_tmp, MAX) == 0) {
		fprintf(stderr, "/proc/pid/stat file read error\n");
		exit(1);
	}
	close(fd);

	char *ptr = strtok(stat_tmp, " "); //공백 기준으로 token 자르기
	int i = 0;
	while(ptr != NULL) {
		strcpy(stat[i++], ptr);
		ptr = strtok(NULL, " ");
	}

	//PPID구하기
	procs[index].CPU = atoll(stat[3]);

	//CPU구하기
	ll utime = atoll(stat[13]);
	ll stime = atoll(stat[14]);
	ll uptime = get_uptime();
	int hertz = (int)sysconf(_SC_CLK_TCK);

	double tic = (double)(utime + stime) /hertz;
	procs[index].CPU = (double)tic / uptime * 100;
	procs[index].CPU = round(procs[index].CPU);
	
	// START구하기
	procs[index].START = atoll(stat[21]);

	//TIME구하기
	procs[index].TIME = (double)(utime + stime) / ((double)hertz / 100);

	//CMD구하기
	i = 0;
	while(stat[1][i+1] != ')') {
		procs[index].CMD[i] = stat[1][i+1];
		i++;
	}

	//STAT구하기
	char stat_str[MAX];
	memset(stat_str, 0, MAX);
	strcpy(stat_str, stat[2]);

	if(procs[index].PID == atoi(stat[5]))
		strcat(stat_str, "s");

	if(atoi(stat[18]) > 0)
		strcat(stat_str, "N");
	else if(atoi(stat[18]) < 0)
		strcat(stat_str, "<");

	if(get_VmLck(procs[index].PID) != 0)
		strcat(stat_str, "L");
	
	if(atoi(stat[19]) > 1)
		strcat(stat_str, "l");

	if(getpgid(procs[index].PID) == atoi(stat[7]))
		strcat(stat_str, "+");

	strcpy(procs[index].STAT, stat_str);

	//NI저장
	procs[index].NI = atoi(stats[18]);
	
	//PRI저장
	strncpy(procs[index].PRI, stats[17], 3);

	//START구하기
	procs[index].START = time(NULL) - (uptime - (atoll(stat[21]) / hertz));

	//ttyNr구하기
	procs[index].ttyNr = atoi(stat[6]);
}

//proc/pid/status 파일 읽기
void get_proc_status(const char* path, int index) {
	int fd;
	char status_tmp[MAX];
	char status[MAX][MAX];
	memset(status_tmp, 0, MAX);
	memset(status, 0, sizeof(status));

	if((fd = open(path, O_RDONLY)) < 0) {
		fprintf(stderr, "%s file open error\n", path);
		exit(1);
	}

	if(read(fd, status_tmp, MAX) == 0) {
		fprintf(stderr, "%s file read error\n", path);
		exit(1);
	}
	close(fd);

	char *ptr = strtok(status_tmp, "\n");
	int i = 0;
	while(ptr != NULL) {
		strcpy(status[i++], ptr);
		ptr = strtok(NULL, "\n");
	}
	
	ll res = get_value(status[20]);
	procs[index].MEM = (double)res / (memtotal * 1024) * 100;
	procs[index].VSZ = get_value(status[17]);
	procs[index].RSS = get_value(status[21]);
}

//proc/pid/cmdline 파일 읽기
void get_proc_cmdline(const char* path, int index) {
	int fd;
	if((fd = open(path, O_RDONLY)) < 0) {
		fprintf(stderr, "/proc/pid/cmdline file open error\n");
		exit(1);
	}
	if(read(fd, procs[index].COMMAND, MAX) < 0) {
		fprintf(stderr, "/proc/pid/cmdline file read error\n");
		exit(1);
	}
	close(fd);
}


void get_procs() { //pid를 확인하고 process 정보들을 가져오는 함수

	DIR *proc_dir; //proc디렉토리 포인터
	struct dirent *dp; //proc디렉토리 엔트리 포인터

	if((proc_dir = opendir("/proc")) == NULL) { //proc directory open
		fprintf(stderr, "/proc open error\n");
		exit(1);
	}

	while((dp = readdir(proc_dir)) != NULL) { //하위 파일들을 하나씩 읽는다.
		 if(isdigit(dp->d_name[0])) { //하위폴더가 숫자일 경우(process 폴더인 경우)
			 procs[tasks++].PID = atoi(dp->d_name); //pid저장
		 }
	}

	char stat_path[MAX];
	char status_path[MAX];
	char cmdline_path[MAX];
	for(int i = 0; i < tasks; i++) {
		memset(stat_path, 0, MAX);
		memset(status_path, 0, MAX);
		memset(cmdline_path, 0, MAX);
		sprintf(stat_path, "/proc/%lld/stat", procs[i].PID);
		sprintf(status_path, "/proc/%lld/status", procs[i].PID);
		sprintf(cmdline_path, "/proc/%lld/cmdline", procs[i].PID);
		//예외처리 파일이 존재할 때 열기
		if(access(stat_path, F_OK) == 0) {
			get_proc_stat(stat_path, i);
		}
		if(access(status_path, F_OK) == 0) {
			get_proc_status(status_path, i);
		}
		if(access(status_path, F_OK) == 0) {
			get_proc_cmdline(cmdline_path, i);
		}
	}

	closedir(proc_dir);
}

void print_data() { //데이터들을 불러오는 함수
	get_mem_info();
	get_procs();
	
	//첫 줄 출력
	/*
	if(option.l)
		printf("F");
	if((option.l && option.bar))
		printf(" S");
		*/
	if(option.u)
		printf("USER    ");
	if((option.bar && option.f))
		printf("    UID");
	printf("    PID");
	if(option.bar && option.f))
		printf("    PPID");
	/*
	if((option.bar && option.f))
		printf("  C");
		*/
	if(!option.u &&(option.bar && option.f))
		printf(" STIME");
	/*
	if(option.l)
		printf(" PRI");
	if(option.l)
		printf("  NI");
	*/
	if(option.u)
		printf(" %%CPU");
	if(option.u)
		printf(" %%MEM");
	if(option.u)
		printf("    VSZ");
	if(option.u)
		printf("    RSS");
	/*
	if((option.l && option.bar))
		printf(" ADDR");
	if((option.l && option.bar))
		printf(" SZ");
	if(option.l)
		printf(" WCHAN");
		*/
	if(option.u || option.r || option.x)
		printf("      STAT");
	printf(" TTY");
	if(option.u)
		printf(" START");
	printf("          TIME");
	if(option.r || option.u || option.x || !option.bar || option.e || option.f)
		printf(" COMMAND\n");
	else
		printf(" CMD\n");
	
}

int main(int argc, char** argv) {

	for(int i = 1; i < argc; i++) { //옵션 설정
		int idx = 0;
		while(argv[i][idx] != '\0') {
			if(argv[i][idx] == '-') option.bar = TRUE;
			if(argv[i][idx] == 'a') option.a = TRUE;
			if(argv[i][idx] == 'e') option.e = TRUE;
			if(argv[i][idx] == 'f') option.f = TRUE;
			if(argv[i][idx] == 'p') option.p = TRUE;
			if(argv[i][idx] == 'r') option.r = TRUE;
			if(argv[i][idx] == 'u') option.u = TRUE;
			if(argv[i][idx] == 'x') option.x = TRUE;
			idx++;
		}
	}
	
	print_data();
}
