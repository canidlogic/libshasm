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
 * An attempt was made to append more than 32,766 bytes to a block
 * reader's internal buffer.
 * 
 * This indicates that a token or string data that decodes to a string
 * longer than 32,766 bytes was encountered in the input file, which is
 * against the Shastina specification.
 */
#define SHASM_ERR_HUGEBLOCK (3)

/*
 * A character outside printing US-ASCII range appeared within a token.
 * 
 * The only characters allowed within tokens are in the range 0x21-0x7e.
 */
#define SHASM_ERR_TOKENCHAR (4)

/*
 * A regular string contained a byte that couldn't be decoded.
 */
#define SHASM_ERR_STRCHAR (5)

/*
 * A numeric escape within a regular string had an improper format.
 */
#define SHASM_ERR_BADNUMESC (6)

/*
 * A numeric escape within a regular string selected an entity code
 * value that is above the acceptable range for the numeric escape.
 */
#define SHASM_ERR_NUMESCRANGE (7)

/*
 * A numeric escape within a regular string selected an entity code in
 * Unicode surrogate range, when this is not allowed for the particular
 * numeric escape type.
 */
#define SHASM_ERR_NUMESCSUR (8)

/*
 * The speculation buffer in the block reader exceeded maximum capacity.
 * 
 * This should only happen if a decoding map is provided that includes
 * huge keys 32 kilobytes or greater in length.
 */
#define SHASM_ERR_OVERSPEC (9)

/*
 * A regular string of the {} type had too much curly bracket nesting.
 * 
 * This should only happen if there are literally billions of nesting
 * levels within a regular string.
 */
#define SHASM_ERR_STRNEST (10)

#endif
