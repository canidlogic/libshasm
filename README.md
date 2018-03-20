# libshasm
## 1. Introduction
libshasm (pronounced "lib sha-ZAM") is a library that implements a Shastina language metainterpreter.

For a specification of the Shastina language, see the [Canidtech website](https://www.canidtech.com/).  See the "Divergences" section below in this readme for differences between Shastina as described in the specification and Shastina as implemented in libshasm.

As of March 19, 2018, the input filter chain and part of the block reader have been completed.  This readme describes the current state of the project.

Development work on this library is being handled by Noah Johnson (canidlogic on GitHub).  He can be reached by email at noah.johnson@loupmail.com

## 2. Divergences

The current version of the Shastina Specification (draft 3V:C4-5) is not the final draft and should be considered a work in progress.  This section of the readme describes the ways that the Shastina language as described in draft 3V:C4-5 differs from the Shastina language as implemented in libshasm.

Once libshasm has reached a functional state, a revision of the Shastina Specification will be made that incorporates the changes described in this section of the readme, thereby bringing the specification back into alignment with libshasm.

### 2.1 Unghosting filters

Section 3 of draft 3V:C4-5 of the Shastina Specification describes an "unghosting" filter that drops whitespace sequences consisting of ASCII Space (SP) and ASCII Horizontal Tab (HT) that occur immediately before an ASCII Line Feed (LF).  This has the effect of removing invisible, trailing whitespace from each line (which could interfere with escape processing in string data) but leaving whitespace intact everywhere else.

Draft 3V:C4-5 recommends implementing a whitespace buffer consisting of (x, y) pairs describing how many space characters are followed by how many horizontal tab characters, so that long sequences of whitespace can be compressed.  On further thought, though, this is rather complex, it has the potential for buffer overflow problems, and it also allows for ghost space characters to be hidden within sequences of horizontal tabs.

libshasm therefore is removing the unghosting filter described in the specification (which hasn't been implemented yet) and replacing it with a tab unghosting filter followed by a line unghosting filter.

The tab unghosting filter discards sequences of one or more SP characters that occur immediately before an HT.  This can be implemented with just a count of SP characters and a one-character buffer.  After this operation, all sequences of whitespace will consist of zero or more HT characters followed by zero or more SP characters.

(The tab unghosting filter may change the alignment of text if haphazard mixtures of HT and SP characters were used for alignment.  To avoid the tab unghosting filter changing the appearance of a text document, avoid using SP characters before HT characters, or convert all HT characters to sequences of SP characters before processing.)

The line unghosting filter discards whitespace sequences consisting of SP and HT characters that occur immediately before an LF.  Since the tab unghosting filter has removed all SP characters that occur before HT, the line unghosting filter can be implemented just by counting the number of SP and HT characters, plus a one-character buffer.

With the caveat that the tab unghosting filter may change the alignment of text in certain circumstances, the tab and line unghosting filter sequence should have the same effect as the original unghosting filter, while being much easier to implement.

### 2.2 Maximum block and string length

Section 5.5 and a few other sections of draft 3V:C4-5 of the Shastina Specification establish a limit of 65,535 bytes as the maximum length of decoded string literals.  libshasm works according to this specification, except when the size_t is less than 32-bit or the implementation constant SHASM_BLOCK_MAXBUFFER has been adjusted in the block reader implementation.  In these cases, there is an implementation limit on strings that is lower than 65,535 bytes.

If size_t is less than 32-bit but SHASM_BLOCK_MAXBUFFER has not been adjusted, then this lower implementation limit on string length is 65,534 bytes, which is just one byte shy of the specification.  However, if SHASM_BLOCK_MAXBUFFER is adjusted down, this limit may be significantly lower.  libshasm distinguishes between the specification limit and the implementation limit by raising SHASM_ERR_HUGEBLOCK if the specification limit has been exceeded or SHASM_ERR_LARGEBLOCK if the implementation-specific lower limit has been exceeded -- see the documentation of those errors for further information.

The divergence, therefore, is that implementations of Shastina do not necessarily support the full 65,535-byte string length.  An updated draft of the specification should set a minimum implementation limit and say that the actual limit is somewhere between this minimum implementation limit and 65,535.  The documentation within the libshasm sources can then be clarified.

## 3. Roadmap
The current development roadmap is as follows.  Section references are to the Shastina language specification, currently on draft 3V:C4-5.

- [x] Input filtering chain (section 3)
- [x] Tokenization function (section 4)
- [ ] Normal string encoding (section 5.2)
- [ ] Normal string decoding (section 5.1)
- [ ] Base-16 special mode (section 5.3)
- [ ] Base-85 special mode (section 5.4)
- [ ] Numeric literals (section 6.2)
- [ ] String literals (section 6.3)
- [ ] Grouping (section 6.4)
- [ ] Lists (section 6.5)
- [ ] Memory variables (section 6.6)
- [ ] Bring everything together to finish

Tasks will be completed in the order shown above.  This roadmap may be revised along the way.

## 4. Current goal
The current goal is the third stage of the roadmap, which is to complete the normal string encoding functionality of the block reader.

This roadmap stage makes use of the block reader architecture and testing module established in the previous roadmap stage.  This stage is closely linked with the next one, which together add a normal string data reader to the block reader module.  Since normal string data interpretation is rather complex, adding the normal string data reader is split into two separate roadmap stages.

In this first stage of adding normal string data reading, the focus is on establishing the string encoding component, which converts decoded entity codes into the output bytes that are placed in the result string.  The next roadmap stage will then complete normal string data reading functionality by adding the string decoding component, which converts filtered input bytes into entity codes.  In short, a bottom-up model will be used to complete the normal string data reading functionality, where the module closest to output is completed first, and then the module closest to input is added on top of it.

In order to be able to test the string encoding functionality, this roadmap stage will define the interface of the full string data reader function and add a placeholder for the decoding stage that ignores input and instead just sends a hardwired sequence of entity codes to the string encoding component.  This will allow the testing module to test the string encoding component as if the string decoding component already existed -- in actuality, program input will be ignored and the placeholder decoder will always provide the encoder with a fixed testing sequence of entity codes.

The specific goals of this roadmap stage are therefore to extend the block reader interface with a normal string data reading function; to add a "string" mode to the test_block program, which reads normal string data from input and reports the result to standard output; to implement the string encoding component; and to define a placeholder string decoding component.

In the next stage, all that must be done is to replace the string encoding component placeholder with an actual implementation.  Then normal string data reading will have been successfully added to the block reader.

Once this roadmap stage has been completed, an alpha 0.2.1 release will be made, keeping with the schedule of handling each roadmap stage that builds out the functionality of the block reader as a separate patch release of the 0.2.x series.

### 4.1 Worklist
To reach the current goal, the following steps will be taken, in the order shown below:

- [ ] Define the normal string reading interface
- [ ] Extend the block testing program with a string mode
- [ ] Define a placeholder string decoder function
- [ ] Define the string encoder, except for output overrides
- [ ] Add UTF-8 and CESU-8 output overrides
- [ ] Add UTF-16 little and big endian overrides
- [ ] Add UTF-32 little and big endian overrides

The string mode for the testing program will read a string from standard input and report the result string to standard output, along with the rest of the input that follows the string data.  The result string is reported with escape sequences standing in for bytes outside of ASCII printing range.  Command-line parameters allow the particular output override mode to be selected, or a non-override mode using a hardwired test encoding table.  Input override mode will always be selected during testing.  A hardwired test decoding map will be used.

These steps will be performed in separate branches, with results merged back into master when complete.  At the end of this process, the block reading architecture will be established and the token reader will be done.

## 5. Releases

### 0.2.0 (alpha)

Established the block reader module (shasm_block) with just a token reader for now, and developed a test program (test_block) for testing the block reader module.

### 0.1.0 (alpha)

Completed the input filter chain (shasm_input) and developed a test program (test_input) for testing the input filter chain.
