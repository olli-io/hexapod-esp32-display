#include <unity.h>

#include <string.h>

#include "Crc16.h"

using proto::Crc16;

void setUp() {}
void tearDown() {}

// Canonical CRC-16/CCITT-FALSE vector.
void test_crc_check_vector(void) {
    const char* s = "123456789";
    const uint16_t crc = Crc16::compute(reinterpret_cast<const uint8_t*>(s), 9);
    TEST_ASSERT_EQUAL_HEX16(0x29B1, crc);
}

void test_crc_empty_is_init(void) {
    Crc16 c;
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, c.value());
}

void test_crc_incremental_matches_compute(void) {
    const uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0xFF, 0xA5, 0x5A};
    const uint16_t batched = Crc16::compute(data, sizeof(data));

    Crc16 c;
    for (uint8_t b : data) c.update(b);
    TEST_ASSERT_EQUAL_HEX16(batched, c.value());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_crc_check_vector);
    RUN_TEST(test_crc_empty_is_init);
    RUN_TEST(test_crc_incremental_matches_compute);
    return UNITY_END();
}
