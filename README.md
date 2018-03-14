# libshasm
## 1. Introduction
libshasm (pronounced "lib sha-ZAM") is a library that implements a Shastina language metainterpreter.

For a specification of the Shastina language, see the [Canidtech website](https://www.canidtech.com/).  See the "Divergences" section below in this readme for differences between Shastina as described in the specification and Shastina as implemented in libshasm.

As of March 2018, this library is currently in the earliest stages of development and nowhere near functional yet.  This readme describes the current state of the project.

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

- [ ] Input filtering chain (section 3)
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
The current goal is the first stage of the roadmap, which is to complete the input filtering chain described in section 3 of the Shastina language specification (draft 3V:C4-5).

In order to complete and test this goal, a simple program will be written that reads from standard input, passes standard input through the sequence of input filters described in the specification, and writes the filtered results to standard output.

In order to allow the program to be used for testing the filter chain, the output will make whitespace and control characters (such as space, line break, and horizontal tab) visible as text representations of the characters.

Additionally, the program will display at the start of each line the line number, as tracked by the line count filter.  At the end of the output, the program will report the line number at the end of input, as well as whether a UTF-8 Byte Order Mark (BOM) was filtered out at the beginning of input.

Finally, the program will support a special mode where the output of the filter chain will be echoed to standard output as-is, except each non-whitespace US-ASCII character will be doubled by using the pushback buffer to backtrack by one character for each non-whitespace US-ASCII character.

### 4.1 Worklist
To reach the current goal, the following steps will be taken, in the order shown below:

- [x] Write the testing program with no filters yet
- [x] Add the UTF-8 Byte Order Mark (BOM) filter
- [x] Add the line break conversion filter
- [x] Add the final LF filter
- [x] Add the tab unghosting filter
- [x] Add the line unghosting filter
- [x] Add the line count filter
- [x] Add the pushback buffer filter
- [ ] Split the filters out to a separate module

See the "Divergences" section of this readme for more about the tab and line unghosting filters, which are not the same as the unghosting filter described in draft 3V:C4-5 of the Shastina Specification.

Each of these steps will be performed in a separate branch, with results merged back into master when complete.  At the end of this process, the input filtering chain module will be complete, and work will proceed to the next stage in the roadmap.
