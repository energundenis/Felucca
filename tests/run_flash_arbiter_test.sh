#!/bin/sh
set -eu
cc -std=c11 -Wall -Wextra -Werror -pedantic -Ifirmware/src \
  firmware/src/energun_flash_arbiter.c tests/flash_arbiter_test.c \
  -o /tmp/energun-flash-arbiter-test
/tmp/energun-flash-arbiter-test
