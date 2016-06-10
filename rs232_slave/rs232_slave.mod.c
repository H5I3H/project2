#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x274787d4, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0x7485e15e, __VMLINUX_SYMBOL_STR(unregister_chrdev_region) },
	{ 0xfcaa2eef, __VMLINUX_SYMBOL_STR(class_destroy) },
	{ 0xa8e232ed, __VMLINUX_SYMBOL_STR(device_destroy) },
	{ 0x3c97631e, __VMLINUX_SYMBOL_STR(cdev_del) },
	{ 0xa202a8e5, __VMLINUX_SYMBOL_STR(kmalloc_order_trace) },
	{ 0x27ac83b8, __VMLINUX_SYMBOL_STR(cdev_add) },
	{ 0x57c0c96f, __VMLINUX_SYMBOL_STR(cdev_init) },
	{ 0x42109b92, __VMLINUX_SYMBOL_STR(device_create) },
	{ 0xb71ff8b9, __VMLINUX_SYMBOL_STR(__class_create) },
	{ 0x29537c9e, __VMLINUX_SYMBOL_STR(alloc_chrdev_region) },
	{ 0x4f8b5ddb, __VMLINUX_SYMBOL_STR(_copy_to_user) },
	{ 0xf0fdf6cb, __VMLINUX_SYMBOL_STR(__stack_chk_fail) },
	{ 0x7878086, __VMLINUX_SYMBOL_STR(sock_release) },
	{ 0x75c72c3c, __VMLINUX_SYMBOL_STR(kernel_setsockopt) },
	{ 0x7a369eee, __VMLINUX_SYMBOL_STR(sock_create_kern) },
	{ 0x6729d3df, __VMLINUX_SYMBOL_STR(__get_user_4) },
	{ 0x6d334118, __VMLINUX_SYMBOL_STR(__get_user_8) },
	{ 0x71e3cecb, __VMLINUX_SYMBOL_STR(up) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0xf22449ae, __VMLINUX_SYMBOL_STR(down_interruptible) },
	{ 0x5807d4fa, __VMLINUX_SYMBOL_STR(sock_recvmsg) },
	{ 0x4c4fef19, __VMLINUX_SYMBOL_STR(kernel_stack) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "4C17647B017C42DCC6645B4");
