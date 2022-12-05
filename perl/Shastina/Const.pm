package Shastina::Const;
use v5.14;
use warnings;
use parent qw(Exporter);

=head1 NAME

Shastina::Const - Define constants for the Shastina library.

=head1 SYNOPSIS

  use Shastina::Const qw(:CONSTANTS :ERROR_CODES snerror_str);
  printf "Error message: %s\n", snerror_str(SNERR_EOF);

=head1 DESCRIPTION

This module defines various Shastina constants that can be imported, as
well as a function that maps error codes to error messages.

Use the tag C<:CONSTANTS> to import all constants except the error
codes.

Use the tag C<:ERROR_CODES> to import all the error codes.  This should
only be necessary if you need to react to specific error codes.  In the
more common case where you just pass error codes through to
C<snerror_str> or ignore specific codes, you do not need to import the
error codes.

This module also allows you to import the C<snerror_str> function.  This
function takes an error code and returns an appropriate error message
string.

=head1 CONSTANTS

=head2 Entity type constants

The I<entity type constants> identify the different types of entities
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
greater.  C<SNENTITY_EOF> constant is always equivalent to zero.

=cut

use constant SNENTITY_EOF         =>  0;
use constant SNENTITY_STRING      =>  1;
use constant SNENTITY_BEGIN_META  =>  2;
use constant SNENTITY_END_META    =>  3;
use constant SNENTITY_META_TOKEN  =>  4;
use constant SNENTITY_META_STRING =>  5;
use constant SNENTITY_NUMERIC     =>  6;
use constant SNENTITY_VARIABLE    =>  7;
use constant SNENTITY_CONSTANT    =>  8;
use constant SNENTITY_ASSIGN      =>  9;
use constant SNENTITY_GET         => 10;
use constant SNENTITY_BEGIN_GROUP => 11;
use constant SNENTITY_END_GROUP   => 12;
use constant SNENTITY_ARRAY       => 13;
use constant SNENTITY_OPERATION   => 14;

=head2 String type constants

The I<string stype constants> identify the different kinds of strings
that may appear in a Shastina file:

  SNSTRING_QUOTED   Double-quoted strings
  SNSTRING_CURLY    Curly-bracketed strings

=cut

use constant SNSTRING_QUOTED => 1;
use constant SNSTRING_CURLY  => 2;

=head2 Error codes

The I<error codes> identify specific error conditions.  If you need
these, you must import them with the C<:ERROR_CODES> tag:

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

=cut

use constant SNERR_IOERR     =>  -1;
use constant SNERR_EOF       =>  -2;
use constant SNERR_BADCR     =>  -3;
use constant SNERR_OPENSTR   =>  -4;
use constant SNERR_LONGSTR   =>  -5;
use constant SNERR_NULLCHR   =>  -6;
use constant SNERR_DEEPCURLY =>  -7;
use constant SNERR_BADCHAR   =>  -8;
use constant SNERR_LONGTOKEN =>  -9;
use constant SNERR_TRAILER   => -10;
use constant SNERR_DEEPARRAY => -11;
use constant SNERR_METANEST  => -12;
use constant SNERR_SEMICOLON => -13;
use constant SNERR_DEEPGROUP => -14;
use constant SNERR_RPAREN    => -15;
use constant SNERR_RSQR      => -16;
use constant SNERR_OPENGROUP => -17;
use constant SNERR_LONGARRAY => -18;
use constant SNERR_UNPAIRED  => -19;
use constant SNERR_OPENMETA  => -20;
use constant SNERR_OPENARRAY => -21;
use constant SNERR_COMMA     => -22;
use constant SNERR_UTF8      => -23;

=head1 FUNCTIONS

=over 4

=item B<snerror_str(code)>

Given one of the C<SNERR_> error codes, return an error message string
corresponding to it.

The string has the first letter capitalized, but no punctuation or line
break at the end.

If the given code is not a scalar or it does not match any of the
recognized C<SNERR_> codes, then the message C<Unknown error> is
returned.

=cut

# Map error codes to their error messages.
#
# Constants are actually functions, so we need to invoke those functions
# to get the key values.
#
my %ERROR_MESSAGES = (
  SNERR_IOERR()     => "I/O error",
  SNERR_EOF()       => "Unexpected end of file",
  SNERR_BADCR()     => "CR must always be followed by LF",
  SNERR_OPENSTR()   => "File ends in middle of string",
  SNERR_LONGSTR()   => "String is too long",
  SNERR_NULLCHR()   => "Nul character encountered in string",
  SNERR_DEEPCURLY() => "Too much curly nesting in string",
  SNERR_BADCHAR()   => "Illegal character encountered",
  SNERR_LONGTOKEN() => "Token is too long",
  SNERR_TRAILER()   => "Content present after |; token",
  SNERR_DEEPARRAY() => "Too much array nesting",
  SNERR_METANEST()  => "Nested metacommands",
  SNERR_SEMICOLON() => "Semicolon used outside of metacommand",
  SNERR_DEEPGROUP() => "Too much group nesting",
  SNERR_RPAREN()    => "Right parenthesis outside of group",
  SNERR_RSQR()      => "Right square bracket outside array",
  SNERR_OPENGROUP() => "Open group",
  SNERR_LONGARRAY() => "Array has too many elements",
  SNERR_UNPAIRED()  => "Unpaired surrogates encountered in input",
  SNERR_OPENMETA()  => "Unclosed metacommand",
  SNERR_OPENARRAY() => "Unclosed array",
  SNERR_COMMA()     => "Comma used outside of array or meta",
  SNERR_UTF8()      => "Invalid UTF-8 encountered in input"
);

# The actual function
#
sub snerror_str {
  # Get parameter
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  my $code = shift;
  
  # If not a scalar, replace it with zero
  if (ref($code)) {
    $code = 0;
  }
  
  # Convert to integer
  $code = int($code);
  
  # Get the error message or a default
  my $msg;
  if (defined $ERROR_MESSAGES{$code}) {
    $msg = $ERROR_MESSAGES{$code};
  } else {
    $msg = "Unknown error";
  }
  
  # Return the error message
  return $msg;
}

=back

=cut

# ==============
# Module exports
# ==============

our @EXPORT_OK = qw(
  snerror_str
  SNENTITY_EOF
  SNENTITY_STRING
  SNENTITY_BEGIN_META
  SNENTITY_END_META
  SNENTITY_META_TOKEN
  SNENTITY_META_STRING
  SNENTITY_NUMERIC
  SNENTITY_VARIABLE
  SNENTITY_CONSTANT
  SNENTITY_ASSIGN
  SNENTITY_GET
  SNENTITY_BEGIN_GROUP
  SNENTITY_END_GROUP
  SNENTITY_ARRAY
  SNENTITY_OPERATION
  SNSTRING_QUOTED
  SNSTRING_CURLY
  SNERR_IOERR
  SNERR_EOF
  SNERR_BADCR
  SNERR_OPENSTR
  SNERR_LONGSTR
  SNERR_NULLCHR
  SNERR_DEEPCURLY
  SNERR_BADCHAR
  SNERR_LONGTOKEN
  SNERR_TRAILER
  SNERR_DEEPARRAY
  SNERR_METANEST
  SNERR_SEMICOLON
  SNERR_DEEPGROUP
  SNERR_RPAREN
  SNERR_RSQR
  SNERR_OPENGROUP
  SNERR_LONGARRAY
  SNERR_UNPAIRED
  SNERR_OPENMETA
  SNERR_OPENARRAY
  SNERR_COMMA
  SNERR_UTF8
);
our %EXPORT_TAGS = (
  CONSTANTS => [qw(
    SNENTITY_EOF
    SNENTITY_STRING
    SNENTITY_BEGIN_META
    SNENTITY_END_META
    SNENTITY_META_TOKEN
    SNENTITY_META_STRING
    SNENTITY_NUMERIC
    SNENTITY_VARIABLE
    SNENTITY_CONSTANT
    SNENTITY_ASSIGN
    SNENTITY_GET
    SNENTITY_BEGIN_GROUP
    SNENTITY_END_GROUP
    SNENTITY_ARRAY
    SNENTITY_OPERATION
    SNSTRING_QUOTED
    SNSTRING_CURLY
  )],
  ERROR_CODES => [qw(
    SNERR_IOERR
    SNERR_EOF
    SNERR_BADCR
    SNERR_OPENSTR
    SNERR_LONGSTR
    SNERR_NULLCHR
    SNERR_DEEPCURLY
    SNERR_BADCHAR
    SNERR_LONGTOKEN
    SNERR_TRAILER
    SNERR_DEEPARRAY
    SNERR_METANEST
    SNERR_SEMICOLON
    SNERR_DEEPGROUP
    SNERR_RPAREN
    SNERR_RSQR
    SNERR_OPENGROUP
    SNERR_LONGARRAY
    SNERR_UNPAIRED
    SNERR_OPENMETA
    SNERR_OPENARRAY
    SNERR_COMMA
    SNERR_UTF8
  )]
);

=head1 AUTHOR

Noah Johnson E<lt>noah.johnson@loupmail.comE<gt>

=head1 COPYRIGHT

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

=cut

# End with something that evaluates to true
1;
