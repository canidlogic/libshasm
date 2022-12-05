# Shastina Metainterpreter
The Shastina Metainterpreter (libshastina) is an implementation of the Shastina metalanguage, which allows for the rapid implementation of special-purpose programming languages.

## Manifest

The Shastina metalanguage is specified in `shastina_spec.md`.  See that document for further information about Shastina.

This library provides multiple implementations of the Shastina parser in different languages.  Each implementation is completely separate from all other implementations.  However, each implementation is designed to be functionally equivalent to each other implementation.

The `c` subdirectory contains a complete ANSI C implementation of the Shastina Metainterpreter.

The `perl` subdirectory contains a complete Perl implementation of the Shastina Metainterpreter.

## Releases

### Development

Added a complete implementation of the library in Perl, which is functionally equivalent to the C implementation.  `libshastina` now has implementations in multiple languages.

Fixed escape handling within string literals.  Double quotes and curly braces are now only escaped if they are preceded by an odd-numbered sequence of backslashes, rather than always being escaped if preceded by a backslash.  This is not a backwards-compatible change, but escaping is otherwise broken for the common case where two backslashes are used to escape a literal backslash.

### 0.9.3 (beta)

Added multipass support to the input source architecture.  This release is fully backwards compatible with 0.9.2.

### 0.9.2 (beta)

Added a new input source architecture.  Added requirement for UTF-8 input and added built-in Unicode support.  Dropped embedded data feature, which was never used in practice and had implementation difficulties.  Added a completed language specification.

This release is __not__ backwards-compatible with earlier betas.  Use beta 0.9.1 if you are compiling a program that is written against a previous Shastina beta that doesn't use the new input source architecture.

### 0.9.1 (beta)

Small update on the version 0.9.0 that is fully backwards compatible.  Fixes an issue that causes a fault when the library tries to read an empty input file, and also adds an error string function.  Otherwise the same as the previous version, and can be used as a drop-in replacement.

### 0.9.0 (beta)

Shastina has been rewritten from scratch with a new, simplified implementation.  The core Shastina metainterpreter is fully functional in this release, except for a few minor issues.

### 0.2.2 (alpha)

Finished the regular string reader framework by implementing the full decoder.  This is the last release in the original alpha line, which was discontinued in favor of a simplified implementation.  The string reader framework should eventually be spun off into a separate project -- it does not appear in the new, simplified implementation of Shastina.

### 0.2.1 (alpha)

Began the regular string reader framework, and completed the encoding component.  The decoding component is currently just a placeholder that sends a fixed sequence of entity codes to the encoding component -- this will be replaced with the full decoder in the next release.

### 0.2.0 (alpha)

Established the block reader module (shasm_block) with just a token reader for now, and developed a test program (test_block) for testing the block reader module.

### 0.1.0 (alpha)

Completed the input filter chain (shasm_input) and developed a test program (test_input) for testing the input filter chain.
