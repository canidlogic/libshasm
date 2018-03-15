#ifndef SHASM_INPUT_H_INCLUDED
#define SHASM_INPUT_H_INCLUDED

/*
 * shasm_input.h
 * 
 * Input filter module of libshasm.
 * 
 * This module implements the input filter chain described in the
 * Shastina Specification.
 * 
 * For multithreaded applications, this module is safe to use, provided
 * that each SHASM_IFLSTATE instance is only used from one thread at a
 * time, and provided that the raw input callback is safe to call from
 * multiple threads (but only from one thread at a time).
 */

#include <limits.h>
#include <stddef.h>

/*
 * Special state codes that the shasm_fp_input callback can return.
 * 
 * The INVALID state is used for buffers, to indicate that nothing is
 * stored in the buffer.  It must not conflict with the other codes, and
 * it is not acceptable to return it from the input function.
 */
#define SHASM_INPUT_EOF     (-1  )  /* End of file */
#define SHASM_INPUT_IOERR   (-2  )  /* I/O error   */
#define SHASM_INPUT_INVALID (-100)  /* Not a valid return code */

/*
 * Function pointer type for the input reader function.
 * 
 * This is the callback that Shastina's input filter chain uses to read
 * raw input bytes from the actual input.
 * 
 * The function takes a single argument, which is a custom pass-through
 * pointer.
 * 
 * The return value is the next byte read, which is either an unsigned
 * byte value (0-255) or SHASM_INPUT_EOF if no more bytes to read, or
 * SHASM_INPUT_IOERR if there was an I/O error trying to read a byte.
 * 
 * Once the function returns SHASM_INPUT_EOF or SHASM_INPUT_IOERR, it
 * will not be called again by the input filter chain.
 */
typedef int (*shasm_fp_input)(void *);

/*
 * Structure prototype for the input filter state structure.
 * 
 * The actual structure is defined in the implementation.
 */
struct SHASM_IFLSTATE_TAG;
typedef struct SHASM_IFLSTATE_TAG SHASM_IFLSTATE;

/*
 * Allocate a new input filter chain.
 * 
 * Pass the callback for reading raw input bytes, as well as the custom
 * pass-through parameter to pass to this callback (NULL is allowed).
 * 
 * The returned input filter chain should eventually be freed with
 * shasm_input_free.
 * 
 * Parameters:
 * 
 *   fpin - the raw input callback
 * 
 *   pCustom - the custom pass-through parameter
 * 
 * Return:
 * 
 *   a new input filter chain
 */
SHASM_IFLSTATE *shasm_input_alloc(shasm_fp_input fpin, void *pCustom);

/*
 * Free an allocated input filter chain.
 * 
 * The function call is ignored if the passed pointer is NULL.
 * 
 * Parameters:
 * 
 *   ps - the input filter chain to free
 */
void shasm_input_free(SHASM_IFLSTATE *ps);

/*
 * Return whether the underlying raw input begins with a UTF-8 Byte
 * Order Mark (BOM) that the input filter chain filtered out.
 * 
 * This can be used before any filtered bytes have been read.  In this
 * case, the function will read and buffer up to three bytes from the
 * beginning of raw input to check for the BOM.
 * 
 * Parameters:
 * 
 *   ps - the input filter state
 * 
 * Return:
 * 
 *   non-zero if a UTF-8 BOM is present at the start of raw input, zero
 *   if not
 */
int shasm_input_hasbom(SHASM_IFLSTATE *ps);

/*
 * Get the current line number of input.
 * 
 * The line number starts at one and increments each time a filtered LF
 * is read.  The line count filter handles updating the line count.  If
 * the line count would overflow the range of a signed long, it stays at
 * LONG_MAX.  LONG_MAX should therefore be interpreted as a line count
 * overflow.
 * 
 * The line count filter happens before the pushback buffer, so
 * unreading a line break will not unread the line number change.  This
 * means the line count may be off by one right next to a line break.
 * 
 * Parameters:
 * 
 *   ps - the input filter state structure
 * 
 * Return:
 * 
 *   the line count, or LONG_MAX if line count overflow
 */
long shasm_input_count(SHASM_IFLSTATE *ps);

/*
 * Get the next byte of input that has been passed through the input
 * filter chain.
 * 
 * The return value is either an unsigned byte value (0-255) or
 * SHASM_INPUT_EOF if at end of input or SHASM_INPUT_IOERR if there has
 * been an I/O error.
 * 
 * This is the function to use to get a byte from the input filter
 * chain.
 * 
 * Parameters:
 * 
 *   ps - the input filter state
 * 
 * Return:
 * 
 *   the unsigned byte value of the next byte (0-255), or
 *   SHASM_INPUT_EOF, or SHASM_INPUT_IOERR
 */
int shasm_input_get(SHASM_IFLSTATE *ps);

/*
 * Backtrack by one filtered input character.
 * 
 * This function activates pushback mode, which means the pushback
 * buffer filter at the end of the input filter chain will return the
 * most recent character read next time it is called rather than reading
 * another character from the underlying filter chain.
 * 
 * It is a fault to use this function when pushback mode is already
 * active (that is, to try to backtrack more than one character).  It is
 * also a fault to use this function if nothing has been read from the
 * input filter chain (using shasm_input_get) yet.
 * 
 * However, provided that the client does not attempt to backtrack more
 * than one character, it is possible to backtrack and reread the same
 * character as many times as the client desires.
 * 
 * Note that the pushback buffer filter occurs after the line count
 * filter in the input filter chain.  This means that backtracking does
 * not affect the line count, which means that the line count may be off
 * by one right next to a filtered LF.
 * 
 * Parameters:
 * 
 *   ps - the input filter state
 */
void shasm_input_back(SHASM_IFLSTATE *ps);

#endif
