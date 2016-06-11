#ifndef RS232_SLAVER_H_
#define RS232_SLAVER_H_

#define DEVICE_NAME "rs232_slave"

#define RS232_SLAVE_DATA_SIZE 65536

#define RS232_SLAVE_CONNECT_PORT 9999

#define ENCODE_IP(ip, ip_arr)         \
	ip = (ip_arr[3] |                 \
	ip_arr[2] << 8  |                 \
	ip_arr[1] << 16 |                 \
	ip_arr[0] << 24)
#define DECODE_IP(ip, ip_arr)           \
	do {                                \
		ip_arr[3] = ip & 0xFF;          \
		ip_arr[2] = (ip >> 8) & 0xFF;   \
		ip_arr[1] = (ip >> 16) & 0xFF;  \
		ip_arr[0] = (ip >> 24) & 0xFF;  \
	} while (0)

#define IOCTL_MAGIC	0x90  //  choose one number after consulting ioctl-number.txt 's'

#define IOCTL_OPENCONN	_IOW(IOCTL_MAGIC, 0, unsigned int)

#define IOCTL_CLOSECONN	_IO(IOCTL_MAGIC, 1)

#define IOCTL_RECEIVEFROMMASTER	_IOW(IOCTL_MAGIC, 0, size_t)

#endif /* RS232_SLAVER_H_ */
