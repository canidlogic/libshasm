# libshasm
## 1. Introduction
libshasm (pronounced "lib sha-ZAM") is a library that implements a Shastina language metainterpreter.

For a specification of the Shastina language, see the [Canidtech website](https://www.canidtech.com/).

As of March 2018, this library is currently in the earliest stages of development and nowhere near functional yet.  This readme describes the current state of the project.

Development work on this library is being handled by Noah Johnson (canidlogic on GitHub).  He can be reached by email at noah.johnson@loupmail.com

## 2. Roadmap
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

## 3. Current goal
The current goal is the first stage of the roadmap, which is to complete the input filtering chain described in section 3 of the Shastina language specification (draft 3V:C4-5).

In order to complete and test this goal, a simple program will be written that reads from standard input, passes standard input through the sequence of input filters described in the specification, and writes the filtered results to standard output.

In order to allow the program to be used for testing the filter chain, the output will make whitespace and control characters (such as space, line break, and horizontal tab) visible as text representations of the characters.

Additionally, the program will display at the start of each line the line number, as tracked by the line count filter.  At the end of the output, the program will report the line number at the end of input, as well as whether a UTF-8 Byte Order Mark (BOM) was filtered out at the beginning of input.

Finally, the program will support a special mode where the output of the filter chain will be echoed to standard output as-is, except each non-whitespace US-ASCII character will be doubled by using the pushback buffer to backtrack by one character for each non-whitespace US-ASCII character.

### 3.1 Worklist
To reach the current goal, the following steps will be taken, in the order shown below:

- [x] Write the testing program with no filters yet
- [x] Add the UTF-8 Byte Order Mark (BOM) filter
- [x] Add the line break conversion filter
- [x] Add the final LF filter
- [ ] Add the unghosting filter
- [ ] Add the line count filter
- [ ] Add the pushback buffer filter
- [ ] Split the filters out to a separate module

Each of these steps will be performed in a separate branch, with results merged back into master when complete.  At the end of this process, the input filtering chain module will be complete, and work will proceed to the next stage in the roadmap.
