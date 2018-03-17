# libshasm
## 1. Introduction
libshasm (pronounced "lib sha-ZAM") is a library that implements a Shastina language metainterpreter.

For a specification of the Shastina language, see the [Canidtech website](https://www.canidtech.com/).  See the "Divergences" section below in this readme for differences between Shastina as described in the specification and Shastina as implemented in libshasm.

As of March 15, 2018, the input filter chain has been completed.  This readme describes the current state of the project.

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

## 3. Roadmap
The current development roadmap is as follows.  Section references are to the Shastina language specification, currently on draft 3V:C4-5.

- [x] Input filtering chain (section 3)
- [ ] Tokenization function (section 4)
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
The current goal is the second stage of the roadmap, which is to complete the tokenization function.  However, this stage of the roadmap will also lay the groundwork for the next few stages (up to and including base-85 special mode).

Whereas the first stage of the roadmap considered the input as a sequence of characters and performed character-based filtering operations, the next stages of the roadmap consider the input as a sequence of blocks.  The purpose of these stages is to transform the filtered character input into blocks.

A block is defined here as a string of bytes with a length between zero bytes (empty) and 65,535 bytes.  A block reader module (shasm_block) will be defined that can read blocks from the filtered input stream of characters.  The block reader will support reading tokens, normal string data, base-16 string data, and base-85 string data.  Each of these reading operations will have its own function in the block reader.  However, the block output of each of these reading operations will use the same buffer within the block reader.

The goal of the second stage of the roadmap is therefore not only to complete the tokenization function, but also to define the block reader module.  Once the block reader module is defined, the next few roadmap stages will simply add additional reader functions to the block reader module to support string data.

The second stage of the roadmap will also define a testing module (test_block) that is able to test each of the block reading functions.  The next roadmap stages can therefore make use of test_block without having to define their own testing modules.

The block testing module is a program that reads from standard input using the block reader module and reports on standard output the blocks that have been read.  A command-line parameter chooses what kind of blocks the block reader module will read from input.  For this roadmap stage, there will only be a "token" mode, which uses the token reader to read one or more token blocks from input, ending when the "|;" block is encountered.  (Note that this not able to parse full Shastina files, because it lacks string data reading functionality.)  Subsequent roadmap stages will add their own modes to the block testing module.

Once the second roadmap stage has been completed, an alpha 0.2.0 release will be made.  The subsequent block reading roadmap stages will be handled as patch releases (0.2.1, 0.2.2, etc.) such that at the end of the 0.2.x alpha series, the block reading layer of shasm will be complete.

### 4.1 Worklist
To reach the current goal, the following steps will be taken, in the order shown below:

- [ ] Define the shasm_block interface in a header
- [ ] Define the test_block testing module
- [ ] Implement the buffer functions of the block reader
- [ ] Implement the token function of the block reader

The block interface and testing module will be limited to just the token function for now.  The string data functions will be added in subsequent roadmap steps.

These steps will be performed in separate branches, with results merged back into master when complete.  At the end of this process, the block reading architecture will be established and the token reader will be done.

## 5. Releases

### 0.1.0 (alpha)

Completed the input filter chain (shasm_input) and developed a test program (test_input) for testing the input filter chain.
