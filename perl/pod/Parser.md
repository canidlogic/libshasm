# NAME

Shastina::Parser - Shastina parser interface.

# SYNOPSIS

    use Shastina::Const qw(:CONSTANTS);
    use Shastina::Parser;
    
    # Wrap a file as an input source
    use Shastina::FileSource;
    my $src = Shastina::FileSource->load("myfile.txt");
    unless (defined $src) {
      # Failed to open file
      ...
    }
    
    # Or, wrap standard input as an input source
    use Shastina::InputSource;
    my $src = Shastina::InputSource->load;
    unless (defined $src) {
      # Failed to open input source
      ...
    }
    
    # Or, wrap a binary string as an input source
    use Shastina::BinarySource;
    my $bytes = "...";
    my $src = Shastina::BinarySource->load(\$bytes);
    
    # Or, wrap a Unicode string as an input source
    use Shastina::UnicodeSource;
    my $codepoints = "...";
    my $src = Shastina::UnicodeSource->load(\$codepoints);
    
    # Build a Shastina parser around an input source
    my $sr = Shastina::Parser->parse($src);
    
    # Read through all the entities
    my $ent;
    for($ent = $sr->readEntity; ref($ent); $ent = $sr->readEntity) {
      # First array element is the entity type
      my $etype = $ent->[0];
      
      # We can get the line count in the source file
      my $lcount = $sr->count;
      if (defined $lcount) {
        ...
      }
      
      # Handle the different entity types
      if ($etype == SNENTITY_STRING) {
        my $prefix = $ent->[1];
        my $stype  = $ent->[2];
        my $sdata  = $ent->[3];
        
        if ($stype == SNSTRING_QUOTED) {
          ...
        } elsif ($stype == SNSTRING_CURLY) {
          ...
        }
        
        ...
      
      } elsif ($etype == SNENTITY_BEGIN_META) {
        ...
      
      } elsif ($etype == SNENTITY_END_META) {
        ...
      
      } elsif ($etype == SNENTITY_META_TOKEN) {
        my $token = $ent->[1];
        ...
      
      } elsif ($etype == SNENTITY_META_STRING) {
        my $prefix = $ent->[1];
        my $stype  = $ent->[2];
        my $sdata  = $ent->[3];
        
        if ($stype == SNSTRING_QUOTED) {
          ...
        } elsif ($stype == SNSTRING_CURLY) {
          ...
        }
        
        ...
      
      } elsif ($etype == SNENTITY_NUMERIC) {
        my $string_representation = $ent->[1];
        ...
      
      } elsif ($etype == SNENTITY_VARIABLE) {
        my $var_name = $ent->[1];
        ...
      
      } elsif ($etype == SNENTITY_CONSTANT) {
        my $const_name = $ent->[1];
        ...
      
      } elsif ($etype == SNENTITY_ASSIGN) {
        my $var_name = $ent->[1];
        ...
      
      } elsif ($etype == SNENTITY_GET) {
        my $var_or_const_name = $ent->[1];
        ...
      
      } elsif ($etype == SNENTITY_BEGIN_GROUP) {
        ...
      
      } elsif ($etype == SNENTITY_END_GROUP) {
        ...
      
      } elsif ($etype == SNENTITY_ARRAY) {
        my $element_count_integer = $ent->[1];
        ...
      
      } elsif ($etype == SNENTITY_OPERATION) {
        my $op_name = $ent->[1];
        ...
      }
    }
    
    if ($ent == SNENTITY_EOF) {
      # Finished with a |; token
      ...
    } elsif ($ent < 0) {
      # Finished with an error code
      my $lcount = $sr->count;
      if (defined $lcount) {
        # Report with line number
        ...
      } else {
        # Report without line number
        ...
      }
    }
    
    # If we have a multipass source, we can read it again if we want
    if ($src->multipass) {
      # Rewind the input source
      $src->rewind;
      
      # We need a new parser for the new run
      $sr = Shastina::Parser->parse($src);
      
      ...
    }

# DESCRIPTION

This module allows you to parse through all the entities in a Shastina
file.  This is the main public interface for the Shastina parsing
library.

You must wrap the input bytestream in a subclass of `Shastina::Source`.
The following subclasses are built in:

    Shastina::FileSource    (reads from a file path)
    Shastina::InputSource   (reads from standard input)
    Shastina::BinarySource  (reads from a byte string)
    Shastina::UnicodeSource (reads from a Unicode string)

The synopsis gives examples of constructing each of these built-in
source types.  See the documentation of those subclass modules as well
as the abstract superclass `Shastina::Source` for further information.
You may also define your own source class by subclassing the abstract
superclass `Shastina::Source`.

Once you have an input source object, you can construct a Shastina
parser object around it with the `parse()` constructor.  If you are
parsing the Shastina file in multiple passes, you will call `rewind` on
the source before each pass and then each pass will need to use a newly
constructed `Shastina::Parser` parser object.

The `readEntity()` instance function allows you to read through the
entities in the Shastina file one by one.  See the synopsis for an
example, and see the documentation of the `readEntity()` function for
further information.

The `count()` instance function allows you at any time to return the
current line number in the underlying source file.  This is especially
useful for error reporting.

# CONSTRUCTOR

- **parse(src)**

    Construct a new Shastina parser instance by around a given input source
    object.

    `src` must be an instance of a subclass of `Shastina::Source`.  The
    `Shastina::Source` class is an abstract superclass that can't be
    directly used.  Either use one of its built-in subclasses (as explained
    in the module description above), or subclass it yourself.

    The parser instance will start reading the input source at its current
    location without attempting to rewind it.  The parser will not attempt
    to close file sources at any time.

    If you want to make multiple passes through a Shastina input file, make
    sure you are using an input source that supports multipass operation.
    Then, before each pass, call the `rewind()` function on the input
    source object and construct a new Shastina parser instance by calling
    this `parse()` constructor.  Each pass will use the same input source
    object instance, but each pass requires a new parser object instance.

    While a parser object is in use, nothing else should read from or modify
    the input source object in any way.  Internal buffering is used, and
    undefined behavior occurs if this buffering is disrupted by something
    else reading from the input source object.  Once you are finished with
    the parser object, the input source object can once again be used
    however you wish.

# INSTANCE METHODS

- **readEntity()**

    Parse the next Shastina entity.

    The return value is either a reference to an array or a scalar integer
    value.

    To get the entity constants you must include `Shastina::Const` with the
    `:CONSTANTS` mode.  To handle specific error codes, you must also
    include the `:ERROR_CODES` mode.  To get error messages from the error
    codes use the `snerror_str()` function exported by `Shastina::Const`.

    If the `|;` final entity was read, a scalar integer `SNENTITY_EOF`
    will be returned on this call and all subsequent calls to this function.

    If an error was encountered, one of the error code constant values
    defined in `Shastina::Const` will be returned.  All such codes will be
    integers less than zero.  Once an error is returned, the same error will
    be returned on all subsequent calls.

    If an entity besides the `|;` final entity was read, then this function
    will return an array reference.  The referenced array always has at
    least one element, and the first element is always an integer that
    corresponds to one of the `SNENTITY` constants (except `SNENTITY_EOF`
    which is handled specially).

    The number of elements in the entity array and their meaning depends on
    the specific entity.  All of the regular entities along with their
    format name are given here:

             Entity type      |  Format
        ----------------------+-----------
         SNENTITY_STRING      | String
         SNENTITY_BEGIN_META  | Primitive
         SNENTITY_END_META    | Primitive
         SNENTITY_META_TOKEN  | Token
         SNENTITY_META_STRING | String
         SNENTITY_NUMERIC     | Token
         SNENTITY_VARIABLE    | Token
         SNENTITY_CONSTANT    | Token
         SNENTITY_ASSIGN      | Token
         SNENTITY_GET         | Token
         SNENTITY_BEGIN_GROUP | Primitive
         SNENTITY_END_GROUP   | Primitive
         SNENTITY_ARRAY       | Integer
         SNENTITY_OPERATION   | Token

    For the _primitive_ format, the entity array only has a single element,
    which is the `SNENTITY` constant.

    For the `token` format, the entity array has two elements.  The first
    is the `SNENTITY` constant and the second is the token as a string.
    For `SNENTITY_VARIABLE`, `SNENTITY_CONSTANT`, `SNENTITY_ASSIGN`, and
    `SNENTITY_GET`, the token string does _not_ include the first
    character of the Shastina token.  For all other token formats, the token
    is exactly same as the whole Shastina token in the source file.

    Note that the `SNENTITY_NUMERIC` type is in the token format and
    therefore provides its data as a string.  This allows clients to
    determine their own number formats and how they will parse them.

    For the `integer` format, the entity array has two elements.  The first
    is the `SNENTITY` constant and the second is the number of elements in
    the array as an integer that is zero or greater.

    For the `string` format, the entity array has four elements.  The first
    is the `SNENTITY` constant.  The second is the string prefix, _not_
    including the opening quote or left curly.  The third is one of the
    `SNSTRING` constants from `Shastina::Const` that determines whether
    this is a quoted or curlied string.  The fourth is the string data, not
    including the enclosing quotes or curly braces.  The string data is in
    Unicode string format.

- **count()**

    Return the current line count.  This is an integer greater than zero, or
    `undef` in the unlikely event that the line count overflows.  (This
    only happens if there are trillions of lines.)

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
