#!/bin/sh
set -eu
cc -std=c11 -Wall -Wextra -Werror -pedantic -Ifirmware/src \
  firmware/src/energun_sidecar_flash.c tests/sidecar_flash_backend_test.c \
  -o /tmp/energun-sidecar-flash-backend-test
/tmp/energun-sidecar-flash-backend-test
