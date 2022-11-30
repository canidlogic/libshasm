package Shastina::InputSource;
use v5.14;
use warnings;
use parent qw(Shastina::Source);

# Core imports
use Scalar::Util qw(looks_like_number);

# Shastina imports
use Shastina::Const qw(:ERROR_CODES);

=head1 NAME

Shastina::InputSource - Shastina input source that reads from standard
input.

=head1 SYNOPSIS

  use Shastina::InputSource;
  
  # Wrap a Shastina source around standard input
  my $src = Shastina::InputSource->load;
  unless (defined $src) {
    # Failed to open input source
    ...
  }
  
  # See Shastina::Source for further information

=head1 DESCRIPTION

This module defines a Shastina input source based on standard input.

In order for this to work correctly, you must open this input source
before anything touches standard input in any way.  This input source
needs to set standard input into raw mode during the load procedure.
Nothing except this input source should interact with standard input
until parsing is complete.

Standard input can only be read through once, so this input source does
I<not> support multipass operation.  Also, you can't create more than
one instance of this class.

See the documentation for the parent class C<Shastina::Source> for more
information about how input sources work.

=cut

# =========
# Constants
# =========

# The size in bytes of the buffer to use when reading through input.
#
use constant BUFFER_SIZE => 4096;

# The maximum byte count size, to avoid overflow problems.
#
# This is set to 2^53-1, which is the maximum integer that can be
# accurately represented with double-precision floating point.
#
use constant MAX_COUNT => 9007199254740991;

# ==========
# Local data
# ==========

# Flag indicating whether an instance has been loaded yet.
#
# This is used to prevent multiple instances of this class.
#
my $_loaded = 0;

=head1 CONSTRUCTOR

=over 4

=item B<load([bufsize])>

Construct a new input source around standard input.

If the optional C<bufsize> parameter is provided, it is an integer value
greater than zero that identifies the size of the buffer to allocate for
reading through input.  If not given, a sensible default buffer size 
will be used.

If this constructor is unable to set the proper input mode on standard
input or if another instance of this class has already been constructed,
the constructor fails and returns C<undef>.

=cut

sub load {
  # Get parameters
  (($#_ == 0) or ($#_ == 1)) or
    die "Wrong number of parameters, stopped";
  
  my $invocant = shift;
  my $class = ref($invocant) || $invocant;
  
  my $buf_size;
  if ($#_ == 1) {
    $buf_size = shift;
  } else {
    $buf_size = BUFFER_SIZE;
  }
  
  (looks_like_number($buf_size) and (int($buf_size) == $buf_size) and
    ($buf_size > 0)) or die "Invalid parameter, stopped";
  
  # If another instance already constructed, fail
  ($_loaded < 1) or return undef;
  
  # Attempt to set raw mode on standard input, or fail
  (binmode(STDIN, ":raw") or return undef);
  
  # Define the new object
  my $self = { };
  bless($self, $class);
  
  # The '_state' property will be 1 for normal, 0 for EOF, -1 for IOERR
  $self->{'_state'} = 1;
  
  # The '_count' property stores how many bytes have been successfully
  # read; set to -1 if the count has overflown
  $self->{'_count'} = 0;
  
  # The '_bsize' property stores the size of the buffer used for this
  # object
  $self->{'_bsize'} = $buf_size;
  
  # The '_buf' property holds a binary string for the byte buffer
  $self->{'_buf'} = '';
  
  # The '_fill' property stores the number of bytes that have been
  # written into the byte buffer
  $self->{'_fill'} = 0;
  
  # The '_take' property stores the number of bytes that have been read
  # out of the byte buffer
  $self->{'_take'} = 0;
  
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
  
  # If nothing is buffered, refill the buffer
  unless ($self->{'_take'} < $self->{'_fill'}) {
    # Attempt to read into the buffer
    my $rc = read STDIN, $self->{'_buf'}, $self->{'_bsize'};
    
    # Handle EOF and IOERR conditions
    if (not (defined $rc)) {
      # I/O error
      $self->{'_state'} = -1;
      return SNERR_IOERR;
      
    } elsif ($rc < 1) {
      # EOF
      $self->{'_state'} = 0;
      return SNERR_EOF;
    }
    
    # If we got here, at least one character was read, so update the
    # _take and _fill
    $self->{'_take'} = 0;
    $self->{'_fill'} = $rc;
  }
  
  # If we got here, buffer should have at least one byte in it; get
  # the first buffered byte
  my $c = ord(substr($self->{'_buf'}, $self->{'_take'}, 1));
  (($c >= 0) and ($c <= 255)) or die "Unexpected";
  
  # Update _take, clearing both _take and _fill if we've exhausted the
  # buffer
  $self->{'_take'}++;
  if ($self->{'_take'} >= $self->{'_fill'}) {
    $self->{'_take'} = 0;
    $self->{'_fill'} = 0;
  }
  
  # If count has not overflown yet, attempt to increment, watching for
  # overflow
  if ($self->{'_count'} >= 0) {
    if ($self->{'_count'} < MAX_COUNT) {
      $self->{'_count'}++;
    } else {
      $self->{'_count'} = -1;
    }
  } 
  
  # Return the buffered byte
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

In the unlikely event that the byte counter has overflown, undef will be
returned.  This only happens if there are many terabytes of data.

=cut

sub count {
  # Check parameter count
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  
  # Get self
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or
    die "Wrong parameter type, stopped";
  
  # Return the count
  return ($self->{'_count'} >= 0) ? $self->{'_count'} : undef;
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
  
  # Enter processing loop
  while(1) {
    # If nothing is buffered, then attempt to fill buffer
    unless ($self->{'_take'} < $self->{'_fill'}) {
      # Attempt to read into the buffer
      my $rc = read STDIN, $self->{'_buf'}, $self->{'_bsize'};
      
      # Handle EOF and IOERR conditions
      if (not (defined $rc)) {
        # I/O error, so update state and fail
        $self->{'_state'} = -1;
        return 0;
        
      } elsif ($rc < 1) {
        # EOF, so update state and succeed
        $self->{'_state'} = 0;
        return 1;
      }
      
      # If we got here, at least one character was read, so update the
      # _take and _fill
      $self->{'_take'} = 0;
      $self->{'_fill'} = $rc;
    }
    
    # There is now at least one byte in the buffer; look for anything in
    # the buffer that is not SP HT CR LF
    if ($self->{'_buf'} =~ /[^ \t\r\n]/g) {
      # Get the index in the buffer of the offending character
      my $bad = pos $self->{'_buf'};
      $bad--;
      
      # Figure out how many additional bytes would be read up to AND
      # INCLUDING the offending character, taking into account what has
      # already been taken from the buffer
      my $adt = $bad - $self->{'_take'} + 1;
      
      # Update _count as if we just read the offending character
      if ($self->{'_count'} >= 0) {
        if ($self->{'_count'} <= MAX_COUNT - $adt) {
          $self->{'_count'} += $adt;
        } else {
          $self->{'_count'} = -1;
        }
      }
      
      # Update _take and _fill as if we just read the offending
      # character
      $self->{'_take'} += $adt;
      unless($self->{'_take'} < $self->{'_fill'}) {
        $self->{'_take'} = 0;
        $self->{'_fill'} = 0;
      }
      
      # Return failure
      return 0;
    }
    
    # No offending characters were found in the buffer, so compute how
    # many characters we should update _count by, taking into account
    # what has already been read from the buffer
    my $adc = $self->{'_fill'} - $self->{'_take'};
    
    # Update _count, act as though we read the whole buffer and loop
    # back for more processing
    if ($self->{'_count'} >= 0) {
      if ($self->{'_count'} <= MAX_COUNT - $adc) {
        $self->{'_count'} += $adc;
      } else {
        $self->{'_count'} = -1;
      }
    }
    $self->{'_take'} = 0;
    $self->{'_fill'} = 0;
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
