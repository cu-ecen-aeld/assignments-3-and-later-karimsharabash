/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>

#include "aesdchar.h"
#include "aesd_ioctl.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Karim Sharabash"); 
MODULE_LICENSE("Dual BSD/GPL");

#define BUFF_BLOCK_SIZE (1024)

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *dev;

    PDEBUG("open");
    /**
     * TODO: handle open
     * get the device 
     * check first time
     * create file 
     * protect it
     * return the buffer if we succeed to do it
     */
    /* get the Device*/
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;

    // if ( (filp->f_flags & O_ACCMODE) == O_WRONLY) {

    // }


    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    struct aesd_dev *dev = filp->private_data;


    PDEBUG("release");
    /**
     * TODO: handle release
     */
    mutex_lock(&dev->lock);
    if (dev->buffer && (dev->string_len == 0))
    {
        kfree(dev->buffer);
        dev->buffer = NULL;
    }
    mutex_unlock(&dev->lock);

    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    struct aesd_dev *dev = filp->private_data;
    unsigned long offset = 0;
    unsigned long retStatus  =0;
    struct aesd_buffer_entry *entry = NULL;

    
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
  
    mutex_lock(&dev->lock);

    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->circular_buffer, (size_t) (*f_pos), &offset);
    // entry->buffptr[entry->size] = '\0';
    
    if (entry )
    {
        PDEBUG("the found buffer  size %ld ", entry->size);
        PDEBUG("the found buffer is %s with lenght %ld ", entry->buffptr, offset);
        retStatus = copy_to_user(buf + retval, entry->buffptr + offset, entry->size - offset);
        if (retStatus !=0)
        {
            PDEBUG("copy_to_user failed with rematining bytes= %ld", retStatus);
            return retStatus;
        }
        else
        {
            retval = (entry->size - offset);
        }
        *f_pos += retval;
    }
    

    mutex_unlock(&dev->lock);

    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    struct aesd_dev *dev = filp->private_data;
    unsigned long retStatus =0;
    struct aesd_buffer_entry * entryToRemove = NULL;
    struct aesd_buffer_entry entry;

    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    
    mutex_lock(&dev->lock);

    if (dev->buffer == NULL )
    {
        dev->buffer = kmalloc( BUFF_BLOCK_SIZE, GFP_KERNEL );
        dev->string_len = 0;
        dev->buffer_size = BUFF_BLOCK_SIZE;
    }

    if (count + dev->string_len  > dev->buffer_size)
    {

        PDEBUG("buffer needs bigger size");
        dev->buffer_size = (((count + dev->string_len ) / dev->buffer_size) + 1) * BUFF_BLOCK_SIZE;

        dev->buffer = krealloc( dev->buffer, dev->buffer_size,  GFP_KERNEL);
    }

    retStatus= copy_from_user(dev->buffer + dev->string_len, buf, count);
    if (retStatus !=0)
    {
        PDEBUG("copy_to_user failed with rematining bytes= %ld", retStatus);
    }

    // if the buffer ended with \n write to the circular buffer
    dev->string_len += count;
    if (dev->buffer[dev->string_len - 1] == '\n')
    {
        PDEBUG("the written Buffer is %s", dev->buffer);
        entry.buffptr = dev->buffer;
        entry.size= dev->string_len;

        dev->total_size += entry.size;

        entryToRemove = aesd_circular_buffer_add_entry(&dev->circular_buffer, &entry);

        if (entryToRemove != NULL)
        {
            dev->total_size -= entryToRemove->size;
            entryToRemove->size = 0;
            kfree(entryToRemove->buffptr);
            entryToRemove->buffptr = NULL;
        }

        dev->buffer = NULL;
        dev->string_len = 0;
    }
    mutex_unlock(&dev->lock);
    
    retval = count;
     
    return retval;
}

loff_t aesd_seek(struct file *filp, loff_t offset, int whence)
{
    loff_t ret = 0;
    struct aesd_dev *dev = filp->private_data;

    mutex_lock(&dev->lock);

    ret = fixed_size_llseek(filp, offset, whence, dev->total_size);
    mutex_unlock(&dev->lock);

    return ret;
}

static long aesd_asdjust_file_offset(struct file *filp, uint32_t wrtie_cmd, uint32_t write_cmd_offset)
{
    size_t char_offset = 0;
    size_t offset = 0;
    uint8_t i =0;

    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry *entry = NULL;

    
    PDEBUG("get char with wrtie_cmd=  %d and write_cmd_offset= %d", wrtie_cmd, write_cmd_offset);
  
    mutex_lock(&dev->lock);

    for (i = 0 ;i <= wrtie_cmd; i++)
    {
        entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->circular_buffer, offset, &char_offset);
        if (entry == NULL)
        {
            return -EINVAL;
        }

        offset += entry->size;
    }
    
    if (entry == NULL)
    {
        return -EINVAL;
    }
    mutex_unlock(&dev->lock);
    offset -= entry->size;
    offset += write_cmd_offset;

    return offset;
}

static long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct aesd_seekto seekto;
    uint32_t retval = 0;
    switch (cmd) {
        case AESDCHAR_IOCSEEKTO:
            // PDEBUG("get char with wrtie_cmd=  %d and write_cmd_offset= %d", wrtie_cmd, write_cmd_offset);
            if (copy_from_user(&seekto, (int*)arg, sizeof(seekto)))
                return -EFAULT;

            retval = aesd_asdjust_file_offset(filp, seekto.write_cmd, seekto.write_cmd_offset);
            PDEBUG("aesd_ioctl, total offset is %d", retval);
            if (retval > 0)
            {
                filp->f_pos = retval;
            }
            break;
        default:
            return -EINVAL;
    }
    
    return retval;
}


struct file_operations aesd_fops = {
    .owner          = THIS_MODULE,
    .read           = aesd_read,
    .write          = aesd_write,
    .open           = aesd_open,
    .release        = aesd_release,
    .llseek         = aesd_seek,
    .unlocked_ioctl = aesd_ioctl,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */

    mutex_init(&aesd_device.lock);
    aesd_circular_buffer_init(&aesd_device.circular_buffer);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);


    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
