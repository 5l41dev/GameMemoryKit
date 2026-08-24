#include "gmk/binary/elf.hpp"

#include <string>
#include <utility>

#include "reader.hpp"

namespace gmk {

namespace {

using binary::detail::Reader;
using binary::detail::sanitize_name;

constexpr std::uint8_t kElfClass32 = 1;
constexpr std::uint8_t kElfClass64 = 2;
constexpr std::uint8_t kElfDataLittleEndian = 1;

constexpr std::uint16_t kShnUndef = 0;
constexpr std::uint16_t kShnXindex = 0xFFFF;
constexpr std::uint16_t kPnXnum = 0xFFFF;

constexpr std::uint32_t kShtSymtab = 2;
constexpr std::uint32_t kShtStrtab = 3;
constexpr std::uint32_t kShtDynsym = 11;

constexpr std::uint8_t kStbGlobal = 1;
constexpr std::uint8_t kStbWeak = 2;

constexpr std::uint16_t kEmX86 = 3;
constexpr std::uint16_t kEmX86_64 = 62;
constexpr std::uint16_t kEmArm = 40;
constexpr std::uint16_t kEmAarch64 = 183;
constexpr std::uint16_t kEmRiscV = 243;

}  // namespace

std::string_view to_string(ElfType type) noexcept
{
    using namespace std::string_view_literals;
    switch (type) {
        case ElfType::None: return "none"sv;
        case ElfType::Relocatable: return "relocatable"sv;
        case ElfType::Executable: return "executable"sv;
        case ElfType::Shared: return "shared"sv;
        case ElfType::Core: return "core"sv;
    }
    return "unknown"sv;
}

namespace {

Architecture machine_to_architecture(std::uint16_t machine) noexcept
{
    switch (machine) {
        case kEmX86: return Architecture::X86;
        case kEmX86_64: return Architecture::X86_64;
        case kEmArm: return Architecture::Arm;
        case kEmAarch64: return Architecture::Arm64;
        case kEmRiscV: return Architecture::RiscV64;  // RISC-V class distinguishes 32/64
        default: return Architecture::Other;
    }
}

/// Reads the ELF identification block; returns the class (32/64).
Result<bool> read_ident(const Reader& r)
{
    if (r.size() < 16) {
        return Error::invalid_binary("file too small to be an ELF image (" +
                                     std::to_string(r.size()) + " bytes)");
    }
    auto magic = r.bytes(0, 4);
    if (magic.failed()) {
        return magic.error();
    }
    if (as_string_view(*magic) != std::string_view{"\x7F" "ELF", 4}) {
        return Error::invalid_binary("missing ELF magic");
    }
    auto elf_class = r.u8(4);
    auto data_encoding = r.u8(5);
    if (elf_class.failed() || data_encoding.failed()) {
        return Error::invalid_binary("truncated ELF identification");
    }
    if (*elf_class != kElfClass32 && *elf_class != kElfClass64) {
        return Error::invalid_binary("unknown ELF class " + std::to_string(*elf_class));
    }
    if (*data_encoding != kElfDataLittleEndian) {
        return Error::unsupported("big-endian ELF images are not supported");
    }
    return *elf_class == kElfClass64;
}

/// A header with sizes already validated. Field offsets are resolved per
/// class by the callers.
struct ElfHeader {
    bool is_64{false};
    std::uint16_t type{0};
    std::uint16_t machine{0};
    std::uint64_t entry{0};
    std::uint64_t phoff{0};
    std::uint64_t shoff{0};
    std::uint16_t phentsize{0};
    std::uint16_t phnum{0};
    std::uint16_t shentsize{0};
    std::uint16_t shnum{0};
    std::uint16_t shstrndx{0};
};

Result<ElfHeader> read_header(const Reader& r, bool is_64)
{
    ElfHeader h;
    h.is_64 = is_64;
    const std::size_t base = 16;
    if (is_64) {
        auto type = r.u16(base);
        auto machine = r.u16(base + 2);
        auto entry = r.u64(base + 8);
        auto phoff = r.u64(base + 16);
        auto shoff = r.u64(base + 24);
        auto phentsize = r.u16(base + 0x26);
        auto phnum = r.u16(base + 0x28);
        auto shentsize = r.u16(base + 0x2A);
        auto shnum = r.u16(base + 0x2C);
        auto shstrndx = r.u16(base + 0x2E);
        if (type.failed() || machine.failed() || entry.failed() || phoff.failed() ||
            shoff.failed() || phentsize.failed() || phnum.failed() ||
            shentsize.failed() || shnum.failed() || shstrndx.failed()) {
            return Error::invalid_binary("truncated ELF64 header");
        }
        h.type = *type;
        h.machine = *machine;
        h.entry = *entry;
        h.phoff = *phoff;
        h.shoff = *shoff;
        h.phentsize = *phentsize;
        h.phnum = *phnum;
        h.shentsize = *shentsize;
        h.shnum = *shnum;
        h.shstrndx = *shstrndx;
    } else {
        auto type = r.u16(base);
        auto machine = r.u16(base + 2);
        auto entry = r.u32(base + 8);
        auto phoff = r.u32(base + 12);
        auto shoff = r.u32(base + 16);
        auto phentsize = r.u16(base + 0x1A);
        auto phnum = r.u16(base + 0x1C);
        auto shentsize = r.u16(base + 0x1E);
        auto shnum = r.u16(base + 0x20);
        auto shstrndx = r.u16(base + 0x22);
        if (type.failed() || machine.failed() || entry.failed() || phoff.failed() ||
            shoff.failed() || phentsize.failed() || phnum.failed() ||
            shentsize.failed() || shnum.failed() || shstrndx.failed()) {
            return Error::invalid_binary("truncated ELF32 header");
        }
        h.type = *type;
        h.machine = *machine;
        h.entry = *entry;
        h.phoff = *phoff;
        h.shoff = *shoff;
        h.phentsize = *phentsize;
        h.phnum = *phnum;
        h.shentsize = *shentsize;
        h.shnum = *shnum;
        h.shstrndx = *shstrndx;
    }
    return h;
}

}  // namespace

bool ElfSection::executable() const noexcept { return (flags & 0x4) != 0; }
bool ElfSection::writable() const noexcept { return (flags & 0x1) != 0; }
bool ElfSection::allocatable() const noexcept { return (flags & 0x2) != 0; }

Result<ElfImage> ElfImage::parse(ByteView data)
{
    Reader r{data};

    auto is_64_result = read_ident(r);
    if (is_64_result.failed()) {
        return is_64_result.error();
    }
    const bool is_64 = *is_64_result;

    auto header = read_header(r, is_64);
    if (header.failed()) {
        return header.error();
    }

    const std::size_t expected_ehsize = is_64 ? 64 : 52;
    if (r.size() < expected_ehsize) {
        return Error::invalid_binary("truncated ELF header");
    }

    ElfImage image;
    image.is_64_bit_ = is_64;
    image.machine_ = header->machine;
    image.entry_point_ = header->entry;
    image.type_ = static_cast<ElfType>(header->type);

    Architecture arch = machine_to_architecture(header->machine);
    if (arch == Architecture::RiscV64) {
        // e_machine alone does not distinguish RV32/RV64; the class does.
        arch = is_64 ? Architecture::RiscV64 : Architecture::RiscV32;
    }
    image.architecture_ = arch;

    // --- Section headers --------------------------------------------------
    // Extended numbering: e_shnum == 0 (or e_shstrndx == SHN_XINDEX) stores
    // the real values in section header 0.
    const std::size_t sh_entsize = is_64 ? 64 : 40;
    std::uint64_t shnum = header->shnum;
    std::uint64_t shstrndx = header->shstrndx;

    std::uint64_t shoff = header->shoff;
    if (header->shnum == 0 && header->shoff != 0) {
        // Real section count is in section 0's sh_size.
        auto size_field = r.u64(shoff + 32);
        auto link_field = r.u64(shoff + 40);
        if (is_64) {
            if (size_field.ok()) {
                shnum = *size_field;
            }
            if (link_field.ok()) {
                shstrndx = *link_field;
            }
        } else {
            auto size32 = r.u32(shoff + 20);
            auto link32 = r.u32(shoff + 24);
            if (size32.ok()) {
                shnum = *size32;
            }
            if (link32.ok()) {
                shstrndx = *link32;
            }
        }
    } else if (header->shstrndx == kShnXindex) {
        auto link_field = r.u64(shoff + 40);
        if (is_64) {
            if (link_field.ok()) {
                shstrndx = *link_field;
            }
        } else {
            auto link32 = r.u32(shoff + 24);
            if (link32.ok()) {
                shstrndx = *link32;
            }
        }
    }

    if (header->shentsize < sh_entsize) {
        return Error::invalid_binary("section header entry size too small");
    }

    // The section table must fit in the file.
    const std::uint64_t sh_table_bytes = shnum * header->shentsize;
    if (shoff > data.size() || sh_table_bytes > data.size() - shoff) {
        return Error::invalid_binary("section header table does not fit in the file");
    }

    image.sections_.reserve(static_cast<std::size_t>(shnum));
    for (std::uint64_t i = 0; i < shnum; ++i) {
        const std::size_t off = static_cast<std::size_t>(shoff + i * header->shentsize);
        ElfSection section;
        if (is_64) {
            auto name = r.u32(off);
            auto type = r.u32(off + 4);
            auto flags = r.u64(off + 8);
            auto addr = r.u64(off + 16);
            auto offset = r.u64(off + 24);
            auto size = r.u64(off + 32);
            auto link = r.u32(off + 40);
            auto info = r.u32(off + 44);
            auto align = r.u64(off + 48);
            auto entsize = r.u64(off + 56);
            if (name.failed() || type.failed() || flags.failed() || addr.failed() ||
                offset.failed() || size.failed() || link.failed() || info.failed() ||
                align.failed() || entsize.failed()) {
                return Error::invalid_binary("truncated ELF64 section header " + std::to_string(i));
            }
            section.type = *type;
            section.flags = *flags;
            section.address = *addr;
            section.offset = *offset;
            section.size = *size;
            section.link = *link;
            section.info = *info;
            section.alignment = *align;
            section.entry_size = *entsize;
            section.name_index = *name;
        } else {
            auto name = r.u32(off);
            auto type = r.u32(off + 4);
            auto flags = r.u32(off + 8);
            auto addr = r.u32(off + 12);
            auto offset = r.u32(off + 16);
            auto size = r.u32(off + 20);
            auto link = r.u32(off + 24);
            auto info = r.u32(off + 28);
            auto align = r.u32(off + 32);
            auto entsize = r.u32(off + 36);
            if (name.failed() || type.failed() || flags.failed() || addr.failed() ||
                offset.failed() || size.failed() || link.failed() || info.failed() ||
                align.failed() || entsize.failed()) {
                return Error::invalid_binary("truncated ELF32 section header " + std::to_string(i));
            }
            section.type = *type;
            section.flags = *flags;
            section.address = *addr;
            section.offset = *offset;
            section.size = *size;
            section.link = *link;
            section.info = *info;
            section.alignment = *align;
            section.entry_size = *entsize;
            section.name_index = *name;
        }
        image.sections_.push_back(std::move(section));
    }

    // --- Section names (via .shstrtab) ------------------------------------
    std::vector<std::string_view> section_names;
    if (shstrndx < image.sections_.size()) {
        const ElfSection& strtab = image.sections_[static_cast<std::size_t>(shstrndx)];
        if (strtab.type == kShtStrtab && strtab.offset <= data.size() &&
            strtab.size <= data.size() - strtab.offset) {
            section_names.resize(image.sections_.size());
            for (std::size_t i = 0; i < image.sections_.size(); ++i) {
                const std::uint32_t name_offset = image.sections_[i].name_index;
                if (name_offset >= strtab.size) {
                    continue;  // leave unnamed
                }
                auto name = r.cstring(strtab.offset + name_offset,
                                      static_cast<std::size_t>(strtab.size - name_offset));
                if (name.ok()) {
                    section_names[i] = *name;
                }
            }
        }
    }
    for (std::size_t i = 0; i < image.sections_.size(); ++i) {
        if (i < section_names.size() && !section_names[i].empty()) {
            image.sections_[i].name = sanitize_name(section_names[i]);
        }
    }

    // --- Program headers --------------------------------------------------
    const std::size_t ph_entsize = is_64 ? 56 : 32;
    if (header->phoff != 0 && header->phnum != 0) {
        std::uint64_t phnum = header->phnum;
        if (header->phnum == kPnXnum && !image.sections_.empty()) {
            phnum = image.sections_[0].info;
        }
        if (header->phentsize < ph_entsize) {
            return Error::invalid_binary("program header entry size too small");
        }
        const std::uint64_t ph_table_bytes = phnum * header->phentsize;
        if (header->phoff > data.size() || ph_table_bytes > data.size() - header->phoff) {
            return Error::invalid_binary("program header table does not fit in the file");
        }
        image.program_headers_.reserve(static_cast<std::size_t>(phnum));
        for (std::uint64_t i = 0; i < phnum; ++i) {
            const std::size_t off =
                static_cast<std::size_t>(header->phoff + i * header->phentsize);
            ElfProgramHeader ph;
            if (is_64) {
                auto type = r.u32(off);
                auto flags = r.u32(off + 4);
                auto offset = r.u64(off + 8);
                auto vaddr = r.u64(off + 16);
                auto paddr = r.u64(off + 24);
                auto filesz = r.u64(off + 32);
                auto memsz = r.u64(off + 40);
                auto align = r.u64(off + 48);
                if (type.failed() || flags.failed() || offset.failed() || vaddr.failed() ||
                    paddr.failed() || filesz.failed() || memsz.failed() || align.failed()) {
                    return Error::invalid_binary("truncated ELF64 program header " + std::to_string(i));
                }
                ph.type = *type;
                ph.flags = *flags;
                ph.offset = *offset;
                ph.virtual_address = *vaddr;
                ph.physical_address = *paddr;
                ph.file_size = *filesz;
                ph.memory_size = *memsz;
                ph.alignment = *align;
            } else {
                auto type = r.u32(off);
                auto offset = r.u32(off + 4);
                auto vaddr = r.u32(off + 8);
                auto paddr = r.u32(off + 12);
                auto filesz = r.u32(off + 16);
                auto memsz = r.u32(off + 20);
                auto flags = r.u32(off + 24);
                auto align = r.u32(off + 28);
                if (type.failed() || flags.failed() || offset.failed() || vaddr.failed() ||
                    paddr.failed() || filesz.failed() || memsz.failed() || align.failed()) {
                    return Error::invalid_binary("truncated ELF32 program header " + std::to_string(i));
                }
                ph.type = *type;
                ph.flags = *flags;
                ph.offset = *offset;
                ph.virtual_address = *vaddr;
                ph.physical_address = *paddr;
                ph.file_size = *filesz;
                ph.memory_size = *memsz;
                ph.alignment = *align;
            }
            image.program_headers_.push_back(std::move(ph));
        }
    }

    // --- Symbols (dynamic table, with .symtab fallback) --------------------
    const ElfSection* symtab = nullptr;
    for (const ElfSection& section : image.sections_) {
        if (section.type == kShtDynsym) {
            symtab = &section;
            break;
        }
    }
    if (symtab == nullptr) {
        for (const ElfSection& section : image.sections_) {
            if (section.type == kShtSymtab) {
                symtab = &section;
                break;
            }
        }
    }

    if (symtab != nullptr && symtab->link < image.sections_.size()) {
        const ElfSection& strtab = image.sections_[static_cast<std::size_t>(symtab->link)];
        if (strtab.type == kShtStrtab && symtab->offset <= data.size() &&
            symtab->size <= data.size() - symtab->offset &&
            strtab.offset <= data.size() && strtab.size <= data.size() - strtab.offset) {
            const std::size_t sym_entsize = is_64 ? 24 : 16;
            if (symtab->entry_size >= sym_entsize) {
                const std::uint64_t count = symtab->size / sym_entsize;
                image.symbols_.reserve(static_cast<std::size_t>(count));
                for (std::uint64_t i = 0; i < count; ++i) {
                    const std::size_t off =
                        static_cast<std::size_t>(symtab->offset + i * sym_entsize);
                    std::uint32_t name_index = 0;
                    std::uint8_t info = 0;
                    std::uint16_t shndx = 0;
                    std::uint64_t value = 0;
                    std::uint64_t size = 0;
                    if (is_64) {
                        auto n = r.u32(off);
                        auto i2 = r.u8(off + 4);
                        auto s = r.u16(off + 6);
                        auto v = r.u64(off + 8);
                        auto z = r.u64(off + 16);
                        if (n.failed() || i2.failed() || s.failed() || v.failed() || z.failed()) {
                            break;  // truncated table; stop gracefully
                        }
                        name_index = *n;
                        info = *i2;
                        shndx = *s;
                        value = *v;
                        size = *z;
                    } else {
                        auto n = r.u32(off);
                        auto v = r.u32(off + 4);
                        auto z = r.u32(off + 8);
                        auto i2 = r.u8(off + 12);
                        auto s = r.u16(off + 14);
                        if (n.failed() || v.failed() || z.failed() || i2.failed() || s.failed()) {
                            break;
                        }
                        name_index = *n;
                        info = *i2;
                        shndx = *s;
                        value = *v;
                        size = *z;
                    }
                    const std::uint8_t bind = info >> 4;
                    const std::uint8_t sym_type = info & 0x0F;
                    if (shndx == kShnUndef) {
                        continue;
                    }
                    if (bind != kStbGlobal && bind != kStbWeak) {
                        continue;
                    }
                    if (name_index >= strtab.size) {
                        continue;
                    }
                    auto name = r.cstring(strtab.offset + name_index,
                                          static_cast<std::size_t>(strtab.size - name_index));
                    if (name.failed() || name->empty()) {
                        continue;
                    }
                    // Function/object symbols only; section symbols and
                    // notype entries are noise for the export view.
                    if (sym_type != 1 && sym_type != 2 && sym_type != 10) {  // OBJECT/FUNC/IFUNC
                        continue;
                    }
                    Symbol symbol;
                    symbol.name = sanitize_name(*name);
                    symbol.address = Address{static_cast<std::uintptr_t>(value)};
                    symbol.size = static_cast<std::size_t>(size);
                    image.symbols_.push_back(std::move(symbol));
                }
            }
        }
    }

    return image;
}

}  // namespace gmk
