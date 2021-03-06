#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>
#include <sys/utsname.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/ioctl.h>
#include <limits.h>

#define SHORT 100
#define MID 1000
#define LONG 3000

//출력정보
char architecture[SHORT];
char op_mode[SHORT];
char byte_order[SHORT];
char address_size[SHORT];
int cpu;
char online_cpu[SHORT];
int thread_per_core;
int core_per_socket;
int socket;
int NUMA_node;
char vendor_id[SHORT];
int cpu_family;
char model[SHORT];
char model_name[SHORT];
int stepping;
char mhz[SHORT];
char max_mhz[SHORT];
char min_mhz[SHORT];
char bogomips[SHORT];
char virtualization[SHORT];
char L1d[SHORT];
char L1i[SHORT];
char L2[SHORT];
char L3[SHORT];
char NUMA_nodes[SHORT][SHORT];
char itlb_multihit[SHORT];
char l1tf[MID];
char mds[MID];
char meltdown[MID];
char spec_store_bypass[MID];
char spectre_v1[MID];
char spectre_v2[MID];
char srbds[MID];
char tsx_async_abort[MID];
char flags[LONG];

typedef struct cache {
	char one_size[30];
	char all_size[30];
	char ways[30];
	char type[30];
	char level[30];
} cache;

cache L1d_C, L1i_C, L2_C, L3_C; //C옵션을 추가할 경우

bool op_64, op_32, op_16;
int core; //cpu 코어 개수
int siblings; //thread per core를 구하기 위한 값
int processor; //processor 개수
int get_value(const char* str) { //string으로부터 정수값 얻기
	int ret = 0;
	for(int i = 0; i < strlen(str); i++) {
		if(isdigit(str[i]))
			ret = ret * 10 + (str[i] - '0');
	}

	return ret;
}

void delete_blank(char *str) {
	char tmp[LONG];
	memset(tmp, 0, LONG);

	int i = 0;
	while(str[i] == ' ') i++;
	strcpy(tmp, str + i);
	strcpy(str, tmp);
}

void delete_n(char *str) {
	if(str[strlen(str)-1] == '\n') str[strlen(str)-1] = '\0';
}

int start_point(char* str) { //공백은 건너뛰어주는 함
	int ret = 0;
	while(true) {
		if(str[ret] == ':') break;
		ret++;
	}
	while(str[ret] == ' ') ret++;
	return ret+1;
}

void get_cpuinfo() { //proc/cpuinfo에서 정보를 얻는 함수
	char *path = "/proc/cpuinfo";
	char parse_tmp[10000]; //읽어올 정보
	char parse[MID][LONG]; //파싱정보 저장
	memset(parse_tmp, 0, 10000);
	memset(parse, 0, sizeof(parse));
	int i = 0;
	char *ptr;
	int fd;//file descriptor
	if(access(path, F_OK)) return;
	if((fd = open(path, O_RDONLY)) < 0) {
		fprintf(stderr, "%s file open error\n", path);
		exit(1);
	}
	while(true) {
		memset(parse_tmp, 0, sizeof(parse_tmp));
		if(read(fd, parse_tmp, 10000) == 0) {
			break;
		}
		//읽어온 정보 개행단위로 파싱하기
		ptr = strtok(parse_tmp, "\n");
		while(ptr != NULL) {
			strcpy(parse[i++], ptr);
			ptr = strtok(NULL, "\n");
		}
	}
	close(fd);
	i = 0;
	int idx = 0;
	for(int i = 0; i < MID; i++) {
		if(!strncmp(parse[i], "processor", 9)) {
			processor += 1;
		}
		else if(!strncmp(parse[i], "address sizes", 13)) {
			if(strlen(address_size) == 0) {
				strcpy(address_size, parse[i] + start_point(parse[i]));
			}
		}
		else if(!strncmp(parse[i], "vendor_id", 9)) {
			if(strlen(vendor_id) == 0) {
				strcpy(vendor_id, parse[i] + start_point(parse[i]));
			}
		}
		else if(!strncmp(parse[i], "cpu family", 10)) {
			cpu_family = get_value(parse[i]);
		}
		else if(!strncmp(parse[i], "model", 5) && strlen(model) == 0) {
			strcpy(model, parse[i] + start_point(parse[i]));
		}
		else if(!strncmp(parse[i], "model name", 10)) {
			if(strlen(model_name) == 0) {
				strcpy(model_name, parse[i] + start_point(parse[i]));
			}
		}
		else if(!strncmp(parse[i], "stepping", 8)) {
			stepping = get_value(parse[i]);
		}
		else if(!strncmp(parse[i], "cpu MHz", 7)) {
			if(strlen(mhz) == 0) {
				strcpy(mhz, parse[i] + start_point(parse[i]));
			}
		}
		else if(!strncmp(parse[i], "bogomips", 8)) {
			if(strlen(bogomips) == 0) {
				strcpy(bogomips, parse[i] + start_point(parse[i]));
			}
		}
		else if(!strncmp(parse[i], "flags", 5)) {
			if(strlen(flags) == 0) {
				strncpy(flags, parse[i] + start_point(parse[i]), LONG);
			}
		}
		else if(!strncmp(parse[i], "cpu cores", 9)) {
			core = get_value(parse[i]);
		}
		else if(!strncmp(parse[i], "siblings", 8)) {
			siblings = get_value(parse[i]);
		}
	}
	//flag 내용을 공백 단위로 파싱
	char flagcopy[LONG];
	memcpy(flagcopy, flags, LONG);

	char parse_flag[SHORT][SHORT];
	i = 0;
	ptr = strtok(flagcopy, " ");
	while(ptr != NULL) {
		strcpy(parse_flag[i++], ptr);
		ptr = strtok(NULL, " ");
	}

	//아무것도 없을 경우 full로 표시
	strcpy(virtualization, "full");
	for(i = 0; i < SHORT; i++) {
		if(!strcmp(parse_flag[i], "vmx")) { //virtualization VT-x로 표시
			strcpy(virtualization, "VT-x");
		}
		if(!strcmp(parse_flag[i], "svm")) { //virtualization AMD-V로 표시
			strcpy(virtualization, "AMD-V");
		}
		if(!strcmp(parse_flag[i], "lm")) { //op-mode long mode이므로 64bit
			if(op_64) continue;
			op_64 = true;
			if(strlen(op_mode) == 0) {
				strcpy(op_mode, "64-bit");
			}
			else {
				strcat(op_mode, ", 64-bit");
			}
		}
		if(!strcmp(parse_flag[i], "tm")) { //op-mode transparent mode이므로 32bit
			if(op_32) continue;
			op_32 = true;
			if(strlen(op_mode) == 0) {
				strcpy(op_mode, "32-bit");
			}
			else {
				strcat(op_mode, ", 32-bit");
			}
		}
		if(!strcmp(parse_flag[i], "rm")) { //op-mode read mode이므로 16bit
			if(op_16) continue;
			op_16 = true;
			if(strlen(op_mode) == 0) {
				strcpy(op_mode, "16-bit");
			}
			else {
				strcat(op_mode, ", 16-bit");
			}
		}
	}
	//thread-per-core구하기
	thread_per_core = siblings / core;
}

void get_vulnerability() { //sys/devices/system/cpu/vulnerabilities 디렉토리를 읽는 함
	char *itlb_multihit_path = "/sys/devices/system/cpu/vulnerabilities/itlb_multihit";
	char *l1tf_path = "/sys/devices/system/cpu/vulnerabilities/l1tf";
	char *mds_path = "/sys/devices/system/cpu/vulnerabilities/mds";
	char *meltdown_path = "/sys/devices/system/cpu/vulnerabilities/meltdown";
	char *bypass_path = "/sys/devices/system/cpu/vulnerabilities/spec_store_bypass";
	char *spectre_v1_path = "/sys/devices/system/cpu/vulnerabilities/spectre_v1";
	char *spectre_v2_path = "/sys/devices/system/cpu/vulnerabilities/spectre_v2";
	char *srbds_path = "/sys/devices/system/cpu/vulnerabilities/srbds";
	char *tsx_path = "/sys/devices/system/cpu/vulnerabilities/tsx_async_abort";

	int fd; //file descriptor

	//Itlb multihit를 읽어옴
	if(access(itlb_multihit_path, F_OK) == 0) {
		if((fd = open(itlb_multihit_path, O_RDONLY)) <0) {
			fprintf(stderr, "%s file open error\n", itlb_multihit_path);
			exit(1);
		}
		if(read(fd, itlb_multihit, MID) == 0) {
			fprintf(stderr, "%s file read error\n", itlb_multihit_path);
			exit(1);
		}
		close(fd);
	}

	//l1tf를 읽어옴
	if(access(l1tf_path, F_OK) == 0) {
		if((fd = open(l1tf_path, O_RDONLY)) <0) {
			fprintf(stderr, "%s file open error\n", l1tf_path);
			exit(1);
		}
		if(read(fd, l1tf, MID) == 0) {
			fprintf(stderr, "%s file read error\n", l1tf_path);
			exit(1);
		}
		close(fd);
	}

	//Mds를 읽어옴
	if(access(mds_path, F_OK) == 0) {
		if((fd = open(mds_path, O_RDONLY)) <0) {
			fprintf(stderr, "%s file open error\n", mds_path);
			exit(1);
		}
		if(read(fd, mds, MID) == 0) {
			fprintf(stderr, "%s file read error\n", mds_path);
			exit(1);
		}
		close(fd);
	}

	//Meltdown을 읽어옴
	if(access(meltdown_path, F_OK) == 0) {
		if((fd = open(meltdown_path, O_RDONLY)) <0) {
			fprintf(stderr, "%s file open error\n", meltdown_path);
			exit(1);
		}
		if(read(fd, meltdown, MID) == 0) {
			fprintf(stderr, "%s file read error\n", meltdown_path);
			exit(1);
		}
		close(fd);
	}

	//spec store bypass를 읽어옴
	if(access(bypass_path, F_OK) == 0) {
		if((fd = open(bypass_path, O_RDONLY)) <0) {
			fprintf(stderr, "%s file open error\n", bypass_path);
			exit(1);
		}
		if(read(fd, spec_store_bypass, MID) == 0) {
			fprintf(stderr, "%s 옴file read error\n", bypass_path);
			exit(1);
		}
		close(fd);
	}

	//spectre v1을 읽어옴
	if(access(spectre_v1_path, F_OK) == 0) {
		if((fd = open(spectre_v1_path, O_RDONLY)) <0) {
			fprintf(stderr, "%s file open error\n", spectre_v1_path);
			exit(1);
		}
		if(read(fd, spectre_v1, MID) == 0) {
			fprintf(stderr, "%s file read error\n", spectre_v1_path);
			exit(1);
		}
		close(fd);
	}

	//spectre v2를 읽어옴
	if(access(spectre_v2_path, F_OK) == 0) {
		if((fd = open(spectre_v2_path, O_RDONLY)) <0) {
			fprintf(stderr, "%s file open error\n", spectre_v2_path);
			exit(1);
		}
		if(read(fd, spectre_v2, MID) == 0) {
			fprintf(stderr, "%s file read error\n", spectre_v2_path);
			exit(1);
		}
		close(fd);
	}
	//Srbds를 읽어옴
	if(access(srbds_path, F_OK) == 0) {
		if((fd = open(srbds_path, O_RDONLY)) <0) {
			fprintf(stderr, "%s file open error\n", srbds_path);
			exit(1);
		}
		if(read(fd, srbds, MID) == 0) {
			fprintf(stderr, "%s file read error\n", srbds_path);
			exit(1);
		}
		close(fd);
	}

	//Tsx async abort를 읽어
	if(access(tsx_path, F_OK) == 0) {
		if((fd = open(tsx_path, O_RDONLY)) <0) {
			fprintf(stderr, "%s file open error\n", tsx_path);
			exit(1);
		}
		if(read(fd, tsx_async_abort, MID) == 0) {
			fprintf(stderr, "%s file read error\n", tsx_path);
			exit(1);
		}
		close(fd);
	}
}

//cpu 개수 구하기
void get_cpus() {
	DIR *dir; //디렉토리 포인터
	struct dirent *dp; //디렉토리 엔트리 포인터
	char *path = "/sys/devices/system/cpu";
	if((dir = opendir(path)) == NULL) {
		fprintf(stderr, "%s directory open error\n", path);
		exit(1);
	}

	while((dp = readdir(dir)) != NULL) {
		if(!strncmp(dp->d_name, "cpu", 3)) { //파일명이 cpu로 시작하는 경우
			if(isdigit(dp->d_name[3])) //cpuN인 경우
				cpu++; //cpu 개수를 셈
		}
	}
}

//L1d, L1i, L2, L3 cache구하기
void get_cache() {
	char *L1d_path = "/sys/devices/system/cpu/cpu0/cache/index0/size";
	char *L1d_way =  "/sys/devices/system/cpu/cpu0/cache/index0/ways_of_associativity";
	char *L1d_type =  "/sys/devices/system/cpu/cpu0/cache/index0/type";
	char *L1d_level =  "/sys/devices/system/cpu/cpu0/cache/index0/level";

	char *L1i_path = "/sys/devices/system/cpu/cpu0/cache/index1/size";
	char *L1i_way =  "/sys/devices/system/cpu/cpu0/cache/index1/ways_of_associativity";
	char *L1i_type =  "/sys/devices/system/cpu/cpu0/cache/index1/type";
	char *L1i_level =  "/sys/devices/system/cpu/cpu0/cache/index1/level";

	char *L2_path = "/sys/devices/system/cpu/cpu0/cache/index2/size";
	char *L2_way =  "/sys/devices/system/cpu/cpu0/cache/index2/ways_of_associativity";
	char *L2_type =  "/sys/devices/system/cpu/cpu0/cache/index2/type";
	char *L2_level =  "/sys/devices/system/cpu/cpu0/cache/index2/level";

	char *L3_path = "/sys/devices/system/cpu/cpu0/cache/index3/size";
	char *L3_way =  "/sys/devices/system/cpu/cpu0/cache/index3/ways_of_associativity";
	char *L3_type =  "/sys/devices/system/cpu/cpu0/cache/index3/type";
	char *L3_level =  "/sys/devices/system/cpu/cpu0/cache/index3/level";

	int fd;
	int rst;
	char tmp[SHORT];

	//L1d cache 구하기
	if(access(L1d_path, F_OK) == 0) {
		memset(tmp, 0, SHORT);
		if((fd = open(L1d_path, O_RDONLY)) < 0) {
			fprintf(stderr, "%s file open error\n", L1d_path);
			exit(1);
		}
		if(read(fd, tmp, SHORT) == 0) {
			fprintf(stderr, "%s file read error\n", L1d_path);
			exit(1);
		}
		close(fd);
		rst = get_value(tmp); //크기에 core를 곱한다.

		if(rst < 1024)
			sprintf(L1d_C.one_size, "%dK", rst);
		else if(rst < 1024*1024)
			sprintf(L1d_C.one_size, "%dM", rst / 1024);
		else
			sprintf(L1d_C.one_size, "%dG", rst / (1024*1024));

		if(rst*core < 1024) {
			sprintf(L1d, "%d KiB", rst*core);
			sprintf(L1d_C.all_size, "%dK", rst*core);
		}
		else if(rst*core < 1024 *1024) {
			sprintf(L1d, "%d MiB", rst*core / 1024);
			sprintf(L1d_C.all_size, "%dM", rst*core / 1024);
		}
		else {
			sprintf(L1d, "%d GiB", rst*core / (1024 *1024));
			sprintf(L1d_C.all_size, "%dG", rst*core / (1024*1024));
		}
	}

	if(access(L1d_way, F_OK) == 0) {
		if((fd = open(L1d_way, O_RDONLY)) < 0) {
			fprintf(stderr, "%s file open error\n", L1d_way);
			exit(1);
		}
		if(read(fd, L1d_C.ways, 30) == 0) {
			fprintf(stderr, "%s file read error\n", L1d_way);
			exit(1);
		}
		close(fd);
	}

	if(access(L1d_type, F_OK) == 0) {
		if((fd = open(L1d_type, O_RDONLY)) < 0) {
			fprintf(stderr, "%s file open error\n", L1d_type);
			exit(1);
		}
		if(read(fd, L1d_C.type, 30) == 0) {
			fprintf(stderr, "%s file read error\n", L1d_type);
			exit(1);
		}
		close(fd);
	}

	if(access(L1d_level, F_OK) == 0) {
		if((fd = open(L1d_level, O_RDONLY)) < 0) {
			fprintf(stderr, "%s file open error\n", L1d_level);
			exit(1);
		}
		if(read(fd, L1d_C.level, 30) == 0) {
			fprintf(stderr, "%s file read error\n", L1d_level);
			exit(1);
		}
		close(fd);
	}



	//L1i cache 구하기
	if(access(L1i_path, F_OK) == 0) {
		memset(tmp, 0, SHORT);

		if((fd = open(L1i_path, O_RDONLY)) < 0) {
			fprintf(stderr, "%s file open error\n", L1i_path);
			exit(1);
		}
		if(read(fd, tmp, SHORT) == 0) {
			fprintf(stderr, "%s file read error\n", L1i_path);
			exit(1);
		}
		close(fd);
		rst = get_value(tmp); //크기를 숫자로 바꾼다

		if(rst < 1024)
			sprintf(L1i_C.one_size, "%dK", rst);
		else if(rst < 1024*1024)
			sprintf(L1i_C.one_size, "%dM", rst / 1024);
		else
			sprintf(L1i_C.one_size, "%dG", rst / (1024*1024));

		if(rst*core < 1024) {
			sprintf(L1i, "%d KiB", rst*core);
			sprintf(L1i_C.all_size, "%dK", rst*core);
		}
		else if(rst*core < 1024 *1024) {
			sprintf(L1i, "%d MiB", rst*core / 1024);
			sprintf(L1i_C.all_size, "%dM", rst*core / 1024);
		}
		else {
			sprintf(L1i, "%d GiB", rst*core / (1024 *1024));
			sprintf(L1i_C.all_size, "%dG", rst*core / (1024*1024));
		}
	}


	if(access(L1i_way, F_OK) == 0) {
		if((fd = open(L1i_way, O_RDONLY)) < 0) {
			fprintf(stderr, "%s file open error\n", L1i_way);
			exit(1);
		}
		if(read(fd, L1i_C.ways, 30) == 0) {
			fprintf(stderr, "%s file read error\n", L1i_way);
			exit(1);
		}
		close(fd);
	}

	if(access(L1i_type, F_OK) == 0) {
		if((fd = open(L1i_type, O_RDONLY)) < 0) {
			fprintf(stderr, "%s file open error\n", L1i_type);
			exit(1);
		}
		if(read(fd, L1i_C.type, 30) == 0) {
			fprintf(stderr, "%s file read error\n", L1i_type);
			exit(1);
		}
		close(fd);
	}

	if(access(L1i_level, F_OK) == 0) {
		if((fd = open(L1i_level, O_RDONLY)) < 0) {
			fprintf(stderr, "%s file open error\n", L1i_level);
			exit(1);
		}
		if(read(fd, L1i_C.level, 30) == 0) {
			fprintf(stderr, "%s file read error\n", L1i_level);
			exit(1);
		}
		close(fd);
	}


	//L2 cache 구하기
	if(access(L2_path, F_OK) == 0) {
		memset(tmp, 0, SHORT);

		if((fd = open(L2_path, O_RDONLY)) < 0) {
			fprintf(stderr, "%s file open error\n", L2_path);
			exit(1);
		}
		if(read(fd, tmp, SHORT) == 0) {
			fprintf(stderr, "%s file read error\n", L2_path);
			exit(1);
		}
		close(fd);
		rst = get_value(tmp); //크기에 core를 곱한다.

		if(rst < 1024)
			sprintf(L2_C.one_size, "%dK", rst);
		else if(rst < 1024*1024)
			sprintf(L2_C.one_size, "%dM", rst / 1024);
		else
			sprintf(L2_C.one_size, "%dG", rst / (1024*1024));

		if(rst*core < 1024) {
			sprintf(L2, "%d KiB", rst*core);
			sprintf(L2_C.all_size, "%dK", rst*core);
		}
		else if(rst*core < 1024 *1024) {
			sprintf(L2, "%d MiB", rst*core / 1024);
			sprintf(L2_C.all_size, "%dM", rst*core / 1024);
		}
		else {
			sprintf(L2, "%d GiB", rst*core / (1024 *1024));
			sprintf(L2_C.all_size, "%dG", rst*core / (1024*1024));
		}

	}


	if(access(L2_way, F_OK) == 0) {
		if((fd = open(L2_way, O_RDONLY)) < 0) {
			fprintf(stderr, "%s file open error\n", L2_way);
			exit(1);
		}
		if(read(fd, L2_C.ways, 30) == 0) {
			fprintf(stderr, "%s file read error\n", L2_way);
			exit(1);
		}
		close(fd);
	}

	if(access(L2_type, F_OK) == 0) {
		if((fd = open(L2_type, O_RDONLY)) < 0) {
			fprintf(stderr, "%s file open error\n", L2_type);
			exit(1);
		}
		if(read(fd, L2_C.type, 30) == 0) {
			fprintf(stderr, "%s file read error\n", L2_type);
			exit(1);
		}
		close(fd);
	}

	if(access(L2_level, F_OK) == 0) {
		if((fd = open(L2_level, O_RDONLY)) < 0) {
			fprintf(stderr, "%s file open error\n", L2_level);
			exit(1);
		}
		if(read(fd, L2_C.level, 30) == 0) {
			fprintf(stderr, "%s file read error\n", L2_level);
			exit(1);
		}
		close(fd);
	}



	//L3 cache 구하기
	if(access(L3_path, F_OK) == 0) {
		memset(tmp, 0, SHORT);

		if((fd = open(L3_path, O_RDONLY)) < 0) {
			fprintf(stderr, "%s file open error\n", L3_path);
			exit(1);
		}
		if(read(fd, tmp, SHORT) == 0) {
			fprintf(stderr, "%s file read error\n", L3_path);
			exit(1);
		}
		close(fd);
		rst = get_value(tmp); //크기에 core를 곱한다.

		if(rst < 1024)
			sprintf(L3_C.one_size, "%dK", rst);
		else if(rst < 1024*1024)
			sprintf(L3_C.one_size, "%dM", rst / 1024);
		else
			sprintf(L3_C.one_size, "%dG", rst / (1024*1024));

		if(rst < 1024) {
			sprintf(L3, "%d KiB", rst);
			sprintf(L3_C.all_size, "%dK", rst);
		}
		else if(rst < 1024 *1024) {
			sprintf(L3, "%d MiB", rst / 1024);
			sprintf(L3_C.all_size, "%dM", rst / 1024);
		}
		else {
			sprintf(L3, "%d GiB", rst / (1024 *1024));
			sprintf(L3_C.all_size, "%dG", rst / (1024*1024));
		}
	}

	if(access(L3_way, F_OK) == 0) {
		if((fd = open(L3_way, O_RDONLY)) < 0) {
			fprintf(stderr, "%s file open error\n", L3_way);
			exit(1);
		}
		if(read(fd, L3_C.ways, 30) == 0) {
			fprintf(stderr, "%s file read error\n", L3_way);
			exit(1);
		}
		close(fd);
	}

	if(access(L3_type, F_OK) == 0) {
		if((fd = open(L3_type, O_RDONLY)) < 0) {
			fprintf(stderr, "%s file open error\n", L3_type);
			exit(1);
		}
		if(read(fd, L3_C.type, 30) == 0) {
			fprintf(stderr, "%s file read error\n", L3_type);
			exit(1);
		}
		close(fd);
	}

	if(access(L3_level, F_OK) == 0) {
		if((fd = open(L3_level, O_RDONLY)) < 0) {
			fprintf(stderr, "%s file open error\n", L3_level);
			exit(1);
		}
		if(read(fd, L3_C.level, 30) == 0) {
			fprintf(stderr, "%s file read error\n", L3_level);
			exit(1);
		}
		close(fd);
	}


}

//online cpu 얻기
void get_online() {
	char *path = "/sys/devices/system/cpu/online";
	int fd = 0;
	if(access(path, F_OK) == 0) {
		if((fd = open(path, O_RDONLY)) < 0) {
			fprintf(stderr, "%s file open error\n", path);
			exit(1);
		}
		if(read(fd, online_cpu, SHORT) == 0) {
			fprintf(stderr, "%s file open error\n", path);
			exit(1);
		}
		close(fd);
	}
}

//cpu max/min Mhz 얻기
void get_Mhz() {
	char *maxpath = "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq";
	char *minpath = "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_min_freq";
	char tmp[SHORT];
	memset(tmp, 0, SHORT);
	int fd;
	if(access(maxpath, F_OK) == 0) {
		if((fd = open(maxpath, O_RDONLY)) < 0) {
			fprintf(stderr, "%s file open error\n", maxpath);
			exit(1);
		}
		if(read(fd, tmp, SHORT) == 0) {
			fprintf(stderr, "%s file open error\n", maxpath);
			exit(1);
		}
		close(fd);
	}
	int len = strlen(tmp);
	int index = 0;
	for(int i = 0; i < len; i++) {
		max_mhz[index++] = tmp[i];
		if(i == len-5) max_mhz[index++] = '.';
	}

	memset(tmp, 0, SHORT);
	if(access(minpath, F_OK) == 0) {
		if((fd = open(minpath, O_RDONLY)) < 0) {
			fprintf(stderr, "%s file open error\n", minpath);
			exit(1);
		}
		if(read(fd, tmp, SHORT) == 0) {
			fprintf(stderr, "%s file open error\n", minpath);
			exit(1);
		}
		close(fd);
	}

	len = strlen(tmp);
	index = 0;
	for(int i = 0; i < len; i++) {
		min_mhz[index++] = tmp[i];
		if(i == len-5) min_mhz[index++] = '.';
	}

}

//NUMA node 정보 얻기
void get_NUMA() {
	char *path = "/sys/devices/system/node";
	char nodepath[SHORT];
	DIR *dir;
	struct dirent *dp;
	int fd;
	if(access(path, F_OK)) return;
	if((dir = opendir(path)) == NULL) {
		fprintf(stderr, "%s directory open error\n", path);
		exit(1);
	}

	while((dp = readdir(dir)) != NULL) {
		if(!strncmp(dp->d_name, "node", 4)) { //node로 시작하는 directory인 경우
			memset(nodepath, 0, SHORT);
			strcpy(nodepath, path);
			strcat(nodepath, "/");
			strcat(nodepath, dp->d_name);
			strcat(nodepath, "/cpulist");
			if((fd = open(nodepath, O_RDONLY)) < 0) {
				fprintf(stderr, "%s file open error\n", nodepath);
				exit(1);
			}
			if(read(fd, NUMA_nodes[NUMA_node], SHORT) == 0) {
				fprintf(stderr, "%s file read error\n", nodepath);
				exit(1);
			}
			close(fd);
			NUMA_node++; //NUMA node의 수 증가
		}
	}
}

//Architecture 얻기
void get_arch() {
	struct utsname u;
	uname(&u);
	strcpy(architecture, u.machine);
}

//byte order 얻기
void get_byte_order() {
	volatile uint32_t i = 0x01234567;
	if((*((uint8_t*)(&i))) == 0x67) 
		strcpy(byte_order, "Little Endian");
	else
		strcpy(byte_order, "Big Endian");
}

void print() {
	int len = 33;
	int start = 0;
	struct winsize win;
	if(ioctl(0, TIOCGWINSZ, (char*)&win) < 0) {
		fprintf(stderr, "ioctl error\n");
		exit(1);
	}

	if(strlen(architecture)) {
		printf("%-33s", "Architecture:");
		printf("%s\n", architecture);
	}

	if(strlen(op_mode)) {
		printf("%-33s", "CPU op-mode(s):");
		printf("%s\n", op_mode);
	}

	if(strlen(byte_order)) {
		printf("%-33s", "Byte Order:");
		printf("%s\n", byte_order);
	}

	if(strlen(address_size)) {
		printf("%-33s", "Address sizes:");
		printf("%s\n", address_size);
	}

	printf("%-33s", "CPU(s):");
	printf("%d\n", cpu);

	if(strlen(online_cpu)) {
		printf("%-33s", "On-line CPU(s) list:");
		printf("%s\n", online_cpu);
	}
	printf("%-33s", "Thread(s) per core:");
	printf("%d\n", thread_per_core);

	printf("%-33s", "Core(s) per socket:");
	printf("%d\n", core_per_socket);

	printf("%-33s", "Socket(s):");
	printf("%d\n", socket);

	if(NUMA_node) {
		printf("%-33s", "NUMA node(s):");
		printf("%d\n", NUMA_node);
	}

	if(strlen(vendor_id)) {
		printf("%-33s", "Vendor ID:");
		printf("%s\n", vendor_id);
	}

	printf("%-33s", "CPU family:");
	printf("%d\n", cpu_family);

	if(strlen(model)) {
		printf("%-33s", "Model:");
		printf("%s\n", model);
	}

	if(strlen(model_name)) {
		printf("%-33s", "Model name:");
		printf("%s\n", model_name);
	}

	printf("%-33s", "Stepping:");
	printf("%d\n", stepping);

	if(strlen(mhz)) {
		printf("%-33s", "CPU MHz:");
		printf("%s\n", mhz);
	}

	if(strlen(max_mhz)) {
		printf("%-33s", "CPU max MHz:");
		printf("%s\n", max_mhz);
	}

	if(strlen(min_mhz)) {
		printf("%-33s", "CPU min MHz:");
		printf("%s\n", min_mhz);
	}

	if(strlen(bogomips)) {
		printf("%-33s", "BogoMIPS:");
		printf("%s\n", bogomips);
	}

	if(strlen(virtualization)) {
		printf("%-33s", "Virtualization:");
		printf("%s\n", virtualization);
	}

	if(strlen(L1d)) {
		printf("%-33s", "L1d cache:");
		printf("%s\n", L1d);
	}

	if(strlen(L1i)) {
		printf("%-33s", "L1i cache:");
		printf("%s\n", L1i);
	}

	if(strlen(L2)) {
		printf("%-33s", "L2 cache:");
		printf("%s\n", L2);
	}

	if(strlen(L3)) {
		printf("%-33s", "L3 cache:");
		printf("%s\n", L3);
	}
	for(int i = 0; i < NUMA_node; i++) {
		char tmp[SHORT];
		memset(tmp, 0, SHORT);
		sprintf(tmp, "NUMA node%d CPU(s):", i);
		printf("%-33s", tmp);
		printf("%s\n", NUMA_nodes[i]);
	}

	if(strlen(itlb_multihit)) {
		start = 0;
		printf("%-33s", "Vulnerability Itlb multihit:");
		if(strlen(itlb_multihit) > win.ws_col - len) {
			while(true) {
				for(int i = 0; i < win.ws_col-len; i++) {
					putc(itlb_multihit[start++], stdout);
				}
				if(itlb_multihit[start] != '\0')
					printf("%33s", " ");
				else {
					printf("\n");
					break;
				}
			}
		}
		else {
			printf("%s\n", itlb_multihit);
		}
	}

	if(strlen(l1tf)) {
		start = 0;
		printf("%-33s", "Vulnerability L1tf:");
		if(strlen(l1tf) > win.ws_col - len) {
			while(true) {
				for(int i = 0; i < win.ws_col-len; i++) {
					putc(l1tf[start++], stdout);
				}
				if(l1tf[start] != '\0')
					printf("%33s", " ");
				else {
					printf("\n");
					break;
				}
			}
		}
		else {
			printf("%s\n", l1tf);
		}
	}

	if(strlen(mds)) {
		start = 0;
		printf("%-33s", "Vulnerability Mds:");
		if(strlen(mds) > win.ws_col - len) {
			while(true) {
				for(int i = 0; i < win.ws_col-len; i++) {
					putc(mds[start++], stdout);
				}
				if(mds[start] != '\0')
					printf("%33s", " ");
				else {
					printf("\n");
					break;
				}
			}
		}
		else {
			printf("%s\n", mds);
		}
	}

	if(strlen(meltdown)) {
		start = 0;
		printf("%-33s", "Vulnerability Meltdown:");
		if(strlen(meltdown) > win.ws_col - len) {
			while(true) {
				for(int i = 0; i < win.ws_col-len; i++) {
					putc(meltdown[start++], stdout);
				}
				if(meltdown[start] != '\0')
					printf("%33s", " ");
				else {
					printf("\n");
					break;
				}
			}
		}
		else {
			printf("%s\n", meltdown);
		}
	}

	if(strlen(spec_store_bypass)) {
		start = 0;
		printf("%-33s", "Vulnerability Spec store bypass:");
		if(strlen(spec_store_bypass) > win.ws_col - len) {
			while(true) {
				for(int i = 0; i < win.ws_col-len; i++) {
					putc(spec_store_bypass[start++], stdout);
				}
				if(spec_store_bypass[start] != '\0')
					printf("%33s", " ");
				else {
					printf("\n");
					break;
				}
			}
		}
		else {
			printf("%s\n", spec_store_bypass);
		}
	}

	if(strlen(spectre_v1)) {
		start = 0;
		printf("%-33s", "Vulnerability Spectre v1:");
		if(strlen(spectre_v1) > win.ws_col - len) {
			while(true) {
				for(int i = 0; i < win.ws_col-len; i++) {
					putc(spectre_v1[start++], stdout);
				}
				if(spectre_v1[start] != '\0')
					printf("%33s", " ");
				else {
					printf("\n");
					break;
				}
			}
		}
		else {
			printf("%s\n", spectre_v1);
		}
	}

	if(strlen(spectre_v2)) {
		start = 0;
		printf("%-33s", "Vulnerability Spectre v2:");
		if(strlen(spectre_v2) > win.ws_col - len) {
			while(true) {
				for(int i = 0; i < win.ws_col-len; i++) {
					putc(spectre_v2[start++], stdout);
				}
				if(spectre_v2[start] != '\0')
					printf("%33s", " ");
				else {
					printf("\n");
					break;
				}
			}
		}
		else {
			printf("%s\n", spectre_v2);
		}
	}

	if(strlen(srbds)) {
		start = 0;
		printf("%-33s", "Vulnerability Srbds:");
		if(strlen(srbds) > win.ws_col - len) {
			while(true) {
				for(int i = 0; i < win.ws_col-len; i++) {
					putc(srbds[start++], stdout);
				}
				if(srbds[start] != '\0')
					printf("%33s", " ");
				else {
					printf("\n");
					break;
				}
			}
		}
		else {
			printf("%s\n", srbds);
		}
	}

	if(strlen(tsx_async_abort)) {
		start = 0;
		printf("%-33s", "Vulnerability Tsx async abort:");
		if(strlen(tsx_async_abort) > win.ws_col - len) {
			while(true) {
				for(int i = 0; i < win.ws_col-len; i++) {
					putc(tsx_async_abort[start++], stdout);
				}
				if(tsx_async_abort[start] != '\0')
					printf("%33s", " ");
				else {
					printf("\n");
					break;
				}
			}
		}
		else {
			printf("%s\n", tsx_async_abort);
		}
	}

	if(strlen(flags)) {
		start = 0;
		printf("%-33s", "Flags:");
		if(strlen(flags) > win.ws_col - len) {
			while(true) {
				for(int i = 0; i < win.ws_col-len; i++) {
					putc(flags[start++], stdout);
				}
				if(flags[start] != '\0') {
					printf("%33s", " ");
				}
				else {
					printf("\n");
					break;
				}
			}
		}
		else {
			printf("%s\n", flags);
		}
	}
}

void print2() {
	printf("NAME ONE-SIZE ALL-SIZE WAYS TYPE        LEVEL\n");
	printf("L1d  %8s %8s %4s %-12s %4s\n", L1d_C.one_size, L1d_C.all_size, L1d_C.ways, L1d_C.type, L1d_C.level);
	printf("L1i  %8s %8s %4s %-12s %4s\n", L1i_C.one_size, L1i_C.all_size, L1i_C.ways, L1i_C.type, L1i_C.level);
	printf("L2   %8s %8s %4s %-12s %4s\n", L2_C.one_size, L2_C.all_size, L2_C.ways, L2_C.type, L2_C.level);
	printf("L3   %8s %8s %4s %-12s %4s\n", L3_C.one_size, L3_C.all_size, L3_C.ways, L3_C.type, L3_C.level);

}

int main(int argc, char **argv) {
	get_cpuinfo();
	get_vulnerability();
	get_cpus();
	get_cache();
	get_online();
	get_Mhz();
	get_NUMA();
	get_arch();
	get_byte_order();
	socket = processor / siblings;
	core_per_socket = core / socket;
	//뒤에 개행 지우기
	delete_n(architecture);
	delete_n(op_mode);
	delete_n(byte_order);
	delete_n(address_size);
	delete_n(online_cpu);
	delete_n(vendor_id);
	delete_n(model);
	delete_n(model_name);
	delete_n(mhz);
	delete_n(max_mhz);
	delete_n(min_mhz);
	delete_n(bogomips);
	delete_n(virtualization);
	delete_n(L1d);
	delete_n(L1i);
	delete_n(L2);
	delete_n(L3);
	for(int i = 0; i < NUMA_node; i++)
		delete_n(NUMA_nodes[i]);
	delete_n(itlb_multihit);
	delete_n(l1tf);
	delete_n(mds);
	delete_n(meltdown);
	delete_n(spec_store_bypass);
	delete_n(spectre_v1);
	delete_n(spectre_v2);
	delete_n(srbds);
	delete_n(tsx_async_abort);
	delete_n(flags);

	delete_n(L1d_C.one_size);
	delete_n(L1d_C.ways);
	delete_n(L1d_C.type);
	delete_n(L1d_C.level);

	delete_n(L1i_C.one_size);
	delete_n(L1i_C.ways);
	delete_n(L1i_C.type);
	delete_n(L1i_C.level);

	delete_n(L2_C.one_size);
	delete_n(L2_C.ways);
	delete_n(L2_C.type);
	delete_n(L2_C.level);

	delete_n(L3_C.one_size);
	delete_n(L3_C.ways);
	delete_n(L3_C.type);
	delete_n(L3_C.level);




	//앞에 불필요한 공백 지우기
	delete_blank(architecture);
	delete_blank(op_mode);
	delete_blank(byte_order);
	delete_blank(address_size);
	delete_blank(online_cpu);
	delete_blank(vendor_id);
	delete_blank(model);
	delete_blank(model_name);
	delete_blank(mhz);
	delete_blank(max_mhz);
	delete_blank(min_mhz);
	delete_blank(bogomips);
	delete_blank(virtualization);
	delete_blank(L1d);
	delete_blank(L1i);
	delete_blank(L2);
	delete_blank(L3);
	for(int i = 0; i < NUMA_node; i++)
		delete_blank(NUMA_nodes[i]);
	delete_blank(itlb_multihit);
	delete_blank(l1tf);
	delete_blank(mds);
	delete_blank(meltdown);
	delete_blank(spec_store_bypass);
	delete_blank(spectre_v1);
	delete_blank(spectre_v2);
	delete_blank(srbds);
	delete_blank(tsx_async_abort);
	delete_blank(flags);

	delete_blank(L1d_C.one_size);
	delete_blank(L1d_C.ways);
	delete_blank(L1d_C.type);
	delete_blank(L1d_C.level);

	delete_blank(L1i_C.one_size);
	delete_blank(L1i_C.ways);
	delete_blank(L1i_C.type);
	delete_blank(L1i_C.level);

	delete_blank(L2_C.one_size);
	delete_blank(L2_C.ways);
	delete_blank(L2_C.type);
	delete_blank(L2_C.level);

	delete_blank(L3_C.one_size);
	delete_blank(L3_C.ways);
	delete_blank(L3_C.type);
	delete_blank(L3_C.level);


	if(argc == 1)
		print();
	else if(argc == 2 && !strcmp(argv[1], "-C")) {
		print2();
	}
}
