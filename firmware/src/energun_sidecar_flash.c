/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — bounded Sidecar A/B Flash backend.
 *
 * Outer record: 32-byte commit header at sector offset 0, payload at +256.
 * Header is programmed last. The final 256 bytes of each sector are never
 * read as payload, programmed or used for commit data (loader update-record
 * exclusion). Only the two exact candidate sector bases are accepted.
 */
#include "energun_sidecar_flash.h"

#define SCF_MAGIC 0x31435345u /* "ESC1" */
#define SCF_VERSION 1u


static void scf_fill(void *destination, uint8_t value, size_t length)
{
    uint8_t *p = (uint8_t *)destination;
    while (length-- != 0u)
        *p++ = value;
}

static int scf_equal(const void *left, const void *right, size_t length)
{
    const uint8_t *a = (const uint8_t *)left;
    const uint8_t *b = (const uint8_t *)right;
    while (length-- != 0u) {
        if (*a++ != *b++)
            return 0;
    }
    return 1;
}

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t identity_kind;
    uint8_t identity_index;
    uint32_t sequence;
    uint32_t length;
    uint32_t payload_crc;
    uint32_t reserved[2];
    uint32_t header_crc;
} scf_header_t;
_Static_assert(sizeof(scf_header_t) == 32u, "Sidecar outer header layout");

static uint32_t scf_crc32(const void *data, size_t length)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFu;
    while (length--) {
        unsigned int bit;
        crc ^= *p++;
        for (bit = 0; bit < 8u; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)-(int32_t)(crc & 1u));
    }
    return ~crc;
}

static int scf_sector(uint8_t copy, uint32_t *base)
{
    if (base == 0 || copy > 1u)
        return -1;
    *base = copy == 0u ? ENERGUN_SC_FLASH_A : ENERGUN_SC_FLASH_B;
    return 0;
}

static int scf_range(uint32_t base, uint32_t offset, size_t length,
                     uint32_t limit)
{
    if ((base != ENERGUN_SC_FLASH_A && base != ENERGUN_SC_FLASH_B) ||
        offset > limit || length > (size_t)(limit - offset))
        return 0;
    return 1;
}

static int scf_read_header(energun_sc_flash_t *flash, uint8_t copy,
                           scf_header_t *header, uint32_t *base_out)
{
    uint32_t base;
    if (flash == 0 || flash->io == 0 || flash->io->read == 0 ||
        header == 0 || scf_sector(copy, &base) != 0 ||
        !scf_range(base, 0u, sizeof *header, ENERGUN_SC_FLASH_TAIL_OFF) ||
        flash->io->read(flash->io_context, base, header, sizeof *header) != 0)
        return -1;
    if (header->magic != SCF_MAGIC || header->version != SCF_VERSION ||
        header->identity_kind < ENERGUN_SC_ID_PROJECT ||
        header->identity_kind > ENERGUN_SC_ID_PRESET_BANK ||
        header->length == 0u || header->length > ENERGUN_SC_FLASH_MAX_RECORD ||
        header->reserved[0] != 0xFFFFFFFFu ||
        header->reserved[1] != 0xFFFFFFFFu ||
        header->header_crc != scf_crc32(header, sizeof *header - 4u))
        return -1;
    if (base_out != 0)
        *base_out = base;
    return 0;
}

int energun_sc_flash_init(energun_sc_flash_t *flash,
                          const energun_sc_flash_io_t *io, void *context)
{
    if (flash == 0 || io == 0 || io->read == 0 ||
        io->erase4k == 0 || io->program_page == 0)
        return -1;
    flash->io = io;
    flash->io_context = context;
    return 0;
}

int energun_sc_flash_read_copy(void *opaque, energun_sc_identity_kind_t kind,
                               uint8_t index, uint8_t copy, uint8_t *record,
                               size_t capacity, size_t *length,
                               uint32_t *sequence)
{
    energun_sc_flash_t *flash = (energun_sc_flash_t *)opaque;
    scf_header_t header;
    uint32_t base;
    if (length == 0 || sequence == 0 || record == 0 ||
        scf_read_header(flash, copy, &header, &base) != 0 ||
        header.identity_kind != (uint8_t)kind ||
        header.identity_index != index || header.length > capacity ||
        !scf_range(base, ENERGUN_SC_FLASH_PAYLOAD_OFF, header.length,
                   ENERGUN_SC_FLASH_TAIL_OFF) ||
        flash->io->read(flash->io_context,
            base + ENERGUN_SC_FLASH_PAYLOAD_OFF, record, header.length) != 0 ||
        scf_crc32(record, header.length) != header.payload_crc)
        return -1;
    *length = header.length;
    *sequence = header.sequence;
    return 0;
}

static int scf_program(energun_sc_flash_t *flash, uint32_t base,
                       uint32_t offset, const uint8_t *data, size_t length)
{
    while (length != 0u) {
        size_t page_left = ENERGUN_SC_FLASH_PAGE_SIZE -
            ((base + offset) & (ENERGUN_SC_FLASH_PAGE_SIZE - 1u));
        size_t chunk = length < page_left ? length : page_left;
        if (!scf_range(base, offset, chunk, ENERGUN_SC_FLASH_TAIL_OFF) ||
            flash->io->program_page(flash->io_context, base + offset,
                                    data, chunk) != 0)
            return -1;
        offset += (uint32_t)chunk;
        data += chunk;
        length -= chunk;
    }
    return 0;
}

int energun_sc_flash_write_copy(void *opaque, energun_sc_identity_kind_t kind,
                                uint8_t index, uint8_t copy,
                                const uint8_t *record, size_t length)
{
    energun_sc_flash_t *flash = (energun_sc_flash_t *)opaque;
    scf_header_t header;
    uint8_t verify[ENERGUN_SC_FLASH_MAX_RECORD];
    size_t verify_length = 0u;
    uint32_t verify_sequence = 0u, base;
    if (flash == 0 || flash->io == 0 || record == 0 || length == 0u ||
        length > ENERGUN_SC_FLASH_MAX_RECORD ||
        kind < ENERGUN_SC_ID_PROJECT || kind > ENERGUN_SC_ID_PRESET_BANK ||
        scf_sector(copy, &base) != 0 ||
        !scf_range(base, ENERGUN_SC_FLASH_PAYLOAD_OFF, length,
                   ENERGUN_SC_FLASH_TAIL_OFF))
        return -1;

    /* Exact-sector erase only; transport must independently enforce this too. */
    if (flash->io->erase4k(flash->io_context, base) != 0)
        return -1;
    if (scf_program(flash, base, ENERGUN_SC_FLASH_PAYLOAD_OFF,
                    record, length) != 0)
        return -1;

    scf_fill(&header, 0xFFu, sizeof header);
    header.magic = SCF_MAGIC;
    header.version = SCF_VERSION;
    header.identity_kind = (uint8_t)kind;
    header.identity_index = index;
    header.sequence = 1u;
    {
        scf_header_t old;
        uint32_t old_base;
        if (scf_read_header(flash, (uint8_t)(copy ^ 1u), &old, &old_base) == 0 &&
            old.identity_kind == (uint8_t)kind &&
            old.identity_index == index)
            header.sequence = old.sequence + 1u;
    }
    header.length = (uint32_t)length;
    header.payload_crc = scf_crc32(record, length);
    header.header_crc = scf_crc32(&header, sizeof header - 4u);

    /* Commit header last, after the payload is fully programmed. */
    if (scf_program(flash, base, 0u, (const uint8_t *)&header,
                    sizeof header) != 0)
        return -1;
    if (energun_sc_flash_read_copy(flash, kind, index, copy, verify,
            sizeof verify, &verify_length, &verify_sequence) != 0 ||
        verify_length != length || !scf_equal(verify, record, length) ||
        verify_sequence != header.sequence)
        return -1;
    return 0;
}

const energun_sidecar_storage_ops_t energun_sc_flash_storage_ops = {
    energun_sc_flash_read_copy,
    energun_sc_flash_write_copy
};
