#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/workqueue.h>
#include <linux/semaphore.h>
#include <linux/ioctl.h>
#include <linux/slab.h> /* kmalloc, kfree */
#include <linux/errno.h>
#include <net/sock.h>
#include <net/tcp.h>
#include <asm/atomic.h>
#include "rs232_slave.h"

MODULE_LICENSE("GPL");

#ifndef VM_RESERVED
# define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)
#endif

static int __init rs232_slave_init(void);
static void __exit rs232_slave_exit(void);

/* file operations */
static int rs232_slave_open( struct inode *inode, struct file *filp );
static int rs232_slave_close( struct inode *inode, struct file *filp );
static loff_t rs232_slave_llseek( struct file *filp, loff_t off, int whence );
static long rs232_slave_ioctl( struct file *filp, unsigned int ioctl_num, unsigned long ioctl_param );
static ssize_t rs232_slave_read( struct file *filp, char __user *buff, size_t count, loff_t *offp );
static ssize_t rs232_slave_write( struct file *filp, const char __user *buff, size_t count, loff_t *offp );
//static int rs232_slave_mmap( struct file *filp, struct vm_area_struct *vma );

/* connection (should be called while holding dev_mutex) */
static int open_connection(unsigned int ip);
static int close_connection(void);

/* receive data from master device (should be called while holding dev_mutex) */
static ssize_t receive_from_master( struct socket *sock, char *buf, size_t size );

/* Some device variables*/
static struct class *dev_class;
static dev_t dev_no;
static struct cdev cdev;


static char * kernel_data; /* Allocate by kmalloc, free by kfree */
static struct socket *conn_sock; /* connect kernel socket */
static size_t data_len;
static size_t unread_index;

static int dev_ready;   /* if the device is ready to be used */

DEFINE_SEMAPHORE(dev_mutex);

static struct file_operations fops = {
        .owner = THIS_MODULE,
        .open = rs232_slave_open,
        .release = rs232_slave_close,
        .llseek = rs232_slave_llseek,
        .unlocked_ioctl = rs232_slave_ioctl,
        .read = rs232_slave_read,
        .write = rs232_slave_write
};

/* module entry/exit points */
static int __init rs232_slave_init(void)
{
	int ret;
	void *ret_ptr;

	/*allocate device*/
	if ((ret = alloc_chrdev_region(&dev_no, 0, 1, "rs232_slave")) < 0) {
		printk(KERN_ERR "[rs232_slave] " "alloc_chrdev_region returned %d\n", ret);
		goto alloc_chrdev_region_failed;
	}
	if (IS_ERR((dev_class = class_create(THIS_MODULE, "rs232_slave")))) {
		ret = PTR_ERR(dev_class);
		printk(KERN_ERR "[rs232_slave] " "class_create returned %d\n", ret);
		goto class_create_failed;
	}
	if (IS_ERR((ret_ptr = device_create( dev_class, NULL, dev_no, NULL, "rs232_slave")))) {
		ret = PTR_ERR(ret_ptr);
		printk(KERN_ERR "[rs232_slave] " "device_create returned %d\n", ret);
		goto device_create_failed;
	}
	cdev_init(&cdev, &fops);
	cdev.owner = THIS_MODULE;
	if ((ret = cdev_add(&cdev, dev_no, 1)) < 0) {
		printk(KERN_ERR "[rs232_slave] " "cdev_add returned %d\n", ret);
		goto cdev_add_failed;
	}

	/* initialize buffer */
	if ((kernel_data = kmalloc(RS232_SLAVE_DATA_SIZE, GFP_KERNEL)) == NULL) {
		printk(KERN_ERR "[rs232_slave] " "kmalloc returned NULL\n");
		ret = -ENOMEM;
		goto kmalloc_failed;
	}

	/* initialize device */
	data_len = 0;
	unread_index = 0;
	dev_ready = 0;

	printk(KERN_INFO "[rs232_slave] " "RS232 slave device initialized\n");
	return 0;

 /* Error handling */
kmalloc_failed:
	cdev_del(&cdev);
cdev_add_failed:
	device_destroy(dev_class, dev_no);
device_create_failed:
	class_destroy(dev_class);
class_create_failed:
	unregister_chrdev_region(dev_no, 1);
alloc_chrdev_region_failed:
	return ret;

}

static void __exit rs232_slave_exit(void)
{
	/* de-allocate device */
	cdev_del(&cdev);
	device_destroy(dev_class, dev_no);
	class_destroy(dev_class);
	unregister_chrdev_region(dev_no, 1);
	kfree(kernel_data);	/* free buffer */
	printk(KERN_INFO "[rs232_slave] " "RS232 slave device finalized\n");
}
module_init(rs232_slave_init);
module_exit(rs232_slave_exit);

static int rs232_slave_open( struct inode *inode, struct file *filp )
{
	return 0;
}
static int rs232_slave_close( struct inode *inode, struct file *filp )
{
	return 0;
}
static loff_t rs232_slave_llseek( struct file *filp, loff_t off, int whence )
{
	return -1;
}
static long rs232_slave_ioctl( struct file *filp, unsigned int ioctl_num, unsigned long ioctl_param )
{
	int ret;
	unsigned int ip;
	size_t receive_len;
	switch(ioctl_num)
	{
		case IOCTL_OPENCONN:
			if (get_user(ip, (unsigned int __user *)ioctl_param)) {
				printk(KERN_ERR "[%s] " "get_user returned non-zero\n", DEVICE_NAME);
				ret = -EFAULT;
				goto get_user_failed;
			}
			if (down_interruptible(&dev_mutex)) {
				printk(KERN_ERR "[%s] " "down_interruptible returned non-zero\n", DEVICE_NAME);
				ret = -ERESTARTSYS;
				goto down_interruptible_failed;
			}
			if ((ret = open_connection(ip)) < 0) {
				printk(KERN_ERR "[%s] " "open_conn returned %d\n", DEVICE_NAME, ret);
				up(&dev_mutex);
				goto open_connection_failed;
            }
			up(&dev_mutex);

                return ret;
		case IOCTL_CLOSECONN:
			if (down_interruptible(&dev_mutex)) {
				printk(KERN_ERR "[%s] " "down_interruptible returned non-zero\n", DEVICE_NAME);
				ret = -ERESTARTSYS;
				goto down_interruptible_failed;
			}
			if ((ret = close_connection()) < 0) {
				printk(KERN_ERR "[%s] " "close_conn returned %d\n", DEVICE_NAME, ret);
				up(&dev_mutex);
				goto close_connection_failed;
			}
			up(&dev_mutex);

			return ret;
		case IOCTL_RECEIVEFROMMASTER:
			if (get_user(receive_len, (size_t __user *)ioctl_param)) {
					printk(KERN_ERR "[%s] " "get_user returned non-zero\n", DEVICE_NAME);
					ret = -EFAULT;
					goto get_user_failed;
			}
			if (receive_len > RS232_SLAVE_DATA_SIZE) {
				ret = -EINVAL;
				 goto receive_len_error;
			}
			if (down_interruptible(&dev_mutex)) {
				printk(KERN_ERR "[%s] " "down_interruptible returned non-zero\n", DEVICE_NAME);
					ret = -ERESTARTSYS;
					goto down_interruptible_failed;
			}
			if (!dev_ready) {
				up(&dev_mutex);
				return -EAGAIN;
			}
			data_len = 0;
			while (data_len < receive_len) {
				if ((ret = receive_from_master( conn_sock, &kernel_data[data_len], receive_len - data_len)) < 0) 
				{
					printk(KERN_ERR "[%s] " "receive_from_master returned %d\n", DEVICE_NAME, ret);
					up(&dev_mutex);
					goto receive_from_master_failed;
				}  else if (ret == 0) {
					break;
				}
				data_len += ret;
			}
			unread_index = 0;
			up(&dev_mutex);

			return (long)data_len;
		default:
			return -ENOTTY;
	}

	return 0;
/*Error handling*/
get_user_failed:
down_interruptible_failed:
open_connection_failed:
close_connection_failed:
receive_len_error:
receive_from_master_failed:
	
	return ret;
}

static ssize_t rs232_slave_read( struct file *filp, char __user *buff, size_t count, loff_t *offp )
{
	int ret=0;
	size_t total=0;
	size_t read_len;
	int is_eof;

	if (down_interruptible(&dev_mutex)) {
		printk(KERN_ERR "[%s] " "down_interruptible returned non-zero\n", DEVICE_NAME);
		ret = -ERESTARTSYS;
		goto down_interruptible_failed;
	}
	if (!dev_ready) {
		ret = -EAGAIN;
		up(&dev_mutex);
		goto out;
	}
	while ( count > 0) {
		if ( data_len == 0) {
			while ( data_len < RS232_SLAVE_DATA_SIZE ) {
				if ((ret = receive_from_master(
					conn_sock,
					&kernel_data[data_len],
					RS232_SLAVE_DATA_SIZE - data_len)) < 0) {
					printk(KERN_ERR "[%s] " "receive_from_master returned %d\n", DEVICE_NAME, ret);
					up(&dev_mutex);
					goto receive_from_master_failed;
				} else if (ret == 0) {
					is_eof = 1;
					break;
				}
				data_len += ret;
			}
			unread_index = 0;
		}
		read_len = (count < data_len? count : data_len);
		if (copy_to_user( &buff[total], &kernel_data[unread_index], read_len)) {
			printk(KERN_ERR "[%s] " "copy_to_user returned non-zero\n", DEVICE_NAME);
			ret = -EFAULT;
			up(&dev_mutex);
			goto copy_to_user_failed;
		}
		data_len -= read_len;
		unread_index += read_len;

		count -= read_len;
		total += read_len;

		if (is_eof)
			break;
	}
	total = 0;
	is_eof = 0;

out:
	return ret;


/* Error handling */
down_interruptible_failed:
receive_from_master_failed:
copy_to_user_failed:

	return ret;
}

static ssize_t rs232_slave_write( struct file *filp, const char __user *buff, size_t count, loff_t *offp )
{
	 return 0;
}

static int open_connection(unsigned int ip)
{
	int ret;
	int option;
	struct sockaddr_in saddr;
	/* check if the device is ready, if so, return -EBUSY */
	if (dev_ready)
		return -EBUSY;
	/* create conn_sock with SO_REUSEADDR set */
	if ((ret = sock_create_kern( AF_INET, SOCK_STREAM, IPPROTO_TCP, &conn_sock)) < 0) 
	{
		printk(KERN_ERR "[%s] " "sock_create_kern returned %d\n", DEVICE_NAME, ret);
		goto sock_create_kern_failed;
	}
	option = 1;
	if ((ret = kernel_setsockopt(
		conn_sock,
		SOL_SOCKET,
		SO_REUSEADDR,
		(char *)&option,
		sizeof(option))) < 0) {
			printk(KERN_ERR "[%s] " "kernel_sock_setsockopt returned %d\n", DEVICE_NAME, ret);
			goto kernel_setsockopt_failed;
	}

	/* connect to the specified IP and port */
	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(RS232_SLAVE_CONNECT_PORT);
	saddr.sin_addr.s_addr = htonl(ip);
	if ((ret = conn_sock->ops->connect(
		conn_sock, (struct sockaddr *)&saddr, sizeof(saddr), 0)) < 0) {
		printk(KERN_ERR "[%s] " "conn_sock->ops->connect returned %d\n", DEVICE_NAME, ret);
		goto connect_failed;
	}
	/* mark the device as ready */
	dev_ready = 1;
	return 0;

/* Error handling */
connect_failed:
kernel_setsockopt_failed:
        sock_release(conn_sock);
sock_create_kern_failed:
	return ret;
}
static int close_connection(void)
{
	/* check if the device is ready, if not, return -EAGAIN */
	if (!dev_ready)
		return -EAGAIN;
	sock_release(conn_sock);	/* clean up socket */
	dev_ready = 0;				/* mark the device as not ready */
	return 0;
}
static ssize_t receive_from_master( struct socket *sock, char *buf, size_t size )
{
	int ret;
	struct msghdr msg;
	struct iovec iov;

	mm_segment_t oldfs;

	iov.iov_base = buf;
	iov.iov_len = size;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = 0;

	oldfs = get_fs();
	set_fs(KERNEL_DS);
	ret = sock_recvmsg(sock, &msg, size, 0);
	set_fs(oldfs);

	return ret;
}
