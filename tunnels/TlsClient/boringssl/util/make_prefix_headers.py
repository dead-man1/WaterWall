#!/usr/bin/env python3

from __future__ import annotations

import argparse
from pathlib import Path


C_HEADER_PREAMBLE = """// Copyright 2018 The BoringSSL Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// BORINGSSL_ADD_PREFIX pastes two identifiers into one. It performs one
// iteration of macro expansion on its arguments before pasting.
#define BORINGSSL_ADD_PREFIX(a, b) BORINGSSL_ADD_PREFIX_INNER(a, b)
#define BORINGSSL_ADD_PREFIX_INNER(a, b) a ## _ ## b

"""

ASM_HEADER_PREAMBLE = """// Copyright 2018 The BoringSSL Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#if !defined(__APPLE__)
#include <boringssl_prefix_symbols.h>
#else
// On iOS and macOS, we need to treat assembly symbols differently from other
// symbols. The linker expects symbols to be prefixed with an underscore.
// Perlasm thus generates symbol with this underscore applied. Our macros must,
// in turn, incorporate it.
#define BORINGSSL_ADD_PREFIX_MAC_ASM(a, b) BORINGSSL_ADD_PREFIX_INNER_MAC_ASM(a, b)
#define BORINGSSL_ADD_PREFIX_INNER_MAC_ASM(a, b) _ ## a ## _ ## b

"""

NASM_HEADER_PREAMBLE = """; Copyright 2018 The BoringSSL Authors
;
; Licensed under the Apache License, Version 2.0 (the "License");
; you may not use this file except in compliance with the License.
; You may obtain a copy of the License at
;
;     https://www.apache.org/licenses/LICENSE-2.0
;
; Unless required by applicable law or agreed to in writing, software
; distributed under the License is distributed on an "AS IS" BASIS,
; WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
; See the License for the specific language governing permissions and
; limitations under the License.

; 32-bit Windows adds underscores to C functions, while 64-bit Windows does not.
%ifidn __OUTPUT_FORMAT__, win32
"""


def read_symbols(path: Path) -> list[str]:
    symbols: list[str] = []
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.split("#", 1)[0].strip()
        if line:
            symbols.append(line)
    return symbols


def write_c_header(symbols: list[str], path: Path) -> None:
    with path.open("w", encoding="utf-8", newline="\n") as handle:
        handle.write(C_HEADER_PREAMBLE)
        for symbol in symbols:
            handle.write(f"#define {symbol} BORINGSSL_ADD_PREFIX(BORINGSSL_PREFIX, {symbol})\n")


def write_asm_header(symbols: list[str], path: Path) -> None:
    with path.open("w", encoding="utf-8", newline="\n") as handle:
        handle.write(ASM_HEADER_PREAMBLE)
        for symbol in symbols:
            handle.write(
                f"#define _{symbol} BORINGSSL_ADD_PREFIX_MAC_ASM(BORINGSSL_PREFIX, {symbol})\n"
            )
        handle.write("#endif\n")


def write_nasm_header(symbols: list[str], path: Path) -> None:
    with path.open("w", encoding="utf-8", newline="\n") as handle:
        handle.write(NASM_HEADER_PREAMBLE)
        for symbol in symbols:
            handle.write(f"%xdefine _{symbol} _ %+ BORINGSSL_PREFIX %+ _{symbol}\n")
        handle.write("%else\n")
        for symbol in symbols:
            handle.write(f"%xdefine {symbol} BORINGSSL_PREFIX %+ _{symbol}\n")
        handle.write("%endif\n")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("symbols", help="Path to the newline-separated symbol list")
    parser.add_argument("--out", default=".", help="Directory for generated headers")
    args = parser.parse_args()

    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    symbols = read_symbols(Path(args.symbols))

    write_c_header(symbols, out_dir / "boringssl_prefix_symbols.h")
    write_asm_header(symbols, out_dir / "boringssl_prefix_symbols_asm.h")
    write_nasm_header(symbols, out_dir / "boringssl_prefix_symbols_nasm.inc")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
