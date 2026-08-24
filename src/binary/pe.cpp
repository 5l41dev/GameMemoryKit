#include "gmk/binary/pe.hpp"

#include <cstdio>
#include <limits>
#include <optional>
#include <string>
#include <utility>

#include "reader.hpp"

namespace gmk {

namespace {

using binary::detail::Reader;
using binary::detail::sanitize_name;

constexpr std::uint32_t kImageScnMemExecute = 0x20000000;
constexpr std::uint32_t kImageScnMemRead = 0x40000000;
constexpr std::uint32_t kImageScnMemWrite = 0x80000000;

constexpr std::size_t kDosHeaderSize = 64;
constexpr std::size_t kCoffHeaderSize = 20;
constexpr std::size_t kSectionHeaderSize = 40;
constexpr std::size_t kExportDirectorySize = 40;
constexpr std::size_t kImportDescriptorSize = 20;

// Directory indices into the optional-header data directories.
constexpr std::size_t kDirExport = 0;
constexpr std::size_t kDirImport = 1;

constexpr std::uint16_t kMachineX86 = 0x014C;
constexpr std::uint16_t kMachineArm = 0x01C0;
constexpr std::uint16_t kMachineArm64 = 0xAA64;
constexpr std::uint16_t kMachineX64 = 0x8664;

}  // namespace

std::string_view to_string(Architecture architecture) noexcept
{
    using namespace std::string_view_literals;
    switch (architecture) {
        case Architecture::Unknown: return "unknown"sv;
        case Architecture::X86: return "x86"sv;
        case Architecture::X86_64: return "x86-64"sv;
        case Architecture::Arm: return "arm"sv;
        case Architecture::Arm64: return "arm64"sv;
        case Architecture::RiscV32: return "riscv32"sv;
        case Architecture::RiscV64: return "riscv64"sv;
        case Architecture::Other: return "other"sv;
    }
    return "unknown"sv;
}

namespace {

Architecture machine_to_architecture(std::uint16_t machine) noexcept
{
    switch (machine) {
        case kMachineX86: return Architecture::X86;
        case kMachineX64: return Architecture::X86_64;
        case kMachineArm: return Architecture::Arm;
        case kMachineArm64: return Architecture::Arm64;
        default: return Architecture::Other;
    }
}

/// Parses the data directories out of the optional header.
struct Directories {
    std::vector<std::uint32_t> rvas;
    std::vector<std::uint32_t> sizes;
};

Result<Directories> parse_directories(const Reader& r, std::size_t dir_offset,
                                      std::uint32_t declared_count)
{
    Directories dirs;
    // Clamp the declared count to what actually fits in the buffer.
    const std::uint32_t max_fit =
        static_cast<std::uint32_t>((r.size() - dir_offset) / 8);
    const std::uint32_t count = declared_count < max_fit ? declared_count : max_fit;
    dirs.rvas.resize(count);
    dirs.sizes.resize(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        auto rva = r.u32(dir_offset + i * 8);
        auto size = r.u32(dir_offset + i * 8 + 4);
        if (rva.failed() || size.failed()) {
            return rva.failed() ? rva.error() : size.error();
        }
        dirs.rvas[i] = *rva;
        dirs.sizes[i] = *size;
    }
    return dirs;
}

/// An RVA plus a resolved file offset (when the RVA maps to a section).
struct RvaLocation {
    std::uint32_t rva{0};
    std::uint32_t offset{0};
};

/// Resolves an RVA to a file offset using the section table.
std::optional<std::uint32_t> rva_to_offset_impl(const std::vector<PeSection>& sections,
                                                std::uint32_t rva) noexcept
{
    for (const PeSection& section : sections) {
        const std::uint64_t virtual_end =
            static_cast<std::uint64_t>(section.virtual_address) +
            (section.virtual_size > section.raw_size ? section.virtual_size
                                                     : section.raw_size);
        if (rva >= section.virtual_address &&
            static_cast<std::uint64_t>(rva) < virtual_end) {
            const std::uint64_t delta = rva - section.virtual_address;
            const std::uint64_t offset =
                static_cast<std::uint64_t>(section.raw_offset) + delta;
            if (offset > std::numeric_limits<std::uint32_t>::max()) {
                return std::nullopt;
            }
            return static_cast<std::uint32_t>(offset);
        }
    }
    return std::nullopt;
}

std::string to_hex(std::uint64_t value)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%llx", static_cast<unsigned long long>(value));
    return buffer;
}

Result<std::string> read_export_name(const Reader& r, const std::vector<PeSection>& sections,
                                     std::uint32_t name_rva)
{
    const auto offset = rva_to_offset_impl(sections, name_rva);
    if (!offset) {
        return Error::invalid_binary("export name RVA 0x" +
                                     to_hex(name_rva) + " is not mapped by any section");
    }
    auto name = r.cstring(*offset, 4096);
    if (name.failed()) {
        return name.error();
    }
    return sanitize_name(*name);
}

}  // namespace

bool PeSection::executable() const noexcept { return (characteristics & kImageScnMemExecute) != 0; }
bool PeSection::readable() const noexcept { return (characteristics & kImageScnMemRead) != 0; }
bool PeSection::writable() const noexcept { return (characteristics & kImageScnMemWrite) != 0; }

Result<PeImage> PeImage::parse(ByteView data)
{
    Reader r{data};

    // --- DOS header -------------------------------------------------------
    if (data.size() < kDosHeaderSize) {
        return Error::invalid_binary("file too small to be a PE image (" +
                                     std::to_string(data.size()) + " bytes)");
    }
    auto magic = r.u16(0);
    if (magic.failed() || *magic != 0x5A4D /* "MZ" */) {
        return Error::invalid_binary("missing DOS 'MZ' signature");
    }
    auto e_lfanew = r.u32(0x3C);
    if (e_lfanew.failed()) {
        return e_lfanew.error();
    }
    if (*e_lfanew == 0 || *e_lfanew + 4 + kCoffHeaderSize > data.size()) {
        return Error::invalid_binary("DOS e_lfanew (0x" + to_hex(*e_lfanew) +
                                     ") points outside the file");
    }

    // --- NT headers -------------------------------------------------------
    auto signature = r.bytes(*e_lfanew, 4);
    if (signature.failed()) {
        return signature.error();
    }
    if (as_string_view(*signature) != std::string_view{"PE\0\0", 4}) {
        return Error::invalid_binary("missing PE signature at 0x" + to_hex(*e_lfanew));
    }
    const std::size_t coff_offset = *e_lfanew + 4;

    auto machine = r.u16(coff_offset);
    auto section_count = r.u16(coff_offset + 2);
    auto timestamp = r.u32(coff_offset + 4);
    auto size_of_optional = r.u16(coff_offset + 16);
    auto characteristics = r.u16(coff_offset + 18);
    if (machine.failed() || section_count.failed() || timestamp.failed() ||
        size_of_optional.failed() || characteristics.failed()) {
        return Error::invalid_binary("truncated COFF header");
    }

    // --- Optional header --------------------------------------------------
    const std::size_t optional_offset = coff_offset + kCoffHeaderSize;
    if (*size_of_optional < 2) {
        return Error::invalid_binary("optional header too small");
    }
    auto opt_magic = r.u16(optional_offset);
    if (opt_magic.failed()) {
        return opt_magic.error();
    }

    bool is_64_bit = false;
    std::uint64_t image_base = 0;
    std::uint32_t entry_rva = 0;
    std::uint32_t size_of_image = 0;
    std::size_t dir_offset = 0;
    std::uint32_t dir_count = 0;

    if (*opt_magic == 0x10B) {  // PE32
        is_64_bit = false;
        auto entry = r.u32(optional_offset + 16);
        auto base = r.u32(optional_offset + 28);
        auto image_size = r.u32(optional_offset + 56);
        auto count = r.u32(optional_offset + 92);
        if (entry.failed() || base.failed() || image_size.failed() || count.failed()) {
            return Error::invalid_binary("truncated PE32 optional header");
        }
        entry_rva = *entry;
        image_base = *base;
        size_of_image = *image_size;
        dir_count = *count;
        dir_offset = optional_offset + 96;
    } else if (*opt_magic == 0x20B) {  // PE32+
        is_64_bit = true;
        auto entry = r.u32(optional_offset + 16);
        auto base = r.u64(optional_offset + 24);
        auto image_size = r.u32(optional_offset + 56);
        auto count = r.u32(optional_offset + 108);
        if (entry.failed() || base.failed() || image_size.failed() || count.failed()) {
            return Error::invalid_binary("truncated PE32+ optional header");
        }
        entry_rva = *entry;
        image_base = *base;
        size_of_image = *image_size;
        dir_count = *count;
        dir_offset = optional_offset + 112;
    } else {
        return Error::invalid_binary("unknown optional header magic 0x" + to_hex(*opt_magic));
    }

    // --- Section headers --------------------------------------------------
    const std::size_t section_table_offset = optional_offset + *size_of_optional;
    const std::uint64_t section_table_bytes =
        static_cast<std::uint64_t>(*section_count) * kSectionHeaderSize;
    if (section_table_offset > data.size() ||
        section_table_bytes > data.size() - section_table_offset) {
        return Error::invalid_binary("section table does not fit in the file");
    }

    PeImage image;
    image.is_64_bit_ = is_64_bit;
    image.machine_ = *machine;
    image.architecture_ = machine_to_architecture(*machine);
    image.image_base_ = image_base;
    image.entry_rva_ = entry_rva;
    image.size_of_image_ = size_of_image;
    image.characteristics_ = *characteristics;
    image.timestamp_ = *timestamp;
    image.sections_.reserve(*section_count);

    for (std::uint16_t i = 0; i < *section_count; ++i) {
        const std::size_t off = section_table_offset + i * kSectionHeaderSize;
        auto raw_name = r.bytes(off, 8);
        auto virtual_size = r.u32(off + 8);
        auto virtual_address = r.u32(off + 12);
        auto raw_size = r.u32(off + 16);
        auto raw_offset = r.u32(off + 20);
        auto sect_characteristics = r.u32(off + 36);
        if (raw_name.failed() || virtual_size.failed() || virtual_address.failed() ||
            raw_size.failed() || raw_offset.failed() || sect_characteristics.failed()) {
            return Error::invalid_binary("truncated section header " + std::to_string(i));
        }

        std::string_view name_view = as_string_view(*raw_name);
        const std::size_t nul = name_view.find('\0');
        if (nul != std::string_view::npos) {
            name_view = name_view.substr(0, nul);
        }
        PeSection section;
        section.name = sanitize_name(name_view);
        section.virtual_size = *virtual_size;
        section.virtual_address = *virtual_address;
        section.raw_size = *raw_size;
        section.raw_offset = *raw_offset;
        section.characteristics = *sect_characteristics;
        image.sections_.push_back(std::move(section));
    }

    // --- Data directories -------------------------------------------------
    auto dirs = parse_directories(r, dir_offset, dir_count);
    if (dirs.failed()) {
        return dirs.error();
    }

    // --- Exports ----------------------------------------------------------
    if (dirs->rvas.size() > kDirExport && dirs->rvas[kDirExport] != 0) {
        const std::uint32_t export_rva = dirs->rvas[kDirExport];
        auto export_offset = rva_to_offset_impl(image.sections_, export_rva);
        if (!export_offset) {
            return Error::invalid_binary("export directory RVA 0x" + to_hex(export_rva) +
                                         " is not mapped by any section");
        }
        if (!r.in_bounds(*export_offset, kExportDirectorySize)) {
            return Error::invalid_binary("export directory does not fit in the file");
        }
        auto base = r.u32(*export_offset + 16);
        auto num_functions = r.u32(*export_offset + 20);
        auto num_names = r.u32(*export_offset + 24);
        auto functions_rva = r.u32(*export_offset + 28);
        auto names_rva = r.u32(*export_offset + 32);
        auto ordinals_rva = r.u32(*export_offset + 36);
        if (base.failed() || num_functions.failed() || num_names.failed() ||
            functions_rva.failed() || names_rva.failed() || ordinals_rva.failed()) {
            return Error::invalid_binary("truncated export directory");
        }

        const auto functions_offset = rva_to_offset_impl(image.sections_, *functions_rva);
        const auto names_offset = rva_to_offset_impl(image.sections_, *names_rva);
        const auto ordinals_offset = rva_to_offset_impl(image.sections_, *ordinals_rva);
        if (!functions_offset || !names_offset || !ordinals_offset) {
            return Error::invalid_binary("export directory arrays are not mapped");
        }
        const std::uint64_t functions_bytes =
            static_cast<std::uint64_t>(*num_functions) * 4;
        const std::uint64_t names_bytes = static_cast<std::uint64_t>(*num_names) * 4;
        const std::uint64_t ordinals_bytes = static_cast<std::uint64_t>(*num_names) * 2;
        if (!r.in_bounds(*functions_offset, functions_bytes) ||
            !r.in_bounds(*names_offset, names_bytes) ||
            !r.in_bounds(*ordinals_offset, ordinals_bytes)) {
            return Error::invalid_binary("export directory arrays exceed the file");
        }

        // Ordinal-only entries are included when they have a name; forwarded
        // exports are flagged rather than resolved.
        for (std::uint32_t i = 0; i < *num_names; ++i) {
            PeExport entry;
            auto name_rva = r.u32(*names_offset + i * 4);
            auto ordinal_index = r.u16(*ordinals_offset + i * 2);
            if (name_rva.failed() || ordinal_index.failed()) {
                return Error::invalid_binary("truncated export name entry");
            }
            if (*ordinal_index >= *num_functions) {
                return Error::invalid_binary("export ordinal index out of range");
            }
            auto function_rva = r.u32(*functions_offset + *ordinal_index * 4);
            if (function_rva.failed()) {
                return Error::invalid_binary("truncated export function entry");
            }

            auto name_result = read_export_name(r, image.sections_, *name_rva);
            if (name_result.failed()) {
                return name_result.error();
            }
            entry.name = std::move(*name_result);
            entry.ordinal = static_cast<std::uint16_t>(*base + *ordinal_index);
            entry.rva = *function_rva;

            // A function RVA inside the export directory itself is a forwarder
            // string (e.g. "KERNEL32.Sleep").
            if (*function_rva >= export_rva &&
                *function_rva < export_rva + dirs->sizes[kDirExport]) {
                entry.forwarded = true;
                auto fwd_offset = rva_to_offset_impl(image.sections_, *function_rva);
                if (fwd_offset) {
                    auto fwd = r.cstring(*fwd_offset, 4096);
                    if (fwd.ok()) {
                        entry.forwarder = sanitize_name(*fwd);
                    }
                }
            }
            image.exports_.push_back(std::move(entry));
        }
    }

    // --- Imports ----------------------------------------------------------
    if (dirs->rvas.size() > kDirImport && dirs->rvas[kDirImport] != 0) {
        const std::uint32_t import_rva = dirs->rvas[kDirImport];
        auto import_offset = rva_to_offset_impl(image.sections_, import_rva);
        if (!import_offset) {
            return Error::invalid_binary("import directory RVA 0x" + to_hex(import_rva) +
                                         " is not mapped by any section");
        }

        for (std::uint32_t index = 0;; ++index) {
            const std::size_t desc_offset = *import_offset + index * kImportDescriptorSize;
            if (!r.in_bounds(desc_offset, kImportDescriptorSize)) {
                return Error::invalid_binary("import descriptor table exceeds the file");
            }
            auto original_thunk = r.u32(desc_offset);
            auto module_name_rva = r.u32(desc_offset + 12);
            auto first_thunk = r.u32(desc_offset + 16);
            if (original_thunk.failed() || module_name_rva.failed() || first_thunk.failed()) {
                return Error::invalid_binary("truncated import descriptor");
            }
            // All-zero descriptor terminates the table.
            if (*original_thunk == 0 && *module_name_rva == 0 && *first_thunk == 0) {
                break;
            }

            PeImportModule module;
            auto name_offset = rva_to_offset_impl(image.sections_, *module_name_rva);
            if (!name_offset) {
                return Error::invalid_binary("import module name RVA is not mapped");
            }
            auto module_name = r.cstring(*name_offset, 4096);
            if (module_name.failed()) {
                return module_name.error();
            }
            module.name = sanitize_name(*module_name);

            // Prefer OriginalFirstThunk; fall back to FirstThunk.
            const std::uint32_t thunk_rva =
                *original_thunk != 0 ? *original_thunk : *first_thunk;
            auto thunk_offset = rva_to_offset_impl(image.sections_, thunk_rva);
            if (!thunk_offset) {
                return Error::invalid_binary("import thunk array RVA is not mapped");
            }

            const std::uint64_t ordinal_flag =
                is_64_bit ? 0x8000000000000000ull : 0x80000000ull;

            for (std::uint32_t thunk_index = 0;; ++thunk_index) {
                const std::size_t entry_offset = *thunk_offset + thunk_index * (is_64_bit ? 8 : 4);
                std::uint64_t thunk_value = 0;
                if (is_64_bit) {
                    auto v = r.u64(entry_offset);
                    if (v.failed()) {
                        return v.error();
                    }
                    thunk_value = *v;
                } else {
                    auto v = r.u32(entry_offset);
                    if (v.failed()) {
                        return v.error();
                    }
                    thunk_value = *v;
                }
                if (thunk_value == 0) {
                    break;  // terminator
                }

                PeImportFunction function;
                if ((thunk_value & ordinal_flag) != 0) {
                    function.by_ordinal = true;
                    function.ordinal = static_cast<std::uint16_t>(thunk_value & 0xFFFF);
                    function.name = "ordinal_" + std::to_string(function.ordinal);
                } else {
                    const std::uint32_t by_name_rva = static_cast<std::uint32_t>(thunk_value);
                    auto by_name_offset = rva_to_offset_impl(image.sections_, by_name_rva);
                    if (!by_name_offset || !r.in_bounds(*by_name_offset, 3)) {
                        return Error::invalid_binary("import-by-name RVA is not mapped");
                    }
                    auto hint = r.u16(*by_name_offset);
                    if (hint.failed()) {
                        return hint.error();
                    }
                    function.ordinal = *hint;
                    auto function_name = r.cstring(*by_name_offset + 2, 4096);
                    if (function_name.failed()) {
                        return function_name.error();
                    }
                    function.name = sanitize_name(*function_name);
                }
                module.functions.push_back(std::move(function));
            }
            image.imports_.push_back(std::move(module));
        }
    }

    return image;
}

std::optional<std::uint32_t> PeImage::rva_to_offset(std::uint32_t rva) const noexcept
{
    return rva_to_offset_impl(sections_, rva);
}

}  // namespace gmk
