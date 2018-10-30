/*
 * shastina.c
 */

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* @@TODO: replace string constants with ASCII constants */

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
#define ASCII_PLUS      (0x2b)  /* + */
#define ASCII_COMMA     (0x2c)  /* , */
#define ASCII_HYPHEN    (0x2d)  /* - */
#define ASCII_ZERO      (0x30)  /* 0 */
#define ASCII_NINE      (0x39)  /* 9 */
#define ASCII_COLON     (0x3a)  /* : */
#define ASCII_SEMICOLON (0x3b)  /* ; */
#define ASCII_EQUALS    (0x3d)  /* = */
#define ASCII_QUESTION  (0x3f)  /* ? */
#define ASCII_ATSIGN    (0x40)  /* @ */
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
 * The types of tokens.
 */
#define SNTOKEN_FINAL  (0)  /* The final |; token */
#define SNTOKEN_SIMPLE (1)  /* Simple tokens, except |; */
#define SNTOKEN_STRING (2)  /* Quoted and curly string tokens */
#define SNTOKEN_EMBED  (3)  /* Embedded tokens */

/*
 * The types of strings.
 */
#define SNSTRING_QUOTED (1) /* Double-quoted strings */
#define SNSTRING_CURLY  (2) /* Curly-bracketed strings */

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
 * The maximum number of queued entities.
 * 
 * This has nothing to do with the total number of entities in a source
 * file.  Rather, it means no more than this many entities can result
 * from processing a single token.
 */
#define SNREADER_MAXQUEUE (8)

/* @@TODO: */
#define SNREADER_KEY_INIT (16)
#define SNREADER_KEY_MAX  (65535)
#define SNREADER_VAL_INIT (256)
#define SNREADER_VAL_MAX  (65535)
#define SNREADER_ARRAY_INIT (8)
#define SNREADER_ARRAY_MAX (1024)
#define SNREADER_GROUP_INIT (8)
#define SNREADER_GROUP_MAX (1024)

/*
 * Structure for storing state of Shastina numeric stacks.
 * 
 * Use the snstack_ functions to manipulate this structure.
 */
typedef struct {
  
  /*
   * Pointer to the buffer.
   * 
   * This pointer is NULL if cap is zero.
   */
  long *pBuf;
  
  /*
   * The number of longs stored in the buffer.
   * 
   * This may not exceed cap.
   */
  long count;
  
  /*
   * The current capacity of the buffer in longs.
   * 
   * If zero, it means that no buffer has been allocated yet.
   */
  long cap;
  
  /*
   * The initial allocation capacity for this buffer in longs.
   * 
   * This must be greater than zero and no greater than maxcap.
   */
  long initcap;
  
  /*
   * The maximum capacity for this buffer in longs.
   * 
   * This must be greater than or equal to initcap.  It may not exceed
   * (LONG_MAX / 2).
   */
  long maxcap;
  
} SNSTACK;

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

/*
 * Structure for a token read from a Shastina source file.
 * 
 * This covers everything that can be found in a Shastina source file,
 * except for embedded data.
 */
typedef struct {
  
  /*
   * The status of a token structure.
   * 
   * This is either one of the SNTOKEN_ constants, or it is negative,
   * indicating an SNERR_ error.
   */
  int status;
  
  /*
   * The string type flag.
   * 
   * For STRING token types, this will be set to one of the SNSTRING_
   * constants to indicate the string type.  Otherwise, it should be
   * ignored.
   */
  int str_type;
  
  /*
   * Pointer to the key buffer.
   * 
   * This will always be reset on read operations.  For SIMPLE and FINAL
   * token types, this buffer will contain the full token on return.
   * For STRING and EMBED token types, this buffer will contain the
   * prefix (excluding the opening quote, bracket, or accent) on return.
   * 
   * The state of the buffer is undefined on error status returns.
   * 
   * The key and value buffer may not point to the same object.
   */
  SNBUFFER *pKey;
  
  /*
   * Pointer to the value buffer.
   * 
   * This will always be reset on read operations.  For STRING token
   * types, this buffer will contain the string data on return.  For
   * other token types, this buffer will be empty on return.
   * 
   * The state of the buffer is undefined on error status returns.
   * 
   * The key and value buffer may not point to the same object.
   */
  SNBUFFER *pValue;
  
} SNTOKEN;

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
 * Structure for storing state of the Shastina source file reader.
 * 
 * Use the snreader_ functions to manipulate this structure.
 */
typedef struct {
  
  /*
   * The status of the reader.
   * 
   * This is normally zero.  If negative, it is one of the SNERR_
   * constants, indicating that an error has occurred.
   */
  int status;
  
  /*
   * The queue of entities.
   * 
   * The queue_count field determines how many entities are queued.
   * 
   * CAUTION:  Do not queue more than one entity that makes use of the
   * string buffers, or there will be dangling pointers!
   */
  SNENTITY queue[SNREADER_MAXQUEUE];
  
  /*
   * The number of queued entities.
   * 
   * This has range zero up to SNREADER_MAXQUEUE.
   */
  int queue_count;

  /*
   * The number of queued entities that have been read.
   *
   * This has range zero up to (queue_count - 1), except when
   * queue_count is zero, in which case queue_read is also zero.
   */
  int queue_read;

  /*
   * The buffer for key strings.
   * 
   * This needs to be properly initialized.  It also needs to be fully
   * reset before the structure is released.
   */
  SNBUFFER buf_key;
  
  /*
   * The buffer for value strings.
   * 
   * This needs to be properly initialized.  It also needs to be fully
   * reset before the structure is released.
   */
  SNBUFFER buf_value;
  
  /*
   * The array stack.
   * 
   * This needs to be properly initialized.  It also needs to be fully
   * reset before the structure is released.
   */
  SNSTACK stack_array;
  
  /*
   * The group stack.
   * 
   * This needs to be properly initialized.  It also needs to be fully
   * reset before the structure is released.
   */
  SNSTACK stack_group;
  
  /*
   * The metacommand flag.
   * 
   * This is non-zero if currently in a metacommand, zero otherwise.
   */
  int meta_flag;
  
  /*
   * The array flag.
   * 
   * This is non-zero if a new array was just opened, zero otherwise.
   */
  int array_flag;
  
} SNREADER;

/* Function prototypes */
static void snstack_init(SNSTACK *pStack, long icap, long maxcap);
static void snstack_reset(SNSTACK *pStack, int full);
static int snstack_push(SNSTACK *pStack, long v);
static long snstack_pop(SNSTACK *pStack);
static long snstack_peek(SNSTACK *pStack);
static int snstack_inc(SNSTACK *pStack);
static int snstack_dec(SNSTACK *pStack);
static long snstack_count(SNSTACK *pStack);

static void snbuffer_init(SNBUFFER *pBuffer, long icap, long maxcap);
static void snbuffer_reset(SNBUFFER *pBuffer, int full);
static int snbuffer_append(SNBUFFER *pBuffer, int c);
static char *snbuffer_get(SNBUFFER *pBuffer);
static long snbuffer_count(SNBUFFER *pBuffer);
static int snbuffer_last(SNBUFFER *pBuffer);
static int snbuffer_less(SNBUFFER *pBuffer);

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
static int sntk_readToken(
    SNBUFFER * pBuffer,
    FILE     * pIn,
    SNFILTER * pFilter);

static void sntoken_read(SNTOKEN *pToken, FILE *pIn, SNFILTER *pFil);

static void snreader_init(SNREADER *pReader);
static void snreader_reset(SNREADER *pReader, int full);
static void snreader_read(
    SNREADER * pReader,
    SNENTITY * pEntity,
    FILE     * pIn,
    SNFILTER * pFilter);

static void snreader_addEntityZ(SNREADER *pReader, int entity);
static void snreader_addEntityS(
    SNREADER * pReader,
    int        entity,
    char     * s);
static void snreader_addEntityL(
    SNREADER * pReader,
    int        entity,
    long       l);
static void snreader_addEntityT(
    SNREADER * pReader,
    int        entity,
    char     * pPrefix,
    int        str_type,
    char     * pData);

static void snreader_arrayPrefix(SNREADER *pReader);
static void snreader_fill(
    SNREADER * pReader,
    FILE     * pIn,
    SNFILTER * pFilter);

/*
 * Initialize a long stack.
 * 
 * Stacks must be initialized before they are used, or undefined 
 * behavior occurs.  A full reset must be performed on the stack using
 * snstack_reset() before the structure is released or a memory leak may
 * occur.
 * 
 * icap is the initial allocation capacity in longs.  maxcap is the 
 * maximum capacity for the buffer (in longs).
 * 
 * icap must be greater than zero, maxcap must be greater than or equal
 * to icap, and maxcap must be no greater than (LONG_MAX / 2) or a fault
 * will occur.
 * 
 * If (sizeof(size_t) < 2), this function will always fault.  Otherwise,
 * if (sizeof(size_t) < 4), this function will fault if maxcap
 * multiplied by sizeof(long) exceeds 65535.  Otherwise, it must be that
 * (sizeof(size_t) >= 4) and this function will fault if maxcap times
 * sizeof(long) exceeds 2147483647.  This is to prevent memory
 * allocation problems.
 * 
 * Do not initialize a stack that is already initialized, or a memory
 * leak may occur.
 * 
 * Parameters:
 * 
 *   pBuffer - pointer to the stack to initialize
 * 
 *   icap - the initial allocation capacity in longs
 * 
 *   maxcap - the maximum allocation capacity in longs
 */
static void snstack_init(SNSTACK *pStack, long icap, long maxcap) {
  
  /* Check parameters */
  if (pStack == NULL) {
    abort();
  }
  
  if ((icap <= 0) || (maxcap < icap) || (maxcap > (LONG_MAX / 2))) {
    abort();
  }
  
  if (sizeof(size_t) < 2) {
    abort();
  } else if (sizeof(size_t) < 4) {
    if (maxcap > (65535L / sizeof(long))) {
      abort();
    }
  } else {
    if (maxcap > (2147483647L / sizeof(long))) {
      abort();
    }
  }
  
  /* Initialize structure */
  memset(pStack, 0, sizeof(SNSTACK));
  pStack->pBuf = NULL;
  pStack->count = 0;
  pStack->cap = 0;
  pStack->initcap = icap;
  pStack->maxcap = maxcap;
}

/*
 * Reset a stack back to empty.
 * 
 * The stack must already have been initialized with snstack_init() or
 * undefined behavior occurs.
 * 
 * full is non-zero to perform a full reset, zero to perform a fast
 * reset.  A fast reset just clears the buffer to empty without
 * releasing the allocated memory buffer, allowing it to be reused.  A
 * full reset also releases the allocated memory buffer, clearing the
 * structure all the way back to its initial state.
 * 
 * A full reset must be performed on all stacks before they are 
 * released, or a memory leak may occur.
 * 
 * Parameters:
 * 
 *   pStack - the stack to reset
 * 
 *   full - non-zero for a full reset, zero for a partial reset
 */
static void snstack_reset(SNSTACK *pStack, int full) {
  
  /* Check parameters */
  if (pStack == NULL) {
    abort();
  }
  
  /* Set the count back to zero */
  pStack->count = 0;
  
  /* If a buffer is allocated, clear it all to zero */
  if (pStack->cap > 0) {
    memset(pStack->pBuf, 0, (size_t) (pStack->cap * sizeof(long)));
  }
  
  /* If we're doing a full reset, release the buffer if allocated and
   * reset capacity back to zero */
  if (full && (pStack->cap > 0)) {
    free(pStack->pBuf);
    pStack->pBuf = NULL;
    pStack->cap = 0;
  }
}

/*
 * Push a long on top of a stack.
 * 
 * The long value v may have any value.
 * 
 * The function fails if there is no more capacity left for another
 * long.  The buffer is unmodified in this case.
 * 
 * Parameters:
 * 
 *   pStack - the stack to push a long onto
 * 
 *   v - the long value to push
 * 
 * Return:
 * 
 *   non-zero if successful, zero if no more capacity
 */
static int snstack_push(SNSTACK *pStack, long v) {
  
  int status = 1;
  long newcap = 0;
  
  /* Check parameters */
  if (pStack == NULL) {
    abort();
  }
  
  /* Proceed only if we are not completely maxed out of capacity */
  if (pStack->count < pStack->maxcap) {
    /* We have capacity left; first, make the initial allocation if we
     * haven't allocated a memory buffer yet */
    if (pStack->cap < 1) {
      pStack->pBuf = (long *) malloc(
                        (size_t) (pStack->initcap * sizeof(long)));
      if (pStack->pBuf == NULL) {
        abort();
      }
      memset(pStack->pBuf, 0,
        (size_t) (pStack->initcap * sizeof(long)));
      pStack->cap = pStack->initcap;
    }
    
    /* Next, increase allocated memory buffer if we need more space */
    if (pStack->count >= pStack->cap) {
      /* New capacity should usually be double current capacity */
      newcap = pStack->cap * 2;
      
      /* If new capacity exceeds max capacity, set to max capacity */
      if (newcap > pStack->maxcap) {
        newcap = pStack->maxcap;
      }
      
      /* Allocate new buffer */
      pStack->pBuf = (long *) realloc(pStack->pBuf,
                                (size_t) (newcap * sizeof(long)));
      if (pStack->pBuf == NULL) {
        abort();
      }
      
      /* Initialize new space to zero */
      memset((void *) (pStack->pBuf + pStack->cap),
              0,
              (size_t) ((newcap - pStack->cap) * sizeof(long)));
      
      /* Update capacity */
      pStack->cap = newcap;
    }
    
    /* Append the new long */
    (pStack->pBuf)[pStack->count] = v;
    (pStack->count)++;
    
  } else {
    /* Out of capacity */
    status = 0;
  }
  
  /* Return status */
  return status;
}

/*
 * Pop a long value off the top of a stack.
 * 
 * pStack is the stack object to a pop a value off of.  It must be
 * properly initialized.  It must not be empty, or a fault will occur.
 * 
 * Parameters:
 * 
 *   pStack - the stack
 * 
 * Return:
 * 
 *   the popped long value
 */
static long snstack_pop(SNSTACK *pStack) {
  
  long v = 0;
  
  /* Check parameter */
  if (pStack == NULL) {
    abort();
  }
  
  /* Check state */
  if (pStack->count < 1) {
    abort();
  }
  
  /* Get the top of the stack */
  v = pStack->pBuf[pStack->count - 1];
  
  /* Take the top element off the stack */
  (pStack->count)--;
  
  /* Return the removed value */
  return v;
}

/*
 * Peek at the long value on top of a stack without removing it.
 * 
 * pStack is the stack object to peek at.  It must be properly
 * initialized.  It must not be empty, or a fault will occur.
 * 
 * Parameters:
 * 
 *   pStack - the stack
 * 
 * Return:
 * 
 *   the peeked long value
 */
static long snstack_peek(SNSTACK *pStack) {
  
  /* Check parameter */
  if (pStack == NULL) {
    abort();
  }
  
  /* Check state */
  if (pStack->count < 1) {
    abort();
  }
  
  /* Return the top of the stack */
  return pStack->pBuf[pStack->count - 1];
}

/*
 * Increment the long value on top of a stack.
 * 
 * pStack is the stack object.  It must be properly initialized.  It
 * must not be empty, or a fault will occur.
 * 
 * The function fails if the top of the stack has value LONG_MAX.
 * 
 * Parameters:
 * 
 *   pStack - the stack
 * 
 * Return:
 * 
 *   non-zero if successful, zero if long value already at maximum
 */
static int snstack_inc(SNSTACK *pStack) {
  
  int status = 1;
  
  /* Check parameter */
  if (pStack == NULL) {
    abort();
  }
  
  /* Check state */
  if (pStack->count < 1) {
    abort();
  }
  
  /* Increment if possible */
  if (pStack->pBuf[pStack->count - 1] < LONG_MAX) {
    (pStack->pBuf[pStack->count - 1])++;
  } else {
    status = 0;
  }
  
  /* Return status */
  return status;
}

/*
 * Decrement the long value on top of a stack.
 * 
 * pStack is the stack object.  It must be properly initialized.  It
 * must not be empty, or a fault will occur.
 * 
 * The function fails if the top of the stack has already has value less
 * than one.
 * 
 * Note that this function will not decrement into negative range.
 * 
 * Parameters:
 * 
 *   pStack - the stack
 * 
 * Return:
 * 
 *   non-zero if successful, zero if long value already less than one
 */
static int snstack_dec(SNSTACK *pStack) {
  
  int status = 1;
  
  /* Check parameter */
  if (pStack == NULL) {
    abort();
  }
  
  /* Check state */
  if (pStack->count < 1) {
    abort();
  }
  
  /* Decrement if possible */
  if (pStack->pBuf[pStack->count - 1] > 0) {
    (pStack->pBuf[pStack->count - 1])--;
  } else {
    status = 0;
  }
  
  /* Return status */
  return status;
}

/*
 * Get the number of elements on a given stack.
 * 
 * Parameters:
 * 
 *   pStack - the stack to query
 * 
 * Return:
 * 
 *   the number of elements on the stack
 */
static long snstack_count(SNSTACK *pStack) {
  
  /* Check parameter */
  if (pStack == NULL) {
    abort();
  }
  
  /* Return count */
  return (pStack->count);
}

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
    pBuffer->pBuf = NULL;
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
 * Get the last character of a buffer.
 * 
 * Parameters:
 * 
 *   pBuffer - the buffer to query
 * 
 * Return:
 * 
 *   the last character, or zero if the buffer is empty
 */
static int snbuffer_last(SNBUFFER *pBuffer) {
  
  int c = 0;
  
  /* Check parameter */
  if (pBuffer == NULL) {
    abort();
  }
  
  /* Get last character if buffer not empty */
  if (pBuffer->count > 0) {
    c = pBuffer->pBuf[pBuffer->count - 1];
  }
  
  /* Return character or zero */
  return c;
}

/*
 * Remove the last character from the given buffer.
 * 
 * The function fails if the buffer is empty.
 * 
 * Parameters:
 * 
 *   pBuffer - the buffer to remove a character from
 * 
 * Return:
 * 
 *   non-zero if successful, zero if buffer already empty
 */
static int snbuffer_less(SNBUFFER *pBuffer) {
  
  int status = 1;
  
  /* Check parameter */
  if (pBuffer == NULL) {
    abort();
  }
  
  /* Only proceed if not empty */
  if (pBuffer->count > 0) {
    /* Remove a character from the buffer */
    pBuffer->pBuf[pBuffer->count - 1] = (char) 0;
    (pBuffer->count)--;
    
  } else {
    /* Buffer was empty */
    status = 0;
  }
  
  /* Return status */
  return status;
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
 * Read a token.
 * 
 * pBuffer is the buffer into which the token will be read.  It must be
 * properly initialized.  This function will reset the buffer and then
 * write the token into it.
 * 
 * pIn is the file to read data from.  It must be open for read access.
 * Reading is fully sequential.
 * 
 * pFilter is the input filter to read the data through.  It should be
 * in the proper state.
 * 
 * This function will skip over comments and whitespace before reading
 * the token.  If the special |; token is read, this function will also
 * read through the rest of the file, confirming that nothing except for
 * whitespace and comments is present after the |; token.
 * 
 * For string tokens and embedded tokens, this function only reads the
 * opening token and not the data that follows it.  Input will be
 * positioned such that the next byte read will the first byte of string
 * data or embedded data.
 * 
 * Note that it is not possible to use this function to iterate through
 * all the tokens in a Shastina source file, because this function
 * doesn't handle string data and embedded data.
 * 
 * Parameters:
 * 
 *   pBuffer - the buffer to read the token into
 * 
 *   pIn - the input file to read the token from
 * 
 *   pFilter - the input filter
 * 
 * Return:
 * 
 *   zero if successful, or one of the SNERR constants if error
 */
static int sntk_readToken(
    SNBUFFER * pBuffer,
    FILE     * pIn,
    SNFILTER * pFilter) {
  
  int err_num = 0;
  int c = 0;
  int c2 = 0;
  int term = 0;
  int leave = 0;
  int omit = 0;
  
  /* Check parameters */
  if ((pBuffer == NULL) || (pIn == NULL) || (pFilter == NULL)) {
    abort();
  }
  
  /* Reset the buffer */
  snbuffer_reset(pBuffer, 0);
  
  /* Skip over whitespace and comments */
  sntk_skip(pIn, pFilter);
  
  /* Read a character */
  c = snfilter_read(pFilter, pIn);
  if (c < 0) {
    err_num = c;
  }
  
  /* Check that the character is legal */
  if (!err_num) {
    if (!snchar_islegal(c)) {
      err_num = SNERR_BADCHAR;
    }
  }
  
  /* Add the first character to the buffer */
  if (!err_num) {
    if (!snbuffer_append(pBuffer, c)) {
      err_num = SNERR_LONGTOKEN;
    }
  }
  
  /* If the first character is a vertical bar, check if the second
   * character is a semicolon, forming the |; pair; set the term flag
   * in this case and add the semicolon to the token; else, unread the
   * second character */
  if ((!err_num) && (c == ASCII_BAR)) {
    c2 = snfilter_read(pFilter, pIn);
    if (c2 < 0) {
      err_num = c2;
    }
    
    if ((!err_num) && (c2 == ASCII_SEMICOLON)) {
      term = 1;
      if (!snbuffer_append(pBuffer, c2)) {
        err_num = SNERR_LONGTOKEN;
      }
    } else {
      if (!snfilter_pushback(pFilter)) {
        abort();  /* shouldn't happen */
      }
    }
  }
  
  /* If we just read the terminal |; token, make sure there's nothing
   * else in the input file */
  if ((!err_num) && term) {
    sntk_skip(pIn, pFilter);
    c2 = snfilter_read(pFilter, pIn);
    if (c2 != SNERR_EOF) {
      if (c2 >= 0) {
        err_num = SNERR_TRAILER;
      } else {
        err_num = c2;
      }
    }
  }
  
  /* If the first character read is not atomic and the term flag is not
   * set, read additional characters into the token up to the exclusive
   * or inclusive character that ends the token */
  if ((!err_num) && (!term)) {
    if (!snchar_isatomic(c)) {
      
      /* Read the additional characters */
      while (!err_num) {
        
        /* Read another character */
        c = snfilter_read(pFilter, pIn);
        if (c < 0) {
          err_num = c;
        }
        
        /* Make sure the character is legal */
        if (!err_num) {
          if (!snchar_islegal(c)) {
            err_num = SNERR_BADCHAR;
          }
        }
        
        /* If the character is inclusive, set the leave flag */
        if (!err_num) {
          if (snchar_isinclusive(c)) {
            leave = 1;
          }
        }
        
        /* If the character is exclusive, set the leave and omit
         * flags, and push back the character */
        if (!err_num) {
          if (snchar_isexclusive(c)) {
            leave = 1;
            omit = 1;
            if (!snfilter_pushback(pFilter)) {
              abort();  /* shouldn't happen */
            }
          }
        }
        
        /* If the omit flag is not set, add the character to the
         * token */
        if ((!err_num) && (!omit)) {
          if (!snbuffer_append(pBuffer, c)) {
            err_num = SNERR_LONGTOKEN;
          }
        }
        
        /* If the leave flag is set, leave the loop */
        if ((!err_num) && leave) {
          break;
        }
      }
    }
  }
  
  /* Return okay or error number */
  return err_num;
}

/*
 * Read a complete token from the given file.
 * 
 * pToken is the structure to receive the read token.  Only the pKey and
 * pValue fields need to be filled in upon entry.  Upon return, all 
 * fields will be filled in.  See the structure documentation for
 * further information.
 * 
 * pIn is the file to read data from.  It must be open for read access.
 * Reading is fully sequential.
 * 
 * pFilter is the input filter to read the data through.  It should be
 * in the proper state.
 * 
 * This function differs from sntk_readToken() in that this function can
 * also read string data.  The only thing that this function is unable
 * to read is embedded data.
 * 
 * For embedded tokens, input will be positioned such that the next byte
 * read will be the first byte of embedded data.
 * 
 * Parameters:
 * 
 *   pToken - the token structure
 * 
 *   pIn - the input file
 * 
 *   pFil - the filter to pass input through
 */
static void sntoken_read(SNTOKEN *pToken, FILE *pIn, SNFILTER *pFil) {
  
  int err_num = 0;
  int c = 0;
  
  /* Check parameters */
  if ((pToken == NULL) || (pIn == NULL) || (pFil == NULL)) {
    abort();
  }
  if ((pToken->pKey == NULL) || (pToken->pValue == NULL) ||
      (pToken->pKey == pToken->pValue)) {
    abort();
  }
  
  /* Reset both buffers and the token fields */
  snbuffer_reset(pToken->pKey, 0);
  snbuffer_reset(pToken->pValue, 0);
  pToken->status = 0;
  pToken->str_type = 0;
  
  /* Read a token into the key buffer */
  err_num = sntk_readToken(pToken->pKey, pIn, pFil);
  
  /* Identify the token by its last character, also setting the str_type
   * flag for string tokens */
  if (!err_num) {
    /* Get last character */
    c = snbuffer_last(pToken->pKey);
    
    /* Identify token */
    if (c == ASCII_DQUOTE) {
      pToken->status = SNTOKEN_STRING;
      pToken->str_type = SNSTRING_QUOTED;
    
    } else if (c == ASCII_LCURL) {
      pToken->status = SNTOKEN_STRING;
      pToken->str_type = SNSTRING_CURLY;
    
    } else if (c == ASCII_GRACCENT) {
      pToken->status = SNTOKEN_EMBED;
    
    } else {
      pToken->status = SNTOKEN_SIMPLE;
    }
  }
  
  /* For simple tokens, distinguish between SIMPLE and FINAL */
  if ((!err_num) && (pToken->status == SNTOKEN_SIMPLE)) {
    if (strcmp(snbuffer_get(pToken->pKey), "|;") == 0) {
      pToken->status = SNTOKEN_FINAL;
    }
  }
  
  /* For string and embedded tokens, remove the last character, so the
   * key buffer only has the prefix */
  if ((!err_num) && ((pToken->status == SNTOKEN_STRING) ||
                      (pToken->status == SNTOKEN_EMBED))) {
    if (!snbuffer_less(pToken->pKey)) {
      abort();  /* shouldn't happen */
    }
  }
  
  /* For string tokens, read the string data into the value buffer */
  if ((!err_num) && (pToken->status == SNTOKEN_STRING)) {
    if (pToken->str_type == SNSTRING_QUOTED) {
      /* Quoted string */
      err_num = snstr_readQuoted(pToken->pValue, pIn, pFil);
      
    } else if (pToken->str_type == SNSTRING_CURLY) {
      /* Curly string */
      err_num = snstr_readCurlied(pToken->pValue, pIn, pFil);
      
    } else {
      /* Unknown string type */
      abort();
    }
  }
  
  /* If an error was encountered, clear the buffers and the fields, and
   * set the status to the error */
  if (err_num) {
    snbuffer_reset(pToken->pKey, 0);
    snbuffer_reset(pToken->pValue, 0);
    pToken->str_type = 0;
    pToken->status = err_num;
  }
}

/* @@TODO: */

static void snreader_init(SNREADER *pReader) {
  
  /* Check parameter */
  if (pReader == NULL) {
    abort();
  }
  
  /* Initialize */
  memset(pReader, 0, sizeof(SNREADER));
  
  pReader->status = 0;
  pReader->queue_count = 0;
  pReader->queue_read = 0;
  
  snbuffer_init(&(pReader->buf_key),
    SNREADER_KEY_INIT, SNREADER_KEY_MAX);
  snbuffer_init(&(pReader->buf_value),
    SNREADER_VAL_INIT, SNREADER_VAL_MAX);
  
  snstack_init(&(pReader->stack_array),
    SNREADER_ARRAY_INIT, SNREADER_ARRAY_MAX);
  snstack_init(&(pReader->stack_group),
    SNREADER_GROUP_INIT, SNREADER_GROUP_MAX);
  
  pReader->meta_flag = 0;
  pReader->array_flag = 0;
}

static void snreader_reset(SNREADER *pReader, int full) {
  
  /* Check parameters */
  if (pReader == NULL) {
    abort();
  }
  
  /* Reset buffers and stacks */
  snbuffer_reset(&(pReader->buf_key), full);
  snbuffer_reset(&(pReader->buf_value), full);
  
  snstack_reset(&(pReader->stack_array), full);
  snstack_reset(&(pReader->stack_group), full);
  
  /* Reset fields */
  pReader->status = 0;
  pReader->queue_count = 0;
  pReader->queue_read = 0;
  
  pReader->meta_flag = 0;
  pReader->array_flag = 0;
}

static void snreader_read(
    SNREADER * pReader,
    SNENTITY * pEntity,
    FILE     * pIn,
    SNFILTER * pFilter) {
  
  int err_code = 0;
  
  /* Check parameters */
  if ((pReader == NULL) || (pEntity == NULL) || (pIn == NULL) ||
      (pFilter == NULL)) {
    abort();
  }
  
  /* Fail immediately if reader is in error state */
  err_code = pReader->status;
  
  /* If queue is empty, fill it until something is in it */
  while ((!err_code) && (pReader->queue_count < 1)) {
    snreader_fill(pReader, pIn, pFilter);
    err_code = pReader->status;
  }
  
  /* Return either an entity or an error code */
  if (!err_code) {
    /* Copy current entity to provided entity */
    memcpy(pEntity, &(pReader->queue[pReader->queue_read]),
            sizeof(SNENTITY));
    
    /* If current entity is not EOF, then remove it from queue */
    if ((pReader->queue[pReader->queue_read]).status !=
          SNENTITY_EOF) {
      
      (pReader->queue_read)++;
      if (pReader->queue_read >= pReader->queue_count) {
        pReader->queue_count = 0;
        pReader->queue_read = 0;
      }
    }
    
  } else {
    /* Error status -- write into the entity */
    pEntity->status = err_code;
  }
}

static void snreader_addEntityZ(SNREADER *pReader, int entity) {
  
  SNENTITY *pe = NULL;
  
  /* Check parameters */
  if (pReader == NULL) {
    abort();
  }
  if ((entity != SNENTITY_EOF) &&
      (entity != SNENTITY_BEGIN_META) &&
      (entity != SNENTITY_END_META) &&
      (entity != SNENTITY_BEGIN_GROUP) &&
      (entity != SNENTITY_END_GROUP)) {
    abort();
  }
  
  /* Proceed only if reader not in error state */
  if (!(pReader->status)) {
    /* Make sure room for another entity */
    if (pReader->queue_count >= SNREADER_MAXQUEUE) {
      abort();
    }
    
    /* Get entity pointer */
    pe = &(pReader->queue[pReader->queue_count]);
    
    /* Fill in entity */
    pe->status = entity;
    
    /* Increase the entity count */
    (pReader->queue_count)++;
  }
}

static void snreader_addEntityS(
    SNREADER * pReader,
    int        entity,
    char     * s) {
  
  SNENTITY *pe = NULL;
  
  /* Check parameters */
  if ((pReader == NULL) || (s == NULL)) {
    abort();
  }
  if ((entity != SNENTITY_EMBEDDED) &&
      (entity != SNENTITY_META_TOKEN) &&
      (entity != SNENTITY_NUMERIC) &&
      (entity != SNENTITY_VARIABLE) &&
      (entity != SNENTITY_CONSTANT) &&
      (entity != SNENTITY_ASSIGN) &&
      (entity != SNENTITY_GET) &&
      (entity != SNENTITY_OPERATION)) {
    abort();
  }
      
  /* Proceed only if reader not in error state */
  if (!(pReader->status)) {
    /* Make sure room for another entity */
    if (pReader->queue_count >= SNREADER_MAXQUEUE) {
      abort();
    }
    
    /* Get entity pointer */
    pe = &(pReader->queue[pReader->queue_count]);
    
    /* Fill in entity */
    pe->status = entity;
    pe->pKey = s;
    
    /* Increase the entity count */
    (pReader->queue_count)++;
  }
}

static void snreader_addEntityL(
    SNREADER * pReader,
    int        entity,
    long       l) {
  
  SNENTITY *pe = NULL;
  
  /* Check parameters */
  if (pReader == NULL) {
    abort();
  }
  if (entity != SNENTITY_ARRAY) {
    abort();
  }
  
  /* Proceed only if reader not in error state */
  if (!(pReader->status)) {
    /* Make sure room for another entity */
    if (pReader->queue_count >= SNREADER_MAXQUEUE) {
      abort();
    }
    
    /* Get entity pointer */
    pe = &(pReader->queue[pReader->queue_count]);
    
    /* Fill in entity */
    pe->status = entity;
    pe->count = l;
    
    /* Increase the entity count */
    (pReader->queue_count)++;
  }
}

static void snreader_addEntityT(
    SNREADER * pReader,
    int        entity,
    char     * pPrefix,
    int        str_type,
    char     * pData) {
  
  SNENTITY *pe = NULL;
  
  /* Check parameters */
  if ((pReader == NULL) || (pPrefix == NULL) || (pData == NULL)) {
    abort();
  }
  if ((str_type != SNSTRING_QUOTED) && (str_type != SNSTRING_CURLY)) {
    abort();
  }
  if ((entity != SNENTITY_STRING) &&
      (entity != SNENTITY_META_STRING)) {
    abort();
  }
  
  /* Proceed only if reader not in error state */
  if (!(pReader->status)) {
    /* Make sure room for another entity */
    if (pReader->queue_count >= SNREADER_MAXQUEUE) {
      abort();
    }
    
    /* Get entity pointer */
    pe = &(pReader->queue[pReader->queue_count]);
    
    /* Fill in entity */
    pe->status = entity;
    pe->pKey = pPrefix;
    pe->str_type = str_type;
    pe->pValue = pData;
    
    /* Increase the entity count */
    (pReader->queue_count)++;
  }
}

static void snreader_arrayPrefix(SNREADER *pReader) {
  
  int err_code = 0;
  
  /* Check parameter and state */
  if (pReader == NULL) {
    abort();
  }
  if (pReader->status) {
    abort();
  }
  
  /* Only do something if array flag is set */
  if (pReader->array_flag) {
    
    /* Clear the array flag */
    pReader->array_flag = 0;
    
    /* Push a value of one on top of the array stack and a value of zero
     * on top of the grouping stack to begin a new array */
    if (!snstack_push(&(pReader->stack_array), 1)) {
      err_code = SNERR_DEEPARRAY;
    }
    
    if (!err_code) {
      if (!snstack_push(&(pReader->stack_group), 0)) {
        err_code = SNERR_DEEPARRAY;
      }
    }
    
    /* Add a BEGIN_GROUP entity */
    if (!err_code) {
      snreader_addEntityZ(pReader, SNENTITY_BEGIN_GROUP);
    }
  }
}

static void snreader_fill(
    SNREADER * pReader,
    FILE     * pIn,
    SNFILTER * pFilter) {
  
  int err_code = 0;
  int firstchar = 0;
  char *pks = NULL;
  SNTOKEN tk;
  
  /* Initialize structures */
  memset(&tk, 0, sizeof(SNTOKEN));
  
  /* Check parameters and state */
  if ((pReader == NULL) || (pIn == NULL) || (pFilter == NULL)) {
    abort();
  }
  if (pReader->status) {
    abort();
  }
  if (pReader->queue_count > 0) {
    abort();
  }
  
  /* If grouping stack is empty, initialize it with a value of zero */
  if (snstack_count(&(pReader->stack_group)) < 1) {
    if (!snstack_push(&(pReader->stack_group), 0)) {
      abort();
    }
  }
  
  /* Read a token */
  tk.pKey = &(pReader->buf_key);
  tk.pValue = &(pReader->buf_value);
  sntoken_read(&tk, pIn, pFilter);
  if (tk.status < 0) {
    err_code = tk.status;
  }
  
  /* Perform array prefix operation if not in metacommand mode, except
   * for "]" token */
  if ((!err_code) && (!pReader->meta_flag)) {
    if (tk.status == SNTOKEN_SIMPLE) {
      if (strcmp(snbuffer_get(&(pReader->buf_key)), "]") != 0) {
        snreader_arrayPrefix(pReader);
      }
      
    } else {
      snreader_arrayPrefix(pReader);
    }
    
    err_code = pReader->status;
  }
  
  /* Get the key string pointer */
  if (!err_code) {
    pks = snbuffer_get(tk.pKey);
  }
  
  /* Handle the token types */
  if ((tk.status == SNTOKEN_SIMPLE) && (!err_code)) {
    /* Simple token -- handle non-primitive and primitive cases */
    if (strcmp(pks, "%") == 0) {
      /* % token -- enter metacommand mode */
      if (!pReader->meta_flag) {
        pReader->meta_flag = 1;
        snreader_addEntityZ(pReader, SNENTITY_BEGIN_META);
        
      } else {
        /* Nested metacommands */
        err_code = SNERR_METANEST;
      }
      
    } else if (strcmp(pks, ";") == 0) {
      /* ; token -- leave metacommand mode */
      if (pReader->meta_flag) {
        pReader->meta_flag = 0;
        snreader_addEntityZ(pReader, SNENTITY_END_META);
        
      } else {
        /* Semicolon outside of metacommand */
        err_code = SNERR_SEMICOLON;
      }
      
    } else if (pReader->meta_flag) {
      /* Other simple tokens in metacommand mode */
      snreader_addEntityS(pReader, SNENTITY_META_TOKEN, pks);
      
    } else {
      /* Primitive tokens -- first, get first character */
      firstchar = pks[0];
      
      /* Handle the various primitive tokens */
      if ((firstchar == ASCII_PLUS) ||
          (firstchar == ASCII_HYPHEN) ||
          ((firstchar >= ASCII_ZERO) && (firstchar <= ASCII_NINE))) {
        /* Numeric token */
        snreader_addEntityS(pReader, SNENTITY_NUMERIC, pks);
        
      } else if (firstchar == ASCII_QUESTION) {
        /* Declare variable */
        snreader_addEntityS(pReader, SNENTITY_VARIABLE, (pks + 1));
        
      } else if (firstchar == ASCII_ATSIGN) {
        /* Declare constant */
        snreader_addEntityS(pReader, SNENTITY_CONSTANT, (pks + 1));
        
      } else if (firstchar == ASCII_COLON) {
        /* Assign variable */
        snreader_addEntityS(pReader, SNENTITY_ASSIGN, (pks + 1));
        
      } else if (firstchar == ASCII_EQUALS) {
        /* Get variable or constant value */
        snreader_addEntityS(pReader, SNENTITY_GET, (pks + 1));
        
      } else if (strcmp(pks, "(") == 0) {
        /* Begin group */
        if (snstack_inc(&(pReader->stack_group))) {
          snreader_addEntityZ(pReader, SNENTITY_BEGIN_GROUP);
        } else {
          err_code = SNERR_DEEPGROUP;
        }
        
      } else if (strcmp(pks, ")") == 0) {
        /* End group */
        if (snstack_dec(&(pReader->stack_group))) {
          snreader_addEntityZ(pReader, SNENTITY_END_GROUP);
        } else {
          err_code = SNERR_RPAREN;
        }
        
      } else if (strcmp(pks, "[") == 0) {
        /* Begin array */
        pReader->array_flag = 1;
        
      } else if (strcmp(pks, "]") == 0) {
        /* End array */
        if (!(pReader->array_flag)) {
          /* Non-empty array -- check that array stack is not empty and
           * that value on top of grouping stack is zero, then perform
           * operation */
          if (snstack_count(&(pReader->stack_array)) > 0) {
            if (snstack_peek(&(pReader->stack_group)) == 0) {
              snreader_addEntityZ(pReader, SNENTITY_END_GROUP);
              snreader_addEntityL(pReader, SNENTITY_ARRAY,
                snstack_pop(&(pReader->stack_array)));
              snstack_pop(&(pReader->stack_group));
              
            } else {
              err_code = SNERR_OPENGROUP;
            }
            
          } else {
            err_code = SNERR_RSQR;
          }
          
        } else {
          /* Empty array */
          pReader->array_flag = 0;
          snreader_addEntityL(pReader, SNENTITY_ARRAY, 0);
        }
        
      } else if (strcmp(pks, ",") == 0) {
        /* Array separator -- check that array stack is not empty and
         * that value on top of grouping stack is zero, then perform
         * operation */
        if (snstack_count(&(pReader->stack_array)) > 0) {
          if (snstack_peek(&(pReader->stack_group)) == 0) {
            if (snstack_inc(&(pReader->stack_array))) {
              snreader_addEntityZ(pReader, SNENTITY_END_GROUP);
              snreader_addEntityZ(pReader, SNENTITY_BEGIN_GROUP);
              
            } else {
              err_code = SNERR_LONGARRAY;
            }
              
          } else {
            err_code = SNERR_OPENGROUP;
          }
            
        } else {
          err_code = SNERR_RSQR;
        }
        
      } else {
        /* Operator */
        snreader_addEntityS(pReader, SNENTITY_OPERATION, pks);
      }
    }
    
  } else if ((tk.status == SNTOKEN_STRING) && (!err_code)) {
    /* String token -- either a normal string or a meta string */
    if (pReader->meta_flag) {
      snreader_addEntityT(pReader, SNENTITY_META_STRING,
        pks, tk.str_type, snbuffer_get(tk.pValue));
    } else {
      snreader_addEntityT(pReader, SNENTITY_STRING,
        pks, tk.str_type, snbuffer_get(tk.pValue));
    }
    
  } else if ((tk.status == SNTOKEN_EMBED) && (!err_code)) {
    /* Embedded token */
    if (!(pReader->meta_flag)) {
      snreader_addEntityS(pReader, SNENTITY_EMBEDDED, pks);
    } else {
      err_code = SNERR_METAEMBED;
    }
  
  } else if ((tk.status == SNTOKEN_FINAL) && (!err_code)) {
    /* Final token */
    if (!(pReader->meta_flag)) {
      if ((!pReader->array_flag) &&
            (snstack_count(&(pReader->stack_array)) == 0)) {
        if (snstack_peek(&(pReader->stack_group)) == 0) {
          
          snreader_addEntityZ(pReader, SNENTITY_EOF);
          
        } else {
          err_code = SNERR_OPENGROUP;
        }
      } else {
        err_code = SNERR_OPENARRAY;
      }
    } else {
      err_code = SNERR_OPENMETA;
    }
    
  } else if (!err_code) {
    /* Unknown token type */
    abort();
  }
  
  /* Error if now an error state in reader */
  if (!err_code) {
    err_code = pReader->status;
  }
  
  /* If error, set error in reader */
  if (err_code) {
    pReader->status = err_code;
  }
}

/*
 * @@TODO:
 */
int main(int argc, char *argv[]) {
  
  SNFILTER fil;
  SNREADER reader;
  SNENTITY ent;
  int retval = 0;
  
  snfilter_reset(&fil);
  snreader_init(&reader);
  memset(&ent, 0, sizeof(SNENTITY));
  
  for(snreader_read(&reader, &ent, stdin, &fil);
      ent.status >= 0;
      snreader_read(&reader, &ent, stdin, &fil)) {
    
    if (ent.status == SNENTITY_EOF) {
      printf("End Of File\n");
      
    } else if (ent.status == SNENTITY_STRING) {
      if (ent.str_type == SNSTRING_QUOTED) {
        printf("String (%s) \"%s\"\n", ent.pKey, ent.pValue);
      } else if (ent.str_type == SNSTRING_CURLY) {
        printf("String (%s) {%s}\n", ent.pKey, ent.pValue);
      } else {
        /* Unknown string type */
        abort();
      }
      
    } else if (ent.status == SNENTITY_EMBEDDED) {
      printf("Embedded (%s)\n", ent.pKey);
      
    } else if (ent.status == SNENTITY_BEGIN_META) {
      printf("Begin metacommand\n");
      
    } else if (ent.status == SNENTITY_END_META) {
      printf("End metacommand\n");
      
    } else if (ent.status == SNENTITY_META_TOKEN) {
      printf("Meta token %s\n", ent.pKey);
      
    } else if (ent.status == SNENTITY_META_STRING) {
      if (ent.str_type == SNSTRING_QUOTED) {
        printf("Meta string (%s) \"%s\"\n", ent.pKey, ent.pValue);
      } else if (ent.str_type == SNSTRING_CURLY) {
        printf("Meta string (%s) {%s}\n", ent.pKey, ent.pValue);
      } else {
        /* Unknown string type */
        abort();
      }
      
    } else if (ent.status == SNENTITY_NUMERIC) {
      printf("Numeric %s\n", ent.pKey);
      
    } else if (ent.status == SNENTITY_VARIABLE) {
      printf("Declare variable %s\n", ent.pKey);
      
    } else if (ent.status == SNENTITY_CONSTANT) {
      printf("Declare constant %s\n", ent.pKey);
      
    } else if (ent.status == SNENTITY_ASSIGN) {
      printf("Assign variable %s\n", ent.pKey);
      
    } else if (ent.status == SNENTITY_GET) {
      printf("Get value %s\n", ent.pKey);
      
    } else if (ent.status == SNENTITY_BEGIN_GROUP) {
      printf("Begin group\n");
      
    } else if (ent.status == SNENTITY_END_GROUP) {
      printf("End group\n");
      
    } else if (ent.status == SNENTITY_ARRAY) {
      printf("Array %ld\n", ent.count);
      
    } else if (ent.status == SNENTITY_OPERATION) {
      printf("Operation %s\n", ent.pKey);
      
    } else {
      /* Unrecognized entity type */
      abort();
    }
    
    if (ent.status == SNENTITY_EOF) {
      break;
    }
  }
  if (ent.status != 0) {
    fprintf(stderr, "Error %d!\n", retval);
  }
  
  snreader_reset(&reader, 1);
  
  return 0;
}
