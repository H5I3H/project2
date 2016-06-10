#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include "rs232_master/rs232_master.h"

static void do_mmap(void);
static void do_fcntl(void);

int fdin, fddev;
size_t total_byte = 0;
int main(int argc, char* argv[]) {
	struct timeval start, end;
	if(argc != 3) {
		fprintf(stderr, "Usage: %s <input_file> <method>\n", argv[0]);
		exit(1);
	}
	if((fddev = open("/dev/rs232_master", O_RDWR)) < 0) {
		perror("open device fail");
		exit(1);
	}
	if(gettimeofday(&start, NULL) < 0) {
		perror("gettimeofday  fail");
		exit(1);
	}
	if((fdin = open(argv[1], O_RDONLY)) < 0) {
		perror("open file fail");
		exit(1);
	}
	if(strncmp("fcntl", argv[2], strlen(argv[2])) == 0)
		do_fcntl();
	else if(strncmp("mmap", argv[2], strlen(argv[2])) == 0)
		do_mmap();
	else {
		fprintf(stderr, "Invalid method, it should be \"fcntl\" or \"mmap\".\n");
		exit(1);
	}
	if(close(fddev) < 0) {
		perror("close device fail");
		exit(1);
	}
	if(close(fdin) < 0) {
		perror("close file fail");
		exit(1);
	}
	if(gettimeofday(&end, NULL) < 0) {
		perror("gettimeofday fail");
		exit(1);
	}
	if(end.tv_usec < start.tv_usec) {
		end.tv_sec -= 1;
		end.tv_usec += 1000000;
	}
	printf("Transmission time: %lf ms, File size: %lu bytes.\n", 
			((end.tv_sec - start.tv_sec) * 1000 + 
			((double)(end.tv_usec - start.tv_usec) / 1000)), total_byte);
	return 0;
}

static void do_fcntl(void) {
	int ret;
	char buf[RS232_MASTER_BUF_SIZE];
	while((ret = read(fdin, buf, sizeof(buf))) > 0) {
		if(write(fddev, buf, ret) < 0) {
			perror("write fail");
			exit(1);
		}
		total_byte += ret;
	}
	if(ret < 0) {
		perror("read fail");
		exit(1);
	}
}

static void do_mmap(void) {
	struct stat stat;
	off_t off, end_off;
	size_t size, send_len;
	void *bufin, *bufdev;
	if(fstat(fdin, &stat) < 0) {
		perror("fstat fail");
		exit(1);
	}
	off = 0;
	end_off = stat.st_size;
	if((bufdev = mmap(0, RS232_MASTER_BUF_SIZE, PROT_WRITE, MAP_SHARED, fddev, 0)) == MAP_FAILED) {
		perror("mmap fail");
		exit(1);
	}
	//while(off < end_off) {
	size = (size_t)(end_off - off);
		//if(RS232_MASTER_BUF_SIZE < size)
		//	size = RS232_MASTER_BUF_SIZE;
	if((bufin = mmap(0, size, PROT_READ, MAP_SHARED, fdin, 0)) == MAP_FAILED) {
		perror("mmap fail");
		exit(1);
	}
	while(off < end_off) {
		send_len = (size_t)(end_off - off);
		if(RS232_MASTER_BUF_SIZE < send_len)
			send_len = RS232_MASTER_BUF_SIZE;
		memcpy(bufdev, &bufin[off], send_len);
		if(ioctl(fddev, IOCTL_SENDTOSLAVE, &send_len) < 0) {
			perror("ioctl fail");
			exit(1);
		}
		off += send_len;
		total_byte += send_len;
	}
	//munmap(bufin, size);
	//off += size;
	//total_byte += size;
	//}
	munmap(bufin, size);
	munmap(bufdev, RS232_MASTER_BUF_SIZE);
}
