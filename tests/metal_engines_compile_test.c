/* SPDX-License-Identifier: GPL-3.0-only
 * Compile-only check of Felucca's real engine include order, including the
 * unregistered ENERGUN METAL CORE adapter. This is not a firmware image. */
#include "felucca_tables.h"
#include "../firmware/src/core.h"
#include "../firmware/src/engines.c"
