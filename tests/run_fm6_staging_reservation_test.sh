#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Host-only FM6 staging ownership acceptance; simulated NOR only.
set -eu
cd "$(dirname "$0")/.."
python3 -c 'import runpy; ns = runpy.run_path("tools/build.py"); ns["generate"]()'
mkdir -p build/cp1-fm6-reservation
cc -O2 -w -Ibuild/gen -Ifirmware/hal -Ifirmware/bsp/chip -Ifirmware/bsp/boards/fm1 -Ifirmware/src \
  -o build/cp1-fm6-reservation/fm6bank tests/fm6bank_test.c -lm
build/cp1-fm6-reservation/fm6bank
