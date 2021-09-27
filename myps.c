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
	bool no; //옵션이 없는지 여부
} opt;

typedef struct {
	//int F;
	//int S;
	char USER[MAX]; //*
	uid_t UID; //*
	pid_t PID; //*
	pid_t PPID; //*
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
	unsigned long START; //*
	unsigned long TIME; //*
	char COMMAND[MAX]; //*
	char CMD[MAX]; //*
	int ttyNr; //*
} proc;

//나의 정보
pid_t mypid;
uid_t myuid;
char mytty[MAX];

//data들
int tasks;
double memtotal; //전체 메모리
opt option; //option들
proc procs[MAX]; //process 정보들
int special_pid = 0;
pid_t pids[MAX];

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

	read(fd, buffer, MAX); ///proc/uptime을 읽음
	ll ret;

	int idx = 0;

	while(buffer[idx] != ' ') idx++; //첫 번째 토큰을 읽음

	memset(buffer + idx, 0, sizeof(char) * (MAX - idx)); //뒷부분은 널로 채움

	ret = atoll(buffer); //ll형으로 바꿈
	close(fd);

	return ret;
}

void get_mem_info() { ///proc/meminfo로부터 memtotal을 얻음
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

	char mem[MAX][MAX]; //파싱 정보 저장
	memset(mem, 0, sizeof(mem));
	int i = 0;
	char *ptr = strtok(mem_tmp, "\n"); //개행단위로 읽은 정보 파싱
	while(ptr != NULL) {
		strcpy(mem[i++], ptr);
		ptr = strtok(NULL, "\n"); //개행으로 정보 파싱
	}

	//파싱한 정보 정수형으로 바꾸기
	int mtotal = get_value(mem[0]);

	//memtotal 얻기
	memtotal = (double)mtotal / 1024; //MiB단위로 바꿈
}

ll get_VmLck(int pid) { //VmLck를 얻는 함수
	char path[MAX]; //경로
	char tmp[MAX]; //읽은 정보 저장할 버퍼
	char status[MAX][MAX]; //파싱결과 저장
	memset(path, 0, MAX);
	memset(tmp, 0, MAX);
	memset(status, 0, sizeof(status));
	sprintf(path, "/proc/%d/status", pid); //경로 지정

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
	//개행 단위로 정보 파싱
	char *ptr = strtok(tmp, "\n");
	while(ptr != NULL) {
		strcpy(status[i++], ptr);
		ptr = strtok(NULL, "\n");
	}
	//VmLck가 있는 경우 저장
	for(int j = 0; j < i; j++) { 
		if(!strncmp(status[j], "VmLck", 5)) { //VmLck를 찾은 경우
			ret = get_value(status[j]);
			break;
		}
	}

	return ret;
}

ll get_VSZ(int pid) {
	char path[MAX]; //경로
	char tmp[MAX]; //읽은 정보 저장할 버퍼
	char status[MAX][MAX]; //파싱결과 저장
	memset(path, 0, MAX);
	memset(tmp, 0, MAX);
	memset(status, 0, sizeof(status));
	sprintf(path, "/proc/%d/status", pid); //경로 지정

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
	//개행 단위로 정보 파싱
	char *ptr = strtok(tmp, "\n");
	while(ptr != NULL) {
		strcpy(status[i++], ptr);
		ptr = strtok(NULL, "\n");
	}
	//VmSize가 있는 경우 저장
	for(int j = 0; j < i; j++) { 
		if(!strncmp(status[j], "VmSize", 6)) { //VmSize를 찾은 경우
			ret = get_value(status[j]);
			//printf("%lld\n",ret);
			break;
		}
	}

	return ret;
}

void get_environ(int index) {
	if(procs[index].UID != myuid) return; //uid가 다르면 접근 권한이 없다.
	char path[MAX];
	char buffer[MAX];
	memset(path, 0, MAX);
	memset(buffer, 0, MAX);
	sprintf(path, "/proc/%d/environ", procs[index].PID);
	int fd;
	if((fd = open(path, O_RDONLY)) < 0) {
		fprintf(stderr, "%s open error\n", path);
		exit(1);
	}
	if(read(fd, buffer, MAX) == 0) {
		fprintf(stderr, "%s file read error\n", path);
		exit(1);
	}
	for(int i = 0; i < MAX; i++) { //사이의 널은 공백으로 바꾼다
		if(buffer[i] == '\0') {
			if(buffer[i+1] == '\0') break;
			else buffer[i] = ' ';
		}
	}
	//환경변수가 있을 경우 COMMAND뒤에 이어 붙인다.
	if(strlen(buffer)) {
		strcat(procs[index].COMMAND, " ");
		strncat(procs[index].COMMAND, buffer, MAX/2);
	}
}

void get_tty(int index) {

	char fdpath[MAX];			//0번 fd에 대한 절대 경로
	memset(fdpath, '\0', MAX);
	sprintf(fdpath, "/proc/%d/fd/0", procs[index].PID);

	DIR *dp;
	struct dirent *dentry;
	if((dp = opendir("/dev")) == NULL){		// 터미널 찾기 위해 /dev 디렉터리 open
		fprintf(stderr, "/dev directory open error\n");
		exit(1);
	}
	char nowPath[MAX];

	if(access(fdpath, F_OK) == 0){
		char symLinkName[MAX];
		memset(symLinkName, 0, MAX);
		if(readlink(fdpath, symLinkName, MAX) < 0){
			fprintf(stderr, "readlink error for %s\n", fdpath);
			exit(1);
		}
		if(!strcmp(symLinkName, "/dev/null"))		//symbolic link로 가리키는 파일이 /dev/null일 경우
			strcpy(procs[index].TTY, "?");					//nonTerminal
		else
			sscanf(symLinkName, "/dev/%s", procs[index].TTY);	//그 외의 경우 tty 획득

	}
	if(!strcmp(procs[index].TTY, "?")) {
		while((dentry = readdir(dp)) != NULL){	// /dev 디렉터리 탐색
			memset(nowPath, 0, MAX);	// 현재 탐색 중인 파일 절대 경로
			sprintf(nowPath, "/dev/%s", dentry->d_name);

			struct stat statbuf;
			if(stat(nowPath, &statbuf) < 0){	// stat 획득
				fprintf(stderr, "stat error for %s\n", nowPath);
				exit(1);
			}
			if(!S_ISCHR(statbuf.st_mode))		//문자 디바이스 파일이 아닌 경우 skip
				continue;
			else if(statbuf.st_rdev == procs[index].ttyNr){	//문자 디바이스 파일의 디바이스 ID가 ttyNr과 같은 경우
				strcpy(procs[index].TTY, dentry->d_name);	//tty에 현재 파일명 복사
				break;
			}
		}
		closedir(dp);
	}
	if(!strlen(procs[index].TTY))					// 어디에서도 찾지 못한 경우
		strcpy(procs[index].TTY, "?");				//nonTerminal

	if(procs[index].PID == mypid)
		strcpy(mytty, procs[index].TTY);

	return;

}


void get_proc_stat(const char* path, int index) {

	//USER읽기
	struct stat statbuf;
	stat(path, &statbuf);
	struct passwd *upasswd = getpwuid(statbuf.st_uid);
	strcpy(procs[index].USER, upasswd->pw_name);
	if(procs[index].USER[8] != '\0') {
		procs[index].USER[7] = '+';
		procs[index].USER[8] = '\0';
	}

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

	//TIME구하기
	procs[index].TIME = (utime + stime) / hertz;

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
	strcpy(procs[index].NI, stat[18]);

	//PRI저장
	strncpy(procs[index].PRI, stat[17], 3);

	//START구하기
	procs[index].START = time(NULL) - uptime + ((unsigned long)atoi(stat[21]) / hertz);
	//printf("%lld\n", procs[index].START);

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
	procs[index].VSZ = get_VSZ(procs[index].PID);
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
	//두번 연속 널이 나올 경우 종료 그렇지 않으면 공백으로 처리
	for(int i = 0; i < MAX; i++) {
		if(procs[index].COMMAND[i] == '\0') {
			if(procs[index].COMMAND[i+1] == '\0') break;
			else procs[index].COMMAND[i] = ' ';
		}
	}
	if(!strlen(procs[index].COMMAND)) { //없는 경우
		sprintf(procs[index].COMMAND, "[%s]", procs[index].CMD);
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
			procs[tasks++].PID = atol(dp->d_name); //pid저장
		}
	}

	char stat_path[MAX];
	char status_path[MAX];
	char cmdline_path[MAX];
	for(int i = 0; i < tasks; i++) {
		memset(stat_path, 0, MAX);
		memset(status_path, 0, MAX);
		memset(cmdline_path, 0, MAX);
		sprintf(stat_path, "/proc/%d/stat", procs[i].PID);
		sprintf(status_path, "/proc/%d/status", procs[i].PID);
		sprintf(cmdline_path, "/proc/%d/cmdline", procs[i].PID);
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
		if(option.e) get_environ(i);
		get_tty(i);
	}

	closedir(proc_dir);
}

bool isSpecial(pid_t p) {
	bool ret = false;
	for(int i = 0; i < special_pid; i++) {
		if(pids[i] ==  p) {
			ret = true;
			break;
		}
	}
	return ret;
}

void print_data() { //데이터들을 불러오는 함수

	//터미널 사이즈
	struct winsize win;
	if(ioctl(0, TIOCGWINSZ, (char*)&win) < 0) {
		fprintf(stderr, "ioctl error\n");
		exit(1);
	}

	//첫 줄 출력
	if(!(option.bar && option.f) && option.u)
		printf("USER    ");
	if((option.bar && option.f))
		printf("UID     ");
	printf("    PID");
	if((option.bar && option.f))
		printf("    PPID");
	if(!option.u &&(option.bar && option.f))
		printf(" STIME");
	if(option.u)
		printf(" %%CPU");
	if(option.u)
		printf(" %%MEM");
	if(option.u)
		printf("    VSZ");
	if(option.u)
		printf("   RSS");
	printf(" TTY    ");
	if(option.u || option.r || option.x || option.f || option.a || option.p)
		printf("STAT");
	if(option.u)
		printf(" START");
	if(option.bar || option.no)
		printf("      TIME");
	else 
		printf("   TIME");
	if(option.r || option.u || option.x || option.e || option.f)
		printf(" COMMAND\n");
	else
		printf(" CMD\n");

	int strlen; //string 길이
	for(int index = 0; index < tasks; index++) {
		strlen = 0;

		//process 거르기
		if(option.r) { //현재 running중인 프로세스만 표시
			if(procs[index].STAT[0] != 'R') continue;
		}

		if(option.a) {
			if(option.u) {
				if(!option.x) {
					if(option.p) { //aup일 경우
						if(!isSpecial(procs[index].PID) && !strcmp(procs[index].TTY, "?")) continue; //특정 pid도 아니고 tty가 ?일 경우 건너뛰기
					}
					else { //au일 경우 tty가 ?가 아닌 것만 출력
						if(!strcmp(procs[index].TTY, "?")) continue;
					}
				}
				//ax는 전부 출력
			}
			else {
				if(!option.x) { //a, ap일 경우
					if(!isSpecial(procs[index].PID) && !strcmp(procs[index].TTY, "?")) continue;
				}
			}
		}
		else {
			if(option.u) {
				if(!option.x) {
					if(option.p) { //up일 경우
						if(!isSpecial(procs[index].PID)) continue; //u는 무시됨
					}
					else { //u일 경우
						if(!strcmp(procs[index].TTY, "?")) continue; //tty가 ?일 경우 건너뜀
					}
				}
				else {
					if(option.p) { //uxp일 경우
						//같은 소유자와 지정한 pid만  출력
						if(!isSpecial(procs[index].PID) && (procs[index].UID != myuid)) continue;
					}
					else { //ux일 경우
						if(procs[index].UID != myuid) continue; //다른 소유자일 경우 건너뜀
					}
				}
			}
			else {
				if(!option.x) {
					if(option.p) { //p일 경우
						if(!isSpecial(procs[index].PID)) continue; //지정한 pid만 출력
					}
					else { //no option인 경우
						if(strcmp(procs[index].TTY, mytty) != 0) continue; //같은 tty만 출력
					}
				}
				else {
					if(option.p) { //xp인 경우
						//같은 소유자와 지정한 pid만 출력
						if(!isSpecial(procs[index].PID) && procs[index].UID != myuid) continue;
					}
					else { //x인 경우
						if(procs[index].UID != myuid) continue; //같은 소유자만 출력
					}
				}
			}
		}

		//출력 정보 거르기
		if(!(option.bar && option.f) && option.u) {
			printf("%-8s", procs[index].USER);
			strlen += 8;
		}
		if((option.bar && option.f)) {
			printf("%-8s", procs[index].USER);
			strlen += 8;
		}
		printf(" %6d", procs[index].PID);
		strlen += 7;
		if((option.bar && option.f)) {
			printf(" %6d", procs[index].PPID);
			strlen += 7;
		}
		/*
		   if((option.bar && option.f))
		   printf("  C");

		   if(!option.u &&(option.bar && option.f)) {
		   struct tm *t;
		   char starttime[MAX];
		   memset(starttime, 0, MAX);
		   t = localtime(&procs[index].START);
		   if(time(NULL) - procs[index].START < 24 * 3600) {
		   strftime(starttime, 5, "%2H:%02M", t);
		   }
		   else if(time(NULL) - procs[index].START < 7 * 24 * 3600) {
		   strftime(starttime, 5, "%b %d", t);
		   }
		   else {
		   strftime(starttime, 5, "%y", t);
		   }
		   printf(" %s", starttime);
		   strlen += 6;
		   }
		   if(option.l)
		   printf(" PRI");
		   if(option.l)
		   printf("  NI");
		 */
		if(option.u) {
			printf(" %4.1f", procs[index].CPU);
			strlen += 5;
		}
		if(option.u) {
			printf(" %4.1f", procs[index].MEM);
			strlen += 5;
		}
		if(option.u) {
			printf(" %6lld", procs[index].VSZ);
			strlen += 7;
		}
		if(option.u) {
			printf(" %5lld", procs[index].RSS);
			strlen += 8;
		}
		/*
		   if((option.l && option.bar))
		   printf(" ADDR");
		   if((option.l && option.bar))
		   printf(" SZ");
		   if(option.l)
		   printf(" WCHAN");
		 */
		printf(" %-7s", procs[index].TTY);
		strlen += 10;
		if(option.a ||option.u || option.r || option.x || option.f || option.p) {
			printf("%-4s", procs[index].STAT);
			strlen += 4;
		}
		if(option.u) {
			struct tm *t;
			char starttime[16];
			memset(starttime, 0, 16);
			t = localtime(&procs[index].START);
			if(time(NULL) - procs[index].START < 24 * 3600) {
				strftime(starttime, 16, "%H:%M", t);
			}
			else if(time(NULL) - procs[index].START < 7 * 24 * 3600) {
				strftime(starttime, 16, "%b %d", t);
			}
			else {
				strftime(starttime, 16, "%y", t);
			}
			printf(" %s", starttime);
			strlen += 6;

		}
		struct tm *T = localtime(&procs[index].TIME);
		if(option.bar || option.no) {
			if(procs[index].TIME == 0)
				printf("  00:00:00");
			else
				printf("  %2d:%02d:%02d", T->tm_hour, T->tm_min, T->tm_sec);
			strlen += 10;
		}
		else {
			printf("  %2d:%02d", T->tm_min, T->tm_sec);
			strlen += 7;
		}
		char cmdstr[MAX];
		memset(cmdstr, 0, MAX);
		if(option.r || option.u || option.x || !option.bar || option.e) {
			strcpy(cmdstr, " ");
			strcat(cmdstr, procs[index].COMMAND);
		}
		else {
			strcpy(cmdstr, " ");
			strcat(cmdstr, procs[index].CMD);
		}
		for(int i = 0; i < win.ws_col-strlen-1; i++)
			putc(cmdstr[i], stdout);
		putc('\n', stdout);
	}
}

int main(int argc, char** argv) {
	mypid = getpid();
	myuid = getuid();
	option.bar = FALSE;
	option.a = FALSE;
	option.e = FALSE;
	option.f = FALSE;
	option.p = FALSE;
	option.no = FALSE;
	option.r = FALSE;
	option.u = FALSE;
	option.x = FALSE;
	if(argc == 1) option.no = TRUE;
	else {
		option.no = FALSE;
		for(int i = 1; i < argc; i++) { //옵션 설정
			int idx = 0;
			while(argv[i][idx] != '\0') {
				if(argv[i][idx] == '-') option.bar = TRUE;
				if(argv[i][idx] == 'a') {
					option.a = TRUE;
				}
				if(argv[i][idx] == 'e') option.e = TRUE;
				if(argv[i][idx] == 'f') option.f = TRUE;
				if(argv[i][idx] == 'p') {
					option.p = TRUE;
					i++;
					while(i < argc) {
						if(!isdigit(argv[i][0])) break;
						pids[special_pid++] = atoi(argv[i]);
						i++;
					}
					i--;
					break;
				}
				if(argv[i][idx] == 'r') option.r = TRUE;
				if(argv[i][idx] == 'u') option.u = TRUE;
				if(argv[i][idx] == 'x') option.x = TRUE;
				idx++;
			}
		}
	}
	get_mem_info();
	get_procs();
	print_data();
}
