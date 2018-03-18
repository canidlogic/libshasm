#ifndef SHASM_BLOCK_H_INCLUDED
#define SHASM_BLOCK_H_INCLUDED

/*
 * shasm_block.h
 * 
 * Block reader module of libshasm.
 * 
 * This module converts a filtered stream of characters from shasm_input
 * into strings of zero to 65,535 bytes.
 * 
 * For multithreaded applications, this module is safe to use, provided
 * that each SHASM_BLOCK instance is only used from one thread at a 
 * time.
 */

#include "shasm_error.h"
#include "shasm_input.h"

/*
 * The maximum number of data bytes that can be stored in the block
 * reader buffer, as a signed long constant.
 * 
 * This does not include a terminating null.
 */
#define SHASM_BLOCK_MAXSTR (65535L)

/*
 * Structure prototype for the block reader state structure.
 * 
 * The actual structure is defined in the implementation.
 */
struct SHASM_BLOCK_TAG;
typedef struct SHASM_BLOCK_TAG SHASM_BLOCK;

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
 * range is zero up to and including SHASM_BLOCK_MAXSTR (65,535).  This
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
 * again after each call to the reading function, in case the block
 * reader has reallocated the buffer.
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
 * (4) If the token is longer than 65,535 bytes, SHASM_ERR_HUGEBLOCK.
 * 
 * (5) If a filtered character that is not in US-ASCII printing range
 * (0x21-0x7e) and not HT SP or LF is encountered, SHASM_ERR_TOKENCHAR,
 * except if this character occurs within a comment, in which case it is
 * skipped over.
 
 */
int shasm_block_token(SHASM_BLOCK *pb, SHASM_IFLSTATE *ps);

#endif
