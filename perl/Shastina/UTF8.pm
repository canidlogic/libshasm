package Shastina::UTF8;
use v5.14;
use warnings;
use parent qw(Exporter);

# Core imports
use Scalar::Util qw(looks_like_number);

=head1 NAME

Shastina::UTF8 - Shastina's built-in UTF-8 codec.

=head1 SYNOPSIS

  use Shastina::UTF8 qw(
    sn_utf8_isCodepoint
    sn_utf8_isSurrogate
    sn_utf8_highSurrogate
    sn_utf8_lowSurrogate
    sn_utf8_unpair
    sn_utf8_encode
    sn_utf8_trail
    sn_utf8_decode
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

=head1 DESCRIPTION

This module provides functions for encoding and decoding UTF-8, which
are used internally by Shastina.

Perl has UTF-8 decoding and encoding built in, of course.  Shastina uses
its own custom codec to make sure that the details of UTF-8 decoding are
exactly the same as the C implementation of Shastina, particularly
regarding the issue of surrogate pairs encoded into UTF-8 (which is not
supposed to be done, but is supported by Shastina and often encountered
in practice).

=head1 FUNCTIONS

=over 4

=item B<sn_utf8_isCodepoint(code)>

Given an integer value, check whether it is in Unicode codepoint range.
This range includes surrogates and also includes supplemental
codepoints.  Return 1 if in Unicode codepoint range and 0 otherwise.

=cut

sub sn_utf8_isCodepoint {
  # Get parameter
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  my $code = shift;
  
  (looks_like_number($code) and ($code == int($code))) or
    die "Wrong parameter type, stopped";
  
  # Check range and return result
  if (($code >= 0) and ($code <= 0x10ffff)) {
    return 1;
  } else {
    return 0;
  }
}

=item B<sn_utf8_isSurrogate(code)>

Given an integer value, check whether it is a surrogate codepoint.
Return 1 if surrogate and 0 otherwise.

=cut

sub sn_utf8_isSurrogate {
  # Get parameter
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  my $code = shift;
  
  (looks_like_number($code) and ($code == int($code))) or
    die "Wrong parameter type, stopped";
  
  # Check range and return result
  if (($code >= 0xd800) and ($code <= 0xdfff)) {
    return 1;
  } else {
    return 0;
  }
}

=item B<sn_utf8_highSurrogate(code)>

Given an integer value, check whether it is a high surrogate codepoint.
Return 1 if high surrogate and 0 otherwise.

=cut

sub sn_utf8_highSurrogate {
  # Get parameter
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  my $code = shift;
  
  (looks_like_number($code) and ($code == int($code))) or
    die "Wrong parameter type, stopped";
  
  # Check range and return result
  if (($code >= 0xd800) and ($code <= 0xdbff)) {
    return 1;
  } else {
    return 0;
  }
}

=item B<sn_utf8_lowSurrogate(code)>

Given an integer value, check whether it is a high surrogate codepoint.
Return 1 if high surrogate and 0 otherwise.

=cut

sub sn_utf8_lowSurrogate {
  # Get parameter
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  my $code = shift;
  
  (looks_like_number($code) and ($code == int($code))) or
    die "Wrong parameter type, stopped";
  
  # Check range and return result
  if (($code >= 0xdc00) and ($code <= 0xdfff)) {
    return 1;
  } else {
    return 0;
  }
}

=item B<sn_utf8_unpair(c1, c2)>

Given two integer codepoints corresponding to a surrogate pair, return
the supplemental codepoint they return to.

C<c1> must satisfy C<sn_utf8_highSurrogate> and C<c2> must satisfy
C<sn_utf8_lowSurrogate> or a fatal error occurs.

=cut

sub sn_utf8_unpair {
  # Get parameters
  ($#_ == 1) or die "Wrong number of parameters, stopped";
  
  my $c1 = shift;
  my $c2 = shift;
  
  (sn_utf8_highSurrogate($c1) and sn_utf8_lowSurrogate($c2)) or
    die "Invalid surrogate pair, stopped";
  
  # Convert surrogates to offsets within each surrogate set
  $c1 -= 0xd800;
  $c2 -= 0xdc00;
  
  # Combine into a supplemental offset
  my $result = ($c1 << 10) + $c2;
  
  # Add supplemental offset to start of supplementals to get the final
  # codepoint
  $result += 0x10000;
  
  # Return result
  return $result;
}

=item B<sn_utf8_encode(code, rbuf)>

Encode a codepoint into UTF-8 bytes.

C<code> is the codepoint to encode.  It must satisfy
C<sn_utf8_isCodepoint> but must I<not> satisfy C<sn_utf8_isSurrogate>.

C<rbuf> is a reference to an encoding buffer array.  This must be an
array reference, and the referenced array must have length at least
four.  This allows the same array to be used repeatedly, to make
repeated encoding operations more efficient.

This function returns the number of trailing bytes in the encoding.
This is an integer that is in range [0, 3].  A value of zero means that
one byte value was written into C<rbuf>, a value of one means that two
byte values were written into C<rbuf>, and so forth.

=cut

sub sn_utf8_encode {
  # Get parameters
  ($#_ == 1) or die "Wrong number of parameters, stopped";
  
  my $code = shift;
  my $rbuf = shift;
  
  (sn_utf8_isCodepoint($code) and (not sn_utf8_isSurrogate($code))) or
    die "Invalid parameter, stopped";
  
  (ref($rbuf) eq 'ARRAY') or die "Invalid parameter type, stopped";
  (scalar(@$rbuf) >= 4) or die "Invalid parameter type, stopped";
  
  # Determine the number of trail bytes based on the range of the
  # codepoint
  my $trail = 0;
  if ($code >= 0x10000) {
    $trail = 3;
  } elsif ($code >= 0x800) {
    $trail = 2;
  } elsif ($code >= 0x80) {
    $trail = 1;
  }
  
  # Write any trail bytes to the buffer and update the codepoint to
  # remove the shifted bytes
  for(my $i = 0; $i < $trail; $i++) {
    # Form the current trailer byte and shift out bits from codepoint
    my $tby = 0x80 + ($code & 0x3f);
    $code >>= 6;
    
    # We need to add trailer bytes in reverse order so that they will
    # be in the correct big endian order in the encoding
    $rbuf->[$trail - $i] = $tby;
  }
  
  # Form the leading byte from the remaining bits
  my $lby = $code;
  if ($trail == 3) {
    $lby += 0xf0;
  } elsif ($trail == 2) {
    $lby += 0xe0;
  } elsif ($trail == 1) {
    $lby += 0xc0;
  }
  
  # Put the leading byte at the start of the buffer and return number of
  # trailing bytes
  $rbuf->[0] = $lby;
  return $trail;
}

=item B<sn_utf8_trail(lead)>

Given a lead byte value of a UTF-8 encoding, determine how many trailer
bytes follow it.

C<lead> is the lead byte value.  It must be an integer in range
[0, 255].

The return value is the number of trailing bytes that follows the given
lead byte.  The range is [0, 3].  If the given byte value is not a valid
leading byte, then -1 is returned.

=cut

sub sn_utf8_trail {
  # Get parameter
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  my $lead = shift;
  
  (looks_like_number($lead) and ($lead == int($lead)) and
    ($lead >= 0) and ($lead <= 255)) or
      die "Invalid parameter type, stopped";
  
  # Figure out the trailing byte count, or -1 if not valid lead
  my $trail = -1;
  
  if (($lead & 0xf8) == 0xf0) {
    $trail = 3;
  
  } elsif (($lead & 0xf0) == 0xe0) {
    $trail = 2;
    
  } elsif (($lead & 0xe0) == 0xc0) {
    $trail = 1;
    
  } elsif (($lead & 0x80) == 0) {
    $trail = 0;
  }
  
  # Return result
  return $trail;
}

=item B<sn_utf8_decode(rbuf, trail)>

Decode UTF-8 bytes into a Unicode codepoint.

C<rbuf> is a reference to a decoding buffer array.  This must be an
array reference, the referenced array must have length at least four,
and the first four elements must be integers in range [0, 255].  The
decoding buffer will not be modified by this function.

C<trail> is the number of trailing bytes that follow the leading byte in
the decoding buffer array.  It must be in range [0, 3], and equal to
C<sn_utf8_trail()> when applied to the first byte in the decoding buffer
array or decoding will fail.

This function returns the decoded Unicode codepoint, or -1 if decoding
failed.  Surrogate codepoints will successfully decode with this
function, even though they are not technically supposed to be present in
UTF-8.  However, overlong encodings will cause decoding to fail.

=cut

# Constant storing the minimum codepoint value for each trail count,
# where the trail count matches the array index
#
my @OVERLONG_CHECK = (
  0x00, 0x80, 0x800, 0x10000
);

# The function
#
sub sn_utf8_decode {
  # Get parameters
  ($#_ == 1) or die "Wrong number of parameters, stopped";
  
  my $rbuf  = shift;
  my $trail = shift;
  
  (ref($rbuf) eq 'ARRAY') or die "Invalid parameter type, stopped";
  (scalar(@$rbuf) >= 4) or die "Invalid parameter type, stopped";
  for(my $i = 0; $i < 4; $i++) {
    my $v = $rbuf->[$i];
    (looks_like_number($v) and (int($v) == $v) and
      ($v >= 0) and ($v <= 255)) or
        die "Invalid parameter, stopped";
  }
  
  (looks_like_number($trail) and (int($trail) == $trail) and
    ($trail >= 0) and ($trail <= 3)) or
      die "Invalid parameter, stopped";
  
  # Fail if leading byte trail count does not match given trail count
  ($trail == sn_utf8_trail($rbuf->[0])) or return -1;
  
  # Result starts out with the decoded bits in the lead byte
  my $result;
  if ($trail == 0) {
    $result = $rbuf->[0];
    
  } elsif ($trail == 1) {
    $result = $rbuf->[0] & 0x1f;
    
  } elsif ($trail == 2) {
    $result = $rbuf->[0] & 0x0f;
    
  } elsif ($trail == 3) {
    $result = $rbuf->[0] & 0x07;
    
  } else {
    die "Unexpected";
  }
  
  # Append any trailing bytes
  for(my $i = 1; $i <= $trail; $i++) {
    (($rbuf->[$i] & 0xc0) == 0x80) or return -1;
    $result = ($result << 6) + ($rbuf->[$i] & 0x3f);
  }
  
  # Fail if overlong encoding
  ($result >= $OVERLONG_CHECK[$trail]) or return -1;
  
  # If we got here, return result
  return $result;
}

=back

=cut

# ==============
# Module exports
# ==============

our @EXPORT_OK = qw(
  sn_utf8_isCodepoint
  sn_utf8_isSurrogate
  sn_utf8_highSurrogate
  sn_utf8_lowSurrogate
  sn_utf8_unpair
  sn_utf8_encode
  sn_utf8_trail
  sn_utf8_decode
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
