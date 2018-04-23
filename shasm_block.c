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
 */
#define SHASM_BLOCK_MINBUFFER (32L)

/*
 * The maximum capacity of the block buffer in bytes, as a signed long
 * constant.
 * 
 * This includes space for the terminating null character.  Blocks may
 * be no longer than one less than this value in length.  Since the
 * maximum block size supported by Shastina is 32,766 bytes, this value
 * is one greater than that.
 * 
 * This value must not be less than SHASM_BLOCK_MINBUFFER.  It must also
 * be less than half of LONG_MAX, so that doubling the capacity never
 * results in overflow.
 */
#define SHASM_BLOCK_MAXBUFFER (32767L)

/*
 * The initial capacity of a temporary buffer (SHASM_BLOCK_TBUF) in
 * bytes.
 * 
 * This value must be at least one, and not be greater than 
 * SHASM_BLOCK_MAXBUFFER.
 */
#define SHASM_BLOCK_MINTBUF (8)

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
 * The first high surrogate and the first low surrogate codepoints.
 * 
 * The high surrogate encodes the ten most significant bits of the
 * supplemental offset and comes first in the pair, while the low
 * surrogate encodes the ten least significant bits of the supplemental
 * offset and comes second in the pair.
 */
#define SHASM_BLOCK_HISURROGATE (0xd800L)
#define SHASM_BLOCK_LOSURROGATE (0xdc00L)

/*
 * The minimum Unicode codepoint that is in supplemental range.
 */
#define SHASM_BLOCK_MINSUPPLEMENTAL (0x10000L)

/*
 * The minimum codepoints for which 2-byte, 3-byte, and 4-byte UTF-8
 * encodings are used.
 */
#define SHASM_BLOCK_UTF8_2BYTE (0x80L)
#define SHASM_BLOCK_UTF8_3BYTE (0x800L)
#define SHASM_BLOCK_UTF8_4BYTE (0x10000L)

/*
 * The leading byte masks for 2-byte, 3-byte, and 4-byte UTF-8
 * encodings.
 */
#define SHASM_BLOCK_UTF8_2MASK (0xC0)
#define SHASM_BLOCK_UTF8_3MASK (0xE0)
#define SHASM_BLOCK_UTF8_4MASK (0XF0)

/*
 * Nesting level change constants.
 */
#define SHASM_BLOCK_DOVER_NEST_STAY  (0)  /* Keep nesting level as-is */
#define SHASM_BLOCK_DOVER_NEST_INC   (1)  /* Increment nesting level */
#define SHASM_BLOCK_DOVER_NEST_DEC   (2)  /* Decrement nesting level */
#define SHASM_BLOCK_DOVER_NEST_RESET (3)  /* Reset level to one */

/*
 * The initial and maximum capacities of the circular buffer, in bytes.
 */
#define SHASM_BLOCK_CIRCBUF_INITCAP (8L)
#define SHASM_BLOCK_CIRCBUF_MAXCAP  (32767L)

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
   * starts out allocated at a capacity of SHASM_BLOCK_MINBUFFER.  It
   * grows by doubling as necessary, to a maximum capacity value of
   * SHASM_BLOCK_MAXBUFFER.
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
 * Structure for storing a temporary buffer.
 * 
 * Use the shasm_block_tbuf functions to initialize and interact with
 * this structure.
 */
typedef struct {
  
  /*
   * The length of the buffer in bytes.
   */
  long len;
  
  /*
   * Pointer to the buffer.
   * 
   * This is NULL if len is zero, else it is a dynamically allocated
   * pointer.
   */
  unsigned char *pBuf;
  
} SHASM_BLOCK_TBUF;

/*
 * Structure for storing decoding map overlay state.
 * 
 * Use the shasm_block_dover functions to work with this structure.
 */
typedef struct {
  
  /*
   * The decoding map that this overlay is set on top of.
   */
  SHASM_BLOCK_DECODER dec;
  
  /*
   * The most recent branch taken, or -1 to indicate that no branches
   * have been taken from root.
   * 
   * This starts out at -1 to indicate the decoding map is at the root
   * node and no branches have been taken.
   * 
   * When a successful branch is taken, the unsigned byte value (0-255)
   * of the branch is stored in this field.
   */
  int recent;
  
  /*
   * The type of string currently being decoded.
   * 
   * This determines whether the string data is a "" '' or {} string.
   * 
   * It must be one of the SHASM_BLOCK_STYPE constants.
   */
  int stype;
  
  /*
   * The input override mode of the string currently being decoded.
   * 
   * This must be one of the SHASM_BLOCK_IMODE constants.  Use the
   * constant SHASM_BLOCK_IMODE_NONE if there are no input overrides.
   */
  int i_over;
  
  /*
   * The bracket nesting level.
   * 
   * This starts out at one and may never go below one.  An error occurs
   * if this reaches LONG_MAX.
   * 
   * The nesting level may only be changed in SHASM_BLOCK_STYPE_CURLY {}
   * strings.  Faults occur if the nesting level is changed for other
   * string types.
   */
  long nest_level;
  
} SHASM_BLOCK_DOVER;

/*
 * Structure for storing information relating to a circular buffer.
 * 
 * Use the shasm_block_circbuf functions to manipulate this structure.
 */
typedef struct {
  
  /*
   * Pointer to the buffer.
   * 
   * The size in bytes of this buffer is stored in the bufcap field.  If
   * bufcap is zero, then this field must be NULL.  Else, this field
   * must point to a dynamically allocated buffer.
   */
  unsigned char *pBuf;
  
  /*
   * Capacity of the buffer in bytes.
   * 
   * This starts out at zero for an empty buffer.  When the first byte
   * is appended, the buffer is allocated at a size in bytes matching
   * SHASM_BLOCK_CIRCBUF_INITCAP.  It then grows as necessary by
   * doubling up to a maximum of SHASM_BLOCK_CIRCBUF_MAXCAP.
   */
  long bufcap;
  
  /*
   * The number of bytes currently stored in the buffer.
   * 
   * This ranges from zero up to and including the value of the bufcap
   * field.
   */
  long length;
  
  /*
   * The byte index within the buffer of the byte that will be written
   * next.
   * 
   * If the length field is greater than zero, then the last byte of the
   * buffer will be at the offset (next - 1) if next is greater than
   * zero, or (bufcap - 1) if next is zero.
   * 
   * If the length field is greater than zero, then the first byte of
   * the buffer will be at the offset (next - length) if next is greater
   * than or equal to length, or else (next - length + bufcap).
   * 
   * The range of this field is zero up to (bufcap - 1) if bufcap is
   * greater than zero.  This field is always zero if bufcap is zero.
   */
  long next;
  
} SHASM_BLOCK_CIRCBUF;

/*
 * Structure for storing information relating to a speculation buffer.
 * 
 * Use the shasm_block_specbuf functions to manipulate this structure.
 */
typedef struct {
  
  /*
   * The circular buffer used to store the data for the speculation
   * buffer.
   */
  SHASM_BLOCK_CIRCBUF cb;
  
  /*
   * The number of bytes in the back buffer.
   * 
   * The circular buffer is divided into a back buffer (comes first) and
   * a front buffer (comes last).  Together the two buffers occupy the
   * full space of the circular buffer.
   * 
   * The number of bytes in the front buffer is the current length of
   * the circular buffer minus back_count.
   * 
   * The range of back_count is zero up to and including the current
   * length of the circular buffer.
   * 
   * back_count must be zero if the marked flag is zero.
   */
  long back_count;
  
  /*
   * The "mark" flag.
   * 
   * If non-zero, then there is a marked position that may be restored.
   * The back_count may be zero or greater in this case.
   * 
   * If zero, then there is no marked position.  The back_count must be
   * zero in this case.
   */
  int marked;
  
} SHASM_BLOCK_SPECBUF;

/*
 * Structure for storing information related to the surrogate buffer.
 * 
 * Use the shasm_block_surbuf functions to manipulate this structure.
 */
typedef struct {
  
  /*
   * The buffered surrogate, or zero if there is no buffered surrogate,
   * or -1 if the surrogate buffer is in an error condition.
   * 
   * This field starts out zero, to indicate that no surrogate is
   * buffered and there is no error.
   * 
   * If a low surrogate is encountered when this field has a high
   * surrogate, then surrogates are paired to select a supplemental
   * codepoint and this buffer is then cleared.  Encountering a low
   * surrogate when this buffer is empty causes the buffer to go into
   * error state since the surrogate is unpaired.
   * 
   * If a high surrogate is encountered when this field is empty, then
   * it is stored in the buffer.  If a high surrogate is encountered
   * when this field already has a high surrogate in it, then the buffer
   * changes to error state on account of improperly paired surrogates.
   * 
   * At the end of input, if this buffer has a high surrogate within it,
   * it changes to error state on account of an unpaired surrogate.
   */
  long buf;
  
} SHASM_BLOCK_SURBUF;

/* 
 * Local functions
 * ===============
 */

static void shasm_block_seterr(
    SHASM_BLOCK *pb,
    SHASM_IFLSTATE *ps,
    int code);
static void shasm_block_clear(SHASM_BLOCK *pb);
static int shasm_block_addByte(SHASM_BLOCK *pb, int c);

static void shasm_block_tbuf_init(SHASM_BLOCK_TBUF *pt);
static void shasm_block_tbuf_reset(SHASM_BLOCK_TBUF *pt);
static int shasm_block_tbuf_widen(SHASM_BLOCK_TBUF *pt, long tlen);
static unsigned char *shasm_block_tbuf_ptr(SHASM_BLOCK_TBUF *pt);
static long shasm_block_tbuf_len(SHASM_BLOCK_TBUF *pt);

static void shasm_block_dover_init(
    SHASM_BLOCK_DOVER *pdo,
    const SHASM_BLOCK_STRING *sp);
static int shasm_block_dover_reset(SHASM_BLOCK_DOVER *pdo, int nest);
static int shasm_block_dover_recent(SHASM_BLOCK_DOVER *pdo);
static int shasm_block_dover_stopped(SHASM_BLOCK_DOVER *pdo);
static int shasm_block_dover_branch(SHASM_BLOCK_DOVER *pdo, int c);
static long shasm_block_dover_entity(SHASM_BLOCK_DOVER *pdo);

static void shasm_block_circbuf_init(SHASM_BLOCK_CIRCBUF *pcb);
static void shasm_block_circbuf_reset(
    SHASM_BLOCK_CIRCBUF *pcb,
    int full);
static int shasm_block_circbuf_append(SHASM_BLOCK_CIRCBUF *pcb, int c);
static void shasm_block_circbuf_advance(
    SHASM_BLOCK_CIRCBUF *pcb,
    long d);
static long shasm_block_circbuf_length(SHASM_BLOCK_CIRCBUF *pcb);
static int shasm_block_circbuf_get(SHASM_BLOCK_CIRCBUF *pcb, long i);

static void shasm_block_specbuf_init(SHASM_BLOCK_SPECBUF *psb);
static void shasm_block_specbuf_reset(SHASM_BLOCK_SPECBUF *psb);
static int shasm_block_specbuf_detach(
    SHASM_BLOCK_SPECBUF *psb,
    SHASM_IFLSTATE *ps);
static int shasm_block_specbuf_get(
    SHASM_BLOCK_SPECBUF *psb,
    SHASM_IFLSTATE *ps);
static void shasm_block_specbuf_mark(SHASM_BLOCK_SPECBUF *psb);
static void shasm_block_specbuf_restore(SHASM_BLOCK_SPECBUF *psb);
static void shasm_block_specbuf_backtrack(SHASM_BLOCK_SPECBUF *psb);
static void shasm_block_specbuf_unmark(SHASM_BLOCK_SPECBUF *psb);

static void shasm_block_pair(long code, long *pHi, long *pLo);

static int shasm_block_ereg(
    SHASM_BLOCK *pb,
    long entity,
    const SHASM_BLOCK_ENCODER *penc,
    SHASM_BLOCK_TBUF *pt);

static int shasm_block_utf8(SHASM_BLOCK *pb, long entity, int cesu8);
static int shasm_block_utf16(SHASM_BLOCK *pb, long entity, int big);
static int shasm_block_utf32(SHASM_BLOCK *pb, long entity, int big);

static int shasm_block_encode(
    SHASM_BLOCK *pb,
    long entity,
    const SHASM_BLOCK_ENCODER *penc,
    int o_over,
    int o_strict,
    SHASM_BLOCK_TBUF *pt);

static long shasm_block_decode_inner(
    SHASM_BLOCK_DOVER *pdo,
    SHASM_BLOCK_SPECBUF *psb,
    SHASM_IFLSTATE *ps,
    int stype,
    int *pStatus);

static long shasm_block_decode_numeric(
    SHASM_BLOCK_DOVER *pdo,
    SHASM_BLOCK_SPECBUF *psb,
    SHASM_IFLSTATE *ps,
    int stype,
    SHASM_BLOCK_ESCLIST *pel,
    int *pStatus);

static int shasm_block_decode_entities(
    SHASM_BLOCK *pb,
    SHASM_BLOCK_DOVER *pdo,
    SHASM_BLOCK_SPECBUF *psb,
    SHASM_BLOCK_TBUF *pt,
    SHASM_IFLSTATE *ps,
    const SHASM_BLOCK_STRING *sp);

static int shasm_block_noesc(
    void *pCustom,
    long entity,
    SHASM_BLOCK_NUMESCAPE *pParam);

static long shasm_block_nomap(
    void *pCustom,
    long entity,
    unsigned char *pBuf,
    long buf_len);

static void shasm_block_surbuf_init(SHASM_BLOCK_SURBUF *psb);
static long shasm_block_surbuf_process(
    SHASM_BLOCK_SURBUF *psb,
    long v);
static int shasm_block_surbuf_finish(SHASM_BLOCK_SURBUF *psb);

static long shasm_block_read_utf8(SHASM_IFLSTATE *ps);

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
    /* If buffer capacity already at maximum capacity, fail */
    if (pb->buf_len >= SHASM_BLOCK_MAXBUFFER) {
      status = 0;
    }
    
    /* Room to grow still -- new capacity is minimum of double the
     * current capacity and the maximum capacity */
    if (status) {
      pb->buf_cap *= 2;
      if (pb->buf_cap > SHASM_BLOCK_MAXBUFFER) {
        pb->buf_cap = SHASM_BLOCK_MAXBUFFER;
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
 * Initialize a temporary buffer.
 * 
 * This initializes the temporary buffer to zero length with no actual
 * buffer allocated.
 * 
 * Do not call this function on a temporary buffer that has already been
 * initialized or a memory leak may occur.
 * 
 * Parameters:
 * 
 *   pt - the uninitialized temporary buffer to initialize
 */
static void shasm_block_tbuf_init(SHASM_BLOCK_TBUF *pt) {
  /* Check parameter */
  if (pt == NULL) {
    abort();
  }
  
  /* Initialize */
  memset(pt, 0, sizeof(SHASM_BLOCK_TBUF));
  pt->len = 0;
  pt->pBuf = NULL;
}

/*
 * Reset a temporary buffer.
 * 
 * This returns the temporary buffer to zero length, freeing any
 * dynamically allocated buffer.
 * 
 * Undefined behavior occurs if this is called on a temporary buffer
 * that has not yet been initialized.
 * 
 * Parameters:
 * 
 *   pt - the initialized temporary buffer to reset
 */
static void shasm_block_tbuf_reset(SHASM_BLOCK_TBUF *pt) {
  /* Check parameter */
  if (pt == NULL) {
    abort();
  }
  
  /* Free the memory buffer if allocated */
  if (pt->pBuf != NULL) {
    free(pt->pBuf);
    pt->pBuf = NULL;
  }
  
  /* Set the length to zero */
  pt->len = 0;
}

/*
 * Widen a temporary buffer if necessary to be at least the given size.
 * 
 * The buffer must have been initialized with shasm_block_tbuf_init or
 * undefined behavior occurs.  After calling this widening function, the
 * shasm_block_tbuf_reset function must eventually be called to free the
 * dynamically allocated buffer.
 * 
 * tlen is the number of bytes that the buffer should at least have.  It
 * must be zero or greater.
 * 
 * If tlen is greater than SHASM_BLOCK_MAXBUFFER, then this function
 * will fail.
 * 
 * If the current buffer size is greater than or equal to tlen, then
 * this function call does nothing.
 * 
 * If the current buffer size is zero and tlen is greater than zero,
 * then the target size will start out at SHASM_BLOCK_MINTBUF.  Else, if
 * the current buffer size is greater than zero and tlen is greater than
 * the current buffer size, the target size will start out at the
 * current size.  The target size is doubled until it is greater than
 * tlen, with the maximum size clamped at SHASM_BLOCK_MAXBUFFER.  The
 * buffer is then reallocated to this size and the function returns
 * successfully.
 * 
 * This function will always clear the temporary buffer to all zero
 * contents, regardless of whether the buffer was actually widened.
 * 
 * Parameters:
 * 
 *   pt - the temporary buffer to widen if necessary
 * 
 *   tlen - the minimum size in bytes
 * 
 * Return:
 * 
 *   non-zero if successful, zero if requested length was too large
 */
static int shasm_block_tbuf_widen(SHASM_BLOCK_TBUF *pt, long tlen) {
  int status = 1;
  long tl = 0;
  
  /* Check parameters */
  if ((pt == NULL) || (tlen < 0)) {
    abort();
  }
  
  /* Fail if requested length exceeds maximum buffer length */
  if (tlen > SHASM_BLOCK_MAXBUFFER) {
    status = 0;
  }
  
  /* Only widen if not yet large enough */
  if (status && (tlen > pt->len)) {
    
    /* Start target length at current length */
    tl = pt->len;
    
    /* If target length is zero, expand to initial capacity */
    if (tl < 1) {
      tl = SHASM_BLOCK_MINTBUF;
    }
    
    /* Keep doubling until greater than tlen */
    for( ; tl < tlen; tl *= 2);
    
    /* If result is greater than maximum buffer size, shrink to maximum
     * possible */
    if (tl > SHASM_BLOCK_MAXBUFFER) {
      tl = SHASM_BLOCK_MAXBUFFER;
    }
    
    /* Set size */
    pt->len = tl;
    
    /* (Re)allocate buffer to new size */
    if (pt->pBuf == NULL) {
      pt->pBuf = (unsigned char *) malloc((size_t) pt->len);
    } else {
      pt->pBuf = (unsigned char *) realloc(pt->pBuf, (size_t) pt->len);
    }
    if (pt->pBuf == NULL) {
      abort();
    }
  }
  
  /* If buffer not zero length, clear its contents */
  if (status && (pt->len > 0)) {
    memset(pt->pBuf, 0, (size_t) pt->len);
  }
  
  /* Return status */
  return status;
}

/* 
 * Get a pointer to the temporary buffer.
 * 
 * Undefined behavior occurs if the temporary buffer has not been
 * initialized with shasm_block_tbuf_init.
 * 
 * If the buffer length is greater than zero, a pointer to the internal
 * buffer is returned.  Else, NULL is returned.
 * 
 * The returned pointer is valid until the buffer is widened or reset.
 * 
 * Parameters:
 * 
 *   pt - the temporary buffer
 * 
 * Return:
 * 
 *   a pointer to the internal buffer, or NULL if the buffer is zero
 *   length
 */
static unsigned char *shasm_block_tbuf_ptr(SHASM_BLOCK_TBUF *pt) {
  
  /* Check parameter */
  if (pt == NULL) {
    abort();
  }
  
  /* Return pointer to internal buffer or NULL */
  return pt->pBuf;
}

/*
 * Return the current length of the temporary buffer in bytes.
 * 
 * The temporary buffer must have been initialized with
 * shasm_block_tbuf_init or undefined behavior occurs.
 * 
 * Parameters:
 * 
 *   pt - the temporary buffer to query
 * 
 * Return:
 * 
 *   the length of the temporary buffer in bytes
 */
static long shasm_block_tbuf_len(SHASM_BLOCK_TBUF *pt) {
  
  /* Check parameter */
  if (pt == NULL) {
    abort();
  }
  
  /* Return length */
  return pt->len;
}

/*
 * Initialize a decoding map overlay structure.
 * 
 * Provide the string type parameters to use to initialize the overlay
 * structure.  All necessary information will be copied into the
 * decoding map overlay structure.
 * 
 * The decoding map overlay structure does not need to be cleaned up or
 * deinitialized in any way before being released.
 * 
 * The underlying decoding map will be reset to root position by this
 * function.  Undefined behavior occurs if the underlying decoding map
 * is accessed by anything besides this decoding map overlay while the
 * overlay is in use.
 * 
 * Parameters:
 * 
 *   pdo - the decoding map overlay to initialize
 * 
 *   sp - the string type parameters to initialize the overlay with
 */
static void shasm_block_dover_init(
    SHASM_BLOCK_DOVER *pdo,
    const SHASM_BLOCK_STRING *sp) {
  /* @@TODO: */
}

/*
 * Reset the position of a decoding map overlay back to the root node,
 * possibly changing the nesting level.
 * 
 * The overlay structure must already have been initialized with
 * shasm_block_dover_init.
 * 
 * The underlying decoding map will be reset back to the root node
 * position, and the state of the overlay except for the nesting level
 * will be reset to its initial value.
 * 
 * The nest parameter is one of the SHASM_BLOCK_DOVER_NEST constants,
 * which specifies what to do with the nesting level on reset.
 * 
 * If STAY is specified, the nesting level stays at its current value.
 * 
 * If INC is specified, the nesting level increases by one.  This may
 * only be used for {} string types; a fault occurs if it is specified
 * for other string types.  The function fails if incrementing the
 * nesting level would cause the level to rise to LONG_MAX.  If the
 * function fails, the overlay structure is unmodified and not reset.
 * 
 * If DEC is specified, the nesting level decreases by one.  This may
 * only be used for {} string types; a fault occurs if it is specified
 * for other string types.  A fault occurs if this would cause the
 * nesting level to decrease below one.
 * 
 * If RESET is specified, the nesting level is set to the initial value
 * of one.
 * 
 * Parameters:
 * 
 *   pdo - the decoding overlay to reset
 * 
 *   nest - the nesting level code
 * 
 * Return:
 * 
 *   non-zero if successful; zero if failure due to nesting level
 *   overflow
 */
static int shasm_block_dover_reset(SHASM_BLOCK_DOVER *pdo, int nest) {
  /* @@TODO: */
  return 0;
}

/*
 * Return the unsigned byte value (0-255) of the most recent branch, or
 * -1 if no branches from the root node in the decoding map have been
 * taken yet.
 * 
 * The overlay structure must have been initialized with
 * shasm_block_dover_init.
 * 
 * Parameters:
 * 
 *   pdo - the decoding overlay to query
 * 
 * Return:
 * 
 *   the unsigned byte value (0-255) of the most recent branch, or -1 if
 *   no branches taken yet
 */
static int shasm_block_dover_recent(SHASM_BLOCK_DOVER *pdo) {
  /* @@TODO: */
  return 0;
}

/*
 * Determine whether the current node in the decoding map is a "stop"
 * node.
 * 
 * This function returns non-zero if at least one branch has been taken
 * and the most recent branch was for a byte corresponding to the ASCII
 * value of a "stop character."  Stop characters are defined as:
 * 
 * (1) For a '' string, the ' character
 * (2) For a "" string, the " character
 * (3) For a {} string, the { and } characters
 * 
 * Parameters:
 * 
 *   pdo - the decoding overlay to query
 * 
 * Return:
 * 
 *   non-zero if the current node is a stop node; zero if it is not
 */
static int shasm_block_dover_stopped(SHASM_BLOCK_DOVER *pdo) {
  /* @@TODO: */
  return 0;
}

/*
 * Attempt to branch from the current node in the decoding overlay.
 * 
 * c is a unsigned byte value (0-255) to branch for.  If a branch for
 * that byte value exists, the branch is taken and a non-zero value is
 * returned.  If no branch for that byte value exists, the position
 * stays on the current node and a zero value is returned.
 * 
 * Normally, this function calls through to the branch function of the
 * underlying decoding map.  However, in the following cases, this
 * overlay branch function will always return that no branch exists,
 * regardless of whether one actually does in the underlying decoding
 * map:
 * 
 * (1) If the current node is a stop node (shasm_block_dover_stopped),
 * then the branch function always fails because there are no branches
 * from a stop node.
 * 
 * (2) If the string type is "", then the branch corresponding to the
 * ASCII value for " fails if it is attempted as the first branch from
 * root position.
 * 
 * (3) If the string type is '', then the branch corresponding to the
 * ASCII value for ' fails if it is attempted as the first branch from
 * root position.
 * 
 * (4) If the string type is {} and the nesting level is one, then the
 * branch corresponding to the ASCII value for } fails if it is
 * attempted as the first branch from root position.
 * 
 * (5) If an input override mode is active, branches for byte values
 * with the most significant bit set (128-255) always fail.
 * 
 * Parameters:
 * 
 *   pdo - the decoding map overlay
 * 
 *   c - the unsigned byte value (0-255) to branch for
 * 
 * Return:
 * 
 *   non-zero if successful, zero if no branch exists and the position
 *   is unchanged
 */
static int shasm_block_dover_branch(SHASM_BLOCK_DOVER *pdo, int c) {
  /* @@TODO: */
  return 0;
}

/*
 * Return the entity code associated with the current node, or -1 if
 * there is no associated entity code.
 * 
 * This function normally calls through to the entity function of the
 * underlying decoding map.  The only exception is that if no branches
 * from root have been taken yet, this function always returns -1
 * regardless of whether the underlying decoding map has an entity code
 * associated with the empty key.
 * 
 * Parameters:
 * 
 *   pdo - the decoding map overlay to query
 * 
 * Return:
 * 
 *   the entity code associated with the current node, or -1 if there is
 *   no associated entity code
 */
static long shasm_block_dover_entity(SHASM_BLOCK_DOVER *pdo) {
  /* @@TODO: */
  return 0;
}

/*
 * Initialize a circular buffer structure.
 * 
 * Do not use this function on a buffer that has already been
 * initialized or a memory leak may occur.  Use the reset function to
 * reset a circular buffer that has already been initialized.
 * 
 * The circular buffer starts out empty.
 * 
 * Before the initialized circular buffer structure is released, be sure
 * to call the reset function to release any dynamically allocated
 * buffer that has been allocated.
 * 
 * Parameters:
 * 
 *   pcb - the uninitialized circular buffer to initialize
 */
static void shasm_block_circbuf_init(SHASM_BLOCK_CIRCBUF *pcb) {
  /* @@TODO: */
}

/*
 * Reset a circular buffer structure.
 * 
 * This operation clears the buffer back to empty.
 * 
 * If the full flag is non-zero, then a full reset is performed that
 * releases any associated buffer.  If the full flag is zero, then a
 * partial reset is performed that keeps the buffer allocated but just
 * resets the statistics to empty the buffer.
 * 
 * A full reset must be done before releasing the circular buffer
 * structure to avoid a memory leak.  Partial resets are recommended in
 * all other cases because they are more efficient.
 * 
 * Parameters:
 * 
 *   pcb - the circular buffer to reset
 * 
 *   full - non-zero for a full reset, zero for a partial reset
 */
static void shasm_block_circbuf_reset(
    SHASM_BLOCK_CIRCBUF *pcb,
    int full) {
  /* @@TODO: */
}

/*
 * Append a byte to the end of a circular buffer.
 * 
 * The buffer is reallocated at a larger size if necessary.  If the
 * buffer is full and at maximum capacity, then this function fails.
 * 
 * The provided byte value must be in unsigned range (0-255).
 * 
 * Parameters:
 * 
 *   pcb - the circular buffer
 * 
 *   c - the unsigned byte value (0-255) to append
 * 
 * Return:
 * 
 *   non-zero if successful, zero if no more capacity to append the byte
 */
static int shasm_block_circbuf_append(SHASM_BLOCK_CIRCBUF *pcb, int c) {
  /* @@TODO: */
  return 0;
}

/*
 * Remove zero or more bytes from the start of a circular buffer.
 * 
 * The d parameter indicates how many bytes to remove.  It must be zero
 * and no more than the length of the buffer (as determined by function
 * shasm_block_circbuf_length).  If zero, then no operation is
 * performed.  If greater than zero, then that many bytes are removed
 * from the start of the buffer.
 * 
 * A fault occurs if d is greater than the current length of the buffer.
 * 
 * Parameters:
 * 
 *   pcb - the circular buffer
 * 
 *   d - the number of bytes to remove from the start of the buffer
 */
static void shasm_block_circbuf_advance(
    SHASM_BLOCK_CIRCBUF *pcb,
    long d) {
  /* @@TODO: */
}

/*
 * Return the current length of a circular buffer.
 * 
 * The return value is in range zero up to and including the constant
 * SHASM_BLOCK_CIRCBUF_MAXCAP.
 * 
 * Parameters:
 * 
 *   pcb - the circular buffer to query
 * 
 * Return:
 * 
 *   the current number of bytes in the buffer
 */
static long shasm_block_circbuf_length(SHASM_BLOCK_CIRCBUF *pcb) {
  /* @@TODO: */
  return 0;
}

/*
 * Get a specific element within a circular buffer.
 * 
 * The index i is relative to the first element in the buffer, with zero
 * as the first element, one as the second element, and so forth.  The
 * range of i is zero up to and including one less than the current
 * length of the buffer (shasm_block_circbuf_length).
 * 
 * A fault occurs if i is out of range.  This function always faults if
 * it is called when the buffer is empty.
 * 
 * The return value is the unsigned value (0-255) of the byte at the
 * requested position within the buffer.
 * 
 * Parameters:
 * 
 *   pcb - the circular buffer to query
 * 
 *   i - the offset of the element to request
 */
static int shasm_block_circbuf_get(SHASM_BLOCK_CIRCBUF *pcb, long i) {
  /* @@TODO: */
  return 0;
}

/*
 * Initialize a speculation buffer structure.
 * 
 * Do not use this function on a buffer that has already been
 * initialized or a memory leak may occur.  Use the reset function to
 * reset a speculation buffer that has already been initialized.
 * 
 * The speculation buffer starts out empty and unmarked.
 * 
 * Before the initialized speculation buffer structure is released, be
 * sure to call the reset function to release any dynamically allocated
 * buffer that has been allocated.
 * 
 * Parameters:
 * 
 *   psb - the uninitialized speculation buffer to initialize
 */
static void shasm_block_specbuf_init(SHASM_BLOCK_SPECBUF *psb) {
  /* @@TODO: */
}

/*
 * Reset a speculation buffer structure.
 * 
 * This operation performs a full reset of the underlying circular
 * buffer, forcibly clearing all its contents.  If the detach function
 * (shasm_block_specbuf_detach) has not been successfully called before
 * performing a reset of the speculation buffer, buffered data may be
 * discarded, causing a discontinuity between where the speculation
 * buffer left off reading input and where the input filter stack starts
 * out reading input.
 * 
 * The reset function must be called before releasing the speculation
 * buffer structure to avoid a memory leak.
 * 
 * Parameters:
 * 
 *   psb - the speculation buffer to reset
 */
static void shasm_block_specbuf_reset(SHASM_BLOCK_SPECBUF *psb) {
  /* @@TODO: */
}

/*
 * Detach a speculation buffer to synchronize it with the underlying
 * input filter stream.
 * 
 * If the speculation buffer is unmarked and empty (its initial state),
 * then this function returns successfully without doing anything.
 * 
 * If the speculation buffer is unmarked and exactly one byte is
 * buffered in the (front) buffer, then this function backtracks the
 * provided input filter stack by one character (shasm_input_back),
 * empties the speculation buffer, and returns successfully.
 * 
 * (This case will fault if the passed input filter stack is already in
 * pushback mode and can't be backtracked further.  However, this fault
 * condition will never occur if the provided input filter stack has
 * only been accessed by the speculation buffer, since the speculation
 * buffer never backtracks the input filter stack except during detach
 * operations after it has read and buffered at least one byte from the
 * input filter stack.)
 * 
 * If the speculation buffer is marked or more than one byte is
 * buffered, then this function fails without doing anything further.
 * 
 * If this function succeeds, then the speculation buffer will be back
 * in its original state of empty and unmarked, and the input filter
 * stack can begin reading input right where the speculation buffer left
 * off.
 * 
 * Parameters:
 * 
 *   psb - the speculation buffer to detach
 * 
 *   ps - the input filter stack to detach from
 * 
 * Return:
 * 
 *   non-zero if successful detach, zero if detach failed
 */
static int shasm_block_specbuf_detach(
    SHASM_BLOCK_SPECBUF *psb,
    SHASM_IFLSTATE *ps) {
  /* @@TODO: */
  return 0;
}

/*
 * Read a filtered byte through the speculation buffer.
 * 
 * If the buffer is unmarked and empty (its initial state), then a
 * filtered byte is read from the provided input filter stack.  The
 * return value of this read operation is passed directly through
 * (including the SHASM_INPUT_EOF and SHASM_INPUT_IOERR returns) and the
 * speculation buffer state is unmodified in this case.
 * 
 * If the buffer is unmarked and not empty, then the next buffered byte
 * (at the start of the front buffer) is returned and removed from the
 * speculation buffer.
 * 
 * If the buffer is marked and the front buffer is empty, then a
 * filtered byte is read from the provided input filter stack.  If the
 * input filter stack returns SHASM_INPUT_EOF or SHASM_INPUT_IOERR, then
 * these special return codes are passed directly through without
 * affecting the state of the speculation buffer.  Otherwise, the byte
 * that was read is appended to the end of the back buffer and this byte
 * is returned.  If the byte could not be appended because the buffer is
 * filled to maximum capacity, then SHASM_INPUT_INVALID is returned; the
 * input filter stack is not backtracked in this case and the input byte
 * is discarded.
 * 
 * If the buffer is marked and the front buffer is not empty, then one
 * byte is transferred from the start of the front buffer to the end of
 * the back buffer, and this byte value is then returned.
 * 
 * Parameters:
 * 
 *   psb - the speculation buffer
 * 
 *   ps - the input filter stack
 * 
 * Return:
 * 
 *   the unsigned byte value (0-255), or SHASM_INPUT_EOF if EOF read, or
 *   SHASM_INPUT_IOERR if there was an I/O error, or SHASM_INPUT_INVALID
 *   if input was discarded because maximum capacity of the buffer was
 *   exceeded
 */
static int shasm_block_specbuf_get(
    SHASM_BLOCK_SPECBUF *psb,
    SHASM_IFLSTATE *ps) {
  /* @@TODO: */
  return SHASM_INPUT_IOERR;
}

/*
 * Mark the current input position as a position that can be restored.
 * 
 * This function discards any bytes currently in the back buffer and
 * sets the buffer to a marked state if not already marked.
 * 
 * Parameters:
 * 
 *   psb - the speculation buffer
 */
static void shasm_block_specbuf_mark(SHASM_BLOCK_SPECBUF *psb) {
  /* @@TODO: */
}

/*
 * Restore the most recently marked input position.
 * 
 * This function may only be called if the speculation buffer is in a
 * marked state (shasm_block_specbuf_mark).  A fault occurs if the
 * speculation buffer is unmarked.
 * 
 * Any bytes that are in the back buffer are transferred to the start of
 * the front buffer.  The mark flag is then cleared.
 * 
 * Parameters:
 * 
 *   psb - the speculation buffer
 */
static void shasm_block_specbuf_restore(SHASM_BLOCK_SPECBUF *psb) {
  /* @@TODO: */
}

/*
 * Backtrack input by one character.
 * 
 * This operation uses the speculation buffer for backtracking.  It does
 * not make use of the input filter stack's backtracking function.
 * 
 * This function may only be used if the speculation buffer is in a
 * marked state and there is at least one byte in the back buffer.  This
 * means that backtracking can go no further backwards than the most
 * recent mark.
 * 
 * This function transfers one byte from the end of the back buffer to
 * the start of the front buffer.
 * 
 * Parameters:
 * 
 *   psb - the speculation buffer
 */
static void shasm_block_specbuf_backtrack(SHASM_BLOCK_SPECBUF *psb) {
  /* @@TODO: */
}

/*
 * Remove any marks from the speculation buffer.
 * 
 * This operation is equivalent to marking the current position and then
 * immediately restoring to the current position.  This has the effect
 * of clearing any marks while leaving the input position unchanged.
 * 
 * Parameters:
 * 
 *   psb - the speculation buffer
 */
static void shasm_block_specbuf_unmark(SHASM_BLOCK_SPECBUF *psb) {
  /* @@TODO: */
}

/*
 * Encode a supplemental Unicode codepoint into a Surrogate pair.
 * 
 * The provided code must be in range SHASM_BLOCK_MINSUPPLEMENTAL up to
 * and including SHASM_BLOCK_MAXCODE.
 * 
 * The provided pointers must not be NULL, and they must not be equal to
 * each other.
 * 
 * To compute the surrogates, first determine the supplemental offset by
 * subtracting SHASM_BLOCK_MINSUPPLEMENTAL from the provided code.
 * 
 * Add the ten most significant bits of the supplemental offset to
 * SHASM_BLOCK_HISURROGATE to get the high surrogate.  Add the ten least
 * significant bits of the supplemental offset to the constant
 * SHASM_BLOCK_LOSURROGATE to get the low surrogate.
 * 
 * The high surrogate should appear before the low surrogate in the
 * output.
 * 
 * Parameters:
 * 
 *   code - the supplemental codepoint
 * 
 *   pHi - pointer to the long to receive the high surrogate
 * 
 *   pLo - pointer to the long to receive the low surrogate
 */
static void shasm_block_pair(long code, long *pHi, long *pLo) {
  
  long offs = 0;
  
  /* Check parameters */
  if ((code < SHASM_BLOCK_MINSUPPLEMENTAL) ||
      (code > SHASM_BLOCK_MAXCODE) ||
      (pHi == NULL) || (pLo == NULL) || (pHi == pLo)) {
    abort();
  }
  
  /* Get supplemental offset */
  offs = code - SHASM_BLOCK_MINSUPPLEMENTAL;
  
  /* Split offset into low and high surrogates */
  *pLo = offs & 0x3ffL;
  *pHi = (offs >> 10) & 0x3ffL;
  
  /* Add the surrogate offsets */
  *pLo += SHASM_BLOCK_LOSURROGATE;
  *pHi += SHASM_BLOCK_HISURROGATE;
}

/*
 * Encode an entity value with an encoding table and append the output
 * bytes to the block reader buffer.
 * 
 * This function does not account for output overrides.
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
 * The temporary buffer must be allocated by the caller and initialized.
 * This allows the same temporary buffer to be used across multiple
 * encoding calls for sake of efficiency.  The caller should reset the
 * temporary buffer when finished to prevent a memory leak.
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
 *   pt - an initialized temporary buffer
 * 
 * Return:
 * 
 *   non-zero if successful, zero if the block reader was already in an
 *   error state or this function ran out of space in the block reader
 *   buffer
 */
static int shasm_block_ereg(
    SHASM_BLOCK *pb,
    long entity,
    const SHASM_BLOCK_ENCODER *penc,
    SHASM_BLOCK_TBUF *pt) {
  
  int status = 1;
  long lv = 0;
  long x = 0;
  unsigned char *pc = NULL;
  
  /* Check parameters */
  if ((pb == NULL) || (entity < 0) || (penc == NULL) || (pt == NULL)) {
    abort();
  }
  
  /* Check that encoder pointer is defined */
  if (penc->fpMap == NULL) {
    abort();
  }
  
  /* Fail immediately if block reader in error status */
  if (pb->code != SHASM_OKAY) {
    status = 0;
  }
  
  /* Call the mapping function until the code has been read into the
   * temporary buffer with lv as the length of the code */
  while (status) {

    /* Try to map entity with current temporary buffer */
    lv = (*(penc->fpMap))(
            penc->pCustom,
            entity,
            shasm_block_tbuf_ptr(pt),
            shasm_block_tbuf_len(pt));

    /* If the temporary buffer was large enough, break from the loop */
    if (shasm_block_tbuf_len(pt) >= lv) {
      break;
    }
    
    /* Buffer wasn't large enough, so widen it before trying again */
    if (!shasm_block_tbuf_widen(pt, lv)) {
      status = 0;
    }
  }
  
  /* Write each byte to the buffer */
  if (status) {
    pc = shasm_block_tbuf_ptr(pt);
    for(x = 0; x < lv; x++) {
      if (!shasm_block_addByte(pb, pc[x])) {
        status = 0;
        break;
      }
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Encode an entity value according to the UTF-8 or CESU-8 encoding
 * systems and append the output bytes to the block reader buffer.
 * 
 * If the block reader is already in an error state when this function
 * is called, this function fails immediately.
 * 
 * The given entity code must be in range zero up to and including
 * SHASM_BLOCK_MAXCODE.  Surrogates are allowed, and will just be
 * encoded like any other codepoint.
 * 
 * If cesu8 is zero, then supplemental characters will be encoded
 * directly in UTF-8, which is standard.  If cesu8 is non-zero, then
 * supplemental characters will first be encoded as a surrogate pair
 * using shasm_block_pair, and then each surrogate codepoint will be
 * encoded in UTF-8.  This is not standard behavior, but it is sometimes
 * used.
 * 
 * This function fails if the block reader buffer runs out of space.  In
 * this case, the block reader buffer state is undefined, and only part
 * of the output bytecode may have been written.
 * 
 * However, this function does *not* set an error state in the block
 * reader.  This is the caller's responsibility.
 * 
 * The UTF-8 encoding system works as follows.  First, determine the
 * total number of output bytes for the codepoint c according to the
 * following table:
 * 
 *   (                               c < SHASM_BLOCK_UTF8_2BYTE) -> 1
 *   (c >= SHASM_BLOCK_UTF8_2BYTE && c < SHASM_BLOCK_UTF8_3BYTE) -> 2
 *   (c >= SHASM_BLOCK_UTF8_3BYTE && c < SHASM_BLOCK_UTF8_4BYTE) -> 3
 *   (c >= SHASM_BLOCK_UTF8_4BYTE                              ) -> 4
 * 
 * Second, extract zero to three continuation bytes, with the number of
 * continuation bytes as one less than the total number of bytes.  To
 * extract a continuation byte, take the six least significant bits of
 * the codepoint, put them in a byte, set the most significant bit of
 * that byte, and shift the codepoint right six bits.
 * 
 * Third, define the leading byte as the remaining bits after the
 * continuation byte extraction.  If the total number of bytes is
 * greater than one, then OR the leading byte with one of the following
 * masks:
 * 
 *   SHASM_BLOCK_UTF8_2MASK for two-byte UTF-8 codes
 *   SHASM_BLOCK_UTF8_3MASK for three-byte UTF-8 codes
 *   SHASM_BLOCK_UTF8_4MASK for four-byte UTF-8 codes
 * 
 * Finally, output the leading byte first.  Then, output any
 * continuation bytes, but output them in the reverse order from which
 * they were extracted.
 * 
 * Parameters:
 * 
 *   pb - the block reader
 * 
 *   entity - the entity code to encode
 * 
 *   cesu8 - non-zero for CESU-8 mode, zero for standard UTF-8
 * 
 * Return:
 * 
 *   non-zero if successful, zero if the block reader was already in an
 *   error state or this function ran out of space in the block reader
 *   buffer
 */
static int shasm_block_utf8(SHASM_BLOCK *pb, long entity, int cesu8) {
  
  int status = 1;
  long s1 = 0;
  long s2 = 0;
  int codelen = 0;
  unsigned char contb[3];
  int i = 0;
  
  /* Initialize buffer */
  memset(&(contb[0]), 0, 3);
  
  /* Check parameters */
  if ((pb == NULL) || (entity < 0) || (entity > SHASM_BLOCK_MAXCODE)) {
    abort();
  }
  
  /* Fail immediately if block reader in error status */
  if (pb->code != SHASM_OKAY) {
    status = 0;
  }
  
  /* If CESU-8 mode is active and the entity code is in supplemental
   * range, then split into a surrogate pair and recursively call this
   * function in regular UTF-8 mode to encode the high surrogate and
   * then use this function call to encode the low surrogate */
  if (status && cesu8 && (entity >= SHASM_BLOCK_MINSUPPLEMENTAL)) {
    /* Split into surrogates */
    shasm_block_pair(entity, &s1, &s2);

    /* Recursively encode the high surrogate */
    if (!shasm_block_utf8(pb, s1, 0)) {
      status = 0;
    }
    
    /* Encode the low surrogate in this function call */
    entity = s2;
  }
  
  /* Determine the total number of bytes in the UTF-8 encoding */
  if (status) {
    if (entity < SHASM_BLOCK_UTF8_2BYTE) {
      codelen = 1;
    
    } else if (entity < SHASM_BLOCK_UTF8_3BYTE) {
      codelen = 2;
      
    } else if (entity < SHASM_BLOCK_UTF8_4BYTE) {
      codelen = 3;
      
    } else {
      codelen = 4;
    }
  }
  
  /* Extract continuation bytes (if any) */
  if (status) {
    for(i = 0; i < (codelen - 1); i++) {
      contb[i] = (unsigned char) ((entity & 0x3f) | 0x80);
      entity >>= 6;
    }
  }
  
  /* Reverse order of extracted continuation bytes so they are in output
   * order -- this is only needed if there is more than one continuation
   * byte (codelen 3 and 4) */
  if (status && (codelen == 3)) {
    /* Two continuation bytes, so swap them */
    i = contb[0];
    contb[0] = contb[1];
    contb[1] = (unsigned char) i;
    
  } else if (status && (codelen == 4)) {
    /* Three continuation bytes, so swap first and third */
    i = contb[0];
    contb[0] = contb[2];
    contb[2] = (unsigned char) i;
  }
  
  /* Append the leading byte to the block reader buffer */
  if (status) {
    /* Leading byte is remaining bits */
    i = (int) entity;
    
    /* If codelen more than one, add appropriate mask */
    if (codelen == 2) {
      i |= SHASM_BLOCK_UTF8_2MASK;
    
    } else if (codelen == 3) {
      i |= SHASM_BLOCK_UTF8_3MASK;
    
    } else if (codelen == 4) {
      i |= SHASM_BLOCK_UTF8_4MASK;
    }
    
    /* Append leading byte */
    if (!shasm_block_addByte(pb, i)) {
      status = 0;
    }
  }
  
  /* Append any trailing continuation bytes */
  if (status) {
    for(i = 0; i < (codelen - 1); i++) {
      if (!shasm_block_addByte(pb, contb[i])) {
        status = 0;
        break;
      }
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Encode an entity value according to the UTF-16 encoding system and
 * append the output bytes to the block reader buffer.
 * 
 * If the block reader is already in an error state when this function
 * is called, this function fails immediately.
 * 
 * The given entity code must be in range zero up to and including
 * SHASM_BLOCK_MAXCODE.  Surrogates are allowed, and will just be
 * encoded like any other codepoint.  Supplemental characters are always
 * encoded as surrogate pairs using shasm_block_pair.
 * 
 * If big is non-zero, each UTF-16 character will be encoded in big
 * endian order, with the most significant byte first.  If big is zero,
 * each UTF-16 character will be encoded in little endian order, with
 * the least significant byte first.
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
 *   big - non-zero for big endian, zero for little endian
 * 
 * Return:
 * 
 *   non-zero if successful, zero if the block reader was already in an
 *   error state or this function ran out of space in the block reader
 *   buffer
 */
static int shasm_block_utf16(SHASM_BLOCK *pb, long entity, int big) {
  
  int status = 1;
  long s1 = 0;
  long s2 = 0;
  unsigned char vb[2];
  int c = 0;
  
  /* Initialize buffer */
  memset(&(vb[0]), 0, 2);
  
  /* Check parameters */
  if ((pb == NULL) || (entity < 0) || (entity > SHASM_BLOCK_MAXCODE)) {
    abort();
  }
  
  /* Fail immediately if block reader in error status */
  if (pb->code != SHASM_OKAY) {
    status = 0;
  }
  
  /* If the entity code is in supplemental range, then split into a
   * surrogate pair and recursively call this function to encode the
   * high surrogate and then use this function call to encode the low
   * surrogate */
  if (status && (entity >= SHASM_BLOCK_MINSUPPLEMENTAL)) {
    /* Split into surrogates */
    shasm_block_pair(entity, &s1, &s2);

    /* Recursively encode the high surrogate */
    if (!shasm_block_utf16(pb, s1, big)) {
      status = 0;
    }
    
    /* Encode the low surrogate in this function call */
    entity = s2;
  }
  
  /* Entity should now be in range 0x0-0xffff -- split into two bytes in
   * vb, in little endian order */
  if (status) {
    vb[0] = (unsigned char) (entity & 0xff);
    vb[1] = (unsigned char) ((entity >> 8) & 0xff);
  }
  
  /* If in big endian order, reverse the two bytes */
  if (status && big) {
    c = vb[0];
    vb[0] = vb[1];
    vb[1] = (unsigned char) c;
  }
  
  
  /* Append the bytes to block reader buffer */
  if (status) {
    if (!shasm_block_addByte(pb, vb[0])) {
      status = 0;
    }
      
    if (status) {
      if (!shasm_block_addByte(pb, vb[1])) {
        status = 0;
      }
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Encode an entity value according to the UTF-32 encoding system and
 * append the output bytes to the block reader buffer.
 * 
 * If the block reader is already in an error state when this function
 * is called, this function fails immediately.
 * 
 * The given entity code must be in range zero up to and including
 * SHASM_BLOCK_MAXCODE.  Surrogates are allowed, and will just be
 * encoded like any other codepoint.
 * 
 * If big is non-zero, each UTF-32 character will be encoded in big
 * endian order, with the most significant byte first.  If big is zero,
 * each UTF-32 character will be encoded in little endian order, with
 * the least significant byte first.
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
 *   big - non-zero for big endian, zero for little endian
 * 
 * Return:
 * 
 *   non-zero if successful, zero if the block reader was already in an
 *   error state or this function ran out of space in the block reader
 *   buffer
 */
static int shasm_block_utf32(SHASM_BLOCK *pb, long entity, int big) {
  
  int status = 1;
  unsigned char vb[4];
  int c = 0;
  
  /* Initialize buffer */
  memset(&(vb[0]), 0, 4);
  
  /* Check parameters */
  if ((pb == NULL) || (entity < 0) || (entity > SHASM_BLOCK_MAXCODE)) {
    abort();
  }
  
  /* Fail immediately if block reader in error status */
  if (pb->code != SHASM_OKAY) {
    status = 0;
  }
  
  /* Split entity into four bytes in vb, in little endian order */
  if (status) {
    vb[0] = (unsigned char) (entity & 0xff);
    vb[1] = (unsigned char) ((entity >> 8) & 0xff);
    vb[2] = (unsigned char) ((entity >> 16) & 0xff);
    vb[3] = (unsigned char) ((entity >> 24) & 0xff);
  }
  
  /* If in big endian order, reverse the four bytes */
  if (status && big) {
    c = vb[0];
    vb[0] = vb[3];
    vb[3] = (unsigned char) c;
    
    c = vb[1];
    vb[1] = vb[2];
    vb[2] = (unsigned char) c;
  }
  
  /* Append the bytes to block reader buffer */
  if (status) {
    if (!shasm_block_addByte(pb, vb[0])) {
      status = 0;
    }
      
    if (status) {
      if (!shasm_block_addByte(pb, vb[1])) {
        status = 0;
      }
    }
    
    if (status) {
      if (!shasm_block_addByte(pb, vb[2])) {
        status = 0;
      }
    }
    
    if (status) {
      if (!shasm_block_addByte(pb, vb[3])) {
        status = 0;
      }
    }
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
 * The temporary buffer must be allocated by the caller and initialized.
 * This allows the same temporary buffer to be used across multiple
 * encoding calls for sake of efficiency.  The caller should reset the
 * temporary buffer when finished to prevent a memory leak.
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
 *   pt - an initialized temporary buffer
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
    int o_strict,
    SHASM_BLOCK_TBUF *pt) {
  
  int status = 1;
  
  /* Check parameters, except for o_over */
  if ((pb == NULL) || (entity < 0) || (penc == NULL) || (pt == NULL)) {
    abort();
  }
  
  /* Fail immediately if block reader in error status */
  if (pb->code != SHASM_OKAY) {
    status = 0;
  }
  
  /* If entity is outside of Unicode codepoint range, set o_over to
   * NONE since output overrides are never used outside of that range */
  if (status && (entity > SHASM_BLOCK_MAXCODE)) {
    o_over = SHASM_BLOCK_OMODE_NONE;
  }
  
  /* If strict flag is on and entity is in Unicode surrogate range, set
   * o_over to NONE since output overrides are never used on surrogates
   * in strict mode */
  if (status && o_strict &&
      (entity >= SHASM_BLOCK_MINSURROGATE) &&
        (entity <= SHASM_BLOCK_MAXSURROGATE)) {
    o_over = SHASM_BLOCK_OMODE_NONE;
  }
  
  /* o_over now has the actual mode to use for this entity -- dispatch
   * to appropriate routine */
  if (status) {
    switch (o_over) {
      
      /* Regular string encoding */
      case SHASM_BLOCK_OMODE_NONE:
        if (!shasm_block_ereg(pb, entity, penc, pt)) {
          status = 0;
        }
        break;
      
      /* UTF-8 encoding */
      case SHASM_BLOCK_OMODE_UTF8:
        if (!shasm_block_utf8(pb, entity, 0)) {
          status = 0;
        }
        break;
      
      /* CESU-8 encoding */
      case SHASM_BLOCK_OMODE_CESU8:
        if (!shasm_block_utf8(pb, entity, 1)) {
          status = 0;
        }
        break;
      
      /* UTF-16 LE encoding */
      case SHASM_BLOCK_OMODE_U16LE:
        if (!shasm_block_utf16(pb, entity, 0)) {
          status = 0;
        }
        break;
      
      /* UTF-16 BE encoding */
      case SHASM_BLOCK_OMODE_U16BE:
        if (!shasm_block_utf16(pb, entity, 1)) {
          status = 0;
        }
        break;
      
      /* UTF-32 LE encoding */
      case SHASM_BLOCK_OMODE_U32LE:
        if (!shasm_block_utf32(pb, entity, 0)) {
          status = 0;
        }
        break;
      
      /* UTF-32 BE encoding */
      case SHASM_BLOCK_OMODE_U32BE:
        if (!shasm_block_utf32(pb, entity, 1)) {
          status = 0;
        }
        break;
      
      /* Unrecognized mode */
      default:
        abort();
    }
  }
  
  /* Return status */
  return status;
}

/*
 * The inner decoding function for regular string decoding.
 * 
 * Pass an initialized decoding map overlay to use for decoding.  This
 * overlay may be in any position upon entry -- it will be reset by this
 * function to root position at the beginning.  However, this function
 * will preserve the nesting level upon entry, so the nesting level
 * should be set properly.  The position in the decoding map overlay
 * upon return will be at root position with a proper nesting level
 * (unless there is an error, in which case the state of the decoding
 * map overlay is undefined).
 * 
 * Also pass an initialized speculation buffer to use for decoding.  The
 * buffer will be unmarked at the start of the function.  The provided
 * input filter stack will be used in conjunction with the speculation
 * buffer to read bytes that shall be decoded.  See later in this
 * function specification for the state of the speculation buffer on
 * return.
 * 
 * The stype parameter must be a SHASM_BLOCK_STYPE constant describing
 * the type of string that is being decoded.
 * 
 * pStatus must not be NULL.  It points to the error status code, which
 * should be one of the constants from shasm_error.  If *pStatus is not
 * SHASM_OKAY upon entry to this function, this function fails
 * immediately, returning -1, leaving the error status as it is, and
 * performing no further operation.
 * 
 * If the return value is zero or greater, then the function has
 * succeeded, *pStatus will be SHASM_OKAY, and the return value equals
 * an entity code that has been decoded from input.  The speculation
 * buffer will be unmarked in this state and ready to read the next byte
 * of input.  It will not have been detached, however.
 * 
 * If the return value is -1 and *pStatus is SHASM_OKAY, then the
 * function has succeeded but it hasn't managed to decode another entity
 * code.  In this case, the speculation buffer will be unmarked and
 * detached, so that the next input byte can be read from the input
 * filter stack.
 * 
 * If the return value is -1 and *pStatus is not SHASM_OKAY, then the
 * function has failed and *pStatus holds the error code.  The state of
 * the speculation buffer in this case is undefined.
 * 
 * The decoding algorithm is described below.  Note that this function
 * does not handle numeric escapes.
 * 
 * Before beginning, the function unmarks the speculation buffer and
 * resets the decoding map overlay to root position (preserving the
 * nesting level, however).  It also sets an internal nesting level
 * alteration variable to zero, indicating no nesting change.
 * 
 * The decoding loop begins by reading a byte from the input through the
 * speculation buffer.  EOF or IOERR returns result in the function
 * failing on SHASM_ERR_EOF or SHASM_ERR_IO.  INVALID returns (which
 * indicate a speculation buffer overflow) result in the function
 * failing on SHASM_ERR_OVERSPEC.
 * 
 * Then, the decoding loop clears an internal stop flag.  It sets an
 * internal first branch flag if the decoding map is at root position,
 * otherwise clearing the internal first branch flag.
 * 
 * Next, the loop attempts to branch in the decoding map overlay
 * according to the byte that was just read.
 * 
 * If branching succeeds and the new overlay position has an associated
 * entity code, the speculation buffer is marked and the entity code is
 * remembered as the largest candidate entity code (overwriting any
 * previous candidates).
 * 
 * If branching succeeds and the new overlay position is a stop node,
 * then the stop flag is set.
 * 
 * If branching succeeds, and the string type is {}, and the first
 * branch flag is set, and the most recent branch was for { or } then
 * set the internal nesting level alteration variable to increment or
 * decrement.
 * 
 * If branching fails, set the stop flag.
 * 
 * Keep looping until the stop flag is set (or there is an error).
 * 
 * After the loop completes, check whether a candidate entity code has
 * been recorded.  If one has, restore the speculation buffer to the
 * most recently marked position and return the candidate entity code.
 * If no candidate entity code has been recorded, then attempt to detach
 * the speculation buffer.  If the detach succeeds, return successfully
 * that no entity code has been read.  If the detach fails, then this
 * function fails with SHASM_ERR_STRCHAR.
 * 
 * Before returning, the decoding map overlay is reset, possibly
 * changing the nesting level in the process, depending on the setting
 * of the internal nesting level alteration variable.  If the reset
 * fails due to nesting level overflow, this function fails on the error
 * SHASM_ERR_STRNEST.
 * 
 * A summary of the errors this function may generate:
 * 
 * - SHASM_ERR_EOF if an EOF was encountered in input.
 * - SHASM_ERR_IO if there was an I/O error reading input.
 * - SHASM_ERR_OVERSPEC if the speculation buffer overflowed.
 * - SHASM_ERR_STRCHAR if an input byte can't be decoded (*see below).
 * - SHASM_ERR_STRNEST if the curly nesting level overflowed.
 * 
 * For SHASM_ERR_STRNEST, this function only generates that error if it
 * can't decode something and it is not possible to detach the
 * speculation buffer to give the caller another chance to decode the
 * bytes using the input filter stack.  The stop node system and the
 * kinds of decoding keys that are ignored by the decoding map overlay
 * should ensure that if SHASM_ERR_STRNEST is generated, the bytes
 * couldn't have been decoded by a built-in key or an input override,
 * either.
 * 
 * Parameters:
 * 
 *   pdo - the decoding map overlay
 * 
 *   psb - the speculation buffer
 * 
 *   ps - the input filter stack
 * 
 *   stype - the string type constant
 * 
 *   pStatus - pointer to the error status field
 * 
 * Return:
 * 
 *   a decoded entity code (zero or greater), or -1 if no entity code
 *   has been read and the speculation buffer has been detached, or -1
 *   if there has been an error (distinguish between the two -1 cases
 *   by checking the error status field for an error)
 */
static long shasm_block_decode_inner(
    SHASM_BLOCK_DOVER *pdo,
    SHASM_BLOCK_SPECBUF *psb,
    SHASM_IFLSTATE *ps,
    int stype,
    int *pStatus) {
  /* @@TODO: */
  return -1;
}

/*
 * Wrapper around shasm_block_decode_inner that adds support for numeric
 * escapes.
 * 
 * The interface of this function is the same as for the function
 * shasm_block_decode_inner, except that there's an extra parameter that
 * specifies the numeric escape list object to query for numeric
 * escapes, and this function may generate some additional error codes.
 * 
 * This function normally passes through all parameters (except the
 * escape list) to the underlying inner function and returns the return
 * value returned from that function.
 * 
 * The only exception is that if the inner function returns an entity
 * code that matches a registered numeric escape, this function will
 * read the numeric escape code from input through the speculation
 * buffer, and then return the decoded numeric code instead of the
 * original entity code received from the inner decoder.
 * 
 * The numeric decoder begins by marking the current position in the
 * speculation buffer so that the backtracking function is available.
 * 
 * It then reads and decodes numeric digits from input (base-10 or
 * base-16 depending on the setting for the numeric escape) until one of
 * the following conditions occurs:
 * 
 * (1) A non-digit is encountered.  Digits are US-ASCII with letter
 * digits case-insensitive (uppercase or lowercase).  Base-10 or base-16
 * depending on the setting for this numeric escape type.
 * 
 * (2) SHASM_INPUT_EOF is encountered.
 * 
 * (3) SHASM_INPUT_IOERR is encountered.
 * 
 * (4) SHASM_INPUT_INVALID is encountered.
 * 
 * (5) The maximum digit count is reached.  This maximum count is
 * determined by the setting for this numeric escape type.  If there is
 * no maximum digit count for this type of numeric escape, then this
 * condition will never occur.
 * 
 * (6) The decoded numeric value exceeds the maximum entity code.  The
 * maximum entity code is determined by the setting for this numeric
 * escape type.
 * 
 * Conditions (2)-(4) result in this function failing and setting an
 * error code -- SHASM_ERR_EOF, SHASM_ERR_IO, SHASM_ERR_OVERSPEC,
 * respectively.
 * 
 * Condition (6) results in this function failing and setting the error
 * code SHASM_ERR_NUMESCRANGE.
 * 
 * For conditions (1) and (5), fail with SHASM_ERR_BADNUMESC if the
 * number of digits read is not at least the minimum number of digits
 * required by this particular escape, and fail with SHASM_ERR_NUMESCSUR
 * if the escape code setting blocks surrogate codes but the decoded
 * value is in surrogate range.  Otherwise, proceed.  However, before
 * proceeding, backtrack one character for condition (1) to unread the
 * non-digit character, and then in both cases unmark the speculation
 * buffer.
 * 
 * Once a numeric escape has been decoded, if there is a terminal byte
 * registered for this numeric escape, read another byte from the
 * speculation buffer.  If this results in EOF, IOERR, or INVALID, fail
 * with the same error codes listed above.  If this is a byte value that
 * mismatches the registered terminal byte value, fail with the error
 * SHASM_ERR_BADNUMESC.  Otherwise, return the numeric escape.
 * 
 * Parameters:
 * 
 *   pdo - the decoding map overlay
 * 
 *   psb - the speculation buffer
 * 
 *   ps - the input filter stack
 * 
 *   stype - the string type constant
 * 
 *   pel - the numeric escape list
 * 
 *   pStatus - pointer to the error status field
 * 
 * Return:
 * 
 *   a decoded entity code (zero or greater), or -1 if no entity code
 *   has been read and the speculation buffer has been detached, or -1
 *   if there has been an error (distinguish between the two -1 cases
 *   by checking the error status field for an error)
 */
static long shasm_block_decode_numeric(
    SHASM_BLOCK_DOVER *pdo,
    SHASM_BLOCK_SPECBUF *psb,
    SHASM_IFLSTATE *ps,
    int stype,
    SHASM_BLOCK_ESCLIST *pel,
    int *pStatus) {
  /* @@TODO: */
  return -1;
}

/*
 * Decode a sequence of zero or more entity codes from input and send
 * them to the encoding phase.
 * 
 * This function handles input bytes that can be decoded with the
 * decoding map, as well as numeric escapes.  It does not handle
 * built-in terminal keys and, if an input override is selected, it does
 * not handle bytes with their most significant bit set to one.  This
 * function will send all entities it decodes to the encoding phase
 * (shasm_block_encode), and return when it encounters something it
 * can't decode (or if there's an error).
 * 
 * On a successful return, the speculation buffer will have been
 * detached, so the caller can retry reading the byte that this function
 * stopped on using the input filter chain.
 * 
 * The input overlay (pdo), speculation buffer (psb), and temporary
 * buffer (pt) are not actually used to pass parameters or return
 * values.  Rather, they are passed in for efficiency reasons so that
 * they do not need to be continually allocated.  The caller should
 * initialize each of these before the first call to this function, then
 * just keep passing them into the function, and reset them
 * appropriately and release them once done.
 * 
 * If the provided block reader is in an error state upon entry to this
 * function, this function fails immediately.  Otherwise, if this
 * function fails, it will set an appropriate error status in the
 * provided block reader.
 * 
 * Parameters:
 * 
 *   pb - the block reader object
 * 
 *   pdo - the decoding map overlay
 * 
 *   psb - the speculation buffer
 * 
 *   pt - the temporary buffer
 * 
 *   ps - the input filter chain
 * 
 *   sp - the string type parameters
 * 
 * Return:
 * 
 *   non-zero if successful, zero if failure
 */
static int shasm_block_decode_entities(
    SHASM_BLOCK *pb,
    SHASM_BLOCK_DOVER *pdo,
    SHASM_BLOCK_SPECBUF *psb,
    SHASM_BLOCK_TBUF *pt,
    SHASM_IFLSTATE *ps,
    const SHASM_BLOCK_STRING *sp) {
  /* @@TODO: */
  return 1;
}

/*
 * Implementation of a numeric escape callback that returns no escapes.
 * 
 * This function merely verifies that entity is non-negative and then
 * returns zero.  It is used when the client passes a NULL function
 * pointer for the numeric escape, indicating no numeric escapes are
 * needed.
 * 
 * Parameters:
 * 
 *   pCustom - (ignored)
 * 
 *   entity - (ignored; must not be negative)
 * 
 *   pParam - (ignored)
 * 
 * Return:
 * 
 *   zero, indicating no escape
 */
static int shasm_block_noesc(
    void *pCustom,
    long entity,
    SHASM_BLOCK_NUMESCAPE *pParam) {
  
  /* Ignore parameters */
  (void) pCustom;
  (void) pParam;
  
  /* Check entity */
  if (entity < 0) {
    abort();
  }
  
  /* Return zero */
  return 0;
}

/*
 * Implementation of an encoding map callback that always returns empty
 * keys.
 * 
 * This function merely verifies that entity is non-negative and then
 * returns zero.  It is used when the client passes a NULL function
 * pointer for the encoding map, indicating there are no keys in the
 * encoding map.
 * 
 * Parameters:
 * 
 *   pCustom - (ignored)
 * 
 *   entity - (ignored; must not be negative)
 * 
 *   pBuf - (ignored)
 * 
 *   buf_len - (ignored)
 * 
 * Return:
 * 
 *   zero, indicating no output sequence
 */
static long shasm_block_nomap(
    void *pCustom,
    long entity,
    unsigned char *pBuf,
    long buf_len) {
  
  /* Ignore parameters */
  (void) pCustom;
  (void) pBuf;
  (void) buf_len;
  
  /* Check entity */
  if (entity < 0) {
    abort();
  }
  
  /* Return zero */
  return 0;
}

/*
 * Initialize a surrogate buffer.
 * 
 * This function may also be used to reset a surrogate buffer back to
 * its initial state.  Surrogate buffers do not need to be deinitialized
 * or cleaned up in any way before releasing.
 * 
 * Parameters:
 * 
 *   psb - the surrogate buffer
 */
static void shasm_block_surbuf_init(SHASM_BLOCK_SURBUF *psb) {
  /* Check parameter */
  if (psb == NULL) {
    abort();
  }
  
  /* Initialize */
  memset(psb, 0, sizeof(SHASM_BLOCK_SURBUF));
  psb->buf = 0;
}

/*
 * Process a Unicode codepoint through a surrogate buffer.
 * 
 * The provided codepoint v must be in range zero up to and including
 * SHASM_BLOCK_MAXCODE.
 * 
 * If the surrogate buffer is already in an error state when this
 * function is called, this function will return -1 indicating an error.
 * 
 * Otherwise, processing depends on whether the surrogate buffer has a
 * high surrogate buffered or whether it is empty.
 * 
 * If the surrogate buffer is empty, then the function will return a
 * non-surrogate codepoint as-is.  A high surrogate will be buffered and
 * -2 will be returned indicating that no codepoint should be sent
 * further yet.  A low surrogate will cause the surrogate buffer to
 * change to error state and -1 will be returned, indicating an
 * improperly paired surrogate.
 * 
 * If the surrogate buffer has a high surrogate buffered, then the
 * function will combine a low surrogate with the high surrogate to
 * form a supplemental codepoint, return the supplemental codepoint, and
 * clear the buffer.  If the surrogate buffer encounters a high
 * surrogate or a non-surrogate when a high surrogate is buffered, the
 * surrogate buffer will transition to error state and -1 will be
 * returned, indicating an improperly paired surrogate.
 * 
 * Parameters:
 * 
 *   psb - the surrogate buffer
 * 
 *   v - the codepoint to process
 * 
 * Return:
 * 
 *   a fully processed codepoint, or -1 indicating an improperly paired
 *   surrogate, or -2 indicating that there is no fully processed
 *   codepoint to report
 */
static long shasm_block_surbuf_process(
    SHASM_BLOCK_SURBUF *psb,
    long v) {
  
  int status = 1;
  long result = 0;
  
  /* Check parameters */
  if ((psb == NULL) || (v < 0) || (v > SHASM_BLOCK_MAXCODE)) {
    abort();
  }
  
  /* Fail immediately if surrogate buffer in error state */
  if (psb->buf < 0) {
    status = 0;
  }
  
  /* Handle the different buffer states */
  if (status && (psb->buf == 0)) {
    /* Surrogate buffer is empty -- handle different codepoints */
    if ((v >= SHASM_BLOCK_HISURROGATE) &&
          (v < SHASM_BLOCK_LOSURROGATE)) {
      /* When surrogate buffer empty, buffer a high surrogate */
      psb->buf = v;
      result = -2;
      
    } else if ((v >= SHASM_BLOCK_LOSURROGATE) &&
                (v <= SHASM_BLOCK_MAXSURROGATE)) {
      /* When surrogate buffer empty, low surrogate is an error */
      status = 0;
    
    } else {
      /* When surrogate buffer empty, return non-surrogate as-is */
      result = v;
    }
    
  } else if (status) {
    /* Surrogate buffer has a high surrogate buffered -- handle
     * different codepoints */
    if ((v >= SHASM_BLOCK_HISURROGATE) &&
          (v < SHASM_BLOCK_LOSURROGATE)) {
      /* When surrogate buffer has a high surrogate, another high
       * surrogate is an error */
      status = 0;
      
    } else if ((v >= SHASM_BLOCK_LOSURROGATE) &&
                (v <= SHASM_BLOCK_MAXSURROGATE)) {
      /* Surrogate buffer has high surrogate and now a low surrogate is
       * provided -- decode into a supplemental codepoint, return that,
       * and clear the buffer */
      result = (psb->buf - SHASM_BLOCK_HISURROGATE) << 10;
      result |= (v - SHASM_BLOCK_LOSURROGATE);
      psb->buf = 0;
      
    } else {
      /* When surrogate buffer has a high surrogate, a non-surrogate is
       * an error */
      status = 0;
    }
  }
  
  /* If error, set error status in buffer and set result to -1 */
  if (!status) {
    psb->buf = -1;
    result = -1;
  }
  
  /* Return result */
  return result;
}

/*
 * Verify that the surrogate buffer is in an appropriate final state.
 * 
 * If the surrogate buffer is in an error state, then this function
 * returns zero indicating the surrogate buffer is not in an acceptable
 * final state.
 * 
 * Otherwise, if the surrogate buffer is empty, then this function
 * returns non-zero indicating the surrogate buffer is in an acceptable
 * final state.  If the surrogate buffer has a high surrogate buffered,
 * the buffer transitions to an error state and zero is returned,
 * indicating an improperly paired surrogate.
 * 
 * After a successful return, the surrogate buffer will be in its
 * initial state, and it can be reused again.
 * 
 * Parameters:
 * 
 *   psb - the surrogate buffer to check
 * 
 * Return:
 * 
 *   non-zero if surrogate buffer is in an acceptable final state, zero
 *   if not
 */
static int shasm_block_surbuf_finish(SHASM_BLOCK_SURBUF *psb) {
  
  int status = 1;
  
  /* Check parameter */
  if (psb == NULL) {
    abort();
  }
  
  /* Fail immediately if surrogate buffer in error state */
  if (psb->buf < 0) {
    status = 0;
  }
  
  /* Fail, setting error state, if buffer has a high surrogate
   * buffered */
  if (status && (psb->buf != 0)) {
    psb->buf = -1;
    status = 0;
  }
  
  /* Return status */
  return status;
}

/*
 * Decode an extended UTF-8 codepoint from input, if possible.
 * 
 * This function only reads UTF-8 for codepoints in range 0x80 up to and
 * including 0x10ffff.  It will return surrogates as-is, without pairing
 * surrogates together to get the supplemental code points.
 * 
 * This function begins by reading a byte from the input filter stack.
 * If this results in an EOF or IOERR, then the function fails on that
 * error condition.  Otherwise, if the byte read has its most
 * significant bit clear, the function unreads the byte and returns zero
 * indicating that there is no extended UTF-8 codepoint to read.
 * 
 * If the function successfully reads a byte with its most significant
 * bit set, then function will either successfully read a UTF-8
 * codepoint and return it, or it will return an error.  This function
 * checks for overlong encodings and treats them as errors.
 * 
 * The return value is either zero, indicating that no extended UTF-8
 * codepoint was present to read, or a value greater than or equal to
 * 0x80, indicating an extended UTF-8 codepoint was read, or the value
 * SHASM_INPUT_EOF or SHASM_INPUT_IOERR if EOF or IOERR was encountered
 * while reading, or SHASM_INPUT_INVALID if the UTF-8 code was invalid.
 * 
 * Parameters:
 * 
 *   ps - the input filter stack to read from
 * 
 * Return:
 * 
 *   zero if no extended UTF-8 present, greater than zero for an
 *   extended UTF-8 codepoint that was read, SHASM_INPUT_EOF for EOF
 *   encountered, SHASM_INPUT_IOERR if I/O error, or SHASM_INPUT_INVALID
 *   if the UTF-8 was invalid
 */
static long shasm_block_read_utf8(SHASM_IFLSTATE *ps) {
  /* @@TODO: */
  abort();
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
  pb->buf_cap = SHASM_BLOCK_MINBUFFER;
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
        shasm_block_seterr(pb, ps, SHASM_ERR_HUGEBLOCK);
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
        shasm_block_seterr(pb, ps, SHASM_ERR_HUGEBLOCK);
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
          shasm_block_seterr(pb, ps, SHASM_ERR_HUGEBLOCK);
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
        shasm_block_seterr(pb, ps, SHASM_ERR_HUGEBLOCK);
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
  
  int status = 1;
  int c = 0;
  int ct = 0;
  SHASM_BLOCK_STRING sparam;
  SHASM_BLOCK_DOVER dover;
  SHASM_BLOCK_SPECBUF specbuf;
  SHASM_BLOCK_TBUF tbuf;
  
  /* Initialize structures -- dover not initialized properly yet */
  memset(&sparam, 0, sizeof(SHASM_BLOCK_STRING));
  memset(&dover, 0, sizeof(SHASM_BLOCK_DOVER));
  shasm_block_specbuf_init(&specbuf);
  shasm_block_tbuf_init(&tbuf);
  
  /* Check parameters */
  if ((pb == NULL) || (ps == NULL) || (sp == NULL)) {
    abort();
  }
  
  /* Copy string parameters over to new structure, checking for valid
   * values as copying, and filling in allowable NULL function pointers
   * with references to local functions */
  if ((sp->stype == SHASM_BLOCK_STYPE_DQUOTE) ||
      (sp->stype == SHASM_BLOCK_STYPE_SQUOTE) ||
      (sp->stype == SHASM_BLOCK_STYPE_CURLY)) {
    sparam.stype = sp->stype;
  } else {
    abort();
  }
  
  if ((sp->dec.fpReset != NULL) &&
      (sp->dec.fpBranch != NULL) &&
      (sp->dec.fpEntity != NULL)) {
    sparam.dec.pCustom = sp->dec.pCustom;
    sparam.dec.fpReset = sp->dec.fpReset;
    sparam.dec.fpBranch = sp->dec.fpBranch;
    sparam.dec.fpEntity = sp->dec.fpEntity;
  } else {
    abort();
  }
  
  if ((sp->i_over == SHASM_BLOCK_IMODE_NONE) ||
      (sp->i_over == SHASM_BLOCK_IMODE_UTF8)) {
    sparam.i_over = sp->i_over;
  } else {
    abort();
  }
  
  if (sp->elist.fpEscQuery != NULL) {
    sparam.elist.pCustom = sp->elist.pCustom;
    sparam.elist.fpEscQuery = sp->elist.fpEscQuery;
  } else {
    sparam.elist.pCustom = NULL;
    sparam.elist.fpEscQuery = &shasm_block_noesc;
  }
  
  if (sp->enc.fpMap != NULL) {
    sparam.enc.pCustom = sp->enc.pCustom;
    sparam.enc.fpMap = sp->enc.fpMap;
  } else {
    sparam.enc.pCustom = NULL;
    sparam.enc.fpMap = &shasm_block_nomap;
  }
  
  if ((sp->o_over == SHASM_BLOCK_OMODE_NONE) ||
      (sp->o_over == SHASM_BLOCK_OMODE_UTF8) ||
      (sp->o_over == SHASM_BLOCK_OMODE_CESU8) ||
      (sp->o_over == SHASM_BLOCK_OMODE_U16LE) ||
      (sp->o_over == SHASM_BLOCK_OMODE_U16BE) ||
      (sp->o_over == SHASM_BLOCK_OMODE_U32LE) ||
      (sp->o_over == SHASM_BLOCK_OMODE_U32BE)) {
    sparam.o_over = sp->o_over;
  } else {
    abort();
  }
  
  if (sp->o_strict) {
    sparam.o_strict = 1;
  } else {
    sparam.o_strict = 0;
  }
  
  /* Initialize dover with the string parameters */
  shasm_block_dover_init(&dover, &sparam);
  
  /* Fail immediately if block reader in error status */
  if (pb->code != SHASM_OKAY) {
    status = 0;
  }

  /* Set the line number to the current position and clear the block
   * reader's buffer to empty */
  if (status) {
    pb->line = shasm_input_count(ps);
    shasm_block_clear(pb);
  }
  
  /* Begin the decoding loop */
  while (status) {
    
    /* Handle zero or more normal entities */
    if (!shasm_block_decode_entities(
            pb, &dover, &specbuf, &tbuf, ps, &sparam)) {
      status = 0;
    }
    
    /* Read the character that normal entity decoding stopped on */
    if (status) {
      c = shasm_input_get(ps);
      if (c == SHASM_INPUT_EOF) {
        status = 0;
        shasm_block_seterr(pb, ps, SHASM_ERR_EOF);
      } else if (c == SHASM_INPUT_IOERR) {
        status = 0;
        shasm_block_seterr(pb, ps, SHASM_ERR_IO);
      }
    }
    
    /* If the character has its most significant bit set and an input
     * override is active, then unread the character and decode a
     * sequence of entities according to the input override; else the
     * character must be a terminal */
    if (status && (c >= 0x80) &&
          (sparam.i_over != SHASM_BLOCK_IMODE_NONE)) {
      /* Use input override -- but unread character that was just read
       * before activating override */
      shasm_input_back(ps);
      
      /* Currently only UTF-8 input override is supported */
      if (sparam.i_over != SHASM_BLOCK_IMODE_UTF8) {
        abort();
      }
      
      /* @@TODO: read a sequence of UTF-8 from input and send entities
       * to encoder */
      abort();
    
    } else {
      /* Not an input override, so this must be a terminal -- set ct to
       * the terminal character that is appropriate for this type of
       * string */
      if (sparam.stype == SHASM_BLOCK_STYPE_DQUOTE) {
        /* Double quote string -- terminal should be double quote */
        ct = SHASM_ASCII_DQUOTE;
        
      } else if (sparam.stype == SHASM_BLOCK_STYPE_SQUOTE) {
        /* Single quote string -- terminal should be single quote */
        ct = SHASM_ASCII_SQUOTE;
        
      } else if (sparam.stype == SHASM_BLOCK_STYPE_CURLY) {
        /* Curly string -- terminal should be right curly */
        ct = SHASM_ASCII_RCURL;
        
      } else {
        /* Unsupported string type */
        abort();
      }
      
      /* Make sure character that was read matches ct, raising
       * SHASM_ERR_STRCHAR if it does not */
      if (c != ct) {
        status = 0;
        shasm_block_seterr(pb, ps, SHASM_ERR_STRCHAR);
      }
      
      /* Terminal read, so break out of decoding loop */
      if (status) {
        break;
      }
    }
  }
  
  /* Reset specbuf and tbuf to free any additional memory */
  shasm_block_specbuf_reset(&specbuf);
  shasm_block_tbuf_reset(&tbuf);
  
  /* Return status */
  return status;
}
