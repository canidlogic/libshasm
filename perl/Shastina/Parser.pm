package Shastina::Parser;
use v5.14;
use warnings;

# Core includes
use Scalar::Util qw(looks_like_number);

# Shastina modules
use Shastina::Const qw(:CONSTANTS :ERROR_CODES);
use Shastina::Filter;
use Shastina::Source;
use Shastina::Token;

=head1 NAME

Shastina::Parser - Shastina parser interface.

=head1 SYNOPSIS

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

=head1 DESCRIPTION

This module allows you to parse through all the entities in a Shastina
file.  This is the main public interface for the Shastina parsing
library.

You must wrap the input bytestream in a subclass of C<Shastina::Source>.
The following subclasses are built in:

  Shastina::FileSource    (reads from a file path)
  Shastina::InputSource   (reads from standard input)
  Shastina::BinarySource  (reads from a byte string)
  Shastina::UnicodeSource (reads from a Unicode string)

The synopsis gives examples of constructing each of these built-in
source types.  See the documentation of those subclass modules as well
as the abstract superclass C<Shastina::Source> for further information.
You may also define your own source class by subclassing the abstract
superclass C<Shastina::Source>.

Once you have an input source object, you can construct a Shastina
parser object around it with the C<parse()> constructor.  If you are
parsing the Shastina file in multiple passes, you will call C<rewind> on
the source before each pass and then each pass will need to use a newly
constructed C<Shastina::Parser> parser object.

The C<readEntity()> instance function allows you to read through the
entities in the Shastina file one by one.  See the synopsis for an
example, and see the documentation of the C<readEntity()> function for
further information.

The C<count()> instance function allows you at any time to return the
current line number in the underlying source file.  This is especially
useful for error reporting.

=cut

# =========
# Constants
# =========

# Maximum array element and group count.
#
use constant MAX_COUNT => 2147483647;

# The maximum height of the reader's array and group stacks.
#
# This affects how deeply arrays can be nested.  The group stack
# technically is one element taller than the array stack regularly, but
# we don't worry about that here.
#
# We impose this limit so that array limits are the same as in the C
# implementation.
#
use constant MAX_AG_STACK => 1024;

=head1 CONSTRUCTOR

=over 4

=item B<parse(src)>

Construct a new Shastina parser instance by around a given input source
object.

C<src> must be an instance of a subclass of C<Shastina::Source>.  The
C<Shastina::Source> class is an abstract superclass that can't be
directly used.  Either use one of its built-in subclasses (as explained
in the module description above), or subclass it yourself.

The parser instance will start reading the input source at its current
location without attempting to rewind it.  The parser will not attempt
to close file sources at any time.

If you want to make multiple passes through a Shastina input file, make
sure you are using an input source that supports multipass operation.
Then, before each pass, call the C<rewind()> function on the input
source object and construct a new Shastina parser instance by calling
this C<parse()> constructor.  Each pass will use the same input source
object instance, but each pass requires a new parser object instance.

While a parser object is in use, nothing else should read from or modify
the input source object in any way.  Internal buffering is used, and
undefined behavior occurs if this buffering is disrupted by something
else reading from the input source object.  Once you are finished with
the parser object, the input source object can once again be used
however you wish.

=cut

sub parse {
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
  
  # The '_state' property is one if OK, zero if final entity has been
  # returned, negative if an error has been returned
  $self->{'_state'} = 1;
  
  # The '_fil' property stores the filter that is wrapped around the
  # input source; this is used for getting the line number
  $self->{'_fil'} = Shastina::Filter->wrap($src);
  
  # The '_tok' property stores the token parser wrapped around the
  # filter object
  $self->{'_tok'} = Shastina::Token->wrap($self->{'_fil'});
  
  # The '_meta' flag is set when we enter metacommand mode
  $self->{'_meta'} = 0;
  
  # The '_array' flag is set if a new array was just opened; this is
  # needed because empty arrays can't open implicit groups, but we don't
  # know immediately whether a new array is empty
  $self->{'_array'} = 0;
  
  # The '_astack' property is a reference to an array that stores the
  # array stack; if any arrays are opened, the value on top of the stack
  # counts the number of elements in the innermost opened array; the
  # limit of count values is MAX_COUNT and the maximum number of
  # elements in this stack is MAX_AG_STACK
  $self->{'_astack'} = [];
  
  # The '_gstack' property is a reference to an array; the value on top
  # of the stack counts the number of open groups; array elements
  # implicitly open special groups that can only be closed at the end of
  # an array element; hence the need for a stack to track open groups;
  # the height of this stack is regularly one more than the height of
  # the array stack; implicit groups created by array elements are NOT
  # counted on this stack
  $self->{'_gstack'} = [ 0 ];
  
  # The '_queue' property stores entities that have been queued for
  # return; Shastina tokens may generate more than one entity, hence the
  # need for a queue; all entities are array references, except the EOF
  # entity is just stored as a scalar
  $self->{'_queue'} = [];
  
  # Return the new object
  return $self;
}

=back

=cut

# ========================
# Private instance methods
# ========================

# _arrayPrefix()
# --------------
#
# Perform the array prefix operation.
#
# This should be performed before processing any token except for "]".
# The reader object may not be in final or error state.
#
# This operation only does something if the _array flag is on.  In that
# case, the _array flag is turned off, the array and grouping stacks are
# adjusted to indicate a new array, and a BEGIN_GROUP entity is added to
# the reader.
#
# These are operations that should have properly been done when the "["
# token was processed.  But that token needs to know whether the array 
# is empty (followed immediately by a "]" token) or non-empty.  Hence,
# it sets the array flag and its processing is delayed until the next
# token, which calls into this array prefix operation to do the delayed
# array operation if the array flag is set.
#
# This is therefore done before every token except for "]", which
# handles the situation differently because the empty array case is
# different.  Hence, this function must *not* be called before the "]"
# token.
#
# This function returns 1 if successful or 0 if error.  If error, then
# _state will be set to the appropriate error code. 
#
sub _arrayPrefix {
  # Check parameter count
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  
  # Get self
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or
    die "Wrong parameter type, stopped";
  
  # Check state
  ($self->{'_state'} > 0) or die "Invalid state, stopped";
  
  # Only do something if _array flag is set
  if ($self->{'_array'}) {
    # Clear the _array flag
    $self->{'_array'} = 0;
    
    # Check that we have space on the group stack (if we do, then we
    # will also have space on the array stack since that is always one
    # less high than the group stack)
    unless (scalar(@{$self->{'_gstack'}}) < MAX_AG_STACK) {
      $self->{'_state'} = SNERR_DEEPARRAY;
      return 0;
    }
    
    # Push a value of one on top of the array stack and a value of zero
    # on top of the grouping stack to begin a new array
    push @{$self->{'_astack'}}, (1);
    push @{$self->{'_gstack'}}, (0);
    
    # Add a BEGIN_GROUP entity to the queue
    push @{$self->{'_queue'}}, ([SNENTITY_BEGIN_GROUP]);
  }
  
  # If we got here, we were successful
  return 1;
}

# _fill()
# -------
#
# Read a token from a Shastina source file in an effort to fill the
# entity queue.
# 
# The reader must not be in an EOF or error state and the queue must be
# empty.  If these conditions are not satisfied, a fatal error occurs.
#
# Not all tokens result in entities being read, so to ensure the entity
# buffer is filled with at least one entity, call this function in a
# loop.
#
# Returns 1 if successful AND queue no longer empty, zero if successful
# but queue still empty, or -1 if an error.  If an error, then _state
# will hold the error code.
#
sub _fill {
  # Check parameter count
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  
  # Get self
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or
    die "Wrong parameter type, stopped";
  
  # Check state
  ($self->{'_state'} > 0) or die "Invalid state, stopped";
  (scalar(@{$self->{'_queue'}}) < 1) or die "Invalid state, stopped";
  
  # Read a token
  my $tk = $self->{'_tok'}->readToken;
  unless (ref($tk)) {
    if ($tk == 0) {
      # Final token -- check not in meta mode
      if ($self->{'_meta'}) {
        $self->{'_state'} = SNERR_OPENMETA;
        return -1;
      }
      
      # Check no open array remains
      if ($self->{'_array'} or (scalar(@{$self->{'_astack'}}) > 0)) {
        $self->{'_state'} = SNERR_OPENARRAY;
        return -1;
      }
      
      # If we got here, group stack should be height one; make sure the
      # value on top is zero so there are no open groups
      (scalar(@{$self->{'_gstack'}} == 1)) or die "Unexpected";
      unless ($self->{'_gstack'}->[0] == 0) {
        $self->{'_state'} = SNERR_OPENGROUP;
        return -1;
      }
      
      # If we got here, everything OK so push the EOF entity directly as
      # a scalar onto the queue and return one indicating queue no
      # longer empty
      push @{$self->{'_queue'}}, (SNENTITY_EOF);
      return 1;
      
    } elsif ($tk < 0) {
      # Error happened while reading token
      $self->{'_state'} = $tk;
      return -1;
    
    } else {
      die "Unexpected";
    }
  }
  
  # If we got here, then we successfully read a token that is not the
  # final token; perform array prefix operation if not in metacommand
  # mode, except for "]" token
  unless ($self->{'_meta'}) {
    if (scalar(@$tk) == 1) {
      unless ($tk->[0] eq ']') {
        # Not in metacommand mode and a simple token that is not ], so
        # perform array prefix
        ($self->_arrayPrefix) or return -1;
      }
      
    } else {
      # Not in metacommand mode and not a simple token, so perform array
      # prefix
      ($self->_arrayPrefix) or return -1;
    }
  }
  
  # Handle the token types
  if (scalar(@$tk) == 1) {
    # Simple token cases, so unpack token
    $tk = $tk->[0];
    
    # Handle simple token cases
    if ($tk eq '%') { # ================================================
      # % token, enter metacommand mode
      if ($self->{'_meta'}) {
        $self->{'_state'} = SNERR_METANEST;
        return -1;
      }
      
      $self->{'_meta'} = 1;
      push @{$self->{'_queue'}}, ([SNENTITY_BEGIN_META]);
      return 1;
      
    } elsif ($tk eq ';') { # ===========================================
      # ; token, leave metacommand mode
      unless ($self->{'_meta'}) {
        $self->{'_state'} = SNERR_SEMICOLON;
        return -1;
      }
      
      $self->{'_meta'} = 0;
      push @{$self->{'_queue'}}, ([SNENTITY_END_META]);
      return 1;
      
    } elsif ($self->{'_meta'}) { # =====================================
      # Other simple tokens in metacommand mode
      push @{$self->{'_queue'}}, ([SNENTITY_META_TOKEN, $tk]);
      return 1;
      
    } elsif ($tk =~ /\A[\+\-0-9]/) { # =================================
      # Numeric token
      push @{$self->{'_queue'}}, ([SNENTITY_NUMERIC, $tk]);
      return 1;
      
    } elsif ($tk =~ /\A\?(.*)\z/) { # ==================================
      # Declare variable
      my $var_name = $1;
      unless (defined $var_name) {
        $var_name = '';
      }
      
      push @{$self->{'_queue'}}, ([SNENTITY_VARIABLE, $var_name]);
      return 1;
      
    } elsif ($tk =~ /\A@(.*)\z/) { # ===================================
      # Declare constant
      my $var_name = $1;
      unless (defined $var_name) {
        $var_name = '';
      }
      
      push @{$self->{'_queue'}}, ([SNENTITY_CONSTANT, $var_name]);
      return 1;
      
    } elsif ($tk =~ /\A:(.*)\z/) { # ===================================
      # Assign variable
      my $var_name = $1;
      unless (defined $var_name) {
        $var_name = '';
      }
      
      push @{$self->{'_queue'}}, ([SNENTITY_ASSIGN, $var_name]);
      return 1;
      
    } elsif ($tk =~ /\A=(.*)\z/) { # ===================================
      # Get variable or constant value
      my $var_name = $1;
      unless (defined $var_name) {
        $var_name = '';
      }
      
      push @{$self->{'_queue'}}, ([SNENTITY_GET, $var_name]);
      return 1;
      
    } elsif ($tk eq '(') { # ===========================================
      # Begin group
      unless ($self->{'_gstack'}->[-1] < MAX_COUNT) {
        $self->{'_state'} = SNERR_DEEPGROUP;
        return -1;
      }
      
      $self->{'_gstack'}->[-1]++;
      push @{$self->{'_queue'}}, ([SNENTITY_BEGIN_GROUP]);
      return 1;
      
    } elsif ($tk eq ')') { # ===========================================
      # End group
      unless ($self->{'_gstack'}->[-1] > 0) {
        $self->{'_state'} = SNERR_RPAREN;
        return -1;
      }
      
      $self->{'_gstack'}->[-1]--;
      push @{$self->{'_queue'}}, ([SNENTITY_END_GROUP]);
      return 1;
      
    } elsif ($tk eq '[') { # ===========================================
      # Begin array -- just set the array flag here and don't add any
      # entities because we don't know yet whether the array is empty or
      # not
      $self->{'_array'} = 1;
      return 0;
      
    } elsif ($tk eq ']') { # ===========================================
      # End array
      if ($self->{'_array'}) {
        # Empty array
        $self->{'_array'} = 0;
        push @{$self->{'_queue'}}, ([SNENTITY_ARRAY, 0]);
        return 1;
      }
      
      unless (scalar(@{$self->{'_astack'}}) > 0) {
        # Unpaired ] token
        $self->{'_state'} = SNERR_RSQR;
        return -1;
      }
      
      unless ($self->{'_gstack'}->[-1] == 0) {
        # Still open groups in stack element
        $self->{'_state'} = SNERR_OPENGROUP;
        return -1;
      }
      
      my $array_count = pop @{$self->{'_astack'}};
      pop @{$self->{'_gstack'}};
      
      push @{$self->{'_queue'}}, ([SNENTITY_END_GROUP]);
      push @{$self->{'_queue'}}, ([SNENTITY_ARRAY, $array_count]);
      return 1;
      
    } elsif ($tk eq ',') { # ===========================================
      # Array separator
      unless (scalar(@{$self->{'_astack'}}) > 0) {
        # Comma used outside of array
        $self->{'_state'} = SNERR_COMMA;
        return -1;
      }
      
      unless ($self->{'_gstack'}->[-1] == 0) {
        # Still open groups in stack element
        $self->{'_state'} = SNERR_OPENGROUP;
        return -1;
      }
      
      unless ($self->{'_astack'}->[-1] < MAX_COUNT) {
        # Too many array elements
        $self->{'_state'} = SNERR_LONGARRAY;
        return -1;
      }
      
      $self->{'_astack'}->[-1]++;
      push @{$self->{'_queue'}}, ([SNENTITY_END_GROUP]);
      push @{$self->{'_queue'}}, ([SNENTITY_BEGIN_GROUP]);
      return 1;
      
    } else { # =========================================================
      # Operator
      push @{$self->{'_queue'}}, ([SNENTITY_OPERATION, $tk]);
      return 1;
    }
  }
  
  # If we got here, then we should have a string token # ===============
  (scalar(@$tk) == 3) or die "Unexpected";
  
  # Either a normal string or a meta string
  if ($self->{'_meta'}) {
    # Meta string
    push @{$self->{'_queue'}}, ([
      SNENTITY_META_STRING,
      $tk->[0],
      $tk->[1],
      $tk->[2]
    ]);
    
  } else {
    # Regular string
    push @{$self->{'_queue'}}, ([
      SNENTITY_STRING,
      $tk->[0],
      $tk->[1],
      $tk->[2]
    ]);
  }
  
  # Both string entity types added something to the queue
  return 1;
}

=head1 INSTANCE METHODS

=over 4

=item B<readEntity()>

Parse the next Shastina entity.

The return value is either a reference to an array or a scalar integer
value.

To get the entity constants you must include C<Shastina::Const> with the
C<:CONSTANTS> mode.  To handle specific error codes, you must also
include the C<:ERROR_CODES> mode.  To get error messages from the error
codes use the C<snerror_str()> function exported by C<Shastina::Const>.

If the C<|;> final entity was read, a scalar integer C<SNENTITY_EOF>
will be returned on this call and all subsequent calls to this function.

If an error was encountered, one of the error code constant values
defined in C<Shastina::Const> will be returned.  All such codes will be
integers less than zero.  Once an error is returned, the same error will
be returned on all subsequent calls.

If an entity besides the C<|;> final entity was read, then this function
will return an array reference.  The referenced array always has at
least one element, and the first element is always an integer that
corresponds to one of the C<SNENTITY> constants (except C<SNENTITY_EOF>
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

For the I<primitive> format, the entity array only has a single element,
which is the C<SNENTITY> constant.

For the C<token> format, the entity array has two elements.  The first
is the C<SNENTITY> constant and the second is the token as a string.
For C<SNENTITY_VARIABLE>, C<SNENTITY_CONSTANT>, C<SNENTITY_ASSIGN>, and
C<SNENTITY_GET>, the token string does I<not> include the first
character of the Shastina token.  For all other token formats, the token
is exactly same as the whole Shastina token in the source file.

Note that the C<SNENTITY_NUMERIC> type is in the token format and
therefore provides its data as a string.  This allows clients to
determine their own number formats and how they will parse them.

For the C<integer> format, the entity array has two elements.  The first
is the C<SNENTITY> constant and the second is the number of elements in
the array as an integer that is zero or greater.

For the C<string> format, the entity array has four elements.  The first
is the C<SNENTITY> constant.  The second is the string prefix, I<not>
including the opening quote or left curly.  The third is one of the
C<SNSTRING> constants from C<Shastina::Const> that determines whether
this is a quoted or curlied string.  The fourth is the string data, not
including the enclosing quotes or curly braces.  The string data is in
Unicode string format.

=cut

sub readEntity {
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
  
  # If queue is empty, fill it
  unless (scalar(@{$self->{'_queue'}}) > 0) {
    my $retval;
    for(
      $retval = $self->_fill;
      $retval == 0;
      $retval = $self->_fill) { }
    unless ($retval > 0) {
      return $self->{'_state'};
    }
  }
  
  # Get the first entity from the start of the queue
  my $ent = shift @{$self->{'_queue'}};
  
  # If the entity we just read is not a reference, then set it as our
  # new state so all subsequent calls return it; it must be SNENTITY_EOF
  # in this case
  unless (ref($ent)) {
    ($ent == SNENTITY_EOF) or die "Unexpected";
    $self->{'_state'} = 0;
  }
  
  # Return the entity we read
  return $ent;
}

=item B<count()>

Return the current line count.  This is an integer greater than zero, or
C<undef> in the unlikely event that the line count overflows.  (This
only happens if there are trillions of lines.)

=cut

sub count {
  # Check parameter count
  ($#_ == 0) or die "Wrong number of parameters, stopped";
  
  # Get self
  my $self = shift;
  (ref($self) and $self->isa(__PACKAGE__)) or
    die "Wrong parameter type, stopped";
  
  # Call through to the filter
  return $self->{'_fil'}->count;
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
