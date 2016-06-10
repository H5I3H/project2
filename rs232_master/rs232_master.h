#ifndef RS232_MASTER_H_
#define RS232_MASTER_H_

#define DEVICE_NAME "rs232_master"

#define RS232_MASTER_BUF_SIZE 65536

#define LISTEN_PORT 9999

/* This should be 8 bit */
#define IOCTL_MAGIC 0x90

#define IOCTL_SENDTOSLAVE _IOW(IOCTL_MAGIC, 0, size_t)

#endif /* RS232_MASTER_H_ */


#ifndef RS232_SLAVER_H_
#define RS232_SLAVER_H_

#define DEVICE_NAME "rs232_slave"

#define RS232_SLAVE_DATA_SIZE 65536

#define RS232_SLAVE_CONNECT_PORT 9999

#define RS232_SLAVE_IOC_MAGIC	0x90  //  choose one number after consulting ioctl-number.txt 's'

#define RS232_SLAVE_OPENCONN	_IOW(RS232_SLAVE_IOC_MAGIC, 0, unsigned int)

#define RS232_SLAVE_CLOSECONN	_IO(RS232_SLAVE_IOC_MAGIC, 1)

#define RS232_SLAVE_RECEIVEFROMMASTER	_IOW(RS232_SLAVE_IOC_MAGIC, 0, size_t

#endif /* RS232_SLAVER_H_ */