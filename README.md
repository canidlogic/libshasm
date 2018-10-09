# Shastina Metainterpreter
## 1. Introduction
The Shastina Metainterpreter (shastina) is an implementation of the Shastina metalanguage, which allows for the rapid implementation of special-purpose programming languages.

## 2. Project status
This project has been dormant for a few months, but as of October 2018, work has restarted.

The new Shastina implementation has a greatly simplified structure that makes it more practical to use, more efficient, and more flexible.

The alpha 0.2.2 series work has been discontinued, in favor of the new implementation.  However, the sophisticated string reading algorithms of the alpha 0.2.2 series (which are not present in the new implementation) will eventually be spun off into a separate project.

I plan to have a fully functional implementation of Shastina available by October 31, 2018.

Development work on this library is being handled by Noah Johnson (canidlogic on GitHub).  He can be reached by email at noah.johnson@loupmail.com

## 3. Releases

### 0.2.2 (alpha)

Finished the regular string reader framework by implementing the full decoder.  This is the last release in the original alpha line, which was discontinued in favor of a simplified implementation.  The string reader framework should eventually be spun off into a separate project -- it does not appear in the new, simplified implementation of Shastina.

### 0.2.1 (alpha)

Began the regular string reader framework, and completed the encoding component.  The decoding component is currently just a placeholder that sends a fixed sequence of entity codes to the encoding component -- this will be replaced with the full decoder in the next release.

### 0.2.0 (alpha)

Established the block reader module (shasm_block) with just a token reader for now, and developed a test program (test_block) for testing the block reader module.

### 0.1.0 (alpha)

Completed the input filter chain (shasm_input) and developed a test program (test_input) for testing the input filter chain.
