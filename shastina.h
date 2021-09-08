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
 * These must all be negative and should be distinct.
 */
#define SNERR_IOERR     (-1)  /* I/O error */
#define SNERR_EOF       (-2)  /* End Of File */
#define SNERR_BADCR     (-3)  /* CR character not followed by LF */
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
#define SNERR_UNPAIRED  (-19) /* Unpaired surrogates encountered */
#define SNERR_OPENMETA  (-20) /* Unclosed metacommand */
#define SNERR_OPENARRAY (-21) /* Unclosed array */
#define SNERR_COMMA     (-22) /* Comma used outside of array or meta */
#define SNERR_UTF8      (-23) /* Invalid UTF-8 in input */

/*
 * Flags for use with snsource_stream().
 * 
 * SNSTREAM_NORMAL has a value of zero, meaning no special flags set.
 * The other flags can be combined with bitwise OR.
 * 
 * If OWNER flag is set, then the file handle will be closed when the
 * source is closed.  Otherwise, the file handle remains owned by the
 * caller and will not be closed by the stream object.  Do not use OWNER
 * when wrapping stdin in a stream!
 * 
 * If RANDOM flag is set, then the file handle supports random access,
 * so multiplass operation is enabled with file I/O.  Do not use RANDOM
 * with stdin!
 */
#define SNSTREAM_NORMAL   (0)
#define SNSTREAM_OWNER    (1)
#define SNSTREAM_RANDOM   (2)

/*
 * The types of entities.
 */
#define SNENTITY_EOF          (0)   /* End Of File */
#define SNENTITY_STRING       (1)   /* String literal */
#define SNENTITY_BEGIN_META   (2)   /* Begin metacommand */
#define SNENTITY_END_META     (3)   /* End metacommand */
#define SNENTITY_META_TOKEN   (4)   /* Metacommand token */
#define SNENTITY_META_STRING  (5)   /* Metacommand string */
#define SNENTITY_NUMERIC      (6)   /* Numeric literal */
#define SNENTITY_VARIABLE     (7)   /* Declare variable */
#define SNENTITY_CONSTANT     (8)   /* Declare constant */
#define SNENTITY_ASSIGN       (9)   /* Assign value of variable */
#define SNENTITY_GET          (10)  /* Get variable or constant */
#define SNENTITY_BEGIN_GROUP  (11)  /* Begin group */
#define SNENTITY_END_GROUP    (12)  /* End group */
#define SNENTITY_ARRAY        (13)  /* Define array */
#define SNENTITY_OPERATION    (14)  /* Operation */

/*
 * The types of strings.
 */
#define SNSTRING_QUOTED (1) /* Double-quoted strings */
#define SNSTRING_CURLY  (2) /* Curly-bracketed strings */

/*
 * The SNSOURCE structure prototype.
 * 
 * The actual structure definition is given in the implementation file.
 */
struct SNSOURCE_TAG;
typedef struct SNSOURCE_TAG SNSOURCE;

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
 * Simple wrapper around snsource_stream().
 * 
 * If owner is non-zero, then the flags will be SNSTREAM_OWNER.  If the
 * owner is zero, then the flags will be SNSTREAM_NORMAL (zero).
 * 
 * Parameters:
 * 
 *   pFile - the file handle to wrap
 * 
 *   flags - combination of SNSTREAM flags
 * 
 * Return:
 * 
 *   a new Shastina source wrapping the file handle
 */
SNSOURCE *snsource_file(FILE *pFile, int owner);

/*
 * Allocate a Shastina source that wraps a stdio FILE handle.
 * 
 * pFile is the file handle to wrap.  It must be open for reading or
 * undefined behavior occurs.  Nothing besides the allocated Shastina
 * source should use the file while the source object is allocated or
 * undefined behavior occurs.
 * 
 * flags is a combination of SNSTREAM flags, or SNSTREAM_NORMAL (zero)
 * if no flags are required.  See the documentation of the SNSTREAM
 * constants for further information.  Unrecognized flags are ignored.
 * 
 * Calls to read from the Shastina source will read from the underlying
 * file handle.  If the RANDOM flag was specified, then multipass
 * support will be enabled.  Otherwise, reading is fully sequential and
 * no multipass operation is supported.
 * 
 * Multipass sources will immediately be rewound during construction.
 * 
 * The returned source object should eventually be freed with
 * snsource_free().
 * 
 * If the whole Shastina file is interpreted successfully, then after
 * reading the |; EOF token, the underlying file handle will be
 * positioned to read the byte immediately following the EOF token, and
 * snsource_bytes() will have how many bytes total were read up to and
 * including the semicolon in the |; EOF token.
 * 
 * If you want to make sure nothing besides whitespace and blank lines
 * remains in the file after the |; EOF token, use snsource_consume().
 * 
 * Parameters:
 * 
 *   pFile - the file handle to wrap
 * 
 *   flags - combination of SNSTREAM flags
 * 
 * Return:
 * 
 *   a new Shastina source wrapping the file handle
 */
SNSOURCE *snsource_stream(FILE *pFile, int flags);

/*
 * Allocate a Shastina source that wraps a nul-terminated string.
 * 
 * pStr is a pointer to the nul-terminated string to wrap.  It must
 * remain allocated and it must not be changed while the Shastina source
 * is allocated or undefined behavior occurs.
 * 
 * The terminating nul character is interpreted as the "end of file" and
 * is not included in the bytes that are read through the source.
 * 
 * Calls to read from the Shastina source will read bytes from the given
 * string.  String sources have full support for multipass.
 * 
 * The returned source object should eventually be freed with
 * snsource_free().
 * 
 * No destructor routine is registered, so you may safely pass static
 * string data.  Note, however, that if you pass a dynamically allocated
 * string, it will NOT be automatically freed when the Shastina source
 * is closed.
 * 
 * If the whole Shastina file is interpreted successfully, then after
 * reading the |; EOF token, snsource_bytes() will have how many bytes
 * total were read up to and including the semicolon in the |; EOF
 * token.
 * 
 * If you want to make sure nothing besides whitespace and blank lines
 * remains in the string after the |; EOF token, use snsource_consume().
 * 
 * Parameters:
 * 
 *   pStr - the nul-terminated string to wrap
 * 
 * Return:
 * 
 *   a new Shastina source wrapping the string
 */
SNSOURCE *snsource_string(const char *pStr);

/*
 * Allocate a custom Shastina source.
 * 
 * This is the most flexible Shastina source constructor, which allows
 * you full control over how the Shastina source works.  The other
 * snsource_file() and snsource_string() constructors are much easier to
 * use, but more limited.
 * 
 * read_func is a function pointer to a callback function.  It may not
 * be NULL.  The void pointer it takes will always be the same as the
 * custom parameter passed to this constructor function.  The read
 * function should return the unsigned byte value (0-255) of the next
 * byte from the input source, or SNERR_EOF if End Of File (EOF) has
 * been reached, or SNERR_IOERR if there was an I/O error reading the
 * input source.
 * 
 * Once the callback function has returned SNERR_EOF or SNERR_IOERR, it
 * will not be called again.  However, multipass sources can clear the
 * SNERR_EOF condition by rewinding.
 * 
 * free_func is a function pointer to a destructor function, or NULL if
 * no destructor function is required.
 * 
 * If free_func is not NULL, then it will be called immediately before
 * releasing the Shastina source object, with the void pointer set to
 * the custom parameter passed to this constructor function.  The
 * destructor may be used for freeing whatever is pointed to by the
 * custom parameter.  If this is not needed, use NULL for the
 * destructor.
 * 
 * rewind_func is a function pointer to a multipass rewinding function,
 * or NULL if the source does not support multipass.  The void pointer
 * it takes will always be the same as the custom parameter passed to
 * this constructor function.  It should return non-zero if successful
 * or zero if an I/O error prevented rewinding.  Failure to rewind
 * results in SNERR_IOERR state for the source.
 * 
 * If rewind_func is provided, it will be called during this constructor
 * routine to make sure the source starts at the beginning.  If it
 * fails, the constructed source will start out in SNERR_IOERR state.
 * 
 * custom is a custom parameter that is passed through to both the read
 * callback and the destructor function (if the destructor is defined).
 * custom may be anything, including NULL.
 * 
 * The returned source object should eventually be freed with
 * snsource_free().
 * 
 * Parameters:
 * 
 *   read_func - the read callback
 * 
 *   free_func - the destructor callback, or NULL
 * 
 *   rewind_func - the multipass rewind callback, or NULL
 * 
 *   custom - the custom data, which may be NULL
 * 
 * Return:
 * 
 *   a new, custom Shastina source
 */
SNSOURCE *snsource_custom(
    int (*read_func)(void *),
    void (*free_func)(void *),
    int (*rewind_func)(void *),
    void *custom);

/*
 * Free a Shastina source.
 * 
 * This call is ignored if NULL is passed.
 * 
 * The source object must not be used again after freeing it.
 * 
 * This call will also run any registered destructor routine within the
 * source structure.
 * 
 * Parameters:
 * 
 *   pSrc - the source object to free or NULL
 */
void snsource_free(SNSOURCE *pSrc);

/*
 * Determine how many bytes have been successfully read through the
 * source.
 * 
 * If the total number of bytes exceeds the range of a long, then
 * LONG_MAX will be returned.
 * 
 * This count does NOT include any SNERR_EOF or SNERR_IOERR codes that
 * were returned from the source.  It only includes actual bytes that
 * were read through the source.
 * 
 * After successfully reading the |; entity, this count will be total
 * number of bytes from the start of the file up to and including the
 * semicolon in the |; entity.
 * 
 * For multipass sources, rewinding will reset this counter to zero.
 * 
 * Parameters:
 * 
 *   pSrc - the source object to query
 * 
 * Return:
 * 
 *   the number of bytes read through the source, or LONG_MAX if this
 *   count exceeds the limit of a long
 */
long snsource_bytes(SNSOURCE *pSrc);

/*
 * Consume the rest of the data in a source and make sure that there is
 * nothing but whitespace and blank lines.
 * 
 * pSrc is the source object to consume.
 * 
 * This function will keep reading from the source until one of the
 * following happens:
 * 
 *   (1) Something other than SP HT CR LF is read
 *   (2) SNERR_EOF is encountered
 *   (3) SNERR_IOERR is encountered
 *   (4) Some other error is encountered
 * 
 * The function succeeds and returns greater than zero in case (2),
 * which indicates that only whitespace and blank lines remained in the
 * file.
 * 
 * The function fails and returns SNERR_TRAILER in case (1) or (4), or
 * the error code SNERR_IOERR in case (3).
 * 
 * Do not use this function in the middle of parsing a Shastina file
 * with the source or undefined behavior occurs.
 * 
 * This function is intended for cases where nothing should follow the
 * |; EOF marker in the Shastina file.  Parsing stops at the semicolon
 * in the |; EOF marker without reading any further than that.  This
 * function can then make sure that nothing besides whitespace and blank
 * lines remains after the |; EOF marker.
 * 
 * Note that after you use this function, snsource_bytes() will be
 * changed to include the number of additional bytes consumed by this
 * function.  You can therefore no longer use snsource_bytes() to count
 * the number of bytes up to the |; EOF marker after calling this
 * function.
 * 
 * You may call this on a source that has already reached EOF, in which
 * case this function will just return greater than zero.
 * 
 * Parameters:
 * 
 *   pSrc - the Shastina source object
 * 
 * Return:
 * 
 *   greater than zero if all remaining data has been read from the
 *   source and nothing besides whitespace and blank lines remained,
 *   SNERR_TRAILER if something besides whitespace and blank lines was
 *   encountered, SNERR_IOERR if there was an I/O error reading from the
 *   source object
 */
int snsource_consume(SNSOURCE *pSrc);

/*
 * Determine whether a given Shastina source supports multipass
 * operation.
 * 
 * Multipass sources can be rewound at any time to start reading again
 * from the beginning.  Single-pass sources can only be read through
 * once.
 * 
 * Parameters:
 * 
 *   pSrc - the Shastina source object
 * 
 * Return:
 * 
 *   non-zero if source supports multipass, zero if not
 */
int snsource_ismulti(SNSOURCE *pSrc);

/*
 * Rewind a Shastina source back to the beginning so it can be read
 * again.
 * 
 * This is only supported by multipass sources.  A fault occurs if it is
 * called on a single-pass source.  Use snsource_ismulti() to check
 * whether a source supports this function.
 * 
 * If you are rewinding, you will probably need to allocate a new parser
 * object to parse the file in the new pass.
 * 
 * Parameters:
 * 
 *   pSrc - the Shastina source object
 * 
 * Return:
 * 
 *   non-zero if successful, zero if an I/O error prevented a successful
 *   rewind
 */
int snsource_rewind(SNSOURCE *pSrc);

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
 *   pParser - the parser object to free or NULL
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
 * pIn is the input source to read from.
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
 *   pIn - the input source
 */
void snparser_read(
    SNPARSER * pParser,
    SNENTITY * pEntity,
    SNSOURCE * pIn);

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
