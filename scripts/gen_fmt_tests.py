import os
import sys
import random

def main() -> int:
    tests_dir = f"{os.path.dirname(os.path.dirname(__file__))}/tests"
    fmt_test_file = f"{tests_dir}/test_fmt.c"

    with open(fmt_test_file, "w", encoding="utf-8") as file:
        file.write("/* SPDX-License-Identifier: BSD-3-Clause */\n")
        file.write("/* Copyright (c) 2023 - Present Romain Augier */\n")
        file.write("/* All rights reserved. */\n")
        file.write("\n")
        file.write("#include \"libromano/fmt.h\"\n")
        file.write("\n")
        file.write("#include <string.h>\n")
        file.write("\n")
        file.write("int main(void)\n")
        file.write("{\n")

        # i64

        file.write(f"    ROMANO_ASSERT(fmt_size_i64(0) == 1, \"\");\n")

        for _ in range(50):
            shift = random.randint(1, 63)
            num = random.randint(-(1 << shift), 1 << shift)

            num_size = len(str(num))

            file.write(f"    ROMANO_ASSERT(fmt_size_i64({num}LL) == {num_size}, \"fmt_size_i64({num}) should be {num_size}\");\n")

        file.write("\n")
        file.write("    char i64_buffer[32];\n")
        file.write("    int i64_fmt_sz;\n")
        file.write("\n")

        for _ in range(50):
            shift = random.randint(1, 63)
            num = random.randint(-(1 << shift), 1 << shift)

            num_size = len(str(num))

            file.write(f"    i64_fmt_sz = fmt_i64(i64_buffer, {num}LL);\n")
            file.write(f"    ROMANO_ASSERT(i64_fmt_sz == {num_size}, \"i64_fmt_sz should be {num_size}\");\n")
            file.write(f"    ROMANO_ASSERT(memcmp(i64_buffer, \"{num}\", i64_fmt_sz) == 0, \"fmt_i64({num}) should be {num}\");\n")

        file.write("\n")

        # u64

        file.write(f"    ROMANO_ASSERT(fmt_size_u64(0) == 1, \"\");\n")

        for _ in range(50):
            shift = random.randint(1, 63)
            num = random.randint(1, 1 << shift)

            num_size = len(str(num))

            file.write(f"    ROMANO_ASSERT(fmt_size_u64({num}ULL) == {num_size}, \"fmt_size_u64({num}) should be {num_size}\");\n")

        file.write("\n")
        file.write("    char u64_buffer[32];\n")
        file.write("    int u64_fmt_sz;\n")
        file.write("\n")

        for _ in range(50):
            shift = random.randint(1, 63)
            num = random.randint(1, 1 << shift)

            num_size = len(str(num))

            file.write(f"    u64_fmt_sz = fmt_u64(u64_buffer, {num}ULL);\n")
            file.write(f"    ROMANO_ASSERT(u64_fmt_sz == {num_size}, \"u64_fmt_sz should be {num_size}\");\n")
            file.write(f"    ROMANO_ASSERT(memcmp(u64_buffer, \"{num}\", u64_fmt_sz) == 0, \"fmt_u64({num}) should be {num}\");\n")

        file.write("\n")

        # f64

        for i in range(1, 18):
            file.write(f"    ROMANO_ASSERT(fmt_size_f64(0.0, {i}) == {i + 2}, \"fmt_size_f64(0.0, {i}) should be {i + 2}\");\n")

        file.write("\n")
        file.write("    char f64_buffer[312];\n")
        file.write("    int f64_fmt_sz;\n")
        file.write("\n")

        for _ in range(100):
            shift = random.randint(1, 63)
            base = random.randint(0, (1 << shift))
            precision = random.randint(1, 15)
            num = base + random.random()

            num_size = len(f"{num:.{precision}f}")

            file.write(f"    f64_fmt_sz = fmt_f64(f64_buffer, {num:.{precision}f}, {precision});\n")
            file.write(f"    ROMANO_ASSERT(f64_fmt_sz == {num_size}, \"f64_fmt_sz should be {num_size}\");\n")
            file.write(f"    ROMANO_ASSERT(memcmp(f64_buffer, \"{num:.{precision}f}\", f64_fmt_sz) == 0, \"fmt_f64({num}) should be {num}\");\n")

        file.write("\n")

        file.write("    return 0;\n")
        file.write("}\n")

    return 0

if __name__ == "__main__":
    sys.exit(main())