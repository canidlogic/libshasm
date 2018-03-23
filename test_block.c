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
 * string
 * ------
 * 
 * Regular string testing mode is invoked as follows:
 * 
 *   test_block string [type] [outover]
 * 
 * Both the [type] and [outover] parameters are required.
 * 
 * The [type] parameter must be either "q" "a" or "c" (case sensitive)
 * for double-quoted strings "", apostrophe-quoted strings '', or curly
 * bracket strings {}, respectively.
 * 
 * The [outover] parameter must be either "none", "utf8", "cesu8",
 * "utf16le", "utf16be", "utf32le", or "utf32be" to select one of the
 * output overrides or specify that no output override is in effect.  If
 * an output override is selected, it will be in strict mode.
 * 
 * The program reads string data from standard input beginning
 * immediately with the first byte.  This does not include the opening
 * quote or curly bracket (which would come at the end of a token
 * introducing the string data rather than being part of the string
 * data), but it must include the closing quote or curly bracket.  Zero
 * or more additional bytes may follow the string data.  However,
 * nothing may precede the string data in input.
 * 
 * The program reports the resulting string that was read from the
 * string data, with escapes of the form "<0a>" used for characters
 * outside of US-ASCII printing range (0x21-0x7e).  The program also
 * reports any additional bytes that were read after the string data,
 * using the same escaping system for byte values outside of printing
 * range.  If there was an error, the program reports it.
 * 
 * The parameters passed on the command line do not fully specify all of
 * the details of the string type.  Some of the parameters are hardwired
 * into the testing program.  The remainder of this testing mode
 * documentation specifies the hardwired testing parameters.
 * 
 * The hardwired decoding map is as follows.  All printing US-ASCII
 * characters (0x21-0x7e) except for backslash (0x5c), ampersand (0x26),
 * and asterisk (0x2a) have a decoding map key consisting just of that
 * character, mapping the character to an entity value of the same
 * numeric value as the ASCII code.  ASCII Space (0x20) and Line Feed
 * (0x0a) also have a decoding map key consisting just of that
 * character, mapping the character to an entity value of the same
 * numeric value as the ASCII code.
 * 
 * There are a set of decoding map keys beginning with the backslash
 * character, representing escapes.  They are:
 * 
 *   \\     - literal backslash
 *   \&     - literal ampersand
 *   \"     - literal double quote
 *   \'     - literal apostrophe
 *   \{     - literal opening curly bracket
 *   \}     - literal closing curly bracket
 *   \n     - literal LF
 *   \<LF>  - line continuation (see below)
 *   \:a    - lowercase a-umlaut
 *   \:A    - uppercase a-umlaut
 *   \:o    - lowercase o-umlaut
 *   \:O    - uppercase o-umlaut
 *   \:u    - lowercase u-umlaut
 *   \:U    - uppercase u-umlaut
 *   \ss    - German eszett
 *   \u#### - Unicode codepoint (see below)
 * 
 * The line continuation escape is a backslash as the last character of
 * a line in the string data.  This maps to the entity code
 * corresponding to ASCII space.  It allows for line breaks within
 * string data that only appear as a single space in the output string.
 * 
 * The Unicode codepoint escape has four to six base-16 digits (####)
 * that represent the numeric value of a Unicode codepoint, with
 * surrogates disallowed but supplemental characters okay.  The opening
 * "u" is case sensitive, but the base-16 digits can be either uppercase
 * or lowercase.  This is handled as a numeric escape.
 * 
 * All of the other backslash escapes map to an entity code matching the
 * Unicode/ASCII codepoint of the character they represent.
 * 
 * There are also a set of decoding map keys beginning with the
 * ampersand character, representing escapes.  They are:
 * 
 *   &amp;  - literal ampersand
 *   &###;  - decimal Unicode codepoint (see below)
 *   &x###; - base-16 Unicode codepoint (see below)
 * 
 * The ampersand escape maps to an entity code matching the ASCII code
 * for an ampersand character.
 * 
 * The decimal codepoint escape has one or more decimal digits (###)
 * terminated with a semicolon, representing the Unicode codepoint, with
 * surrogates disallowed but supplemental characters okay.
 * 
 * The base-16 codepoint escape has one or more base-16 digits (###)
 * terminated with a semicolon, representing the Unicode codepoint, with
 * surrogates disallowed but supplemental characters okay.
 * 
 * Finally, there are a set of decoding map keys beginning with the
 * asterisk.  They are:
 * 
 *   **                              - literal asterisk
 *   *                               - special key #1
 *   *hello                          - special key #2
 *   *helloWorld                     - special key #3
 *   *helloEvery                     - special key #4
 *   *helloEveryone                  - special key #5
 *   *helloEveryoneOut               - special key #6
 *   *helloEveryoneOutThere          - special key #7
 *   *helloEveryoneOutThereSome      - special key #8
 *   *helloEveryoneOutThereSomewhere - special key #9
 * 
 * The literal asterisk key maps to an entity matching the ASCII code
 * for an asterisk.  Special keys 1-9 map to special entity codes
 * outside of Unicode range.
 * 
 * The numeric escape list string format parameter is defined for the
 * ampersand and backslash numeric escapes described above.
 * 
 * The encoding table is defined such that all entity codes
 * corresponding printing to US-ASCII characters (0x21-0x7e) generate
 * the equivalent ASCII codes, except that uppercase letters map to
 * lowercase letters (thereby making all characters lowercase), and the
 * tilde character is not defined (meaning it gets dropped from output).
 * ASCII Space (0x20) and Line Feed (0x0a) are also defined, and map to
 * their equivalent ASCII codes.  The umlaut characters and eszett
 * character defined in the backslash escapes are defined and map to
 * their 8-bit ISO 8859-1 values.  Finally, the special keys are defined
 * such that they yield a sequence of :-) emoticons with the number of
 * emoticons in the sequence equal to the number of the special key.
 * 
 * If a UTF-16 or UTF-32 output override is in effect, each character in
 * the special key definitions has padding zeros added in the
 * appropriate place to make the ASCII characters work in the desired
 * encoding scheme.
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
