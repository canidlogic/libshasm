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
 * longer than 65,535 bytes was encountered in the input file, which is
 * against the Shastina specification.
 * 
 * On platforms with a size_t less than 32 bits or where the constant
 * SHASM_BLOCK_MAXBUFFER has been adjusted down to a lower value, this
 * error never occurs.  Instead, SHASM_ERR_LARGEBLOCK will occur if the
 * block size exceeds the implementation-specific lower limit on block
 * length.
 */
#define SHASM_ERR_HUGEBLOCK (3)

/*
 * An attempt was made to append more than the implementation-specific
 * limit on block length to a block reader's internal buffer.
 * 
 * On platforms with a 32-bit size_t value and where the implementation
 * constant SHASM_BLOCK_MAXBUFFER in the block module has not been
 * adjusted down, this error never occurs.  Instead, SHASM_ERR_HUGEBLOCK
 * will occur if the block size exceeds Shastina's specified maximum
 * of 65,535 bytes.
 * 
 * On platforms with a size_t less than 32-bits or where the constant
 * SHASM_BLOCK_MAXBUFFER has been adjusted to a lower value, this error
 * occurs if a token or string data is within the 65,535-byte limit of
 * the specification, but it exceeds an implementation-specific limit on
 * string length that is lower than this.
 * 
 * The occurrence of this error indicates that although the file can't
 * be parsed on this specific build of libshasm, there might be other
 * builds that could potentially parse the same data without error due
 * to a higher limit on block length.
 */
#define SHASM_ERR_LARGEBLOCK (4)

/*
 * A character outside printing US-ASCII range appeared within a token.
 * 
 * The only characters allowed within tokens are in the range 0x21-0x7e.
 */
#define SHASM_ERR_TOKENCHAR (5)

/*
 * A regular string contained a byte that couldn't be decoded.
 */
#define SHASM_ERR_STRINGCHAR (6)

/*
 * A numeric escape within a regular string had an improper format.
 */
#define SHASM_ERR_BADNUMESC (7)

/*
 * A numeric escape within a regular string selected an entity code
 * value that is above the acceptable range for the numeric escape.
 */
#define SHASM_ERR_NUMESCRANGE (8)

/*
 * A numeric escape within a regular string selected an entity code in
 * Unicode surrogate range, when this is not allowed for the particular
 * numeric escape type.
 */
#define SHASM_ERR_NUMESCSUR (9)

#endif
