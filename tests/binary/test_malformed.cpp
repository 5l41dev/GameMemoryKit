// Robustness tests: the binary parsers must never read out of bounds,
// crash, or loop forever on malformed input. We use a deterministic
// pseudo-random mutator (no external dependencies) so runs are repeatable.

#include "gmk/binary/binary_image.hpp"

#include "../fixtures/elf_fixture.hpp"
#include "../fixtures/pe_fixture.hpp"
#include "../support/minitest.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

/// Deterministic LCG (xorshift-style) so mutation runs are reproducible.
struct Lcg {
    std::uint64_t state{0x9E3779B97F4A7C15ull};

    std::uint64_t next()
    {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return state;
    }

    std::size_t below(std::size_t limit) { return static_cast<std::size_t>(next() % limit); }
};

/// Returns true if a parse crashed (a crash would abort the test process, so
/// reaching this function means the parse returned normally).
void must_not_crash(const std::vector<std::byte>& data)
{
    auto result = gmk::BinaryImage::parse(data);
    (void)result;  // error or success are both acceptable
}

void mutate_and_check(const std::vector<std::byte>& seed, std::size_t iterations)
{
    Lcg rng;
    std::vector<std::byte> buffer = seed;

    for (std::size_t i = 0; i < iterations; ++i) {
        // Keep the buffer non-empty: the parsers are exercised on meaningful
        // sizes, and empty truncation is covered by the dedicated test.
        if (buffer.empty()) {
            buffer.push_back(static_cast<std::byte>(rng.next() & 0xFF));
        }
        switch (rng.below(4)) {
            case 0:  // flip a random byte
                buffer[rng.below(buffer.size())] =
                    static_cast<std::byte>(rng.next() & 0xFF);
                break;
            case 1:  // write a random 32-bit value at a random offset
                if (buffer.size() >= 4) {
                    const std::size_t offset = rng.below(buffer.size() - 3);
                    const std::uint32_t value = static_cast<std::uint32_t>(rng.next());
                    for (int b = 0; b < 4; ++b) {
                        buffer[offset + b] =
                            static_cast<std::byte>((value >> (b * 8)) & 0xFF);
                    }
                }
                break;
            case 2:  // truncate (but never to zero)
                buffer.resize(rng.below(buffer.size()) + 1);
                break;
            case 3:  // extend with garbage
                buffer.insert(buffer.end(), rng.below(64), std::byte{0xFF});
                break;
        }
        must_not_crash(buffer);
    }
}

}  // namespace

MT_TEST(malformed, pe_random_mutations)
{
    auto fixture = fixture::build_pe(true);
    mutate_and_check(fixture.data, 4000);
}

MT_TEST(malformed, pe32_random_mutations)
{
    auto fixture = fixture::build_pe(false);
    mutate_and_check(fixture.data, 2000);
}

MT_TEST(malformed, elf_random_mutations)
{
    auto fixture = fixture::build_elf(true);
    mutate_and_check(fixture.data, 4000);
}

MT_TEST(malformed, elf32_random_mutations)
{
    auto fixture = fixture::build_elf(false);
    mutate_and_check(fixture.data, 2000);
}

MT_TEST(malformed, all_truncations)
{
    // Every prefix of a valid PE and ELF must parse without crashing.
    auto pe = fixture::build_pe(true);
    for (std::size_t size = 0; size <= pe.data.size(); ++size) {
        std::vector<std::byte> truncated(pe.data.begin(), pe.data.begin() + size);
        must_not_crash(truncated);
    }

    auto elf = fixture::build_elf(true);
    for (std::size_t size = 0; size <= elf.data.size(); ++size) {
        std::vector<std::byte> truncated(elf.data.begin(), elf.data.begin() + size);
        must_not_crash(truncated);
    }
}

MT_TEST(malformed, adversarial_field_values)
{
    // Targeted corruption of high-risk fields: offsets, counts, indices.
    auto pe = fixture::build_pe(true);
    const std::vector<std::pair<std::size_t, std::uint32_t>> pe_targets = {
        {0x3C, 0xFFFFFFFF},   // e_lfanew
        {0x86, 0xFFFF},       // section count (u16 at 0x86..0x87 handled below)
        {0x94, 0xFFFF},       // size of optional header
        {0x108, 0x7FFFFFFF},  // export directory rva
        {0x10C, 0x7FFFFFFF},  // import directory rva
    };
    for (const auto& [offset, value] : pe_targets) {
        std::vector<std::byte> buffer = pe.data;
        if (offset + 4 > buffer.size()) {
            continue;
        }
        for (int b = 0; b < 4; ++b) {
            buffer[offset + b] = static_cast<std::byte>((value >> (b * 8)) & 0xFF);
        }
        must_not_crash(buffer);
    }

    auto elf = fixture::build_elf(true);
    // e_phoff, e_shoff, e_shnum, e_shstrndx, shstrtab size.
    for (std::size_t offset : {0x20, 0x28, 0x3C, 0x3E}) {
        std::vector<std::byte> buffer = elf.data;
        for (int b = 0; b < 8; ++b) {
            if (offset + b < buffer.size()) {
                buffer[offset + b] = std::byte{0xFF};
            }
        }
        must_not_crash(buffer);
    }
}

MT_TEST(malformed, garbage_inputs)
{
    // Purely random buffers of assorted sizes must never crash the parsers.
    Lcg rng;
    for (std::size_t size : {0, 1, 2, 3, 4, 15, 16, 17, 63, 64, 65, 256, 1024}) {
        std::vector<std::byte> buffer(size);
        for (auto& b : buffer) {
            b = static_cast<std::byte>(rng.next() & 0xFF);
        }
        must_not_crash(buffer);
    }
}
