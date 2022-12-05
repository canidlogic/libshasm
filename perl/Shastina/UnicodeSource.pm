package Shastina::UnicodeSource;
use v5.14;
use warnings;
use parent qw(Shastina::Source);

# Shastina imports
use Shastina::Const qw(:ERROR_CODES);
use Shastina::UTF8 qw(sn_utf8_isCodepoint sn_utf8_encode);

=head1 NAME

Shastina::UnicodeSource - Shastina input source based on a Unicode
string.

=head1 SYNOPSIS

  use Shastina::UnicodeSource;
  
  # Suppose we have a file in a Unicode string
  my $codepoints = "...";
  
  # Wrap a Shastina source around a reference to that string
  my $src = Shastina::UnicodeSource->load(\$codepoints);
  
  # See Shastina::Source for further information

=head1 DESCRIPTION

This module defines a Shastina input source based on an in-memory
Unicode file stored in a string.  This must be a I<Unicode> string,
where each character is an individual Unicode codepoint in range
[0, 0x10ffff].  If you have a binary string, you need to use
C<Shastina::BinarySource> instead.  (If you have an ASCII string, you
can use either source type, but C<Shastina::BinarySource> may be
slightly faster.)

Shastina requires a binary input stream, so this subclass will encode
each codepoint into UTF-8 as it goes along and return the unsigned byte
values that Shastina is expecting.  Hence, it is preferable to use
C<Shastina::BinarySource> as an in-memory source, if you can.

See the documentation for the parent class C<Shastina::Source> for more
information about how input sources work.

This subclass supports multipass operation.

=head1 CONSTRUCTOR

=over 4

=item B<load(string_ref)>

Construct a new input source around the given string reference.  Note
that you must pass a I<reference> to a string, rather than directly
passing a string.  If the referenced string changes while this source is
still in use, the changes will be immediately reflected in the input
source.  In other words, be careful not to modify the referenced string
until you are done parsing it.

The referenced string must be a Unicode string, with characters in
codepoint range [0, 0x10ffff].  If any other kind of characters are
encountered, an I/O error will be returned.  If you want to read through
a binary string, you should be using the C<Shastina::BinarySource> class
instead.

Surrogate codepoints are allowed.  Shastina's input layer will
automatically reassemble surrogate pairs that were encoded in UTF-8,
even though this is not technically correct.  This subclass will just
encode surrogates like any other Unicode codepoint.

=cut

sub load {
  # Get parameters
  ($#_ == 1) or die "Wrong number of parameters, stopped";
  
  my $invocant = shift;
  my $class = ref($invocant) || $invocant;
  
  my $src_ref = shift;
  (ref($src_ref) eq 'SCALAR') or die "Wrong parameter types, stopped";
  
  # Define the new object
  my $self = { };
  bless($self, $class);
  
  # The '_src' property stores the reference to the Unicode string
  $self->{'_src'} = $src_ref;
  
  # The '_state' property will be 1 for normal, 0 for EOF, -1 for IOERR
  $self->{'_state'} = 1;
  
  # The '_count' property stores how many BYTES have been successfully
  # read
  $self->{'_count'} = 0;
  
  # The '_char' property stores the string CODEPOINT index that will be
  # read next
  $self->{'_char'} = 0;
  
  # The '_bfill' property stores the number of bytes that are currently
  # in the encoding buffer
  $self->{'_bfill'} = 0;
  
  # The '_btake' property stores the number of bytes that have been
  # read and returned from the encoding buffer
  $self->{'_btake'} = 0;
  
  # The '_buf' property is a reference to an array of four elements that
  # is used for encoding codepoints; _bfill indicates how many bytes are
  # in this buffer and _btake indicates how many bytes have been taken
  # out
  $self->{'_buf'} = [0, 0, 0, 0];
  
  # Return the new object
  return $self;
}

=back

=head1 INHERITED METHODS

See the documentation in the base class C<Shastina::Source> for
specifications of each of these functions.

=over 4

=item B<readByte()>

Note that this subclass returns unsigned byte values in the UTF-8
encoding of codepoints from the source string.  It does I<not> return
codepoints.

=cut

sub readByte {
  # Check parameter count
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  
  # Get self
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or
    die "Wrong parameter type, stopped";
  
  # If we are in a special state, return that
  if ($self->{'_state'} == 0) {
    return SNERR_EOF;
    
  } elsif ($self->{'_state'} < 0) {
    return SNERR_IOERR;
  }
  
  # If there is something in the encoding buffer, use that instead of
  # reading another codepoint
  if ($self->{'_bfill'} > 0) {
    # _btake should be less than _bfill if we got here
    ($self->{'_btake'} < $self->{'_bfill'}) or die "Unexpected";
    
    # Grab the next buffered byte
    my $nb = $self->{'_buf'}->[$self->{'_btake'}];
    
    # Update _btake and clear the buffer if this was the last buffered
    # byte
    $self->{'_btake'}++;
    if ($self->{'_btake'} >= $self->{'_bfill'}) {
      $self->{'_bfill'} = 0;
      $self->{'_btake'} = 0;
    }
    
    # If we got here, update the _count property and return the buffered
    # byte
    $self->{'_count'}++;
    return $nb;
  }
  
  # If we got here, there is nothing in the encoding buffer; if _char is
  # beyond end of string, go to EOF state and set that state property
  if ($self->{'_char'} >= length(${$self->{'_src'}})) {
    $self->{'_state'} = 0;
    return SNERR_EOF;
  }
  
  # If we got here, read the next codepoint and update the _char 
  # property
  my $c = ord(substr(${$self->{'_src'}}, $self->{'_char'}, 1));
  $self->{'_char'}++;
  
  # If codepoint value out of Unicode range, go to IOERR state and set
  # that state property
  if (not sn_utf8_isCodepoint($c)) {
    $self->{'_state'} = -1;
    return SNERR_IOERR;
  }
  
  # If the codepoint value is in range [0, 0x7f], then just update
  # _count property and return the codepoint value directly, since
  # codepoints in this range just encode to unsigned byte values with an
  # identical numeric value
  if ($c <= 0x7f) {
    $self->{'_count'}++;
    return $c;
  }
  
  # If we got here, encode into a multi-byte UTF-8 encoding in _buf and
  # set _bfill as one greater than the number of trailing bytes, which
  # should be at least two buffered bytes
  $self->{'_bfill'} = 1 + sn_utf8_encode($c, $self->{'_buf'});
  ($self->{'_bfill'} >= 2) or die "Unexpected";
  
  # Set _btake to one because we are going to return the first byte
  # already
  $self->{'_btake'} = 1;
  
  # Update the _count property and return the first buffered byte
  $self->{'_count'}++;
  return $self->{'_buf'}->[0];
}

=item B<hasError()>

=cut

sub hasError {
  # Check parameter count
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  
  # Get self
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or
    die "Wrong parameter type, stopped";
  
  # Return 1 if IOERR state, else 0
  return ($self->{'_state'} < 0) ? 1 : 0;
}

=item B<count()>

This returns the number of encoded UTF-8 bytes that have been read
through this subclass, I<not> the number of codepoints that have been
read from the underlying string.

=cut

sub count {
  # Check parameter count
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  
  # Get self
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or
    die "Wrong parameter type, stopped";
  
  # Return the count
  return $self->{'_count'};
}

=item B<consume()>

This subclass provides a special implementation that is much faster than
just calling C<readByte()> repeatedly.

=cut

sub consume {
  # Check parameter count
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  
  # Get self
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or
    die "Wrong parameter type, stopped";
  
  # If we are in a special state, return appropriate value
  if ($self->{'_state'} == 0) {
    # We are in EOF state, so this call succeeds
    return 1;
    
  } elsif ($self->{'_state'} < 0) {
    # We are in IOERR state, so this call fails
    return 0;
  }
  
  # If we got here and there is anything buffered, then take one byte
  # out of the buffer, increase _count, and return failure
  if ($self->{'_bfill'} > 0) {
    ($self->{'_btake'} < $self->{'_bfill'}) or die "Unexpected";
    $self->{'_btake'}++;
    if ($self->{'_btake'} >= $self->{'_bfill'}) {
      $self->{'_btake'} = 0;
      $self->{'_bfill'} = 0;
    }
    $self->{'_count'}++;
    return 0;
  }
  
  # If we got here, then check that _char index is still within the
  # underlying string; if it is not, then set EOF state and return
  # successful
  if ($self->{'_char'} >= length(${$self->{'_src'}})) {
    $self->{'_state'} = 0;
    return 1;
  }
  
  # If we got here, then get a substring containing everything left in
  # the underlying string
  my $remainder = substr(${$self->{'_src'}}, $self->{'_char'});
  
  # Look for anything that is not SP HT CR LF
  if ($remainder =~ /[^ \t\r\n]/g) {
    # Get the index in the remainder string of the offending character
    my $bad = pos $remainder;
    $bad--;
    
    # Increase _count and _char to just before the offending character;
    # we can assume that everything up to the offending character is in
    # ASCII range and so has exactly one byte in the encoding
    $self->{'_count'} += $bad;
    $self->{'_char' } += $bad;
    
    # Get the offending character code
    my $badc = ord(substr($remainder, $bad, 1));
    
    # If the offending character code is out of Unicode codepoint range,
    # then set an IOERR condition and return failure
    if (not sn_utf8_isCodepoint($badc)) {
      $self->{'_state'} = -1;
      return 0;
    }
    
    # If we got here, increase _char
    $self->{'_char' }++;
    
    # If the offending character code is in ASCII range, then just
    # increase _count and return failure
    if ($badc <= 0x7f) {
      $self->{'_count'}++;
      return 0;
    }
    
    # If we got here, then encode offending character into a multi-byte
    # UTF-8 encoding in _buf and set _bfill as one greater than the number
    # of trailing bytes, which should be at least two buffered bytes
    $self->{'_bfill'} = 1 + sn_utf8_encode($badc, $self->{'_buf'});
    ($self->{'_bfill'} >= 2) or die "Unexpected";
    
    # Set _btake to one because we are going to consume the first byte
    # already
    $self->{'_btake'} = 1;
    
    # Update the _count property and return failure
    $self->{'_count'}++;
    return 0;
  }
  
  # If we got here, we can successfully consume the remainder of the
  # string, set EOF state, and return true; since remainder must have
  # only ASCII characters in this case, we can assume that each
  # character in the remainder is encoded in one byte
  $self->{'_count'} += length($remainder);
  $self->{'_char' } += length($remainder);
  $self->{'_state'} = 0;
  return 1;
}

=item B<multipass()>

This subclass I<does> support multipass, so this function always returns
1.

=cut

sub multipass {
  # Check parameter count
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  
  # Get self
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or
    die "Wrong parameter type, stopped";
  
  # This subclass supports multipass
  return 1;
}

=item B<rewind()>

This subclass I<does> support multipass, so this function is available.
Rewinding will clear any I/O error.

=cut

sub rewind {
  # Check parameter count
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  
  # Get self
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or
    die "Wrong parameter type, stopped";
  
  # Reset state back to initial values
  $self->{'_state'} = 1;
  $self->{'_count'} = 0;
  $self->{'_char'} = 0;
  $self->{'_bfill'} = 0;
  $self->{'_btake'} = 0;
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
