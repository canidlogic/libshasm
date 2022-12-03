package Shastina::Token;
use v5.14;
use warnings;

# Core includes
use Scalar::Util qw(looks_like_number);

# Shastina modules
use Shastina::Const qw(:CONSTANTS :ERROR_CODES);
use Shastina::Filter;

=head1 NAME

Shastina::Token - Shastina token reader.

=head1 SYNOPSIS

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

=head1 DESCRIPTION

This module steps through all the tokens in a Shastina file.  It is used
internally by the Shastina parser.

Construct instances of this class by passing a C<Shastina::Filter>
instance that allows a filtered codepoint sequence to be read.

There is a single public instance function, C<readToken>, which allows
iteration through all the tokens.

=cut

# =========
# Constants
# =========

# Constants for special ASCII codepoint values.
#
use constant CPV_HT        => 0x09;
use constant CPV_LF        => 0x0a;
use constant CPV_SP        => 0x20;
use constant CPV_DQUOTE    => 0x22;
use constant CPV_POUNDSIGN => 0x23;
use constant CPV_PERCENT   => 0x25;
use constant CPV_LPAREN    => 0x28;
use constant CPV_RPAREN    => 0x29;
use constant CPV_COMMA     => 0x2c;
use constant CPV_SEMICOLON => 0x3b;
use constant CPV_LSQR      => 0x5b;
use constant CPV_BACKSLASH => 0x5c;
use constant CPV_RSQR      => 0x5d;
use constant CPV_LCURL     => 0x7b;
use constant CPV_BAR       => 0x7c;
use constant CPV_RCURL     => 0x7d;

use constant CPV_VISIBLE_MIN => 0x21;
use constant CPV_VISIBLE_MAX => 0x7e;

# Maximum size of string data and tokens.
#
use constant MAX_STRING => 65535;

# Maximum curly nesting within curly string literals.
#
use constant MAX_NEST => 2147483647;

# ======================
# Private static methods
# ======================

# _isLegalCode(c)
# ---------------
#
# Determine whether the given numeric codepoint value is legal, outside
# of string literals and comments.
#
# This range includes all visible, printing ASCII characters, plus 
# Space (SP), Horizontal Tab (HT), and Line Feed (LF).
#
sub _isLegalCode {
  # Get parameter
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  my $c = shift;
  
  (looks_like_number($c) and (int($c) == $c)) or
    die "Wrong parameter type, stopped";
  
  # Check code
  if ((($c >= CPV_VISIBLE_MIN) and ($c <= CPV_VISIBLE_MAX)) or
      ($c == CPV_SP) or ($c == CPV_HT) or ($c == CPV_LF)) {
    return 1;
  } else {
    return 0;
  }
}

# _isAtomicCode(c)
# ----------------
#
# Determine whether the given numeric codepoint value is an atomic
# primitive character.
#
# These characters can stand by themselves as a full token.
#
sub _isAtomicCode {
  # Get parameter
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  my $c = shift;
  
  (looks_like_number($c) and (int($c) == $c)) or
    die "Wrong parameter type, stopped";
  
  # Check code
  if (($c == CPV_LPAREN   ) or ($c == CPV_RPAREN ) or
      ($c == CPV_LSQR     ) or ($c == CPV_RSQR   ) or
      ($c == CPV_COMMA    ) or ($c == CPV_PERCENT) or
      ($c == CPV_SEMICOLON) or ($c == CPV_DQUOTE ) or
      ($c == CPV_LCURL    ) or ($c == CPV_RCURL  )) {
    return 1;
  } else {
    return 0;
  }
}

# _isInclusiveCode(c)
# -------------------
#
# Determine whether the given numeric codepoint value is an inclusive
# token closer.
#
# Inclusive token closers end the token and are included as the last
# character of the token.
#
sub _isInclusiveCode {
  # Get parameter
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  my $c = shift;
  
  (looks_like_number($c) and (int($c) == $c)) or
    die "Wrong parameter type, stopped";
  
  # Check code
  if (($c == CPV_DQUOTE) or ($c == CPV_LCURL)) {
    return 1;
  } else {
    return 0;
  }
}

# _isExclusiveCode(c)
# -------------------
#
# Determine whether the given numeric codepoint value is an exclusive
# token closer.
#
# Exclusive token closers end the token but are not included as the last
# character of the token.
#
sub _isExclusiveCode {
  # Get parameter
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  my $c = shift;
  
  (looks_like_number($c) and (int($c) == $c)) or
    die "Wrong parameter type, stopped";
  
  # Check code
  if (($c == CPV_HT       ) or ($c == CPV_SP       ) or
      ($c == CPV_LF       ) or
      ($c == CPV_LPAREN   ) or ($c == CPV_RPAREN   ) or
      ($c == CPV_LSQR     ) or ($c == CPV_RSQR     ) or
      ($c == CPV_COMMA    ) or ($c == CPV_PERCENT  ) or
      ($c == CPV_SEMICOLON) or ($c == CPV_POUNDSIGN) or
      ($c == CPV_RCURL    )) {
    return 1;
  } else {
    return 0;
  }
}

=head1 CONSTRUCTOR

=over 4

=item B<wrap(filter)>

Construct a new tokenizer instance by wrapping an existing filter
object.

The given C<filter> must be an instance of C<Shastina::Filter>.  In
order for parsing to work properly, the given filter should only be used
by this tokenizer and nothing else.

=cut

sub wrap {
  # Get parameters
  ($#_ == 1) or die "Wrong number of parameters, stopped";
  
  my $invocant = shift;
  my $class = ref($invocant) || $invocant;
  
  my $fil = shift;
  (ref($fil) and $fil->isa("Shastina::Filter"))
    or die "Wrong parameter types, stopped";
  
  # Define the new object
  my $self = { };
  bless($self, $class);
  
  # The '_state' property is one if OK, zero if final token has been
  # returned, negative if an error has been returned
  $self->{'_state'} = 1;
  
  # The '_fil' property stores the filter
  $self->{'_fil'} = $fil;
  
  # The '_err' property stores error codes returned from internal
  # parsing functions
  $self->{'_err'} = 0;
  
  # Return the new object
  return $self;
}

=back

=cut

# ========================
# Private instance methods
# ========================

# _readQuoted()
# -------------
#
# Read a quoted string payload.
#
# This function assumes that the opening quote as already been read.
# The first character read is therefore the first character of string
# data.  The closing quote will be read and consumed by this function.
#
# The return value is either a Unicode string containing the string
# payload (not including either surrounding quote), or undef if there
# was an error.  If undef is returned, the _err property of the object
# will be set to the error code.
#
sub _readQuoted {
  # Check parameter count
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  
  # Get self
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or
    die "Wrong parameter type, stopped";
  
  # Result starts out as empty string and escape count starts out zero
  my $result = '';
  my $esc_count = 0;
  
  # Processing loop
  while(1) {
    # Read a filtered codepoint and check for error conditions
    my $c = $self->{'_fil'}->readCode;
    if ($c < 0) {
      if ($c == SNERR_EOF) {
        # EOF error causes OPENSTR error
        $self->{'_err'} = SNERR_OPENSTR;
        return undef;
      } else {
        # All other errors pass through
        $self->{'_err'} = $c;
        return undef;
      }
    }
    
    # If this codepoint is double quote and the escape count is not odd,
    # then we are done so leave loop
    if (($c == CPV_DQUOTE) and (($esc_count & 0x1) == 0)) {
      last;
    }
    
    # Update escape count -- increment if current character is a
    # backslash, clear otherwise; only least significant bit of the
    # count is important so mask out upper bits to prevent overflow
    if ($c == CPV_BACKSLASH) {
      $esc_count = ($esc_count + 1) & 0x1;
    } else {
      $esc_count = 0;
    }
    
    # Make sure character is not a null character
    if ($c == 0) {
      $self->{'_err'} = SNERR_NULLCHR;
      return undef;
    }
    
    # Append character to result string, watching that string doesn't
    # get too large
    if (length($result) < MAX_STRING) {
      $result = $result . chr($c);
    } else {
      $self->{'_err'} = SNERR_LONGSTR;
      return undef;
    }
  }
  
  # If we got here, return result
  return $result;
}

# _readCurly()
# ------------
#
# Read a curly string payload.
#
# This function assumes that the opening curly bracket as already been
# read.  The first character read is therefore the first character of
# string data.  The closing curly bracket will be read and consumed by
# this function.
#
# The return value is either a Unicode string containing the string
# payload (not including either surrounding curly brace), or undef if
# there was an error.  If undef is returned, the _err property of the
# object will be set to the error code.
#
sub _readCurly {
  # Check parameter count
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  
  # Get self
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or
    die "Wrong parameter type, stopped";
  
  # Result starts out as empty string, escape count starts out zero, and
  # nest level starts out at one
  my $result = '';
  my $esc_count = 0;
  my $nest_level = 1;
  
  # Processing loop
  while(1) {
    # Read a filtered codepoint and check for error conditions
    my $c = $self->{'_fil'}->readCode;
    if ($c < 0) {
      if ($c == SNERR_EOF) {
        # EOF error causes OPENSTR error
        $self->{'_err'} = SNERR_OPENSTR;
        return undef;
      } else {
        # All other errors pass through
        $self->{'_err'} = $c;
        return undef;
      }
    }
    
    # If escape count is not odd, update the nesting level if current
    # character is a curly bracket
    if (($esc_count & 0x1) == 0) {
      if ($c == CPV_LCURL) {
        
        # Left curly -- increase nesting level, watching for overflow
        if ($nest_level < MAX_NEST) {
          $nest_level++;
        } else {
          $self->{'_err'} = SNERR_DEEPCURLY;
          return undef;
        }
        
      } elsif ($c == CPV_RCURL) {
        # Right curly -- decrease nesting level
        $nest_level--;
      }
    }
    
    # If nesting level has been brought down to zero, leave loop
    if ($nest_level < 1) {
      last;
    }
    
    # Update escape count -- increment if current character is a
    # backslash, clear otherwise; only least significant bit of the
    # count is important so mask out upper bits to prevent overflow
    if ($c == CPV_BACKSLASH) {
      $esc_count = ($esc_count + 1) & 0x1;
    } else {
      $esc_count = 0;
    }
    
    # Make sure character is not a null character
    if ($c == 0) {
      $self->{'_err'} = SNERR_NULLCHR;
      return undef;
    }
    
    # Append character to result string, watching that string doesn't
    # get too large
    if (length($result) < MAX_STRING) {
      $result = $result . chr($c);
    } else {
      $self->{'_err'} = SNERR_LONGSTR;
      return undef;
    }
  }
  
  # If we got here, return result
  return $result;
}

# _skip()
# -------
#
# Skip over zero or more characters of whitespace and comments.
# 
# After this operation, the underlying filter will be positioned at the
# first character that is not whitespace and not part of a comment, or
# at the first special or error condition that was encountered.
#
sub _skip {
  # Check parameter count
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  
  # Get self
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or
    die "Wrong parameter type, stopped";
  
  # Skip over whitespace and comments
  my $c = 0;
  while ($c >= 0) {
    # Skip over zero or more characters of whitespace
    for($c = $self->{'_fil'}->readCode;
        ($c == CPV_SP) or ($c == CPV_HT) or ($c == CPV_LF);
        $c = $self->{'_fil'}->readCode) { }
    
    # If we encountered anything except the pound sign, set pushback
    # mode (unless a special condition) and leave the loop
    unless ($c == CPV_POUNDSIGN) {
      if ($c >= 0) {
        $self->{'_fil'}->pushback;
      }
      last;
    }
    
    # We encountered the start of a comment -- read until we encounter
    # LF or some special condition
    for($c = $self->{'_fil'}->readCode;
        ($c >= 0) and ($c != CPV_LF);
        $c = $self->{'_fil'}->readCode) { }
  }
}

# _readPlain()
# ------------
#
# Read a plain token, excluding any string payload.
#
# This function will skip over comments and whitespace before reading
# the token.  Either returns the plain token as a string, or undef if
# there was an error.  If there was an error, the _err property of the
# object will be set to the error code.
#
# For string tokens, this function only reads the opening token and not
# the payload data that follows it.  Input will be positioned such that
# the next byte read will the first byte of string data, after the
# opening quote or left curly.  The opening quote or left curly will be
# the last character in the returned plain token.
#
# Note that it is not possible to use this function to iterate through
# all the tokens in a Shastina source file, because this function
# doesn't handle string data payloads.
#
sub _readPlain {
  # Check parameter count
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  
  # Get self
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or
    die "Wrong parameter type, stopped";
  
  # Skip over whitespace and comments
  $self->_skip;
  
  # Read a character
  my $c = $self->{'_fil'}->readCode;
  if ($c < 0) {
    $self->{'_err'} = $c;
    return undef;
  }
  
  # Check that the character is legal
  unless (_isLegalCode($c)) {
    $self->{'_err'} = SNERR_BADCHAR;
    return undef;
  }
  
  # Begin the token string with this character
  my $tks = chr($c);
  
  # If the first character is a vertical bar, check if the second 
  # character is a semicolon, forming the |; pair; return a |; token in
  # this case; else, unread the second character and proceed
  if ($c == CPV_BAR) {
    my $c2 = $self->{'_fil'}->readCode;
    if ($c2 < 0) {
      $self->{'_err'} = $c2;
      return undef;
    }
    
    if ($c2 == CPV_SEMICOLON) {
      return '|;';
    } else {
      $self->{'_fil'}->pushback;
    }
  }
  
  # If the first character read is not atomic, read additional
  # characters into the token up to the exclusive or inclusive character
  # that ends the token
  unless (_isAtomicCode($c)) {
    # Processing loop
    while (1) {
      # Read the next codepoint
      $c = $self->{'_fil'}->readCode;
      if ($c < 0) {
        $self->{'_err'} = $c;
        return undef;
      }
      
      # Make sure characer is legal
      unless (_isLegalCode($c)) {
        $self->{'_err'} = SNERR_BADCHAR;
        return undef;
      }
      
      # Handling depends on character type
      if (_isInclusiveCode($c)) {
        # Inclusive character, so add the character to the token
        # (checking for overflow), and then leave the loop
        if (length($tks) < MAX_STRING) {
          $tks = $tks . chr($c);
        } else {
          $self->{'_err'} = SNERR_LONGTOKEN;
          return undef;
        }
        last;
        
      } elsif (_isExclusiveCode($c)) {
        # Exclusive character, so push it back and leave the loop
        # without adding it to the token
        $self->{'_fil'}->pushback;
        last;
        
      } else {
        # Legal character, but neither inclusive nor exclusive, so add
        # the character to the token (checking for overflow), and
        # continue on in the loop
        if (length($tks) < MAX_STRING) {
          $tks = $tks . chr($c);
        } else {
          $self->{'_err'} = SNERR_LONGTOKEN;
          return undef;
        }
      }
    }
  }
  
  # If we got here, return the plain token
  return $tks;
}

=head1 INSTANCE METHODS

=over 4

=item B<readToken()>

Read the next token from the Shastina file.

If this returns a scalar, it is either an integer zero indicating the
final C<|;> token was read, or it is a negative integer selecting one of
the error codes from the C<Shastina::Const> module.  Once a scalar has
been returned, that same scalar will be returned on all subsequent
calls.

If this returns a reference, then the reference will be to an array of
length one or three.  If length one, then this is a simple token and the
single element is a Unicode string storing the token.  If length three,
then this is a string token.  The first element is the string prefix
excluding the opening quote or curly; this prefix might be empty.  The
second element is one of the C<SNSTRING> constants from
C<Shastina::Const> indicating the type of quoting for the string.  The
third element is the string data, excluding the opening and closing
quotes or curlies.

=cut

sub readToken {
  # Check parameter count
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  
  # Get self
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or
    die "Wrong parameter type, stopped";
  
  # If already returned a scalar, just return that again
  if ($self->{'_state'} < 1) {
    return $self->{'_state'};
  }
  
  # Read a plain token
  my $plain = $self->_readPlain;
  unless (defined $plain) {
    $self->{'_state'} = $self->{'_err'};
    return $self->{'_err'};
  }
  
  # Identify the type of token
  if ($plain eq '|;') {
    # Final token
    $self->{'_state'} = 0;
    return 0;
    
  } elsif ($plain =~ /"\z/) {
    # Double-quote string token -- read the payload
    my $payload = $self->_readQuoted;
    unless (defined $payload) {
      $self->{'_state'} = $self->{'_err'};
      return $self->{'_err'};
    }
    
    # Trim off the opening quote
    $plain = substr($plain, 0, -1);
    
    # Return the full string token
    return [ $plain, SNSTRING_QUOTED, $payload ];
    
  } elsif ($plain =~ /\{\z/) {
    # Curlied string token -- read the payload
    my $payload = $self->_readCurly;
    unless (defined $payload) {
      $self->{'_state'} = $self->{'_err'};
      return $self->{'_err'};
    }
  
    # Trim off the opening curly
    $plain = substr($plain, 0, -1);
    
    # Return the full string token
    return [ $plain, SNSTRING_CURLY, $payload ];
  
  } else {
    # Simple token that is not final token
    return [ $plain ];
  }
}

=back

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
