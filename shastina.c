/*
 * shastina.c
 */

#include "shastina.h"
#include <stdlib.h>
#include <string.h>

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
#define ASCII_LCURL     (0x7b)  /* { */
#define ASCII_BAR       (0x7c)  /* | */
#define ASCII_RCURL     (0x7d)  /* } */

/* Visible printing characters */
#define ASCII_VISIBLE_MIN (0x21)
#define ASCII_VISIBLE_MAX (0x7e)

/*
 * Unicode constants.
 */
#define UNICODE_MAX_CPV          (0x10ffffL)
#define UNICODE_MIN_SUPPLEMENTAL (0x10000L)

#define UNICODE_MIN_SURROGATE    (0xd800L)
#define UNICODE_MAX_SURROGATE    (0xdfffL)

#define UNICODE_MIN_HI_SUR       (0xd800L)
#define UNICODE_MAX_HI_SUR       (0xdbffL)

#define UNICODE_MIN_LO_SUR       (0xdc00L)
#define UNICODE_MAX_LO_SUR       (0xdfffL)

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

/*
 * The maximum number of queued entities.
 * 
 * This has nothing to do with the total number of entities in a source
 * file.  Rather, it means no more than this many entities can result
 * from processing a single token.
 */
#define SNREADER_MAXQUEUE (8)

/*
 * The initial and maximum character allocations of the reader's key
 * buffer.
 * 
 * The key buffer stores the main strings of tokens.  At first, it will
 * be allocated at size SNREADER_KEY_INIT.  It can grow up to a maximum
 * of SNREADER_KEY_MAX.
 * 
 * See snbuffer_init() for further information about these values.
 * 
 * Note that these sizes refer to bytes in the UTF-8 encoding of keys,
 * not to the number of codepoints.
 */
#define SNREADER_KEY_INIT (16)
#define SNREADER_KEY_MAX  (65535)

/*
 * The initial and maximum character allocations of the reader's value
 * buffer.
 * 
 * The value buffer stores string data.  At first, it will be allocated
 * at size SNREADER_VAL_INIT.  It can grow up to a maximum of
 * SNREADER_VAL_MAX.
 * 
 * See snbuffer_init() for further information about these values.
 * 
 * Note that these sizes refer to bytes in the UTF-8 encoding of string
 * data, not to the number of codepoints.
 */
#define SNREADER_VAL_INIT (256)
#define SNREADER_VAL_MAX  (65535)

/*
 * The initial and maximum allocations of the reader's array and group
 * stacks, in longs.
 * 
 * This affects how deeply arrays can be nested.  At first, it will be
 * allocated at size SNREADER_AGSTACK_INIT.  It can grow up to a maximum
 * of SNREADER_AGSTACK_MAX.
 * 
 * See snstack_init() for further information about these values.
 * 
 * (The group stack technically is one element taller than the array
 * stack regularly, but we don't worry about that here.)
 */
#define SNREADER_AGSTACK_INIT (8)
#define SNREADER_AGSTACK_MAX (1024)

/*
 * Structure for storing an input source.
 * 
 * Use the snsource_ functions to manipulate this structure.
 * 
 * The prototype of this structure (SNSOURCE) is defined in the header.
 */
struct SNSOURCE_TAG {
  
  /*
   * Function pointer to the function that is used to read a character
   * from input.
   * 
   * This must be present, and may not be NULL.
   * 
   * The void pointer parameter is a custom parameter that is stored as
   * the pCustom field in this SNSOURCE structure and always passed
   * through to the function.
   * 
   * The function should read a byte from input and return the unsigned
   * byte value, in range [0, 255].  If there are no more bytes to read,
   * return SNERR_EOF to indicate EOF.  If there was an I/O error trying
   * to read a byte from input, return SNERR_IOERR to indicate I/O
   * error.
   * 
   * Once the callback has returned SNERR_EOF or SNERR_IOERR, the status
   * field of the source object will be set and all further calls to
   * read from the source will return the same value as the last call
   * without calling through to this callback function.
   * 
   * Parameters:
   * 
   *   (void *) - the custom data parameter that is passed through to
   *   the function from the pCustom field of the SNSOURCE structure
   * 
   * Return:
   * 
   *   the unsigned byte value of the byte that was read, or SNERR_EOF
   *   or SNERR_IOERR
   */
  int (*pfRead)(void *);
  
  /*
   * Function pointer to an optional destructor function.
   * 
   * This may be NULL, in which case there is no destructor routine.
   * 
   * If a destructor routine is defined with this field, then the
   * destructor will be called exactly once with the value of the
   * pCustom field in this SNSOURCE structure immediately before the
   * SNSOURCE structure is released.
   * 
   * This allows the custom data stored in this structure to be released
   * when the source is released.
   * 
   * Parameters:
   * 
   *   (void *) - the custom data parameter that is stored in the
   *   pCustom field of this SNSOURCE structure
   */
  void (*pfDestruct)(void *);
  
  /*
   * The total number of (unfiltered) bytes that have been read through
   * this source object, not including any EOF or I/O error returns.
   * 
   * If this is LONG_MAX, then the total number of bytes read has
   * exceeded the range of a long.
   */
  long read_count;
  
  /*
   * The current status of this source object.
   * 
   * This is zero initially, which means that calls to read from the
   * source will call through to the function pointer stored in pfRead.
   * 
   * When pfRead returns a value of SNERR_EOF or SNERR_IOERR, then that
   * return value will be stored in this status field before it is
   * returned.  All further calls to read from this source will simply
   * return the stored status value instead of calling through to the
   * stored function pointer.
   * 
   * This may also be set to SNERR_UTF8 if UTF-8 decoding fails.
   */
  int status;
  
  /*
   * Pointer to custom data.
   * 
   * This may be NULL if not required.
   * 
   * This value is passed through to the read callback defined by the
   * pfRead function whenever the read callback is invoked.
   * 
   * This value is also passed through to the destructor routine defined
   * by pfDestruct immediately before the SNSOURCE structure is
   * released, if the destructor is defined.  This allows the custom
   * data to be released when the source is released, though this is not
   * required and may not be appropriate in all cases (such as when the
   * custom data is a FILE * correpsonding to stdin).
   */
  void *pCustom;
};

/*
 * Structure used for string reader source.
 */
typedef struct {
  
  /*
   * The pointer to the string.
   * 
   * This is updated and advanced as the string is read until it reaches
   * the terminating nul.
   */
  const unsigned char *pStr;
  
} SNSTRSRC;

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
   * This may not exceed the cap limit.
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
   * The number of bytes (not including terminating null) stored in the
   * buffer.
   * 
   * This may not exceed the cap limit.
   * 
   * Note that this counts bytes in the UTF-8 encoding, not Unicode
   * codepoints.
   */
  long count;
  
  /*
   * The current capacity of the buffer in bytes.
   * 
   * This includes space for the terminating null.
   * 
   * If zero, it means that no buffer has been allocated yet.
   * 
   * Note that this counts byte capacity for UTF-8 encoding, not Unicode
   * codepoint capacity.
   */
  long cap;
  
  /*
   * The initial allocation capacity for this buffer in bytes.
   * 
   * This must be greater than zero and no greater than maxcap.
   * 
   * Note that this counts bytes, not Unicode codepoints.
   */
  long initcap;
  
  /*
   * The maximum capacity for this buffer in bytes.
   * 
   * This must be greater than or equal to initcap.  It may not exceed
   * (LONG_MAX / 2).
   * 
   * Note that this counts bytes, not Unicode codepoints.
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
   * The line number of the codepoint that was most recently read, or
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
   * The codepoint most recently read, or an error code.
   * 
   * This field is only valid if line_count is greater than zero.
   * Otherwise, nothing has been read yet and this field is ignored.
   * 
   * If a Unicode codepoint, it is in range [0, UNICODE_MAX_CPV] and NOT
   * in range [UNICODE_MIN_SURROGATE, UNICODE_MAX_SURROGATE].
   * 
   * If it is less than zero, it is an SNERR_ code.
   */
  long c;
  
  /*
   * The pushback flag.
   * 
   * If zero, then pushback mode is not active.
   * 
   * If non-zero, it means pushback mode is active.  line_count must be
   * greater than zero in this case, and c can not be an error code.
   * The next read operation will read the buffered character rather
   * than the next character.  Adjustment will also be made to the line
   * count returned, although the line_count field remains unmodified.
   */
  int pushback;
  
} SNFILTER;

/*
 * Structure for a token read from a Shastina source file.
 * 
 * This covers everything that can be found in a Shastina source file.
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
   * For STRING token types, this buffer will contain the prefix
   * (excluding the opening quote or bracket) on return.
   * 
   * The state of the buffer is undefined on error status returns.
   * 
   * The key and value buffer may not point to the same object.
   * 
   * Note that this pointer will be to a buffer within the SNREADER
   * structure.  The buffer is not owned by this structure.
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
   * 
   * Note that this pointer will be to a buffer within the SNREADER
   * structure.  The buffer is not owned by this structure.
   */
  SNBUFFER *pValue;
  
} SNTOKEN;

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
   * 
   * The numeric value on top of the stack counts the number of array
   * elements.
   */
  SNSTACK stack_array;
  
  /*
   * The group stack.
   * 
   * This needs to be properly initialized.  It also needs to be fully
   * reset before the structure is released.
   * 
   * The numeric value on top of the stack counts the number of open
   * groups.  Array elements implicitly open special groups that can
   * only be closed at the end of an array element.  Hence the need for
   * a stack to track open groups.
   * 
   * The height of this stack is regularly one greater than the height
   * of the array stack.
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

/*
 * Structure for storing state of the Shastina metalanguage parser.
 * 
 * Use the snparser_ functions to manipulate this structure.
 * 
 * The prototype of this structure (SNPARSER) is defined in the header.
 */
struct SNPARSER_TAG {
  
  /*
   * The reader object.
   * 
   * This must be initialized with snreader_init().
   * 
   * It must be fully reset with snreader_reset() before the structure
   * is released.
   */
  SNREADER reader;
  
  /*
   * The input filter object.
   * 
   * This must be initialized with snfilter_reset().
   */
  SNFILTER filter;
};

/* Function prototypes */
static long snutf_pair(long hi, long lo);
static int snutf_count(int c);
static long snutf_decode(const unsigned char *pc);
static void snutf_encode(long cpv, unsigned char *pb);

static int snsource_file_read(void *pCustom);
static void snsource_file_free(void *pCustom);
static int snsource_str_read(void *pCustom);
static void snsource_str_free(void *pCustom);

static int snsource_read(SNSOURCE *pIn);
static long snsource_readCPV(SNSOURCE *pIn);

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
static int snbuffer_appendByte(SNBUFFER *pBuffer, int c);
static int snbuffer_append(SNBUFFER *pBuffer, long cpv);
static char *snbuffer_get(SNBUFFER *pBuffer);
static long snbuffer_last(SNBUFFER *pBuffer);
static int snbuffer_less(SNBUFFER *pBuffer);

static void snfilter_reset(SNFILTER *pFilter);
static long snfilter_read(SNFILTER *pFilter, SNSOURCE *pIn);
static long snfilter_count(SNFILTER *pFilter);
static int snfilter_pushback(SNFILTER *pFilter);

static int snchar_islegal(long c);
static int snchar_isatomic(long c);
static int snchar_isinclusive(long c);
static int snchar_isexclusive(long c);
static int snchar_strequals(int c, const char *pStr);
static int snchar_strequals2(int c1, int c2, const char *pStr);

static int snstr_readQuoted(
    SNBUFFER * pBuffer,
    SNSOURCE * pIn,
    SNFILTER * pFilter);

static int snstr_readCurlied(
    SNBUFFER * pBuffer,
    SNSOURCE * pIn,
    SNFILTER * pFilter);

static void sntk_skip(SNSOURCE *pIn, SNFILTER *pFilter);
static int sntk_readToken(
    SNBUFFER * pBuffer,
    SNSOURCE * pIn,
    SNFILTER * pFilter);

static void sntoken_read(
    SNTOKEN  * pToken,
    SNSOURCE * pIn,
    SNFILTER * pFil);

static void snreader_init(SNREADER *pReader);
static void snreader_reset(SNREADER *pReader, int full);
static void snreader_read(
    SNREADER * pReader,
    SNENTITY * pEntity,
    SNSOURCE * pIn,
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
    SNSOURCE * pIn,
    SNFILTER * pFilter);

/*
 * Given a high surrogate and a low surrogate, return the supplemental
 * codepoint that the pair selects.
 * 
 * A fault occurs if hi is not in high surrogate range or lo is not in
 * low surrogate range.
 * 
 * Parameters:
 * 
 *   hi - the high surrogate
 * 
 *   lo - the low surrogate
 * 
 * Return:
 * 
 *   the supplemental codepoint
 */
static long snutf_pair(long hi, long lo) {
  
  long result = 0;
  
  /* Check parameters */
  if ((hi < UNICODE_MIN_HI_SUR) || (hi > UNICODE_MAX_HI_SUR) ||
      (lo < UNICODE_MIN_LO_SUR) || (lo > UNICODE_MAX_LO_SUR)) {
    abort();
  }
  
  /* Convert hi and lo into offsets within their surrogate ranges */
  hi = hi - UNICODE_MIN_HI_SUR;
  lo = lo - UNICODE_MIN_LO_SUR;
  
  /* Combine hi and lo with bit shifting */
  result = (hi << 10) | lo;
  
  /* Use as offset into surrogate range */
  result = result + UNICODE_MIN_SUPPLEMENTAL;
  
  /* Return result */
  return result;
}

/*
 * Check a byte within a UTF-8 encoding to determine how many bytes long
 * the encoding is.
 * 
 * c is the unsigned byte value to check.  It must be in range [0, 255].
 * 
 * The return value is one if the most significant bit of the byte is
 * clear (indicating an ASCII character), two to four if the given byte
 * is a UTF-8 lead byte, zero if the byte is a UTF-8 continuation byte,
 * and -1 if the byte is not a valid UTF-8 byte.
 * 
 * If a value greater than zero is returned, then the whole encoded
 * codepoint (including the leading byte) has that many bytes in it,
 * where any bytes after the first are continuation bytes.
 * 
 * Parameters:
 * 
 *   c - the byte value to check
 * 
 * Return:
 * 
 *   the number of bytes in the encoded codepoint, or zero if the given
 *   byte is a continuation byte, or -1 if the byte is not valid UTF-8
 */
static int snutf_count(int c) {
  
  int result = 0;
  
  /* Check parameter */
  if ((c < 0) || (c > 255)) {
    abort();
  }
  
  /* Determine result */
  if (c <= 127) {
    /* Everything in [0, 127] has one byte */
    result = 1;
  
  } else if ((c & 0xC0) == 0x80) {
    /* 10?????? bytes are continuation bytes */
    result = 0;
    
  } else if ((c & 0xE0) == 0xC0) {
    /* 110????? bytes are lead bytes for two-byte encodings */
    result = 2;
    
  } else if ((c & 0xF0) == 0xE0) {
    /* 1110???? bytes are lead bytes for three-byte encodings */
    result = 3;
    
  } else if ((c & 0xF8) == 0xF0) {
    /* 11110??? bytes are lead bytes for four-byte encodings */
    result = 4;
    
  } else {
    /* Everything else is invalid UTF-8 */
    result = -1;
  }
  
  /* Return result */
  return result;
}

/*
 * Decode a UTF-8 codepoint.
 * 
 * pc points to the start of an encoded UTF-8 codepoint.  Encoded UTF-8
 * codepoints may have from one to four bytes for a codepoint.  This
 * function will stop immediately and return an error if it encounters
 * a faulty encoding, and it is guaranteed safe to use on nul-terminated
 * strings, even if the strings have bad UTF-8 encodings.  (It will
 * never read beyond the terminating nul, even if the terminating nul
 * comes unexpectedly in the middle of an encoded UTF-8 codepoint.)
 * 
 * The leading byte pointed to by pc determines how many bytes are in
 * the encoding.  You can use snutf_count() to get this information
 * given a byte value.  Encodings that have more than one byte will have
 * all bytes after the first byte be continuation bytes.
 * 
 * This function will check for and fail on overlong encodings -- that
 * is, encodings of codepoints that are unnecessarily long.  These are
 * blocked for security reasons, and should never occur in valid streams
 * of UTF-8.
 * 
 * This function allows surrogates to be decoded, even though surrogates
 * aren't supposed to be used in UTF-8.  Surrogates need to be resolved
 * by looking at more than one codepoint at a time, so they can't be
 * handled properly by this function.
 * 
 * Parameters:
 * 
 *   pc - pointer to the UTF-8 codepoint
 * 
 * Return:
 * 
 *   a Unicode codepoint (including surrogates!), or -1 if the UTF-8
 *   encoding wasn't valid
 */
static long snutf_decode(const unsigned char *pc) {
  
  int status = 1;
  int ec = 0;
  int x = 0;
  long result = 0;
  
  /* Check parameter */
  if (pc == NULL) {
    abort();
  }
  
  /* Determine encoded length */
  ec = snutf_count(*pc);
  if (ec < 1) {
    /* If start of encoding is a continuation byte or not a valid UTF-8
     * byte, fail */
    status = 0;
  }
  
  /* If there is more than one byte in the encoding, make sure all
   * additional bytes are continuation bytes */
  if (status) {
    for(x = 1; x < ec; x++) {
      if (snutf_count(pc[x]) != 0) {
        status = 0;
        break;
      }
    }
  }
  
  /* All UTF-8 bytes are available and of the correct type, so begin by
   * getting the payload of the lead byte */
  if (status) {
    switch (ec) {
      case 1:
        /* For one-byte codes, the byte value is equal to the code */
        result = pc[0];
        break;
      
      case 2:
        /* For two-byte codes, lead byte payload is ---PPPPP */
        result = (pc[0] & 0x1F);
        break;
      
      case 3:
        /* For three-byte codes, lead byte payload is ----PPPP */
        result = (pc[0] & 0x0F);
        break;
      
      case 4:
        /* For four-byte codes, lead byte payload is -----PPP */
        result = (pc[0] & 0x07);
        break;
      
      default:
        /* Shouldn't happen */
        abort();
    }
  }
  
  /* Include the payload from any continuation bytes -- continuation
   * byte payload is --PPPPPP */
  if (status) {
    for(x = 1; x < ec; x++) {
      result = (result << 6) | (pc[x] & 0x3F);
    }
  }
  
  /* Fail for any overlong encodings */
  if (status) {
    switch (ec) {
      case 1:
        /* One-byte encodings are never overlong */
        break;
      
      case 2:
        /* Two-byte encodings must be at least U+0080 */
        if (result < 0x80L) {
          status = 0;
        }
        break;
      
      case 3:
        /* Three-byte encodings must be at least U+0800 */
        if (result < 0x800L) {
          status = 0;
        }
        break;
      
      case 4:
        /* Four-byte encodings must be at least U+10000 */
        if (result < 0x10000L) {
          status = 0;
        }
        break;
      
      default:
        /* Shouldn't happen */
        abort();
    }
  }
  
  /* If we failed, set result to -1 */
  if (!status) {
    result = -1;
  }
  
  /* Return result */
  return result;
}

/*
 * Encode a Unicode codepoint into UTF-8.
 * 
 * The given codepoint must be in [0, UNICODE_MAX_CPV] range.  Also, it
 * must NOT be in [UNICODE_MIN_SURROGATE, UNICODE_MAX_SURROGATE] range.
 * 
 * pb points to the buffer to write the encoded codepoint into.  This
 * function will write from one to four bytes into the buffer.  Make
 * sure there is enough room in the buffer for the encoded codepoint!
 * 
 * One way of handling the buffer is to allocate a buffer of five
 * characters, initialize all the characters to nul, and then call this
 * function on the buffer.  After this function returns, then, the
 * result will be nul-terminated (except if the given codepoint is zero,
 * in which case the result will be empty).
 * 
 * Parameters:
 * 
 *   cpv - the Unicode codepoint
 * 
 *   pb - the buffer to write the result into
 */
static void snutf_encode(long cpv, unsigned char *pb) {
  
  int ec = 0;
  int c = 0;
  int i = 0;
  
  /* Check parameters */
  if (pb == NULL) {
    abort();
  }
  if ((cpv < 0) || (cpv > UNICODE_MAX_CPV) ||
        ((cpv >= UNICODE_MIN_SURROGATE) &&
          (cpv <= UNICODE_MAX_SURROGATE))) {
    abort();
  }
  
  /* From the codepoint value, figure out the encoded length of the
   * codepoint */
  if (cpv < 0x80L) {
    /* U+0000 to 0+007F has one-byte encodings */
    ec = 1;
  
  } else if (cpv < 0x800L) {
    /* U+0080 to U+07FF has two-byte encodings */
    ec = 2;
    
  } else if (cpv < 0x10000L) {
    /* U+0800 to U+FFFF has three-byte encodings */
    ec = 3;
    
  } else if (cpv <= UNICODE_MAX_CPV) {
    /* U+10000 to end of Unicode range has four-byte encodings */
    ec = 4;
    
  } else {
    /* Shouldn't happen */
    abort();
  }
  
  /* Align the cpv by left shifting so that everything has payload bits
   * for three continuation bytes; that is, for one-byte codes, add
   * 18 (= 6 x 3) bits; for two-byte codes, add 12 (= 6 x 2) bits; for
   * three-byte codes, add 6 bits; and nothing for four-byte codes,
   * which already have three continuation bytes */
  if (ec == 1) {
    cpv = cpv << 18;
  
  } else if (ec == 2) {
    cpv = cpv << 12;
    
  } else if (ec == 3) {
    cpv = cpv << 6;
  }
  
  /* Lead byte payload is now always 18 bits shifted, so combine it with
   * the appropriate marker bits and write the first encoded byte */
  c = (int) (cpv >> 18);
  
  if (ec == 2) {
    c = c | 0xC0;
  
  } else if (ec == 3) {
    c = c | 0xE0;
    
  } else if (ec == 4) {
    c = c | 0xF0;
  }
  
  *pb = (unsigned char) c;
  pb++;
  
  /* Mask out the lead byte payload, leaving just the continuation byte
   * payloads */
  cpv = cpv & 0x3ffffL;
  
  /* Write any continuation bytes */
  for(i = 1; i < ec; i++) {
    /* Get most significant continuation payload */
    c = (int) (cpv >> 12);
    
    /* Add continuation marker bit */
    c = c | 0x80;
    
    /* Output continuation byte */
    *pb = (unsigned char) c;
    pb++;
    
    /* Mask out most significant continuation payload */
    cpv = cpv & 0xfffL;
    
    /* Shift continuation payloads left */
    cpv = cpv << 6;
  }
}

/*
 * Reading callback for a stdio FILE * source.
 * 
 * The function prototype matches pfRead in SNSOURCE.  See the
 * documentation of that field for further information.
 */
static int snsource_file_read(void *pCustom) {
  
  FILE *pIn = NULL;
  int result = 0;
  
  /* Check parameter */
  if (pCustom == NULL) {
    abort();
  }
  
  /* Convert parameter to a FILE * handle */
  pIn = (FILE *) pCustom;
  
  /* Read from the file and check for EOF and I/O error conditions */
  result = getc(pIn);
  if (result == EOF) {
    if (feof(pIn)) {
      result = SNERR_EOF;
    } else {
      result = SNERR_IOERR;
    }
  }
  
  /* Return result */
  return result;
}

/*
 * Destructor callback for a stdio FILE * source.
 * 
 * The function prototype matches pfDestruct in SNSOURCE.  See the
 * documentation of that field for further information.
 */
static void snsource_file_free(void *pCustom) {
  
  FILE *pIn = NULL;
  
  /* Check parameter */
  if (pCustom == NULL) {
    abort();
  }
  
  /* Convert parameter to a FILE * handle */
  pIn = (FILE *) pCustom;
  
  /* Close the file */
  fclose(pIn);
}

/*
 * Reading callback for a string source.
 * 
 * The function prototype matches pfRead in SNSOURCE.  See the
 * documentation of that field for further information.
 */
static int snsource_str_read(void *pCustom) {
  
  SNSTRSRC *pStS = NULL;
  int c = 0;
  
  /* Check parameter */
  if (pCustom == NULL) {
    abort();
  }
  
  /* Convert parameter to the string handling structure */
  pStS = (SNSTRSRC *) pCustom;
  
  /* Get current character */
  c = *(pStS->pStr);
  
  /* If current character is not nul, then advance pointer; else, set
   * return to EOF condition and don't advance pointer */
  if (c != 0) {
    (pStS->pStr)++;
  } else {
    c = SNERR_EOF;
  }
  
  /* Return the character or EOF */
  return c;
}

/*
 * Destructor callback for a string source.
 * 
 * The function prototype matches pfDestruct in SNSOURCE.  See the
 * documentation of that field for further information.
 */
static void snsource_str_free(void *pCustom) {
  
  SNSTRSRC *pStS = NULL;
  
  /* Check parameter */
  if (pCustom == NULL) {
    abort();
  }
  
  /* Convert parameter to the string handling structure */
  pStS = (SNSTRSRC *) pCustom;
  
  /* Free the structure */
  free(pStS);
}

/*
 * Read a single byte from a source object.
 * 
 * The return value is an unsigned byte value in range [0, 255], or else
 * SNERR_EOF or SNERR_IOERR.  SNERR_UTF8 is never generated by this
 * function, but it may be returned because snsource_readCPV() may set
 * the status of the source object to that error.
 * 
 * Clients are recommended to use snsource_readCPV() instead of directly
 * using this function.
 * 
 * Parameters:
 * 
 *   pIn - the source to read from
 * 
 * Return:
 * 
 *   the next byte read, or SNERR_EOF if nothing remains in input
 *   source, or SNERR_IOERR if there was an error reading the input
 *   source, or SNERR_UTF8 if there was a previous UTF-8 decoding error
 */
static int snsource_read(SNSOURCE *pIn) {
  
  int result = 0;
  
  /* Check parameter */
  if (pIn == NULL) {
    abort();
  }
  
  /* Determine first whether we have a special condition to return
   * without invoking the callback */
  if (pIn->status < 0) {
    /* We have a special status code, so just use that */
    result = pIn->status;
    
  } else {
    /* No special status code, so we need to invoke the read callback;
     * check first that the callback is defined */
    if (pIn->pfRead == NULL) {
      abort();
    }
    
    /* Invoke the read callback */
    result = (*(pIn->pfRead))(pIn->pCustom);
    
    /* Check range of returned result */
    if (((result < 0) || (result > 255)) &&
          (result != SNERR_EOF) && (result != SNERR_IOERR)) {
      abort();
    }
    
    /* If returned result was an unsigned byte value, increment the read
     * count (unless it has overflown); otherwise, if it is a special
     * return code, store it in status so we return it in future
     * invocations */
    if ((result >= 0) && (result <= 255)) {
      if (pIn->read_count < LONG_MAX) {
        (pIn->read_count)++;
      }
    } else {
      pIn->status = result;
    }
  }
  
  /* Return result */
  return result;
}

/*
 * Read a UTF-8 encoded Unicode codepoint from a source object.
 * 
 * The return value is a Unicode codepoint [0, UNICODE_MAX_CPV], or else
 * SNERR_EOF if end of source file, SNERR_IOERR if I/O error reading
 * from source, or SNERR_UTF8 if error decoding UTF-8.
 * 
 * This function will only decode a single UTF-8 codepoint.  It will
 * check for overlong encodings and invalid encodings, causing the
 * SNERR_UTF8 error if such illegal encodings occur.
 * 
 * This function WILL successfully decode surrogate codepoints, even
 * though surrogates aren't supposed to be used in UTF-8.  It is assumed
 * that surrogate pairs will be properly decoded at a higher level.
 * 
 * Parameters:
 * 
 *   pIn - the source to read from
 * 
 * Return:
 * 
 *   the next codepoint read (including surrogates!), or SNERR_EOF if
 *   nothing remains in input source, or SNERR_IOERR if there was an
 *   error reading the input source, or SNERR_UTF8 if there was a UTF-8
 *   decoding error
 */
static long snsource_readCPV(SNSOURCE *pIn) {
  
  unsigned char buf[5];
  long result = 0;
  int err_num = 0;
  int c = 0;
  int ec = 0;
  int i = 0;
  
  /* Initialize buffer */
  memset(buf, 0, 5);
  
  /* Check parameters */
  if (pIn == NULL) {
    abort();
  }
  
  /* Get the next byte */
  c = snsource_read(pIn);
  
  /* If we got a special status return, then result is that; otherwise,
   * proceed */
  if (c < 0) {
    /* Special status return, so that will be the result */
    result = c;
  
  } else {
    /* We read a byte, so determine how many bytes in the encoded
     * codepoint */
    ec = snutf_count(c);
    if (ec < 1) {
      /* Not a valid first byte of an encoded codepoint, so fail */
      err_num = SNERR_UTF8;
    } else if (ec > 4) {
      /* Shouldn't happen */
      abort();
    }
    
    /* Write the character we just read into the decoding buffer */
    if (!err_num) {
      buf[0] = (unsigned char) c;
    }
    
    /* Read any additional bytes into the decoding buffer -- EOF returns
     * will be changed to UTF-8 decoding errors */
    if (!err_num) {
      for(i = 1; i < ec; i++) {
        c = snsource_read(pIn);
        if (c < 0) {
          if (c == SNERR_EOF) {
            err_num = SNERR_UTF8;
          } else {
            err_num = c;
          }
          break;
        } else {
          buf[i] = (unsigned char) c;
        }
      }
    }
    
    /* Decode the buffered character bytes into a codepoint */
    if (!err_num) {
      result = snutf_decode(buf);
      if (result < 0) {
        err_num = SNERR_UTF8;
      }
    }
  }
  
  /* If error occurred, set result to error code and store error code in
   * status field of source */
  if (err_num) {
    result = err_num;
    pIn->status = err_num;
  }
  
  /* Return result */
  return result;
}

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
 * icap is the initial allocation capacity in bytes.  maxcap is the
 * maximum capacity in bytes for the buffer.  Note that the capacity
 * counts bytes, not Unicode codepoints.
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
 * Append a byte value to a string buffer.
 * 
 * The character c may be any unsigned byte value except for zero.  That
 * is, the range is 1-255.
 * 
 * The function fails if there is no more capacity left for another 
 * byte.  The buffer is unmodified in this case.
 * 
 * This is a low-level function.  Clients should use append() instead to
 * work with Unicode codepoints.
 * 
 * Parameters:
 * 
 *   pBuffer - the string buffer to add a byte to
 * 
 *   c - the unsigned byte value to add
 * 
 * Return:
 * 
 *   non-zero if successful, zero if no more capacity
 */
static int snbuffer_appendByte(SNBUFFER *pBuffer, int c) {
  
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
    ((unsigned char *) pBuffer->pBuf)[pBuffer->count] =
      (unsigned char) c;
    (pBuffer->count)++;
    
  } else {
    /* Out of capacity */
    status = 0;
  }
  
  /* Return status */
  return status;
}

/*
 * Append a Unicode codepoint to a string buffer.
 * 
 * cpv is the Unicode codepoint to append.  It must be greater than zero
 * and no greater than UNICODE_MAX_CPV.  Furthermore, it may not be in
 * surrogate range [UNICODE_MIN_SURROGATE, UNICODE_MAX_SURROGATE].
 * 
 * The codepoint will be encoded into UTF-8 and the bytes of the UTF-8
 * encoding will be added to the string buffer.
 * 
 * The function fails if there is not enough capacity left for the full
 * UTF-8 encoding of the codepoint.  The buffer is unmodified in this
 * case.
 * 
 * Parameters:
 * 
 *   pBuffer - the string buffer to add a codepoint to
 * 
 *   cpv - the Unicode codepoint to add
 * 
 * Return:
 * 
 *   non-zero if successful, zero if not enough capacity
 */
static int snbuffer_append(SNBUFFER *pBuffer, long cpv) {
  
  int status = 1;
  unsigned char buf[5];
  unsigned char *pc = NULL;
  int elen = 0;
  
  /* Initialize buffer to all nul */
  memset(buf, 0, 5);
  
  /* Check parameters */
  if (pBuffer == NULL) {
    abort();
  }
  if ((cpv < 1) || (cpv > UNICODE_MAX_CPV) ||
      ((cpv >= UNICODE_MIN_SURROGATE) &&
        (cpv <= UNICODE_MAX_SURROGATE))) {
    abort();
  }
  
  /* Encode the codepoint and get the encoded length */
  snutf_encode(cpv, buf);
  elen = (int) strlen(buf);
  
  /* Make sure we have enough capacity for the all the bytes */
  if (pBuffer->count >= (pBuffer->maxcap - elen)) {
    status = 0;
  }
  
  /* Add each of the bytes */
  if (status) {
    for(pc = buf; *pc != 0; pc++) {
      if (!snbuffer_appendByte(pBuffer, *pc)) {
        /* Shouldn't happen because we already checked that we have
         * enough capacity remaining */
        abort();
      }
    }
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
 * Get the last Unicode codepoint in a buffer.
 * 
 * Parameters:
 * 
 *   pBuffer - the buffer to query
 * 
 * Return:
 * 
 *   the last Unicode codepoint, or zero if the buffer is empty
 */
static long snbuffer_last(SNBUFFER *pBuffer) {
  
  long c = 0;
  unsigned char *pc = NULL;
  int sw = 0;
  
  /* Check parameter */
  if (pBuffer == NULL) {
    abort();
  }
  
  /* Get last codepoint if buffer not empty */
  if (pBuffer->count > 0) {
    
    /* Find the last byte in the buffer that is not a continuation
     * byte */
    sw = 1;
    for(pc = &(((unsigned char *) pBuffer->pBuf)[pBuffer->count - 1]);
        snutf_count(*pc) == 0;
        pc--) {
      sw++;
    }
    
    /* Get how many encoded bytes and make sure valid and matches how
     * many bytes we moved back in the search */
    if (snutf_count(*pc) != sw) {
      /* Shouldn't happen because everything in buffer should be
       * valid */
      abort();
    }
    
    /* Decode the last codepoint */
    c = snutf_decode(pc);
    if (c < 0) {
      /* Shouldn't happen because everything in buffer should be
       * valid */
      abort();
    }
  }
  
  /* Return character or zero */
  return c;
}

/*
 * Remove the last codepoint from the given buffer.
 * 
 * The function fails if the buffer is empty.
 * 
 * Parameters:
 * 
 *   pBuffer - the buffer to remove a codepoint from
 * 
 * Return:
 * 
 *   non-zero if successful, zero if buffer already empty
 */
static int snbuffer_less(SNBUFFER *pBuffer) {
  
  int status = 1;
  unsigned char *pc = NULL;
  
  /* Check parameter */
  if (pBuffer == NULL) {
    abort();
  }
  
  /* Only proceed if not empty */
  if (pBuffer->count > 0) {
    /* Remove any continuation bytes from the end of the buffer */
    for(pc = &(((unsigned char *) pBuffer->pBuf)[pBuffer->count - 1]);
        snutf_count(*pc) == 0;
        pc--) {
      *pc = (unsigned char) 0;
      (pBuffer->count)--;
    }
    
    /* Remove the last codepoint from the buffer */
    *pc = (unsigned char) 0;
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
}

/*
 * Read the next codepoint through the input filter.
 * 
 * The next codepoint value is in Unicode codepoint range, which is
 * [0, UNICODE_MAX_CPV], excluding the surrogate characters, in range
 * [UNICODE_MIN_SURROGATE, UNICODE_MAX_SURROGATE].
 * 
 * CR+LF to LF conversion, UTF-8 Byte Order Mark (BOM) filtering, and
 * correction of encoded surrogate pairs to supplemental codepoints are
 * performed by this function.
 * 
 * SNERR_ codes are returned if there is an error.  These codes are
 * always less than zero, so they are easy to distinguish from Unicode
 * codepoints, which are always zero or greater.
 * 
 * The provided input filter state must be properly initialized.
 * 
 * Parameters:
 * 
 *   pFilter - the input filter state
 * 
 *   pIn - the source to read from
 * 
 * Return:
 * 
 *   the next codepoint value, or an SNERR_ value that is less than zero
 */
static long snfilter_read(SNFILTER *pFilter, SNSOURCE *pIn) {
  
  int err_num = 0;
  long c = 0;
  long c2 = 0;
  
  /* Check parameters */
  if ((pFilter == NULL) || (pIn == NULL)) {
    abort();
  }
  
  /* If we're not in pushback mode and we don't have a special
   * condition, we need to read another codepoint */
  if ((!(pFilter->pushback)) &&
        ((pFilter->line_count == 0) || (pFilter->c >= 0))) {
    
    /* Read a codepoint */
    c = snsource_readCPV(pIn);
    if (c < 0) {
      err_num = (int) c;
    }
    
    /* If this is the first codepoint read, and it is the U+FEFF Byte
     * Order Mark (BOM), then skip it by reading again */
    if ((!err_num) && (pFilter->line_count == 0) && (c == 0xfeffL)) {
      c = snsource_readCPV(pIn);
      if (c < 0) {
        err_num = (int) c;
      }
    }
    
    /* If we read a CR, read the LF it must be paired with */
    if ((!err_num) && (c == ASCII_CR)) {
      
      /* We read a CR, so read next character and make sure it is LF */
      c = snsource_readCPV(pIn);
      if (c < 0) {
        err_num = (int) c;
      } else if (c != ASCII_LF) {
        err_num = SNERR_BADCR;
      }
    }
    
    /* If we read a low surrogate, error because it means surrogate is
     * not paired properly */
    if ((!err_num) && (c >= UNICODE_MIN_LO_SUR) && 
          (c <= UNICODE_MAX_LO_SUR)) {
      err_num = SNERR_UNPAIRED;
    }
    
    /* If we read a high surrogate, read next codepoint which must be a
     * low surrogate and then replace the character read with the
     * supplemental codepoint selected by the surrogate pair */
    if ((!err_num) && (c >= UNICODE_MIN_HI_SUR) &&
          (c <= UNICODE_MAX_HI_SUR)) {
      
      /* Read the low surrogate */
      c2 = snsource_readCPV(pIn);
      if (c2 < 0) {
        err_num = (int) c2;
      } else if ((c2 < UNICODE_MIN_LO_SUR) ||
                  (c2 > UNICODE_MAX_LO_SUR)) {
        err_num = SNERR_UNPAIRED;
      }
      
      /* Replace codepoint read with the supplemental codepoint */
      if (!err_num) {
        c = snutf_pair(c, c2);
      }
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
static int snchar_islegal(long c) {
  
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
static int snchar_isatomic(long c) {
  
  int result = 0;
  
  if ((c == ASCII_LPAREN) || (c == ASCII_RPAREN) ||
      (c == ASCII_LSQR) || (c == ASCII_RSQR) ||
      (c == ASCII_COMMA) || (c == ASCII_PERCENT) ||
      (c == ASCII_SEMICOLON) || (c == ASCII_DQUOTE) ||
      (c == ASCII_LCURL) || (c == ASCII_RCURL)) {
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
static int snchar_isinclusive(long c) {
  
  int result = 0;
  
  if ((c == ASCII_DQUOTE) || (c == ASCII_LCURL)) {
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
static int snchar_isexclusive(long c) {
  
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
 * Determine whether a given string consists purely of the given byte.
 * 
 * c is a unsigned byte value in 7-bit range excluding nul (1-127) and
 * pStr points to a null-terminated string.
 * 
 * This function returns non-zero only if the string has exactly one
 * byte, which is equal to c.
 * 
 * Parameters:
 * 
 *   c - the byte
 * 
 *   pStr - pointer to the string
 * 
 * Return:
 * 
 *   non-zero if string is equal to byte, zero if not
 */
static int snchar_strequals(int c, const char *pStr) {
  
  char strb[2];
  int result = 0;
  
  /* Initialize buffer */
  memset(&(strb[0]), 0, 2);
  
  /* Check parameters */
  if ((c < 1) || (c > 127) || (pStr == NULL)) {
    abort();
  }
  
  /* Write character into local buffer */
  strb[0] = (char) c;
  
  /* Compare strings */
  if (strcmp(pStr, &(strb[0])) == 0) {
    result = 1;
  } else {
    result = 0;
  }
  
  /* Return result */
  return result;
}

/*
 * Determine whether a given string consists purely of two given bytes.
 * 
 * c1 and c2 are unsigned byte values in 7-bit range excluding nul
 * (1-127) and pStr points to a null-terminated string.
 * 
 * This function returns non-zero only if the string has exactly two
 * bytes, the first of which equals c1 and the second of which equals
 * c2.
 * 
 * Parameters:
 * 
 *   c1 - the first byte
 * 
 *   c2 - the second byte
 * 
 *   pStr - pointer to the string
 * 
 * Return:
 * 
 *   non-zero if string is equal to the two bytes, zero if not
 */
static int snchar_strequals2(int c1, int c2, const char *pStr) {
  
  char strb[3];
  int result = 0;
  
  /* Initialize buffer */
  memset(&(strb[0]), 0, 3);
  
  /* Check parameters */
  if ((c1 < 1) || (c1 > 127) || (pStr == NULL) ||
      (c2 < 1) || (c2 > 127)) {
    abort();
  }
  
  /* Write characters into local buffer */
  strb[0] = (char) c1;
  strb[1] = (char) c2;
  
  /* Compare strings */
  if (strcmp(pStr, &(strb[0])) == 0) {
    result = 1;
  } else {
    result = 0;
  }
  
  /* Return result */
  return result;
}

/*
 * Read a quoted string.
 * 
 * pBuffer is the buffer into which the string data will be read.  It
 * must be properly initialized.  This function will reset the buffer
 * and then write the string data into it.
 * 
 * pIn is the source to read data from.
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
 *   pIn - the source to read the string data from
 * 
 *   pFilter - the input filter
 * 
 * Return:
 * 
 *   zero if successful, or one of the SNERR constants if error
 */
static int snstr_readQuoted(
    SNBUFFER * pBuffer,
    SNSOURCE * pIn,
    SNFILTER * pFilter) {
  
  int err_num = 0;
  int esc_flag = 0;
  long c = 0;
  
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
        err_num = (int) c;
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
 * pIn is the source to read data from.
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
 *   pIn - the source to read the string data from
 * 
 *   pFilter - the input filter
 * 
 * Return:
 * 
 *   zero if successful, or one of the SNERR constants if error
 */
static int snstr_readCurlied(
    SNBUFFER * pBuffer,
    SNSOURCE * pIn,
    SNFILTER * pFilter) {
  
  int err_num = 0;
  int esc_flag = 0;
  long nest_level = 1;
  long c = 0;
  
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
        err_num = (int) c;
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
 * After this operation, the source and filter will be positioned at the
 * first character that is not whitespace and not part of a comment, or
 * at the first special or error condition that was encountered.
 * 
 * Parameters:
 * 
 *   pIn - the input source
 * 
 *   pFilter - the filter to pass the input through
 */
static void sntk_skip(SNSOURCE *pIn, SNFILTER *pFilter) {
  
  long c = 0;
  
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
     * mode (unless a special condition) and leave the loop */
    if (c != ASCII_POUNDSIGN) {
      if (c >= 0) {
        if (!snfilter_pushback(pFilter)) {
          abort();  /* shouldn't happen */
        }
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
 * pIn is the source to read data from.
 * 
 * pFilter is the input filter to read the data through.  It should be
 * in the proper state.
 * 
 * This function will skip over comments and whitespace before reading
 * the token.
 * 
 * For string tokens, this function only reads the opening token and not
 * the data that follows it.  Input will be positioned such that the
 * next byte read will the first byte of string data.
 * 
 * Note that it is not possible to use this function to iterate through
 * all the tokens in a Shastina source file, because this function
 * doesn't handle string data.
 * 
 * Parameters:
 * 
 *   pBuffer - the buffer to read the token into
 * 
 *   pIn - the input source to read the token from
 * 
 *   pFilter - the input filter
 * 
 * Return:
 * 
 *   zero if successful, or one of the SNERR constants if error
 */
static int sntk_readToken(
    SNBUFFER * pBuffer,
    SNSOURCE * pIn,
    SNFILTER * pFilter) {
  
  int err_num = 0;
  long c = 0;
  long c2 = 0;
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
    err_num = (int) c;
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
      err_num = (int) c2;
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
          err_num = (int) c;
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
 * pIn is the source to read data from.
 * 
 * pFilter is the input filter to read the data through.  It should be
 * in the proper state.
 * 
 * This function differs from sntk_readToken() in that this function can
 * also read string data.
 * 
 * Parameters:
 * 
 *   pToken - the token structure
 * 
 *   pIn - the input source
 * 
 *   pFil - the filter to pass input through
 */
static void sntoken_read(
    SNTOKEN  * pToken,
    SNSOURCE * pIn,
    SNFILTER * pFil) {
  
  int err_num = 0;
  long c = 0;
  
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
    
    } else {
      pToken->status = SNTOKEN_SIMPLE;
    }
  }
  
  /* For simple tokens, distinguish between SIMPLE and FINAL */
  if ((!err_num) && (pToken->status == SNTOKEN_SIMPLE)) {
    if (snchar_strequals2(ASCII_BAR, ASCII_SEMICOLON,
          snbuffer_get(pToken->pKey))) {
      pToken->status = SNTOKEN_FINAL;
    }
  }
  
  /* For string tokens, remove the last character, so the key buffer
   * only has the prefix */
  if ((!err_num) && (pToken->status == SNTOKEN_STRING)) {
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

/*
 * Initialize a Shastina reader state structure.
 * 
 * All Shastina readers must be initialized before they are used, or
 * undefined behavior occurs.
 * 
 * Do not re-initialize a Shastina reader that is already initialized,
 * or a memory leak may occur.
 * 
 * Shastina readers must be fully reset with snreader_reset() before
 * they are released, or a memory leak may occur.
 * 
 * Parameters:
 * 
 *   pReader - the reader structure to initialize
 */
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
    SNREADER_AGSTACK_INIT, SNREADER_AGSTACK_MAX);
  snstack_init(&(pReader->stack_group),
    SNREADER_AGSTACK_INIT, SNREADER_AGSTACK_MAX);
  
  pReader->meta_flag = 0;
  pReader->array_flag = 0;
}

/*
 * Reset a Shastina reader back to its initial state.
 * 
 * If full is non-zero, a full reset will be performed.  If full is
 * zero, a fast reset is performed.  A full reset releases all memory
 * buffers, while a fast reset keeps the memory buffers allocated.
 * 
 * A full reset must be performed on a reader before it is released, or
 * a memory leak occurs.
 * 
 * Parameters:
 * 
 *   pReader - the reader structure to reset
 * 
 *   full - non-zero for full reset, zero for fast reset
 */
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

/*
 * Read an entity from a Shastina source file.
 * 
 * pReader is the reader object, which must be properly initialized.
 * 
 * pEntity is the entity structure that will be filled in.  Its state
 * upon entry to the function is irrelevant.  Upon return, it will hold
 * the results of the operation.  See the structure documentation for
 * further information.
 * 
 * pIn is the input source to read from.
 * 
 * pFilter is the input filter to pass the input through.  It must be
 * properly initialized.
 * 
 * Once an error is encountered, the reader object will return that same
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
 * 
 *   pFilter - the input filter
 */
static void snreader_read(
    SNREADER * pReader,
    SNENTITY * pEntity,
    SNSOURCE * pIn,
    SNFILTER * pFilter) {
  
  int err_code = 0;
  
  /* Check parameters */
  if ((pReader == NULL) || (pEntity == NULL) || (pIn == NULL) ||
      (pFilter == NULL)) {
    abort();
  }
  
  /* Clear provided entity */
  memset(pEntity, 0, sizeof(SNENTITY));
  
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
        /* We've read everything in the queue, so clear it */
        pReader->queue_count = 0;
        pReader->queue_read = 0;
      }
    }
    
  } else {
    /* Error status -- write into the entity */
    pEntity->status = err_code;
  }
}

/*
 * Add an entity with no parameters (type "Z") to the queue of a given
 * reader.
 * 
 * pReader is the reader to add the entity to.
 * 
 * entity is the SNENTITY_ constant describing the kind of entity.
 * 
 * The only entities allowed by this function are:
 * 
 *   - SNENTITY_EOF
 *   - SNENTITY_BEGIN_META
 *   - SNENTITY_END_META
 *   - SNENTITY_BEGIN_GROUP
 *   - SNENTITY_END_GROUP
 * 
 * Passing any other kind of entity code results in a fault.
 * 
 * The queue of the reader must not be full or this function will fault.
 * See SNREADER_MAXQUEUE for more information.
 * 
 * If the reader is in an error state, this call is ignored.
 * 
 * Parameters:
 * 
 *   pReader - the reader object
 * 
 *   entity - the entity code
 */
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

/*
 * Add an entity with one string parameter (type "S") to the queue of a
 * given reader.
 * 
 * pReader is the reader to add the entity to.
 * 
 * entity is the SNENTITY_ constant describing the kind of entity.
 * 
 * s is the string parameter.
 * 
 * The only entities allowed by this function are:
 * 
 *   - SNENTITY_META_TOKEN
 *   - SNENTITY_NUMERIC
 *   - SNENTITY_VARIABLE
 *   - SNENTITY_CONSTANT
 *   - SNENTITY_ASSIGN
 *   - SNENTITY_GET
 *   - SNENTITY_OPERATION
 * 
 * Passing any other kind of entity code results in a fault.
 * 
 * The queue of the reader must not be full or this function will fault.
 * See SNREADER_MAXQUEUE for more information.
 * 
 * If the reader is in an error state, this call is ignored.
 * 
 * Parameters:
 * 
 *   pReader - the reader object
 * 
 *   entity - the entity code
 * 
 *   s - the string parameter
 */
static void snreader_addEntityS(
    SNREADER * pReader,
    int        entity,
    char     * s) {
  
  SNENTITY *pe = NULL;
  
  /* Check parameters */
  if ((pReader == NULL) || (s == NULL)) {
    abort();
  }
  if ((entity != SNENTITY_META_TOKEN) &&
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

/*
 * Add an entity with one long parameter (type "L") to the queue of a
 * given reader.
 * 
 * pReader is the reader to add the entity to.
 * 
 * entity is the SNENTITY_ constant describing the kind of entity.
 * 
 * l is the long parameter.
 * 
 * The only entities allowed by this function are:
 * 
 *   - SNENTITY_ARRAY
 * 
 * Passing any other kind of entity code results in a fault.
 * 
 * The queue of the reader must not be full or this function will fault.
 * See SNREADER_MAXQUEUE for more information.
 * 
 * If the reader is in an error state, this call is ignored.
 * 
 * Parameters:
 * 
 *   pReader - the reader object
 * 
 *   entity - the entity code
 * 
 *   l - the long value
 */
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

/*
 * Add an entity with prefix, type, and data parameters (type "T") to
 * the queue of a given reader.
 * 
 * pReader is the reader to add the entity to.
 * 
 * entity is the SNENTITY_ constant describing the kind of entity.
 * 
 * pPrefix points to the prefix string.
 * 
 * str_type is the string type.  It must be one of the SNSTRING_
 * constants.
 * 
 * pData points to the string data.
 * 
 * The only entities allowed by this function are:
 * 
 *   - SNENTITY_STRING
 *   - SNENTITY_META_STRING
 * 
 * Passing any other kind of entity code results in a fault.
 * 
 * The queue of the reader must not be full or this function will fault.
 * See SNREADER_MAXQUEUE for more information.
 * 
 * If the reader is in an error state, this call is ignored.
 * 
 * Parameters:
 * 
 *   pReader - the reader object
 * 
 *   entity - the entity code
 */
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

/*
 * Perform the array prefix operation.
 * 
 * This should be performed before processing any token except for "]".
 * 
 * pReader is the reader object.  The reader must not be in an error
 * state, or a fault occurs.
 * 
 * This operation only does something if the array flag is on.  In that
 * case, the array flag is turned off, the array and grouping stacks are
 * adjusted to indicate a new array, and a BEGIN_GROUP entity is added
 * to the reader.
 * 
 * These are operations that should have properly been done when the "["
 * token was processed.  But that token needs to know whether the array
 * is empty (followed immediately by a "]" token) or non-empty.  Hence,
 * it sets the array flag and its processing is delayed until the next
 * token, which calls into this array prefix operation to do the delayed
 * array operation if the array flag is set.
 * 
 * This is therefore done before every token except for "]", which
 * handles the situation differently because the empty array case is
 * different.  Hence, this function must *not* be called before the "]"
 * token.
 * 
 * This function may set the reader into an error state.
 * 
 * Parameters:
 * 
 *   pReader - the reader object
 */
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

/* 
 * Read a token from a Shastina source file in an effort to fill the
 * entity queue.
 * 
 * Clients should not use this function directly.  Use snreader_read()
 * instead (which makes use of this function).
 * 
 * The reader must not be in an error state and the queue must be empty.
 * If these conditions are not satisfied, a fault occurs.  A fault may
 * also occur if SNREADER_MAXQUEUE is not large enough -- see that
 * constant for further information.
 * 
 * Not all tokens result in entities being read, so to ensure the entity
 * buffer is filled with at least one entity, call this function in a
 * loop.
 * 
 * Parameters:
 * 
 *   pReader - the reader object
 * 
 *   pIn - the input file
 * 
 *   pFilter - the filter to pass input through
 */
static void snreader_fill(
    SNREADER * pReader,
    SNSOURCE * pIn,
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
  
  /* Get the key string pointer */
  if (!err_code) {
    pks = snbuffer_get(tk.pKey);
  }
  
  /* Perform array prefix operation if not in metacommand mode, except
   * for "]" token */
  if ((!err_code) && (!pReader->meta_flag)) {
    if (tk.status == SNTOKEN_SIMPLE) {
      if (!snchar_strequals(ASCII_RSQR, pks)) {
        /* Simple token except for "]" */
        snreader_arrayPrefix(pReader);
      }
      
    } else {
      /* Not in metacommand mode and not a simple token */
      snreader_arrayPrefix(pReader);
    }
    
    /* Check for errors from the prefix operation */
    err_code = pReader->status;
  }
  
  /* Handle the token types */
  if ((tk.status == SNTOKEN_SIMPLE) && (!err_code)) {
    /* Simple token -- handle non-primitive and primitive cases */
    if (snchar_strequals(ASCII_PERCENT, pks)) {
      /* % token -- enter metacommand mode */
      if (!pReader->meta_flag) {
        pReader->meta_flag = 1;
        snreader_addEntityZ(pReader, SNENTITY_BEGIN_META);
        
      } else {
        /* Nested metacommands */
        err_code = SNERR_METANEST;
      }
      
    } else if (snchar_strequals(ASCII_SEMICOLON, pks)) {
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
      /* Primitive tokens -- first, get first byte */
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
        
      } else if (snchar_strequals(ASCII_LPAREN, pks)) {
        /* Begin group */
        if (snstack_inc(&(pReader->stack_group))) {
          snreader_addEntityZ(pReader, SNENTITY_BEGIN_GROUP);
        } else {
          /* Too much group nesting */
          err_code = SNERR_DEEPGROUP;
        }
        
      } else if (snchar_strequals(ASCII_RPAREN, pks)) {
        /* End group */
        if (snstack_dec(&(pReader->stack_group))) {
          snreader_addEntityZ(pReader, SNENTITY_END_GROUP);
        } else {
          /* Closing parenthesis without an opening parenthesis */
          err_code = SNERR_RPAREN;
        }
        
      } else if (snchar_strequals(ASCII_LSQR, pks)) {
        /* Begin array */
        pReader->array_flag = 1;
        
      } else if (snchar_strequals(ASCII_RSQR, pks)) {
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
              /* Still unclosed parentheses in current element */
              err_code = SNERR_OPENGROUP;
            }
            
          } else {
            /* "]" without a corresponding opening bracket */
            err_code = SNERR_RSQR;
          }
          
        } else {
          /* Empty array */
          pReader->array_flag = 0;
          snreader_addEntityL(pReader, SNENTITY_ARRAY, 0);
        }
        
      } else if (snchar_strequals(ASCII_COMMA, pks)) {
        /* Array separator -- check that array stack is not empty and
         * that value on top of grouping stack is zero, then perform
         * operation */
        if (snstack_count(&(pReader->stack_array)) > 0) {
          if (snstack_peek(&(pReader->stack_group)) == 0) {
            if (snstack_inc(&(pReader->stack_array))) {
              snreader_addEntityZ(pReader, SNENTITY_END_GROUP);
              snreader_addEntityZ(pReader, SNENTITY_BEGIN_GROUP);
              
            } else {
              /* Too many array elements */
              err_code = SNERR_LONGARRAY;
            }
              
          } else {
            /* Open parentheses in current element */
            err_code = SNERR_OPENGROUP;
          }
            
        } else {
          /* Comma used outside of array */
          err_code = SNERR_COMMA;
        }
        
      } else {
        /* Operator */
        snreader_addEntityS(pReader, SNENTITY_OPERATION, pks);
      }
    }
    
  } else if ((tk.status == SNTOKEN_STRING) && (!err_code)) {
    /* String token -- either a normal string or a meta string */
    if (pReader->meta_flag) {
      /* Meta string */
      snreader_addEntityT(pReader, SNENTITY_META_STRING,
        pks, tk.str_type, snbuffer_get(tk.pValue));
    } else {
      /* Normal string */
      snreader_addEntityT(pReader, SNENTITY_STRING,
        pks, tk.str_type, snbuffer_get(tk.pValue));
    }
  
  } else if ((tk.status == SNTOKEN_FINAL) && (!err_code)) {
    /* Final token */
    if (!(pReader->meta_flag)) {
      if ((!pReader->array_flag) &&
            (snstack_count(&(pReader->stack_array)) == 0)) {
        if (snstack_peek(&(pReader->stack_group)) == 0) {
          
          /* Add the EOF entity */
          snreader_addEntityZ(pReader, SNENTITY_EOF);
          
        } else {
          /* Open parentheses group remains */
          err_code = SNERR_OPENGROUP;
        }
      } else {
        /* Open array remains */
        err_code = SNERR_OPENARRAY;
      }
    } else {
      /* Open metacommand remains */
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
 * Public functions
 * ================
 * 
 * (see the header for specifications)
 */

/*
 * snsource_file function.
 */
SNSOURCE *snsource_file(FILE *pFile, int owner) {
  
  SNSOURCE *pSrc = NULL;
  
  /* Check parameters */
  if (pFile == NULL) {
    abort();
  }
  
  /* Call through to construct object */
  if (owner) {
    pSrc = snsource_custom(
              &snsource_file_read,
              &snsource_file_free,
              (void *) pFile);
  } else {
    pSrc = snsource_custom(
              &snsource_file_read,
              NULL,
              (void *) pFile);
  }
  
  /* Return the new source object */
  return pSrc;
}

/*
 * snsource_string function.
 */
SNSOURCE *snsource_string(const char *pStr) {
  
  SNSTRSRC *pStS = NULL;
  
  /* Check parameter */
  if (pStr == NULL) {
    abort();
  }
  
  /* Allocate new structure */
  pStS = (SNSTRSRC *) malloc(sizeof(SNSTRSRC));
  if (pStS == NULL) {
    abort();
  }
  memset(pStS, 0, sizeof(SNSTRSRC));
  
  /* Copy the pointer into the structure */
  pStS->pStr = (const unsigned char *) pStr;
  
  /* Call through to construct object */
  return snsource_custom(
            &snsource_str_read,
            &snsource_str_free,
            (void *) pStS);
}

/*
 * snsource_custom function.
 */
SNSOURCE *snsource_custom(
    int (*read_func)(void *),
    void (*free_func)(void *),
    void *custom) {
  
  SNSOURCE *pSrc = NULL;
  
  /* Check parameters */
  if (read_func == NULL) {
    abort();
  }
  
  /* Allocate structure */
  pSrc = (SNSOURCE *) malloc(sizeof(SNSOURCE));
  if (pSrc == NULL) {
    abort();
  }
  memset(pSrc, 0, sizeof(SNSOURCE));
  
  /* Initialize structure */
  pSrc->pfRead = read_func;
  pSrc->pfDestruct = free_func;
  
  pSrc->read_count = 0;
  pSrc->status = 0;
  pSrc->pCustom = custom;
  
  /* Return the new source object */
  return pSrc;
}

/*
 * snsource_free function.
 */
void snsource_free(SNSOURCE *pSrc) {
  
  /* Only proceed if non-NULL parameter passed */
  if (pSrc != NULL) {
    
    /* If destructor is defined, call it */
    if (pSrc->pfDestruct != NULL) {
      (*(pSrc->pfDestruct))(pSrc->pCustom);
    }
    
    /* Release the structure */
    free(pSrc);
  }
}

/*
 * snsource_bytes function.
 */
long snsource_bytes(SNSOURCE *pSrc) {
  
  /* Check parameter */
  if (pSrc == NULL) {
    abort();
  }
  
  /* Return count */
  return pSrc->read_count;
}

/*
 * snsource_consume function.
 */
int snsource_consume(SNSOURCE *pSrc) {
  
  long c = 0;
  int result = 0;
  
  /* Check parameter */
  if (pSrc == NULL) {
    abort();
  }
  
  /* Keep reading until we get something besides SP HT CR LF */
  for(c = snsource_readCPV(pSrc);
      (c == ASCII_SP) || (c == ASCII_HT) ||
      (c == ASCII_CR) || (c == ASCII_LF);
      c = snsource_readCPV(pSrc));
  
  /* Set result depending on what we stopped on */
  if (c == SNERR_EOF) {
    /* Nothing but whitespace and blank lines present, so succeed */
    result = 1;
  
  } else if (c == SNERR_IOERR) {
    /* I/O error, so return that */
    result = SNERR_IOERR;
    
  } else {
    /* In all other cases, including data bytes besides whitespace and
     * line breaks and other kinds of errors, return a trailer error */
    result = SNERR_TRAILER;
  }
  
  /* Return result */
  return result;
}

/*
 * snparser_alloc function.
 */
SNPARSER *snparser_alloc(void) {
  
  SNPARSER *pParser = NULL;
  
  /* Allocate structure */
  pParser = (SNPARSER *) malloc(sizeof(SNPARSER));
  if (pParser == NULL) {
    abort();
  }
  memset(pParser, 0, sizeof(SNPARSER));
  
  /* Initialize */
  snreader_init(&(pParser->reader));
  snfilter_reset(&(pParser->filter));
  
  /* Return parser */
  return pParser;
}

/*
 * snparser_free function.
 */
void snparser_free(SNPARSER *pParser) {
  
  /* Only do something if not NULL */
  if (pParser != NULL) {
    /* Fully reset reader and release */
    snreader_reset(&(pParser->reader), 1);
    free(pParser);
  }
}

/*
 * snparser_read function.
 */
void snparser_read(
    SNPARSER * pParser,
    SNENTITY * pEntity,
    SNSOURCE * pIn) {
  
  /* Check parameters */
  if ((pParser == NULL) || (pEntity == NULL) || (pIn == NULL)) {
    abort();
  }
  
  /* Call through to reader */
  snreader_read(&(pParser->reader), pEntity, pIn, &(pParser->filter));
}

/*
 * snparser_count function.
 */
long snparser_count(SNPARSER *pParser) {
  
  /* Check parameter */
  if (pParser == NULL) {
    abort();
  }
  
  /* Return line count */
  return snfilter_count(&(pParser->filter));
}

/*
 * snerror_str function.
 */
const char *snerror_str(int code) {
  
  const char *pResult = NULL;
  
  switch (code) {
    
    case SNERR_IOERR:
      pResult = "I/O error";
      break;
    
    case SNERR_EOF:
      pResult = "Unexpected end of file";
      break;
    
    case SNERR_BADCR:
      pResult = "CR must always be followed by LF";
      break;
    
    case SNERR_OPENSTR:
      pResult = "File ends in middle of string";
      break;
    
    case SNERR_LONGSTR:
      pResult = "String is too long";
      break;
    
    case SNERR_NULLCHR:
      pResult = "Nul character encountered in string";
      break;
    
    case SNERR_DEEPCURLY:
      pResult = "Too much curly nesting in string";
      break;
    
    case SNERR_BADCHAR:
      pResult = "Illegal character encountered";
      break;
    
    case SNERR_LONGTOKEN:
      pResult = "Token is too long";
      break;
      
    case SNERR_TRAILER:
      pResult = "Content present after |; token";
      break;
    
    case SNERR_DEEPARRAY:
      pResult = "Too much array nesting";
      break;
    
    case SNERR_METANEST:
      pResult = "Nested metacommands";
      break;
    
    case SNERR_SEMICOLON:
      pResult = "Semicolon used outside of metacommand";
      break;
    
    case SNERR_DEEPGROUP:
      pResult = "Too much group nesting";
      break;
    
    case SNERR_RPAREN:
      pResult = "Right parenthesis outside of group";
      break;
    
    case SNERR_RSQR:
      pResult = "Right square bracket outside array";
      break;
    
    case SNERR_OPENGROUP:
      pResult = "Open group";
      break;

    case SNERR_LONGARRAY:
      pResult = "Array has too many elements";
      break;
    
    case SNERR_UNPAIRED:
      pResult = "Unpaired surrogates encountered in input";
      break;
    
    case SNERR_OPENMETA:
      pResult = "Unclosed metacommand";
      break;
    
    case SNERR_OPENARRAY:
      pResult = "Unclosed array";
      break;
    
    case SNERR_COMMA:
      pResult = "Comma used outside of array or meta";
      break;
    
    case SNERR_UTF8:
      pResult = "Invalid UTF-8 encountered in input";
      break;
    
    default:
      pResult = "Unknown error";
  }
  
  return pResult;
}
