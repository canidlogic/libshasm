# Shastina Metainterpreter
## 1. Introduction
The Shastina Metainterpreter (libshastina) is an implementation of the Shastina metalanguage, which allows for the rapid implementation of special-purpose programming languages.

## 2. Known issues
### 2.1 Line numbers in embedded data
This issue only affects clients that make use of embedded data.  Embedded data will work, except there is currently no way for the client to inform Shastina how many line breaks were present in the embedded data, so the line numbers reported by Shastina may be wrong after embedded data.  Apart from the faulty line numbers after embedded data, everything else should work.

### 2.2 Missing documentation
Some comments in the source code still need to be written, but more importantly there needs to be a rewrite of the Shastina Specification.  The current specification draft (3V:C4-5; March 9, 2018, available separately) does not match the Shastina language as implemented by the current library.

## 3. Releases

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
