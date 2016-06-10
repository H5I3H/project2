#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define __USE_GNU 
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>

#include "rs232_slave/rs232_slave.h"

static void do_fcntl(void);
static void do_mmap(void);

int fdin, fdout;

int main(int argc, char *argv[])
{
        unsigned int ip, ip_arr[4];
        size_t len;
        
        struct timeval start, end;
        struct stat stat;
        
        if (argc < 4) {
                fprintf(stderr, "usage: %s [output_file] [method] [IP]\n", argv[0]);
                exit(1);
        }
        if ((fdin = open("/dev/rs232_slave", O_RDONLY)) < 0) {
                perror("open");
                exit(1);
        }
		if(gettimeofday(&start, NULL) < 0) {
			perror("gettimeofday");
			exit(1);
		}
        if ((fdout = open(argv[1], O_RDWR | O_TRUNC | O_CREAT, 0644)) < 0) {
                perror("open");
                exit(1);
        }

        if (sscanf(argv[3], "%u.%u.%u.%u",
                   &ip_arr[0], &ip_arr[1], &ip_arr[2], &ip_arr[3]) != 4) {
                fprintf(stderr, "invalid IP\n");
                exit(1);
        }
        ENCODE_IP(ip, ip_arr);
        if (ioctl(fdin, RS232_SLAVE_IOCOPENCONN, &ip) < 0) {
                perror("ioctl");
                exit(1);
        }
        
        len = strlen(argv[2]);
        if (strncmp("fcntl", argv[2], len) == 0) {
                do_fcntl();
        } else if (strncmp("mmap", argv[2], len) == 0) {
                do_mmap();
        } else {
                fprintf(stderr, "invalid method\n");
                exit(1);
        }
        
        if (ioctl(fdin, RS232_SLAVE_IOCCLOSECONN) < 0) {
                perror("ioctl");
                exit(1);
        }
        
        if (close(fdin) < 0) {
                perror("close");
                exit(1);
        }
        if (fstat(fdout, &stat) < 0) {
                perror("fstat");
                exit(1);
        }
        if (close(fdout) < 0) {
                perror("close");
                exit(1);
        }

        if (gettimeofday(&end, NULL) < 0) {
                perror("gettimeofday");
                exit(1);
        }
        
        if (end.tv_usec < start.tv_usec) {
                end.tv_sec -= 1;
                end.tv_usec += 1000000;
        }
        printf("Transmission time: %lf ms, File size: %lu bytes\n",
               ((end.tv_sec - start.tv_sec) * 1000 +
                ((double)(end.tv_usec - start.tv_usec) / 1000)),
               (size_t)(stat.st_size));

        return 0;
}

static void do_fcntl(void)
{
        int ret;

        char buf[RS232_SLAVE_DATA_SIZE];
        
        while ((ret = read(fdin, buf, sizeof(buf))) > 0) {
                if (write(fdout, buf, ret) < 0) {
                        perror("write");
                        exit(1);
                }
        }
        if (ret < 0) {
                perror("read");
                exit(1);
        }
}
static void do_mmap(void)
{
        long ret;
        
        off_t off;
        size_t size;
        void *bufin, *bufout;
		void *temp;

        if ((bufin = mmap(0,
                          RS232_SLAVE_DATA_SIZE,
                          PROT_READ,
                          MAP_SHARED,
                          fdin,
                          0)) == MAP_FAILED) {
                perror("mmap");
                exit(1);
        }

        size = RS232_SLAVE_DATA_SIZE;
        off = 0;
		ret = ioctl(fdin, RS232_SLAVE_IOCRECEIVEFROMMASTER, &size);
		if(ftruncate(fdout, off + ret) < 0) {
			perror("lseek");
			exit(1);
		}
		if((bufout = mmap(0, ret, PROT_WRITE, MAP_SHARED, fdout, off)) == MAP_FAILED) {
			perror("mmap");
			exit(1);
		}
		memcpy(bufout, bufin, ret);
		off += ret;
        while ((ret = ioctl(fdin, RS232_SLAVE_IOCRECEIVEFROMMASTER, &size)) > 0) {
                if (ftruncate(fdout, off + ret) < 0) {
                        perror("lseek");
                        exit(1);
                }
				if((bufout = mremap(bufout, off, off + ret, MREMAP_MAYMOVE)) == MAP_FAILED) {
					perror("mremap");
					exit(1);
				}
                memcpy(&bufout[off], bufin, ret);

                off += ret;
        }
		munmap(bufout, off);
        munmap(bufin, RS232_SLAVE_DATA_SIZE);
}
