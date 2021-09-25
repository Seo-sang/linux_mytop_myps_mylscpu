#include <stdio.h>
#include <time.h>
#include <stdlib.h>
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
#include <ncurses.h>
#include <curses.h>
#include <stdbool.h>
#include <math.h>
#include <limits.h>

#define MAX 1024

struct tm *t;

//헤더 정보들
int tasks, run, slp, stop, zombie;
double us, sy, ni, id, wa, hi, si, st;
double memtotal, memfree, memused, membuff_cache;
double swaptotal, swapfree, swapused, swapavail_mem;


//옵션 변수
int start_row = 0, start_col = 0;
int begin[MAX] = {0, 8, 17, 21, 25, 33, 40, 47, 48, 53, 59, 69};
int option; //정렬 옵션
int uptime_line; //uptime_line 표시 여부
double delay; //refresh 초단위
int option_c; //명령 인자 표시/ 비표시
char string[MAX];
int option_i; //유휴 프로세스 표시하지 않는 옵션
int print_cnt; //print 실행 횟수
int print_max; //print 최대 실행 수

int print_row; //result배열에 저장할 행 번

int special_pid; //특정 pid만 출력하는 경우 pid 개수 
int pids[MAX]; //출력 pid 넣는 함수

int option_b; //나열모드 옵션

int special_user; //특정 user만 출력하는 경우 user의 수
char users[MAX]; //출력 user 넣는 함수
char blank[MAX];
int kill_pid; //signal을 보낼 pid
//cpu읽은 정보
int current_cpu[9];
int before_cpu[9];


typedef struct {
	unsigned long PID;
	char USER[MAX];
	char PR[3];
	char NI;
	long long VIRT;
	long long RES;
	long long SHR;
	char S;
	double CPU;
	double MEM;
	long long TIME;
	char COMMAND[MAX];
} proc;

const char *proc_path = "/proc";
proc procs[MAX];

char result[MAX][MAX];

void print_1();
void print_2();
void get_data();
void sort_by_cpu();
void sort_by_time();
int get_ch();
void sort_by_mem();

long long get_value(const char* str) { //string으로부터 정수값을 얻는 함수
	long long ret = 0;
	for(int i = 0; i < MAX; i++) {
		if(isdigit(str[i])) { //숫자일 경우 10을 곱하고 더한다.
			ret = ret*10 + ((long long)str[i] - '0');
		}
	}

	return ret;
}

void handler(int signo) { //SIGALRM 핸들러 함수
	if(print_max == print_cnt) {
		endwin();
		exit(0);
	}
	print_cnt++;
	memset(result, 0, sizeof(result));
	memset(procs, 0, sizeof(procs));
	print_row = 0;
	erase();
	get_data();
	if(option == 'P')
		sort_by_cpu();
	else if(option == 'T')
		sort_by_time();
	else if(option == 'M')
		sort_by_mem();
	if(!option_b) {
		print_1();
		refresh();
	}
	else {
		print_2();
	}
	alarm(delay);
}

void swap(proc *a, proc *b) {
	proc tmp;
	memcpy(&tmp, a, sizeof(proc));
	memcpy(a, b, sizeof(proc));
	memcpy(b, &tmp, sizeof(proc));
}

double round(double a) { //소수 첫째자리에서 반올림하는 함수
	int tmp = a * 10;
	double ret = (double)tmp / 10;
	return ret;
}

int get_user() { //USER의 수를 구하는 함수
	struct utmp *user;
	setutent();
	int ret = 0;
	while((user = getutent()) != NULL)
		if(user->ut_type == USER_PROCESS)
			ret++;
	endutent();

	return ret;
}

long long get_uptime() { //UPTIME을 초단위로 바꾸는 함수
	char buffer[MAX];
	memset(buffer, 0, sizeof(buffer));

	int fd;
	if((fd = open("/proc/uptime", O_RDONLY)) < 0) {
		fprintf(stderr, "/proc/uptime open error\n");
		exit(1);
	}

	read(fd, buffer, MAX);
	long long ret;

	int idx = 0;
	while(buffer[idx] != ' ')
		idx++;

	memset(buffer + idx, 0, sizeof(char) *(MAX - idx));

	ret = atoll(buffer);
	close(fd);

	return ret;
}

void get_loadavg(char* loadavg) { //loadavg를 string으로 저장하는 함수
	int fd;
	if((fd = open("/proc/loadavg", O_RDONLY)) <0) {
		fprintf(stderr, "/proc/loadavg open error\n");
		exit(1);
	}
	if(read(fd, loadavg, 14) == 0) {
		fprintf(stderr, "/proc/loadavg read error\n");
		exit(1);
	}
	close(fd);
}

void get_cpu_info() { //proc/stat으로부터 cpu정보를 받는 함
	int fd;
	int total = 0;
	int tmp_us, tmp_sy, tmp_ni, tmp_id, tmp_wa, tmp_hi, tmp_si, tmp_st;
	char tmp_stat[MAX]; //stat파일 읽을 버퍼
	memset(tmp_stat, 0, sizeof(tmp_stat));
	if((fd = open("/proc/stat", O_RDONLY)) < 0) {
		fprintf(stderr, "/proc/stat file open error\n");
		exit(1);
	}
	read(fd, tmp_stat, MAX);

	memset(current_cpu, 0, sizeof(current_cpu));

	char *ptr = strtok(tmp_stat, " ");
	//CPU정보 읽기
	for(int i = 0; i < 8; i++) {
		ptr = strtok(NULL, " ");
		current_cpu[i] = atoi(ptr);
		current_cpu[8] += current_cpu[i];
	}

	//백분율로 계산
	us = (double)(current_cpu[0] - before_cpu[0]) / (double)(current_cpu[8] - before_cpu[8]) *100;
	ni = (double)(current_cpu[1] - before_cpu[1]) / (double)(current_cpu[8] - before_cpu[8]) *100;
	sy = (double)(current_cpu[2] - before_cpu[2]) / (double)(current_cpu[8] - before_cpu[8]) *100;
	id = (double)(current_cpu[3] - before_cpu[3]) / (double)(current_cpu[8] - before_cpu[8]) *100;
	wa = (double)(current_cpu[4] - before_cpu[4]) / (double)(current_cpu[8] - before_cpu[8]) *100;
	hi = (double)(current_cpu[5] - before_cpu[5]) / (double)(current_cpu[8] - before_cpu[8]) *100;
	si = (double)(current_cpu[6] - before_cpu[6]) / (double)(current_cpu[8] - before_cpu[8]) *100;
	st = (double)(current_cpu[7] - before_cpu[7]) / (double)(current_cpu[8] - before_cpu[8]) *100;

	memcpy(before_cpu, current_cpu, sizeof(before_cpu));
}

void get_mem_info() {//proc/meminfo로부터 정보를 얻는 함수
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
	int mfree = get_value(mem[1]); 
	int	buffers = get_value(mem[3]);
	int SReclaimable = get_value(mem[23]);
	int cache = get_value(mem[4]);
	int mused = mtotal - mfree - buffers - cache - SReclaimable;
	int stotal = get_value(mem[14]);
	int sfree = get_value(mem[15]);
	int mavail = get_value(mem[2]);

	//header 4행 정보 저장
	memtotal = (double)mtotal / 1024;
	memfree = (double)mfree / 1024;
	memused = (double)mused / 1024;
	membuff_cache = (double)(buffers + cache + SReclaimable) / 1024;
	swaptotal = (double)stotal / 1024;
	swapfree = (double)sfree / 1024;
	swapused = (double)(stotal - sfree) / 1024;
	swapavail_mem = (double)mavail / 1024;

}

void get_proc_stat(char* proc_stat_path, int index) { //proc/pid/stat에서 정보를 얻는 함
	//fprintf(stderr, "%s\n", proc_stat_path);
	int fd;
	char stat_tmp[MAX];
	memset(stat_tmp, 0, MAX);
	if((fd = open(proc_stat_path, O_RDONLY)) < 0) { //해당 process의 stat 읽어오기
		fprintf(stderr, "process stat file open error\n");
		exit(1);
	}
	if(read(fd, stat_tmp, MAX) == 0) { //stat파일 읽기
		fprintf(stderr, "stat file read error\n");
		exit(1);
	}
	close(fd); //stat파일 닫기

	char *ptr = strtok(stat_tmp, " "); //공백을 기준으로 stat 정보 자르기
	int i = 0;

	char stats[MAX][MAX];
	memset(stats, 0, sizeof(stats));
	while(ptr != NULL) { //공백을 기준으로 stat 정보 자르기
		strcpy(stats[i++],ptr);
		ptr = strtok(NULL, " ");
	}
	procs[index].S = stats[2][0]; //S저장
	switch(stats[2][0]) { //cpu상태 계산
		case 'R':
			run++;
			break;
		case 'S':
		case 'I':
			slp++;
			break;
		case 'T':
		case 't':
			stop++;
			break;
		case 'Z':
			zombie++;
			break;
	}

	struct stat statbuf; //process stat구조체
	stat(proc_stat_path, &statbuf); //해당 stat 읽기
	struct passwd *upasswd = getpwuid(statbuf.st_uid); //uid읽어오기수
	
	//username 읽기
	for(int i = 0; i < MAX; i++) {
		if(upasswd->pw_name[i] != '\0') {
			procs[index].USER[i] = upasswd->pw_name[i];
		}
		else {
			break;
		}
	}
	strncpy(procs[index].PR, stats[17], 3); //PR저장
	procs[index].NI = atoi(stats[18]); //NI저장

	long long utime = atoll(stats[13]);
	long long stime = atoll(stats[14]);
	long long uptime = get_uptime();
	int hertz = (int)sysconf(_SC_CLK_TCK);

	double tic = (double)(utime + stime) / hertz;
	procs[index].CPU = (double)tic / uptime * 100;//%CPU구하기
	procs[index].CPU = round(procs[index].CPU); //소수점 첫째자리까지 반올림
	procs[index].TIME = (double)(utime + stime) / ((double)hertz / 100); //TIME+구하기

	//COMMAND저장
	if(option_c) {
		i = 1;
		procs[index].COMMAND[0] = '[';
		while(stats[1][i] != ')') {
			procs[index].COMMAND[i] = stats[1][i];
			i++;
		}
		procs[index].COMMAND[i] = ']';
	}
	else {
		i = 0;
		while(stats[1][i+1] != ')') {
			procs[index].COMMAND[i] = stats[1][i+1];
			i++;
		}
	}
}

void get_proc_status(const char* proc_status_path, int index) { //proc/pid/status에서 정보를 얻는 함
	int fd;
	char status_tmp[MAX];
	if((fd = open(proc_status_path, O_RDONLY)) < 0) {//proc/pid/status 파일 열기
		fprintf(stderr, "/proc/pid/status file open error\n");
		exit(1);
	}
	if(read(fd, status_tmp, MAX) == 0) { //모든 행 읽기
		fprintf(stderr, "/proc/pid/status file read error\n");
		exit(1);
	}
	close(fd);

	char *ptr = strtok(status_tmp, "\n"); //개행 단위로 토큰을 자른다
	int i = 0;
	char status[MAX][MAX];
	memset(status, 0, sizeof(status));
	while(ptr != NULL) { //2차원 배열 status에 행마다 저장
		strcpy(status[i++], ptr);
		ptr = strtok(NULL, "\n");
	}
	procs[index].VIRT = get_value(status[17]); //VIRT값 str -> integer로 변환
	procs[index].RES = get_value(status[21]); //RES값 str -> integer로 변환
	procs[index].SHR = get_value(status[23]) + get_value(status[24]); //SHR값 str -> integer로 변환
	procs[index].MEM = (double)procs[index].RES / (memtotal * 1024) * 100;
}

void get_cmdline(const char* path, int index) { //proc/pid/cmdline에서 정보를 얻기
	//fprintf(stderr, "%s\n", path);
	int fd;
	if((fd = open(path, O_RDONLY)) < 0) { //proc/pid/cmdline 파일 열기
		fprintf(stderr, "/proc/pid/cmdline file open error\n");
		exit(1);
	}
	if(read(fd, procs[index].COMMAND, MAX) < 0) { //proc/pid/cmdline 파일 읽기
		fprintf(stderr, "/proc/pid/cmdline file read error\n");
		exit(1);
	}
	close(fd);
}

void get_procs() { //pid를 확인하고 process 정보들을 가져오는 함수
	DIR *proc_dir; ///proc디렉토리 포인터
	struct dirent *dp; //proc디렉토리 엔트리 포인터
	if((proc_dir = opendir(proc_path)) == NULL) { //dir open
		fprintf(stderr, "/proc open error\n");
		exit(1);
	}

	while((dp = readdir(proc_dir)) != NULL) { //하위 파일들을 하나씩 읽는다.
		if(isdigit(dp->d_name[0])) { //만약 폴더가 숫자로 시작하는 경우(process폴더인 경우)
			procs[tasks].PID = atoi(dp->d_name);
			//printf("%s\n", dp->d_name);
			tasks++;
		}
	}

	char proc_stat_path[MAX]; //prod/pid/sta t경로
	char proc_status_path[MAX]; //proc/pid/status 경로
	char proc_cmdline_path[MAX]; //proc/pid/cmdline 경로
	for(int i = 0; i < tasks; i++) {
		memset(proc_stat_path, 0, MAX);
		memset(proc_status_path, 0, MAX);
		memset(proc_cmdline_path, 0, MAX);
		sprintf(proc_stat_path, "%s/%ld/stat", proc_path, procs[i].PID);
		sprintf(proc_status_path, "%s/%ld/status", proc_path, procs[i].PID);
		sprintf(proc_cmdline_path, "%s/%ld/cmdline", proc_path, procs[i].PID);
		if(access(proc_stat_path, F_OK) == 0) //파일이 존재할 때만 열기
			get_proc_stat(proc_stat_path, i); //proc/pid/stat의 정보를 얻는다.
		if(access(proc_status_path, F_OK) == 0) //파일이 존재할 때만 열기
			get_proc_status(proc_status_path, i); //proc/pid/status의 정보를 얻는다.
		if(option_c && access(proc_cmdline_path, F_OK) == 0) get_cmdline(proc_cmdline_path, i);
	}
	closedir(proc_dir);
}


void sort_by_cpu() { //%CPU순으로 정렬
	for(int i = 0; i < tasks-1; i++) {
		int mnum = i;
		for(int j = i+1; j < tasks; j++) {
			if(procs[mnum].CPU < procs[j].CPU) {
				mnum = j;
			}
			else if(procs[mnum].CPU == procs[j].CPU) { //사용량이 같을 경우 PID가 작은 순으로 정렬
				if(procs[mnum].PID > procs[j].PID) mnum = j;
			}
		}
		if(mnum != i) swap(&procs[mnum], &procs[i]);
	}
}

void sort_by_time() { //TIME+순으로 정렬
	for(int i = 0; i < tasks-1; i++) {
		int mnum = i;
		for(int j = i+1; j < tasks; j++) {
			if(procs[mnum].TIME < procs[j].TIME) {
				mnum = j;
			}
			else if(procs[mnum].TIME == procs[j].TIME) {
				if(procs[mnum].PID > procs[j].PID) mnum = j;
			}
		}
		if(mnum != i) swap(&procs[mnum], &procs[i]);
	}
}

void sort_by_mem() { //%MEM순으로 정렬
	for(int i = 0; i < tasks-1; i++) {
		int mnum = i;
		for(int j = i+1; j < tasks; j++) {
			if(procs[mnum].MEM < procs[j].MEM) {
				mnum = j;
			}
			else if(procs[mnum].MEM == procs[j].MEM) {
				if(procs[mnum].PID > procs[j].PID) mnum = j;
			}
		}
		if(mnum != i) swap(&procs[mnum], &procs[i]);
	}
}

void get_data() {//데이터를 읽어오는 함수
	tasks = 0, run = 0, slp = 0, stop = 0, zombie = 0;
	FILE* fp;
	struct tm *t; //서버시간 불러올
	time_t tim = time(NULL);
	t = localtime(&tim);
	int user = get_user(); //user 수 읽기
	long long uptime = get_uptime(); //uptime 읽기
	int uptime_h = uptime / 3600;
	int uptime_m = (uptime - (uptime_h * 3600)) / 60;
	char loadavg[15];
	get_loadavg(loadavg);
	loadavg[14] = '\0';
	get_mem_info();
	get_cpu_info();
	get_procs();

	memset(result, '\0', sizeof(result));
	//head 저장
	if(!uptime_line) {
		sprintf(result[print_row++], "top - %02d:%02d:%02d up  %2d:%02d,%3d user,  load average: %s", t->tm_hour, t->tm_min, t->tm_sec, uptime_h, uptime_m, user, loadavg);
	}
	sprintf(result[print_row++], "Tasks: %d total,   %d running, %d sleeping,   %d stopped,   %d zombie", tasks, run, slp, stop, zombie);
	sprintf(result[print_row++], "%%Cpu(s):  %.1f us,  %.1f sy,  %.1f ni,  %.1f id,  %.1f wa,  %.1f hi,  %.1f si,  %.1f st", us, sy, ni, id, wa, hi, si, st);
	sprintf(result[print_row++], "MiB Mem :   %.1f total,   %.1f free,   %.1f used,   %.1f buff/cache", memtotal, memfree, memused, membuff_cache);
	sprintf(result[print_row++], "MiB Swap:   %.1f total,   %.1f free,   %.1f used.   %.1f avail Mem", swaptotal, swapfree, swapused, swapavail_mem);
	strcpy(result[print_row++], "");
	sprintf(result[print_row++], "%7s %-8s %3s %3s %7s %6s %6s %c %4s %4s   %7s %s",
			"PID", "USER", "PR", "NI", "VIRT", "RES", "SHR", 'S', "%CPU", "%MEM", "TIME+", "COMMAND");
}

bool ispid(int pid) { //출력해야하는 pid인지 판단
	if(special_pid == 0) return true; //출력해야하는 pid의 수가 0인 경우 출력
	bool ret = false;
	for(int i = 0; i < special_pid; i++) {
		if(pids[i] == pid) {
			ret = true;
			break;
		}
	}
	return ret;
}

bool isuser(char* str) { //출력해야하는 user인지 판단
	if(special_user == 0) return true; //출력해야하는 user의 수가 0인 경우 출력
	bool ret = false;
	if(!strcmp(users, str)) {
		ret = true;
	}

	return ret;
}

void print_1() { //나열모드가 아닐 때 결과를 출력하는 함수
	//터미널 크기 구하기
	struct winsize win; 
	if(ioctl(0, TIOCGWINSZ, (char*)&win) < 0) {
		fprintf(stderr, "ioctl error\n");
		exit(1);
	}

	char tm[8]; //TIME 문자열 저장함수
	for(int i = start_row; i < tasks; i++) {
		if(option_i && procs[i].CPU < 0.1) continue;
		memset(tm, 0, sizeof(tm));//TIME을 문자열로 나타내기
		int min = procs[i].TIME / 6000;
		int sec = (procs[i].TIME - (min *6000)) / 100;
		int rest = (procs[i].TIME - (min *6000) - (sec * 100));
		sprintf(tm, "%d:%02d.%02d", min, sec, rest);
		char username[MAX];
		memcpy(username, procs[i].USER, MAX);
		if(username[7] != '\0') {
			username[7] = '+'; //이름이 너무 긴 경우 뒤에 '+'로 표시
			memset(username + 8, '\0', MAX - 8);
		}
		if(ispid(procs[i].PID) && isuser(procs[i].USER)) {//출력하고자 하는 pid만 출력
			snprintf(result[print_row++], win.ws_col, "%7ld %-8s %3s %3d %7lld %6lld %6lld %c %4.1lf %4.1lf   %7s %s", 
					procs[i].PID, username, procs[i].PR, procs[i].NI, procs[i].VIRT, procs[i].RES, 
					procs[i].SHR, procs[i].S, procs[i].CPU, procs[i].MEM, tm, procs[i].COMMAND);
		}
	}

	//내용 출력
	if(!uptime_line) {
		for(int i = 0; i < 6; i++)
			mvprintw(i, 0, "%s", result[i]);
		attron(A_REVERSE);
		mvprintw(6, 0, "%s", result[6] + begin[start_col]);
		attroff(A_REVERSE);
		for(int i = 7; i <= win.ws_row; i++)
			mvprintw(i, 0, "%s", result[i] + begin[start_col]);
	}
	else {
		for(int i = 0; i < 5; i++)
			mvprintw(i, 0, "%s", result[i]);
		attron(A_REVERSE);
		mvprintw(5, 0, "%s", result[5] + begin[start_col]);
		attroff(A_REVERSE);
		for(int i = 6; i <= win.ws_row; i++)
			mvprintw(i, 0, "%s", result[i] + begin[start_col]);
	}
}


void print_2() { //나열 모드 결과를 출력하는 함수
	printf("\n");
	//터미널 크기 구하기
	struct winsize win; 
	if(ioctl(0, TIOCGWINSZ, (char*)&win) < 0) {
		fprintf(stderr, "ioctl error\n");
		exit(1);
	}

	
	char tm[8]; //TIME 문자열 저장함수
	for(int i = start_row; i < tasks; i++) {
		if(option_i && procs[i].CPU < 0.1) continue;
		memset(tm, 0, sizeof(tm));//TIME을 문자열로 나타내기
		int min = procs[i].TIME / 6000;
		int sec = (procs[i].TIME - (min *6000)) / 100;
		int rest = (procs[i].TIME - (min *6000) - (sec * 100));
		sprintf(tm, "%d:%02d.%02d", min, sec, rest);
		char username[MAX];
		memcpy(username, procs[i].USER, MAX);
		if(username[7] != '\0') {
			username[7] = '+'; //이름이 너무 긴 경우 뒤에 '+'로 표시
			memset(username + 8, '\0', MAX - 8);
		}
		if(ispid(procs[i].PID) && isuser(procs[i].USER)) {//출력하고자 하는 pid만 출력
			snprintf(result[print_row++], win.ws_col, "%7ld %-8s %3s %3d %7lld %6lld %6lld %c %4.1lf %4.1lf   %7s %s", 
					procs[i].PID, username, procs[i].PR, procs[i].NI, procs[i].VIRT, procs[i].RES, 
					procs[i].SHR, procs[i].S, procs[i].CPU, procs[i].MEM, tm, procs[i].COMMAND);
		}
	}
	
	//내용 출력
	for(int i = 0; i < print_row; i++)
		printf("%s\n", result[i]);
}

void operation_d() { //d옵션을 수행하는 함수
	alarm(0); //alarm 취소
	char str[MAX];
	memset(str, 0, MAX);
	int idx = 0;
	int c;
	int chk = 0;
	if(!uptime_line) { //uptime_line이 있는 경우
		mvaddstr(5, 0, blank);
		mvaddstr(5, 0, "Change delay from 3.0 to "); 
	}
	else { //uptime_line이 없는 경우
		mvaddstr(4, 0, blank);
		mvaddstr(4, 0, "Change delay from 3.0 to ");
	}
	while((c = getch()) != '\n') {	
		if(c == 8) { //backspace인 경우
			idx--;
			delch();
			continue;
		}
		if(c == 27) { //esc를 누른 경우
			chk = 1;
			break;
		}
		str[idx] = c;
		idx++;
	}
	if(!chk) //esc를 누른 경우 취소
		delay = atoi(str);
	alarm(3);
	raise(SIGALRM);
}

void operation_u() {
	alarm(0); //alarm 취소
	char str[MAX];
	memset(str, 0, MAX);
	int idx = 0;
	char c;
	int chk = 0;
	if(!uptime_line) { //uptime_line이 있는 경우
		mvaddstr(5, 0, blank);
		mvaddstr(5, 0, "Which user (blank for all) "); 
	}
	else { //uptime_line이 없는 경우
		mvaddstr(4, 0, blank);
		mvaddstr(4, 0, "Which user (blank for all) ");
	}
	char tmp[10];
	while((c = getch()) != '\n') {
		if(c == 8) { //backspace인 경우
			idx--;
			delch();
			continue;
		}
		if(c == 27) { //esc를 누른 경우
			chk = 1;
			break;
		}
		str[idx++] = c;
	}
	if(!chk) { //esc를 누르지 않았을 때
		strncpy(users, str, MAX); //출력하고자 하는 user에 복사
		mvaddstr(5, 0, users);
		if(idx == 0) //아무것도 입력을 받지 않은 경우
			special_user = 0;
		else
			special_user = 1;
	}
	alarm(3);
	raise(SIGALRM);

}


void operation_k() { //k옵션을 수행하는 함수
	alarm(0); //alarm 취소
	char str[MAX];
	char sig[10];
	char tmp[MAX];
	char trash[MAX];
	memset(str, 0, MAX);
	memset(sig, 0, 10);
	int idx = 0;
	int c;
	int chk = 0;
	memset(tmp, 0, MAX);
	sprintf(tmp, "PID to signal/kill [default pid = %ld] ", procs[0].PID);
	if(!uptime_line) { //uptime_line이 있는 경우
		mvaddstr(5, 0, blank);
		mvaddstr(5, 0, tmp); //default는 가장 위에 있는 Process
	}
	else { //uptime_line이 없는 경우
		mvaddstr(4, 0, blank);
		mvaddstr(4, 0, tmp);
	}
	while((c = getch()) != '\n') {
		if(c == 8) { //backspace인 경우
			idx--;
			delch();
			continue;
		}
		if(c == 27) { //esc를 누른 경우
			chk = 1;
			break;
		}
		str[idx++] = c;
	}
	if(!chk) {//esc를 누르지 않은 경우
		if(idx == 0) //아무것도 입력을 받지 않은 경우
			kill_pid = procs[0].PID;
		else
			kill_pid = atoi(str);

		memset(tmp, 0, MAX);
		sprintf(tmp, "Send pid %d signal [15/sigterm] ", kill_pid);
		if(!uptime_line) { //uptime_line이 있는 경우
			mvaddstr(5, 0, blank);
			mvaddstr(5, 0, tmp);
		}
		else { //uptime_line이 없는 경우
			mvaddstr(4, 0, blank);
			mvaddstr(4, 0, tmp);
		}
		idx = 0;
		tcflush(0, TCIFLUSH);
		while((c = getch()) != '\n') {
			if(c == 8) { //backspace인 경우
				idx--;
				delch();
				continue;
			}
			if(c == 27) { //esc를 누른 경우
				chk = 1;
				break;
			}
			sig[idx++] = c;
		}
		if(!chk) { //esc를 누르지 않은 경우
			int signal = atoi(sig);
			//printf("%s\n", sig);
			//exit(0);
			if(signal == 9) {
				kill(kill_pid, SIGKILL);
			}
		}
	}
	alarm(3);
	raise(SIGALRM);
}



void start_status() { //ncurses 초기 설정
	noecho(); //echo 제거
	curs_set(0); //커서 안보이게 함
	initscr(); //출력 윈도우 초기화
	halfdelay(10); //0.1초마다 갱신
	keypad(stdscr, true);
}

int get_ch() {
	int ret;
	struct termios buf, save;
	tcgetattr(0, &save);
	buf = save;
	buf.c_lflag &= ~ICANON;
	buf.c_lflag &= ~ECHO;
	tcsetattr(0, TCSAFLUSH,	&buf);
	ret = getchar();
	tcsetattr(0, TCSAFLUSH, &save);
	return ret;
}


int main(int argc, char **argv) {

	for(int i = 12; i < MAX; i++) begin[i] = begin[i-1] + 8;
	memset(blank, ' ', 70);
	//초기 옵션 설정
	signal(SIGALRM, handler); //alarm 시그널에 동작할 handler함수를 설정
	option = 'P';
	uptime_line = 0;
	delay = 3;
	option_c = 0;
	option_i = 0;
	print_max = -1;
	special_pid = 0;
	option_b = 0;
	special_user = 0;
	kill_pid = 0;

	//실행 전 옵션 parsing
	for(int i = 1; i < argc; i++) {
		if(!strcmp(argv[i], "-n")) { //실행 횟수 제한옵션
			print_max = atoi(argv[i+1]);
			if(print_max == 0) { //올바르지 않은 숫자를 입력한 경우 에러처리 후 종료
				fprintf(stderr, "top: bad iterations argument '0'\n");
				endwin();
				exit(0);
			}
		}
		if(!strcmp(argv[i], "-p")) { //특정 PID만 출력
			int pid = atoi(argv[i+1]);
			pids[special_pid] = pid;
			special_pid++; //출력 pid 개수 증가
		}
		if(!strcmp(argv[i], "-b")) { //모든 process들을 나열하기만 하는 경우
			option_b = 1;
		}
	}

	get_data();
	sort_by_cpu();
	if(!option_b) {
		start_status();
		print_1();
		refresh();
	}
	else {
		print_2();
	}
	print_cnt = 1;
	//3초마다 갱신
	while(1) {
		if(print_max != -1 && print_max == print_cnt) break;
		alarm(delay);
		int input = get_ch();
		int sum = input;
		if(input == 'q') { //종료
			break;
		}
		else if(input == 'P') { //CPU순 정렬
			option = 'P';
			raise(SIGALRM);
		}
		else if(input == 'M') { //MEM순 정렬
			option = 'M';
			raise(SIGALRM);
		}
		else if(input == 'T') { //TIME순 정렬
			option = 'T';
			raise(SIGALRM);
		}
		
		else if(input == 'l') { //uptime_line숨김/표시
			if(uptime_line)
				uptime_line = 0;
			else
				uptime_line = 1;
			raise(SIGALRM);
		}
		
		else if(input == ' ' || input == 27) { //space나 esc를 누를 경우 refresh
			raise(SIGALRM);
		}
		
		else if(input == 'c') { //명령 인자 표시/ 비표시
			if(option_c) option_c = 0;
			else option_c = 1;
			raise(SIGALRM);
		}
		
		else if(input == 'i') {
			if(option_i) option_i = 0;
			else option_i = 1;
			raise(SIGALRM);
		}
		
		else if(input == 'd') { //delay를 설정
			operation_d();
		}
		
		else if(input == 'u' || input == 'U') { //특정 user 설정
			operation_u();
		}
		
		else if(input == 'k') { //kill하려는 pid 출력
			operation_k();
		}
		else { //방향키를 입력하는 경우
			for(int i = 0; i < 2; i++) {
				input = get_ch();
				sum += input;
			}
			//printf("%d\n", sum);
			if(sum == 171) {//위 방향키인 경우
				start_row--;
				if(start_row < 0) start_row = 0;
				raise(SIGALRM);
			}
			else if(sum == 172) { //아래 방향키인 경우
				start_row++;
				if(start_row == tasks) start_row = tasks-1;
				raise(SIGALRM);
			}
			else if(sum == 174) { //왼쪽 방향키인 경우
				start_col--;
				if(start_col < 0) start_col = 0;
				raise(SIGALRM);
			}
			else if(sum == 173) { //오른쪽 방향키인 경우
				start_col++;
				raise(SIGALRM);
			}
		}
	}

	endwin();
	return 0;
}
