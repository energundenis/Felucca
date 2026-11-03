/* SPDX-License-Identifier: GPL-3.0-only
 * Host acceptance for typed Sidecar V1 logical payload codecs.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "energun_sidecar_payload_v1.h"

static int check(const char *label, int ok)
{
    printf("%-58s %s\n", label, ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

int main(void)
{
    energun_sc_project_payload_v1_t project, decoded_project;
    energun_sc_preset_payload_v1_t preset, decoded_preset;
    uint8_t bytes[ENERGUN_SC_V1_PROJECT_MAX_PAYLOAD];
    uint8_t preset_bytes[ENERGUN_SC_V1_PRESET_PAYLOAD];
    size_t length = 0u;
    unsigned i, bad = 0;
    memset(&project, 0, sizeof project);
    memset(&decoded_project, 0xA5, sizeof decoded_project);
    for (i = 0; i < 64u; ++i)
        ((uint8_t *)&project.p_mc[0][0])[i] = (uint8_t)(i & 0x7Fu);
    for (i = 0; i < 52u; ++i)
        ((uint8_t *)&project.private_values[0][0])[i] = (uint8_t)((i + 64u) & 0x7Fu);
    project.motion_count = ENERGUN_SC_V1_PROJECT_MAX_MOTION;
    for (i = 0; i < 128u * 3u; ++i)
        ((uint8_t *)project.motion)[i] = (uint8_t)(i * 13u);
    bad += check("project payload encodes full 501-byte maximum",
        energun_sc_project_payload_v1_encode(bytes, sizeof bytes, &length, &project) == ENERGUN_SC_OK &&
        length == ENERGUN_SC_V1_PROJECT_MAX_PAYLOAD);
    bad += check("project payload decode roundtrip all fields",
        energun_sc_project_payload_v1_decode(bytes, length, &decoded_project) == ENERGUN_SC_OK &&
        !memcmp(&project, &decoded_project, sizeof project));
    bad += check("project decode rejects truncated motion tail",
        energun_sc_project_payload_v1_decode(bytes, length - 1u, &decoded_project) == ENERGUN_SC_ERR_LENGTH);
    bad += check("project encode rejects insufficient output capacity",
        energun_sc_project_payload_v1_encode(bytes, 500u, &length, &project) == ENERGUN_SC_ERR_LENGTH &&
        length == 0u);

    memset(&preset, 0, sizeof preset);
    for (i = 0; i < sizeof preset; ++i)
        ((uint8_t *)&preset)[i] = (uint8_t)(i * 7u);
    bad += check("preset payload encodes exact 210-byte bank",
        energun_sc_preset_payload_v1_encode(preset_bytes, sizeof preset_bytes, &length, &preset) == ENERGUN_SC_OK &&
        length == ENERGUN_SC_V1_PRESET_PAYLOAD);
    bad += check("preset payload decode roundtrip all slots",
        energun_sc_preset_payload_v1_decode(preset_bytes, sizeof preset_bytes, &decoded_preset) == ENERGUN_SC_OK &&
        !memcmp(&preset, &decoded_preset, sizeof preset));
    bad += check("preset decode rejects wrong payload size",
        energun_sc_preset_payload_v1_decode(preset_bytes, sizeof preset_bytes - 1u, &decoded_preset) ==
            ENERGUN_SC_ERR_LENGTH);
    printf("%s\n", bad ? "SIDECAR PAYLOAD TEST FAILED" : "SIDECAR PAYLOAD TEST PASSED");
    return bad ? 1 : 0;
}
