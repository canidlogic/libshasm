/*
 * shasm.c
 * 
 * Work-in-progress testing program for developing libshasm.
 * 
 * See README.md for a summary of the current development work.
 */

#include <stdio.h>    /* only for the testing program */
#include <stdlib.h>

/*
 * Print the current line number at the start of a new line of output.
 * 
 * The current line number will be determined by querying the line count
 * filter.
 * 
 * The line number will be printed as three zero-padded digits, with a
 * fault if the line number is outside of the range 0-999.  After the
 * digits there will be a colon followed by a space.
 */
static void printLineNumber(void) {

  long line_num = 0;
  
  /* @@TODO: once the line number filter is written, replace this code
   * with a call to the filter to determine the line number, converting
   * the result to a signed long */
  line_num = 12;
  
  /* Check that line number is in range */
  if ((line_num < 0) || (line_num > 999)) {
    fprintf(stderr, "Line number out of range!\n");
    abort();
  }
  
  /* Print the line number */
  printf("%03ld: ", line_num);
}

/*
 * Print a given unsigned byte value, replacing it with a text
 * representation if it is outside non-whitespace US-ASCII range.
 * 
 * The given value must be in unsigned byte range 0-255 or there will be
 * a fault.  If the value is in range 0x21-0x7e, then it is printed to
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
  if ((c >= 0x21) && (c <= 0x7e)) {
    /* In non-whitespace US-ASCII printing range -- print directly */
    putchar(c);
    
  } else {
    /* Not in visible printing range -- print as <ab> representation */
    printf("<%02x>", c);
  }
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
 * (2) All filtered bytes that are outside of non-whitespace US-ASCII
 * printing range (0x21-0x7e) will be written in the form <ab> where a
 * and b are the base-16 digits of the unsigned byte value.
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
  /* @@TODO: */
  printLineNumber();
  printByte(0x21);
  printByte(0xa);
  printf("\n");
  return 0;
}
