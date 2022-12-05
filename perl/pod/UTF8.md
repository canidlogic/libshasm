# NAME

Shastina::UTF8 - Shastina's built-in UTF-8 codec.

# SYNOPSIS

    use Shastina::UTF8 qw(
      sn_utf8_isCodepoint
      sn_utf8_isSurrogate
      sn_utf8_highSurrogate
      sn_utf8_lowSurrogate
      sn_utf8_unpair
      sn_utf8_encode
      sn_utf8_trail
      sn_utf8_decode
      sn_utf8_bytelen
    );
    
    # Check if an integer is in Unicode range, including surrogates
    if (sn_utf8_isCodepoint(0x10ffff)) {
      ...
    }
    
    # Check if an integer is in surrogate range
    if (sn_utf8_isSurrogate(0xd800)) {
      ...
    } 
    
    # Check if an integer is a high or a low surrogate
    if (sn_utf8_highSurrogate(0xd901)) {
      ...
    } elsif (sn_utf8_lowSurrogate(0xdcff)) {
      ...
    }
    
    # Convert a surrogate pair into a supplemental codepoint
    my $codep = sn_utf8_unpair(0xd800, 0xd901);
    
    # Define an array of four values for use as a codec buffer
    my @buffer = (0, 0, 0, 0);
    
    # Encode a numeric Unicode codepoint into a codec buffer
    my $trail = sn_utf8_encode(0x2019, \@buffer);
    
    # Check how many additional bytes follow a lead byte
    my $trail = sn_utf8_trail(0x4e);
    if ($trail == 0) {
      ...
    } elsif ($trail == 1) {
      ...
    } elsif ($trail == 2) {
      ...
    } elsif ($trail == 3) {
      ...
    } elsif ($trail < 0) {
      # Invalid lead byte
      ...
    }
    
    # Decode bytes from a codec buffer into a codepoint
    my $codep = sn_utf8_decode(\@buffer, $trail);
    if ($codep < 0) {
      # Invalid UTF-8 encoding or overlong encoding
      ...
    }
    
    # Figure out how many encoded UTF-8 bytes in a codepoint
    my $byte_count = sn_utf8_bytelen(0x2019);

# DESCRIPTION

This module provides functions for encoding and decoding UTF-8, which
are used internally by Shastina.

Perl has UTF-8 decoding and encoding built in, of course.  Shastina uses
its own custom codec to make sure that the details of UTF-8 decoding are
exactly the same as the C implementation of Shastina, particularly
regarding the issue of surrogate pairs encoded into UTF-8 (which is not
supposed to be done, but is supported by Shastina and often encountered
in practice).

# FUNCTIONS

- **sn\_utf8\_isCodepoint(code)**

    Given an integer value, check whether it is in Unicode codepoint range.
    This range includes surrogates and also includes supplemental
    codepoints.  Return 1 if in Unicode codepoint range and 0 otherwise.

- **sn\_utf8\_isSurrogate(code)**

    Given an integer value, check whether it is a surrogate codepoint.
    Return 1 if surrogate and 0 otherwise.

- **sn\_utf8\_highSurrogate(code)**

    Given an integer value, check whether it is a high surrogate codepoint.
    Return 1 if high surrogate and 0 otherwise.

- **sn\_utf8\_lowSurrogate(code)**

    Given an integer value, check whether it is a high surrogate codepoint.
    Return 1 if high surrogate and 0 otherwise.

- **sn\_utf8\_unpair(c1, c2)**

    Given two integer codepoints corresponding to a surrogate pair, return
    the supplemental codepoint they return to.

    `c1` must satisfy `sn_utf8_highSurrogate` and `c2` must satisfy
    `sn_utf8_lowSurrogate` or a fatal error occurs.

- **sn\_utf8\_encode(code, rbuf)**

    Encode a codepoint into UTF-8 bytes.

    `code` is the codepoint to encode.  It must satisfy
    `sn_utf8_isCodepoint` but must _not_ satisfy `sn_utf8_isSurrogate`.

    `rbuf` is a reference to an encoding buffer array.  This must be an
    array reference, and the referenced array must have length at least
    four.  This allows the same array to be used repeatedly, to make
    repeated encoding operations more efficient.

    This function returns the number of trailing bytes in the encoding.
    This is an integer that is in range \[0, 3\].  A value of zero means that
    one byte value was written into `rbuf`, a value of one means that two
    byte values were written into `rbuf`, and so forth.

- **sn\_utf8\_trail(lead)**

    Given a lead byte value of a UTF-8 encoding, determine how many trailer
    bytes follow it.

    `lead` is the lead byte value.  It must be an integer in range
    \[0, 255\].

    The return value is the number of trailing bytes that follows the given
    lead byte.  The range is \[0, 3\].  If the given byte value is not a valid
    leading byte, then -1 is returned.

- **sn\_utf8\_decode(rbuf, trail)**

    Decode UTF-8 bytes into a Unicode codepoint.

    `rbuf` is a reference to a decoding buffer array.  This must be an
    array reference, the referenced array must have length at least four,
    and the first four elements must be integers in range \[0, 255\].  The
    decoding buffer will not be modified by this function.

    `trail` is the number of trailing bytes that follow the leading byte in
    the decoding buffer array.  It must be in range \[0, 3\], and equal to
    `sn_utf8_trail()` when applied to the first byte in the decoding buffer
    array or decoding will fail.

    This function returns the decoded Unicode codepoint, or -1 if decoding
    failed.  Surrogate codepoints will successfully decode with this
    function, even though they are not technically supposed to be present in
    UTF-8.  However, overlong encodings will cause decoding to fail.

- **sn\_utf8\_bytelen(code)**

    Given a numeric Unicode codepoint value, determine how many bytes are in
    its UTF-8 encoding.

    The codepoint value must satisfy `sn_utf8_isCodepoint()`.  The return
    value will be in range \[1, 4\].

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
