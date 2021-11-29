.. title:: clang-tidy - bugprone-non-portable-integer-constant

bugprone-non-portable-integer-constant
======================================

Finds integer literals that might have different bit-width on different platforms.

.. code-block:: c

    const unsigned long mask = 0xFFFFFFFF; // warn
    
    unsigned long flipbits(unsigned long x) {
      return x ^ mask;
    }

.. code-block:: c++

    const unsigned long mask = 0B1000'0000'0000'0000'0000'0000'0000'0000; // warn
    unsigned long x;

    /* Initialize x */

    x |= mask;
