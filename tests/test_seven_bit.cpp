#include "volsa2/seven_bit.hpp"
#include <cassert>
#include <iostream>
#include <random>
#include <vector>

void test_take_nth_msb() {
    volsa2::U7 u1(0b00000001);
    assert(u1.take_nth_msb(0) == 0b10000000);
    assert(u1.take_nth_msb(1) == 0);

    volsa2::U7 u2(0b00000010);
    assert(u2.take_nth_msb(1) == 0b10000000);
    assert(u2.take_nth_msb(0) == 0);

    volsa2::U7 u3(0b01000000);
    assert(u3.take_nth_msb(6) == 0b10000000);
    assert(u3.take_nth_msb(5) == 0);

    std::cout << "[PASS] test_take_nth_msb\n";
}

void test_round_trip() {
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 255);

    std::vector<size_t> test_lengths = {
        0, 1, 2, 6, 7, 8, 14, 15, 16, 23, 24, 31, 32, 33, 100, 255, 1024, 4096, 65536
    };

    for (size_t len : test_lengths) {
        std::vector<uint8_t> original(len);
        for (size_t i = 0; i < len; ++i) {
            original[i] = static_cast<uint8_t>(dist(rng));
        }

        size_t expected_u7_len = volsa2::U8ToU7::convert_len(len);
        auto u7 = volsa2::U8ToU7::convert(original);
        assert(u7.size() == expected_u7_len);

        size_t expected_u8_len = volsa2::U7ToU8::convert_len(u7.size());
        assert(expected_u8_len == len);

        auto recovered = volsa2::U7ToU8::convert(u7);
        assert(recovered.size() == original.size());
        assert(recovered == original);
    }

    std::cout << "[PASS] test_round_trip (" << test_lengths.size() << " lengths verified)\n";
}

int main() {
    test_take_nth_msb();
    test_round_trip();
    std::cout << "All seven_bit tests passed!\n";
    return 0;
}
