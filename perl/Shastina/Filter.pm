package Shastina::Filter;
use v5.14;
use warnings;

# Shastina modules
use Shastina::Const qw(:ERROR_CODES);
use Shastina::Source;
use Shastina::UTF8 qw(
  sn_utf8_highSurrogate
  sn_utf8_lowSurrogate
  sn_utf8_unpair
  sn_utf8_trail
  sn_utf8_decode
);

=head1 NAME

Shastina::Filter - Shastina codepoint input filter.

=head1 SYNOPSIS

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

=head1 DESCRIPTION

This module performs low-level input filtering and assembles bytes into
codepoints.  It is used internally by the Shastina parser.

Construct instances of this class by passing a C<Shastina::Source>
instance that allows a byte sequence to be read to the C<wrap()>
constructor.  Input bytes will be read from the source and then passed
through the filter defined by this class.

While C<Shastina::Source> objects output bytes, this class outputs
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

Instances of this class module also support I<pushback>.  Pushback
allows clients to push a codepoint they read back into the filter so
that the filter behaves as the codepoint hasn't been read yet.  This is
implemented with an internal buffer, so the underlying source instances
do not need to support pushback.

=cut

# =========
# Constants
# =========

# Constants for special ASCII and Unicode codepoint values.
#
use constant CPV_LF  => 0xa;
use constant CPV_CR  => 0xd;
use constant CPV_BOM => 0xfeff;

# The maximum line count, to avoid overflow problems.
#
# This is set to 2^53-1, which is the maximum integer that can be
# accurately represented with double-precision floating point.
#
use constant MAX_COUNT => 9007199254740991;

=head1 CONSTRUCTOR

=over 4

=item B<wrap(src)>

Construct a new filter instance by wrapping an existing input source.

The given C<src> must be an instance of C<Shastina::Source>.  In order
for filtering to work properly, the given source should be at the
beginning of input and it should not be used by any other client while
it is being read by this filter.

=cut

sub wrap {
  # Get parameters
  ($#_ == 1) or die "Wrong number of parameters, stopped";
  
  my $invocant = shift;
  my $class = ref($invocant) || $invocant;
  
  my $src = shift;
  (ref($src) and $src->isa("Shastina::Source"))
    or die "Wrong parameter types, stopped";
  
  # Define the new object
  my $self = { };
  bless($self, $class);
  
  # The '_src' property stores the input source
  $self->{'_src'} = $src;
  
  # The '_count' property stores the current line count, or zero if
  # nothing has been read yet; when an LF codepoint is buffered, the
  # line count is updated after the LF leaves the buffer
  $self->{'_count'} = 0;
  
  # The '_c' property stores the value that was most recently read; only
  # valid when _count is greater than zero; either a Unicode codepoint
  # (excluding surrogates), or an error code with a value less than zero
  $self->{'_c'} = 0;
  
  # The '_push' flag is set when pushback mode is set; this may only be
  # set when _count is greater than zero and _c is not an error code;
  # when set, the next read operation will return _c again instead of
  # reading another codepoint; the state of this flag also affects the
  # value returned by the count() function
  $self->{'_push'} = 0;
  
  # The '_dbuf' property has a decoding buffer for use with the _readCPV
  # function
  $self->{'_dbuf'} = [0, 0, 0, 0];
  
  # Return the new object
  return $self;
}

=back

=cut

# ========================
# Private instance methods
# ========================

# _readCPV()
# ----------
#
# Read and decode an unfiltered codepoint from the input source.
#
# This does not perform any filtering.  The return is either a decoded
# codepoint (which might be a surrogate), or a Shastina error code with
# value less than zero.
#
# The error codes that might be returned are SNERR_EOF, SNERR_IOERR, or
# SNERR_UTF8.
#
sub _readCPV {
  # Check parameter count
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  
  # Get self
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or
    die "Wrong parameter type, stopped";
  
  # Read the lead byte from the underlying input source into the start
  # of the decoding buffer
  $self->{'_dbuf'}->[0] = $self->{'_src'}->readByte;
  if ($self->{'_dbuf'}->[0] < 0) {
    # Error condition while reading lead byte, so return that
    return $self->{'_dbuf'}->[0];
  }
  
  # Figure out the number of trailing bytes from the leading byte
  my $trail = sn_utf8_trail($self->{'_dbuf'}->[0]);
  ($trail >= 0) or return SNERR_UTF8;
  
  # Read all trailing bytes into the decoding buffer; if EOF error
  # encountered return UTF8 error; else, pass through the error
  for(my $i = 1; $i <= $trail; $i++) {
    $self->{'_dbuf'}->[$i] = $self->{'_src'}->readByte;
    if ($self->{'_dbuf'}->[$i] < 0) {
      if ($self->{'_dbuf'}->[$i] == SNERR_EOF) {
        return SNERR_UTF8;
      } else {
        return $self->{'_dbuf'}->[$i];
      }
    }
  }
  
  # Attempt to decode the decoding buffer into a codepoint and return it
  my $cpv = sn_utf8_decode($self->{'_dbuf'}, $trail);
  if ($cpv >= 0) {
    return $cpv;
  } else {
    return SNERR_UTF8;
  }
}

=head1 INSTANCE METHODS

=over 4

=item B<readCode()>

Read the next filtered codepoint.

The return value is either a Unicode codepoint value zero or greater,
but excluding surrogates; or, it is a Shastina error code that is less
than zero, defined by C<Shastina::Const>.

The low-level filters described earlier in the module documentation will
already have been applied to the codepoints read from this function.  If
pushback mode is on, this function will return the most recent codepoint
and clear pushback mode.

=cut

sub readCode {
  # Check parameter count
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  
  # Get self
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or
    die "Wrong parameter type, stopped";
  
  # If we are not in pushback mode AND either we haven't read anything
  # yet, or we have read something other than an error code, then we
  # need to read another codepoint
  if ((not $self->{'_push'}) and
        (($self->{'_count'} == 0) or ($self->{'_c'} >= 0))) {
    
    # Read an unfiltered codepoint from the input source
    my $cpv = $self->_readCPV;
    
    # Apply BOM filtering
    if ($cpv == CPV_BOM) {
      # If this BOM codepoint is the very first codepoint read, then
      # skip it by reading another codepoint
      if ($self->{'_count'} == 0) {
        $cpv = $self->_readCPV;
      }
    }
    
    # Apply CR+LF filtering
    if ($cpv == CPV_CR) {
      # We read a CR, so read the next codepoint to skip it, which must
      # either be a LF or an error condition
      $cpv = $self->_readCPV;
      if (($cpv >= 0) and ($cpv != CPV_LF)) {
        $cpv = SNERR_BADCR;
      }
    }
    
    # Apply surrogate filtering
    if (sn_utf8_highSurrogate($cpv)) {
      # Read a high surrogate, so read the next codepoint
      my $lo = $self->_readCPV;
      
      # Check what we read next
      if (sn_utf8_lowSurrogate($lo)) {
        # We got properly paired surrogates, so replace the read
        # codepoint with the decoded value
        $cpv = sn_utf8_unpair($cpv, $lo);
        
      } elsif ($lo < 0) {
        # Some kind of error reading the low surrogate, so replace
        # codepoint with that error status
        $cpv = $lo;
        
      } else {
        # Something other than a low surrogate was read, so replace
        # codepoint with a pairing error
        $cpv = SNERR_UNPAIRED;
      }
      
    } elsif (sn_utf8_lowSurrogate($cpv)) {
      # Reading a low surrogate is an error because surrogate pairs must
      # begin with a high surrogate
      $cpv = SNERR_UNPAIRED;
    }
    
    # Update state
    if ($cpv >= 0) {
      # Got a filtered codepoint -- update _count
      if ($self->{'_count'} == 0) {
        # This is very first codepoint, so just change the line number
        # to one
        $self->{'_count'} = 1;
        
      } elsif ($self->{'_count'} > 0) {
        # Not very first codepoint and line count has not overflown, so
        # if an LF is leaving the buffer, increment line count, watching
        # for overflow
        if ($self->{'_c'} == CPV_LF) {
          if ($self->{'_count'} < MAX_COUNT) {
            $self->{'_count'}++;
          } else {
            $self->{'_count'} = -1;
          }
        }
      }
      
      # Store the new filtered codepoint in the buffer
      $self->{'_c'} = $cpv;
      
    } else {
      # Error occured
      $self->{'_c'} = $cpv;
    }
  }
  
  # Always clear the pushback flag here
  $self->{'_push'} = 0;
  
  # Return the buffered codepoint or the buffered error status
  return $self->{'_c'};
}

=item B<count()>

Return the current line count.

The first line of the file is line one.  C<undef> is returned in the
unlikely event that the line count overflows.  (This only happens if
there are trillions of lines.)

The line count is affected by pushback mode, changing backwards if
characters are pushed back before a line break.

=cut

sub count {
  # Check parameter count
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  
  # Get self
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or
    die "Wrong parameter type, stopped";
  
  # Get current line count
  my $lcv = $self->{'_count'};
  
  # If count is negative, then return undef
  ($lcv >= 0) or return undef;
  
  # If count is zero, indicating the very beginning of the file, change
  # it to line one
  if ($lcv == 0) {
    $lcv = 1;
  }
  
  # If we're not in pushback mode, _c is LF, and we're not at the very
  # beginning of the file, then increment returned line count, watching
  # for overflow
  if ((not $self->{'_push'}) and ($self->{'_count'} > 0) and
        ($self->{'_c'} == CPV_LF)) {
    if ($lcv < MAX_COUNT) {
      $lcv++;
    } else {
      $lcv = undef;
    }
  }
  
  # Return adjusted line count
  return $lcv;
}

=item B<pushback()>

Set pushback mode, so the next call to C<readCode> will once again
return the codepoint that was just returned.

This call is ignored if the filter is currently in EOF or some other
error condition.

Fatal errors occur if the filter is already in pushback mode or if no
characters have been read yet.

=cut

sub pushback {
  # Check parameter count
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  
  # Get self
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or
    die "Wrong parameter type, stopped";
  
  # Only proceed if not in an error code state
  if (($self->{'_count'} == 0) || ($self->{'_c'} >= 0)) {
    # At least one codepoint must have already been read
    ($self->{'_count'} > 0) or die "Invalid pushback, stopped";
    
    # Can't already be in pushback state
    (not $self->{'_push'}) or die "Invalid pushback, stopped";
    
    # Set pushback flag
    $self->{'_push'} = 1;
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
