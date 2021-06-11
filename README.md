# Neolithic
 Neolithic 6502 Cross-Compiler

The Neolithic Compiler is cross-compiler that targets the 6502.
The Neolithic language is very similar to C.  It's been simplified
to help with generating optimal 6502 assembly code.


**Disclaimer for Alpha release:**

>    The compiler and language itself are currently in active
>    development and are subject to change at any time.  While
>    some things may work perfectly fine in one version, they
>    could potentially be changed/removed in the future.  Please
>    keep this in mind when writing code targeting this compiler.


---------------
## Quick Intro


This compiler supports many C-like constructs including variables, functions,
structures, unions, and enumerations.  Most of the basic expression and
assignment syntax falls in-line with how C works, albeit, simplified.
Even the basic structured code elements such as IF, WHILE, DO, and FOR
statements generally work the same.

So, what are the differences?

Well, in general, the language has been simplified.  Primitive types
have been reduced to just 8-bit chars, 16-bit ints, booleans, and pointers.
Arrays are one-dimensional.

User-defined types still exist, but only consist of structs, unions, and
enumerations; general typedefs have been eliminated.  This has been done
to simplify code generation.  Structs and unions have been "promoted" to
being the primary ways of defining new types.  In Neolithic, they are
handled as typedefs without the need to use the C keyword "typedef".

Another minor difference is that semicolons are optional.  They can still
be used, but for the most part, they are not necessary.  In fact, I have
yet to come upon a case in which they are needed.  Though I do believe
someone might still stumble upon a place where semicolons will be needed.


-------
## Limitations

- Currently there is minimal 16-bit support, enough to support pointers, but
not enough to handle much of anything else.

- There really is no type checking at this point.

- Syntax is still in flux.

-----

For more information, please read 'Neolithic Language Guide' in the /docs/ directory.


> The book "The C Programming Language: Second Edition" by Brian W. Kernighan
and Dennis M. Ritchie was used as a reference for the design of this language.


