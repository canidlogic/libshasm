/*
 * shasm_block.c
 * 
 * Implementation of shasm_block.h
 * 
 * See the header for further information.
 */
#include "shasm_block.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/*
 * The initial capacity of the block buffer in bytes, as a signed long
 * constant.
 * 
 * This includes space for the terminating null character.
 * 
 * This value must be at least two, and not be greater than
 * SHASM_BLOCK_MAXBUFFER.
 * 
 * This module will run this value through shasm_block_adjcap to limit
 * it to 65535 on platforms where size_t is less than 32-bit.
 */
#define SHASM_BLOCK_MINBUFFER (32L)

/*
 * The maximum capacity of the block buffer in bytes, as a signed long
 * constant.
 * 
 * This includes space for the terminating null character.  Blocks may
 * be no longer than one less than this value in length.  Since the
 * maximum block size supported by Shastina is 65,535 bytes, this value
 * is one greater than that.
 * 
 * This value must not be less than SHASM_BLOCK_MINBUFFER.  It must also
 * be less than half of LONG_MAX, so that doubling the capacity never
 * results in overflow.
 * 
 * This module will run this value through shasm_block_adjcap to limit
 * it to 65535 on platforms where size_t is less than 32-bit.  In this
 * case, the maximum block size supported by libshasm will be 65,534
 * bytes, which is one byte shy of the specification.
 * 
 * On platforms with a 16-bit size_t, it may be a good idea to adjust
 * this value down to avoid memory allocation faults.  But note that
 * adjusting this value down will diverge from the specification by not
 * supporting the full 65,535-byte size of strings.
 */
#define SHASM_BLOCK_MAXBUFFER (65536L)

/*
 * SHASM_BLOCK structure for storing block reader state.
 * 
 * The prototype of this structure is given in the header.
 */
struct SHASM_BLOCK_TAG {
  
  /*
   * The status of the block reader.
   * 
   * This is one of the codes from shasm_error.h
   * 
   * If SHASM_OKAY (the initial value), then there is no error and the
   * block reader is in a function state.  Otherwise, the block reader
   * is in an error state and code indicates the error.
   */
  int code;
  
  /*
   * The line number.
   * 
   * If the status code is SHASM_OKAY, this is the line number that the
   * most recently read block begins at, or one if no blocks have been
   * read yet (the initial value), or LONG_MAX if the line count has
   * overflowed.
   * 
   * If the status code indicates an error state, this is the line
   * number that the error occurred at, or LONG_MAX if the line count
   * has overflowed.
   */
  long line;
  
  /*
   * The capacity of the allocated buffer in bytes.
   * 
   * This includes space for a terminating null character.  The buffer
   * starts out allocated at a capacity of SHASM_BLOCK_MINBUFFER, run
   * through the shasm_block_adjcap function.  It grows by doubling as
   * necessary, to a maximum of SHASM_BLOCK_MAXBUFFER, run through the
   * shasm_block_adjcap function.
   */
  long buf_cap;
  
  /*
   * The length of data stored in the buffer, in bytes.
   * 
   * This does not include the terminating null character.  If zero, it
   * means the string is empty.  This starts out at zero.
   */
  long buf_len;
  
  /*
   * Flag indicating whether a null byte has been written as data to the
   * buffer.
   * 
   * The buffer will always be given a terminating null byte regardless
   * of whether a null byte is present in the data.  However, the ptr
   * function will check this flag to determine whether it's safe for
   * the client to treat the string as null-terminated.
   * 
   * This flag starts out as zero to indicate no null byte has been
   * written yet.
   */
  int null_present;
  
  /*
   * Pointer to the dynamically allocated buffer.
   * 
   * This must be freed when the block structure is freed.  Its capacity
   * is stored in buf_cap, the actual data length is buf_len, and
   * null_present indicates whether the data includes a null byte.
   * 
   * The data always has a null termination byte following it, even if
   * the data includes null bytes as data.
   */
  unsigned char *pBuf;
  
};

/* 
 * Local functions
 * ===============
 */

static long shasm_block_adjcap(long v);
static void shasm_block_clear(SHASM_BLOCK *pb);
static int shasm_block_addByte(SHASM_BLOCK *pb, int c);

/*
 * Adjust a capacity constant if necessary for the current platform.
 * 
 * The SHASM_BLOCK_MINBUFFER and SHASM_BLOCK_MAXBUFFER constants are run
 * through this function before actually being used.
 * 
 * If sizeof(size_t) is four or greater, then this function simply
 * returns the value that was passed to it as-is.
 * 
 * If sizeof(size_t) is two or three, then this function returns the
 * minimum of the passed value and 65,535.
 * 
 * If sizeof(size_t) is less than two, a fault occurs.
 * 
 * This function has the effect of limiting buffer capacity to 65,535
 * bytes on platforms with less than a 32-bit size_t value.  (It also
 * verifies that size_t is at least 16-bit, faulting if this is not the
 * case.)
 * 
 * The passed capacity value must be greater than one or a fault occurs.
 * 
 * Parameters:
 * 
 *   v - the capacity value to adjust
 * 
 * Return:
 * 
 *   the adjusted capacity value
 */
static long shasm_block_adjcap(long v) {
  
  /* Check parameter */
  if (v <= 1) {
    abort();
  }
  
  /* Adjust if necessary depending of size_t size */
  if ((sizeof(size_t) < 4) && (sizeof(size_t) >= 2)) {
    /* Less than 32-bit but at least 16-bit size_t */
    if (v > 65535L) {
      v = 65535L;
    }
  } else if (sizeof(size_t) < 2) {
    /* Less than 16-bit size_t -- not supported */
    abort();
  }
  
  /* Return value, possibly adjusted */
  return v;
}

/*
 * Clear the block reader's internal buffer to an empty string.
 * 
 * This does not reset the error status of the block reader (see
 * shasm_block_status).
 * 
 * Parameters:
 * 
 *   pb - the block reader to clear
 */
static void shasm_block_clear(SHASM_BLOCK *pb) {
  /* Check parameter */
  if (pb == NULL) {
    abort();
  }
  
  /* Reset buf_len and null_present */
  pb->buf_len = 0;
  pb->null_present = 0;
  
  /* Clear the buffer to all zero */
  memset(pb->pBuf, 0, (size_t) pb->buf_cap);
}

/*
 * Append an unsigned byte value (0-255) to the end of the block
 * reader's internal buffer.
 * 
 * This might cause the buffer to be reallocated, so pointers returned
 * by shasm_block_ptr become invalid after calling this function.
 * 
 * The function will fail if there is no more room for another
 * character.  This function does *not* set an error state within the
 * block reader if it fails, leaving that for the client.
 * 
 * If this function is called when the block reader is already in an
 * error state, the function fails and does nothing further.
 * 
 * Parameters:
 * 
 *   pb - the block reader to append a byte to
 * 
 *   c - the unsigned byte value to append (0-255)
 * 
 * Return:
 * 
 *   non-zero if successful, zero if failure
 */
static int shasm_block_addByte(SHASM_BLOCK *pb, int c) {
  int status = 1;
  
  /* Check parameters */
  if ((pb == NULL) || (c < 0) || (c > 255)) {
    abort();
  }
  
  /* Fail immediately if already in error state */
  if (pb->code != SHASM_OKAY) {
    status = 0;
  }
  
  /* If buf_len is one less than buf_cap, buffer capacity needs to be
   * increased -- the "one less" is to account for the terminating
   * null */
  if (status && (pb->buf_len >= pb->buf_cap - 1)) {
    /* If buffer capacity already at adjusted maximum capacity, fail */
    if (pb->buf_len >= shasm_block_adjcap(SHASM_BLOCK_MAXBUFFER)) {
      status = 0;
    }
    
    /* Room to grow still -- new capacity is minimum of double the
     * current capacity and the adjusted maximum capacity */
    if (status) {
      pb->buf_cap *= 2;
      if (pb->buf_cap > shasm_block_adjcap(SHASM_BLOCK_MAXBUFFER)) {
        pb->buf_cap = shasm_block_adjcap(SHASM_BLOCK_MAXBUFFER);
      }
    }
    
    /* Expand the size of the buffer to the new capacity */
    if (status) {
      pb->pBuf = (unsigned char *) realloc(
                    pb->pBuf, (size_t) pb->buf_cap);
      if (pb->pBuf == NULL) {
        abort();
      }
    }
    
    /* Clear the expanded buffer contents to zero */
    if (status) {
      memset(&(pb->pBuf[pb->buf_len]), 0, pb->buf_cap - pb->buf_len);
    }
  }
  
  /* If we're adding a null byte, set the null_present flag */
  if (status && (c == 0)) {
    pb->null_present = 1;
  }
  
  /* Append the new byte */
  if (status) {
    pb->pBuf[pb->buf_len] = (unsigned char) c;
    (pb->buf_len)++;
  }
  
  /* Return status */
  return status;
}

/*
 * Public functions
 * ================
 * 
 * See the header for specifications.
 */

/*
 * shasm_block_alloc function.
 */
SHASM_BLOCK *shasm_block_alloc(void) {
  SHASM_BLOCK *pb = NULL;
  
  /* Allocate a block */
  pb = (SHASM_BLOCK *) malloc(sizeof(SHASM_BLOCK));
  if (pb == NULL) {
    abort();
  }
  
  /* Clear the block */
  memset(pb, 0, sizeof(SHASM_BLOCK));
  
  /* Initialize fields */
  pb->code = SHASM_OKAY;
  pb->line = 1;
  pb->buf_cap = shasm_block_adjcap(SHASM_BLOCK_MINBUFFER);
  pb->buf_len = 0;
  pb->null_present = 0;
  pb->pBuf = NULL;
  
  /* Allocate the initial dynamic buffer */
  pb->pBuf = (unsigned char *) malloc((size_t) pb->buf_cap);
  if (pb->pBuf == NULL) {
    abort();
  }
  
  /* Clear the dynamic buffer to all zero */
  memset(pb->pBuf, 0, (size_t) pb->buf_cap);
  
  /* Return the new block object */
  return pb;
}

/*
 * shasm_block_free function.
 */
void shasm_block_free(SHASM_BLOCK *pb) {
  /* Free object if not NULL */
  if (pb != NULL) {
    /* Free dynamic buffer if not NULL */
    if (pb->pBuf != NULL) {
      free(pb->pBuf);
      pb->pBuf = NULL;
    }
    
    /* Free the main structure */
    free(pb);
  }
}

/*
 * shasm_block_status function.
 */
int shasm_block_status(SHASM_BLOCK *pb, long *pLine) {
  /* Check parameter */
  if (pb == NULL) {
    abort();
  }
  
  /* Write line number if in error state and pLine provided */
  if ((pb->code != SHASM_OKAY) && (pLine != NULL)) {
    *pLine = pb->line;
  }
  
  /* Return status */
  return pb->code;
}

/*
 * shasm_block_count function.
 */
long shasm_block_count(SHASM_BLOCK *pb) {
  long result = 0;
  
  /* Check parameter */
  if (pb == NULL) {
    abort();
  }
  
  /* Use the count field or a value of zero, depending on error state */
  if (pb->code == SHASM_OKAY) {
    /* Not in error state -- use length field of structure */
    result = pb->buf_len;
  } else {
    /* Error state -- use value of zero */
    result = 0;
  }
  
  /* Return result */
  return result;
}

/*
 * shasm_block_ptr function.
 */
unsigned char *shasm_block_ptr(SHASM_BLOCK *pb, int null_term) {
  unsigned char *pc = NULL;
  
  /* Check parameter */
  if (pb == NULL) {
    abort();
  }
  
  /* Get pointer to buffer */
  pc = pb->pBuf;
  
  /* Special handling for a few cases */
  if (pb->code != SHASM_OKAY) {
    /* Error state -- write a terminating null into the start of the
     * buffer to make sure there's an empty string */
    *pc = (unsigned char) 0;
  
  } else if (null_term) {
    /* Client expecting null-terminated string -- clear pc to NULL if
     * null is present in the data */
    if (pb->null_present) {
      pc = NULL;
    }
  }
  
  /* Return the buffer pointer */
  return pc;
}

/*
 * shasm_block_line function.
 */
long shasm_block_line(SHASM_BLOCK *pb) {
  long result = 0;
  
  /* Check parameter */
  if (pb == NULL) {
    abort();
  }
  
  /* Determine result depending on error state */
  if (pb->code == SHASM_OKAY) {
    /* Not in error state -- return line field */
    result = pb->line;
  
  } else {
    /* In error state -- return LONG_MAX */
    result = LONG_MAX;
  }
  
  /* Return result */
  return result;
}

/*
 * shasm_block_token function.
 */
int shasm_block_token(SHASM_BLOCK *pb, SHASM_IFLSTATE *ps) {
  /* @@TODO: replace placeholder */
  return 0;
}
