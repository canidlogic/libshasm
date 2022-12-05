package Shastina::FileSource;
use v5.14;
use warnings;
use parent qw(Shastina::Source);

# Core imports
use Scalar::Util qw(looks_like_number);

# Shastina imports
use Shastina::Const qw(:ERROR_CODES);

=head1 NAME

Shastina::FileSource - Shastina input source based on a disk file.

=head1 SYNOPSIS

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

=head1 DESCRIPTION

This module defines a Shastina input source based on a disk file.

See the documentation for the parent class C<Shastina::Source> for more
information about how input sources work.  Note however, that this
subclass has the special method C<closeFile()>.  See the documentation
of that method for further information.

This subclass supports multipass operation.

=cut

# =========
# Constants
# =========

# The size in bytes of the buffer to use when reading through files.
#
use constant BUFFER_SIZE => 4096;

# The maximum byte count size, to avoid overflow problems.
#
# This is set to 2^53-1, which is the maximum integer that can be
# accurately represented with double-precision floating point.
#
use constant MAX_COUNT => 9007199254740991;

=head1 CONSTRUCTOR

=over 4

=item B<load(path[, bufsize])>

Construct a new input source around the file identified by the given
path.

The file will be kept open until either the object destructor is run or
the underlying file is closed explicitly with C<closeFile()> (whichever
occurs first).

If the optional C<bufsize> parameter is provided, it is an integer value
greater than zero that identifies the size of the buffer to allocate for
reading through the file.  If not given, a sensible default buffer size
will be used.

If this constructor is unable to open the file, it returns C<undef>.

=cut

sub load {
  # Get parameters
  (($#_ == 1) or ($#_ == 2)) or
    die "Wrong number of parameters, stopped";
  
  my $invocant = shift;
  my $class = ref($invocant) || $invocant;
  
  my $src_path = shift;
  (not ref($src_path)) or die "Wrong parameter types, stopped";
  
  my $buf_size;
  if ($#_ == 2) {
    $buf_size = shift;
  } else {
    $buf_size = BUFFER_SIZE;
  }
  
  (looks_like_number($buf_size) and (int($buf_size) == $buf_size) and
    ($buf_size > 0)) or die "Invalid parameter, stopped";
  
  # Define the new object
  my $self = { };
  bless($self, $class);
  
  # The '_fh' property stores the file handle, or undef when the file is
  # closed
  open(my $fh, "< :raw", $src_path) or return undef;
  $self->{'_fh'} = $fh;
  
  # The '_state' property will be 1 for normal, 0 for EOF, -1 for IOERR;
  # IOERR state is used when file is closed
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

=head1 DESTRUCTOR

The destructor routine closes the file handle if it is not already
closed.

=cut

sub DESTROY {
  # Check parameter count
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  
  # Get self
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or
    die "Wrong parameter type, stopped";
  
  # If file handle has not been closed, close it
  if (defined $self->{'_fh'}) {
    close($self->{'_fh'}) or warn "Failed to close file handle";
    $self->{'_fh'} = undef;
  }
}

=head1 SPECIAL METHODS

=over 4

=item B<closeFile()>

Explicitly close the underlying file handle, if it is not already
closed.

If the file handle is not explicitly closed, it will be implicity closed
by the object destructor.

Once a source object is closed, any further attempt to use it will cause
a fatal error, except that calling C<closeFile()> again is harmless.

=cut

sub closeFile {
  # Check parameter count
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  
  # Get self
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or
    die "Wrong parameter type, stopped";
  
  # Only do something if hasn't been closed yet
  if (defined $self->{'_fh'}) {
    # Close the file handle
    close($self->{'_fh'}) or warn "Failed to close file handle";
    $self->{'_fh'} = undef;
    
    # Set rest of state to closed state
    $self->{'_state'} = -1;
    $self->{'_buf'} = '';
  }
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
  
  # Check that instance isn't closed
  (defined $self->{'_fh'}) or die "Source is closed, stopped";
  
  # If we are in a special state, return that
  if ($self->{'_state'} == 0) {
    return SNERR_EOF;
    
  } elsif ($self->{'_state'} < 0) {
    return SNERR_IOERR;
  }
  
  # If nothing is buffered, refill the buffer
  unless ($self->{'_take'} < $self->{'_fill'}) {
    # Attempt to read into the buffer
    my $rc = read $self->{'_fh'}, $self->{'_buf'}, $self->{'_bsize'};
    
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
  
  # Check that instance isn't closed
  (defined $self->{'_fh'}) or die "Source is closed, stopped";
  
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
  
  # Check that instance isn't closed
  (defined $self->{'_fh'}) or die "Source is closed, stopped";
  
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
  
  # Check that instance isn't closed
  (defined $self->{'_fh'}) or die "Source is closed, stopped";
  
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
      my $rc = read $self->{'_fh'}, $self->{'_buf'}, $self->{'_bsize'};
      
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
    
    # If some bytes were already taken from buffer, then drop them from
    # the start of the buffer
    if ($self->{'_take'} > 0) {
      $self->{'_buf'} = substr($self->{'_buf'}, $self->{'_take'});
      $self->{'_fill'} -= $self->{'_take'};
      $self->{'_take'} = 0;
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
  
  # Check that instance isn't closed
  (defined $self->{'_fh'}) or die "Source is closed, stopped";
  
  # This subclass supports multipass
  return 1;
}

=item B<rewind()>

This subclass I<does> support multipass, so this function is available.
Rewinding will clear any I/O error (unless the seek operation fails).

=cut

sub rewind {
  # Check parameter count
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  
  # Get self
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or
    die "Wrong parameter type, stopped";
  
  # Check that instance isn't closed
  (defined $self->{'_fh'}) or die "Source is closed, stopped";
  
  # Rewind, updating state appropriately
  if (seek($self->{'_fh'}, 0, 0)) {
    # Successful, so set state back to normal
    $self->{'_state'} = 1;
  } else {
    # Seek failed, so set I/O error
    $self->{'_state'} = -1;
  }
  
  # Reset rest of state back to initial values
  $self->{'_count'} = 0;
  $self->{'_fill' } = 0;
  $self->{'_take' } = 0;
  $self->{'_buf'  } = "";
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
