#import "../bookmaster/bookmaster.typ": *

// Narrow cells: ragged right.
#show table: set par(justify: false)

#let xlate = read("../ex/pg-charset/xlate.c").split("\n")

= Character Sets <pg-charset>

#idx("character set")#idx("EBCDIC")#idx("code page 037")
A C program built with cc370 runs in EBCDIC, the character code of MVS,
in the form of code page 037 (CP037). The source of the program is written
on a workstation in ASCII, Latin-1 or UTF-8, and the compiler translates
every character constant and string literal; at run time the program sees
only EBCDIC. Most C code works unchanged. This chapter describes where it
does not: the newline character, numeric character codes, the order of the
letters, the classification functions, and the translation that data needs
when it leaves MVS or comes from elsewhere.

== Characters in the Program <pg-charset-program>

#idx("character constant", "value")
cc370 translates character constants and string literals to CP037 when it
compiles them. #cmd("'A'") is 193, #cmd("X'C1'"), in the program, and the
preprocessor agrees: #cmd("#if 'A' == 193") is true. So:

- *Write characters as character constants.* #cmd("c == 'A'") is right on
  every system; #cmd("c == 0x41") is #cmd("'A'") only in ASCII, and on MVS
  it compares with an unassigned code.
- *A numeric escape is not translated.* #cmd("'\\x41'") and #cmd("\"\\101\"")
  stand for the byte with that value, X'41', not for the letter #cmd("A").
  Use them for binary data, never to spell a character.
- *Only Latin-1 can be written in a literal.* The source may be in UTF-8 or
  Latin-1, but a character constant or string literal can hold only the
  characters up to U+00FF, which CP037 contains. #cmd("€") is a compile-time
  error. Comments may contain anything.

#idx("newline", "X'15'")#idx("NEL")
*The newline is X'15'.* #cmd("'\\n'") is translated to X'15', the EBCDIC
new-line character (NEL), not to X'25', the line feed of CP037. X'15' is the
newline that the library and the programs around it agree on: the stream
functions end a record at it and return it at the end of a record
(@pg-io-records), and the servers that move text between MVS and the
network translate it to and from the line feed of ASCII. Two consequences:

- Data that contains X'15' as an ordinary byte breaks into lines when it is
  read as a text stream. Read such data as a binary or record stream.
- Text that marks the ends of its lines with X'25', the line feed of CP037,
  is a single line to the C library: X'25' is an ordinary character.

The other control characters have their CP037 values: #cmd("'\\t'") is
X'05', #cmd("'\\r'") X'0D', #cmd("'\\f'") X'0C'.

== Letters, Digits and Order <pg-charset-order>

#idx("EBCDIC", "letters")#idx("ctype.h", "in EBCDIC")
The letters of EBCDIC are not one block. The lowercase letters are in three
groups, #cmd("a")--#cmd("i") at X'81'--X'89', #cmd("j")--#cmd("r") at
X'91'--X'99' and #cmd("s")--#cmd("z") at X'A2'--X'A9', and the uppercase
letters likewise at X'C1'--X'C9', X'D1'--X'D9' and X'E2'--X'E9'. The gaps
hold other characters. Code that relies on ASCII's contiguous alphabet
goes wrong without a warning:

- #cmd("c >= 'a' && c <= 'z'") is also true for #cmd("~") and 14 other
  characters between the groups. @pg-charset-letters-fig counts the letters
  of #cmd("\"ab~z\"") both ways and prints #cmd("range test 4, isalpha 3").
- #cmd("c - 'a'") is not the position of a letter in the alphabet:
  #cmd("'j' - 'a'") is 16, not 9.
- #cmd("c | 0x20") and #cmd("c - 32") do not change case. Use
  #cmd("toupper()") and #cmd("tolower()").

The digits are contiguous, X'F0' to X'F9', so #cmd("c - '0'") is the value
of a digit, as C requires everywhere.

#fig(caption: [A range test and isalpha() disagree])[
  #code(read("../ex/pg-charset/letters.c"), numbers: true)
] <pg-charset-letters-fig>

#idx("collating sequence")#idx("strcmp", "order")
*The order is not ASCII's.* #cmd("strcmp()"), #cmd("memcmp()") and
#cmd("qsort()") with them compare EBCDIC codes: the blank and most
punctuation come first, then the lowercase letters, then the uppercase
letters, then the digits. A sorted list on MVS puts #cmd("\"abc\"") before
#cmd("\"ABC\"") and both before #cmd("\"123\"") -- the reverse of what the
same program does on a workstation. That is the order MVS users expect from
their own utilities. A program that has to produce ASCII order sorts with a
comparison function that translates the bytes first (@pg-charset-xlate).

#idx("ctype.h", "national characters")
The classification functions of #cmd("<ctype.h>") use a table for CP037 and
are correct for letters, digits and white space. Two limits remain:

- Only the 26 letters of the Latin alphabet are letters. The accented
  letters of CP037, such as #cmd("é"), belong to no class, and
  #cmd("toupper()") and #cmd("tolower()") leave them alone.
- *#cmd("[") , #cmd("]") and #cmd("^") are not classified.*
  #cmd("ispunct()"), #cmd("isgraph()") and #cmd("isprint()") return 0 for
  them, because the table has the brackets at the positions code page 1047
  gives them. A program that must treat these three as printable
  punctuation -- a tokenizer, a filter for printable data -- tests for them
  itself:

```
if (ispunct(c) || c == '[' || c == ']' || c == '^')
```

== Translating Data <pg-charset-xlate>

#idx("ASCII", "translating to")#idx("translation table")
Data that leaves MVS, or arrives from outside, is usually in ASCII or one of
its extensions. The library translates nothing on its own: a stream
writes the bytes the program gives it, and #cmd("send()") and
#cmd("recv()") move bytes unchanged (@pg-sockets). Where translation is
needed, the program does it with a table of its own. Text needs it; binary
data -- the integers of a protocol header, a compressed file, an image --
must not be translated.

#idx("file transfer", "and code pages")
Whether data in a data set is EBCDIC or ASCII depends on how it got there.
A text transfer, by FTP or by a file-transfer server, translates; a binary
transfer keeps the bytes of the workstation. A
program that reads data uploaded in binary from a workstation reads ASCII
and translates it itself.

A translation table maps each of the 256 byte values to another. The
tables in @pg-charset-e2a-fig and @pg-charset-a2e-fig translate between
CP037 and ISO 8859-1 (Latin-1), the code that agrees with ASCII in its first
128 characters. They are the CP037 tables with one change, the one the
newline requires: X'15' translates to the line feed X'0A', and X'25' to the
character X'85' that would otherwise have been X'15'. So a text that ends
its lines with #cmd("'\\n'") arrives with line feeds, and the two tables
remain exact inverses of each other: no byte is lost in either direction.
The table from Latin-1 to CP037 is the one cc370 uses for the literals of a
program, except that it sends X'85' to X'25', where cc370 also uses X'15'.

To translate text for the network or for a workstation:

+ Put the two tables and the functions of @pg-charset-func-fig into a
  source file of the program. The tables are #cmd("static const"), so a
  reentrant program can use them (@pg-rent).
+ Translate an outgoing buffer with #cmd("to_ascii()") just before it is
  sent, and an incoming buffer with #cmd("to_ebcdic()") just after it is
  received. Translate in place, or into a buffer of the same length: every
  character remains one byte.
+ Keep the data in EBCDIC everywhere else in the program, so that
  #cmd("printf()"), #cmd("strcmp()") and the #cmd("<ctype.h>") functions
  see what they expect.

#fig(caption: [Translation table from CP037 to ISO 8859-1])[
  #code(xlate.slice(0, 40).join("\n"))
] <pg-charset-e2a-fig>

#fig(caption: [Translation table from ISO 8859-1 to CP037])[
  #code(xlate.slice(41, 75).join("\n"))
] <pg-charset-a2e-fig>

#fig(caption: [Translating a buffer])[
  #code(xlate.slice(76).join("\n"))
] <pg-charset-func-fig>

The #cmd("main()") function translates #cmd("\"Hello, [world]\\n\"") to
Latin-1 and prints the bytes,
#cmd("48 65 6C 6C 6F 2C 20 5B 77 6F 72 6C 64 5D 0A"), the same bytes the
string has in ASCII, and then translates them back and prints the string
itself.

#fig(caption: [Output of the translation example])[_Output to be captured on
MVS._] <pg-charset-xlate-out>

#note[Many peers expect UTF-8, not Latin-1. The first 128 characters are the
same in both, so text that holds only those -- the letters without accents,
the digits, the punctuation of ASCII -- needs nothing more. A character
above X'7F' in Latin-1 is two bytes in UTF-8, and the program must encode
it.]
