# Shastina Specification

- &#x200b;1. [Introduction](#mds1)
- &#x200b;2. [Input processing](#mds2)
- &#x200b;3. [Token parsing](#mds3)
  - &#x200b;3.1 [Parsing definitions](#mds3p1)
  - &#x200b;3.2 [Tokens](#mds3p2)
  - &#x200b;3.3 [String tokens](#mds3p3)
- &#x200b;4. [Client state](#mds4)
- &#x200b;5. [Token interpretation](#mds5)
  - &#x200b;5.1 [Metacommand mode](#mds5p1)
  - &#x200b;5.2 [Grouping](#mds5p2)
  - &#x200b;5.3 [Arrays](#mds5p3)
  - &#x200b;5.4 [Numeric literals](#mds5p4)
  - &#x200b;5.5 [String literals](#mds5p5)
  - &#x200b;5.6 [Load/store entities](#mds5p6)
  - &#x200b;5.7 [Operations](#mds5p7)
  - &#x200b;5.8 [EOF entity](#mds5p8)

## 1. <span id="mds1">Introduction</span>

Shastina is a simple, plain-text format for storing structured data.  It is based on the concept of a stack machine and reverse-Polish notation, inspired by languages such as Forth and PostScript.  However, Shastina is not intended to be a full programming language.  It is only intended to be used as a data storage and transmission format, although its procedural architecture makes it flexible enough to resemble a primitive scripting language.

Shastina has a similar range of applications as XML.  XML is designed like a document markup system due to its evolution from SGML.  This makes XML a natural choice for representing data that is like a plain-text file with additional annotations and structuring information.  XML is also used as a general-purpose data storage format.  However, its document-based architecture requires verbose workarounds when the underlying data does not resemble a linear document, and it those cases many features of XML are unused, resulting in an unnecessarily complicated data storage standard.  XML also has a hierarchical format, which can be a problem when the underlying data does not fit easily into clear hierarchies.  Shastina's procedural architecture, on the other hand, is more flexible and far less verbose, especially in cases of non-hierarchical data that does not resemble a linear document.

Shastina also has a similar range of applications as JSON.  JSON is (almost) a subset of JavaScript, but with programming features removed such that only the data definition syntax remains.  JSON is often used as a simplified alternative to XML, and it is widely supported.  However, there are a few quirks that make the format less than ideal.  JavaScript is an unusual programming language in that it does not have an integer data type, instead using floating-point numbers everywhere.  As a result, JSON also does not have an integer data type, instead making do with just a floating-point numeric type.  This can be a problem, since floating-point parsing is much more complicated than integer parsing, and floating-point interpretation can have platform-specific behavior.  JSON also is based around an associative array type that maps string keys to arbitrary values.  This architecture can make it difficult to work with JSON without loading the entire parsed representation into memory, which can be a problem with very large data sets.  Finally, JSON can become unwieldly to edit when there are multiple nested levels, due to the various separator symbols that are required.  Shastina, on the other hand, allows for multiple numeric types, it is easy to interpret Shastina in a single linear pass through the file without loading the whole data into memory, and the reverse Polish syntax is much easier to manage with complex data.

## 2. <span id="mds2">Input processing</span>

Shastina files may be encoded in US-ASCII or UTF-8.  Line breaks may be LF or CR+LF, or any mixture of these two styles.  CR characters may only occur immediately before an LF or there is an error.  Shastina's input filtering will convert all line break styles to LF line breaks.

Shastina files may optionally begin with a UTF-8 Byte Order Mark (BOMs).  A UTF-8 BOM is the three literal byte values `0xEF 0xBB 0xBF`.  If present at the very start of the file, they will be filtered out by Shastina's input filtering.

Shastina will decode Unicode codepoints from UTF-8 and then re-encode the codepoints into UTF-8 during input filtering to make sure that all the UTF-8 encoding is clean.  Encoded surrogates are not supposed to be present in UTF-8 input, but Shastina will decode encoded surrogate pairs into the supplemental codepoint and then re-encode the supplemental codepoint directly into a single UTF-8 codepoint.  Improperly paired surrogate characters will cause interpretation of the Shastina file to fail on an error.  The input is also forbidden from having nul characters (codepoint zero) anywhere, with interpretation stopping on an error if any such codepoints are present.

This Unicode filtering allows clients to assume that there will be no surrogate codepoints, and that they may use terminating nul characters without worrying about nul characters in the input.  However, beware that Shastina does not do any Unicode normalization.

Interpretation of Shastina input stops immediately after reading the semicolon in the closing `|;` token of the file.  This means that additional data can be added after the Shastina file, or that multiple Shastina files may be put together in sequence.  The Shastina interpreter will not consume input beyond the `|;` token unless the client specifically requests it.

## 3. <span id="mds3">Token parsing</span>

After input processing, the next step is to parse the text data into tokens.  There are two kinds of tokens &mdash; _simple tokens_ and _string tokens_.  Simple tokens are a sequence of one or more visible US-ASCII characters.  String tokens have a prefix sequence of zero or more visible US-ASCII characters, a type flag which indicates whether the string is quoted or surrounded by curly braces, and zero or more bytes of string literal data.

The following subsections discuss parsing in further detail.

### 3.1 <span id="mds3p1">Parsing definitions</span>

__Visible US-ASCII characters__ consist of codepoints \[0x21, 0x7E\].  This includes all US-ASCII characters that have a visible appearance and excludes space, tab, line breaks, control characters, and extended Unicode characters.

__Whitespace__ consists of Horizontal Tab (HT), Space (SP), and Line Feed (LF).  Input processing (see [&sect;2](#mds2)) has already converted CR+LF line breaks into LF-only line breaks.

__Comments__ begin with a pound sign `#` and proceed up to the next LF.  However, a pound sign that occurs within a string literal is treated as part of the string data and does __not__ mark the start of a comment.  It is an error to encounter End Of File within a comment, since Shastina files must end with the `|;` marker token.

__Atomic characters__ are the following:

    ( ) [ ] , % ; " { }

__Exclusive characters__ are the following:

    HT SP LF ( ) [ ] , % ; # }

__Inclusive characters__ are the following:

    " {

The range of input characters (after input processing) is limited to the visible US-ASCII characters, SP, HT, LF.  It is an error to encounter other characters, except within comments and string data.

### 3.2 <span id="mds3p2">Tokens</span>

Shastina reads __tokens__ according to the following method:

1. Zero or more bytes of whitespace and comments are skipped until the first visible, non-comment US-ASCII character is encountered.

2. If the first visible, non-comment US-ASCII character is an atomic character, then the token consists just of that character.

3. If the first visible, non-comment US-ASCII character is a vertical bar, and this is followed immediately by a semicolon, then the token consists of the vertical bar followed by the semicolon `|;`

4. If the first visible, non-comment US-ASCII character is not an atomic character, and not a bar-semicolon `|;` token, then the token consists of the first character and all legal US-ASCII characters that immediately follow it, up to the first exclusive character or inclusive character that is encountered.  If the token ends with an exclusive character, the exclusive character is __not__ included in the token.  If the token ends with an inclusive character, the inclusive character __is__ included in the token.

The special `|;` token is always the last token in the file.  Only whitespace and comments may follow it.  It is an error to encounter End Of File before the `|;` token has been read.  Note however that the `|;` may occur within comments or string literals, in which case it is __not__ treated as a token and does __not__ mark the end of the Shastina file.

If the last character of the token that was read is a double quote or a left curly bracket, then the token is a string token (see [&sect;3.3](#mds3p3)).  Otherwise, the token is a simple token.

### 3.3 <span id="mds3p3">String tokens</span>

When Shastina reads a token (according to the rules in [&sect;3.2](#mds3p2)) for which the last character is either a double quote or a left curly bracket, the token is a __string token__.  The zero or more visible US-ASCII characters that precede the double quote or the left curly bracket are the prefix sequence.  The double quote or the left curly bracket determines whether a quoted or curly string is present.

In order to read a __quoted string__, Shastina uses the following method.  The string data consists of the zero or more bytes that immediately follow the opening double quote, up to but excluding the terminating double quote.  The terminating double quote is the first double quote after the opening double quote that is not immediately preceded by an odd-numbered sequence of backslash characters.  The input filtering described in [&sect;2](#mds2) is applied to string data.  Note that embedded double quotes that are escaped with an odd-numbered sequence of backslashes are left in the string data as-is, without removing the preceding backslash.  This allows clients to implement their own escaping systems, where backslash followed by double quote is an escaped double quote, and backslash followed by another backslash is an escaped backslash.

In order to read a __curly string__, Shastina uses the following method.  Left curly brackets `{` and right curly brackets `}` that are not immediately preceded by an odd-numbered sequence of backslashes are _balanced curlies_.  Left and right curly brackets that are immediately preceded by an odd-numbered sequence of backslashes are _escaped curlies_.  The _nesting level_ starts out at one immediately after the opening left curly bracket.  Each time a balanced left curly is read, the nesting level is increased by one.  Each time a balanced right curly is read, the nesting level is decreased by one.  The balanced right curly that causes the nesting level to decrease to zero is the _closing curly_.  The curly string data includes the zero or more bytes of data that immediately follow the opening left curly bracket, up to but excluding the closing curly.  The input filtering described in [&sect;2](#mds2) is applied to string data.  Balanced and escaped curlies are left in the data as-is.  Clients may implement their own escaping systems where backslash followed by a left or right curly bracket is an escape for the curly bracket, and backslash followed by backslash is an escape for a backslash.

Shastina has an internal limit on the maximum string length.  An error will occur for strings that are longer than this.  Shastina has an internal limit on the maximum nesting level within curly strings.  This limit is greater than two billion, so it shouldn't normally be an issue.

Unicode characters encoded in UTF-8 are supported within string data, but beware that Shastina does not perform any Unicode normalization.

See [&sect;5.5](#mds5p5) for further information about Shastina string handling.

## 4. <span id="mds4">Client state</span>

The Shastina implementation only provides an entity parser for Shastina files ([&sect;5](#mds5)) and leaves the interpretation of entities up to the client.  However, Shastina was designed for a certain kind of client architecture that is described in this section.  There is no requirement that clients actually use this architecture, however.

Shastina is designed for a client architecture that features a stack.  The kind of data stored on the stack depends on the needs of the client.  Simple applications of Shastina might just have a stack of integers, while more complex applications might allow for references to objects and have automatic garbage collection.

Shastina provides string entities and numeric entities that are intended to allow string and number literals to be pushed on top of the stack.  Shastina has a flexible definition of string and number entities that allow for multiple kinds of numeric types and multiple kinds of string types to be distinguished if the client so desires.  For simple clients, if certain entity types are not needed, the client can choose to cause an "unsupported entity type" error whenever they are encountered, preventing such entity types from being used.  This allows the complexity of the Shastina implementation to scale depending on the needs of the client.

Shastina also provides an operation entity that is intended to run operations, using the stack to provide any parameters and return values.  The combination of string and numeric entity types that allow for pushing literal values on the stack with operations that operate on stack data is the basis of Shastina's procedural, reverse Polish, stack-based architecture.

More complex types beyond string and numeric entities can also be supported by clients.  This is possible by building these more complex entities out of simpler entities, until everything can be decomposed into strings and numbers.  Complex data types can therefore be constructed in Shastina by starting with strings and numbers and then using operations to build up the higher-level structures in a bottom-up fashion.  (Note that this is the opposite of the top-down structuring used in XML and JSON.)

For many Shastina clients, the stack may be all that is necessary in the client state.  Sophisticated clients, however, might find the stack by itself to be too limiting.  In these complex cases, Shastina also provides entities for declaring and working with constants and variables.  The constants and variables are recommended to be stored in a dictionary mapping string keys with the constant and variable names to the values.  There are store and put entities that allow popping values from the stack into a variable and pushing a value from a variable or a constant onto the stack.  Clients are, however, free to decide all the details of how variables and constants work.  For simpler clients that do not need this facility, they can just cause an "unsupported entity type" error whenever the variable and constant entities are encountered, preventing such entities from being used.

## 5. <span id="mds5">Token interpretation</span>

After the input has been parsed into tokens (see [&sect;3](#mds3)), the final step is token interpretation.  Shastina only performs low-level interpretation to establish the basic structure of a Shastina file.  Through this process, the raw tokens that are parsed are categorized into _entities_.  It is then left to the client to decide how specifically to interpret the various entities that are present within a Shastina file.  See [&sect;4](#mds4) for recommendations about the client architecture.

It should also be noted that clients do not have to make use of every entity type supported by Shastina.  For simpler formats, only a few different entity types may be needed, and all the rest can just cause errors indicating that the entity type is unsupported.

### 5.1 <span id="mds5p1">Metacommand mode</span>

The first distinction that Shastina interpretation makes is between _metacommand mode_ and _regular mode_.  Shastina interpretation always begins regular mode, and interpretation must always be in regular mode when the `|;` token that ends the Shastina file occurs, or an error occurs.

Metacommand mode is entered when the `%` token is parsed.  Since `%` is both an atomic character and an exclusive character (see [&sect;3.1](#mds3p1)), by the parsing rules of [&sect;3.2](#mds3p2) it will always stand by itself within a token.  (It is not interpreted as a token when it occurs within a comment or string data, however.)  The `%` token is not allowed within metacommand mode, so there is no possibility of nested metacommand modes.

Metacommand mode is left when the `;` token is parsed.  Since `;` is both an atomic character and an exclusive character, it always stands by itself within a token, except for the special case of the `|;` token that ends the Shastina file, as well as when it occurs in comments and in string data.  The `;` token is not allowed outside of metacommand mode.

While in metacommand mode, Shastina only distinguishes between _metacommand tokens_ and _metacommand strings_.  Metacommand strings are the parsed string tokens ([&sect;3.3](#mds3p3)) that occur within metacommand mode, while metacommand tokens are all the other tokens that occur within metacommand mode.  The `%` token that begins metacommand mode and the `;` token that ends metacommand mode are _not_ considered to be metacommand tokens themselves.  Shastina will report the start of metacommand mode at `%` and the end of metacommand mode at `;` as separate entities.

Note that Shastina still applies parsing rules related to atomic characters, inclusive characters, and exclusive characters while in metacommand mode.  Therefore, given the following:

    %example (metacommand);

Shastina will generate the following tokens:

1. Begin metacommand (`%`)
2. Token `example`
3. Token `(`
4. Token `metacommand`
5. Token `)`
6. End metacommand (`;`)

Although it is up to clients to determine the meaning of metacommand mode, it is recommended that Shastina formats always require Shastina files to begin with a metacommand that defines the specific kind of format that is used within the file, and possibly also version information.  For example, a Shastina format `foo-format` might require files to begin with the following metacommand:

    %foo-format;

This then serves as the signature of the file type and allows different kinds of Shastina files to be clearly distinguished from each other.

### 5.2 <span id="mds5p2">Grouping</span>

In regular mode (outside of metacommand mode, see [&sect;5.1](#mds5p1)), Shastina supports the concept of _grouping_.  Grouping is never actually required, but it is provided to help users who are manually entering Shastina code to avoid common typos and to easily debug their files.  Simpler Shastina formats may choose to not support grouping, since grouping is only useful in more complicated formats.

A group begins with the `(` entity and ends with the `)` entity.  Groups may be nested, though the Shastina implementation may impose a limit on the maximum amount of nesting supported and cause an error if the nesting becomes too deep.  Each `(` entity must be closed with a `)` entity, and this is enforced by the Shastina parser.

In order for grouping to be useful, the client must implement grouping checks when the begin group and end group entity are encountered.  At the start of a group, the client should hide everything that is currently on the stack (see [&sect;4](#mds4)) and proceed as if the stack were empty.  At the end of a group, the client should check that exactly one element remains on the stack, raising an error if there are no elements on the stack or if there is more than one element on the stack.  Once the grouping check is complete, the items on the stack that were hidden when the group began should be made to reappear (along with the new element on top).

Grouping allows authors of Shastina files to check that everything within a group results in a single value on top of the interpreter stack at the end of the group.  Since the Shastina library allows the client to define and manage the interpreter stack, the client must implement the grouping check.  The Shastina library simply makes sure that all groups are closed properly.

### 5.3 <span id="mds5p3">Arrays</span>

In regular mode (outside of metacommand mode, see [&sect;5.1](#mds5p1)), Shastina has support for declaring arrays.  Arrays in Shastina are simply a syntactic sugar that automatically counts the number of elements in an array and then generates a count of how many elements were in the array.

Arrays begin with the `[` entity and end with the `]` entity.  Array elements must be separated with `,` entities.  Arrays may be nested, though Shastina implementations may impose a limit on how deep array nesting may become.  Shastina implementations may also impose a limit on how many elements may be in an array, but this limit must at least be a billion elements, so this is unlikely to be a problem in most cases.

Note that Shastina arrays are only relevant to arrays declared as literals within the Shastina source file.  The client may support their own array type that is constructed using the bottom-up strategy for complex data types described in [&sect;4](#mds4).  This client-defined array type is independent of the array definition given in this section, though it would be easy to define a constructor for the client-defined array type that takes a Shastina array.

When an array begins with `[`, Shastina does __not__ generate any entity.  Instead, Shastina remembers that a new array is now open and begins a count of array elements, starting the count at zero.

If the next entity in the Shastina file after `[` is `]`, then this is handled as a special "empty array" case.  In this case, Shastina simply generates an array entity with a count of zero, indicating an empty array.

If the next entity after `[` is anything other than `]`, then Shastina assumes the array has at least one element.  Shastina automatically inserts a begin grouping entity (see [&sect;5.2](#mds5p2)) before the entity that comes after the `[`.  Note that this means that if clients support array entities, they must also support grouping entities.  Furthermore, Shastina hides all open groups below the newly opened group, so that end group entities within the Shastina data file may not break out of the group that begins the array element.

The `,` entity may only be used within arrays.  When it occurs, Shastina automatically inserts an end group entity, and it also verifies that this end group entity corresponds to the begin group entity that began the array element.  Shastina then inserts another begin group entity, keeping open groups below this newly opened group hidden.

The `]` entity ends an array.  Every `[` must be closed by a corresponding `]` or Shastina will cause an error.  Note the special case of an empty array described earlier, in which case `]` is handled specially.  Otherwise, `]` automatically inserts an end group entity, and it also verifies that this end group entity corresponds to the begin group entity that began the last array element.  Finally, Shastina unhides the open groups that were hidden when the array was originally opened.

To support arrays, clients must simply implement grouping as described in [&sect;5.2](#mds5p2) and also implement the array entity, which simply provides the number of elements in the array.  The recommended way to implement the array entity is to simply push an integer corresponding to the number of array elements onto the stack.  If this recommended implementation is used, then Shastina array notation becomes a pure syntactic sugar for automatically counting elements and pushing the count onto the stack after all the elements, such that the following are equivalent:

    [1, 2, 3, 1, 2, 3]
    1 2 3 1 2 3 6

Clients may also choose to handle array elements differently, in which case the above equivalency does __not__ necessarily hold.

### 5.4 <span id="mds5p4">Numeric literals</span>

In regular mode (outside of metacommand mode, see [&sect;5.1](#mds5p1)), Shastina has support for numeric literal entities.  To allow clients flexibility in deciding how they want to handle numeric values, Shastina only requires that numeric literal tokens begin with a sign (`+` or `-`) or a decimal digit.  The rest of the interpretation of numeric literals is up to the client.

Simple clients might choose to only support integers.  More sophisticated clients might have multiple numeric types distinguished by suffixes or other means.  Note that there are no limits on the content of numeric tokens, beyond the requirement that tokens may only use visible US-ASCII characters and that the first character must be a sign or a decimal digit.

If more sophisticated numeric types are required that can not be represented by a numeric literal token, the two strategies are either to build the complex type from the bottom-up with operators (see [&sect;4](#mds4)), or to represent the type as a string type with a special prefix (see [&sect;5.5](#mds5p5)).

It is recommended (but not required) that clients implement numeric literals by pushing the numeric literal value on top of the interpreter stack (see [&sect;4](#mds4)).

### 5.5 <span id="mds5p5">String literals</span>

In regular mode (outside of metacommand mode, see [&sect;5.1](#mds5p1)), Shastina has support for string literal entities.  The parsing of string literals is covered in [&sect;3.3](#mds3p3).  Parsing a string literal will result in the following data:

1. The type of string &mdash; quoted or curly
2. Zero or more US-ASCII characters in the string prefix
3. Zero or more UTF-8 characters in the string data

The _string prefix_ optionally occurs immediately before the opening quote or curly of the string.  For example:

    exp"My string"
    ae{Curly}
    "Further"

The first example is a quoted string with a prefix of `exp` and string data `My string`.  The second example is a curly string with a prefix of `ae` and string data `Curly`.  The third example is a quoted string with no string prefix and string data `Further`.

As explained in [&sect;3.3](#mds3p3), an odd-numbered sequence of backslashes before a quote in a quoted string or an odd-numbered sequence of backslashes before an opening or closing curly in a curly string is an escape that causes the quote or curly to be ignored for sake of determining where the closing quote or closing curly is in the string data.  These escaping backslashes are kept in the string data, so that the client is free to implement an escaping system on top of it &mdash; but it is the client's responsibility to handle decoding string escaping.

The reason for escaping requiring an odd-numbered sequence of backslashes is to correctly handle the common situation where two backslashes serve as an escape for a literal backslash.  For example:

    "Hello, \\world\\"

In this case, the second double quote _is_ a closing double quote, even though it is preceded by a backslash.  An even-numbered sequence of backslashes implies that each backslash in the sequence is part of an escape pair consisting of two backslashes.

String data is subject to input processing described in [&sect;2](#mds2), so line breaks within string data will always be a LF character regardless of the type of line break used in the Shastina file, the character encoding will be UTF-8 with no encoded surrogates, and there will be no nul characters anywhere in the string data.  Note, however, that Shastina does not perform any Unicode normalization.  If Unicode normalization is required, this is the client's responsibility to handle.

Empty strings are allowed by Shastina.

It is recommended that string literal entities push a string object on top of the interpreter stack (see [&sect;4](#mds4)).  Clients can choose to handle quoted and curly strings the same way, or clients can choose to make quoted and curly strings mean something different.  Prefixes can be used to extend the string literal entity type to allow for other kinds of literal data to be embedded in the Shastina file.  For example, a certain prefix might cause the string to be parsed as a calendar date.  The details of string handling are left to the client.

### 5.6 <span id="mds5p6">Load/store entities</span>

In regular mode (see [&sect;5.1](#mds5p1)), Shastina provides four kinds of load/store entities:

1. Define variable `?`
2. Define constant `@`
3. Get variable or constant `=`
4. Put variable `:`

The load/store entities are intended for sophisticated Shastina interpreters that need another method of data storage beyond the interpreter stack.  For simple Shastina clients, the load/store entities are likely unnecessary.  See [&sect;4](#mds4) for further information.

Each of the load/store entity types begins with the special symbol noted above and the rest of the entity contains the name of a constant or a variable.  Shastina simply reports the load/store entities that it encounters in the Shastina file.  It is up to the client to implement the load/store architecture if required.

It is recommended that variables and constants must be defined before they are used.  Constants differ from variables in that their value can't be changed.  Both constants and variables share the same global namespace for variables and constants.  Redeclaring a variable or constant after it has already been declared should be an error.

The "get" entity `=` should push the current value of a variable or a constant on top of the interpreter stack.  The "put" entity `:` should pop the a value off the top of the interpreter stack and store it in a variable (but not a constant!).

Certain Shastina clients might choose to have predefined variables or constants to pass data in and out of Shastina interpretation.  In this case, the variable or constant is not allowed to be declared in the Shastina file because it is predeclared by the interpreter.

### 5.7 <span id="mds5p7">Operations</span>

Operation entities occur in regular mode (see [&sect;5.1](#mds5p1)) and consist of all entities that do not fall into one of the other entity categories.  It is recommended that clients interpret operation entities by performing a specific operation named by the entity, using the stack to pass input parameters to the operator and receive output parameters from the operator.

See [&sect;4](#mds4) for further information about how operators work within the Shastina architecture.

### 5.8 <span id="mds5p8">EOF entity</span>

The special EOF entity is `|;` which stops the Shastina parser after reading the semicolon, leaving the input positioned immediately after the semicolon.  This allows other kinds of data to be present after the EOF entity, which Shastina will ignore.

There are three basic strategies for handling the EOF entity.  The simplest is to declare that nothing except for whitespace and blank lines may occur after the EOF entity.  The Shastina implementation provides a special function that can perform this check for clients.

The second strategy for handling the EOF entity is to define what extra data is stored after EOF within the Shastina data.  That way, when interpretation reaches the EOF entity, the client will know exactly what (if anything) comes after the EOF marker, and the client can then read the extra data properly.

The third strategy for handling the EOF entity is to look for something other than whitespace or blank lines after the EOF entity and start a new Shastina interpreter instance if there is more data after the EOF.  This allows multiple Shastina blocks to be sequenced one after the other in a single file, with each being interpreted separately.
