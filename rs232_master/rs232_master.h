#ifndef RS232_MASTER_H_
#define RS232_MASTER_H_

#define DEVICE_NAME "rs232_master"

#define RS232_MASTER_BUF_SIZE 65536

#define LISTEN_PORT 9999

/* This should be 8 bit */
#define IOCTL_MAGIC 0x90

#define IOCTL_SENDTOSLAVE _IOW(IOCTL_MAGIC, 0, size_t)

#endif /* RS232_MASTER_H_ */
