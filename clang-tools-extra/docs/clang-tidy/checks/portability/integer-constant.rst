.. title:: clang-tidy - portability-integer-constant

portability-integer-constant
============================

`cert-int17-c` redirects here as an alias for this check.

Finds integer literals that are being used in a non-portable manner.

Currently the check detects cases where maximum or minimum values should be used
instead, as well as error-prone integer literals having leading zeroes, or
relying on the most significant bits.

This check corresponds to CERT C Coding Standard rule `INT17-C. Define integer 
constants in an implementation-independent manner
<https://wiki.sei.cmu.edu/confluence/display/c/INT17-C.+Define+integer+constants+in+an+implementation-independent+manner>`_.

.. code-block:: c
    
    unsigned long flip_bits(unsigned long x) {
      return x ^ 0xFFFFFFFF;
      // The right way to write this would be ULONG_MAX, or -1.
    }

.. code-block:: c++

    const unsigned long mask = 0b1000'0000'0000'0000'0000'0000'0000'0000; // warn
    // The right way to write this would be ~(ULONG_MAX >> 1).
    unsigned long x;

    x &= mask;

.. code-block:: c++

    const unsigned long bit_to_set = 0x00010000; // warn
    // Incorrectly assumes that long is 4 bytes.

    unsigned long set_bit(unsigned long x) {
      return x | bit_to_set;
    }
