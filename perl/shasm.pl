#!/usr/bin/env perl
use v5.14;
use warnings;

# Shastina imports
use Shastina::Const qw(:CONSTANTS snerror_str);
use Shastina::InputSource;
use Shastina::Parser;

=head1 NAME

shasm.pl - Shastina parser test program.

=head1 SYNOPSIS

  ./shasm.pl < input.shastina

=head1 DESCRIPTION

Reads a Shastina file from standard input and writes the parsed entities
to standard output.

This allows you to see how the Perl Shastina parser understands the
input file.

=cut


# ==================
# Program entrypoint
# ==================

# Wrap standard input as an input source
#
my $src = Shastina::InputSource->load;
unless (defined $src) {
  die "Failed to wrap standard input, stopped";
}

# Build a Shastina parser around the input source
#
my $sr = Shastina::Parser->parse($src);

# Read through all the entities and report them
#
my $ent;
for($ent = $sr->readEntity; ref($ent); $ent = $sr->readEntity) {
  # First array element is the entity type
  my $etype = $ent->[0];
  
  # Get the line count in the source file and report it
  my $lcount = $sr->count;
  if (defined $lcount) {
    printf "%d: ", $lcount;
  } else {
    print "??: ";
  }
  
  # Handle the different entity types
  if ($etype == SNENTITY_STRING) {
    my $prefix = $ent->[1];
    my $stype  = $ent->[2];
    my $sdata  = $ent->[3];
    
    if ($stype == SNSTRING_QUOTED) {
      printf "String (%s) \"%s\"\n", $prefix, $sdata;
    } elsif ($stype == SNSTRING_CURLY) {
      printf "String (%s) {%s}\n", $prefix, $sdata;
    } else {
      die "Unexpected";
    }
  
  } elsif ($etype == SNENTITY_BEGIN_META) {
    print "Begin metacommand\n";
  
  } elsif ($etype == SNENTITY_END_META) {
    print "End metacommand\n";
  
  } elsif ($etype == SNENTITY_META_TOKEN) {
    my $token = $ent->[1];
    printf "Meta token %s\n", $token;
  
  } elsif ($etype == SNENTITY_META_STRING) {
    my $prefix = $ent->[1];
    my $stype  = $ent->[2];
    my $sdata  = $ent->[3];
    
    if ($stype == SNSTRING_QUOTED) {
      printf "Meta string (%s) \"%s\"\n", $prefix, $sdata;
    } elsif ($stype == SNSTRING_CURLY) {
      printf "Meta string (%s) {%s}\n", $prefix, $sdata;
    } else {
      die "Unexpected";
    }
  
  } elsif ($etype == SNENTITY_NUMERIC) {
    my $string_representation = $ent->[1];
    printf "Numeric %s\n", $string_representation;
  
  } elsif ($etype == SNENTITY_VARIABLE) {
    my $var_name = $ent->[1];
    printf "Declare variable %s\n", $var_name;
  
  } elsif ($etype == SNENTITY_CONSTANT) {
    my $const_name = $ent->[1];
    printf "Declare constant %s\n", $const_name;
  
  } elsif ($etype == SNENTITY_ASSIGN) {
    my $var_name = $ent->[1];
    printf "Assign variable %s\n", $var_name;
  
  } elsif ($etype == SNENTITY_GET) {
    my $var_or_const_name = $ent->[1];
    printf "Get value %s\n", $var_or_const_name;
  
  } elsif ($etype == SNENTITY_BEGIN_GROUP) {
    print "Begin group\n";
  
  } elsif ($etype == SNENTITY_END_GROUP) {
    print "End group\n";
  
  } elsif ($etype == SNENTITY_ARRAY) {
    my $element_count_integer = $ent->[1];
    printf "Array %d\n", $element_count_integer;
  
  } elsif ($etype == SNENTITY_OPERATION) {
    my $op_name = $ent->[1];
    printf "Operation %s\n", $op_name;
  
  } else {
    die "Unexpected";
  }
}

# Check for parsing error
#
unless ($ent == SNENTITY_EOF) {
  # Finished with error code
  die "Error: %s!\n", snerror_str($ent);
}

# Report End Of File if we got here
#
my $lcount = $sr->count;
if (defined $lcount) {
  printf "%d: End Of File\n", $lcount;
} else {
  print "??: End Of File\n";
}

# Attempt to consume rest of input
#
($src->consume) or die "Failed to consume remaining data!\n";

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
