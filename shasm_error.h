#ifndef SHASM_ERROR_H_INCLUDED
#define SHASM_ERROR_H_INCLUDED

/*
 * shasm_error.h
 * 
 * Error code definitions for libshasm.
 */

/*
 * Status code that means there is no error.
 */
#define SHASM_OKAY (0)

/*
 * The raw input reader returned an I/O error.
 */
#define SHASM_ERR_IO (1)

/*
 * The raw input reader returned an End Of File (EOF) condition.
 * 
 * Since Shastina files must end with a "|;" token, it is always an
 * error to encounter an EOF while parsing the file.
 */
#define SHASM_ERR_EOF (2)

/*
 * An attempt was made to append more than 65,535 bytes to a block
 * reader's internal buffer.
 * 
 * This indicates that a token or string data that decodes to a string
 * longer than 65,535 bytes was encountered in the input file.
 * 
 * On platforms with a size_t type less than 32-bit, the length limit is
 * 65,534 bytes (or less, if the SHASM_BLOCK_MAXBUFFER constant has been
 * adjusted in the block reader implementation).
 */
#define SHASM_ERR_HUGEBLOCK (3)

/*
 * A character outside printing US-ASCII range appeared within a token.
 * 
 * The only characters allowed within tokens are in the range 0x21-0x7e.
 */
#define SHASM_ERR_TOKENCHAR (4)

#endif
