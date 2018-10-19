/*
 * shastina.c
 */

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 
 * Error constants.
 * 
 * These must all be negative.
 */
#define SNERR_IOERR     (-1)  /* I/O error */
#define SNERR_EOF       (-2)  /* End Of File */
#define SNERR_BADSIG    (-3)  /* Unrecognized file signature */
#define SNERR_OPENSTR   (-4)  /* File ends in middle of string */
#define SNERR_LONGSTR   (-5)  /* String is too long */
#define SNERR_NULLCHR   (-6)  /* Null character encountered in string */
#define SNERR_DEEPCURLY (-7)  /* Too much curly nesting in string */

/*
 * ASCII constants.
 */
#define ASCII_HT        (0x09)  /* Horizontal Tab */
#define ASCII_LF        (0x0a)  /* Line Feed */
#define ASCII_CR        (0x0d)  /* Carriage Return */
#define ASCII_SP        (0x20)  /* Space */
#define ASCII_DQUOTE    (0x22)  /* " */
#define ASCII_POUNDSIGN (0x23)  /* # */
#define ASCII_PERCENT   (0x25)  /* % */
#define ASCII_LPAREN    (0x28)  /* ( */
#define ASCII_RPAREN    (0x29)  /* ) */
#define ASCII_COMMA     (0x2c)  /* , */
#define ASCII_SEMICOLON (0x3b)  /* ; */
#define ASCII_LSQR      (0x5b)  /* [ */
#define ASCII_BACKSLASH (0x5c)  /* \ */
#define ASCII_RSQR      (0x5d)  /* ] */
#define ASCII_GRACCENT  (0x60)  /* ` */
#define ASCII_LCURL     (0x7b)  /* { */
#define ASCII_BAR       (0x7c)  /* | */
#define ASCII_RCURL     (0x7d)  /* } */

/* Visible printing characters */
#define ASCII_VISIBLE_MIN (0x21)
#define ASCII_VISIBLE_MAX (0x7e)

/*
 * The bytes of the UTF-8 Byte Order Mark (BOM).
 */
#define SNFILTER_BOM_1 (0xef)
#define SNFILTER_BOM_2 (0xbb)
#define SNFILTER_BOM_3 (0xbf)

/*
 * Structure for storing state of Shastina string buffers.
 * 
 * Use the snbuffer_ functions to manipulate this structure.
 */
typedef struct {
  
  /*
   * Pointer to the buffer.
   * 
   * The string data is null-terminated.
   * 
   * This pointer is NULL if cap is zero.
   */
  char *pBuf;
  
  /*
   * The number of characters (not including terminating null) stored in
   * the buffer.
   * 
   * This may not exceed cap.
   */
  long count;
  
  /*
   * The current capacity of the buffer in characters.
   * 
   * This includes space for the terminating null.
   * 
   * If zero, it means that no buffer has been allocated yet.
   */
  long cap;
  
  /*
   * The initial allocation capacity for this buffer in characters.
   * 
   * This must be greater than zero and no greater than maxcap.
   */
  long initcap;
  
  /*
   * The maximum capacity for this buffer in characters.
   * 
   * This must be greater than or equal to initcap.  It may not exceed
   * (LONG_MAX / 2).
   */
  long maxcap;
  
} SNBUFFER;

/*
 * Structure for storing state of the Shastina input filter.
 * 
 * Use the snfilter_ functions to manipulate this structure.
 */
typedef struct {
  
  /*
   * The line number of the character that was most recently read, or
   * zero if no characters have been read yet.
   * 
   * Line numbers change *after* the line feed, so the line_count of an
   * LF character will be equal to the line it ends.  The next character
   * after the LF will then have an incremented line_count.
   * 
   * The first line is line one.  A value of LONG_MAX means the counter
   * has overflowed.
   */
  long line_count;
  
  /*
   * The character most recently read.
   * 
   * This is the unsigned value of the character (0-255).  It is only
   * valid if line_count is greater than zero.  Otherwise, no characters
   * have been read yet and this field is ignored.
   * 
   * The special values of SNERR_EOF and SNERR_IOERR mean End Of File or
   * I/O error have been encountered, respectively.
   * 
   * SNERR_BADSIG means that the first character or the first two 
   * characters of the file matched a UTF-8 Byte Order Mark (BOM), but
   * a complete BOM could not be read.
   */
  int c;
  
  /*
   * The pushback flag.
   * 
   * If zero, then pushback mode is not active.
   * 
   * If non-zero, it means pushback mode is active.  line_count must be
   * greater than zero in this case, and c can not be EOF, IOERR, or
   * BADSIG.  The next read operation will read the buffered character
   * rather than the next character.
   */
  int pushback;
  
  /*
   * The Byte Order Mark (BOM) present flag.
   * 
   * If set, this means a UTF-8 Byte Order Mark (BOM) was present at the
   * start of the file (and was filtered out).
   */
  int bom_present;
  
} SNFILTER;

/* Function prototypes */
static void snbuffer_init(SNBUFFER *pBuffer, long icap, long maxcap);
static void snbuffer_reset(SNBUFFER *pBuffer, int full);
static int snbuffer_append(SNBUFFER *pBuffer, int c);
static char *snbuffer_get(SNBUFFER *pBuffer);
static long snbuffer_count(SNBUFFER *pBuffer);

static void snfilter_reset(SNFILTER *pFilter);
static int snfilter_read(SNFILTER *pFilter, FILE *pIn);
static long snfilter_count(SNFILTER *pFilter);
static int snfilter_bomflag(SNFILTER *pFilter);
static int snfilter_pushback(SNFILTER *pFilter);

static int snchar_islegal(int c);
static int snchar_isatomic(int c);
static int snchar_isinclusive(int c);
static int snchar_isexclusive(int c);

static int snstr_readQuoted(
    SNBUFFER * pBuffer,
    FILE     * pIn,
    SNFILTER * pFilter);

static int snstr_readCurlied(
    SNBUFFER * pBuffer,
    FILE     * pIn,
    SNFILTER * pFilter);

static void sntk_skip(FILE *pIn, SNFILTER *pFilter);

/*
 * Initialize a string buffer.
 * 
 * String buffers must be initialized before they are used, or undefined
 * behavior occurs.  A full reset must be performed on the string buffer
 * using snbuffer_reset() before the structure is released or a memory
 * leak may occur.
 * 
 * icap is the initial allocation capacity in characters.  maxcap is the
 * maximum capacity for the buffer.
 * 
 * icap must be greater than zero, maxcap must be greater than or equal
 * to icap, and maxcap must be no greater than (LONG_MAX / 2) or a fault
 * will occur.
 * 
 * If (sizeof(size_t) < 2), this function will always fault.  Otherwise,
 * if (sizeof(size_t) < 4), this function will fault if maxcap exceeds
 * 65535.  Otherwise, (sizeof(size_t) >= 4) and this function will fault
 * if maxcap exceeds 2147483647.  This is to prevent memory allocation
 * problems.
 * 
 * Do not initialize a string buffer that is already initialized, or a
 * memory leak may occur.
 * 
 * Parameters:
 * 
 *   pBuffer - pointer to the buffer to initialize
 * 
 *   icap - the initial allocation capacity
 * 
 *   maxcap - the maximum allocation capacity
 */
static void snbuffer_init(SNBUFFER *pBuffer, long icap, long maxcap) {
  
  /* Check parameters */
  if (pBuffer == NULL) {
    abort();
  }
  
  if ((icap <= 0) || (maxcap < icap) || (maxcap > (LONG_MAX / 2))) {
    abort();
  }
  
  if (sizeof(size_t) < 2) {
    abort();
  } else if (sizeof(size_t) < 4) {
    if (maxcap > 65535L) {
      abort();
    }
  } else {
    if (maxcap > 2147483647L) {
      abort();
    }
  }
  
  /* Initialize structure */
  memset(pBuffer, 0, sizeof(SNBUFFER));
  pBuffer->pBuf = NULL;
  pBuffer->count = 0;
  pBuffer->cap = 0;
  pBuffer->initcap = icap;
  pBuffer->maxcap = maxcap;
}

/*
 * Reset a string buffer back to empty.
 * 
 * The string buffer must already have been initialized with
 * snbuffer_init() or undefined behavior occurs.
 * 
 * full is non-zero to perform a full reset, zero to perform a fast
 * reset.  A fast reset just clears the buffer to empty without
 * releasing the allocated memory buffer, allowing it to be reused.  A
 * full reset also releases the allocated memory buffer, clearing the
 * structure all the way back to its initial state.
 * 
 * A full reset must be performed on all string buffers before they are
 * released, or a memory leak may occur.
 * 
 * Parameters:
 * 
 *   pBuffer - the string buffer to reset
 * 
 *   full - non-zero for a full reset, zero for a partial reset
 */
static void snbuffer_reset(SNBUFFER *pBuffer, int full) {
  
  /* Check parameters */
  if (pBuffer == NULL) {
    abort();
  }
  
  /* Set the count back to zero */
  pBuffer->count = 0;
  
  /* If a buffer is allocated, clear it all to zero */
  if (pBuffer->cap > 0) {
    memset(pBuffer->pBuf, 0, (size_t) pBuffer->cap);
  }
  
  /* If we're doing a full reset, release the buffer if allocated and
   * reset capacity back to zero */
  if (full && (pBuffer->cap > 0)) {
    free(pBuffer->pBuf);
    pBuffer->cap = 0;
  }
}

/*
 * Append a character to a string buffer.
 * 
 * The character c may be any unsigned byte value except for zero.  That
 * is, the range is 1-255.
 * 
 * The function fails if there is no more capacity left for another
 * character.  The buffer is unmodified in this case.
 * 
 * Parameters:
 * 
 *   pBuffer - the string buffer to add a character to
 * 
 *   c - the unsigned byte value to add
 * 
 * Return:
 * 
 *   non-zero if successful, zero if no more capacity
 */
static int snbuffer_append(SNBUFFER *pBuffer, int c) {
  
  int status = 1;
  long newcap = 0;
  
  /* Check parameters */
  if ((pBuffer == NULL) || (c < 1) || (c > 255)) {
    abort();
  }
  
  /* Proceed only if we are not completely maxed out of capacity */
  if (pBuffer->count < (pBuffer->maxcap - 1)) {
    /* We have capacity left; first, make the initial allocation if we
     * haven't allocated a memory buffer yet */
    if (pBuffer->cap < 1) {
      pBuffer->pBuf = (char *) malloc((size_t) pBuffer->initcap);
      if (pBuffer->pBuf == NULL) {
        abort();
      }
      memset(pBuffer->pBuf, 0, (size_t) pBuffer->initcap);
      pBuffer->cap = pBuffer->initcap;
    }
    
    /* Next, increase allocated memory buffer if we need more space */
    if (pBuffer->count >= (pBuffer->cap - 1)) {
      /* New capacity should usually be double current capacity */
      newcap = pBuffer->cap * 2;
      
      /* If new capacity exceeds max capacity, set to max capacity */
      if (newcap > pBuffer->maxcap) {
        newcap = pBuffer->maxcap;
      }
      
      /* Allocate new buffer */
      pBuffer->pBuf = (char *) realloc(pBuffer->pBuf, (size_t) newcap);
      if (pBuffer->pBuf == NULL) {
        abort();
      }
      
      /* Initialize new space to zero */
      memset((pBuffer->pBuf + pBuffer->cap),
              0,
              (size_t) (newcap - pBuffer->cap));
      
      /* Update capacity */
      pBuffer->cap = newcap;
    }
    
    /* Add the new character */
    (pBuffer->pBuf)[pBuffer->count] = (char) c;
    (pBuffer->count)++;
    
  } else {
    /* Out of capacity */
    status = 0;
  }
  
  /* Return status */
  return status;
}

/*
 * Get a pointer to the current string stored in the buffer.
 * 
 * The string will be null-terminated.  The returned pointer remains
 * valid until another character is appended to the buffer or the buffer
 * is reset.
 * 
 * Clients should not modify the data pointed to, or undefined behavior
 * occurs.
 * 
 * Parameters:
 * 
 *   pBuffer - the string buffer to query
 * 
 * Return:
 * 
 *   the current buffered string
 */
static char *snbuffer_get(SNBUFFER *pBuffer) {
  
  /* Check parameter */
  if (pBuffer == NULL) {
    abort();
  }
  
  /* If we haven't made the initial allocation yet, do it */
  if (pBuffer->cap < 1) {
    pBuffer->pBuf = (char *) malloc((size_t) pBuffer->initcap);
    if (pBuffer->pBuf == NULL) {
      abort();
    }
    memset(pBuffer->pBuf, 0, (size_t) pBuffer->initcap);
    pBuffer->cap = pBuffer->initcap;
  }
  
  /* Return the pointer */
  return pBuffer->pBuf;
}

/*
 * Return the number of characters currently buffered.
 * 
 * This does not include the terminating null.
 * 
 * Parameters:
 * 
 *   pBuffer - the buffer to query
 * 
 * Return:
 * 
 *   the number of buffered characters
 */
static long snbuffer_count(SNBUFFER *pBuffer) {
  
  /* Check parameter */
  if (pBuffer == NULL) {
    abort();
  }
  
  /* Return the count */
  return pBuffer->count;
}

/*
 * Reset an input filter back to its original state.
 * 
 * This should also be used for initializing filter structures.
 * 
 * Parameters:
 * 
 *   pFilter - the filter to reset
 */
static void snfilter_reset(SNFILTER *pFilter) {
  
  /* Check parameter */
  if (pFilter == NULL) {
    abort();
  }
  
  /* Clear structure */
  memset(pFilter, 0, sizeof(SNFILTER));
  
  /* Set initial state */
  pFilter->line_count = 0;
  pFilter->c = 0;
  pFilter->pushback = 0;
  pFilter->bom_present = 0;
}

/*
 * Read the next character through the input filter.
 * 
 * The next character is returned as an unsigned byte value (0-255).
 * 
 * SNERR_EOF is returned if End Of File has been reached.
 * 
 * SNERR_IOERR is returned if an I/O error has been encountered.
 * 
 * SNERR_BADSIG is returned if one or two characters corresponding to 
 * the start of a UTF-8 Byte Order Mark (BOM) were read at the start of
 * the file, but the complete UTF-8 BOM could not be read.  The filter
 * will not continue reading the file in this case.
 * 
 * The provided input filter state must be properly initialized.  The
 * passed file must be open for reading.  It will be read sequentially.
 * 
 * Parameters:
 * 
 *   pFilter - the input filter state
 * 
 *   pIn - the file to read from
 * 
 * Return:
 * 
 *   the next unsigned byte value, or SNERR_EOF, SNERR_IOERR, or
 *   SNERR_BADSIG
 */
static int snfilter_read(SNFILTER *pFilter, FILE *pIn) {
  
  int err_num = 0;
  int c = 0;
  int c2 = 0;
  
  /* Check parameters */
  if ((pFilter == NULL) || (pIn == NULL)) {
    abort();
  }
  
  /* If line_count is zero, this is the first time we're reading from
   * the file, so check for a UTF-8 BOM and filter it out (setting the
   * bom_present flag) if present */
  if ((!err_num) && (pFilter->line_count == 0)) {
    
    /* Read the first character */
    c = getc(pIn);
    if (c == EOF) {
      if (feof(pIn)) {
        err_num = SNERR_EOF;
      } else {
        err_num = SNERR_IOERR;
      }
    }
    
    /* If the first character is the first character of a UTF-8 BOM,
     * read and filter out the BOM; else, unread it and proceed */
    if ((!err_num) && (c == SNFILTER_BOM_1)) {
      /* We have a UTF-8 BOM; read the second character and confirm it's
       * present and part of the BOM */
      c = getc(pIn);
      if (c == EOF) {
        if (feof(pIn)) {
          /* File ends before complete UTF-8 BOM */
          err_num = SNERR_BADSIG;
        } else {
          /* I/O error */
          err_num = SNERR_IOERR;
        }
      }
      if ((!err_num) && (c != SNFILTER_BOM_2)) {
        err_num = SNERR_BADSIG;
      }
      
      /* Read the third character and confirm it's present and part of
       * the BOM */
      if (!err_num) {
        c = getc(pIn);
        if (c == EOF) {
          if (feof(pIn)) {
            /* File ends before complete UTF-8 BOM */
            err_num = SNERR_BADSIG;
          } else {
            /* I/O error */
            err_num = SNERR_IOERR;
          }
        }
        if ((!err_num) && (c != SNFILTER_BOM_3)) {
          err_num = SNERR_BADSIG;
        }
      }
      
      /* If we got here successfully, set the BOM flag */
      if (!err_num) {
        pFilter->bom_present = 1;
      }
      
    } else if (!err_num) {
      /* First character not part of a UTF-8 BOM; unread it */
      if (ungetc(c, pIn) == EOF) {
        err_num = SNERR_IOERR;
      }
    }
  }
  
  /* If we're not in pushback mode and we don't have a special
   * condition, we need to read another character */
  if ((!err_num) && (!(pFilter->pushback)) &&
        ((pFilter->line_count == 0) || (pFilter->c >= 0))) {
    
    /* Read a character */
    c = getc(pIn);
    if (c == EOF) {
      if (feof(pIn)) {
        err_num = SNERR_EOF;
      } else {
        err_num = SNERR_IOERR;
      }
    }
    
    /* If we read a CR or a LF, see if we can pair it with the next
     * character to form a CR+LF or LF+CR combination */
    if ((!err_num) && ((c == ASCII_CR) || (c == ASCII_LF))) {
      
      /* Read the next character */
      c2 = getc(pIn);
      if (c2 == EOF) {
        if (feof(pIn)) {
          /* End Of File -- can't pair, so set c2 to special value of
           * -1 */
          c2 = -1;
        } else {
          /* I/O error */
          err_num = SNERR_IOERR;
        }
      }
      
      /* If there is no CR+LF or LF+CR pair, then unread the character
       * we just read -- unless it was EOF */
      if ((!err_num) &&
            (((c == ASCII_LF) && (c2 != ASCII_CR)) ||
              (c == ASCII_CR) && (c2 != ASCII_LF))) {
        if (c2 != -1) {
          if (ungetc(c2, pIn) == EOF) {
            err_num = SNERR_IOERR;
          }
        }
      }
    }
    
    /* Convert CR characters to LF */
    if ((!err_num) && (c == ASCII_CR)) {
      c = ASCII_LF;
    }
    
    /* Update state of filter structure */
    if (!err_num) {
      if (pFilter->line_count == 0) {
        /* Very first character -- set line count to one */
        pFilter->c = c;
        pFilter->line_count = 1;
        
      } else {
        /* Not the very first character -- increase line count by one if
         * the previous character was LF and the line count is not at
         * the overflow value of LONG_MAX */
        if ((c == ASCII_LF) && (pFilter->line_count < LONG_MAX)) {
          (pFilter->line_count)++;
        }
        pFilter->c = c;
      }
    }
  }
  
  /* Clear the pushback flag if it is set -- even if there has been an
   * error or EOF condition */
  pFilter->pushback = 0;
  
  /* If we've encountered an error (or EOF), write it into the
   * structure */
  if (err_num) {
    pFilter->c = err_num;
  }
  
  /* Return the character or special condition */
  return (pFilter->c);
}

/*
 * Return the current line count.
 * 
 * The line count is always at least one and at most LONG_MAX.  The
 * value of LONG_MAX is an overflow value, so any count of lines above
 * that will just remain at LONG_MAX.
 * 
 * The line count is affected by pushback mode, changing backwards if
 * characters are pushed back before a line break.
 * 
 * Parameters:
 * 
 *   pFilter - the input filter state
 * 
 * Return:
 * 
 *   the current line count
 */
static long snfilter_count(SNFILTER *pFilter) {
  
  long lc = 0;
  
  /* Check parameter */
  if (pFilter == NULL) {
    abort();
  }
  
  /* Read current line count */
  lc = pFilter->line_count;
  
  /* If line count is zero, change it to one to indicate the line count
   * at the start of the file */
  if (lc < 1) {
    lc = 1;
  }
  
  /* If we're not in pushback mode, c is LF, we're not at the very
   * beginning of the file, and line_count is less than LONG_MAX,
   * increase the line count by one */
  if ((!(pFilter->pushback)) && (pFilter->line_count > 0) &&
        (pFilter->c == ASCII_LF) && (lc < LONG_MAX)) {
    lc++;
  }
  
  /* Return the adjusted line count */
  return lc;
}

/*
 * Check whether the Byte Order Mark flag is set.
 * 
 * This flag will be set after the first filtered byte is read if a
 * UTF-8 Byte Order Mark (BOM) was filtered out at the very start of the
 * file.  If no BOM was was present, zero will be returned.
 * 
 * This is only meaningful after the first call to snfilter_read.  At
 * the initial state, the BOM flag will always be clear because the
 * start of the file has not been read yet.
 * 
 * Parameter:
 * 
 *   pFilter - the filter state
 * 
 * Return:
 * 
 *   non-zero if BOM flag set, zero if clear
 */
static int snfilter_bomflag(SNFILTER *pFilter) {
  
  /* Check parameter */
  if (pFilter == NULL) {
    abort();
  }
  
  /* Return flag */
  return (pFilter->bom_present);
}

/*
 * Set the pushback flag, so the character that was just read will be
 * read again.
 * 
 * This call is ignored if the filter state is currently in an EOF or
 * error condition.
 * 
 * The call fails if the filter state is already in pushback mode or if
 * no characters have been read yet.
 * 
 * Parameters:
 * 
 *   pFilter - the filter state
 * 
 * Return:
 * 
 *   non-zero if successful, zero if pushback mode could not be set
 */
static int snfilter_pushback(SNFILTER *pFilter) {
  
  int status = 1;
  
  /* Check parameter */
  if (pFilter == NULL) {
    abort();
  }
  
  /* Only proceed if not in special state */
  if ((pFilter->line_count == 0) || (pFilter->c >= 0)) {
    
    /* We can only set the pushback flag if it is not already set and at
     * least one character has been read */
    if ((pFilter->line_count > 0) && (!(pFilter->pushback))) {
      pFilter->pushback = 1;
    } else {
      status = 0;
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Determine whether the given character is legal, outside of string
 * literals and comments.
 * 
 * This range includes all visible, printing ASCII characters, plus
 * Space (SP), Horizontal Tab (HT), and Line Feed (LF).
 * 
 * Parameters:
 * 
 *   c - the character to check
 * 
 * Return:
 * 
 *   non-zero if legal, zero if not
 */
static int snchar_islegal(int c) {
  
  int result = 0;
  
  if (((c >= ASCII_VISIBLE_MIN) && (c <= ASCII_VISIBLE_MAX)) ||
      (c == ASCII_SP) || (c == ASCII_HT) || (c == ASCII_LF)) {
    result = 1;
  } else {
    result = 0;
  }
  
  return result;
}

/*
 * Determine whether the given character is an atomic primitive
 * character.
 * 
 * These characters can stand by themselves as a full token.
 * 
 * Parameters:
 * 
 *   c - the character to check
 * 
 * Return:
 * 
 *   non-zero if atomic, zero if not
 */
static int snchar_isatomic(int c) {
  
  int result = 0;
  
  if ((c == ASCII_LPAREN) || (c == ASCII_RPAREN) ||
      (c == ASCII_LSQR) || (c == ASCII_RSQR) ||
      (c == ASCII_COMMA) || (c == ASCII_PERCENT) ||
      (c == ASCII_SEMICOLON) || (c == ASCII_DQUOTE) ||
      (c == ASCII_GRACCENT) || (c == ASCII_LCURL) ||
      (c == ASCII_RCURL)) {
    result = 1;
  } else {
    result = 0;
  }
  
  return result;
}

/*
 * Determine whether the given character is an inclusive token closer.
 * 
 * Inclusive token closers end the token and are included as the last
 * character of the token.
 * 
 * Parameters:
 * 
 *   c - the character to check
 * 
 * Return:
 * 
 *   non-zero if inclusive, zero if not
 */
static int snchar_isinclusive(int c) {
  
  int result = 0;
  
  if ((c == ASCII_DQUOTE) || (c == ASCII_GRACCENT) ||
      (c == ASCII_LCURL)) {
    result = 1;
  } else {
    result = 0;
  }
  
  return result;
}

/*
 * Determine whether the given character is an exclusive token closer.
 * 
 * Exclusive token closers end the token but are not included as the
 * last character of the token.
 * 
 * Parameters:
 * 
 *   c - the character to check
 * 
 * Return:
 * 
 *   non-zero if exclusive, zero if not
 */
static int snchar_isexclusive(int c) {
  
  int result = 0;
  
  if ((c == ASCII_HT) || (c == ASCII_SP) || (c == ASCII_LF) ||
      (c == ASCII_LPAREN) || (c == ASCII_RPAREN) ||
      (c == ASCII_LSQR) || (c == ASCII_RSQR) ||
      (c == ASCII_COMMA) || (c == ASCII_PERCENT) ||
      (c == ASCII_SEMICOLON) || (c == ASCII_POUNDSIGN) ||
      (c == ASCII_RCURL)) {
    result = 1;
  } else {
    result = 0;
  }
  
  return result;
}

/*
 * Read a quoted string.
 * 
 * pBuffer is the buffer into which the string data will be read.  It
 * must be properly initialized.  This function will reset the buffer
 * and then write the string data into it.
 * 
 * pIn is the file to read data from.  It must be open for read access.
 * Reading is fully sequential.
 * 
 * pFilter is the input filter to read the data through.  It should be
 * in the proper state.
 * 
 * This function assumes that the opening quote as already been read.
 * The first character read is therefore the first character of string
 * data.  The closing quote will be read and consumed by this function.
 * 
 * Parameters:
 * 
 *   pBuffer - the buffer to read the string data into
 * 
 *   pIn - the input file to read the string data from
 * 
 *   pFilter - the input filter
 * 
 * Return:
 * 
 *   zero if successful, or one of the SNERR constants if error
 */
static int snstr_readQuoted(
    SNBUFFER * pBuffer,
    FILE     * pIn,
    SNFILTER * pFilter) {
  
  int err_num = 0;
  int esc_flag = 0;
  int c = 0;
  
  /* Check parameters */
  if ((pBuffer == NULL) || (pIn == NULL) || (pFilter == NULL)) {
    abort();
  }
  
  /* Reset the buffer */
  snbuffer_reset(pBuffer, 0);
  
  /* Read all string data */
  while (!err_num) {
    
    /* Read a character */
    c = snfilter_read(pFilter, pIn);
    if (c < 0) {
      if (c == SNERR_EOF) {
        err_num = SNERR_OPENSTR;
      } else {
        err_num = c;
      }
    }
    
    /* If this character is a double quote and the escape flag is not
     * set, then we are done so leave loop */
    if ((!err_num) && (!esc_flag) && (c == ASCII_DQUOTE)) {
      break;
    }
    
    /* Update escape flag -- set if current character is a backslash,
     * clear otherwise */
    if ((!err_num) && (c == ASCII_BACKSLASH)) {
      esc_flag = 1;
    } else {
      esc_flag = 0;
    }
    
    /* Make sure character is not a null character */
    if ((!err_num) && (c == 0)) {
      err_num = SNERR_NULLCHR;
    }
    
    /* Append character to buffer */
    if (!err_num) {
      if (!snbuffer_append(pBuffer, c)) {
        err_num = SNERR_LONGSTR;
      }
    }
  }
  
  /* Return okay or error code */
  return err_num;
}

/*
 * Read a curly-quoted string.
 * 
 * pBuffer is the buffer into which the string data will be read.  It
 * must be properly initialized.  This function will reset the buffer
 * and then write the string data into it.
 * 
 * pIn is the file to read data from.  It must be open for read access.
 * Reading is fully sequential.
 * 
 * pFilter is the input filter to read the data through.  It should be
 * in the proper state.
 * 
 * This function assumes that the opening curly bracket as already been
 * read.  The first character read is therefore the first character of
 * string data.  The closing curly bracket will be read and consumed by
 * this function.
 * 
 * Parameters:
 * 
 *   pBuffer - the buffer to read the string data into
 * 
 *   pIn - the input file to read the string data from
 * 
 *   pFilter - the input filter
 * 
 * Return:
 * 
 *   zero if successful, or one of the SNERR constants if error
 */
static int snstr_readCurlied(
    SNBUFFER * pBuffer,
    FILE     * pIn,
    SNFILTER * pFilter) {
  
  int err_num = 0;
  int esc_flag = 0;
  long nest_level = 1;
  int c = 0;
  
  /* Check parameters */
  if ((pBuffer == NULL) || (pIn == NULL) || (pFilter == NULL)) {
    abort();
  }
  
  /* Reset the buffer */
  snbuffer_reset(pBuffer, 0);
  
  /* Read all string data */
  while (!err_num) {
    
    /* Read a character */
    c = snfilter_read(pFilter, pIn);
    if (c < 0) {
      if (c == SNERR_EOF) {
        err_num = SNERR_OPENSTR;
      } else {
        err_num = c;
      }
    }
    
    /* If escape flag is not set, update the nesting level if current
     * character is a curly bracket */
    if ((!err_num) && (!esc_flag)) {
      if (c == ASCII_LCURL) {
        
        /* Left curly -- increase nesting level, watching for
         * overflow */
        if (nest_level < LONG_MAX) {
          nest_level++;
        } else {
          err_num = SNERR_DEEPCURLY;
        }
        
      } else if (c == ASCII_RCURL) {
        /* Right curly -- decrease nesting level */
        nest_level--;
      }
    }
    
    /* If nesting level has been brought down to zero, leave loop */
    if ((!err_num) && (nest_level < 1)) {
      break;
    }
    
    /* Update escape flag -- set if current character is a backslash,
     * clear otherwise */
    if ((!err_num) && (c == ASCII_BACKSLASH)) {
      esc_flag = 1;
    } else {
      esc_flag = 0;
    }
    
    /* Make sure character is not a null character */
    if ((!err_num) && (c == 0)) {
      err_num = SNERR_NULLCHR;
    }
    
    /* Append character to buffer */
    if (!err_num) {
      if (!snbuffer_append(pBuffer, c)) {
        err_num = SNERR_LONGSTR;
      }
    }
  }
  
  /* Return okay or error code */
  return err_num;
}

/*
 * Skip over zero or more characters of whitespace and comments.
 * 
 * After this operation, the file and filter will be positioned at the
 * first character that is not whitespace and not part of a comment, or
 * at the first special or error condition that was encountered.
 * 
 * Parameters:
 * 
 *   pIn - the input file
 * 
 *   pFilter - the filter to pass the input through
 */
static void sntk_skip(FILE *pIn, SNFILTER *pFilter) {
  
  int c = 0;
  
  /* Check parameters */
  if ((pIn == NULL) || (pFilter == NULL)) {
    abort();
  }
  
  /* Skip over whitespace and comments */
  while (c >= 0) {
    
    /* Skip over zero or more characters of whitespace */
    for(c = snfilter_read(pFilter, pIn);
        (c == ASCII_SP) || (c == ASCII_HT) || (c == ASCII_LF);
        c = snfilter_read(pFilter, pIn));
    
    /* If we encountered anything except the pound sign, set pushback
     * mode and leave the loop */
    if (c != ASCII_POUNDSIGN) {
      if (!snfilter_pushback(pFilter)) {
        abort();  /* shouldn't happen */
      }
      break;
    }
    
    /* We encountered the start of a comment -- read until we encounter
     * LF or some special condition */
    for(c = snfilter_read(pFilter, pIn);
        (c >= 0) && (c != ASCII_LF);
        c = snfilter_read(pFilter, pIn));
  }
}

/*
 * @@TODO:
 */
int main(int argc, char *argv[]) {
  
  SNFILTER fil;
  int retval = 0;
  
  snfilter_reset(&fil);
  
  sntk_skip(stdin, &fil);
  for(retval = snfilter_read(&fil, stdin);
      retval >= 0;
      retval = snfilter_read(&fil, stdin)) {
    putchar(retval);
  }
  if (retval != SNERR_EOF) {
    fprintf(stderr, "Error %d!\n", retval);
  }
  
  return 0;
}
