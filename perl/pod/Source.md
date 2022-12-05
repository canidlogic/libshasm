# NAME

Shastina::Source - Base class for Shastina input sources.

# SYNOPSIS

    # Base class is abstract; construct with a subclass
    my $src = ...
    
    # Read an unsigned byte value
    my $byte = $src->readByte;
    if ($byte >= 0) {
      # Read the byte successfully
      ...
    
    } else {
      # Read an error code
      ...
    }
    
    # Check whether an I/O error has been encountered
    if ($src->hasError) {
      ...
    }
    
    # Count the number of bytes that have been read
    my $count = $src->count;
    
    # Consume rest of input, make sure nothing but blank
    ($src->consume) or die "Additional data after |;\n";
    
    # If multipass is suported, input source can be rewound
    if ($src->multipass) {
      $src->rewind;
    }

# DESCRIPTION

This module defines the abstract base class for all Shastina input
sources.

Input sources can read through all the raw, unsigned byte values in an
underlying data source.  They also keep track of how many bytes have
been read, which can determine the location in an input file at a
certain point during parsing.

For applications that do not want to allow anything after the final
`|;` token except for blank lines, the `consume` function can consume
the rest of input in a source and verify it is all blank.

Certain input sources can only be read through once (like standard
input) while other input sources can be read through many times (like
disk files).  The `multipass` function determines whether an input
source supports multiple passes through the input source.  If it does,
then the `rewind` function can return the input source back to the
beginning.

This abstract base class defines methods that cause an error if they are
called.  Use a specific subclass that overrides these methods and
implements an actual input source.

# INSTANCE METHODS

- **readByte()**

    Read the next byte from the data source.

    The return value is an unsigned byte value in range \[0, 255\], or else
    the `SNERR_EOF` or `SNERR_IOERR` error codes defined by
    `Shastina::Const` for End Of File or I/O error conditions,
    respectively.

    Once this function returns one of the two error codes, it should
    continue to return that same error code unless it supports multipass and
    is rewound.

    Do not use this function in the middle of parsing a Shastina file with
    this source or the parsing will be disturbed in an undefined way.

- **hasError()**

    Check whether the data source has encountered an I/O error.

    This is true if `readByte()` has returned `SNERR_IOERR`, and otherwise
    false.  For multipass sources, rewinding the source might clear this
    error condition.

- **count()**

    Check how many bytes have been read so far from the data source.

    If the underlying data source buffers reads, this function should only
    return the number of bytes that have been returned through `readByte`,
    and not the number of bytes that have been buffered.

    This count does _not_ include any `SNERR_EOF` or `SNERR_IOERR` codes
    that were returned through `readByte`.  It only includes actual bytes
    that were read through the source.

    After successfully reading the `|;` entity, this count will be total
    number of bytes from the start of the file up to and including the
    semicolon in the `|;` entity.

    For multipass sources, rewinding will reset this counter to zero.

- **consume()**

    Consume the rest of the data in the source and make sure that there is
    nothing but whitespace and blank lines remaining.

    This function will keep reading from the source until one of the
    following happens:

        (1) Something other than SP HT CR LF is read
        (2) SNERR_EOF is encountered
        (3) SNERR_IOERR is encountered

    The function succeeds and returns a value of one in case (2), which
    indicates that only whitespace and blank lines remained in the rest of
    the file.

    The function fails and returns a value of zero in cases (1) and (3).  If
    you need to distinguish between these two cases, call `hasError` to
    check whether case (3) applies.

    Do not use this function in the middle of parsing a Shastina file with
    this source or the parsing will be disturbed in an undefined way.

    This function is intended for cases where nothing should follow the
    `|;` EOF marker in the Shastina file.  Parsing stops at the semicolon
    in the |; EOF marker without reading any further than that.  This
    function can then make sure that nothing besides whitespace and blank
    lines remains after the `|;` EOF marker.

    Note that after you use this function, the `count()` function will
    include the number of additional bytes consumed by this function.  You
    can therefore no longer use `count()` to count the number of bytes up
    to the `|;` EOF marker after calling this function.

    You may call this on a source that has already reached EOF, in which
    case this function will just return greater than zero.

- **multipass()**

    Check whether this input source suports multipass operation.  If
    multipass is supported, this function will return one and the `rewind`
    function will be available.  If multipass is not supported, this
    function will return zero and the `rewind` function will not work.

    The implementation in the base class returns zero from this function and
    causes an error indicating that multipass is not supported if the
    `rewind` function is called.  Subclasses that do not support multipass
    can therefore just use the inherited implementations of these two
    functions.

- **rewind()**

    Rewind the input source back to the beginning.

    This is only supported if the `multipass()` function is true.
    Otherwise, calling this function will result in an error.

    Rewinding will always clear the EOF condition, and it _might_ clear the
    I/O error condition.

    The implementation in the base class always causes an error saying that
    multipass is not supported.  Subclasses that do not support multipass
    can therefore just use the inherited implementations of `multipass()`
    and `rewind` functions.

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
