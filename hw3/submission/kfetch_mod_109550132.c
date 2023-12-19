#include <linux/fs.h>	
#include <linux/mm.h>
#include <linux/init.h>			
#include <linux/module.h>		
#include <linux/device.h>		
#include <linux/kernel.h>		
#include <linux/uaccess.h>		
#include <linux/string.h>
#include <linux/unistd.h>		
#include <linux/utsname.h>		
#include <linux/cpufreq.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/swap.h>
#include <linux/mman.h>
#include <asm/errno.h>

/*  Prototypes - this would normally go in a .h file */
static int kfetch_open(struct inode *, struct file *);
static int kfetch_release(struct inode *, struct file *);
static ssize_t kfetch_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t kfetch_write(struct file *, const char __user *, size_t,
                            loff_t *);

#define SUCCESS 0
#define DEVICE_NAME "kfetch" /* Dev name as it appears in /proc/devices   */
#define BUF_LEN 80 /* Max length of the message from the device */
#define RightBuf 48

#define KFETCH_NUM_INFO 6
#define KFETCH_RELEASE   (1 << 0)
#define KFETCH_NUM_CPUS  (1 << 1)
#define KFETCH_CPU_MODEL (1 << 2)
#define KFETCH_MEM       (1 << 3)
#define KFETCH_UPTIME    (1 << 4)
#define KFETCH_NUM_PROCS (1 << 5)
#define KFETCH_FULL_INFO ((1 << KFETCH_NUM_INFO) - 1)


static int major; /* major number assigned to our device driver */

enum {
    CDEV_NOT_USED = 0,
    CDEV_EXCLUSIVE_OPEN = 1,
};

/* Is device open? Used to prevent multiple access to device */
static atomic_t already_open = ATOMIC_INIT(CDEV_NOT_USED);

// static char msg[BUF_LEN + 1]; /* The msg the device will give when asked */
static int mask_info;

static struct class *cls;

const static struct file_operations kfetch_ops = {
    .owner   = THIS_MODULE,
    .read    = kfetch_read,
    .write   = kfetch_write,
    .open    = kfetch_open,
    .release = kfetch_release,
};

static int __init chardev_init(void)
{
    major = register_chrdev(0, DEVICE_NAME, &kfetch_ops);

    if (major < 0) {
        pr_alert("Registering char device failed with %d\n", major);
        return major;
    }

    pr_info("I was assigned major number %d!\n", major);


    cls = class_create(THIS_MODULE, DEVICE_NAME);
    device_create(cls, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);

    pr_info("Device created on /dev/%s\n", DEVICE_NAME);

    return SUCCESS;
}

static void __exit chardev_exit(void)
{
    device_destroy(cls, MKDEV(major, 0));
    class_destroy(cls);

    /* Unregister the device */
    unregister_chrdev(major, DEVICE_NAME);
}

/* Methods */

/* Called when a process tries to open the device file, like
 * "sudo cat /dev/kfetch"
 */
static int kfetch_open(struct inode *inode, struct file *file)
{
    static int counter = 0;

    if (atomic_cmpxchg(&already_open, CDEV_NOT_USED, CDEV_EXCLUSIVE_OPEN))
        return -EBUSY;

    printk(KERN_INFO "successfully open times: %d\n", counter++);

    try_module_get(THIS_MODULE);

    return SUCCESS;
}

/* Called when a process closes the device file. */
static int kfetch_release(struct inode *inode, struct file *file)
{
    /* We're now ready for our next caller */
    atomic_set(&already_open, CDEV_NOT_USED);

    /* Decrement the usage count, or else once you opened the file, you will
     * never get rid of the module.
     */
    module_put(THIS_MODULE);

    return SUCCESS;
}

/* Called when a process, which already opened the dev file, attempts to
 * read from it.
 */
static ssize_t kfetch_read(struct file *filp, /* see include/linux/fs.h   */
                           char __user *buffer, /* buffer to fill with data */
                           size_t length, /* length of the buffer     */
                           loff_t *offset)
{
	// initialization
	int outputCnt = 0;
	char delimiter [24] = "------------";
	char output[10][RightBuf];

	unsigned int cpu = 0;
	unsigned long uptime_mins = 0;
	size_t procNum = 0;
	char mod_output[2048];
	char mod_nodeName[24];
	char mod_release[48] = "";
	char mod_cpu_model[100] = "";
	char mod_cpu_nums[24] = "";
    char mod_totalmem[20] = "", mod_freemem[20] = "";
	char mod_mem[40];
	char mod_uptime[24] = "";
	char mod_procNum[24] = "";
	struct cpuinfo_x86 *cpu_x86;
    struct sysinfo sys_info;
	struct task_struct* task;
	struct timespec64 uptime;


	// get node info 
	snprintf(mod_nodeName, sizeof(mod_release), "%s", utsname()->nodename);
	// get release info
	snprintf(mod_release, sizeof(mod_release), "Kernel:   %s", utsname()->release);
    // get cpu info
	cpu_x86 = &cpu_data(cpu);
	sprintf(mod_cpu_model, "CPU:      %s", cpu_x86->x86_model_id);
	// get cpu num info
	sprintf(mod_cpu_nums, "CPUs:     %d / %d", num_active_cpus(), num_online_cpus());
	// get memory info
    si_meminfo(&sys_info);
    snprintf(mod_totalmem, sizeof(mod_totalmem), "%lu MB", sys_info.totalram * 4 / 1024);
    snprintf(mod_freemem, sizeof(mod_freemem), "%lu MB", sys_info.freeram * 4 / 1024);
	snprintf(mod_mem, sizeof(mod_mem), "Mem:      %s / %s", mod_freemem, mod_totalmem);
	// get proc info
	for_each_process(task){
		procNum ++;
	}
	snprintf(mod_procNum, sizeof(mod_procNum),"Procs:    %ld", procNum);
	// get upTime info
	ktime_get_boottime_ts64(&uptime);
	uptime_mins = (unsigned long) uptime.tv_sec / 60;
	snprintf(mod_uptime, sizeof(mod_uptime), "Uptime:   %lu mins", uptime_mins);

	
	// update output info with coorsponding maskinfo
	snprintf(output[outputCnt++], RightBuf, "%s", mod_nodeName);
	snprintf(output[outputCnt++], RightBuf, "%s", delimiter);
	if (mask_info & KFETCH_RELEASE) {
		snprintf(output[outputCnt++], RightBuf, "%s", mod_release);
	}
	if (mask_info & KFETCH_CPU_MODEL) {
		snprintf(output[outputCnt++], RightBuf, "%s", mod_cpu_model);
	}
	if (mask_info & KFETCH_NUM_CPUS) {
		snprintf(output[outputCnt++], RightBuf, "%s", mod_cpu_nums);
	}
	if (mask_info & KFETCH_MEM) {
		snprintf(output[outputCnt++], RightBuf, "%s", mod_mem);
	}
	if (mask_info & KFETCH_NUM_PROCS) {
		snprintf(output[outputCnt++], RightBuf, "%s", mod_procNum);
	}
	if (mask_info & KFETCH_UPTIME) {
		snprintf(output[outputCnt++], RightBuf, "%s", mod_uptime);
	}
	for (int i = outputCnt; i < 10; i++) {
		output[i][0] = '\0';
	}
	// test output 
	// printk(KERN_INFO "%s", delimiter);
	// int i;
	// for (i = 0; i < 8; i++) {
	// 	printk(KERN_INFO "%s", output[i]);
	// }

	snprintf(mod_output, sizeof(mod_output), 
			"\n"
			"                     %s\n"
			"        .-.          %s\n"
			"       (.. |         %s\n"
			"       <>  |         %s\n"
			"      / --- \\        %s\n"
			"     ( |   | |       %s\n"
			"   |\\\\_)___/\\)/\\     %s\n"
			"  <__)------(__/     %s",
			output[0],
			output[1],
			output[2],
			output[3],
			output[4],
			output[5],
			output[6],
			output[7]
	);
	pr_info("%s", mod_output);


	if ((copy_to_user(buffer, mod_output, strlen(mod_output))) == 0){            // if true then have success
		printk(KERN_INFO "kfetch: Sent %ld characters to the user\n", strlen(mod_output));
		return strlen(mod_output);  // clear the position to the start and return 0
	}
	else {
		return -EFAULT;              
	}
	return 0;
}



/* Called when a process writes to dev file: echo "1" > /dev/hello */
static ssize_t kfetch_write(struct file *filp, const char __user *buff,
                            size_t len, loff_t *off)
{

    if (copy_from_user(&mask_info, buff, len)) {
        pr_alert("Failed to copy data from user");
        return 0;
    }

    return 0;
}

module_init(chardev_init);
module_exit(chardev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nian Rong, Wu");
MODULE_DESCRIPTION("A simple character device driver");
