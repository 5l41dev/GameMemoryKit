#include "gmk/binary/binary_image.hpp"

#include <fstream>
#include <iterator>
#include <string>
#include <utility>

namespace gmk {

namespace {

bool looks_like_pe(ByteView data) noexcept
{
    return data.size() >= 2 && static_cast<char>(data[0]) == 'M' &&
           static_cast<char>(data[1]) == 'Z';
}

bool looks_like_elf(ByteView data) noexcept
{
    return data.size() >= 4 && as_string_view(data.first(4)) == std::string_view{"\x7F" "ELF", 4};
}

}  // namespace

std::string_view to_string(BinaryFormat format) noexcept
{
    using namespace std::string_view_literals;
    switch (format) {
        case BinaryFormat::Unknown: return "Unknown"sv;
        case BinaryFormat::Pe: return "PE"sv;
        case BinaryFormat::Elf: return "ELF"sv;
    }
    return "Unknown"sv;
}

Result<BinaryImage> BinaryImage::parse(ByteView data)
{
    BinaryImage image;
    if (looks_like_pe(data)) {
        auto pe = PeImage::parse(data);
        if (pe.failed()) {
            return pe.error();
        }
        image.image_.emplace<PeImage>(std::move(*pe));
    } else if (looks_like_elf(data)) {
        auto elf = ElfImage::parse(data);
        if (elf.failed()) {
            return elf.error();
        }
        image.image_.emplace<ElfImage>(std::move(*elf));
    } else {
        return Error::invalid_binary(
            "unrecognized executable format (expected PE or ELF)");
    }

    // Normalize sections.
    if (const PeImage* pe = std::get_if<PeImage>(&image.image_)) {
        image.sections_.reserve(pe->sections().size());
        for (const PeSection& section : pe->sections()) {
            Section normalized;
            normalized.name = section.name;
            normalized.address = Address{pe->image_base()}.add(section.virtual_address);
            normalized.size = section.virtual_size;
            normalized.flags = section.characteristics;
            normalized.readable = section.readable();
            normalized.writable = section.writable();
            normalized.executable = section.executable();
            image.sections_.push_back(std::move(normalized));
        }
        // Named, non-forwarded exports.
        image.exports_.reserve(pe->exports().size());
        for (const PeExport& entry : pe->exports()) {
            if (entry.forwarded || entry.name.empty()) {
                continue;
            }
            Symbol symbol;
            symbol.name = entry.name;
            symbol.address = Address{pe->image_base()}.add(entry.rva);
            image.exports_.push_back(std::move(symbol));
        }
    } else if (const ElfImage* elf = std::get_if<ElfImage>(&image.image_)) {
        image.sections_.reserve(elf->sections().size());
        for (const ElfSection& section : elf->sections()) {
            Section normalized;
            normalized.name = section.name;
            normalized.address = Address{static_cast<std::uintptr_t>(section.address)};
            normalized.size = section.size;
            normalized.flags = section.flags;
            normalized.readable = (section.flags & 0x4) != 0;
            normalized.writable = (section.flags & 0x1) != 0;
            normalized.executable = (section.flags & 0x2) != 0;
            image.sections_.push_back(std::move(normalized));
        }
        image.exports_ = elf->symbols();
    }

    return image;
}

Result<BinaryImage> BinaryImage::parse_file(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return Error::io("cannot open file: " + path.string());
    }
    const std::streamsize size = file.tellg();
    if (size < 0) {
        return Error::io("cannot determine file size: " + path.string());
    }
    std::vector<std::byte> buffer(static_cast<std::size_t>(size));
    file.seekg(0, std::ios::beg);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return Error::io("cannot read file: " + path.string());
    }
    return parse(buffer);
}

BinaryFormat BinaryImage::format() const noexcept
{
    if (std::holds_alternative<PeImage>(image_)) {
        return BinaryFormat::Pe;
    }
    if (std::holds_alternative<ElfImage>(image_)) {
        return BinaryFormat::Elf;
    }
    return BinaryFormat::Unknown;
}

std::string_view BinaryImage::format_name() const noexcept
{
    return to_string(format());
}

Architecture BinaryImage::architecture() const noexcept
{
    if (const PeImage* pe = std::get_if<PeImage>(&image_)) {
        return pe->architecture();
    }
    if (const ElfImage* elf = std::get_if<ElfImage>(&image_)) {
        return elf->architecture();
    }
    return Architecture::Unknown;
}

bool BinaryImage::is_64_bit() const noexcept
{
    if (const PeImage* pe = std::get_if<PeImage>(&image_)) {
        return pe->is_64_bit();
    }
    if (const ElfImage* elf = std::get_if<ElfImage>(&image_)) {
        return elf->is_64_bit();
    }
    return false;
}

Address BinaryImage::entry_point() const noexcept
{
    if (const PeImage* pe = std::get_if<PeImage>(&image_)) {
        return pe->entry_point();
    }
    if (const ElfImage* elf = std::get_if<ElfImage>(&image_)) {
        return Address{static_cast<std::uintptr_t>(elf->entry_point())};
    }
    return Address{};
}

const std::vector<BinaryImage::Section>& BinaryImage::sections() const noexcept
{
    return sections_;
}

const std::vector<Symbol>& BinaryImage::exports() const noexcept
{
    return exports_;
}

const PeImage* BinaryImage::as_pe() const noexcept
{
    return std::get_if<PeImage>(&image_);
}

const ElfImage* BinaryImage::as_elf() const noexcept
{
    return std::get_if<ElfImage>(&image_);
}

}  // namespace gmk
