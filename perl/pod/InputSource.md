# NAME

Shastina::InputSource - Shastina input source that reads from standard
input.

# SYNOPSIS

    use Shastina::InputSource;
    
    # Wrap a Shastina source around standard input
    my $src = Shastina::InputSource->load;
    unless (defined $src) {
      # Failed to open input source
      ...
    }
    
    # See Shastina::Source for further information

# DESCRIPTION

This module defines a Shastina input source based on standard input.

In order for this to work correctly, you must open this input source
before anything touches standard input in any way.  This input source
needs to set standard input into raw mode during the load procedure.
Nothing except this input source should interact with standard input
until parsing is complete.

Standard input can only be read through once, so this input source does
_not_ support multipass operation.  Also, you can't create more than
one instance of this class.

See the documentation for the parent class `Shastina::Source` for more
information about how input sources work.

# CONSTRUCTOR

- **load(\[bufsize\])**

    Construct a new input source around standard input.

    If the optional `bufsize` parameter is provided, it is an integer value
    greater than zero that identifies the size of the buffer to allocate for
    reading through input.  If not given, a sensible default buffer size 
    will be used.

    If this constructor is unable to set the proper input mode on standard
    input or if another instance of this class has already been constructed,
    the constructor fails and returns `undef`.

# INHERITED METHODS

See the documentation in the base class `Shastina::Source` for
specifications of each of these functions.

- **readByte()**
- **hasError()**
- **count()**

    In the unlikely event that the byte counter has overflown, undef will be
    returned.  This only happens if there are many terabytes of data.

- **consume()**

    This subclass provides a special implementation that is much faster than
    just calling `readByte()` repeatedly.

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
