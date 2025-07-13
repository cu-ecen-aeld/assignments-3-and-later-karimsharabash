/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>

#include <linux/printk.h>
#include <linux/types.h>
#include <linux/fs.h> // file_operations

#else
#include <string.h>
#endif



#include "aesd-circular-buffer.h"

#define NEXT_INDEX(current) (((current) + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
   int readChars = 0;
   int index = 0;
   int rem = 0;

    /**
    * TODO: implement per description
    */
   if (buffer->in_offs == 0 && buffer->full == 0)
   {
    return NULL;
   }


   PDEBUG("find entry , in = %d, out = %d, char_offset= %ld", buffer->in_offs , buffer->out_offs, char_offset );
   for (index = buffer->out_offs ; ; index++)
   {    
        index %= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        PDEBUG("index = %d, buffer->entry[index].size = %ld", index, buffer->entry[index].size);
        if ((readChars + buffer->entry[index].size - 1) < char_offset)
        {
            readChars += buffer->entry[index].size;
        }
        else
        {
            rem = char_offset - readChars;
            *entry_offset_byte_rtn = rem;
            PDEBUG("rem = %d, string = %s", rem, buffer->entry[index].buffptr);      
            return &(buffer->entry[index]);
        }

        if (( (index + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) == buffer->in_offs )
        {
            break;
        }
   }
   
   return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void* aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    void *bufferToremove = NULL;

    buffer->entry[buffer->in_offs].buffptr = add_entry->buffptr;
    buffer->entry[buffer->in_offs].size = add_entry->size;

    buffer->in_offs = NEXT_INDEX(buffer->in_offs);

    if (buffer->full == 1)
    {
        bufferToremove = (void *)buffer->entry[buffer->out_offs].buffptr;
        buffer->entry[buffer->out_offs].size = 0;
        buffer->out_offs = NEXT_INDEX(buffer->out_offs);
    }
    else if (buffer->out_offs  == buffer->in_offs )
    {      
        buffer->full = 1;
        
    }

    PDEBUG("in = %d, out = %d, full = %d ", buffer->in_offs,buffer->out_offs, buffer->full );
    return bufferToremove;
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));

    PDEBUG("aesd_circular_buffer_init");
}
