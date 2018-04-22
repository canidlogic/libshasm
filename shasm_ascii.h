#ifndef SHASM_ASCII_H_INCLUDED
#define SHASM_ASCII_H_INCLUDED

/*
 * shasm_ascii.h
 * 
 * ASCII character constants for use by the libshasm implementation.
 * 
 * This header is not part of the public interface.  It is for use by
 * the implementation source files.
 */

/* Relevant characters */
#define SHASM_ASCII_HT        (0x9 )  /* Horizontal tab */
#define SHASM_ASCII_LF        (0xa )  /* Line feed */
#define SHASM_ASCII_CR        (0xd )  /* Carriage return */
#define SHASM_ASCII_SP        (0x20)  /* Space */
#define SHASM_ASCII_DQUOTE    (0x22)  /* " */
#define SHASM_ASCII_AMPERSAND (0x23)  /* # */
#define SHASM_ASCII_PERCENT   (0x25)  /* % */
#define SHASM_ASCII_SQUOTE    (0x27)  /* ' */
#define SHASM_ASCII_LPAREN    (0x28)  /* ( */
#define SHASM_ASCII_RPAREN    (0x29)  /* ) */
#define SHASM_ASCII_COMMA     (0x2c)  /* , */
#define SHASM_ASCII_SEMICOLON (0x3b)  /* ; */
#define SHASM_ASCII_LSQR      (0x5b)  /* [ */
#define SHASM_ASCII_RSQR      (0x5d)  /* ] */
#define SHASM_ASCII_LCURL     (0x7b)  /* { */
#define SHASM_ASCII_BAR       (0x7c)  /* | */
#define SHASM_ASCII_RCURL     (0x7d)  /* } */

/* Relevant ranges */
#define SHASM_ASCII_VISPRINT_MIN (0x21) /* visible, printing US-ASCII */
#define SHASM_ASCII_VISPRINT_MAX (0x7e)

#endif
