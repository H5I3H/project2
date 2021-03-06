/* Note: linux error number http://www-numi.fnal.gov/offline_software/srt_public_context/WebDocs/Errors/unix_system_errors.html */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/slab.h> /* kmalloc, kfree */
#include <linux/net.h>
#include <linux/mm.h>
#include <net/sock.h>
#include <net/tcp.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include "rs232_master.h"

#ifndef VM_RESERVED
#define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)
#endif

MODULE_LICENSE("GPL");

static int __init rs232_master_init(void);
static void __exit rs232_master_final(void);

/* File operations */
static int rs232_master_open(struct inode *inode, struct file *filp);
static int rs232_master_close(struct inode *inode, struct file *filp);
static ssize_t rs232_master_write(struct file* filp, const char __user *buff, size_t data_size, loff_t* offp);
static int rs232_master_mmap(struct file* filp, struct vm_area_struct* vma);
static long rs232_master_ioctl(struct file* filp, unsigned int ioctl_num, unsigned long param);

static void vma_open(struct vm_area_struct* vma);
static void vma_close(struct vm_area_struct* vma);

static int open_connection(unsigned short port);
static int close_connection(void);

static ssize_t send(struct socket* client_socket, char* buf, size_t size);

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = rs232_master_open,
	.release = rs232_master_close,
	.write = rs232_master_write,
	.mmap = rs232_master_mmap,
	.unlocked_ioctl = rs232_master_ioctl,
};

/* Some device variable */
static struct class* dev_class;
static dev_t dev_no;
static struct cdev cdev;

static struct socket* client_socket; /* Client side kernel socket */
static struct socket* server_socket; /* Server side kernel socket */

static char* kernel_buff; /* Buffer in kernel space which is allocate by kmalloc and free by kfree */
static size_t device_ref_count; /* Device reference count, record how many time this device been opened */
static size_t datalen_in_kern_buf;
static size_t unflush_index;

/* vma operation */
static struct vm_operations_struct vmops = {
	.open = vma_open,
	.close = vma_close,
};


DEFINE_SEMAPHORE(device_mutex);

/** 
 * Create character device follow this procedure:
 * Register device number by alloc_chrdev_region
 * Initailize cdev object and set its file operations by cdev_init
 * Add this device to kernel by cdev_add
 *
 * Device file(at /dev/FILE_NAME) can be create as follow:
 * Add new device class by class_create
 * Create device file with the device number and file name by device_create
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
	if((kernel_buff = kmalloc(RS232_MASTER_BUF_SIZE, GFP_KERNEL)) == NULL) {
		printk(KERN_ERR "[%s] kmalloc fail\n", DEVICE_NAME);
		ret = -ENOMEM;
		goto kmalloc_fail;
	}
	device_ref_count = 0;
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
	kfree(kernel_buff);
	cdev_del(&cdev);
	device_destroy(dev_class, dev_no);
	class_destroy(dev_class);
	unregister_chrdev_region(dev_no, 1);
	printk(KERN_INFO "[%s] Release done\n", DEVICE_NAME);
}
module_init(rs232_master_init);
module_exit(rs232_master_final);

static int rs232_master_open(struct inode* inode, struct file* filp) {
	int ret;

	/* Lock critical section */
	if(down_interruptible(&device_mutex)) {
		printk(KERN_ERR "[%s] In rs232_master_open down_interruptible fail.\n", DEVICE_NAME);
		ret = -ERESTARTSYS; /* If down_interruptible fail, it'll return this value */
		goto down_interruptible_fail;
	}
	/* If this is first time open this device, initialize some needed variable */
	if(device_ref_count == 0) {
		datalen_in_kern_buf = 0;
		unflush_index = 0;
		if((ret = open_connection(LISTEN_PORT)) < 0) {
			printk(KERN_ERR "[%s] open_connection fail, reutrn %d.\n", DEVICE_NAME, ret);
			goto open_connection_fail;
		}
	}
	device_ref_count++;
	up(&device_mutex);
	return 0;

open_connection_fail:
	up(&device_mutex);
down_interruptible_fail:
	return ret;
}

static int rs232_master_close(struct inode* inode, struct file* filp) {
	int ret;

	/* Lock critical section */
	if(down_interruptible(&device_mutex)) {
		printk(KERN_ERR "[%s] In rs232_master_close down_interruptible fail.\n", DEVICE_NAME);
		ret = -ERESTARTSYS;
		goto down_interruptible_fail;
	}
	/* If this is the last close, send all the unsend data to slave device
	   immediately and close */
	if(device_ref_count == 1) {
		while(datalen_in_kern_buf > 0) {
			if((ret = send(client_socket, &kernel_buff[unflush_index], datalen_in_kern_buf)) < 0) {
				printk(KERN_ERR "[%s] In rs232_master_close send fail, return %d.\n", DEVICE_NAME, ret);
				goto send_fail;
			}
			datalen_in_kern_buf -= ret;
			unflush_index += ret;
		}
		unflush_index = 0;
		if((ret = close_connection()) < 0) {
			printk(KERN_ERR "close_connections fail, return %d.\n", ret);
			goto close_connection_fail;
		}
	}
	device_ref_count--;
	up(&device_mutex);
	return 0;

close_connection_fail:
send_fail:
	up(&device_mutex);
down_interruptible_fail:
	return ret;
}

/** 
 * Open connection with slave device through kernel socket follow this procedure:
 * 1. Create server kernel socket
 * 2. Bind server socket to specific port
 * 3. Server socket listen to that port
 * 4. Create client socket and wait fot its connection
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
	sock_release(client_socket);
	sock_release(server_socket);
	return 0;
}

/** 
 * Copy the data in user space to kernel buffer which can be read by device, then send the data
 * in kernel buffer to slave device with the following step:
 * 1. Check whether there are data in kernel buffer and not send to slave device yet, if so, send it.
 * 2. Copy the data in user space buffer to kernel space buffer and constantly check whether kernel buffer is full.
 * If it's full, send the data in kernal buffer to slave device and flush kernel buffer.
 * 3. Repead step 2 until all the data in user space has been copy to kernal space buffer.
 */
static ssize_t rs232_master_write(struct file* filp, const char __user *buff, size_t data_size, loff_t* offp) {
	int ret;
	size_t per_copy_len;
	size_t copied_len = 0;
	size_t remain_len = data_size;
	if(down_interruptible(&device_mutex)) {
		ret = -ERESTARTSYS;
		printk(KERN_ERR "[%s] In rs232_master_write down_interruptible fail.\n", DEVICE_NAME);
		goto down_interruptible_fail;
	}
	/* Check whether there is unsend data in kernel buffer, if yes, then send it and flush the buffer */
	while(datalen_in_kern_buf > 0 && unflush_index > 0) {
		if((ret = send(client_socket, &kernel_buff[unflush_index], datalen_in_kern_buf)) < 0) {
			printk(KERN_ERR "[%s] send fail, return %d.\n", DEVICE_NAME, ret);
			goto send_fail;
		}
		datalen_in_kern_buf -= ret;
		unflush_index += ret;
	}
	unflush_index = 0;
	while(remain_len > 0) {
		per_copy_len = RS232_MASTER_BUF_SIZE - datalen_in_kern_buf;
		if(per_copy_len > remain_len)
			per_copy_len = remain_len;
		if((copy_from_user(&kernel_buff[datalen_in_kern_buf], &buff[copied_len], per_copy_len)) < 0) {
			ret = -EFAULT;
			printk(KERN_ERR "[%s] copy_from_user fail.\n", DEVICE_NAME);
			goto copy_from_user_fail;
		}
		remain_len -= per_copy_len;
		datalen_in_kern_buf += per_copy_len;
		unflush_index += per_copy_len;
		copied_len += per_copy_len;
		/* If kernel buffer is full, send all the data to slave device and flush it */
		if(RS232_MASTER_BUF_SIZE == datalen_in_kern_buf) {
			while(datalen_in_kern_buf > 0) {
				if((ret = send(client_socket, &kernel_buff[unflush_index], datalen_in_kern_buf)) < 0) {
					printk(KERN_ERR "[%s] send fail, return %d.\n", DEVICE_NAME, ret);
					goto send_fail;
				}
				datalen_in_kern_buf -= ret;
				unflush_index += ret;
			}
			unflush_index = 0;
		}
	}
	up(&device_mutex);
	return (long)copied_len;

copy_from_user_fail:
send_fail:
	up(&device_mutex);
down_interruptible_fail:
	return ret;
}

/**
 * This operation is invoked when mmap system call is used my and user program
 */
static int rs232_master_mmap(struct file* filp, struct vm_area_struct* vma) {
	int ret;
	unsigned long size;

	/*struct mm_struct* mm;
	pgd_t* pgd;
	pmd_t* pmd;
	pud_t* pud;
	pte_t* pte;
	int i = 0;*/

	size = (vma->vm_end - vma->vm_start);
	/* Check whether virtual memory area size is larger to our kernel buffer */
	if(size > RS232_MASTER_BUF_SIZE) {
		ret = -EINVAL;
		goto vma_size_error;
	}
	vma->vm_pgoff = __pa(kernel_buff) >> PAGE_SHIFT;
	vma->vm_ops = &vmops;
	vma->vm_flags |= VM_RESERVED;
	/* Making a page table */
	if(remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, size, vma->vm_page_prot)) {
		ret = -EAGAIN;
		goto making_page_table_fail;
	}
	
	vma_open(vma);
	/*mm = vma->vm_mm;
	for(i = 0;;i++) {
		pgd = pgd_offset(mm, vma->vm_start + 4096 * i);
		pud = pud_offset(pgd, vma->vm_start + 4096 * i);
		pmd = pmd_offset(pud, vma->vm_start + 4096 * i);
		pte = pte_offset_kernel(pmd, vma->vm_start + 4096 * i);
		if(pte_none(*pte))
			break;
		printk(KERN_INFO "%lx\n", pte_val(*pte));
	}*/
	return 0;

making_page_table_fail:
vma_size_error:
	return ret;
}

static long rs232_master_ioctl(struct file* filp, unsigned int ioctl_num, unsigned long param) {
	int ret;
	size_t datalen;
	size_t sended_len;
	switch(ioctl_num) {
	case IOCTL_SENDTOSLAVE:
		/* Get a simple variable from user space */
		if(get_user(datalen, (size_t __user *)param)) {
			ret = -EFAULT;
			printk(KERN_ERR "[%s] get_user fail, return %d.\n", DEVICE_NAME, ret);
			goto get_user_fail;
		}
		/* Check whether data length is larger than device buffer size */
		if(datalen > RS232_MASTER_BUF_SIZE) {
			ret = -EINVAL;
			goto datalen_error;
		}
		/* Lock critical section */
		if(down_interruptible(&device_mutex)) {
			ret = -ERESTARTSYS;
			printk(KERN_ERR "[%s] In rs232_master_ioctl down_interruptible fail, return %d.\n", DEVICE_NAME, ret);
			goto down_interruptible_fail;
		}
		/* Below is very similar to rs232_master_write */
		datalen_in_kern_buf = datalen;
		sended_len = 0;
		while(datalen_in_kern_buf > 0) {
			if((ret = send(client_socket, &kernel_buff[unflush_index], datalen_in_kern_buf)) < 0) {
				printk(KERN_ERR "[%s] In rs232_master_ioctl send fail, return %d.\n", DEVICE_NAME, ret);
				goto send_fail;
			}
			sended_len += ret;
			datalen_in_kern_buf -= ret;
			unflush_index += ret;
		}
		unflush_index = 0;
		up(&device_mutex);
		return (long)sended_len;
	default:
		return -ENOTTY;
	}
	return 0;

send_fail:
		up(&device_mutex);
down_interruptible_fail:
datalen_error:
get_user_fail:
		return ret;
}

static void vma_open(struct vm_area_struct* vma) {
	printk(KERN_INFO "[%s] vma open, virt %lx, phys %lx.\n", DEVICE_NAME, vma->vm_start, 
			vma->vm_pgoff << PAGE_SHIFT);
	return;
}

static void vma_close(struct vm_area_struct* vma) {
	return;
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
