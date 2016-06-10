/* Note: linux error number http://www-numi.fnal.gov/offline_software/srt_public_context/WebDocs/Errors/unix_system_errors.html */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/slab.h> /* kmalloc, kfree */
#include <linux/net.h>
#include <net/sock.h>
#include <net/tcp.h>

#define DEVICE_NAME "rs232_master"

#ifndef VM_RESERVED
#define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)
#endif

MODULE_LICENSE("GPL");

static int __init rs232_master_init(void);
static void __exit rs232_master_exit(void);

/* File operations */
static int rs232_master_open(struct inode *inode, struct file *filp);
static int rs232_master_close(struct inode *inode, struct file *filp);

static int open_connection(unsigned short port);
static int close_connection(void);

static ssize_t send(struct socket* client_socket, char* buf, size_t size);

static struct file_operations fops = {
	.owner = THIS_MODULE,
	//.open = rs232_master_open,
	//.release = rs232_master_close
};

/* Some device variable */
static struct class* dev_class;
static dev_t dev_no;
static struct cdev cdev;

static char* kernel_data; /* Allocate by kmalloc, free by kfree */
static struct socket* client_socket; /* Server side kernel socket */
static struct socket* server_socket; /* Client side kernel socket */

/** Create character device follow this procedure:
 * 	Register device number by alloc_chrdev_region
 * 	Initailize cdev object and set its file operations by cdev_init
 * 	Add this device to kernel by cdev_add
 *
 *  Device file(at /dev/FILE_NAME) can be create as follow:
 *  Add new device class by class_create
 *  Create device file with the device number and file name by device_create
 */
static int __init rs232_master_init(void) {
	int ret;
	void* ret_ptr;

	/* Obtain device number from OS */
	if((ret = alloc_chrdev_region(&dev_no, 0, 1, DEVICE_NAME)) < 0) {
		printk(KERN_ERR "[%s] alloc_chrdev_region fail, return %d\n", DEVICE_NAME, ret);
		goto alloc_chrdev_region_fail;
	}
	if (IS_ERR((dev_class = class_create(THIS_MODULE, DEVICE_NAME)))) {
	 	ret = PTR_ERR(dev_class);
	 	printk(KERN_ERR "[%s] class_create fail, return %d\n", DEVICE_NAME, ret);
	 	goto class_create_fail;
	}
	if (IS_ERR((ret_ptr = device_create(dev_class, NULL, dev_no, NULL, DEVICE_NAME)))) {
		ret = PTR_ERR(ret_ptr);
		printk(KERN_ERR "[%s] device_create fail, reutrn %d\n", DEVICE_NAME, ret);
		goto device_create_fail;
	}
	/* Character device setup and registration */
	cdev_init(&cdev, &fops);
	cdev.owner = THIS_MODULE;
	if((ret = cdev_add(&cdev, dev_no, 1)) < 0) {
		printk(KERN_ERR "[%s] cdev_add fail, return %d\n", DEVICE_NAME, ret);
		goto cdev_add_fail;
	}
	
	/* Since our deivce will have read or write file operations, 
	 * we need memory allocation for the device which is done as follow */
	if((kernel_data = kmalloc(65535, GFP_KERNEL)) == NULL) {
		printk(KERN_ERR "[%s] kmalloc fail\n", DEVICE_NAME);
		ret = ENOMEM;
		goto kmalloc_fail;
	}
	printk(KERN_INFO "[%s] Initailization done\n", DEVICE_NAME);

	return 0;

/* Error handling */
kmalloc_fail:
	cdev_del(&cdev);
cdev_add_fail:
	device_destroy(dev_class, dev_no);
device_create_fail:
	class_destroy(dev_class);
class_create_fail:
	unregister_chrdev_region(dev_no, 1);
alloc_chrdev_region_fail:
	return ret;

}

static void __exit rs232_master_final(void) {
	kfree(kernel_data);
	cdev_del(&cdev);
	device_destroy(dev_class, dev_no);
	class_destroy(dev_class);
	unregister_chrdev_region(dev_no, 1);
	printk(KERN_INFO "[%s] Release done\n", DEVICE_NAME);
}
module_init(rs232_master_init);
module_exit(rs232_master_final);

/** Open connection with slave device through kernel socket follow this procedure:
 *  create server size kernel socket
 *  bind server socket to specific port
 *  server socket list to that port
 *  create client socket and wait fot its connection
 */
static int open_connection(unsigned short port) {
	int ret;
	int val = 1;
	struct sockaddr_in saddr;
	memset(&saddr, 0, sizeof(saddr));

	if((ret = sock_create_kern(AF_INET, SOCK_STREAM, IPPROTO_TCP, &server_socket)) < 0) {
		printk(KERN_ERR "[%s] sock_create_kern fail, return %d\n", DEVICE_NAME, ret);
		goto sock_create_kern_fail;
	}
	if((ret = kernel_setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&val, sizeof(val))) < 0) {
		printk(KERN_ERR "[%s] kernel_setsockopt fail, return %d\n", DEVICE_NAME, ret);
		goto kernel_setsockopt_fail;
	}
	saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = INADDR_ANY;
    /* Bind and listen to port you want */
    if((ret = server_socket->ops->bind(server_socket, (struct sockaddr*)&saddr, sizeof(saddr))) < 0) {
    	printk(KERN_ERR "[%s] kernel socket bind fail, return %d\n", DEVICE_NAME, ret);
    	goto bind_fail;
    }
    if((ret = server_socket->ops->listen(server_socket, 1)) < 0) {
    	printk(KERN_ERR "[%s] kernel socket listen fail, return %d\n", DEVICE_NAME, ret);
    	goto listen_fail;
    }
    /* Create kernel socket for slave device(client) and for its connection */
    if((ret = sock_create_kern(AF_INET, SOCK_STREAM, IPPROTO_TCP, &client_socket)) < 0) {
    	printk(KERN_ERR "[%s] sock_create_kern fail, return %d\n", DEVICE_NAME, ret);
    	goto client_socket_create_fail;
    }
    if((ret = server_socket->ops->accept(server_socket, client_socket, 0)) < 0) {
    	printk(KERN_ERR "[%s] accept fail, return %d\n", DEVICE_NAME, ret);
    	goto accept_fail;
    }
    return 0;
	/* Error handling */
accept_fail:
	sock_release(client_socket);
client_socket_create_fail:
listen_fail:
bind_fail:
kernel_setsockopt_fail:
	sock_release(server_socket);
sock_create_kern_fail:
	return ret;
}
static int close_connection(void) {
	sock_release(server_socket);
	sock_release(client_socket);
	return 0;
}

/* Send the data to slave device through kernel socket, here buf is at kernel space */
static ssize_t send(struct socket* client_socket, char* buf, size_t size) {
	int ret;
	struct msghdr msg_header;
	struct iovec iov;

	mm_segment_t old_fs;

	iov.iov_base = buf;
	iov.iov_len = size;

	msg_header.msg_name = NULL;
	msg_header.msg_namelen = 0;
	msg_header.msg_iov = &iov;
	msg_header.msg_iovlen = 1;
	msg_header.msg_control = NULL;
	msg_header.msg_controllen = 0;
	msg_header.msg_flags = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	ret = sock_sendmsg(client_socket, &msg_header, size);
	set_fs(old_fs);

	return ret;
}