#ifndef SHASTINA_H_INCLUDED
#define SHASTINA_H_INCLUDED

/*
 * shastina.h
 */

#include <limits.h>
#include <stddef.h>
#include <stdio.h>

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
#define SNERR_BADCHAR   (-8)  /* Illegal character encountered */
#define SNERR_LONGTOKEN (-9)  /* Token is too long */
#define SNERR_TRAILER   (-10) /* Content present after |; token */
#define SNERR_DEEPARRAY (-11) /* Too much array nesting */
#define SNERR_METANEST  (-12) /* Nested metacommands */
#define SNERR_SEMICOLON (-13) /* Semicolon outside of metacommand */
#define SNERR_DEEPGROUP (-14) /* Too much group nesting */
#define SNERR_RPAREN    (-15) /* Right parenthesis outside of group */
#define SNERR_RSQR      (-16) /* Right square bracket outside array */
#define SNERR_OPENGROUP (-17) /* Open group */
#define SNERR_LONGARRAY (-18) /* Array has too many elements */
#define SNERR_METAEMBED (-19) /* Embedded data in metacommand */
#define SNERR_OPENMETA  (-20) /* Unclosed metacommand */
#define SNERR_OPENARRAY (-21) /* Unclosed array */
#define SNERR_COMMA     (-22) /* Comma used outside of array or meta */

/*
 * The types of entities.
 */
#define SNENTITY_EOF          (0)   /* End Of File */
#define SNENTITY_STRING       (1)   /* String literal */
#define SNENTITY_EMBEDDED     (2)   /* Embedded data */
#define SNENTITY_BEGIN_META   (3)   /* Begin metacommand */
#define SNENTITY_END_META     (4)   /* End metacommand */
#define SNENTITY_META_TOKEN   (5)   /* Metacommand token */
#define SNENTITY_META_STRING  (6)   /* Metacommand string */
#define SNENTITY_NUMERIC      (7)   /* Numeric literal */
#define SNENTITY_VARIABLE     (8)   /* Declare variable */
#define SNENTITY_CONSTANT     (9)   /* Declare constant */
#define SNENTITY_ASSIGN       (10)  /* Assign value of variable */
#define SNENTITY_GET          (11)  /* Get variable or constant */
#define SNENTITY_BEGIN_GROUP  (12)  /* Begin group */
#define SNENTITY_END_GROUP    (13)  /* End group */
#define SNENTITY_ARRAY        (14)  /* Define array */
#define SNENTITY_OPERATION    (15)  /* Operation */

/*
 * The types of strings.
 */
#define SNSTRING_QUOTED (1) /* Double-quoted strings */
#define SNSTRING_CURLY  (2) /* Curly-bracketed strings */

/*
 * The SNPARSER structure prototype.
 * 
 * The actual structure definition is given in the implementation file.
 */
struct SNPARSER_TAG;
typedef struct SNPARSER_TAG SNPARSER;

/*
 * Structure for an entity read from a Shastina source file.
 */
typedef struct {
  
  /*
   * The status of the entity.
   * 
   * If this is zero or greater, it is one of the SNENTITY_ constants,
   * defining the kind of entity this is.
   * 
   * If this is negative, it is one of the SNERR_ constants, defining an
   * error condition.
   */
  int status;
  
  /*
   * Pointer to the null-terminated key string.
   * 
   * For OPERATION entities, this is the name of the operation.
   * 
   * For VARIABLE, CONSTANT, ASSIGN, and GET entities, this is the name
   * of the constant or variable.
   * 
   * For META_TOKEN entities, this is the token value.
   * 
   * For NUMERIC entities, this is the numeric value represented as a
   * string.  It is up to the clients to parse this as a number.
   * 
   * For EMBEDDED entities, this is the embedded data prefix, which does
   * not include the opening grave accent.
   * 
   * For STRING and META_STRING entities, this is the string prefix,
   * which does not include the opening quote or curly bracket.
   * 
   * For all other entities, this is set to NULL and ignored.
   * 
   * The pointer is valid until the next entity is read or the entity
   * reader is reset (whichever occurs first).  The client should not
   * modify the data at the pointer.
   */
  char *pKey;
  
  /*
   * Pointer to the null-terminated value string.
   * 
   * For STRING and META_STRING entities, this is the actual string
   * data, which does not include the opening and closing quotes or
   * brackets.
   * 
   * For all other entities, this is set to NULL and ignored.
   * 
   * The pointer is valid until the next entity is read or the entity
   * reader is reset (whichever occurs first).  The client should not
   * modify the data at the pointer.
   */
  char *pValue;
  
  /*
   * The string type.
   * 
   * For STRING and META_STRING entities, this is one of the SNSTRING_
   * constants, which defines whether the string is a quoted string or a
   * curly-bracket string.
   * 
   * For all other entities, this is set to zero and ignored.
   */
  int str_type;
  
  /*
   * The count value.
   * 
   * For ARRAY entities, this is the count of the number of array
   * elements.  Its range is zero up to and including LONG_MAX.
   * 
   * For all other entities, this is set to zero and ignored.
   */
  long count;
  
} SNENTITY;

/*
 * Allocate a new Shastina parser.
 * 
 * The parser must eventually be freed with snparser_free().
 * 
 * Return:
 * 
 *   a new Shastina parser
 */
SNPARSER *snparser_alloc(void);

/*
 * Free a Shastina parser.
 * 
 * This call is ignored if NULL is passed.
 * 
 * The parser object must not be used again after freeing it.
 * 
 * Parameters:
 * 
 *   pParser - the parser object to free
 */
void snparser_free(SNPARSER *pParser);

/*
 * Parse an entity from a Shastina source file.
 * 
 * pParser is the parser object.
 * 
 * pEntity is the entity structure that will be filled in.  Its state
 * upon entry to the function is irrelevant.  Upon return, it will hold
 * the results of the operation.  See the structure documentation for
 * further information.
 * 
 * pIn is the input file to read from.  It must be open for reading.
 * Reading is sequential.
 * 
 * Once an error is encountered, the parser object will return that same
 * error each time this function is called without doing anything
 * further.
 * 
 * Once the End Of File (EOF) entity is returned, this function will
 * return the EOF entity on all subsequent calls without doing anything
 * further.
 * 
 * Parameters:
 * 
 *   pReader - the reader object
 * 
 *   pEntity - pointer to the entity to receive the results
 * 
 *   pIn - the input file
 */
void snparser_read(SNPARSER *pParser, SNENTITY *pEntity, FILE *pIn);

/*
 * Return the current line count.
 * 
 * The line count is always at least one and at most LONG_MAX.  The
 * value of LONG_MAX is an overflow value, so any count of lines above
 * that will just remain at LONG_MAX.
 * 
 * Parameters:
 * 
 *   pParser - the parser object
 * 
 * Return:
 * 
 *   the current line count
 */
long snparser_count(SNPARSER *pParser);

/*
 * Check whether a UTF-8 Byte Order Mark was present at the start of
 * input.
 * 
 * This is only meaningful after the first entity has been read with a
 * call to snparser_read().  Before the first call to the read function,
 * the BOM flag is always clear because the file hasn't been read yet.
 * 
 * Parameter:
 * 
 *   pParser - the parser object
 * 
 * Return:
 * 
 *   non-zero if UTF-8 BOM was present, zero if not
 */
int snparser_bomflag(SNPARSER *pParser);

/*
 * Convert a Shastina SNERR_ error code into a string.
 * 
 * The string has the first letter capitalized, but no punctuation or
 * line break at the end.
 * 
 * If the code is not one of the recognized SNERR_ codes, then the
 * message "Unknown error" is returned.
 * 
 * The returned string is statically allocated, and should not be freed
 * by the client.
 * 
 * Parameters:
 * 
 *   code - the error code
 * 
 * Return:
 * 
 *   an error message
 */
const char *snerror_str(int code);

#endif
