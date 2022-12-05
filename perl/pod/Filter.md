# NAME

Shastina::Filter - Shastina codepoint input filter.

# SYNOPSIS

    use Shastina::Filter;
    
    # Build a filter around a Shastina::Source
    my $filter = Shastina::Filter->wrap($src);
    
    # Read codepoint, with BOM, CR+LF, and surrogate filtering applied
    my $cpv = $filter->readCode;
    if ($cpv < 0) {
      # Error code
      ...
    }
    
    # Get the current line number
    my $line = $filter->count;
    if (defined $line) {
      # Line count hasn't overflown
      ...
    }
    
    # Push the most recently read codepoint back onto input stream
    $filter->pushback;

# DESCRIPTION

This module performs low-level input filtering and assembles bytes into
codepoints.  It is used internally by the Shastina parser.

Construct instances of this class by passing a `Shastina::Source`
instance that allows a byte sequence to be read to the `wrap()`
constructor.  Input bytes will be read from the source and then passed
through the filter defined by this class.

While `Shastina::Source` objects output bytes, this class outputs
Unicode codepoints, which may include supplemental codepoints but never
include surrogates.  The basic operation is to decode UTF-8 bytes into
codepoints, but a three low-level filters are also applied.

The first low-level filter is the BOM filter.  UTF-8 text files may
optionally begin with a Byte Order Mark that decodes to a codepoint
value of U+FEFF.  If U+FEFF is the very first codepoint that is decoded,
it will be silently skipped.

The second low-level filter is the CR+LF filter.  ASCII control code
Carriage Return (CR; U+000D) is only allowed to occur immediately before
ASCII control code Line Feed (LF; U+000A) as part of a CR+LF line break
pair.  All the CR controls are silently filtered out, so that line
breaks are always just single LF controls.

The third low-level filter is the surrogate filter.  Officially, UTF-8
is never supposed to encode surrogate codepoints, since supplemental
codepoints can directly be encoded.  However, often surrogates will be
encoded in UTF-8 anyways.  The surrogate filter will identify surrogate
pairs that have been encoded in the UTF-8 stream and silently replace
them with the supplemental codepoint they correspond to.  In this way,
the filtered output will never include surrogates.

Instances of this class module also support _pushback_.  Pushback
allows clients to push a codepoint they read back into the filter so
that the filter behaves as the codepoint hasn't been read yet.  This is
implemented with an internal buffer, so the underlying source instances
do not need to support pushback.

# CONSTRUCTOR

- **wrap(src)**

    Construct a new filter instance by wrapping an existing input source.

    The given `src` must be an instance of `Shastina::Source`.  In order
    for filtering to work properly, the given source should be at the
    beginning of input and it should not be used by any other client while
    it is being read by this filter.

# INSTANCE METHODS

- **readCode()**

    Read the next filtered codepoint.

    The return value is either a Unicode codepoint value zero or greater,
    but excluding surrogates; or, it is a Shastina error code that is less
    than zero, defined by `Shastina::Const`.

    The low-level filters described earlier in the module documentation will
    already have been applied to the codepoints read from this function.  If
    pushback mode is on, this function will return the most recent codepoint
    and clear pushback mode.

- **count()**

    Return the current line count.

    The first line of the file is line one.  `undef` is returned in the
    unlikely event that the line count overflows.  (This only happens if
    there are trillions of lines.)

    The line count is affected by pushback mode, changing backwards if
    characters are pushed back before a line break.

- **pushback()**

    Set pushback mode, so the next call to `readCode` will once again
    return the codepoint that was just returned.

    This call is ignored if the filter is currently in EOF or some other
    error condition.

    Fatal errors occur if the filter is already in pushback mode or if no
    characters have been read yet.

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
