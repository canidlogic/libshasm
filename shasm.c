/*
 * shasm.c
 * 
 * Work-in-progress testing program for developing libshasm.
 * 
 * See README.md for a summary of the current development work.
 */

#include <limits.h>
#include <stddef.h>
#include <stdio.h>  /* only for testing functions */
#include <stdlib.h>
#include <string.h>

/*
 * Special state codes that the shasm_fp_input callback can return.
 */
#define SHASM_INPUT_EOF   (-1)    /* End of file */
#define SHASM_INPUT_IOERR (-2)    /* I/O error   */

/*
 * The three unsigned byte values for a UTF-8 Byte Order Mark (BOM).
 */
#define SHASM_BOM_BYTE_1 (0xef)
#define SHASM_BOM_BYTE_2 (0xbb)
#define SHASM_BOM_BYTE_3 (0xbf)

/*
 * Function pointer type for the input reader function.
 * 
 * This is the callback that Shastina's input filter chain uses to read
 * raw input bytes from the actual input.
 * 
 * The function takes a single argument, which is a custom pass-through
 * pointer.
 * 
 * The return value is the next byte read, which is either an unsigned
 * byte value (0-255) or SHASM_INPUT_EOF if no more bytes to read, or
 * SHASM_INPUT_IOERR if there was an I/O error trying to read a byte.
 * 
 * Once the function returns SHASM_INPUT_EOF or SHASM_INPUT_IOERR, it
 * will not be called again by the input filter chain.
 */
typedef int (*shasm_fp_input)(void *);

/*
 * Structure for storing state information of input filter chain.
 * 
 * Use shasm_iflstate_init to properly initialize this structure.
 */
typedef struct {
  
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
  
  /* @@TODO: */

} SHASM_IFLSTATE;

/* Function prototypes for Shastina functions */
static void shasm_iflstate_init(
    SHASM_IFLSTATE *ps,
    shasm_fp_input fpin,
    void *pCustom);
static int shasm_input_read(SHASM_IFLSTATE *ps);
static void shasm_input_initbom(SHASM_IFLSTATE *ps);
static int shasm_input_bom(SHASM_IFLSTATE *ps);
static int shasm_input_hasbom(SHASM_IFLSTATE *ps);
static long shasm_input_count(SHASM_IFLSTATE *ps);
static int shasm_input_get(SHASM_IFLSTATE *ps);

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
  
  /* @@TODO: make sure this function is up to date with the
   * SHASM_IFLSTATE structure */
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
 * Return whether the underlying raw input begins with a UTF-8 Byte
 * Order Mark (BOM) that the input filter chain filtered out.
 * 
 * This can be used before any filtered bytes have been read.  In this
 * case, the function will read and buffer up to three bytes from the
 * beginning of raw input to check for the BOM.
 * 
 * Parameters:
 * 
 *   ps - the input filter state
 * 
 * Return:
 * 
 *   non-zero if a UTF-8 BOM is present at the start of raw input, zero
 *   if not
 */
static int shasm_input_hasbom(SHASM_IFLSTATE *ps) {
  
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
 * Get the current line number of input.
 * 
 * The line number starts at one and increments each time a filtered LF
 * is read.  The line count filter handles updating the line count.  If
 * the line count would overflow the range of a signed long, it stays at
 * LONG_MAX.  LONG_MAX should therefore be interpreted as a line count
 * overflow.
 * 
 * The line count filter happens before the pushback buffer, so
 * unreading a line break will not unread the line number change.  This
 * means the line count may be off by one right next to a line break.
 * 
 * Parameters:
 * 
 *   ps - the input filter state structure
 * 
 * Return:
 * 
 *   the line count, or LONG_MAX if line count overflow
 */
static long shasm_input_count(SHASM_IFLSTATE *ps) {
  /* @@TODO: query for line count after line count filter added; for now
   * just return a dummy value */
  return 1;
}

/*
 * Get the next byte of input that has been passed through the input
 * filter chain.
 * 
 * The return value is either an unsigned byte value (0-255) or
 * SHASM_INPUT_EOF if at end of input or SHASM_INPUT_IOERR if there has
 * been an I/O error.
 * 
 * This is the function to use to get a byte from the input filter
 * chain.
 * 
 * Parameters:
 * 
 *   ps - the input filter state
 * 
 * Return:
 * 
 *   the unsigned byte value of the next byte (0-255), or
 *   SHASM_INPUT_EOF, or SHASM_INPUT_IOERR
 */
static int shasm_input_get(SHASM_IFLSTATE *ps) {
  /* @@TODO: update so that this calls through to the last filter of the
   * filter chain -- while under development this will call through to
   * the last filter that has been developed */
  return shasm_input_bom(ps);
}

/* @@TODO: testing functions below */

/*
 * Read the next byte of input from the input filter chain.
 * 
 * The return value is an unsigned byte value (0-255).  -1 is returned
 * if EOF is encountered.  -2 is returned if there was an I/O error.
 * 
 * Parameters:
 * 
 *   ps - the input filter state
 * 
 * Return:
 * 
 *   the next filtered byte from the input filter chain, or -1 for EOF,
 *   or -2 if I/O error
 */
static int readNextByte(SHASM_IFLSTATE *ps) {
  
  int c = 0;
  
  /* Check parameter */
  if (ps == NULL) {
    abort();
  }
  
  /* Read a filtered byte of input */
  c = shasm_input_get(ps);
  if (c == SHASM_INPUT_EOF) {
    c = -1;
  } else if (c == SHASM_INPUT_IOERR) {
    c = -2;
  }
  
  /* Return result */
  return c;
}

/*
 * Print the current line number at the start of a new line of output.
 * 
 * The current line number will be determined by querying the line count
 * filter.
 * 
 * The line number will be printed as three zero-padded digits, with
 * "???" printed if the line number is outside the range 0-999.  After
 * the digits there will be a colon followed by a space.
 * 
 * Parameters:
 * 
 *   ps - the input filter state
 */
static void printLineNumber(SHASM_IFLSTATE *ps) {

  long line_num = 0;
  
  /* Check parameter */
  if (ps == NULL) {
    abort();
  }
  
  /* Get the line number, or LONG_MAX if line count overflow */
  line_num = shasm_input_count(ps);
  
  /* Check whether line number is in range */
  if ((line_num >= 0) && (line_num <= 999)) {
    /* In range -- print the line number */
    printf("%03d: ", (int) line_num);
  } else {
    /* Out of range -- print "???" for line number */
    printf("???: ");
  }
}

/*
 * Print a given unsigned byte value, replacing it with a text
 * representation if it is outside non-whitespace US-ASCII range.
 * 
 * The given value must be in unsigned byte range 0-255 or there will be
 * a fault.  If the value is in range 0x20-0x7e, then it is printed to
 * standard output as-is.  Otherwise, it is printed as "<ab>" where a
 * and b are base-16 digits that represent the unsigned value of the
 * byte in base-16.
 * 
 * Parameters:
 * 
 *   c - the unsigned byte value to print
 */
static void printByte(int c) {
  
  /* Check parameter */
  if ((c < 0) || (c > 255)) {
    abort();
  }
  
  /* Determine how to print the value */
  if ((c >= 0x20) && (c <= 0x7e)) {
    /* In US-ASCII printing range -- print directly */
    putchar(c);
    
  } else {
    /* Not in visible printing range -- print as <ab> representation */
    printf("<%02x>", c);
  }
}

/*
 * Print a full line of filtered input, passing the results through the
 * normal mode output filters.
 * 
 * This begins by printing the line number with printLineNumber.  Then,
 * the function uses readNextByte to read bytes until either LF (0xa),
 * EOF, or I/O error is encountered.  Bytes before LF, EOF, or I/O error
 * are printed with printByte.  If LF is encountered, the function
 * prints it with printByte and returns one to indicate that more lines
 * should be printed.  If EOF is encountered, the function returns zero
 * to indicate no further lines should be printed.  If an I/O error is
 * encountered, the function returns -1.
 * 
 * In all cases, the function prints an actual line break at the end of
 * the line.
 * 
 * This function does not report the I/O error, leaving that for the
 * caller.
 * 
 * Parameters:
 * 
 *   ps - the input filter state
 * 
 * Return:
 * 
 *   greater than zero if more lines to read; zero if all lines have
 *   been read; less than zero if I/O error
 */
static int printFullLine(SHASM_IFLSTATE *ps) {
  
  int c = 0;
  
  /* Check parameter */
  if (ps == NULL) {
    abort();
  }
  
  /* Print the line number */
  printLineNumber(ps);
  
  /* Print all bytes before the LF, EOF, or I/O error */
  for(  c = readNextByte(ps);
        (c >=0) && (c != 0xa);
        c = readNextByte(ps)) {
    printByte(c);
  }
  
  /* If LF, print that too */
  if (c == 0xa) {
    printByte(c);
  }
  
  /* Print an actual line break */
  putchar('\n');
  
  /* Set c to the return value depending on its current state */
  if (c == 0xa) {
    /* LF -- more lines to read */
    c = 1;
  } else if (c == -1) {
    /* EOF -- no more lines to read */
    c = 0;
  } else {
    /* I/O error */
    c = -1;
  }
  
  /* Return result */
  return c;
}

/*
 * Print all lines with printFullLine.
 * 
 * This prints the input in normal mode, passing it through the input
 * filter chain.
 * 
 * The return value indicates whether printing was stopped with an EOF
 * (successful return) or whether printing was stopped on an I/O error
 * from input (failure return).
 * 
 * I/O errors are not reported by this function, leaving that for the
 * caller.
 * 
 * Parameters:
 * 
 *   ps - the input filter state
 * 
 * Return:
 * 
 *   non-zero if successful, zero if I/O error
 */
static int printAllLines(SHASM_IFLSTATE *ps) {
  int result = 0;
  
  /* Check parameter */
  if (ps == NULL) {
    abort();
  }
  
  /* Print lines until done or I/O error */
  for(  result = printFullLine(ps);
        result > 0;
        result = printFullLine(ps));
  
  /* Set result of this function depending on result of last call to
   * printFullLine */
  if (result == 0) {
    /* Stopped on EOF -- success */
    result = 1;
  } else {
    /* Stopped on I/O error */
    result = 0;
  }
  
  /* Return result */
  return result;
}

/*
 * Implementation of the raw input callback.
 * 
 * Reads the next byte of input from standard input, or SHASM_INPUT_EOF
 * for EOF, or SHASM_INPUT_IOERR for I/O error.
 * 
 * The custom pass-through parameter is not used.
 * 
 * Parameters:
 * 
 *   pCustom - (ignored)
 * 
 * Return:
 * 
 *   the unsigned value of the next byte of input (0-255), or
 *   SHASM_INPUT_EOF, or SHASM_INPUT_IOERR
 */
static int rawInput(void *pCustom) {
  int c = 0;
  
  /* Ignore parameter */
  (void) pCustom;
  
  /* Read next byte from standard input */
  c = getchar();
  if (c == EOF) {
    if (feof(stdin)) {
      c = SHASM_INPUT_EOF;
    } else {
      c = SHASM_INPUT_IOERR;
    }
  }
  
  /* Return result */
  return c;
}

/*
 * Program entrypoint.
 * 
 * Beyond the module name as the first parameter, this program
 * optionally takes an additional parameter that must be a
 * case-sensitive match for the letter "d".  If there are no parameters
 * beyond the module name, the program will be in normal mode.  If the
 * "d" parameter is provided, the program will be in doubling mdoe.
 * 
 * Any other parameters provided to the program results in an error.
 * 
 * In normal mode, the program reads from standard input, passes it
 * through a sequence of input filters matching section 3 of the 3V:C4-5
 * draft of the Shastina Specification, and writes the filtered results
 * to standard output.
 * 
 * Additionally, the filtered results will be passed through some
 * additional output filters in normal mode:
 * 
 * (1) At the start of each line, the line number as determined by the
 * line count filter will be written as a zero-padded three-digit number
 * followed by a colon and a space.
 * 
 * (2) All filtered bytes that are outside of US-ASCII printing range
 * (0x20-0x7e) will be written in the form <ab> where a and b are the
 * base-16 digits of the unsigned byte value.  A normal space character
 * will be written as a space and not as an <ab> pair.
 * 
 * (3) After all the filtered results have been printed, the final line
 * count as determined by the line count filter and whether or not a
 * UTF-8 Byte Order Mark (BOM) was present will be printed out on
 * separate lines.
 * 
 * In doubling mode, the filtered input will be echoed to standard
 * output without any of the output filters described above.  Instead,
 * the pushback buffer filter will be used to double each character that
 * is in non-whitespace US-ASCII printing range (0x21-0x7e) by pushing
 * each such character back once.
 * 
 * Any errors will be reported to stderr and cause the program to stop.
 * 
 * Parameters:
 * 
 *   argc - the number of elements in argv
 * 
 *   argv - an array of pointers to null-terminated strings representing
 *   the program arguments, with the first argument representing the
 *   module name and any arguments after that representing additional
 *   parameters passed on the command line
 * 
 * Return:
 * 
 *   zero if successful, one if there was an error
 */
int main(int argc, char *argv[]) {
  
  int status = 1;
  int doubling = 0;
  long lcount = 0;
  SHASM_IFLSTATE ifs;
  
  /* Initialize input state structure */
  shasm_iflstate_init(&ifs, &rawInput, NULL);
  
  /* Determine if doubling mode in effect by examining parameters */
  if (argc < 2) {
    /* No parameters beyond module name -- doubling mode not in
     * effect */
    doubling = 0;
    
  } else if (argc == 2) {
    /* Exactly one additional parameter -- check that it is "d" */
    if (argv == NULL) {
      abort();
    }
    if (argv[1] == NULL) {
      abort();
    }
    if (strcmp(argv[1], "d") != 0) {
      status = 0;
      fprintf(stderr, "Invalid program argument!\n");
    }
    
    /* Set doubling mode */
    if (status) {
      doubling = 1;
    }
    
  } else {
    /* Invalid number of parameters */
    status = 0;
    fprintf(stderr, "Expecting at most one additional argument!\n");
  }
  
  /* Run program depending on whether doubling mode is selected */
  if (status && doubling) {
    /* Doubling mode */
    /* @@TODO: write this once pushback filter has been implemented */
    status = 0;
    fprintf(stderr, "Doubling mode not yet implemented!\n");
    
  } else if (status) {
    /* Normal mode -- begin by printing all lines */
    if (!printAllLines(&ifs)) {
      status = 0;
      fprintf(stderr, "I/O error!\n");
    }
    
    /* Print total number of lines */
    if (status) {
      lcount = shasm_input_count(&ifs);
      if (lcount != LONG_MAX) {
        printf("\nTotal line count: %ld\n", lcount);
      } else {
        printf("\nTotal line count: (overflow)\n");
      }
    }
    
    /* Print whether there was a BOM */
    if (status) {
      if (shasm_input_hasbom(&ifs)) {
        printf("UTF-8 BOM was present.\n");
      } else {
        printf("UTF-8 BOM was absent.\n");
      }
    }
  }
  
  /* Invert status */
  if (status) {
    status = 0;
  } else {
    status = 1;
  }
  
  /* Return inverted status */
  return status;
}
