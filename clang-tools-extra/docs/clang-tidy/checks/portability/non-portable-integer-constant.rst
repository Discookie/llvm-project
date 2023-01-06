.. title:: clang-tidy - bugprone-non-portable-integer-constant

bugprone-non-portable-integer-constant
======================================

Finds integer literals that might have different bit-widths on different platforms.

Currently the check detects cases where maximum or minimum values should be used
instead, as well as error-prone integer literals where all bits are defined.

This check corresponds to CERT C Coding Standard rule `INT17-C. Define integer 
constants in an implementation-independent manner
<https://wiki.sei.cmu.edu/confluence/display/c/INT17-C.+Define+integer+constants+in+an+implementation-independent+manner>`_.

.. code-block:: c

    const unsigned long mask = 0xFFFFFFFF; // warn
    // the right way to write this would be ULONG_MAX, or -1
    
    unsigned long flipbits(unsigned long x) {
      return x ^ mask;
    }

.. code-block:: c++

    const unsigned long mask = 0B1000'0000'0000'0000'0000'0000'0000'0000; // warn
    // a correct way to write this would be ~(ULONG_MAX >> 1)
    unsigned long x;

    /* Initialize x */

    x |= mask;
