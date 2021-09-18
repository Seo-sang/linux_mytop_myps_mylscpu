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
int tasks, run, sleep, stop, zombie;
float us, sy, ni, id, wa, hi, si, st;
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
	double MEN;
	char TIME[9];
	char COMMAND[MAX];
} proc;
const char proc_path = "/proc";
proc procs[MAX];

int get_user() {
	struct utmp *user;
	setutent();
	int ret = 0;
	while((user = getutent()) != NULL)
		if(user->ut_type == USER_PROCESS)
			ret++;
	endutent();

	return ret;
}

long long get_uptime() {
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

void get_loadavg(char* loadavg) {
	int fd;
	if((fd = open("/proc/loadavg", O_RDONLY)) <0) {
		fprintf(stderr, "/proc/loadavg open error\n");
		exit(1);
	}
	if(read(fd, loadavg, 14) == 0) {
		fprintf(stderr, "/proc/loadavg read error\n");
		exit(1);
	}
	printf("%s\n", loadavg);
	close(fd);
}

void get_cpuinfo() {
	int fd;
	int total = 0;
	int tmp_us, tmp_sy, tmp_ni, tmp_id, tmp_wa, tmp_hi, tmp_si, tmp_st;
	int tmp_stat[MAX]; //stat파일 읽을 버퍼
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


}

void get_proc_stat(char* proc_stat_path) {
	int fd;
	char stat_tmp[MAX];
	if((fd = open(proc_stat_path, O_RDONLY)) < 0) { //해당 process의 stat 읽어오기
		fprintf(stderr, "process status file open error\n");
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
	procs[tasks].S = stats[2][0]; //S저장

	switch(stats[2][0]) { //cpu상태 계산
		case 'R':
			run++;
			break;
		case 'S':
			sleep++;
			break;
		case 'T':
			stop++;
			break;
		case 'Z':
			zombie++;
			break;
	}

	procs[tasks].PR = atoi(stats[17]); //PR저장
	procs[tasks].NI = atoi(stats[18]); //NI저장
	procs
	
}

void get_procs() { //pid를 확인하고 process 정보들을 가져오는 함수
	DIR *proc_dir; ///proc디렉토리 포인터
	struct dirent *dp; //proc디렉토리 엔트리 포인터
	if((proc_dir = opendir(proc_path)) == NULL) { //dir open
		fprintf(stderr, "/proc open error\n");
		exit(1);
	}
	while((dp = readdir(pric_dir)) != NULL) { //하위 파일들을 하나씩 읽는다.
		if(isdigit(dp->d_name[0])) { //만약 폴더가 숫자로 시작하는 경우(process폴더인 경우)
			char proc_stat_path[MAX];
			sprintf(proc_stat_path, "%s%s/stat", proc_path, dp->dname);
			get_proc_stat(proc_stat_path); //process 상태를 얻는다.
			tasks++; //전체 프로세스 수 증가
		}
	}
}

void print() {
	FILE* fp;
	struct tm *t; //서버시간 불러올
	time_t tim = time(NULL);
	t = localtime(&tim);
	int user = get_user(); //user 수 읽기
	long long uptime = get_uptime(); //uptime 읽기
	int uptime_min = uptime / 60;
	char loadavg[15];
	get_loadavg(loadavg);
	loadavg[14] = '\0';
	
	get_procs();
	
	system("clear");
	printf("top - %d:%d:%d up  %d min,  %d user,  load average: %s\n", t->tm_hour, t->tm_min, t->tm_sec, uptime_min, user, loadavg);
	printf("Tasks: %d total,    %d running, %d sleeping,    %d stopped,    %d zombie\n", tasks, run, sleep, stop, zombie);
	printf("%Cpu(s):  %.1f us,  %.1f sy,  %.1f ni,  %.1f id,  %1f wa,  %.1f hi,  %.1f si,  %.1f st\n", us, sy, ni id, wa, hi, si, st);
}

int main() {

	//3초마다 갱신
	while(1) {
		print();
		sleep(3);
	}

}
