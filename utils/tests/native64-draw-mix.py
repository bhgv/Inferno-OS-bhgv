#!/usr/bin/env python3
"""Independent mathematical periodic-coordinate oracle for the native Draw helper."""
import random


# Python's unbounded subtraction and positive-period modulo avoid copying C overflow behavior.
def rows():
    rng = random.Random(116)
    values = [(0, 10, -1), (0, 0, 7), (9, -4, 15), (-2147483648, 2147483647, 2147483647),
              (-2147483648, -2147483647, 2147483647), (2147483646, 2147483647, -2147483648)]
    for _ in range(1000):
        low, high, point = (rng.randrange(-2147483648, 2147483648) for _ in range(3))
        values.append((low, high, point))
    output = ['typedef struct ReplCase ReplCase;\n\nstruct ReplCase {\n'
              ' int low; // Inclusive interval boundary.\n int high; // Exclusive interval boundary.\n'
              ' int point; // Arbitrary signed input coordinate.\n int expected; // Unbounded independent result.\n};\n'
              '\nstatic ReplCase replcases[] = {']
    for low, high, point in values:
        result = low+(point-low) % (high-low) if high > low else low
        output.append('{'+','.join(map(str, (low, high, point, result)))+'},')
    return '\n'.join(output)+'\n};\n'
