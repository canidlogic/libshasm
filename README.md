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

### 2.6 Regular string decoding algorithm

In section 5.1 of draft 3V:C4-5 of the Shastina Specification, the method for regular string decoding is described.  It's a bit difficult from the description to determine how to implement the decoding algorithm.  This section provides a clarified description of the regular string decoding algorithm.  This is not a true divergence since it doesn't change the specified behavior, but the specification should be updated with this clarified explanation to make the system clearer.

The following subsections describe the clarified regular string decoding algorithm.

#### 2.6.1 Decoder architecture

The purpose of the regular string decoder is convert a filtered sequence of input bytes received from the input filter stack into a sequence of entity codes which can then be passed to the string encoder.

In order to implement the greedy matching required for using the decoding map, a more sophisticated buffer than the single-character pushback buffer offered by the input filter stack will be required.  However, to limit the complexities of dealing with two levels of nonlinear buffers, the more sophisticated buffer will be confined to one part of the decoder, and the rest of the decoder will simply use the pushback buffer.  The part of the decoder that uses the sophisticated buffer is the "inner" decoder, while the rest of the decoder that only requires the pushback buffer is the "outer" decoder.

The inner decoder will decode zero or more entity codes from the input stream, passing these through to the encoder function.  The inner decoder will also handle numeric escapes.  The inner decoder stops when it encounters something it can't decode.  The inner decoder is successful if it manages to detach its buffer (described later) such that the outer decoder can make another attempt at decoding what the inner decoder stopped at by reading directly from the input filter stack.  A decoding error occurs in any other case -- if the inner decoder's buffer can't be detached, it means that there is data that the outer decoder wouldn't be able to interpret, either.  (This is guaranteed by the kinds of decoding keys that the inner decoding loop will ignore, described later.)

The outer decoder has a loop that begins by calling the inner decoder.  If the inner decoder returns successfully, the outer decoder will then attempt to interpret the data the inner decoder stopped at (reading direct from the input filter stack) using the built-in keys.  The built-in keys include the terminal characters (which cause the decoding loop to end successfully), and, if an input override is selected, sequences of bytes with their most significant bit set -- which are decoded according to the override and passed through to the encoding function.  If the loop hasn't terminated on error or on account of a terminal built-in key, it then loops back and calls the inner decoder again.

#### 2.6.2 Inner decoder buffer

The buffer used by the inner decoder is based on a circular queue that grows dynamically in size.  The basic operations are to add a byte to the end of the queue (growing the queue if necessary), to remove one or more bytes from the front of the queue, and to read a byte from the start of the queue (if available).

The buffer uses the dynamic circular queue within it to store the buffered data.  It is wrapped around the input filter chain.  Normally it just passes data through without buffering it.  However, it supports a "mark" and a "restore" function.  The mark function remembers the current location in input and starts buffering data.  The restore function uses the buffer to simulate a return to the most recently marked position and clears the mark.  (Clearing a mark can be done by marking the current position and immediately restoring to the current position.)

Finally, there is a "detach" function.  If there are no buffered bytes and no active marks, the detach function succeeds without doing anything.  If there is one buffered byte and no active marks, the detach function succeeds after clearing the buffer and using the pushback filter to push the byte back.  If there is more than one buffered byte or an active mark, the detach function fails.  A successful detach function causes the buffer to be completely emptied, so that the inner decoder can return to the outer decoder and the outer decoder can pick up the input using just the pushback buffer from the input filter chain.

#### 2.6.3 Decoding map overlay

In order to ensure the decoding operation proceeds smoothly, certain keys in the decoding map must be ignored if they occur.  The decoding map overlay is a wrapper around the decoding map that the client provides, which makes sure the necessary keys are ignored.  In addition, the overlay provides a function which returns whether a marker character was just read, which means the client should not read any
bytes further and that there are no further branches from the current node.

The wrapped reset function of the overlay resets the underlying decoding map to root position and resets it own state.

...

## 3. Roadmap
The current development roadmap is as follows.  Section references are to the Shastina language specification, currently on draft 3V:C4-5.

- [x] Input filtering chain (section 3)
- [x] Tokenization function (section 4)
- [x] Regular string encoding (section 5.2)
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
The current goal is the fourth stage of the roadmap, which is to complete the regular string decoding functionality of the block reader.  Together with the previous roadmap stage, this will complete the regular string reader of the shasm_block module.

The interface and testing module for the string decoder were already established in the previous roadmap stage, with a placeholder for the string decoder function that simply sends a fixed sequence of entity codes to the encoder.  The only objective of the current roadmap stage is therefore to replace the placeholder string decoder function with a full regular string decoder implementation.

However, regular string decoding is rather complex, so this roadmap stage will likely turn out to be the most difficult of the project.  Once this roadmap stage has been completed, an alpha 0.2.2 release will be made, keeping with the schedule of handling each roadmap stage that builds out the functionality of the block reader as a separate patch release of the 0.2.x series.

See the "Regular string decoding algorithm" divergence earlier in this readme for a clarification of how the regular string decoding system works.

...

### 4.1 Worklist
To reach the current goal, the following steps will be taken, in the order shown below:

- [x] Define the regular string reading interface
- [x] Extend the block testing program with a string mode
- [x] Define a placeholder string decoder function
- [x] Define the string encoder, except for output overrides
- [x] Add UTF-8 and CESU-8 output overrides
- [x] Add UTF-16 little and big endian overrides
- [x] Add UTF-32 little and big endian overrides

The string mode for the testing program will read a string from standard input and report the result string to standard output, along with the rest of the input that follows the string data.  The result string is reported with escape sequences standing in for bytes outside of ASCII printing range.  Command-line parameters allow the particular output override mode to be selected, or a non-override mode using a hardwired test encoding table.  Input override mode will always be selected during testing.  A hardwired test decoding map will be used.

These steps will be performed in separate branches, with results merged back into master when complete.  At the end of this process, the block reading architecture will be established and the token reader will be done.

## 5. Releases

### 0.2.1 (alpha)

Began the regular string reader framework, and completed the encoding component.  The decoding component is currently just a placeholder that sends a fixed sequence of entity codes to the encoding component -- this will be replaced with the full decoder in the next release.

### 0.2.0 (alpha)

Established the block reader module (shasm_block) with just a token reader for now, and developed a test program (test_block) for testing the block reader module.

### 0.1.0 (alpha)

Completed the input filter chain (shasm_input) and developed a test program (test_input) for testing the input filter chain.
