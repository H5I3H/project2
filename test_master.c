#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>

int fdin, fddev;
int main(int argc, char* argv[]) {
	void* bufout;
	if((fddev = open("/dev/rs232_master", O_RDWR)) < 0) {
		perror("open device fail");
		exit(1);
	}

	if((bufout = mmap(0, 65536, PROT_WRITE, MAP_SHARED, fddev, 0)) == MAP_FAILED) {
		perror("memory map to device fail.");
		exit(1);
	}

	if(close(fddev) < 0) {
		perror("close device fail");
		exit(1);
	}
	return 0;
}
