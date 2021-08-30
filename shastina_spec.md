# Shastina Specification

__NOTE: This partial specification was written against the 0.9.0 beta version.  It does not match 0.9.2 beta.  And it's incomplete.  Plain-text formatting has been updated to MarkDown.__

## 1. Input processing

Shastina files may be encoded in US-ASCII or any superset thereof.  UTF-8, for example, is acceptable for Shastina files because US-ASCII is a subset of UTF-8.  However, UTF-16 is unacceptable because the 16-bit characters of UTF-16 are incompatible with the 8-bit characters of US-ASCII.

The following points clarify Shastina's use of US-ASCII and support for other character sets:

(1) Shastina's preferred line breaking style is LF-only.  However, Shastina will automatically convert CR-only, CR+LF, and LF+CR line breaks into LF-only line breaks, so any of those line breaking styles can also be used.  Line break conversion is applied to string literals (see later), such that quoted strings and curly strings will have all internal line breaks converted to LF.  However, line break conversion is *not* applied to embedded data (see later), such that embedded data might have line breaks in any of these styles.

(2) Shastina has special handling for UTF-8 Byte Order Marks (BOMs) at the start of the input file.  If the first byte of the file has value 0xEF, then the file must be at least three bytes long and the first three bytes must have the values `0xEF 0xBB 0xBF` or a "bad signature" error will occur.  When these three bytes (corresponding to a UTF-8 BOM) are present at the start of the file, Shastina will filter the bytes out and set a "BOM Present" flag, so that clients can detect whether a UTF-8 BOM was present at the start of the file, which may be meaningful for clients that support UTF-8.

(3) Shastina ignores data within comments, so characters beyond US-ASCII can safely be used in comments.

(4) Shastina passes string literal data through to the client, so clients may use UTF-8, ISO-8559-1, and other character sets within string literals, so long as US-ASCII is a subset of the character set.

(5) Shastina allows clients to handle reading embedded data from the file, so anything may be included in embedded data, including raw binary data and character sets that are not compatible with US-ASCII, such as UTF-16.

Therefore, although Shastina only directly supports US-ASCII, clients can easily add support for UTF-8 and various other character sets and encodings, by taking advantage of Shastina's pass-through data support for string literals and embedded data.

## 2. Token parsing

After input processing, the next step is to parse the text data into tokens.  There are three kinds of tokens:

(1) *Simple tokens* are a sequence of one or more visible US-ASCII characters.

(2) *String tokens* include a prefix sequence of zero or more visible US-ASCII characters, a type flag which selects either a quoted string or a curly string, and zero or more bytes of string literal data.

(3) *Embedded tokens* include a prefix sequence of zero or more visible US-ASCII characters and the embedded data.  Unlike string tokens, which have already read the string data into memory, embedded tokens let the client read the embedded data directly from the Shastina source file.  This gives clients more flexibility in reading the embedded data, at the expense of added complexity for clients.

The following subsections discuss these token types in further detail.

### 2.1 Simple tokens

Shastina reads tokens according to the following method:

(1) Zero or more bytes of whitespace and comments (see below) are skipped until the first visible, non-comment US-ASCII character is encountered.

(2) If the first visible, non-comment US-ASCII character is an atomic character (see below), then the token consists just of that character.

(3) If the first visible, non-comment US-ASCII character is a vertical bar, and this is followed immediately by a semicolon, then the token consists the vertical bar followed by the semicolon.

(4) If the first visible, non-comment US-ASCII character is not an atomic character, and not a bar-semicolon token, then the token consists of the first character and all legal US-ASCII characters that immediately follow it, up to the first exclusive character or inclusive character that is encountered.  If the token ends with an exclusive character, the exclusive character is *not* included in the token.  If the token ends with an inclusive character, the inclusive character *is* included in the token.

The special `|;` token is always the last token in the file.  Only whitespace and comments may follow it.  It is an error to encounter End Of File before the `|;` token has been read.  It is an error if anything but whitespace and comments follow `|;`

Whitespace consists of Horizontal Tab (HT), Space (SP), and Line Feed (LF).  Input processing (see section 1) has already converted CR, CR+LF, and LF+CR line breaks into LF-only line breaks.  Comments begin with a pound sign (`#`) and proceed up to the next LF or the End Of File (whichever occurs first).

Atomic characters are the following:

    ( ) [ ] , % ; ` " { }

Exclusive characters are the following:

    HT SP LF ( ) [ ] , % ; # }

Inclusive characters are the following:

    ` " {

The range of input characters (after input processing) is limited to the visible US-ASCII characters, SP, HT, LF.  It is an error to encounter other characters, except within comments, string data, and embedded data.

If the last character of the token that was read is a double quote or a left curly bracket, then the token is a string token (see section 2.2).  If the last character of the token that was read is a grave accent, then the token is an embedded token (see section 2.3).  Otherwise, the token is a simple token.

### 2.2 String tokens

When Shastina reads a token (according to the rules in section 2.1) for which the last character is either a double quote or a left curly bracket, the token is a string token.  The zero or more visible US-ASCII characters that precede the double quote or the left curly bracket are the prefix sequence.  The double quote or the left curly bracket determines whether a quoted or curly string is present.

In order to read a quoted string, Shastina uses the following method.  The string data consists of the zero or more bytes that immediately follow the opening double quote, up to but excluding the terminating double quote.  The terminating double quote is the first double quote after the opening double quote that is not immediately preceded by a backslash.  Shastina reads all bytes of string data as-is, except that CR-only, CR+LF, and LF+CR line breaks are converted to LF-only line breaks.  Note that embedded double quotes that are escaped with a preceding backslash are left in the string data as-is, without removing the preceding backslash.

In order to read a curly string, Shastina uses the following method.  Left curly brackets and right curly brackets that are not immediately preceded by a backslash are *balanced curlies.*  Left and right curly brackets that are immediately preceded by a backslash are *escaped curlies.*  The *nesting level* starts out at one immediately after the opening left curly bracket.  Each time a balanced left curly is read, the nesting level is increased by one.  Each time a balanced right curly is read, the nesting level is decreased by one.  The balanced right curly that causes the nesting level to decrease to zero is the *closing curly.*  The curly string data includes the zero or more bytes of data that immediately follow the opening left curly bracket, up to but excluding the closing curly.  Shastina reads all bytes of string data as-is, except that CR-only, CR+LF, and LF+CR line breaks are converted to LF-only line breaks.  Balanced and escaped curlies are left in the data as-is.

Shastina has an internal limit on the maximum string length.  An error will occur for strings that are longer than this.  Use embedded data (see later) for embedding large quantities of data.

Shastina has an internal limit on the maximum nesting level within curly strings.  This limit is greater than two billion, so it shouldn't normally be an issue.

For clients that support UTF-8 and other character sets, the BOM Present flag may be of interest &mdash; see section 1.

### 2.3 Embedded tokens

When Shastina reads a token (according to the rules in section 2.1) for which the last character is a grave accent, the token is an embedded token.  The zero or more visible US-ASCII characters that precede the grave accent are the prefix sequence.

After the embedded token is read, the Shastina input file will be positioned such that the next byte that is read is the byte immediately after the grave accent.  It is up to the client to read the embedded data directly from the file, and to determine where it ends.  After the embedded data is read, clients should update Shastina's line number count if possible, so that error messages have an accurate line location.

Since clients handle reading the embedded data directly from the file, Shastina imposes no limits on the length of data or the content of data.  This is entirely up to the client to decide.  This also means that line breaks are *not* converted, so clients may encounter CR-only, LF-only, CR+LF, or LF+CR line breaks -- whichever style was used in the original file.

Raw binary data may also be used within embedded data.  The meaning of Shastina's line count is unclear in this case since the data is no longer purely text.  Clients could just set Shastina's line count to the special value of `LONG_MAX`, indicating that the line numbers are no longer valid.  Alternatively, clients could treat all the embedded binary data as a single line of text, or use some kind of other convention.

For clients that support UTF-8 and other character sets, the BOM Present flag may be of interest &mdash; see section 1.

## 3. Token interpretation

After the input has been parsed into tokens (see section 2), the final step is token interpretation.

    ... @@TODO: meta mode, embedded data not allowed
