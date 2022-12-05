# NAME

Shastina::Token - Shastina token reader.

# SYNOPSIS

    use Shastina::Token;
    
    # Build a token reader around a Shastina::Filter
    my $tok = Shastina::Token->wrap($filter);
    
    # Read through all the tokens
    my $tk;
    for($tk = $tok->readToken; ref($tk); $tk = $tok->readToken) {
      # Check token type
      if (scalar(@$tk) == 1) {
        # Simple token, except |;
        my $token = $tk->[0];
        ...
      } elsif (scalar(@$tk) == 3) {
        # String token
        my $prefix  = $tk->[0];
        my $qtype   = $tk->[1];
        my $payload = $tk->[2];
        ...
      }
    }
    
    if ($tk == 0) {
      # Finished with a |; token
      ...
    } elsif ($tk < 0) {
      # Finished with an error code
      ...
    }

# DESCRIPTION

This module steps through all the tokens in a Shastina file.  It is used
internally by the Shastina parser.

Construct instances of this class by passing a `Shastina::Filter`
instance that allows a filtered codepoint sequence to be read.

There is a single public instance function, `readToken`, which allows
iteration through all the tokens.

# CONSTRUCTOR

- **wrap(filter)**

    Construct a new tokenizer instance by wrapping an existing filter
    object.

    The given `filter` must be an instance of `Shastina::Filter`.  In
    order for parsing to work properly, the given filter should only be used
    by this tokenizer and nothing else.

# INSTANCE METHODS

- **readToken()**

    Read the next token from the Shastina file.

    If this returns a scalar, it is either an integer zero indicating the
    final `|;` token was read, or it is a negative integer selecting one of
    the error codes from the `Shastina::Const` module.  Once a scalar has
    been returned, that same scalar will be returned on all subsequent
    calls.

    If this returns a reference, then the reference will be to an array of
    length one or three.  If length one, then this is a simple token and the
    single element is a Unicode string storing the token.  If length three,
    then this is a string token.  The first element is the string prefix
    excluding the opening quote or curly; this prefix might be empty.  The
    second element is one of the `SNSTRING` constants from
    `Shastina::Const` indicating the type of quoting for the string.  The
    third element is the string data, excluding the opening and closing
    quotes or curlies.

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
