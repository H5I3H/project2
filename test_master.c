#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

int fdin, fddev;
int main(int argc, char* argv[]) {
	if((fddev = open("/dev/rs232_master", O_RDWR)) < 0) {
		perror("open device fail");
		exit(1);
	}
	if(close(fddev) < 0) {
		perror("close device fail");
		exit(1);
	}
	return 0;
}
