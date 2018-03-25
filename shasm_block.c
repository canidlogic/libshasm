/*
 * shasm_block.c
 * 
 * Implementation of shasm_block.h
 * 
 * See the header for further information.
 */
#include "shasm_block.h"
#include "shasm_ascii.h"
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
 * The maximum Unicode codepoint value.
 */
#define SHASM_BLOCK_MAXCODE (0x10ffffL)

/*
 * The minimum and maximum Unicode surrogate codepoints.
 */
#define SHASM_BLOCK_MINSURROGATE (0xd800L)
#define SHASM_BLOCK_MAXSURROGATE (0xdfffL)

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

static void shasm_block_seterr(
    SHASM_BLOCK *pb,
    SHASM_IFLSTATE *ps,
    int code);
static void shasm_block_setbigerr(SHASM_BLOCK *pb, SHASM_IFLSTATE *ps);
static long shasm_block_adjcap(long v);
static void shasm_block_clear(SHASM_BLOCK *pb);
static int shasm_block_addByte(SHASM_BLOCK *pb, int c);

static int shasm_block_encode(
    SHASM_BLOCK *pb,
    long entity,
    const SHASM_BLOCK_ENCODER *penc,
    int o_over,
    int o_strict);

/*
 * Set a block reader into an error state.
 * 
 * The provided code must not be SHASM_OKAY.  It can be anything else,
 * though it should be defined in shasm_error.
 * 
 * If the block reader is already in an error state, this function
 * performs no further action besides checking that the code is not
 * SHASM_OKAY.
 * 
 * If the block reader is not currently in an error state, this function
 * clears the buffer of the block reader to empty, sets the error
 * status, and sets the line number field to the current line number of
 * the input filter chain.
 * 
 * Parameters:
 * 
 *   pb - the block reader to set to error state
 * 
 *   ps - the input filter chain to query for the line number
 * 
 *   code - the error code to set
 */
static void shasm_block_seterr(
    SHASM_BLOCK *pb,
    SHASM_IFLSTATE *ps,
    int code) {
  
  /* Check parameters */
  if ((pb == NULL) || (ps == NULL) || (code == SHASM_OKAY)) {
    abort();
  }
  
  /* Only set error state if not already in error state */
  if (pb->code == SHASM_OKAY) {
    /* Set error state */
    shasm_block_clear(pb);
    pb->code = code;
    pb->line = shasm_input_count(ps);
  }
}

/*
 * Wrapper function for shasm_block_seterr that sets either the error
 * SHASM_ERR_HUGEBLOCK or SHASM_ERR_LARGEBLOCK.
 * 
 * HUGEBLOCK is set if the adjusted constant SHASM_BLOCK_MAXBUFFER is
 * greater than SHASM_BLOCK_MAXSTR (greater than to account for space
 * for terminating null).  LARGEBLOCK is set otherwise.
 * 
 * This function determines the appropriate error and calls through to
 * shasm_block_seterr with it.
 * 
 * Parameters:
 * 
 *   pb - the block reader to set to error state
 * 
 *   ps - the input filter chain to query for the line number
 */
static void shasm_block_setbigerr(SHASM_BLOCK *pb, SHASM_IFLSTATE *ps) {
  if (shasm_block_adjcap(SHASM_BLOCK_MAXBUFFER) > SHASM_BLOCK_MAXSTR) {
    shasm_block_seterr(pb, ps, SHASM_ERR_HUGEBLOCK);
  } else {
    shasm_block_seterr(pb, ps, SHASM_ERR_LARGEBLOCK);
  }
}

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
 * Encode an entity value using the regular string method and append the
 * output bytes to the block reader buffer.
 * 
 * If the block reader is already in an error state when this function
 * is called, this function fails immediately.
 * 
 * The given entity code must be zero or greater.
 * 
 * The provided encoder callback defines the encoding table that will be
 * used.  The encoding table defines the mapping of entity codes to
 * sequences of zero or more output bytes.  Unrecognized entity codes
 * are mapped to zero-length output byte sequences.
 * 
 * o_over must be one of the SHASM_BLOCK_OMODE constants.  They have the
 * following meanings:
 * 
 *   NONE -- the encoding table will be used for all entity codes.  The
 *   o_strict parameter is ignored.
 * 
 *   UTF8 -- entity codes in range zero up to and including
 *   SHASM_BLOCK_MAXCODE will be output in their UTF-8 encoding,
 *   ignoring the encoding table for this range of entity codes.  If
 *   o_strict is non-zero, then the surrogate range is excluded (see
 *   below).
 * 
 *   CESU8 -- same as UTF8, except supplemental codepoints are first
 *   encoded as a surrogate pair, and then each surrogate is encoded in
 *   UTF-8.  This is not standard UTF-8, but it is sometimes used when
 *   full Unicode support is lacking.
 * 
 *   U16LE -- entity codes in range zero up to and including
 *   SHASM_BLOCK_MAXCODE will be output in their UTF-16 encoding, in
 *   little endian order (least significant byte first).  The encoding
 *   table is ignored for entity codes in this range.  Supplemental 
 *   characters are encoded as a surrogate pair, as is standard for
 *   UTF-16.  If o_strict is non-zero, then the surrogate range is
 *   excluded (see below).
 * 
 *   U16BE -- same as U16LE, except big endian order (most significant
 *   byte first) is used.
 * 
 *   U32LE -- entity codes in range zero up to and including
 *   SHASM_BLOCK_MAXCODE will be output in their UTF-32 encoding, in
 *   little endian order (least significant byte first).  The encoding
 *   table is ignored for entity codes in this range.  If o_strict is
 *   non-zero, then the surrogate range is excluded (see below).
 * 
 *   U32BE -- same as U32LE, except big endian order (most significant
 *   byte first) is used.
 * 
 * For UTF8, CESU8, U16LE, U16BE, U32LE, and U32BE, the o_strict flag
 * can be used to exclude the surrogate range.  If the o_strict flag is
 * non-zero for these modes, then entity codes in Unicode surrogate
 * range (SHASM_BLOCK_MINSURROGATE to SHASM_BLOCK_MAXSURROGATE) will be
 * handled by the encoding table rather than by the UTF encoder.  If
 * o_strict is zero for these mdoes, then all entity codes in the range
 * zero to SHASM_BLOCK_MAXCODE will be handled by the UTF encoder.
 * 
 * This function fails if the block reader buffer runs out of space.  In
 * this case, the block reader buffer state is undefined, and only part
 * of the output bytecode may have been written.
 * 
 * However, this function does *not* set an error state in the block
 * reader.  This is the caller's responsibility.
 * 
 * Parameters:
 * 
 *   pb - the block reader
 * 
 *   entity - the entity code to encode
 * 
 *   penc - the encoding table
 * 
 *   o_over - the output override selection
 * 
 *   o_strict - non-zero for strict output override mode, zero for loose
 *   output override mode
 * 
 * Return:
 * 
 *   non-zero if successful, zero if the block reader was already in an
 *   error state or this function ran out of space in the block reader
 *   buffer
 */
static int shasm_block_encode(
    SHASM_BLOCK *pb,
    long entity,
    const SHASM_BLOCK_ENCODER *penc,
    int o_over,
    int o_strict) {
  /* @@TODO: replace placeholder */
  return shasm_block_addByte(pb, 'a');
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
  
  int status = 1;
  int c = 0;
  
  /* Check parameters */
  if ((pb == NULL) || (ps == NULL)) {
    abort();
  }
  
  /* Fail immediately if block reader in error state */
  if (pb->code != SHASM_OKAY) {
    status = 0;
  }
  
  /* Read zero or more bytes of whitespace and comments */
  while (status) {
    
    /* Read zero or more filtered HT, SP, or LF characters until either
     * a character that is not one of those three has been read, or EOF
     * or I/O error */
    for(c = shasm_input_get(ps);
        (c == SHASM_ASCII_HT) || (c == SHASM_ASCII_SP) ||
          (c == SHASM_ASCII_LF);
        c = shasm_input_get(ps));
    
    /* Fail if stopped on EOF or I/O error */
    if (c == SHASM_INPUT_EOF) {
      /* Fail on EOF */
      status = 0;
      shasm_block_seterr(pb, ps, SHASM_ERR_EOF);
      
    } else if (c == SHASM_INPUT_IOERR) {
      /* Fail on I/O error */
      status = 0;
      shasm_block_seterr(pb, ps, SHASM_ERR_IO);
    }
    
    /* If the non-whitespace character that was read is not the
     * ampersand, then this is the first character of the token -- in
     * this case, set the line number field of the block reader to the
     * current line number, unread the character, and break out of this
     * loop */
    if (status && (c != SHASM_ASCII_AMPERSAND)) {
      /* First token character found */
      pb->line = shasm_input_count(ps);
      shasm_input_back(ps);
      break;
    }
    
    /* If we got here, we just read an ampersand beginning a comment --
     * proceed by reading characters until either LF, EOF, or I/O is
     * encountered; leave the LF read as it is part of the comment */
    if (status) {
      /* Read the comment characters */
      for(c = shasm_input_get(ps);
          (c != SHASM_ASCII_LF) && (c != SHASM_INPUT_EOF) &&
            (c != SHASM_INPUT_IOERR);
          c = shasm_input_get(ps));
      
      /* Fail if EOF or I/O error */
      if (c == SHASM_INPUT_EOF) {
        /* Fail on EOF */
        status = 0;
        shasm_block_seterr(pb, ps, SHASM_ERR_EOF);
      
      } else if (c == SHASM_INPUT_IOERR) {
        /* Fail on I/O error */
        status = 0;
        shasm_block_seterr(pb, ps, SHASM_ERR_IO);
      }
    }
    
    /* Now that we've read the comment, loop back to read any further
     * whitespace and comments */
  }
  
  /* If we got here successfully, we just unread the first character of
   * the token and set the line number of the token -- now clear the
   * buffer to prepare for reading the token characters */
  if (status) {
    shasm_block_clear(pb);
  }
  
  /* Read the first character of the token into the buffer, verifying
   * that it is in visible, printing US-ASCII range */
  if (status) {
    /* Read the character and check for error */
    c = shasm_input_get(ps);
    if (c == SHASM_INPUT_EOF) {
      /* Fail on EOF */
      status = 0;
      shasm_block_seterr(pb, ps, SHASM_ERR_EOF);
    
    } else if (c == SHASM_INPUT_IOERR) {
      /* Fail on I/O error */
      status = 0;
      shasm_block_seterr(pb, ps, SHASM_ERR_IO);
    }
    
    /* Check range */
    if (status &&
          ((c < SHASM_ASCII_VISPRINT_MIN) ||
            (c > SHASM_ASCII_VISPRINT_MAX))) {
      status = 0;
      shasm_block_seterr(pb, ps, SHASM_ERR_TOKENCHAR);
    }
    
    /* Add the character to the buffer */
    if (status) {
      if (!shasm_block_addByte(pb, c)) {
        status = 0;
        shasm_block_setbigerr(pb, ps);
      }
    }
  }
  
  /* If the first character read was vertical bar, read the next
   * character -- if it's semicolon, then add it to the buffer to yield
   * the "|;" token; if it's anything else, unread it */
  if (status && (pb->pBuf[0] == SHASM_ASCII_BAR)) {
    /* Read the next character */
    c = shasm_input_get(ps);
    if (c == SHASM_INPUT_EOF) {
      /* Fail on EOF */
      status = 0;
      shasm_block_seterr(pb, ps, SHASM_ERR_EOF);
    
    } else if (c == SHASM_INPUT_IOERR) {
      /* Fail on I/O error */
      status = 0;
      shasm_block_seterr(pb, ps, SHASM_ERR_IO);
    }
    
    /* If next character is semicolon, add it to buffer; else, unread
     * it */
    if (status && (c == SHASM_ASCII_SEMICOLON)) {
      /* Semicolon -- add it to buffer */
      if (!shasm_block_addByte(pb, c)) {
        status = 0;
        shasm_block_setbigerr(pb, ps);
      }
      
    } else {
      /* Not a semicolon -- unread it */
      shasm_input_back(ps);
    }
  }
  
  /* If the buffer currently is something other than ( ) [ ] , % ; " ' {
   * or the special token "|;" then read additional token characters;
   * else leave the buffer as it currently is */
  if (status &&
        (pb->pBuf[0] != SHASM_ASCII_LPAREN   ) &&
        (pb->pBuf[0] != SHASM_ASCII_RPAREN   ) &&
        (pb->pBuf[0] != SHASM_ASCII_LSQR     ) &&
        (pb->pBuf[0] != SHASM_ASCII_RSQR     ) &&
        (pb->pBuf[0] != SHASM_ASCII_COMMA    ) &&
        (pb->pBuf[0] != SHASM_ASCII_PERCENT  ) &&
        (pb->pBuf[0] != SHASM_ASCII_SEMICOLON) &&
        (pb->pBuf[0] != SHASM_ASCII_DQUOTE   ) &&
        (pb->pBuf[0] != SHASM_ASCII_SQUOTE   ) &&
        (pb->pBuf[0] != SHASM_ASCII_LCURL    ) &&
        (pb->pBuf[1] != SHASM_ASCII_SEMICOLON)) {
    
    /* Read zero or more additional token characters */
    while (status) {
      /* Read the next character */
      c = shasm_input_get(ps);
      if (c == SHASM_INPUT_EOF) {
        /* Fail on EOF */
        status = 0;
        shasm_block_seterr(pb, ps, SHASM_ERR_EOF);
      
      } else if (c == SHASM_INPUT_IOERR) {
        /* Fail on I/O error */
        status = 0;
        shasm_block_seterr(pb, ps, SHASM_ERR_IO);
      }
      
      /* If this is a stop character, then break out of the loop */
      if (status && (
            (c == SHASM_ASCII_HT       ) ||
            (c == SHASM_ASCII_SP       ) ||
            (c == SHASM_ASCII_LF       ) ||
            (c == SHASM_ASCII_LPAREN   ) ||
            (c == SHASM_ASCII_RPAREN   ) ||
            (c == SHASM_ASCII_LSQR     ) ||
            (c == SHASM_ASCII_RSQR     ) ||
            (c == SHASM_ASCII_COMMA    ) ||
            (c == SHASM_ASCII_PERCENT  ) ||
            (c == SHASM_ASCII_SEMICOLON) ||
            (c == SHASM_ASCII_AMPERSAND) ||
            (c == SHASM_ASCII_DQUOTE   ) ||
            (c == SHASM_ASCII_SQUOTE   ) ||
            (c == SHASM_ASCII_LCURL    ))) {
        break;
      }
      
      /* Check range */
      if (status &&
            ((c < SHASM_ASCII_VISPRINT_MIN) ||
              (c > SHASM_ASCII_VISPRINT_MAX))) {
        status = 0;
        shasm_block_seterr(pb, ps, SHASM_ERR_TOKENCHAR);
      }
      
      /* Not a stop character and range validated, so add to buffer */
      if (status) {
        if (!shasm_block_addByte(pb, c)) {
          status = 0;
          shasm_block_setbigerr(pb, ps);
        }
      }
    }

    /* If stopped on an inclusive stop character, add it to the buffer;
     * else the stop character is exclusive so unread it */
    if (status && (
          (c == SHASM_ASCII_DQUOTE) ||
          (c == SHASM_ASCII_SQUOTE) ||
          (c == SHASM_ASCII_LCURL ))) {
      /* Inclusive stop character -- add to buffer */
      if (!shasm_block_addByte(pb, c)) {
        status = 0;
        shasm_block_setbigerr(pb, ps);
      }
      
    } else if (status) {
      /* Exclusive stop character -- unread it */
      shasm_input_back(ps);
    }
  }
  
  /* Return status */
  return status;
}

/*
 * shasm_block_string function.
 */
int shasm_block_string(
    SHASM_BLOCK *pb,
    SHASM_IFLSTATE *ps,
    const SHASM_BLOCK_STRING *sp) {
  /* @@TODO: placeholder */
  return 0;
}
