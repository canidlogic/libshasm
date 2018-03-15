/*
 * shasm_input.c
 * 
 * Implementation of shasm_input.h
 * 
 * See the header for further information.
 */
#include "shasm_input.h"
#include <stdlib.h>
#include <string.h>

/*
 * Relevant ASCII values.
 */
#define SHASM_ASCII_HT (0x9)    /* Horizontal tab */
#define SHASM_ASCII_LF (0xa)    /* Line feed */
#define SHASM_ASCII_CR (0xd)    /* Carriage return */
#define SHASM_ASCII_SP (0x20)   /* Space */

/*
 * The three unsigned byte values for a UTF-8 Byte Order Mark (BOM).
 */
#define SHASM_BOM_BYTE_1 (0xef)
#define SHASM_BOM_BYTE_2 (0xbb)
#define SHASM_BOM_BYTE_3 (0xbf)

/*
 * SHASM_IFLSTATE structure for storing state information of input
 * filter chain.
 * 
 * Use shasm_iflstate_init to properly initialize this structure.
 * 
 * The prototype for this structure is given in the header.
 */
struct SHASM_IFLSTATE_TAG {
  
  /* 
   * The callback for reading raw bytes of input, along with the custom
   * pass-through pointer that is passed to it.
   */
  shasm_fp_input fpin;
  void *pCustomIn;
  
  /*
   * The final state of the raw input function.
   * 
   * If zero, the raw input function has not yet reached a final state.
   * 
   * If SHASM_INPUT_EOF, the raw input function has returned EOF and
   * should not be called again.
   * 
   * If SHASM_INPUT_IOERR, the raw input function has returned an I/O
   * error (or it returned an invalid value that was interpreted as an
   * I/O error by the filter chain).  The function should not be called
   * again.
   */
  int final_raw;
  
  /*
   * Flag indicating whether the BOM buffer has been initialized yet.
   * 
   * If zero, then the BOM buffer has not yet been initialized.  If one,
   * then the buffer is initialized.
   */
  int bom_init;
  
  /*
   * The BOM buffer.
   * 
   * Stores the first three return values from calling the raw input
   * function.  These may be unsigned byte values (0-255) or
   * SHASM_INPUT_EOF or SHASM_INPUT_IOERR.
   * 
   * This buffer is used by the BOM filter to check for a UTF-8 Byte
   * Order Mark (BOM) at the start of raw input.  It is only relevant if
   * the bom_init flag is set, indicating the buffer has been
   * initialized.
   * 
   * bom_left indicates the number of buffered return values that the
   * BOM filter still needs to read from bom_buf.
   */
  int bom_buf[3];
  
  /*
   * The number of bytes left to read in the BOM buffer.
   * 
   * This field is only relevant if the bom_init flag is set.  If the
   * three buffered return values read into bom_buf match a UTF-8 Byte
   * Order Mark (BOM), then this will be initialized to zero so that the
   * initial BOM is skipped.  Otherwise, this will be initialized to
   * three such that the buffered return values are returned by the BOM
   * filter before reading further raw input.
   */
  int bom_left;
  
  /*
   * A buffer of one return value for use by the line break conversion
   * filter.
   * 
   * If this is SHASM_INPUT_INVALID (its initial value), then the buffer
   * is empty.
   * 
   * Otherwise, the line break buffer should use this buffered value
   * next time instead of calling through to the BOM filter.
   */
  int break_buf;
  
  /*
   * Flag that is set when the last character read was ASCII Line Feed.
   * 
   * If zero, then no characters have been read or the last character
   * read was something besides LF.  If one, then at least one character
   * has been read and the last character read was LF.
   */
  int last_lf;
  
  /*
   * The count of space characters for the tab unghosting filter.
   * 
   * This starts out zero.  If this is non-zero, then a buffered
   * sequence of space characters is present that the tab unghosting
   * filter should read out in subsequent calls, returning space
   * characters this many times, and then using tu_buffer before
   * returning to reading through to the final LF filter.
   * 
   * If this is LONG_MAX, then the counter has overflowed.
   */
  long tu_count;
  
  /*
   * A buffered character for the tab unghosting filter.
   * 
   * This starts out at SHASM_INPUT_INVALID, meaning there is nothing in
   * the buffer.  If tu_count is non-zero, then buffered spaces from
   * that should be read out from the tab unghosting filter before
   * consulting this buffer.  Once the buffered space buffer and
   * tu_buffer are both emptied, the tab unghosting filter returns to
   * reading through from the final LF filter.
   */
  int tu_buffer;
  
  /*
   * The count of tab characters for the line unghosting filter.
   * 
   * This starts out zero.  If this is non-zero, then a buffered
   * sequence of tab characters is present that the line unghosting
   * filter should read out in subsequent calls, returning HT characters
   * this many times, and then using lu_spc and lu_buffer before
   * returning to reading through to the tab unghosting filter.
   * 
   * If this is LONG_MAX, then the counter has overflowed.
   */
  long lu_htc;
  
  /*
   * The count of space characters for the line unghosting filter.
   * 
   * This starts out zero.  If this is non-zero, then a buffered
   * sequence of space characters is present that the line unghosting
   * filter should read out in subsequent calls, returning SP characters
   * this many times.  However, the line unghosting filter should empty
   * the lu_htc count before working on lu_spc.  The line unghosting
   * filter should then clear our lu_buffer before returning to reading
   * through to the tab unghosting filter.
   * 
   * If this is LONG_MAX, then the counter has overflowed.
   */
  long lu_spc;
  
  /*
   * A buffered character for the line unghosting filter.
   * 
   * This starts out at SHASM_INPUT_INVALID, meaning there is nothing in
   * this buffer.  The line unghosting filter should clear out lu_htc
   * and lu_spc before checking lu_buffer.  Once lu_htc, lu_spc, and
   * lu_buffer have been cleared out, the line unghosting filter should
   * return to reading through to the tab unghosting filter.
   */
  int lu_buffer;
  
  /*
   * The line count.
   * 
   * This starts out at one at the beginning of input.  For each
   * filtered LF that is read, it is incremented by one.  If it reaches
   * LONG_MAX, it stays there, so LONG_MAX should be interpreted as an
   * overflow.
   * 
   * Note that the line count filter occurs before the pushback buffer
   * filter, so the line count is unaffected by single-character
   * backtracking.  This means the line count could be off by one right
   * next to a line break.
   */
  long line;
  
  /*
   * The pushback buffer.
   * 
   * This starts out with SHASM_INPUT_INVALID, to indicate that the
   * buffer is empty.  Each character that is read from the underlying
   * line count filter is stored in this buffer, so that the buffer has
   * the most recent character read (or SHASM_INPUT_INVALID if no
   * characters have been read).
   * 
   * If pushback mode is active (see pb_active), then the contents of
   * the buffer should be used for the next return from the pushback
   * buffer, rather than reading another character from the underlying
   * line count buffer.
   */
  int pb_buffer;
  
  /*
   * Pushback buffer active flag.
   * 
   * This starts out zero, indicating that pushback mode is inactive.
   * In this case, the pushback buffer filter will read characters from
   * the underlying line count filter, storing the most recent character
   * in the pushback buffer pb_buffer.
   * 
   * Pushback buffer mode may only be activated when it is currently
   * inactive (it is a fault if it is activated when already active),
   * and only when at least one character has been read from input (that
   * is, the pushback buffer is not empty).
   */
  int pb_active;

};

/* 
 * Local functions
 * ===============
 */
static void shasm_iflstate_init(
    SHASM_IFLSTATE *ps,
    shasm_fp_input fpin,
    void *pCustom);
static int shasm_input_read(SHASM_IFLSTATE *ps);
static void shasm_input_initbom(SHASM_IFLSTATE *ps);
static int shasm_input_bom(SHASM_IFLSTATE *ps);
static int shasm_input_break(SHASM_IFLSTATE *ps);
static int shasm_input_final(SHASM_IFLSTATE *ps);
static int shasm_input_tabung(SHASM_IFLSTATE *ps);
static int shasm_input_lineung(SHASM_IFLSTATE *ps);
static int shasm_input_linec(SHASM_IFLSTATE *ps);
static int shasm_input_pushb(SHASM_IFLSTATE *ps);

/*
 * Properly initialize an input filter chain state structure.
 * 
 * Pass the callback for reading raw input bytes, as well as the custom
 * pass-through parameter to pass to this callback (NULL is allowed).
 * 
 * Parameters:
 * 
 *   ps - the input filter chain state structure to initialize
 * 
 *   fpin - the raw input callback (may not be NULL)
 * 
 *   pCustom - the custom pass-through parameter to pass to the raw
 *   input callback (may be NULL)
 */
static void shasm_iflstate_init(
    SHASM_IFLSTATE *ps,
    shasm_fp_input fpin,
    void *pCustom) {
  
  /* Check parameters */
  if ((ps == NULL) || (fpin == NULL)) {
    abort();
  }
  
  /* Clear structure */
  memset(ps, 0, sizeof(SHASM_IFLSTATE));
  
  /* Initialize structure fields */
  ps->fpin = fpin;
  ps->pCustomIn = pCustom;
  ps->final_raw = 0;
  
  ps->bom_init = 0;
  ps->break_buf = SHASM_INPUT_INVALID;
  ps->last_lf = 0;
  
  ps->tu_count = 0;
  ps->tu_buffer = SHASM_INPUT_INVALID;
  
  ps->lu_htc = 0;
  ps->lu_spc = 0;
  ps->lu_buffer = SHASM_INPUT_INVALID;
  
  ps->line = 1;
  
  ps->pb_buffer = SHASM_INPUT_INVALID;
  ps->pb_active = 0;
}

/*
 * Use the raw input callback in the input filter state structure to
 * read a raw byte of input from the underlying input source.
 * 
 * The return value is either an unsigned byte value (0-255), or
 * SHASM_INPUT_EOF for end of input, or SHASM_INPUT_IOERR for I/O error.
 * 
 * If the raw input function returns something other than these allowed
 * possibilities, it is changed by this function to SHASM_INPUT_IOERR
 * and handled that way.
 * 
 * Once this function returns SHASM_INPUT_EOF or SHASM_INPUT_IOERR, it
 * will return that on all subsequent calls and no longer make any
 * further calls to the raw input function.
 * 
 * Parameters:
 * 
 *   ps - the input filter state structure
 * 
 * Return:
 * 
 *   the unsigned byte value of the next byte (0-255), or
 *   SHASM_INPUT_EOF, or SHASM_INPUT_IOERR
 */
static int shasm_input_read(SHASM_IFLSTATE *ps) {
  
  int result = 0;
  
  /* Check parameter */
  if (ps == NULL) {
    abort();
  }
  
  /* Check if already a final result */
  if (ps->final_raw == 0) {
    /* No final result yet -- call through to raw input reader */
    result = (*(ps->fpin))(ps->pCustomIn);
    
    /* Handle special results */
    if (result == SHASM_INPUT_EOF) {
      /* EOF -- store as final result */
      ps->final_raw = SHASM_INPUT_EOF;
      
    } else if ((result < 0) || (result > 255)) {
      /* SHASM_INPUT_IOERR or invalid return value -- set IOERR as
       * result and store as final result */
      result = SHASM_INPUT_IOERR;
      ps->final_raw = SHASM_INPUT_IOERR;
    }
    
  } else {
    /* Already a final result -- just return that */
    result = ps->final_raw;
  }
  
  /* Return result */
  return result;
}

/*
 * Initialize the BOM buffer if not already initialized.
 * 
 * This function only takes action if the bom_init flag is zero,
 * indicating that the BOM buffer has not yet been initialized.  In this
 * case, the function calls shasm_input_read three times and buffers
 * these first three return values in bom_buf (including special EOF and
 * IOERR returns).  If these first three return values match a UTF-8
 * Byte Order Mark (BOM), then bom_left is set to zero so that the BOM
 * will be skipped; else, bom_left is set to three so that the BOM
 * filter will use the buffered return values for its first three
 * returns.  In both cases, the bom_init flag is set to indicate that
 * the BOM buffer is now initialized.
 * 
 * Parameters:
 * 
 *   ps - the input filter state structure
 */
static void shasm_input_initbom(SHASM_IFLSTATE *ps) {
  /* Check parameter */
  if (ps == NULL) {
    abort();
  }
  
  /* Only take action if not yet initialized */
  if (!(ps->bom_init)) {
    
    /* Read the first three raw return values into the buffer */
    ps->bom_buf[0] = shasm_input_read(ps);
    ps->bom_buf[1] = shasm_input_read(ps);
    ps->bom_buf[2] = shasm_input_read(ps);
    
    /* Set bom_left depending on whether we read a UTF-8 BOM */
    if ((ps->bom_buf[0] == SHASM_BOM_BYTE_1) &&
        (ps->bom_buf[1] == SHASM_BOM_BYTE_2) &&
        (ps->bom_buf[2] == SHASM_BOM_BYTE_3)) {
      /* UTF-8 BOM -- set bom_left to zero to skip it */
      ps->bom_left = 0;
    
    } else {
      /* Not a UTF-8 BOM -- set bom_left to three so the buffered values
       * will be reread by the BOM filter */
      ps->bom_left = 3;
    }
    
    /* Set the BOM initialization flag */
    ps->bom_init = 1;
  }
}

/*
 * The UTF-8 Byte Order Mark (BOM) filter.
 * 
 * This filter is built on top of shasm_input_read, which reads directly
 * from the raw input callback.
 * 
 * This filter first initializes the BOM buffer if not already
 * initialized by calling shasm_input_initbom.  This has the effect of
 * buffering the first three raw input returns in bom_buf and setting
 * bom_left to zero if the first three returns match a UTF-8 BOM, or
 * else setting bom_left to three.
 * 
 * This BOM filter will pass through all input from shasm_input_read,
 * except when bom_left is non-zero.  In this case, the BOM filter will
 * return a buffered return value and decrement bom_left.
 * 
 * The effect of this filter is to skip over the first three bytes of
 * the file if they match a UTF-8 BOM, but otherwise pass through all
 * raw input.  In short, a UTF-8 BOM at the start of input is filtered
 * out if present.  shasm_input_hasbom can be used to determine whether
 * a BOM was present at the start of input.
 * 
 * Parameters:
 * 
 *   ps - the input filter state
 * 
 * Return:
 * 
 *   the unsigned byte value of the next filtered byte (0-255), or
 *   SHASM_INPUT_EOF, or SHASM_INPUT_IOERR
 */
static int shasm_input_bom(SHASM_IFLSTATE *ps) {
  
  int result = 0;
  
  /* Check parameter */
  if (ps == NULL) {
    abort();
  }
  
  /* Initialize BOM buffer if not already initialized */
  shasm_input_initbom(ps);
  
  /* Determine whether to read from underlying filter or use a buffered
   * result */
  if (ps->bom_left > 0) {
    /* Buffered result -- read it from buffer and update bom_left */
    result = ps->bom_buf[3 - ps->bom_left];
    (ps->bom_left)--;
  
  } else {
    /* No buffered results to read -- read through from underlying
     * filter */
    result = shasm_input_read(ps);
  }
  
  /* Return result */
  return result;
}

/*
 * The line break conversion filter.
 * 
 * This filter is built on top of the BOM filter.  It passes through all
 * characters from the BOM filter, except for ASCII Carriage Return (CR)
 * and ASCII Line Feed (LF).
 * 
 * When CR is encountered, the function will peek at the next character
 * to see if it is LF.  If it is, then the CR+LF pair will be converted
 * to an LF; otherwise, the CR will be converted to LF and the other
 * character will be buffered in the break buffer and read next time.
 * 
 * When LF is encountered, the function will peek at the next character
 * to see if it is CR.  If it is, then the LF+CR pair will be converted
 * to an LF; otherwise, the LF will be passed through as-is and the
 * other character will be buffered in the break buffer and read next
 * time.
 * 
 * The effect of this filter is to convert CR, LF, CR+LF, and LF+CR line
 * breaks in any mixture to LF line breaks.  In ambiguous cases, the
 * first match that is longest is selected.
 * 
 * Parameters:
 * 
 *   ps - the input filter state
 * 
 * Return:
 * 
 *   the unsigned byte value of the next filtered byte (0-255), or
 *   SHASM_INPUT_EOF, or SHASM_INPUT_IOERR
 */
static int shasm_input_break(SHASM_IFLSTATE *ps) {
  
  int c = 0;
  int c2 = 0;
  
  /* Check parameter */
  if (ps == NULL) {
    abort();
  }
  
  /* Read the next character from the buffer if the buffer is not empty;
   * otherwise, read it from the BOM filter */
  if (ps->break_buf != SHASM_INPUT_INVALID) {
    /* Buffered character */
    c = ps->break_buf;
    ps->break_buf = SHASM_INPUT_INVALID;
    
  } else {
    /* No buffered character */
    c = shasm_input_bom(ps);
  }
  
  /* Special handling for CR and LF */
  if (c == SHASM_ASCII_CR) {
    /* Carriage return -- read next character to check for pair */
    c2 = shasm_input_bom(ps);
    
    /* If not paired CR+LF then buffer next character for next time;
     * otherwise leave the paired LF read */
    if (c2 != SHASM_ASCII_LF) {
      ps->break_buf = c2;
    }
    
    /* Convert break to LF */
    c = SHASM_ASCII_LF;
    
  } else if (c == SHASM_ASCII_LF) {
    /* Line feed -- read next character to check for pair */
    c2 = shasm_input_bom(ps);
    
    /* If not paired LF+CR then buffer next character for next time;
     * otherwise leave the paired CR read */
    if (c2 != SHASM_ASCII_CR) {
      ps->break_buf = c2;
    }
    
    /* Break is already LF so no need to convert */
  }
  
  /* Return the filtered character */
  return c;
}

/*
 * Input filter that makes sure the input ends with an ASCII Line Feed
 * character.
 * 
 * This filter is built on top of the line break conversion filter.  It
 * passes through all input as-is until SHASM_INPUT_EOF is encountered.
 * When EOF is encountered, if no characters have been read before it or
 * if the previous character read is not LF, then the filter will
 * convert the EOF to an LF.  Since the earlier shasm_input_read filter
 * returns an infinite sequence of SHASM_INPUT_EOF characters after the
 * first EOF, the next read will then return an EOF.
 * 
 * This filter makes sure that there is at least one line in the file,
 * that every line in the file ends with LF, and that EOF occurs
 * immediately after an EOF.
 * 
 * Parameters:
 * 
 *   ps - the input filter state
 * 
 * Return:
 * 
 *   the unsigned byte value of the next filtered byte (0-255), or
 *   SHASM_INPUT_EOF, or SHASM_INPUT_IOERR
 */
static int shasm_input_final(SHASM_IFLSTATE *ps) {
  
  int c = 0;
  
  /* Check parameter */
  if (ps == NULL) {
    abort();
  }
  
  /* Read the next character from the line break conversion filter */
  c = shasm_input_break(ps);
  
  /* Special handling for LF and EOF; in all other cases clear the
   * last_lf flag if it is set */
  if (c == SHASM_ASCII_LF) {
    /* LF -- set the last_lf flag so next time we know the previous
     * character was an LF */
    ps->last_lf = 1;
    
  } else if (c == SHASM_INPUT_EOF) {
    /* EOF -- if the last_lf flag is clear, convert EOF to an LF and set
     * the last_lf flag; otherwise, leave the last_lf flag set and leave
     * the EOF as-is */
    if (!(ps->last_lf)) {
      c = SHASM_ASCII_LF;
      ps->last_lf = 1;
    }
    
  } else {
    /* Neither LF nor EOF -- clear the last_lf flag */
    ps->last_lf = 0;
  }
  
  /* Return the filtered character */
  return c;
}

/*
 * The tab unghosting filter.
 * 
 * This filter is built on top of the final LF filter.  It discards
 * sequences of one or more ASCII Space (SP) characters that occur
 * immediately before an ASCII Horizontal Tab (HT).
 * 
 * If a sequence of SP characters is long enough to overflow the
 * filter's count of SP characters, then this filter will return
 * SHASM_INPUT_IOERR and continue to return I/O errors for all
 * subsequent calls.  However, this can only happen if there are
 * literally billions of SP characters in a row, so this should be a
 * rare case in practice.
 * 
 * As a result of this filter, all sequences of whitespace consisting
 * of SP and HT characters will consist of a sequence of zero or more
 * HT characters followed by a sequence of zero or more SP characters.
 * 
 * Note that this filter can change the alignment of text from the
 * unfiltered input if mixtures of SP and HT characters were used
 * haphazardly to align text.  To avoid this problem, never use SP
 * immediately before HT, or convert all HT characters to sequences of
 * SP characters before passing as input to this filter chain.
 * 
 * Parameters:
 * 
 *   ps - the input filter state
 * 
 * Return:
 * 
 *   the unsigned byte value of the next filtered byte (0-255), or
 *   SHASM_INPUT_EOF, or SHASM_INPUT_IOERR
 */
static int shasm_input_tabung(SHASM_IFLSTATE *ps) {
  
  int result = 0;
  
  /* Check parameter */
  if (ps == NULL) {
    abort();
  }
  
  /* Determine whether to read a buffered space, an I/O error due to
   * overflow, a buffered character, or to read a new character from the
   * underlying final LF filter */
  if ((ps->tu_count > 0) && (ps->tu_count < LONG_MAX)) {
    /* Read a buffered space */
    result = SHASM_ASCII_SP;
    (ps->tu_count)--;
    
  } else if (ps->tu_count == LONG_MAX) {
    /* SP counter overflow, so read an I/O error */
    result = SHASM_INPUT_IOERR;
    
  } else if (ps->tu_buffer != SHASM_INPUT_INVALID) {
    /* No buffered spaces, but a buffered character, so use that */
    result = ps->tu_buffer;
    ps->tu_buffer = SHASM_INPUT_INVALID;
  
  } else {
    /* Nothing in tab unghosting buffers, so read from underlying final
     * LF filter */
    result = shasm_input_final(ps);
    
    /* Special processing required if we read a SP character; else, just
     * proceed */
    if (result == SHASM_ASCII_SP) {
      /* We just read a SP character -- count how many SP characters
       * occur after this one in tu_count, and buffer the return value
       * that ended the sequence of SP characters in tu_buffer; in case
       * of SP character count overflow, set the count to LONG_MAX and
       * don't bother reading further in input */
      ps->tu_count = 0;
      for(result = shasm_input_final(ps);
          result == SHASM_ASCII_SP;
          result = shasm_input_final(ps)) {
        /* Increment SP count; in case of overflow set count to LONG_MAX
         * and break out of loop */
        if (ps->tu_count < (LONG_MAX - 1)) {
          (ps->tu_count)++;
        } else {
          ps->tu_count = LONG_MAX;
          break;
        }
      }
      
      /* Buffer the non-SP character that was read in tu_buffer, except
       * if the SP counter overflowed, in which case clear buffer */
      if (ps->tu_count < LONG_MAX) {
        ps->tu_buffer = result;
      } else {
        ps->tu_buffer = SHASM_INPUT_INVALID;
      }
      
      /* tu_count now counts how many SP characters occur after the
       * initial one, or it is LONG_MAX in case of overflow; tu_buffer
       * has the return value that ended the sequence of SP characters,
       * or it is SHASM_INPUT_INVALID in case of overflow -- if
       * overflow, return I/O error; else, if tu_buffer is not HT,
       * return SP; else, if tu_buffer is HT, clear the buffers and
       * return HT */
      if (ps->tu_count == LONG_MAX) {
        /* Overflow */
        result = SHASM_INPUT_IOERR;
      
      } else if (ps->tu_buffer == SHASM_ASCII_HT) {
        /* HT was the last character read, so discard the SP sequence
         * by clearing the buffers and return the HT */
        ps->tu_count = 0;
        ps->tu_buffer = SHASM_INPUT_INVALID;
        result = SHASM_ASCII_HT;
      
      } else {
        /* Something other than HT ended the sequence of SP characters,
         * so leave the buffers as-is and return the initial SP that was
         * read */
        result = SHASM_ASCII_SP;
      }
    }
  }
  
  /* Return the filtered character */
  return result;
}

/*
 * The line unghosting filter.
 * 
 * This filter is built on top of the tab unghosting filter.  It
 * discards whitespace sequences consisting of ASCII Space (SP) and
 * ASCII Horizontal Tab (HT) that occur immediately before an ASCII Line
 * Feed (LF).
 * 
 * This filter assumes that in whitespace sequences, any HT characters
 * occur before any SP characters.  The tab unghosting filter should
 * guarantee this condition.  A fault occurs if SP occurs immediately
 * before HT anywhere in input.
 * 
 * The effect of this filter is to remove invisible whitespace at the
 * ends of lines.
 * 
 * If a sequence of SP or HT characters is long enough to overflow the
 * filter's counters, then this filter will return SHASM_INPUT_IOERR and
 * continue to return I/O errors for all subsequent calls.  However,
 * this can only happen if there are literally billions of SP or HT
 * characters in a row, so this should be a rare case in practice.
 * 
 * Parameters:
 * 
 *   ps - the input filter state
 * 
 * Return:
 * 
 *   the unsigned byte value of the next filtered byte (0-255), or
 *   SHASM_INPUT_EOF, or SHASM_INPUT_IOERR
 */
static int shasm_input_lineung(SHASM_IFLSTATE *ps) {
  
  int result = 0;
  int c = 0;
  
  /* Check parameter */
  if (ps == NULL) {
    abort();
  }
  
  /* Determine whether to read a buffered HT, a buffer SP, an I/O error
   * due to overflow, a buffered character, or a new character from the
   * underlying tab unghosting filter */
  if ((ps->lu_htc == LONG_MAX) || (ps->lu_spc == LONG_MAX)) {
    /* I/O error due to counter overflow */
    result = SHASM_INPUT_IOERR;
    
  } else if (ps->lu_htc > 0) {
    /* Read a buffered tab */
    result = SHASM_ASCII_HT;
    (ps->lu_htc)--;
    
  } else if (ps->lu_spc > 0) {
    /* Read a buffered space */
    result = SHASM_ASCII_SP;
    (ps->lu_spc)--;
    
  } else if (ps->lu_buffer != SHASM_INPUT_INVALID) {
    /* Read a buffered character */
    result = ps->lu_buffer;
    ps->lu_buffer = SHASM_INPUT_INVALID;
    
  } else {
    /* Buffers are clear and no overflow error, so read a character from
     * the underlying tab unghosting filter */
    result = shasm_input_tabung(ps);
    
    /* Special handling required for SP and HT characters; otherwise,
     * just pass through the result from the underlying tab unghosting
     * filter */
    if ((result == SHASM_ASCII_HT) || (result == SHASM_ASCII_SP)) {
      /* We just read a SP or HT character into result -- count how many
       * SP and HT characters occur after it in lu_htc and lu_spc,
       * record the return value that ended the whitespace sequence in
       * lu_buffer, or set both counters to LONG_MAX and stop reading in
       * case of overflow; leave result alone for now */
      ps->lu_htc = 0;
      ps->lu_spc = 0;
      for(c = shasm_input_tabung(ps);
          (c == SHASM_ASCII_HT) || (c == SHASM_ASCII_SP);
          c = shasm_input_tabung(ps)) {
        
        /* If HT read, fault if the SP count is non-zero or if the first
         * character read (result) was SP, because HT is never supposed
         * to occur immediately after SP in the input to this filter */
        if ((c == SHASM_ASCII_HT) &&
              ((ps->lu_spc > 0) || (result == SHASM_ASCII_SP))) {
          abort();
        }
        
        /* Update the appropriate counter, setting both to LONG_MAX if
         * there is an overflow */
        if (c == SHASM_ASCII_HT) {
          /* Update HT count, watching for overflow */
          if (ps->lu_htc < (LONG_MAX - 1)) {
            (ps->lu_htc)++;
          } else {
            ps->lu_htc = LONG_MAX;
            ps->lu_spc = LONG_MAX;
          }
          
        } else if (c == SHASM_ASCII_SP) {
          /* Update SP count, watching for overflow */
          if (ps->lu_spc < (LONG_MAX - 1)) {
            (ps->lu_spc)++;
          } else {
            ps->lu_htc = LONG_MAX;
            ps->lu_spc = LONG_MAX;
          }
          
        } else {
          /* Something other than SP or HT -- shouldn't happen */
          abort();
        }
        
        /* Break out of loop if overflow occurred */
        if ((ps->lu_htc == LONG_MAX) || (ps->lu_spc == LONG_MAX)) {
          break;
        }
      }
      
      /* Buffer the non-SP non-HT character that was read in lu_buffer,
       * except if one of the counters overflowed, clear the buffer */
      if ((ps->lu_htc < LONG_MAX) && (ps->lu_spc < LONG_MAX)) {
        ps->lu_buffer = c;
      } else {
        ps->lu_buffer = SHASM_INPUT_INVALID;
      }
    
      /* lu_htc now counts the HT characters after the initial
       * whitespace character or is LONG_MAX if overflow; lu_spc counts
       * the SP characters after the initial whitespace character or it
       * is LONG_MAX if overflow; lu_buffer has the input return that
       * ended the whitespace sequence or it is SHASM_INPUT_INVALID if
       * overflow; result has the initial whitespace character -- if
       * overflow, return I/O error; else, if lu_buffer has an LF then
       * clear the counters and buffer and return the LF; else, leave
       * result as the first whitespace character and leave the counters
       * and buffer as-is */
      if ((ps->lu_htc == LONG_MAX) || (ps->lu_spc == LONG_MAX)) {
        /* Overflow, so return I/O error */
        result = SHASM_INPUT_IOERR;
      
      } else if (ps->lu_buffer == SHASM_ASCII_LF) {
        /* LF ended the sequence, so discard whitespace sequence by
         * clearing counters and buffer and returning the LF */
        ps->lu_htc = 0;
        ps->lu_spc = 0;
        ps->lu_buffer = SHASM_INPUT_INVALID;
        result = SHASM_ASCII_LF;
      }
    }
  }
  
  /* Return the filtered character */
  return result;
}

/*
 * The line count filter.
 * 
 * This filter is built on top of the line unghosting filter.  It passes
 * through everything from the line unghosting filter.  However,
 * whenever a filtered LF is encountered, this filter will increment the
 * line count, unless the line count has already reached LONG_MAX, in
 * which case it is left there.
 * 
 * This filter therefore makes sure that the line count is updated.
 * Note that because this filter occurs before the pushback buffer
 * filter, the line count is unaffected by backtracking.  This means
 * that the line count could be off by one near a line break.
 * 
 * Parameters:
 * 
 *   ps - the input filter state
 * 
 * Return:
 * 
 *   the unsigned byte value of the next filtered byte (0-255), or
 *   SHASM_INPUT_EOF, or SHASM_INPUT_IOERR
 */
static int shasm_input_linec(SHASM_IFLSTATE *ps) {
  
  int result = 0;
  
  /* Check parameter */
  if (ps == NULL) {
    abort();
  }
  
  /* Read a character from the underlying line unghosting filter */
  result = shasm_input_lineung(ps);
  
  /* If an LF was read, increment line count unless line count already
   * at LONG_MAX */
  if ((result == SHASM_ASCII_LF) && (ps->line < LONG_MAX)) {
    (ps->line)++;
  }
  
  /* Return result */
  return result;
}

/*
 * The pushback buffer filter.
 * 
 * This is built on top of the line count filter.  If pushback mode is
 * inactive, it passes through characters from the underlying line count
 * filter, storing the most recent character in the pushback buffer.  If
 * pushback mode is active, it returns the contents of the pushback
 * buffer and clears pushback mode.
 * 
 * This filter allows backtracking by one character by using the
 * shasm_input_back function.  See that function for further
 * information.
 * 
 * Parameters:
 * 
 *   ps - the input filter state
 * 
 * Return:
 * 
 *   the unsigned byte value of the next filtered byte (0-255), or
 *   SHASM_INPUT_EOF, or SHASM_INPUT_IOERR
 */
static int shasm_input_pushb(SHASM_IFLSTATE *ps) {
  
  int c = 0;
  
  /* Check parameter */
  if (ps == NULL) {
    abort();
  }
  
  /* Either read from underlying filter or from the pushback buffer */
  if (ps->pb_active) {
    /* Pushback mode active, so read from pushback buffer and clear
     * pushback mode */
    c = ps->pb_buffer;
    ps->pb_active = 0;
    
  } else {
    /* Pushback mode inactive, so read from underlying line count filter
     * and store the most recent character in the pushback buffer */
    c = shasm_input_linec(ps);
    ps->pb_buffer = c;
  }
  
  /* Return the filtered character */
  return c;
}

/*
 * Public functions
 * ================
 * 
 * See the header for specifications.
 */

/*
 * shasm_input_alloc function.
 */
SHASM_IFLSTATE *shasm_input_alloc(shasm_fp_input fpin, void *pCustom) {
  SHASM_IFLSTATE *ps = NULL;
  
  /* Allocate a new structure */
  ps = (SHASM_IFLSTATE *) malloc(sizeof(SHASM_IFLSTATE));
  if (ps == NULL) {
    abort();
  }
  
  /* Initialize structure */
  shasm_iflstate_init(ps, fpin, pCustom);
  
  /* Return the new structure */
  return ps;
}

/*
 * shasm_input_free function.
 */
void shasm_input_free(SHASM_IFLSTATE *ps) {
  /* Free structure if pointer is not NULL */
  if (ps != NULL) {
    free(ps);
  }
}

/*
 * shasm_input_hasbom function.
 */
int shasm_input_hasbom(SHASM_IFLSTATE *ps) {
  
  int result = 0;
  
  /* Check parameter */
  if (ps == NULL) {
    abort();
  }
  
  /* Initialize BOM buffer if necessary */
  shasm_input_initbom(ps);
  
  /* Determine result depending on whether we read a UTF-8 BOM */
  if ((ps->bom_buf[0] == SHASM_BOM_BYTE_1) &&
      (ps->bom_buf[1] == SHASM_BOM_BYTE_2) &&
      (ps->bom_buf[2] == SHASM_BOM_BYTE_3)) {
    /* UTF-8 BOM -- set result to true */
    result = 1;
    
  } else {
    /* Not a UTF-8 BOM -- set result to false */
    result = 0;
  }
  
  /* Return result */
  return result;
}

/*
 * shasm_input_count function.
 */
long shasm_input_count(SHASM_IFLSTATE *ps) {
  
  /* Check parameter */
  if (ps == NULL) {
    abort();
  }
  
  /* Return the line count */
  return ps->line;
}

/*
 * shasm_input_get function.
 */
int shasm_input_get(SHASM_IFLSTATE *ps) {
  /* Call through to the last filter in the input filter chain, which is
   * the pushback buffer filter */
  return shasm_input_pushb(ps);
}

/*
 * shasm_input_back function.
 */
void shasm_input_back(SHASM_IFLSTATE *ps) {
  
  /* Check parameter */
  if (ps == NULL) {
    abort();
  }
  
  /* Verify that not already in pushback mode, and that at least one
   * filtered character has been read */
  if (ps->pb_active || (ps->pb_buffer == SHASM_INPUT_INVALID)) {
    abort();
  }
  
  /* Activate pushback mode */
  ps->pb_active = 1;
}
