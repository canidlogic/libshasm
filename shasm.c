/*
 * shasm.c
 * 
 * Work-in-progress testing program for developing libshasm.
 * 
 * See README.md for a summary of the current development work.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Read the next byte of input from the input filter chain.
 * 
 * The return value is an unsigned byte value (0-255).  -1 is returned
 * if EOF is encountered.  -2 is returned if there was an I/O error.
 * 
 * Return:
 * 
 *   the next filtered byte from the input filter chain, or -1 for EOF,
 *   or -2 if I/O error
 */
static int readNextByte(void) {
  
  int c = 0;
  
  /* @@TODO: replace the following call with a call to the input filter
   * chain -- at the moment input is taken directly from standard
   * input */
  c = getchar();
  if (c == EOF) {
    /* Determine whether EOF or I/O error and set c appropriately */
    if (ferror(stdin)) {
      /* I/O error */
      c = -2;
    } else {
      /* EOF */
      c = -1;
    }
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
 */
static void printLineNumber(void) {

  long line_num = 0;
  
  /* @@TODO: once the line number filter is written, replace this code
   * with a call to the filter to determine the line number, converting
   * the result to a signed long */
  line_num = 12;
  
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
 * Return:
 * 
 *   greater than zero if more lines to read; zero if all lines have
 *   been read; less than zero if I/O error
 */
static int printFullLine(void) {
  
  int c = 0;
  
  /* Print the line number */
  printLineNumber();
  
  /* Print all bytes before the LF, EOF, or I/O error */
  for(c = readNextByte(); (c >=0) && (c != 0xa); c = readNextByte()) {
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
 * Return:
 * 
 *   non-zero if successful, zero if I/O error
 */
static int printAllLines(void) {
  int result = 0;
  
  /* Print lines until done or I/O error */
  for(result = printFullLine(); result > 0; result = printFullLine());
  
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
 * (3) After all the filtered results have been printed, a final status
 * line will be printed that reports the line number as determined by
 * the line count filter at the very end of input.  The report will also
 * indicate if a UTF-8 Byte Order Mark (BOM) has been filtered out by
 * the filter chain.
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
    if (!printAllLines()) {
      status = 0;
      fprintf(stderr, "I/O error!\n");
    }
    
    /* @@TODO: print final line with line count and BOM suppression once
     * those filters have been written */
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
