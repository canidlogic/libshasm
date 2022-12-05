# Shastina Perl library

This subdirectory contains the Perl implementation of the Shastina parser.  The Perl implementation is written entirely in Perl and designed to be functionally equivalent to the C library.

All of the Perl Shastina modules are in the `Shastina` subdirectory.  You will need to include this `Shastina` subdirectory somewhere in Perl's module search path.  (Include the parent directory of the `Shastina` subdirectory, not the `Shastina` subdirectory itself.)  You can run Perl with an `-I` option to add a directory to Perl's module search path.

Within this directory there is also the `shasm.pl` test script.  This behaves equivalently to the C version of this program.  It will read a Shastina file from standard input and report all the entities that were parsed by the Shastina library, using the Perl implementation.  In order to run the script successfully, the directory containing the `Shastina` subdirectory must be in Perl's module search path, as explained above.

The `pod` directory contains documentation files in Markdown format that were automatically generated from the source files.  The documentation for the `Shastina::Parser` class is what you want to look at first.  That will explain how to use the public interface of the library.

For the Shastina specification, see the main directory of `libshastina`.
