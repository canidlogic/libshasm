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

#### 2.6.1 Decoding map overlay

In order to ensure the decoding operation proceeds smoothly, certain keys in the decoding map must be ignored if they occur.  The decoding map overlay is a wrapper around the decoding map that the client provides, which makes sure the necessary keys are ignored.  In addition, the overlay provides a function which returns whether the current node is a "stop" node, which means it is guaranteed that there are no branches from this node and that the client should not read any further in input.  (This is to prevent the client from reading beyond the closing quote or curly bracket of the input string data.)  There are also functions for returning the most recent branch character (if at least one branch has been taken) and whether the most recent branch was the first branch taken.

The overlay's function for resetting the position back to the root takes a parameter indicating whether the nesting level should be changed, or whether it should be reset to the initial value of one.  The change can increase the nesting level or decrease it.  This is only allowed for {} string types, and the nesting level may never go below the initial level of one.  The branch and entity functions have the same interface as the underlying decoding map.

The stop node function returns true in a "" string if at least one branch has been taken and the last branch taken was for the " quote.  The stop node function returns true in a '' string if at least one branch has been taken and the last branch taken was for the ' quote.  The stop node function returns true in a {} string if at least one branch has been taken and the last branch taken was for the { or } bracket.

The overlay branch function normally calls through to the underlying decoding map.  However, in the following cases, it always returns that there is no branch, without consulting the underlying decoding map:

(1) If the current node is a stop node, the branch function always fails.

(2) In a "" string, if no branches have been taken yet, the " branch fails if attempted as the first branch.  In a '' string, if no branches have been taken yet, the ' branch fails if attempted as the first branch.

(3) In a {} string, if no branches have been taken yet and the nesting level is one, the } branch fails if attempted as the first branch.

(4) If an input override is active, branches corresponding to bytes with their most significant bit set always fail.

The overlay entity function normally calls through to the underlying decoding map.  The only exception is that if no branches have been taken yet, the function always returns no entity without querying the underlying decoding map.  This has the effect of ignoring empty keys.

#### 2.6.2 Speculation buffer

The regular string decoder requires a more sophisticated buffer than the single-byte pushback buffer offered by the input filter stack.  The "speculation buffer" is built on top of the input filter stack to allow for more sophisticated backtracking during regular string decoding.

The speculation buffer has a "detach" function (described later) that empties the speculation buffer such that the input filter stack can pick up right where the speculation buffer left off.  This allows the speculation buffer to be used only where required, and for the rest the speculation buffer can be detached and Shastina can return to only using the single-byte pushback buffer in the input filter stack.

The speculation buffer stores zero or more bytes before the current filtered input position.  The speculation buffer is divided into zero or more bytes in a "back" buffer followed by zero or more bytes in a "front" buffer.  Together, the back and front buffer fill the entire speculation buffer, such that the bytes in the back buffer come first, followed by the bytes in the front buffer, followed by additional filtered input bytes read from the input filter stack.  The speculation buffer never uses the pushback buffer of the input filter stack, except during the detach operation as described later.

The speculation buffer starts out with both the back and front buffers empty.  There is also a "mark" flag that starts out clear.  If the mark flag is clear, then the back buffer must be empty.  (If the mark flag is set, the back buffer may be empty or it may have one or more bytes in it.)

The read operation of the speculation buffer first checks whether the mark flag is set.  If the mark flag is clear, the speculation buffer checks whether the front buffer is empty; if it is not empty, a byte is taken from the front of the front buffer and returned; else, a byte is read directly from the input filter stack and returned.  If the mark flag is set, the speculation buffer checks whether the front buffer is empty; if it is not empty, a byte is transferred from the front of the front buffer to the end of the back buffer and this transferred byte is returned; else, a byte is read directly from the input filter stack, copied to the end of the back buffer, and returned.

The mark operation of the speculation buffer empties the back buffer if not already empty, discarding its contents.  It then sets the mark flag if it has not yet been set.

The restore operation of the speculation buffer clears the mark flag and transfers any bytes in the back buffer to the start of the front buffer.

The backtrack operation can only be used if the mark flag is set and there is at least one byte in the back buffer.  If these conditions hold, one byte is transferred from the end of the back buffer to the start of the front buffer.

The unmark operation performs a mark operation followed by a restore operation.  It has the effect of keeping the input position where it is but clearing any marks.

The detach operation has three cases.  If the mark flag is clear and the front buffer is empty, the detach operation returns successfully without doing anything.  If the mark flag is clear and the front buffer has one byte, the detach operation sets pushback mode in the input filter stack, empties the front buffer, and returns successfully.  If the mark flag is set or the front buffer has more than one byte, the detach operation fails.

The read operation allows for regular reading from the buffer.  Use the mark operation to remember the input position, and use restore to recall the most recently marked input position.  Backtrack allows for backtracking, provided that a mark is active and backtracking does not go further back than the most recent mark.  Unmark clears any mark that has been set.  Finally, detach empties the speculation buffer while synchronizing with the input filter stack (if possible).

The speculation buffer is built around a circular queue structure that grows dynamically.  The queue provides functions for appending a byte to the end of the queue, for removing one or more bytes from the start of the queue, and for retrieving an element given an index relative to the start of the queue.  Growth is achieved by expanding the memory block and moving any trailing portion of the queue to the new end of the memory area.  The back and front buffers are built on top of the circular queue by storing the count of bytes in each.  Transfers between the two buffers can be done by just adjusting the two byte counts.

#### 2.6.3 Algorithm

The innermost layer of the regular string decoding algorithm uses the decoding map to either decode the next entity value and return that, or return that no entity could be found and that the speculation buffer has been successfully detached.  All other conditions result in error.  The innermost layer uses the decoding map overlay (described above) over the decoding map, and the speculation buffer (described above) over the input filter stack.  Before the loop, the speculation buffer is unmarked to reset the buffer and the decoding overlay is reset to root position (but the nesting level is kept at its current state).  The loop begins by reading a byte from the speculation buffer.  It then attempts to branch in the decoding overlay according to the read byte.  If branching succeeds, the speculation buffer is marked and the entity code remembered if the new location has an entity code; also, the stop flag is set if the new location is a stop node; also, if this is a {} string and this is the first branch and the byte was either { or } then increase or decrease the nesting level in the decoding overlay.  If branching fails, the stop flag is set.  At the end of the loop, if the stop flag is set, then check whether the speculation buffer is marked; if it is marked, restore the marked location and return the most recently remembered entity code; otherwise, attempt to detach, returning successfully with no entity code if the detach operation is successful, and otherwise returning an error.  If the stop flag is clear at the end of the loop, loop back to the beginning.

Around the innermost layer is a wrapper that handles numeric escapes.  It has the same interface as the innermost layer.  It calls through to the innermost layer.  If the innermost layer returns successfully with no entity code, this result is passed through.  If the innermost layer returns an entity code, this wrapper checks whether the entity code matches a numeric escape by querying the escape list.  If there is no match, the entity code is passed through.  If there is a match, the wrapper gets the numeric escape information, reads the digits (using mark and backtrack functionality of the speculation buffer), and then returns the entity code decoded from the digits rather than the entity code returned by the innermost layer.  This enables numeric escaping.

The outermost inner decoding function calls through to the numeric escape wrapper.  For any entity codes it receives, it sends these to the encoding phase.  It returns when a successful no-entity return has been received.

The outer decoding function is built around the inner decoding functions.  Unlike the inner decoding functions, it does not use the speculation buffer but rather handles everything with the pushback buffer of the input filter stack.  Its loop begins by calling the outermost inner decoding function to decode a sequence of zero or more entities (including numeric escaped entities) and send those to the encoder.  Then, it picks up where the inner decoding functions left off and tries to interpret the data using the built-in keys.  The built-in keys include the terminal (closing single or double quote or closing curly bracket) for the particular string type, and, if an input override is active, sequences of bytes with their most significant bit set.  If a terminal key is encountered, the decoder finishes.  If an input override key is encountered, the decoder decodes one or more filtered input bytes according to the input override, sending the decoded entities to the encoder.  It then loops back to the beginning.

### 2.7 Input override surrogate handling

In section 5.1 of draft 3V:C4-5 of the Shastina Specification, it is specified that when input override mode is active, improperly paired surrogates encoded in UTF-8 will be passed through to the encoder as-is.  In libshasm, improperly paired surrogates are not allowed during input override decoding, and their presence causes an error.

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

### 4.1 Worklist
To reach the current goal, the following steps will be taken, in the order shown below:

- [x] Define decoding overlay interface
- [x] Define circular queue interface
- [x] Define speculation buffer interface
- [x] Define inner decoding interfaces
- [x] Revise outer decoding interface
- [x] Write inner placeholders (return no entity)
- [x] Write outer function with terminal keys only
- [ ] Add input override support to outer function
- [ ] Write outermost inner function
- [ ] Implement circular queue
- [ ] Implement speculation buffer
- [ ] Write numeric escape inner function
- [ ] Implement decoding overlay
- [ ] Write innermost decoding function

The basic approach is to write the interfaces first (first five steps), add placeholders (next step), and then finish the functions starting with the outermost decoding function and working inward.  At first, only empty strings (terminal key immediately) are supported.  Then, strings with only extended UTF-8 characters in input override mode are supported.  Then, strings beginning with numeric escapes.  Finally, full decoding support.

These steps will be performed in separate branches, with results merged back into master when complete.  At the end of this process, the block reading architecture will be established and the token reader will be done.

## 5. Releases

### 0.2.1 (alpha)

Began the regular string reader framework, and completed the encoding component.  The decoding component is currently just a placeholder that sends a fixed sequence of entity codes to the encoding component -- this will be replaced with the full decoder in the next release.

### 0.2.0 (alpha)

Established the block reader module (shasm_block) with just a token reader for now, and developed a test program (test_block) for testing the block reader module.

### 0.1.0 (alpha)

Completed the input filter chain (shasm_input) and developed a test program (test_input) for testing the input filter chain.
