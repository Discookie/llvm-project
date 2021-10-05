.. title:: clang-tidy - bugprone-non-portable-integer-constant

bugprone-non-portable-integer-constant
======================================

Finds masks that are being used in a non-portable manner.

.. code-block:: c

    const unsigned long mask = 0xFFFFFFFF;
    
    unsigned long flipbits(unsigned long x) {
      return x ^ mask;
    }
