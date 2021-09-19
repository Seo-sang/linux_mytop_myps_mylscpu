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



#define MAX 1024
struct tm *t;

//헤더 정보들
int tasks, run, slp, stop, zombie;
double us, sy, ni, id, wa, hi, si, st;
double memtotal, memfree, memused, membuff_cache;
double swaptotal, swapfree, swapused, swapavail_mem;

int start_row = 0, start_col = 0;

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

void print();
void get_data();
void sort_by_cpu();

int get_value(const char* str) { //string으로부터 정수값을 얻는 함수
	int ret = 0;
	for(int i = 0; i < MAX; i++) {
		if(isdigit(str[i])) { //숫자일 경우 10을 곱하고 더한다.
			ret = ret*10 + (str[i] - '0');
		}
	}

	return ret;
}

void handler(int signo) { //SIGALRM 핸들러 함수
	get_data();
	print();
	alarm(3);
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

	char *ptr = strtok(tmp_stat, " ");
	ptr = strtok(NULL, " "); //us읽기
	tmp_us = atoi(ptr);
	total += tmp_us;
	ptr = strtok(NULL, " "); //ni읽기
	tmp_ni = atoi(ptr);
	total += tmp_ni;
	ptr = strtok(NULL, " "); //sy읽기
	tmp_sy = atoi(ptr);
	total += tmp_sy;
	ptr = strtok(NULL, " "); //id읽기
	tmp_id = atoi(ptr);
	total += tmp_id;
	ptr = strtok(NULL, " "); //wa읽기
	tmp_wa = atoi(ptr);
	total += tmp_wa;
	ptr = strtok(NULL, " "); //hi읽기
	tmp_hi = atoi(ptr);
	total += tmp_hi;
	ptr = strtok(NULL, " "); //si읽기
	tmp_si = atoi(ptr);
	total += tmp_si;
	ptr = strtok(NULL, " "); //st읽기
	tmp_st = atoi(ptr);
	total += tmp_st;
	int idx = 0;
	//백분율로 계산
	us = (double)(tmp_us *100) / total;
	ni = (double)(tmp_ni *100) / total; 
	sy = (double)(tmp_sy * 100) / total;
	id = (double)(tmp_id * 100) / total;
	wa = (double)(tmp_wa * 100) / total;
	hi = (double)(tmp_hi * 100) / total;
	si = (double)(tmp_si * 100) / total;
	st = (double)(tmp_st *100) / total;
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
	while(ptr != NULL) {수
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

void get_proc_stat(char* proc_stat_path, int index) {
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
	struct passwd *upasswd = getpwuid(statbuf.st_uid); //uid읽어오기
	strcpy(procs[index].USER, upasswd->pw_name);
	strncpy(procs[index].PR, stats[17], 3); //PR저장
	procs[index].NI = atoi(stats[18]); //NI저장

	int utime = atoi(stats[13]);
	int stime = atoi(stats[14]);
	int startTime = atoi(stats[21]);
	long long uptime = get_uptime();
	int hertz = (int)sysconf(_SC_CLK_TCK);

	procs[index].CPU = ((double)(utime+stime) / hertz) / (uptime - ((double)startTime / hertz)) * 100; //%CPU저장
	procs[index].TIME = (double)(utime + stime) / ((double)hertz / 100);
	
	//COMMAND저장
	i = 0;
	while(stats[1][i+1] != ')') {
		procs[index].COMMAND[i] = stats[1][i+1];
		i++;
	}
}

void get_proc_status(const char* proc_status_path, int index) {
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
	procs[index].RES = get_value(status[20]); //RES값 str -> integer로 변환
	procs[index].SHR = get_value(status[23]); //SHR값 str -> integer로 변환
	procs[index].MEM = (double)procs[index].RES / (memtotal *1024);
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
			tasks++;
		}
	}
	char proc_stat_path[MAX]; //prod/pid/stat
	char proc_status_path[MAX]; //proc/pid/status
	for(int i = 0; i < tasks; i++) {
		memset(proc_stat_path, 0, MAX);
		memset(proc_status_path, 0, MAX);
		sprintf(proc_stat_path, "%s/%ld/stat", proc_path, procs[i].PID);
		sprintf(proc_status_path, "%s/%ld/status", proc_path, procs[i].PID);
		get_proc_stat(proc_stat_path, i); //proc/pid/stat의 정보를 얻는다.
		get_proc_status(proc_status_path, i); //proc/pid/status의 정보를 얻는다.
	}
	closedir(proc_dir);
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
	get_procs();
	get_cpu_info();
	get_mem_info();
	
	//head 저장
	sprintf(result[0], "top - %02d:%02d:%02d up  %2d:%02d,%3d user,  load average: %s\n", t->tm_hour, t->tm_min, t->tm_sec, uptime_h, uptime_m, user, loadavg);
	sprintf(result[1], "Tasks: %d total,   %d running, %d sleeping,   %d stopped,   %d zombie\n", tasks, run, slp, stop, zombie);
	sprintf(result[2], "%%Cpu(s):  %.1f us,  %.1f sy,  %.1f ni,  %.1f id,  %.1f wa,  %.1f hi,  %.1f si,  %.1f st\n", us, sy, ni, id, wa, hi, si, st);
	sprintf(result[3], "MiB Mem :   %.1f total,   %.1f free,   %.1f used,   %.1f buff/cache\n", memtotal, memfree, memused, membuff_cache);
	sprintf(result[4], "MiB Swap:   %.1f total,   %.1f free,   %.1f used.   %.1f avail Mem\n\n", swaptotal, swapfree, swapused, swapavail_mem);
	result[5] = "\n";
	snprintf(result[6], win.ws_col, "%5s %-8s %3s %3s %7s %6s %6s %c %4s %4s   %7s %s",
		"PID", "USER", "PR", "NI", "VIRT", "RES", "SHR", 'S', "%CPU", "%MEM", "TIME+", "COMAND");
}

void print() { //결과를 출력하는 함수
	//터미널 크기 구하기
	struct winsize win; 
	if(ioctl(0, TIOCGWINSZ, (char*)&win) < 0) {
		fprintf(stderr, "ioctl error\n");
		exit(1);
	}
	
	char tm[8];
	for(int i = 0; i < tasks; i++) {
		memset(tm, 0, sizeof(tm));
		int min = procs[i].TIME / 6000;
		int sec = (procs[i].TIME - (min *6000)) / 100;
		int rest = (procs[i].TIME - (min *6000) - (sec * 100));
		sprintf(tm, "%d:%d.%d", min, sec, rest);
		snprintf(result[i+7], win.ws_col, "%5ld %-8s %3s %3d %7lld %6lld %6lld %c %4.1lf %4.1lf    %7s %s", procs[i].PID, procs[i].USER, procs[i].PR, procs[i].NI, procs[i].VIRT, procs[i].RES,
				procs[i].SHR, procs[i].S, procs[i].CPU, procs[i].MEM, tm, procs[i].COMMAND);
	}
	
	//내용 출력
	for(int i = 0; i < 7; i++) {
		printf("%s\n", reusult[i]);
	}
	for(int i = start_row + 6; i < win.ws_row + start_row - 7; i++) {
		printf("%s\n", result[i]);
	}
}

int get_input() { //입력을 받는 함수
	int ret;
	struct termios term;
	tcgetattr(STDIN_FILENO, &term);
	term.c_lflag &= ~ICANON; //line단위 입력 끄기
	term.c_lflag &= ~ECHO; //입력이 터미널에 보이지 않게 하기
	//term.c_cc[VMIN] = 1; //최소 입력 버퍼 크기
	//term.c_cc[VTIME] = 0; //버퍼 비우는 시간
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &term); 
	ret = getchar();

	return ret;
}

void return_status() {
	struct termios term;
	tcgetattr(STDIN_FILENO, &term);
	term.c_lflag |= ICANON; //line단위 입력 켜기
	term.c_lflag |= ECHO; //입력이 터미널에 다시 보이게 하기
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &term);
}


int main() {
	printf("\n");
	signal(SIGALRM, handler);
	get_data();
	print();

	//3초마다 갱신
	
	while(1) {
		alarm(3);
		int input = get_input();
		int sum = input;
		if(input == 'q') {
			exit(0);
		}
		else {
			for(int i = 0; i < 2; i++) {
				input = get_input();
				sum += input;
			}
			if(sum == 183) {//위 방향키인 경우
				start_row--;
				if(start_row < 0) start_row = 0;
				raise(SIGALRM);
			}
			else if(sum == 184) { //아래 방향키인 경우
				 start_row++;
				 if(start_row == tasks) start_row = tasks-1;
				 raise(SIGALRM);
			}
			else if(sum == 185) { //왼쪽 방향키인 경우
				start_col--;
				if(start_col < 0) start_col = 0;
				raise(SIGALRM);
			}
			else if(sum == 186) { //오른쪽 방향키인 경우
				start_col++;
				raise(SIGALRM);
			}
		}
	}

	return_status();

	return 0;
}
