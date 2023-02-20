local lm = require "luamake"

require 'compile.common.detect_platform'

local ARCH = lm.runtime_platform:match "[%w]+-([%w]+)"
local NearBranch = ARCH == "arm64"

local Includes = {
    ".",
    "include",
    "source",
    "source/include",
    "external",
    "external/logging",
    "source/Backend/UserMode",
}

lm:source_set "dobbySymbolResolver" {
    rootdir = "3rd/Dobby",
    includes = {
        Includes,
        "builtin-plugin",
        "builtin-plugin/SymbolResolver",
    },
    windows = {
        sources = {
            "builtin-plugin/SymbolResolver/pe/**/*.cc",
        }
    },
    linux = {
        sources = {
            "builtin-plugin/SymbolResolver/elf/**/*.cc",
        }
    },
    macos = {
        sources = {
            "builtin-plugin/SymbolResolver/macho/**/*.cc",
        }
    }
}
if lm.os == "macos" then
    lm:source_set "dobbyImportTableReplace" {
        rootdir = "3rd/Dobby",
        includes = {
            Includes,
            "builtin-plugin/ImportTableReplace",
        },
        sources = "builtin-plugin/ImportTableReplace/dobby_import_replace.cc",
    }
end

lm:source_set "dobbyLogger" {
    rootdir = "3rd/Dobby",
    includes = {
        Includes,
        "builtin-plugin/ImportTableReplace",
    },
    sources = {
		"external/logging/logging.c",
		"external/logging/cxxlogging.cc",
	}
}

lm:source_set "dobby" {
    rootdir = "3rd/Dobby",
    deps = {
		"dobbyLogger",
        "dobbySymbolResolver",
        lm.os == "macos" and "dobbyImportTableReplace",
    },
    includes = Includes,
    defines = {
        "__DOBBY_BUILD_VERSION__=\\\"unknown\\\"",
        ARCH == "arm64" and "FULL_FLOATING_POINT_REGISTER_PACK",
        lm.mode == "debug" and "LOGGING_DEBUG",
    },
    sources = {
        -- cpu
        "source/core/arch/CpuFeature.cc",
        "source/core/arch/CpuRegister.cc",
        -- assembler
        "source/core/assembler/assembler.cc",
        "source/core/assembler/assembler-arm.cc",
        "source/core/assembler/assembler-arm64.cc",
        "source/core/assembler/assembler-ia32.cc",
        "source/core/assembler/assembler-x64.cc",
        -- codegen
        "source/core/codegen/codegen-arm.cc",
        "source/core/codegen/codegen-arm64.cc",
        "source/core/codegen/codegen-ia32.cc",
        "source/core/codegen/codegen-x64.cc",
        -- memory kit
        "source/MemoryAllocator/CodeBuffer/CodeBufferBase.cc",
        "source/MemoryAllocator/AssemblyCodeBuilder.cc",
        "source/MemoryAllocator/MemoryAllocator.cc",
        -- instruction relocation
        "source/InstructionRelocation/arm/InstructionRelocationARM.cc",
        "source/InstructionRelocation/arm64/InstructionRelocationARM64.cc",
        "source/InstructionRelocation/x86/InstructionRelocationX86.cc",
        "source/InstructionRelocation/x86/InstructionRelocationX86Shared.cc",
        "source/InstructionRelocation/x64/InstructionRelocationX64.cc",
        "source/InstructionRelocation/x86/x86_insn_decode/x86_insn_decode.c",
        -- intercept routing
        "source/InterceptRouting/InterceptRouting.cpp",
        -- intercept routing trampoline
        "source/TrampolineBridge/Trampoline/arm/trampoline_arm.cc",
        "source/TrampolineBridge/Trampoline/arm64/trampoline_arm64.cc",
        "source/TrampolineBridge/Trampoline/x86/trampoline_x86.cc",
        "source/TrampolineBridge/Trampoline/x64/trampoline_x64.cc",
        -- closure trampoline bridge - arm
        "source/TrampolineBridge/ClosureTrampolineBridge/common_bridge_handler.cc",
        "source/TrampolineBridge/ClosureTrampolineBridge/arm/helper_arm.cc",
        "source/TrampolineBridge/ClosureTrampolineBridge/arm/closure_bridge_arm.cc",
        "source/TrampolineBridge/ClosureTrampolineBridge/arm/ClosureTrampolineARM.cc",
        -- closure trampoline bridge - arm64
        "source/TrampolineBridge/ClosureTrampolineBridge/arm64/helper_arm64.cc",
        "source/TrampolineBridge/ClosureTrampolineBridge/arm64/closure_bridge_arm64.cc",
        "source/TrampolineBridge/ClosureTrampolineBridge/arm64/ClosureTrampolineARM64.cc",
        -- closure trampoline bridge - x86
        "source/TrampolineBridge/ClosureTrampolineBridge/x86/helper_x86.cc",
        "source/TrampolineBridge/ClosureTrampolineBridge/x86/closure_bridge_x86.cc",
        "source/TrampolineBridge/ClosureTrampolineBridge/x86/ClosureTrampolineX86.cc",
        -- closure trampoline bridge - x64
        "source/TrampolineBridge/ClosureTrampolineBridge/x64/helper_x64.cc",
        "source/TrampolineBridge/ClosureTrampolineBridge/x64/closure_bridge_x64.cc",
        "source/TrampolineBridge/ClosureTrampolineBridge/x64/ClosureTrampolineX64.cc",
        --
        "source/InterceptRouting/Routing/InstructionInstrument/InstructionInstrument.cc",
        "source/InterceptRouting/Routing/InstructionInstrument/RoutingImpl.cc",
        "source/InterceptRouting/Routing/InstructionInstrument/instrument_routing_handler.cc",
        --
        "source/InterceptRouting/Routing/FunctionInlineHook/FunctionInlineHook.cc",
        "source/InterceptRouting/Routing/FunctionInlineHook/RoutingImpl.cc",
        -- plugin register
        "source/InterceptRouting/RoutingPlugin/RoutingPlugin.cc",
        -- main
        "source/dobby.cpp",
        "source/Interceptor.cpp",
        "source/InterceptEntry.cpp",
        NearBranch and {
            "source/InterceptRouting/RoutingPlugin/NearBranchTrampoline/near_trampoline_arm64.cc",
            "source/InterceptRouting/RoutingPlugin/NearBranchTrampoline/NearBranchTrampoline.cc",
        },
        "source/MemoryAllocator/NearMemoryAllocator.cc",
    },
    windows = {
        sources = {
            -- platform util
            "source/Backend/UserMode/PlatformUtil/Windows/ProcessRuntimeUtility.cc",
            -- user mode - platform interface
            "source/Backend/UserMode/UnifiedInterface/platform-windows.cc",
            -- user mode - executable memory
            "source/Backend/UserMode/ExecMemory/code-patch-tool-windows.cc",
            "source/Backend/UserMode/ExecMemory/clear-cache-tool-all.c",
        },
    },
    linux = {
        sources = {
            -- platform util
            "source/Backend/UserMode/PlatformUtil/Linux/ProcessRuntimeUtility.cc",
            -- user mode - platform interface
            "source/Backend/UserMode/UnifiedInterface/platform-posix.cc",
            -- user mode - executable memory
            "source/Backend/UserMode/ExecMemory/code-patch-tool-posix.cc",
            "source/Backend/UserMode/ExecMemory/clear-cache-tool-all.c",
        },
    },
    macos = {
        sources = {
            -- platform util
            "source/Backend/UserMode/PlatformUtil/Darwin/ProcessRuntimeUtility.cc",
            -- user mode - platform interface
            "source/Backend/UserMode/UnifiedInterface/platform-posix.cc",
            -- user mode - executable memory
            "source/Backend/UserMode/ExecMemory/code-patch-tool-darwin.cc",
            "source/Backend/UserMode/ExecMemory/clear-cache-tool-all.c",
        },
    },
}
