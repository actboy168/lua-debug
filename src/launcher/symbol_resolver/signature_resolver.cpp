#include <cstddef>
#include <string>
#include <string_view>
#include <array>
#include <vector>
#include <list>
#include <charconv>
#include <bit>
#include <mutex>
#include <set>

#ifdef _WIN32
#define NOMINMAX
#ifndef __wtypes_h__
#include <wtypes.h>
#endif

#ifndef __WINDEF_
#include <windef.h>
#endif

#ifndef _WINUSER_
#include <winuser.h>
#endif

#ifndef __RPC_H__
#include <rpc.h>
#endif
#include <winnt.h>
#elif defined(__linux__)
#include <elf.h>
#include <dlfcn.h>
#elif defined(__APPLE__)
#include <mach-o/dyld_images.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <dlfcn.h>
#else
#error "unsupported platform"
#endif

#include "signatures.hpp"
#include "../utility/string_helper.hpp"
#include "symbol_resolver.h"


namespace autoattach::symbol_resolver {

    struct PatternScanResult {
        /// <summary>
        /// The offset of the pattern if found, else -1.
        /// </summary>
        int Offset;

        /// <summary>
        /// True if the pattern has been found, else false.
        /// </summary>
        bool Found() const {
            return Offset != -1;
        }

        /// <summary>
        /// Creates a pattern scan result given the offset of the pattern.
        /// </summary>
        /// <param name="offset">The offset of the pattern if found. -1 if not found.</param>
        PatternScanResult(int offset) {
            Offset = offset;
        }

        /// <summary>
        /// Appends to the existing offset if the offset is valid.
        /// </summary>
        PatternScanResult AddOffset(int offset) {
            return Offset != -1 ? PatternScanResult(Offset + offset) : *this;
        }
    };

/// <summary>
/// Represents a generic instruction to match an 8 byte masked value at a given address.
/// </summary>
    struct GenericInstruction {
        /// <summary>
        /// The value to match.
        /// </summary>
        intptr_t LongValue;

        /// <summary>
        /// The mask to apply before comparing with the value.
        /// </summary>
        intptr_t Mask;
    };

    template<typename T>
    T swap_endian(T u) {
        static_assert(CHAR_BIT == 8, "CHAR_BIT != 8");

        union {
            T u;
            unsigned char u8[sizeof(T)];
        } source, dest;

        source.u = u;

        for (size_t k = 0; k < sizeof(T); k++)
            dest.u8[k] = source.u8[sizeof(T) - k - 1];

        return dest.u;
    }
	inline bool islittle() {
		union {
			char a;
			int b;
		}t;
		t.b = 1;
		return t.a == 1;
	}
    struct CompiledScanPattern {
        const std::string MaskIgnore = "??";

        /// <summary>
        /// The pattern the instruction set was created from.
        /// </summary>
        const std::string Pattern;

        /// <summary>
        /// The length of the original given pattern.
        /// </summary>
        size_t Length;

        /// <summary>
        /// Contains the functions that will be executed in order to validate a given block of memory to equal
        /// the pattern this class was instantiated with.
        /// </summary>
        std::vector <GenericInstruction> Instructions;

        /// <summary>
        /// Contains the number of instructions in the <see cref="Instructions"/> object.
        /// </summary>
        int NumberOfInstructions;

        /// <summary>
        /// Creates a new pattern scan target given a string representation of a pattern.
        /// </summary>
        /// <param name="stringPattern">
        ///     The pattern to look for inside the given region.
        ///     Example: "11 22 33 ?? 55".
        ///     Key: ?? represents a byte that should be ignored, anything else if a hex byte. i.e. 11 represents 0x11, 1F represents 0x1F.
        /// </param>
        CompiledScanPattern(std::string stringPattern)
                : Pattern{std::move(stringPattern)} {
            auto entries = strings::spilt_string(Pattern, ' ');
            Length = entries.size();

            // Get bytes to make instructions with.
            Instructions.resize(Length);
            NumberOfInstructions = 0;

            // Optimization for short-medium patterns with masks.
            // Check if our pattern is 1-8 bytes and contains any skips.
            auto spanEntries = entries;
            std::vector <std::string_view> spanEntries_tmp;
            while (spanEntries.size() > 0) {
                int nextSliceLength = std::min(sizeof(intptr_t), spanEntries.size());
                intptr_t mask, value;
                spanEntries_tmp.reserve(nextSliceLength);
                std::copy_n(spanEntries.begin(), nextSliceLength,
                            std::inserter(spanEntries_tmp, spanEntries_tmp.begin()));
                GenerateMaskAndValue(spanEntries_tmp, mask, value);
                AddInstruction({value, mask});
                for (size_t i = 0; i < nextSliceLength; i++) {
                    spanEntries.erase(spanEntries.cbegin());
                }
            }
        }

        void AddInstruction(GenericInstruction instruction) {
            Instructions[NumberOfInstructions] = instruction;
            NumberOfInstructions++;
        }

        /// <summary>
        /// Generates a mask given a pattern between size 0-8.
        /// </summary>
        void GenerateMaskAndValue(std::vector <std::string_view> entries, intptr_t &mask, intptr_t &value) {
            mask = 0;
            value = 0;
            for (int x = 0; x < entries.size(); x++) {
                mask = mask << 8;
                value = value << 8;
                if (entries[x] != MaskIgnore) {
                    mask = mask | 0xFF;
                    uint8_t b;
                    auto [p, ec] = std::from_chars(&*entries[x].begin(), &*entries[x].end(), b, 16);
                    if (ec != std::errc()) {
                        b = 0;
                    }
                    value = value | b;
                }
            }

            // Reverse order of value.
            if (islittle())
            {
                value = swap_endian<intptr_t>(value);
                mask = swap_endian<intptr_t>(mask);

                // Trim excess zeroes.
                int extraPadding = sizeof(intptr_t) - entries.size();
                for (int x = 0; x < extraPadding; x++) {
                    mask = mask >> 8;
                    value = value >> 8;
                }
            }
        }
    };

    struct SimplePatternScanData {
        static constexpr std::array<char, 2>
        _maskIgnore = {'?', '?'};

        /// <summary>
        /// The pattern of bytes to check for.
        /// </summary>
        std::vector <std::byte> Bytes;

        /// <summary>
        /// The mask string to compare against. `x` represents check while `?` ignores.
        /// Each `x` and `?` represent 1 byte.
        /// </summary>
        std::vector <std::byte> Mask;

        /// <summary>
        /// The original string from which this pattern was created.
        /// </summary>
        std::string Pattern;

        /// <summary>
        /// Creates a new pattern scan target given a string representation of a pattern.
        /// </summary>
        /// <param name="stringPattern">
        ///     The pattern to look for inside the given region.
        ///     Example: "11 22 33 ?? 55".
        ///     Key: ?? represents a byte that should be ignored, anything else if a hex byte. i.e. 11 represents 0x11, 1F represents 0x1F.
        /// </param>
        SimplePatternScanData(std::string stringPattern) {
            Pattern = std::move(stringPattern);
            auto patterns = strings::spilt_string(Pattern, ' ');
            auto enumerator = patterns.cbegin();
            const auto &questionMarkFlag = _maskIgnore;


            while (enumerator != patterns.cend()) {
                auto current = *enumerator;
                if (std::equal(current.cbegin(), current.cend(), questionMarkFlag.cbegin(), questionMarkFlag.cend())) {
                    Mask.emplace_back((std::byte) 0x0);
                } else {
                    uint8_t b;
                    auto [p, ec] = std::from_chars(&*current.cbegin(), &*current.cend(), b, 16);
                    if (ec != std::errc()) {
                        b = 0;
                    }
                    Bytes.emplace_back((std::byte) 0);
                    Mask.emplace_back((std::byte) 0x1);
                }
                ++enumerator;
            }
        }
    };

    struct Scanner {
        std::byte *_dataPtr;
        int _dataLength;

        PatternScanResult FindPattern(std::string pattern) const{
            return FindPatternCompiled(_dataPtr, _dataLength, std::move(pattern));
        }

        PatternScanResult FindPattern(std::string pattern, int offset){
            return FindPatternCompiled(_dataPtr + offset, _dataLength - offset, std::move(pattern));
		}

        std::vector <PatternScanResult> FindPatterns(const std::set <std::string> &patterns) const {
			std::vector<PatternScanResult> res;
			res.reserve(patterns.size());
			for (auto &&pattern : patterns){
				res.emplace_back(FindPattern(pattern));
			}
			return res;
		}


        PatternScanResult FindPattern_Compiled(std::string pattern) const{
			return FindPatternCompiled(_dataPtr, _dataLength, std::move(pattern));
		}

        PatternScanResult FindPattern_Simple(std::string pattern)  const{
			return FindPatternSimple(_dataPtr, _dataLength, std::move(pattern));
		}

        static PatternScanResult FindPatternCompiled(std::byte *data, int dataLength, CompiledScanPattern pattern) {
            const int numberOfUnrolls = 8;
            int numberOfInstructions = pattern.NumberOfInstructions;
            int lastIndex = dataLength - std::max(pattern.Length, sizeof(intptr_t)) - numberOfUnrolls;
            if (lastIndex < 0)
                return FindPatternSimple(data, dataLength, SimplePatternScanData(pattern.Pattern));

            auto firstInstruction = pattern.Instructions[0];
            auto dataCurPointer = data;
            auto dataMaxPointer = data + lastIndex;
            while (dataCurPointer < dataMaxPointer) {
                if ((*(intptr_t *) dataCurPointer & firstInstruction.Mask) != firstInstruction.LongValue) {
                    if ((*(intptr_t * )(dataCurPointer + 1) & firstInstruction.Mask) != firstInstruction.LongValue) {
                        if ((*(intptr_t * )(dataCurPointer + 2) & firstInstruction.Mask) !=
                            firstInstruction.LongValue) {
                            if ((*(intptr_t * )(dataCurPointer + 3) & firstInstruction.Mask) !=
                                firstInstruction.LongValue) {
                                if ((*(intptr_t * )(dataCurPointer + 4) & firstInstruction.Mask) !=
                                    firstInstruction.LongValue) {
                                    if ((*(intptr_t * )(dataCurPointer + 5) & firstInstruction.Mask) !=
                                        firstInstruction.LongValue) {
                                        if ((*(intptr_t * )(dataCurPointer + 6) & firstInstruction.Mask) !=
                                            firstInstruction.LongValue) {
                                            if ((*(intptr_t * )(dataCurPointer + 7) & firstInstruction.Mask) !=
                                                firstInstruction.LongValue) {
                                                dataCurPointer += 8;
                                                goto end;
                                            } else {
                                                dataCurPointer += 7;
                                            }
                                        } else {
                                            dataCurPointer += 6;
                                        }
                                    } else {
                                        dataCurPointer += 5;
                                    }
                                } else {
                                    dataCurPointer += 4;
                                }
                            } else {
                                dataCurPointer += 3;
                            }
                        } else {
                            dataCurPointer += 2;
                        }
                    } else {
                        dataCurPointer += 1;
                    }
                }

                if (numberOfInstructions <= 1 ||
                    TestRemainingMasks(numberOfInstructions, dataCurPointer, pattern.Instructions.data()))
                    return PatternScanResult((int) (dataCurPointer - data));

                dataCurPointer += 1;
                end:;

            }
			// Check last few bytes in cases pattern was not found and long overflows into possibly unallocated memory.
                return FindPatternSimple(data + lastIndex, dataLength - lastIndex, pattern.Pattern).AddOffset(
                        lastIndex);
        }

        static bool TestRemainingMasks(int numberOfInstructions, std::byte *currentDataPointer, GenericInstruction *instructions) {
            /* When NumberOfInstructions > 1 */
            currentDataPointer += sizeof(intptr_t);

            int y = 1;
            do {
                auto compareValue = *(intptr_t *) currentDataPointer & instructions[y].Mask;
                if (compareValue != instructions[y].LongValue)
                    return false;

                currentDataPointer += sizeof(intptr_t);
                y++;
            } while (y < numberOfInstructions);

            return true;
        }

        static PatternScanResult FindPatternSimple(std::byte *data, int dataLength, SimplePatternScanData pattern) {
            const auto &patternData = pattern.Bytes;
            const auto &patternMask = pattern.Mask;

            int lastIndex = (dataLength - patternMask.size()) + 1;

            const std::byte *patternDataPtr = patternData.data();
            {
                for (int x = 0; x < lastIndex; x++) {
                    int patternDataOffset = 0;
                    int currentIndex = x;

                    int y = 0;
                    do {
                        // Some performance is saved by making the mask a non-string, since a string comparison is a bit more involved with e.g. null checks.
                        if (patternMask[y] == std::byte(0x0)) {
                            currentIndex += 1;
                            y++;
                            continue;
                        }

                        // Performance: No need to check if Mask is `x`. The only supported wildcard is '?'.
                        if (data[currentIndex] != patternDataPtr[patternDataOffset])
                            goto loopexit;

                        currentIndex += 1;
                        patternDataOffset += 1;
                        y++;
                    } while (y < patternMask.size());

                    return PatternScanResult(x);
                    loopexit:;
                }

                return PatternScanResult(-1);
            }
        }
    };

    struct signature_rsesolver : interface {
		Scanner scanner;
		signatures symbol_signatures;
        signature_rsesolver(const RuntimeModule &module, signatures&& data):symbol_signatures{std::move(data)} {
			scanner._dataPtr = (std::byte *)module.load_address;
			scanner._dataLength = get_lib_memory_size((uintptr_t)module.load_address);
        }

        ~signature_rsesolver() override {
            
        }

        void *getsymbol(const char *name) const override {
			auto pattern = symbol_signatures[name];
			auto res = scanner.FindPattern(std::string(pattern));
			if (!res.Found())
				return nullptr;
			return scanner._dataPtr + res.Offset;
        }

		size_t get_lib_memory_size(uintptr_t baseAddr);
    };

    std::unique_ptr <interface> create_signature_resolver(const RuntimeModule &module, signatures&& data) {
        auto p = std::make_unique<signature_rsesolver>(module, std::move(data));

		return p;
    }

	size_t signature_rsesolver::get_lib_memory_size(uintptr_t baseAddr) {
		size_t memorySize = 0;

#ifdef _WIN32
		IMAGE_DOS_HEADER *dos = reinterpret_cast<IMAGE_DOS_HEADER *>( baseAddr );
		IMAGE_NT_HEADERS *pe = reinterpret_cast<IMAGE_NT_HEADERS *>( baseAddr + dos->e_lfanew );
		IMAGE_FILE_HEADER *file = &pe->FileHeader;
		IMAGE_OPTIONAL_HEADER *opt = &pe->OptionalHeader;

		if( dos->e_magic != IMAGE_DOS_SIGNATURE || pe->Signature != IMAGE_NT_SIGNATURE || opt->Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC )
			return memorySize;

		memorySize = opt->SizeOfImage;
#elif defined(__linux__)
#if defined (__x86_64__) || defined(__aarch64__)
		typedef Elf64_Ehdr Elf_Ehdr;
		typedef Elf64_Phdr Elf_Phdr;
		const unsigned char ELFCLASS = ELFCLASS64;
#ifdef __aarch64__
		const uint16_t EM = EM_AARCH64;
#else
		const uint16_t EM = EM_X86_64;
#endif
#else
		typedef Elf32_Ehdr Elf_Ehdr;
		typedef Elf32_Phdr Elf_Phdr;
		const unsigned char ELFCLASS = ELFCLASS32;
		const uint16_t EM = EM_386;
#endif
		Elf_Ehdr *file = reinterpret_cast<Elf_Ehdr *>( baseAddr );
		if( memcmp( ELFMAG, file->e_ident, SELFMAG ) != 0 )
			return false;

		if( file->e_ident[EI_VERSION] != EV_CURRENT )
			return false;

		if( file->e_ident[EI_CLASS] != ELFCLASS || file->e_machine != EM || file->e_ident[EI_DATA] != ELFDATA2LSB )
			return false;

		uint16_t phdrCount = file->e_phnum;
		Elf_Phdr *phdr = reinterpret_cast<Elf_Phdr *>( baseAddr + file->e_phoff );
		for( uint16_t i = 0; i < phdrCount; ++i )
		{
			Elf_Phdr &hdr = phdr[i];
			if( hdr.p_type == PT_LOAD && hdr.p_flags == ( PF_X | PF_R ) )
			{
				memorySize = PAGE_ALIGN_UP( hdr.p_filesz );
				break;
			}
		}

#elif defined(__APPLE__)
#if defined (__x86_64__) || defined(__aarch64__)
		using mach_header_t = struct mach_header_64;
		using segment_command_t = struct segment_command_64;
		constexpr auto MH_MAGIC_VALUE = MH_MAGIC_64;
		constexpr auto LC_SEGMENT_VALUE = LC_SEGMENT_64;
#else
		using mach_header_t = struct mach_header;
		using segment_command_t = struct segment_command;
		constexpr auto MH_MAGIC_VALUE = MH_MAGIC;
		constexpr auto LC_SEGMENT_VALUE = LC_SEGMENT;
#endif
		mach_header_t *file = reinterpret_cast<mach_header_t *>( baseAddr );
		if( file->magic != MH_MAGIC_VALUE )
			return false;

		uint32_t cmd_count = file->ncmds;
		segment_command_t *seg = reinterpret_cast<segment_command_t *>( baseAddr + sizeof( mach_header_t ) );
		for( uint32_t i = 0; i < cmd_count; ++i )
		{
			if( seg->cmd == LC_SEGMENT_VALUE )
				memorySize += seg->vmsize;

			seg = reinterpret_cast<segment_command_t *>( reinterpret_cast<uintptr_t>( seg ) + seg->cmdsize );
		}

#else
#error "unsupported platform"
#endif
		return memorySize;
	}
}