# NAME

Shastina::Const - Define constants for the Shastina library.

# SYNOPSIS

    use Shastina::Const qw(:CONSTANTS :ERROR_CODES snerror_str);
    printf "Error message: %s\n", snerror_str(SNERR_EOF);

# DESCRIPTION

This module defines various Shastina constants that can be imported, as
well as a function that maps error codes to error messages.

Use the tag `:CONSTANTS` to import all constants except the error
codes.

Use the tag `:ERROR_CODES` to import all the error codes.  This should
only be necessary if you need to react to specific error codes.  In the
more common case where you just pass error codes through to
`snerror_str` or ignore specific codes, you do not need to import the
error codes.

This module also allows you to import the `snerror_str` function.  This
function takes an error code and returns an appropriate error message
string.

# CONSTANTS

## Entity type constants

The _entity type constants_ identify the different types of entities
that may appear in a Shastina file:

    SNENTITY_EOF           End Of File
    SNENTITY_STRING        String literal
    SNENTITY_BEGIN_META    Begin metacommand
    SNENTITY_END_META      End metacommand
    SNENTITY_META_TOKEN    Metacommand token
    SNENTITY_META_STRING   Metacommand string
    SNENTITY_NUMERIC       Numeric literal
    SNENTITY_VARIABLE      Declare variable
    SNENTITY_CONSTANT      Declare constant
    SNENTITY_ASSIGN        Assign value of variable
    SNENTITY_GET           Get variable or constant
    SNENTITY_BEGIN_GROUP   Begin group
    SNENTITY_END_GROUP     End group
    SNENTITY_ARRAY         Define array
    SNENTITY_OPERATION     Operation

All of these entity type constants are integer values that are zero or
greater.  `SNENTITY_EOF` constant is always equivalent to zero.

## String type constants

The _string stype constants_ identify the different kinds of strings
that may appear in a Shastina file:

    SNSTRING_QUOTED   Double-quoted strings
    SNSTRING_CURLY    Curly-bracketed strings

## Error codes

The _error codes_ identify specific error conditions.  If you need
these, you must import them with the `:ERROR_CODES` tag:

    SNERR_IOERR       I/O error
    SNERR_EOF         End Of File
    SNERR_BADCR       CR character not followed by LF
    SNERR_OPENSTR     File ends in middle of string
    SNERR_LONGSTR     String is too long
    SNERR_NULLCHR     Null character encountered in string
    SNERR_DEEPCURLY   Too much curly nesting in string
    SNERR_BADCHAR     Illegal character encountered
    SNERR_LONGTOKEN   Token is too long
    SNERR_TRAILER     Content present after |; token
    SNERR_DEEPARRAY   Too much array nesting
    SNERR_METANEST    Nested metacommands
    SNERR_SEMICOLON   Semicolon outside of metacommand
    SNERR_DEEPGROUP   Too much group nesting
    SNERR_RPAREN      Right parenthesis outside of group
    SNERR_RSQR        Right square bracket outside array
    SNERR_OPENGROUP   Open group
    SNERR_LONGARRAY   Array has too many elements
    SNERR_UNPAIRED    Unpaired surrogates encountered
    SNERR_OPENMETA    Unclosed metacommand
    SNERR_OPENARRAY   Unclosed array
    SNERR_COMMA       Comma used outside of array or meta
    SNERR_UTF8        Invalid UTF-8 in input

All of these error codes are integer values that are less than zero.

# FUNCTIONS

- **snerror\_str(code)**

    Given one of the `SNERR_` error codes, return an error message string
    corresponding to it.

    The string has the first letter capitalized, but no punctuation or line
    break at the end.

    If the given code is not a scalar or it does not match any of the
    recognized `SNERR_` codes, then the message `Unknown error` is
    returned.

# AUTHOR

Noah Johnson <noah.johnson@loupmail.com>

# COPYRIGHT

Copyright 2022 Multimedia Data Technology, Inc.

This program is free software.  You can redistribute it and/or modify it
under the same terms as Perl itself.

This program is also dual-licensed under the MIT license:

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
