/*
 * test_block.c
 * 
 * Testing module for the block reader module (shasm_block).
 * 
 * This testing program reads from standard input using the block
 * reader.  The testing program is invoked like this:
 * 
 *   test_block [mode] [...]
 * 
 * The [mode] argument must be a string selecting one of the testing
 * modes (defined below).  The [...] represents zero or more additional
 * arguments specific to the testing mode.
 * 
 * Testing modes
 * =============
 * 
 * This section describes each of the testing modes that this testing
 * module supports.
 * 
 * token
 * -----
 * 
 * Token testing mode is invoked as follows:
 * 
 *   test_block token
 * 
 * There are no additional parameters beyond the mode name.  The testing
 * mode name "token" is case sensitive.
 * 
 * In token mode, the testing program will use the block reader to read
 * one or more tokens from input, stopping when the token "|;" is
 * encountered.  For each token, the line number will be reported along
 * with the contents of the token.
 * 
 * Note that this mode can't fully parse all the tokens in a normal
 * Shastina file because it does not handle interpolated string data.
 * 
 * @@TODO: additional testing modes
 * 
 * Compilation
 * ===========
 * 
 * This must be compiled with shasm_block and shasm_input.  Sample build
 * line:
 * 
 *   cc -o test_block test_block.c shasm_input.c shasm_block.c
 */

#include "shasm_block.h"
#include "shasm_input.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* @@TODO: additional testing modes */

/* Function prototypes */
static int rawInput(void *pCustom);
static int test_token(void);

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
 * Use the block reader to read one or more tokens from standard input,
 * ending with the "|;" token.
 * 
 * For each token, report the line number and the token data.
 * 
 * If an error occurs, this function will report the specifics.
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int test_token(void) {
  
  int status = 1;
  long lx = 0;
  int errcode = 0;
  unsigned char *ptk = NULL;
  SHASM_IFLSTATE *ps = NULL;
  SHASM_BLOCK *pb = NULL;
  
  /* Allocate a new input filter chain on standard input */
  ps = shasm_input_alloc(&rawInput, NULL);
  
  /* Allocate a new block reader */
  pb = shasm_block_alloc();
  
  /* Read all the tokens */
  while (status) {
    
    /* Read the next token */
    if (!shasm_block_token(pb, ps)) {
      status = 0;
      errcode = shasm_block_status(pb, &lx);
      if (lx != LONG_MAX) {
        fprintf(stderr, "Error %d at line %ld!\n", errcode, lx);
      } else {
        fprintf(stderr, "Error %d at unknown line!\n", errcode);
      }
    }
    
    /* Get the token and line number */
    if (status) {
      ptk = shasm_block_ptr(pb, 1);
      if (ptk == NULL) {
        abort();  /* shouldn't happen as tokens never have null bytes */
      }
      lx = shasm_block_line(pb);
    }
    
    /* Report the token */
    if (status) {
      if (lx != LONG_MAX) {
        printf("@%ld: %s\n", lx, ptk);
      } else {
        printf("@???: %s\n", ptk);
      }
    }
    
    /* Break out of loop if token is "|;" */
    if (status) {
      if (strcmp(ptk, "|;") == 0) {
        break;
      }
    }
  }
  
  /* Free the block reader */
  shasm_block_free(pb);
  pb = NULL;
  
  /* Free the input filter chain */
  shasm_input_free(ps);
  ps = NULL;
  
  /* Return status */
  return status;
}

/*
 * Program entrypoint.
 * 
 * See the documentation at the top of this source file for a
 * description of how this testing program works.
 * 
 * The main function parses the command line and invokes the appropriate
 * testing function.
 * 
 * Parameters:
 * 
 *   argc - the number of elements in argv
 * 
 *   argv - the program arguments, where the first argument is the
 *   module name and any arguments beyond that are parameters passed on
 *   the command line; each argument must be null terminated
 * 
 * Return:
 * 
 *   zero if successful, one if error
 */
int main(int argc, char *argv[]) {
  
  int status = 1;
  
  /* Fail if less than one extra argument */
  if (argc < 2) {
    status = 0;
    fprintf(stderr,
      "Expecting a program argument choosing the testing mode!\n");
  }
  
  /* Verify that extra argument is present */
  if (status) {
    if (argv == NULL) {
      abort();
    }
    if (argv[1] == NULL) {
      abort();
    }
  }
  
  /* Invoke the appropriate testing function based on the command
   * line */
  if (status) {
    if (strcmp(argv[1], "token") == 0) {
      /* Token mode -- make sure no additional parameters */
      if (argc != 2) {
        status = 0;
        fprintf(stderr, "Too many parameters for token mode!\n");
      }
      
      /* Invoke token mode */
      if (status) {
        if (!test_token()) {
          status = 0;
        }
      }
      
    } else {
      /* Unrecognized mode */
      status = 0;
      fprintf(stderr, "Unrecognized testing mode!\n");
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
