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
 * An attempt was made to append more than 65,535 bytes to a block
 * reader's internal buffer.
 * 
 * This indicates that a token or string data that decodes to a string
 * longer than 65,535 bytes was encountered in the input file.
 */
#define SHASM_ERR_HUGEBLOCK (1)

#endif
