/*
 * shastina.c
 */

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 
 * Status constants for the input filter.
 * 
 * These must all be negative.
 */
#define SNFILTER_IOERR  (-1)  /* I/O error */
#define SNFILTER_EOF    (-2)  /* End Of File */
#define SNFILTER_BADSIG (-3)  /* Unrecognized file signature */

/*
 * ASCII constants.
 */
#define ASCII_LF (0x0a)   /* Line Feed */
#define ASCII_CR (0x0d)   /* Carriage Return */

/*
 * The bytes of the UTF-8 Byte Order Mark (BOM).
 */
#define SNFILTER_BOM_1 (0xef)
#define SNFILTER_BOM_2 (0xbb)
#define SNFILTER_BOM_3 (0xbf)

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
   * The special values of SNFILTER_EOF and SNFILTER_IOERR mean End Of
   * File or I/O error have been encountered, respectively.
   * 
   * SNFILTER_BADSIG means that the first character or the first two
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
static void snfilter_reset(SNFILTER *pFilter);
static int snfilter_read(SNFILTER *pFilter, FILE *pIn);

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
 * SNFILTER_EOF is returned if End Of File has been reached.
 * 
 * SNFILTER_IOERR is returned if an I/O error has been encountered.
 * 
 * SNFILTER_BADSIG is returned if one or two characters corresponding to
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
 *   the next unsigned byte value, or SNFILTER_EOF, SNFILTER_IOERR, or
 *   SNFILTER_BADSIG
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
        err_num = SNFILTER_EOF;
      } else {
        err_num = SNFILTER_IOERR;
      }
    }
    
    /* If the first character is the first character of a UTF-8 BOM,
     * read and filter out the BOM; else, unread it and proceed */
    if ((!err_num) && (c == SNFILTER_BOM_1)) {
      /* We have a UTF-8 BOM; begin by setting the flag */
      pFilter->bom_present = 1;
      
      /* Read the second character and confirm it's present and part of
       * the BOM */
      c = getc(pIn);
      if (c == EOF) {
        if (feof(pIn)) {
          /* File ends before complete UTF-8 BOM */
          err_num = SNFILTER_BADSIG;
        } else {
          /* I/O error */
          err_num = SNFILTER_IOERR;
        }
      }
      if ((!err_num) && (c != SNFILTER_BOM_2)) {
        err_num = SNFILTER_BADSIG;
      }
      
      /* Read the third character and confirm it's present and part of
       * the BOM */
      if (!err_num) {
        c = getc(pIn);
        if (c == EOF) {
          if (feof(pIn)) {
            /* File ends before complete UTF-8 BOM */
            err_num = SNFILTER_BADSIG;
          } else {
            /* I/O error */
            err_num = SNFILTER_IOERR;
          }
        }
        if ((!err_num) && (c != SNFILTER_BOM_3)) {
          err_num = SNFILTER_BADSIG;
        }
      }
      
    } else if (!err_num) {
      /* First character not part of a UTF-8 BOM; unread it */
      if (ungetc(c, pIn) == EOF) {
        err_num = SNFILTER_IOERR;
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
        err_num = SNFILTER_EOF;
      } else {
        err_num = SNFILTER_IOERR;
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
          err_num = SNFILTER_IOERR;
        }
      }
      
      /* If there is no CR+LF or LF+CR pair, then unread the character
       * we just read -- unless it was EOF */
      if ((!err_num) &&
            (((c == ASCII_LF) && (c2 != ASCII_CR)) ||
              (c == ASCII_CR) && (c2 != ASCII_LF))) {
        if (c2 != -1) {
          if (ungetc(c2, pIn) == EOF) {
            err_num = SNFILTER_IOERR;
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
        if ((pFilter->c == ASCII_LF) &&
              (pFilter->line_count < LONG_MAX)) {
          (pFilter->line_count)++;
        }
        pFilter->c = c;
      }
    }
  }
  
  /* Clear the pushback flag if it is set */
  if (!err_num) {
    pFilter->pushback = 0;
  }
  
  /* If we've encountered an error, write it into the structure */
  if (err_num) {
    pFilter->c = err_num;
  }
  
  /* Return the character */
  return (pFilter->c);
}

/*
 * @@TODO:
 */
int main(int argc, char *argv[]) {
  
  SNFILTER fil;
  int c = 0;
  
  snfilter_reset(&fil);
  
  for(c = snfilter_read(&fil, stdin);
      c >= 0;
      c = snfilter_read(&fil, stdin)) {
    putchar(c);
  }
  if (c == SNFILTER_IOERR) {
    fprintf(stderr, "I/O error!\n");
  } else if (c == SNFILTER_BADSIG) {
    fprintf(stderr, "Bad signature!\n");
  }
  
  return 0;
}
