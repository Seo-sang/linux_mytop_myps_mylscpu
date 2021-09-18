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

void get_procs() {

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
}

int main() {

	//3초마다 갱신
	while(1) {
		print();
		sleep(3);
	}

}
