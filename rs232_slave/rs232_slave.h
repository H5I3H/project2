#ifndef RS232_SLAVER_H_
#define RS232_SLAVER_H_

#define DEVICE_NAME "rs232_slave"

#define RS232_SLAVE_DATA_SIZE 65536

#define RS232_SLAVE_CONNECT_PORT 9999

#define IOCTL_MAGIC	0x90  //  choose one number after consulting ioctl-number.txt 's'

#define IOCTL_OPENCONN	_IOW(IOCTL_MAGIC, 0, unsigned int)

#define IOCTL_CLOSECONN	_IO(IOCTL_MAGIC, 1)

#define IOCTL_RECEIVEFROMMASTER	_IOW(IOCTL_MAGIC, 0, size_t

#endif /* RS232_SLAVER_H_ */