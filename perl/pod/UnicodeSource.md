# NAME

Shastina::UnicodeSource - Shastina input source based on a Unicode
string.

# SYNOPSIS

    use Shastina::UnicodeSource;
    
    # Suppose we have a file in a Unicode string
    my $codepoints = "...";
    
    # Wrap a Shastina source around a reference to that string
    my $src = Shastina::UnicodeSource->load(\$codepoints);
    
    # See Shastina::Source for further information

# DESCRIPTION

This module defines a Shastina input source based on an in-memory
Unicode file stored in a string.  This must be a _Unicode_ string,
where each character is an individual Unicode codepoint in range
\[0, 0x10ffff\].  If you have a binary string, you need to use
`Shastina::BinarySource` instead.  (If you have an ASCII string, you
can use either source type, but `Shastina::BinarySource` may be
slightly faster.)

Shastina requires a binary input stream, so this subclass will encode
each codepoint into UTF-8 as it goes along and return the unsigned byte
values that Shastina is expecting.  Hence, it is preferable to use
`Shastina::BinarySource` as an in-memory source, if you can.

See the documentation for the parent class `Shastina::Source` for more
information about how input sources work.

This subclass supports multipass operation.

# CONSTRUCTOR

- **load(string\_ref)**

    Construct a new input source around the given string reference.  Note
    that you must pass a _reference_ to a string, rather than directly
    passing a string.  If the referenced string changes while this source is
    still in use, the changes will be immediately reflected in the input
    source.  In other words, be careful not to modify the referenced string
    until you are done parsing it.

    The referenced string must be a Unicode string, with characters in
    codepoint range \[0, 0x10ffff\].  If any other kind of characters are
    encountered, an I/O error will be returned.  If you want to read through
    a binary string, you should be using the `Shastina::BinarySource` class
    instead.

    Surrogate codepoints are allowed.  Shastina's input layer will
    automatically reassemble surrogate pairs that were encoded in UTF-8,
    even though this is not technically correct.  This subclass will just
    encode surrogates like any other Unicode codepoint.

# INHERITED METHODS

See the documentation in the base class `Shastina::Source` for
specifications of each of these functions.

- **readByte()**

    Note that this subclass returns unsigned byte values in the UTF-8
    encoding of codepoints from the source string.  It does _not_ return
    codepoints.

- **hasError()**
- **count()**

    This returns the number of encoded UTF-8 bytes that have been read
    through this subclass, _not_ the number of codepoints that have been
    read from the underlying string.

- **consume()**

    This subclass provides a special implementation that is much faster than
    just calling `readByte()` repeatedly.

- **multipass()**

    This subclass _does_ support multipass, so this function always returns
    1.

- **rewind()**

    This subclass _does_ support multipass, so this function is available.
    Rewinding will clear any I/O error.

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
