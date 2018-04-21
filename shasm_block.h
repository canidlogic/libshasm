#ifndef SHASM_BLOCK_H_INCLUDED
#define SHASM_BLOCK_H_INCLUDED

/*
 * shasm_block.h
 * 
 * Block reader module of libshasm.
 * 
 * This module converts a filtered stream of characters from shasm_input
 * into strings of zero to 32,766 bytes.
 * 
 * Undefined behavior occurs if the platform's size_t has a maximum
 * value less than 32,767.
 * 
 * For multithreaded applications, this module is safe to use, provided
 * that each SHASM_BLOCK instance is only used from one thread at a 
 * time.
 */

#include "shasm_error.h"
#include "shasm_input.h"

/*
 * Basic definitions
 * =================
 */

/*
 * The maximum number of data bytes that can be stored in the block
 * reader buffer, as a signed long constant.
 * 
 * This does not include a terminating null.
 */
#define SHASM_BLOCK_MAXSTR (32766L)

/*
 * Constants for the types of regular strings.
 * 
 * This selects either "" '' or {} strings.
 */
#define SHASM_BLOCK_STYPE_DQUOTE (1)   /* "" strings */
#define SHASM_BLOCK_STYPE_SQUOTE (2)   /* '' strings */
#define SHASM_BLOCK_STYPE_CURLY  (3)   /* {} strings */

/*
 * Constants for the input override modes for regular string decoding.
 * 
 * In "none" mode, there is no input override and everything is handled
 * by the decoding map.
 * 
 * In "utf8" mode, all bytes in the string data that have their most
 * significant bit set will be interpreted as UTF-8 sequences and they
 * will decode to entity codes matching the Unicode codepoint they
 * represent.  Properly paired surrogates encoded in UTF-8 will be
 * replaced by the supplemental character they represent in the decoded
 * entities.  Improperly paired surrogates will be output as an entity
 * code matching the surrogate value.  Improper UTF-8 sequences and
 * overlong UTF-8 encodings will cause errors.
 */
#define SHASM_BLOCK_IMODE_NONE (0)    /* no input override */
#define SHASM_BLOCK_IMODE_UTF8 (1)    /* UTF-8 input override */

/*
 * Constants for the output override modes for regular string encoding.
 * 
 * In "none" mode, there is no output override and everything is handled
 * by the encoding map.
 * 
 * In "utf8" mode, entity codes in range 0x0 through 0x10ffff will be
 * encoded in UTF-8 on output, with supplemental characters represented
 * by a single UTF-8 character, as is standard for UTF-8.
 * 
 * In "cesu8" mode, entity codes in range 0x0 through 0x10ffff will be
 * encoded in UTF-8 on output, with supplemental characters first
 * represented as a surrogate pair, and then each surrogate character
 * encoded in UTF-8.  This is not standard UTF-8 handling for
 * supplemental characters, but it is sometimes useful.
 * 
 * In "u16le" mode, entity codes in range 0x0 through 0x10ffff will be
 * encoded in UTF-16 on output, with little endian byte order.
 * 
 * In "u16be" mode, entity codes in range 0x0 through 0x10ffff will be
 * encoded in UTF-16 on output, with big endian byte order.
 * 
 * In "u32le" mode, entity codes in range 0x0 through 0x10ffff will be
 * encoded in UTF-32 on output, with little endian byte order.
 * 
 * In "u32be" mode, entity codes in range 0x0 through 0x10ffff will be
 * encoded in UTF-32 on output, with big endian byte order.
 */
#define SHASM_BLOCK_OMODE_NONE (0)    /* no output override */
#define SHASM_BLOCK_OMODE_UTF8 (1)    /* UTF-8 output override */
#define SHASM_BLOCK_OMODE_CESU8 (2)   /* CESU-8 output override */
#define SHASM_BLOCK_OMODE_U16LE (3)   /* UTF-16 little endian */
#define SHASM_BLOCK_OMODE_U16BE (4)   /* UTF-16 big endian */
#define SHASM_BLOCK_OMODE_U32LE (5)   /* UTF-32 little endian */
#define SHASM_BLOCK_OMODE_U32BE (6)   /* UTF-32 big endian */

/*
 * Structure prototype for the block reader state structure.
 * 
 * The actual structure is defined in the implementation.
 */
struct SHASM_BLOCK_TAG;
typedef struct SHASM_BLOCK_TAG SHASM_BLOCK;

/*
 * Decoding map
 * ============
 */

/*
 * Callback type for resetting the decoding map to its initial state.
 * 
 * The initial state is the empty string.  The void * parameter is the
 * pass-through parameter.
 */
typedef void (*shasm_block_fp_reset)(void *);

/*
 * Attempt to branch to another node.
 * 
 * The void * parameter is the pass-through parameter.  The int
 * parameter is an unsigned byte value (0-255) that represents the next
 * character to branch to.
 * 
 * If the decoding map has successfully branched to a new node, return
 * a non-zero value.  If no branch exists corresponding to the provided
 * unsigned byte value, stay on the current node and return a zero
 * value.
 */
typedef int (*shasm_block_fp_branch)(void *, int);

/*
 * Read the entity code associated with the current node.
 * 
 * If there is no associated entity code, return a negative value.
 * Otherwise, a value greater than or equal to zero is interpreted as
 * an entity code.
 * 
 * The void * parameter is the pass-through parameter.
 */
typedef long (*shasm_block_fp_entity)(void *);

/*
 * Structure for storing callback information related to the decoding
 * map used during the decoding phase of regular string data processing.
 * 
 * It is up to the client implementation to decide what kind of data
 * structure to use to implement the decoding map.  The interface within
 * this structure works similar to a "trie."  Its initial position is
 * the node corresponding to the empty string.  Branch operations allow
 * movement to a node that has one additional character beyond the
 * current node.  Each node may have an entity value associated with it.
 * 
 * The way keys and values are encoded is that one starts at the root
 * with the empty string and performs zero or more branch operations to
 * add the characters in the key one by one until the node corresponding
 * to the full key value has been reached.  If there is a value (an
 * entity code) associated with the key, it will be stored at the node
 * that has been reached after this operation.
 * 
 * Each character in the key is an unsigned byte value (0-255).  Each
 * value is an entity code, which may have any non-negative signed long
 * value.
 */
typedef struct {
  
  /*
   * Parameter that is passed through to each callback.
   * 
   * The interpretation of this parameter is entirely up to the client.
   * It may be NULL if not needed.
   */
  void *pCustom;
  
  /*
   * Reset callback.
   * 
   * See the function pointer type for a specification.
   * 
   * This parameter may not be NULL.
   */
  shasm_block_fp_reset fpReset;
  
  /*
   * Branch callback.
   * 
   * See the function pointer type for a specification.
   * 
   * This parameter may not be NULL.
   */
  shasm_block_fp_branch fpBranch;
  
  /*
   * Entity callback.
   * 
   * See the function pointer type for a specification.
   * 
   * This parameter may not be NULL.
   */
  shasm_block_fp_entity fpEntity;
  
} SHASM_BLOCK_DECODER;

/*
 * Numeric escapes
 * ===============
 */

/*
 * Structure representing information about a numeric escape.
 * 
 * Numeric escapes are an optional feature supported during regular
 * string data decoding.  This feature allows the numeric value of a
 * desired entity code to be embedded within the string data as a
 * sequence of base-16 or base-10 ASCII digits.  This structure
 * specifies how to decode the sequence of digits.
 */
typedef struct {
  
  /*
   * Flag for selecting base-16 mode.
   * 
   * If this field is non-zero, then each digit is a base-16 digit,
   * which includes ASCII decimal digits 0-9, ASCII lowercase letters
   * a-f, and ASCII uppercase letters A-F.
   * 
   * If this field is zero, then each digit is a base-10 (decimal)
   * digit, which includes ASCII decimal digits 0-9.
   */
  int base16;
  
  /*
   * The minimum number of digits that must be provided in a numeric
   * escape.
   * 
   * This value must be one or greater.  At least this many digits must
   * be present in the escape or there will be an error.
   */
  int min_len;
  
  /*
   * The maximum number of digits that must be provided in a numeric
   * escape.
   * 
   * This value must either be -1, indicating that there is no maximum,
   * or it must be greater than or equal to min_len.  At most this many
   * digits may be present in the escape.  The numeric escape does not
   * proceed beyond this many digits, even if the next character in
   * input would also count as a digit.
   */
  int max_len;
  
  /*
   * The maximum entity value that may be encoded in the digit sequence.
   * 
   * The sequence of one or more digits encodes an unsigned value of
   * zero up to and including max_entity.  If the encoded value is
   * greater than max_entity, there is an error.
   */
  long max_entity;
  
  /*
   * Flag for blocking the Unicode surrogate range.
   * 
   * If this value is non-zero, then decoded entity values in the range
   * 0xD800 up to and including 0xDFFF will cause an error, since these
   * correspond to a Unicode high or low surrogate.
   * 
   * This is useful when entity codes correspond to Unicode codepoints,
   * and the client wishes to disallow surrogates.  Note, however, that
   * entity codes are not required to correspond to Unicode codepoints.
   * 
   * If this value is zero, then the full range of zero up to and
   * including max_entity is available for encoded entity values.
   * 
   * It is acceptable to set this flag even if max_entity is less than
   * 0xD800.  (The flag won't do anything in this case.)
   */
  int block_surrogates;
  
  /*
   * The terminal byte value.
   * 
   * This is either -1, indicating that there is no terminal byte, or it
   * is an unsigned byte value (0-255).
   * 
   * In base-16 mode, the terminal may not correspond to the ASCII value
   * for decimal digits 0-9, lowercase letters a-f, or uppercase letters
   * A-F.
   * 
   * In base-10 mode, the terminal may not correspond to the ASCII value
   * for decimal digits 0-9.
   * 
   * If the terminal byte value is specified, then the digit sequence
   * must end with this terminal byte value or there will be an error.
   * 
   * This is useful for handling escaping systems where the digit
   * sequence ends with a specific character, such as a semicolon.
   */
  int terminal;
  
} SHASM_BLOCK_NUMESCAPE;

/*
 * Callback for querying whether an input entity code represents a
 * numeric escape.
 * 
 * The void * is the pass-through parameter.  The long value is the
 * entity code to check, which must not be negative.
 * 
 * If the provided entity code is for the start of a numeric escape,
 * then the function should fill in information about the numeric
 * escape in the provided SHASM_BLOCK_NUMESCAPE structure (see that
 * structure for further information) and return non-zero.
 * 
 * In this case, the entity code for the numeric escape is not passed
 * through to the output encoder.  Instead, the numeric escape will be
 * decoded and the entity value encoded in the numeric escape will
 * then be passed through to the output encoder.
 * 
 * If the provided entity code is not for the start of a numeric
 * escape, then the function should return zero and it can ignore the
 * SHASM_BLOCK_NUMESCAPE parameter.
 */
typedef int (*shasm_block_fp_qesc)(
    void *,
    long,
    SHASM_BLOCK_NUMESCAPE *);

/*
 * Structure for storing callback information related to numeric escapes
 * used during the decoding phase of regular string data processing.
 * 
 * This callback lets the client specify which decoded entity codes in
 * the input (if any) should be interpreted as numeric escapes, and
 * provide definitions of the kind of numeric escapes in use.
 */
typedef struct {
  
  /*
   * Parameter that is passed through to the callback.
   * 
   * The interpretation of this parameter is entirely up to the client.
   * It may be NULL if not needed.
   */
  void *pCustom;
  
  /*
   * Escape query callback.
   * 
   * See the function pointer type for a specification.
   * 
   * This function pointer is allowed to be NULL.  In that case, it will
   * be assumed that there are no numeric escapes.
   */
  shasm_block_fp_qesc fpEscQuery;
  
} SHASM_BLOCK_ESCLIST;

/*
 * Encoding table
 * ==============
 */

/*
 * Callback for querying the encoding map.
 * 
 * The void * parameter is the pass-through parameter.
 * 
 * The first long parameter is the entity code key, which must not be a
 * negative value.
 * 
 * The unsigned char * and the second long parameter are a pointer to a
 * buffer to receive the output sequence of bytes, and the length of 
 * this buffer in bytes.
 * 
 * The return value is the number of output bytes the provided entity
 * maps to.  It must not be negative.  If it is zero, it means the 
 * entity has no associated output bytes (or that the entity is not a
 * recognized key value).  In this case, the output buffer can be
 * completely ignored.
 * 
 * If the return value is greater than zero and less than or equal to
 * the length of the passed buffer, then the buffer will have been 
 * filled in with the appropriate output sequence and the return value
 * determines how many bytes to use.  The output sequence need not be
 * null terminated, and null bytes are allowed in the output data -- the
 * return value determines how many bytes in the output buffer are 
 * actually used.
 * 
 * If the return value is greater than the length of the passed buffer,
 * then it means the buffer wasn't large enough to store the output
 * bytes.  The output buffer can be completely ignored in this case.
 * 
 * It is allowed for the buffer pointer to be NULL and the buffer length
 * to be zero.  This can be used to check whether a particular key maps
 * to a non-empty output sequence, and what length of buffer needs to be
 * allocated to store a particular output sequence.
 * 
 * Fetching a particular output sequence might be handled as a loop that
 * proceeds until the buffer has been expanded enough to fit the output
 * sequence.  If the callback returns it needs a larger buffer, the
 * buffer is expanded and the callback is invoked again.  This proceeds
 * until the query finally works.  This also allows the encoding map to
 * change as the encoding process goes along.
 */
typedef long (*shasm_block_fp_map)(void *, long, unsigned char *, long);

/*
 * Structure for storing callback information related to the encoding
 * map used during the encoding phase of regular string data processing.
 * 
 * The encoding map is a key/value map where entity codes received from
 * the regular string data decoder are the key and the value is a
 * sequence of zero or more unsigned byte values (0-255) to output in
 * the resulting string.
 * 
 * For unrecognized entity keys, an empty sequence zero bytes in length
 * should be returned.
 */
typedef struct {
  
  /*
   * Parameter that is passed through to the callback.
   * 
   * The interpretation of this parameter is entirely up to the client.
   * It may be NULL if not needed.
   */
  void *pCustom;
  
  /*
   * Encoding map callback.
   * 
   * See the function pointer type for a specification.
   * 
   * If this function pointer is NULL, the encoding table is assumed to
   * be empty, with every key mapping to an empty byte sequence.  This
   * could be appropriate if an output override is being used.
   */
  shasm_block_fp_map fpMap;
  
} SHASM_BLOCK_ENCODER;

/*
 * String type parameters
 * ======================
 */

/*
 * Structure for defining how regular string data is to be read.
 * 
 * This structure provides all the parameters necessary for the decoding
 * and encoding phases for regular string data.
 */
typedef struct {
  
  /*
   * The string type.
   * 
   * This determines whether the string data is a "" '' or {} string.
   * 
   * It must be one of the SHASM_BLOCK_STYPE constants.
   */
  int stype;
  
  /*
   * The decoding map to use during the decoding phase.
   * 
   * Byte sequences in the string data are converted to entity codes by
   * consulting this decoding table using first match and then longest
   * match to resolve ambiguous cases.
   * 
   * However, certain keys that are associated with entity codes in the
   * decoding map will be ignored.  The following keys are ignored:
   * 
   * (1) The empty string
   * 
   * (2) In a "" string, any key that has a " character in a position
   * other than the last (or only) character.  In a '' string, any key
   * that has a ' character in a position other than the last (or only)
   * character.  In a {} string, and key that has a { or } character in
   * a position other than the last (or only) character.
   * 
   * (3) In a "" string, the key consisting only of "
   * 
   * (4) In a '' string, the key consisting only of '
   * 
   * (5) In a {} string, the key consisting only of } is ignored if the
   * curly bracket nesting level is the same level it was at the start
   * of the string data; else it can be matched in the decoding map.
   * 
   * (6) If an input override is selected (see i_over) then any key
   * containing a byte falling in a certain range of input bytes
   * specific to the input override will be ignored.
   */
  SHASM_BLOCK_DECODER dec;
  
  /*
   * The input override to use, or SHASM_BLOCK_IMODE_NONE for no input
   * override.
   * 
   * If an input override is in place, then certain bytes of the string
   * data may be processed by the override rather than being handled in
   * the normal way by the decoding map.
   * 
   * This field must be one of the SHASM_BLOCK_IMODE constants.
   */
  int i_over;
  
  /*
   * The numeric escape list.
   * 
   * Each entity code that is decoded will be checked against the
   * numeric escape list.  If the entity code is on the numeric escape
   * list, the entity code will not be sent to the encoder.  Instead, a
   * numeric escape will be read and the decoded entity value will be
   * sent to the encoder.
   */
  SHASM_BLOCK_ESCLIST elist;
  
  /*
   * The encoding table to use during the encoding phase of interpreting
   * regular string data.
   * 
   * Entity codes received from the decoder will be converted into
   * sequences of output bytes using this encoding table.  The only
   * exception is that if one of the output override modes is selected
   * (see o_over), then certain ranges of entity codes will be handled
   * by the output override rather than by the encoding table.
   */
  SHASM_BLOCK_ENCODER enc;
  
  /*
   * The output override to use, or SHASM_BLOCK_OMODE_NONE for no output
   * override.
   * 
   * If an output override is in place, then certain ranges of entity
   * codes may be processed by the override rather than being handled in
   * the normal way by the encoding table.
   * 
   * This field must be one of the SHASM_BLOCK_OMODE constants.
   */
  int o_over;
  
  /*
   * A flag indicating whether an output override is in "strict" mode.
   * 
   * This flag is ignored if no output override is in place.
   * 
   * If an output override is in place and if strict mode is selected,
   * then the output override will not cover entity codes in Unicode
   * surrogate range (0xD800 up to and including 0xDFFF), with these
   * surrogate codepoints instead being handled in the normal manner by
   * the encoding table.
   * 
   * If an output override is in place and if strict mode is not
   * selected, then Unicode surrogate range will be covered and handled
   * like any other codepoint (which allows for improperly paired
   * surrogates).
   */
  int o_strict;
  
} SHASM_BLOCK_STRING;

/*
 * Public functions
 * ================
 */

/*
 * Allocate a block reader.
 * 
 * The returned block reader should eventually be freed with
 * shasm_block_free.
 * 
 * Return:
 * 
 *   a new block reader
 */
SHASM_BLOCK *shasm_block_alloc(void);

/*
 * Free a block reader.
 * 
 * If NULL is passed, this call is ignored.
 * 
 * Parameters:
 * 
 *   pb - the block reader to free, or NULL
 */
void shasm_block_free(SHASM_BLOCK *pb);

/*
 * Determine the status of a block reader.
 * 
 * This returns one of the codes defined in shasm_error.h
 * 
 * If the return value is SHASM_OKAY, then it means the block reader is
 * functional and not in an error state.  pLine is ignored in this case.
 * 
 * If the return value is something else, it means the block reader is
 * in an error state.  The return value is an error code from
 * shasm_error.  If pLine is not NULL, the input line number the error
 * occurred at will be saved to *pLine, or LONG_MAX will be written if
 * the line count overflowed.
 * 
 * Parameters:
 * 
 *   pb - the block reader to query
 * 
 *   pLine - pointer to variable to receive the line number if there is
 *   an error, or NULL
 * 
 * Return:
 * 
 *   SHASM_OKAY if block reader is functional, or else an error code
 *   from shasm_error
 */
int shasm_block_status(SHASM_BLOCK *pb, long *pLine);

/*
 * The number of bytes of data stored in the block reader buffer.
 * 
 * This is initially zero.  This will always return zero if the block
 * reader is in an error state (see shasm_block_status).
 * 
 * After a successful block reading function, this will be the number of
 * bytes in the result string that has been read into the buffer.  The
 * range is zero up to and including SHASM_BLOCK_MAXSTR (32,766).  This
 * does not include a terminating null character.
 * 
 * Parameters:
 * 
 *   pb - the block reader to query
 * 
 * Return:
 * 
 *   the number of bytes present in the string buffer
 */
long shasm_block_count(SHASM_BLOCK *pb);

/*
 * Get a pointer to the block reader's data buffer.
 * 
 * If the client is expecting a null-terminated string pointer,
 * null_term should be set to non-zero.  The function will return NULL
 * in this case if the string data includes null bytes (and is therefore
 * impossible to safely null terminate).
 * 
 * If the client can handle data that includes null bytes, then set
 * null_term to zero and use shasm_block_count to determine how many
 * bytes of data are stored in the buffer.
 * 
 * The provided pointer is to the internal buffer of the string reader.
 * The client should not modify it or try to free it.  The pointer is
 * valid until another reading function is called or the block reader is
 * freed (whichever occurs first).  The client should call this function
 * again after each call to a reading function, in case the block reader
 * has reallocated the buffer.
 * 
 * If the length of the data is zero, then the returned pointer will
 * point to a string consisting solely of a null termination byte.
 * 
 * After each successful block read function, the resulting string will
 * be stored in this buffer.  If the block reader is in an error state
 * (see shasm_block_status), then an empty string will be stored in the
 * buffer.
 * 
 * Parameters:
 * 
 *   pb - the block reader to query
 * 
 *   null_term - non-zero if the client expects a null-terminated
 *   string, zero if the client can handle strings including null bytes
 * 
 * Return:
 * 
 *   a pointer to the block reader's internal buffer, or NULL if the
 *   null_term flag was set but the string data includes null bytes
 */
unsigned char *shasm_block_ptr(SHASM_BLOCK *pb, int null_term);

/*
 * Return the input line number that the block that was just read began
 * at.
 * 
 * If no blocks have been read yet, this function will return a value of
 * one.  This function will return LONG_MAX if the block reader is in
 * an error state (see shasm_block_status).
 * 
 * Otherwise, this function returns the line number of the start of the
 * most recent block, or LONG_MAX if the line number overflowed.
 * 
 * Parameters:
 * 
 *   pb - the block reader to query
 * 
 * Return:
 * 
 *   the line number or LONG_MAX
 */
long shasm_block_line(SHASM_BLOCK *pb);

/*
 * Read a token from the provided input filter chain into the block
 * reader's internal buffer.
 * 
 * If the block reader is already in an error state when this function
 * is called (see shasm_block_status), then this function fails
 * immediately without performing any operation.
 * 
 * The provided input filter chain may be in pushback mode when passed
 * to this function.
 * 
 * This function begins by reading zero or more bytes of whitespace and
 * comments from input.  Whitespace is defined as filtered ASCII
 * characters Space (SP), Horizontal Tab (HT), and Line Feed (LF).
 * (Carriage Return CR characters are filtered out by the input filter
 * chain.)  Comments begin with an ASCII ampersand and proceed up to and
 * including the next LF.
 * 
 * After the function has skipped over whitespace and comments, the
 * function clears the block reader's internal buffer and sets the line
 * number to the line number from the input filter chain at the first
 * character of the token.
 * 
 * The function then reads one or more token characters into the block
 * reader's buffer.  The following rules are used:
 * 
 * (1) If the first character is one of ( ) [ ] , % ; " ' { then the
 * token consists of just that character.
 * 
 * (2) |; is a complete token, regardless of what follows the semicolon.
 * 
 * (3) If neither (1) nor (2) apply, then read bytes until a stop
 * character is encountered (see below for a definition).  Inclusive
 * stop characters are included as part of the token, while exclusive
 * stop characters are not included in the token.  Only non-whitespace,
 * printing US-ASCII characters (0x21 - 0x7e) are allowed within tokens.
 * 
 * Exclusive stop characters are:  HT SP LF ( ) [ ] , % ; #
 * Inclusive stop characters are:  "  '  {
 * 
 * If the operation is successful, then the input filter chain will be
 * positioned to read the byte immediately after the last byte of the
 * token that was just read.  (It may be in pushback mode.)  The block
 * reader's buffer will contain the token, which may always be treated
 * as a null-terminated string since null bytes aren't allowed within
 * tokens.  The line number of the most recent block will be set to the
 * line number of the input reader at the first character of the token.
 * 
 * The operation may fail for the following reasons:
 * 
 * (1) If the block reader was already in an error state when this
 * function is called, the function fails without changing the error
 * state.
 * 
 * (2) If there is an I/O error, SHASM_ERR_IO.
 * 
 * (3) If EOF is encountered, SHASM_ERR_EOF.
 * 
 * (4) If the token is more than 32,766 bytes, SHASM_ERR_HUGEBLOCK.
 * 
 * (5) If a filtered character that is not in US-ASCII printing range
 * (0x21-0x7e) and not HT SP or LF is encountered, SHASM_ERR_TOKENCHAR,
 * except if this character occurs within a comment, in which case it is
 * skipped over.
 *
 * Parameters:
 * 
 *   pb - the block reader
 * 
 *   ps - the input filter chain to read from
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int shasm_block_token(SHASM_BLOCK *pb, SHASM_IFLSTATE *ps);

/*
 * Read a regular string from the provided input filter chain into the
 * block reader's internal buffer.
 * 
 * If the block reader is already in an error state when this function
 * is called (see shasm_block_status), then this function fails
 * immediately without performing any operation.
 * 
 * The provided input filter chain may be in pushback mode when passed
 * to this function.
 * 
 * The opening quote or bracket (either a double quote, apostrophe, or
 * opening curly bracket) is assumed to have occurred at the end of the
 * token that indicated that string data follows.  Therefore, this
 * function begins right with the first byte of string data that comes
 * immediately after the opening quote or bracket.  However, the closing
 * quote or bracket of the string data *is* included in the string data,
 * so after this function concludes the next character read will be the
 * character that comes immediately after the closing quote or bracket.
 *  
 * The output bytes are placed into the block buffer.  If the operation
 * is successful, then the input filter chain will be positioned to read
 * the byte immediately after closing quote or bracket of the string
 * data that was just read, the block reader's buffer will contain the
 * output string data, and the line number of the most recent block will
 * be set to the line number of the input reader at the start of the
 * string data.
 * 
 * If the operation fails, an error code will be recorded, available
 * through shasm_block_status.  The following error codes are possible:
 * 
 * - SHASM_ERR_EOF if an EOF was encountered in input.
 * - SHASM_ERR_IO if there was an I/O error reading input.
 * - SHASM_ERR_OVERSPEC if the speculation buffer overflowed.
 * - SHASM_ERR_STRCHAR if an input byte can't be decoded.
 * - SHASM_ERR_STRNEST if the curly nesting level overflowed.
 * - SHASM_ERR_NUMESCRANGE if a numeric escape was out of range.
 * - SHASM_ERR_BADNUMESC if there was an invalid numeric escape.
 * - SHASM_ERR_NUMESCSUR if an illegal numeric escape surrogate occurs.
 * - SHASM_ERR_HUGEBLOCK if the output string is too large.
 * 
 * Parameters:
 * 
 *   pb - the block reader
 * 
 *   ps - the input filter chain to read from
 * 
 *   sp - the string type parameters to use, filled in by the client
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int shasm_block_string(
    SHASM_BLOCK *pb,
    SHASM_IFLSTATE *ps,
    const SHASM_BLOCK_STRING *sp);

#endif
