#include <dlfcn.h>
#include <cstddef>
#include <string>
#include <list>
#include <charconv>
#include <bit>
#include <mutex>
#include <set>

#include <unistd.h>

#include "signatures.hpp"
#include "../utility/string_helper.hpp"
#include "symbol_resolver.h"

using namespace std;
namespace autoattach::symbol_resolver {
#ifdef _WIN32
	
#elif defined(__linux__)

#elif defined(__APPLE__)

#else
#error "unsupported platform"
#endif

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
        const string MaskIgnore = "??";

        /// <summary>
        /// The pattern the instruction set was created from.
        /// </summary>
        const string Pattern;

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
        CompiledScanPattern(string stringPattern)
                : Pattern{std::move(stringPattern)} {
            auto entries = strings::spilt_string(Pattern, ' ');
            Length = entries.size();

            // Get bytes to make instructions with.
            Instructions.resize(Length);
            NumberOfInstructions = 0;

            // Optimization for short-medium patterns with masks.
            // Check if our pattern is 1-8 bytes and contains any skips.
            auto spanEntries = entries;
            vector <string_view> spanEntries_tmp;
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
        void GenerateMaskAndValue(vector <string_view> entries, intptr_t &mask, intptr_t &value) {
            mask = 0;
            value = 0;
            for (int x = 0; x < entries.size(); x++) {
                mask = mask << 8;
                value = value << 8;
                if (entries[x] != MaskIgnore) {
                    mask = mask | 0xFF;
                    uint8_t b;
                    auto [p, ec] = std::from_chars(entries[x].begin(), entries[x].end(), b, 16);
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
        static constexpr array<char, 2>
        _maskIgnore = {'?', '?'};

        /// <summary>
        /// The pattern of bytes to check for.
        /// </summary>
        vector <byte> Bytes;

        /// <summary>
        /// The mask string to compare against. `x` represents check while `?` ignores.
        /// Each `x` and `?` represent 1 byte.
        /// </summary>
        vector <byte> Mask;

        /// <summary>
        /// The original string from which this pattern was created.
        /// </summary>
        string Pattern;

        /// <summary>
        /// Creates a new pattern scan target given a string representation of a pattern.
        /// </summary>
        /// <param name="stringPattern">
        ///     The pattern to look for inside the given region.
        ///     Example: "11 22 33 ?? 55".
        ///     Key: ?? represents a byte that should be ignored, anything else if a hex byte. i.e. 11 represents 0x11, 1F represents 0x1F.
        /// </param>
        SimplePatternScanData(string stringPattern) {
            Pattern = std::move(stringPattern);
            auto patterns = strings::spilt_string(Pattern, ' ');
            auto enumerator = patterns.cbegin();
            const auto &questionMarkFlag = _maskIgnore;


            while (enumerator != patterns.cend()) {
                auto current = *enumerator;
                if (std::equal(current.cbegin(), current.cend(), questionMarkFlag.cbegin(), questionMarkFlag.cend())) {
                    Mask.emplace_back((byte) 0x0);
                } else {
                    uint8_t b;
                    auto [p, ec] = std::from_chars(current.cbegin(), current.cend(), b, 16);
                    if (ec != std::errc()) {
                        b = 0;
                    }
                    Bytes.emplace_back((byte) 0);
                    Mask.emplace_back((byte) 0x1);
                }
                ++enumerator;
            }
        }
    };

    struct IScanner {
        virtual ~IScanner() = default;

        virtual PatternScanResult FindPattern(string pattern) = 0;

        virtual PatternScanResult FindPattern(string pattern, int offset) = 0;

        virtual vector <PatternScanResult> FindPatterns(const set <string> &patterns) = 0;

        virtual PatternScanResult FindPattern_Compiled(std::string pattern) = 0;

        virtual PatternScanResult FindPattern_Simple(std::string pattern) = 0;
    };

    struct Scanner : IScanner {
        byte *_dataPtr;
        int _dataLength;

        PatternScanResult FindPattern(string pattern) override{
            return FindPatternCompiled(_dataPtr, _dataLength, std::move(pattern));
        }

        PatternScanResult FindPattern(string pattern, int offset) override{
            return FindPatternCompiled(_dataPtr + offset, _dataLength - offset, std::move(pattern));
		}

        vector <PatternScanResult> FindPatterns(const std::set <string> &patterns) override {
			vector<PatternScanResult> res;
			res.reserve(patterns.size());
			for (auto &&pattern : patterns) 			{
				res.emplace_back(FindPattern(pattern));
			}
		}


        PatternScanResult FindPattern_Compiled(std::string pattern) override{
			return FindPatternCompiled(_dataPtr, _dataLength, std::move(pattern));
		}

        PatternScanResult FindPattern_Simple(std::string pattern) override {
			return FindPatternSimple(_dataPtr, _dataLength, std::move(pattern));
		}

        PatternScanResult FindPatternCompiled(byte *data, int dataLength, CompiledScanPattern pattern) {
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

                // Check last few bytes in cases pattern was not found and long overflows into possibly unallocated memory.
                return FindPatternSimple(data + lastIndex, dataLength - lastIndex, pattern.Pattern).AddOffset(
                        lastIndex);

            }
        }

        bool TestRemainingMasks(int numberOfInstructions, byte *currentDataPointer, GenericInstruction *instructions) {
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

        PatternScanResult FindPatternSimple(byte *data, int dataLength, SimplePatternScanData pattern) {
            const auto &patternData = pattern.Bytes;
            const auto &patternMask = pattern.Mask;

            int lastIndex = (dataLength - patternMask.size()) + 1;

            const byte *patternDataPtr = patternData.data();
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
        signature_rsesolver(const RuntimeModule &module) {
			scanner._dataPtr = (byte *)module.load_address;
			scanner._dataLength = 0;
        }

        ~signature_rsesolver() override {
            
        }

        void *getsymbol(const char *name) const override {
			return nullptr;
        }
    };

    std::unique_ptr <interface> create_signature_resolver(const RuntimeModule &module, const signatures &data) {
        auto p = std::make_unique<signature_rsesolver>(module.path);

		return p;
    }
}