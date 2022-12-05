# NAME

Shastina::FileSource - Shastina input source based on a disk file.

# SYNOPSIS

    use Shastina::FileSource;
    
    # Wrap a Shastina source around a file
    my $src = Shastina::FileSource->load("myfile.txt");
    unless (defined $src) {
      # Failed to open file
      ...
    }
    
    # We can explicitly close the underlying file
    $src->closeFile;
    
    # See Shastina::Source for further information

# DESCRIPTION

This module defines a Shastina input source based on a disk file.

See the documentation for the parent class `Shastina::Source` for more
information about how input sources work.  Note however, that this
subclass has the special method `closeFile()`.  See the documentation
of that method for further information.

This subclass supports multipass operation.

# CONSTRUCTOR

- **load(path\[, bufsize\])**

    Construct a new input source around the file identified by the given
    path.

    The file will be kept open until either the object destructor is run or
    the underlying file is closed explicitly with `closeFile()` (whichever
    occurs first).

    If the optional `bufsize` parameter is provided, it is an integer value
    greater than zero that identifies the size of the buffer to allocate for
    reading through the file.  If not given, a sensible default buffer size
    will be used.

    If this constructor is unable to open the file, it returns `undef`.

# DESTRUCTOR

The destructor routine closes the file handle if it is not already
closed.

# SPECIAL METHODS

- **closeFile()**

    Explicitly close the underlying file handle, if it is not already
    closed.

    If the file handle is not explicitly closed, it will be implicity closed
    by the object destructor.

    Once a source object is closed, any further attempt to use it will cause
    a fatal error, except that calling `closeFile()` again is harmless.

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

- **multipass()**

    This subclass _does_ support multipass, so this function always returns
    1.

- **rewind()**

    This subclass _does_ support multipass, so this function is available.
    Rewinding will clear any I/O error (unless the seek operation fails).

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
