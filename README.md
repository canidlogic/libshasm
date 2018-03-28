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

### 2.2 Maximum length and register limit changes

Draft 3V:C4-5 of the Shasinta Specification has a number of limits depending on an unsigned 16-bit maximum of 65,535.  It turns out that it is much more portable and easier to implement libshasm if these limits are reduced a bit.  This section documents all the specific limit changes that libshasm makes, diverging from draft 3V:C4-5 of the specification.

(1) In sections 5.3, 5.4, 5.5, and 6.3, change the 65,535-byte string and block limit to 32,766 bytes.  This is two bytes shy of 32 kilobytes, which allows the maximum length including a terminating null byte to be stored in a signed 16-bit value.

(2) In section 5.5, change the example calculations to take into account the new 32,766-byte limit.

(3) In section 4, add a maximum token length limit of 32,766 bytes.

(4) In section 5, note that the maximum length of the final string literal is 32,766 bytes.

(5) In section 5.1, note that the length limit of 32,766 bytes on the final string literal does not apply to the size of the string data on input.  All that matters is the size of the final output.

(6) In section 5.2, document the maximum string literal length limit of 32,766 bytes.

(7) In section 6.6, change the maximum register index to 32,767 so it can be stored in a signed 16-bit integer.

(8) In section 6.7, change the maximum size of all metacommand parameter data including terminating nulls to 32,767 bytes.

### 2.3 Removal of implementation details

In section 5.1 of draft 3V:C4-5 of the Shastina Specification, the decoding map is specified to be provided to Shastina as an rtrie structure.  However, libshasm is more flexible in interface, allowing the client to use any data structure to store the decoding map.

Similarly, section 5.2 of draft 3V:C4-5 of the Shastina Specification specifies details of how the encoding table is implemented, such as that records should be in ascending order.  libshasm is once again more flexible in interface, allowing the client to use any data structure to store the encoding map.

This divergence, therefore, is that libshasm isn't limited to implementing the decoding and encoding tables with the structures given in the specification.  The specification should be updated to remove these implementation-specific requirements.

### 2.4 Removal of disallow null flag

In sections 5.2, 5.3, 5.4, 5.5, and 6.7 of draft 3V:C4-5 of the Shastina Specification, there are references to a "disallow null" flag, which prevents result strings from including null bytes.  This flag is handled as an encoding parameter in the specification.

In libshasm, on the other hand, there is no disallow null flag.  Instead, the block reader buffer keeps track of whether a null byte has been written into it as data and therefore whether or not it is safe to null-terminate the string.

The specification should be updated to remove the disallow null flag as an encoding parameter in sections 5.2-5.5, and in section 6.7 to replace the use of disallow null flag encoding parameter with a check after the string reading step to see whether the final string data includes null bytes.

### 2.5 Strict output override mode

libshasm adds a strict flag to be used in conjunction with the output overrides described in section 5.2 of draft 3V:C4-5 of the Shastina Specification.

In the current specification, output overrides always apply to the full Unicode codepoint range of 0x0 through 0x10ffff.  libshasm behaves like the specification if the strict flag is off.  If the strict flag is on, then output overrides only apply to the full Unicode codepoint range excluding the surrogate range, which is handled by the encoding table.

## 3. Roadmap
The current development roadmap is as follows.  Section references are to the Shastina language specification, currently on draft 3V:C4-5.

- [x] Input filtering chain (section 3)
- [x] Tokenization function (section 4)
- [ ] Regular string encoding (section 5.2)
- [ ] Regular string decoding (section 5.1)
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
The current goal is the third stage of the roadmap, which is to complete the regular string encoding functionality of the block reader.

This roadmap stage makes use of the block reader architecture and testing module established in the previous roadmap stage.  This stage is closely linked with the next one, which together add a regular string data reader to the block reader module.  Since regular string data interpretation is rather complex, adding the regular string data reader is split into two separate roadmap stages.

In this first stage of adding regular string data reading, the focus is on establishing the string encoding component, which converts decoded entity codes into the output bytes that are placed in the result string.  The next roadmap stage will then complete regular string data reading functionality by adding the string decoding component, which converts filtered input bytes into entity codes.  In short, a bottom-up model will be used to complete the regular string data reading functionality, where the module closest to output is completed first, and then the module closest to input is added on top of it.

In order to be able to test the string encoding functionality, this roadmap stage will define the interface of the full string data reader function and add a placeholder for the decoding stage that ignores input and instead just sends a hardwired sequence of entity codes to the string encoding component.  This will allow the testing module to test the string encoding component as if the string decoding component already existed -- in actuality, program input will be ignored and the placeholder decoder will always provide the encoder with a fixed testing sequence of entity codes.

The specific goals of this roadmap stage are therefore to extend the block reader interface with a regular string data reading function; to add a "string" mode to the test_block program, which reads regular string data from input and reports the result to standard output; to implement the string encoding component; and to define a placeholder string decoding component.

In the next stage, all that must be done is to replace the string encoding component placeholder with an actual implementation.  Then regular string data reading will have been successfully added to the block reader.

Once this roadmap stage has been completed, an alpha 0.2.1 release will be made, keeping with the schedule of handling each roadmap stage that builds out the functionality of the block reader as a separate patch release of the 0.2.x series.

### 4.1 Worklist
To reach the current goal, the following steps will be taken, in the order shown below:

- [x] Define the regular string reading interface
- [x] Extend the block testing program with a string mode
- [x] Define a placeholder string decoder function
- [x] Define the string encoder, except for output overrides
- [x] Add UTF-8 and CESU-8 output overrides
- [x] Add UTF-16 little and big endian overrides
- [ ] Add UTF-32 little and big endian overrides

The string mode for the testing program will read a string from standard input and report the result string to standard output, along with the rest of the input that follows the string data.  The result string is reported with escape sequences standing in for bytes outside of ASCII printing range.  Command-line parameters allow the particular output override mode to be selected, or a non-override mode using a hardwired test encoding table.  Input override mode will always be selected during testing.  A hardwired test decoding map will be used.

These steps will be performed in separate branches, with results merged back into master when complete.  At the end of this process, the block reading architecture will be established and the token reader will be done.

## 5. Releases

### 0.2.0 (alpha)

Established the block reader module (shasm_block) with just a token reader for now, and developed a test program (test_block) for testing the block reader module.

### 0.1.0 (alpha)

Completed the input filter chain (shasm_input) and developed a test program (test_input) for testing the input filter chain.
