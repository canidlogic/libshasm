/*
 * shasm_block.c
 * 
 * Implementation of shasm_block.h
 * 
 * See the header for further information.
 */
#include "shasm_block.h"

/* @@TODO: finish the implementation */

/* @@TODO: finish the local functions given below */

/*
 * Set the input line number associated with the block that has just
 * been read into the buffer.
 * 
 * The provided line number must be in range one up to LONG_MAX, with
 * LONG_MAX being a special value indicating that the line number has
 * overflowed.
 * 
 * If the block reader is in an error state (see shasm_block_status),
 * then the line number of the most recent block will be frozen at
 * LONG_MAX, and calls to this function will be ignored.  The line
 * number of the error that stopped the block reader is handled
 * separately.
 * 
 * Parameters:
 * 
 *   pb - the block reader
 * 
 *   line - the line number to set
 */
static void shasm_block_setLine(SHASM_BLOCK *pb, long line);

/*
 * Clear the block reader's internal buffer to an empty string.
 * 
 * This does not reset the error status of the block reader (see
 * shasm_block_status).
 * 
 * Parameters:
 * 
 *   pb - the block reader to clear
 */
static void shasm_block_clear(SHASM_BLOCK *pb);

/*
 * Append an unsigned byte value (0-255) to the end of the block
 * reader's internal buffer.
 * 
 * This might cause the buffer to be reallocated, so pointers returned
 * by shasm_block_ptr become invalid after calling this function.
 * 
 * This function will fail if the buffer already has SHASM_BLOCK_MAXSTR
 * bytes in it.  In this case, the internal buffer will be cleared and
 * the block reader will be set to error SHASM_ERR_HUGEBLOCK.
 * 
 * If this function is called when the block reader is already in an
 * error state, the function fails and does nothing further.
 * 
 * Parameters:
 * 
 *   pb - the block reader to append a byte to
 * 
 *   c - the unsigned byte value to append (0-255)
 * 
 * Return:
 * 
 *   non-zero if successful, zero if failure
 */
static int shasm_block_addByte(SHASM_BLOCK *pb, int c);
