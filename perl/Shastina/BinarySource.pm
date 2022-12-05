package Shastina::BinarySource;
use v5.14;
use warnings;
use parent qw(Shastina::Source);

# Shastina imports
use Shastina::Const qw(:ERROR_CODES);

=head1 NAME

Shastina::BinarySource - Shastina input source based on a binary string.

=head1 SYNOPSIS

  use Shastina::BinarySource;
  
  # Suppose we have a binary file in a string
  my $bytes = "...";
  
  # Wrap a Shastina source around a reference to that string
  my $src = Shastina::BinarySource->load(\$bytes);
  
  # See Shastina::Source for further information

=head1 DESCRIPTION

This module defines a Shastina input source based on an in-memory binary
file stored in a string.  This must be a I<binary> string, where each
character is in range [0, 255].  If you have a Unicode string, you need
to use C<Shastina::UnicodeSource> instead.  (If you have an ASCII
string, you can use either source type, but C<Shastina::BinarySource>
may be slightly faster.)

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

The referenced string may only have character codes in range [0, 255].
If any other kind of characters are encountered, an I/O error will be
returned.  If you want to read through a Unicode string, you should be
using the C<Shastina::UnicodeSource> class instead.

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
  
  # The '_src' property stores the reference to the binary string
  $self->{'_src'} = $src_ref;
  
  # The '_state' property will be 1 for normal, 0 for EOF, -1 for IOERR
  $self->{'_state'} = 1;
  
  # The '_count' property stores how many bytes have been successfully
  # read
  $self->{'_count'} = 0;
  
  # The '_char' property stores the string character index that will be
  # read next
  $self->{'_char'} = 0;
  
  # Return the new object
  return $self;
}

=back

=head1 INHERITED METHODS

See the documentation in the base class C<Shastina::Source> for
specifications of each of these functions.

=over 4

=item B<readByte()>

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
  
  # If _char is beyond end of string, go to EOF state and set that state
  # property
  if ($self->{'_char'} >= length(${$self->{'_src'}})) {
    $self->{'_state'} = 0;
    return SNERR_EOF;
  }
  
  # If we got here, read the next character code
  my $c = ord(substr(${$self->{'_src'}}, $self->{'_char'}, 1));
  
  # If character value out of binary range [0, 255], go to IOERR state
  # and set that state property
  if (not (($c >= 0) and ($c <= 255))) {
    $self->{'_state'} = -1;
    return SNERR_IOERR;
  }
  
  # If we got here, we successfully read a character, so update the
  # _char and _count properties
  $self->{'_char' }++;
  $self->{'_count'}++;
  
  # Return the character we just read
  return $c;
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
    
    # Increase _count and _char to just before the offending character
    $self->{'_count'} += $bad;
    $self->{'_char' } += $bad;
    
    # Get the offending character code
    my $badc = ord(substr($remainder, $bad, 1));
    
    # If the offending character code is out of [0, 255] range, then set
    # an IOERR condition and return failure
    if (not (($badc >= 0) and ($badc <= 255))) {
      $self->{'_state'} = -1;
      return 0;
    }
    
    # If we got here, then there is no IOERR but we read some other
    # character, so increment _count and _char to make it appear we just
    # read that offending character
    $self->{'_count'}++;
    $self->{'_char' }++;
    
    # Return failure
    return 0;
  }
  
  # If we got here, we can successfully consume the remainder of the
  # string, set EOF state, and return true
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
