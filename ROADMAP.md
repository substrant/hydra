# Hydra 1.0 - To-Do List and Roadmap

## Executive Summary
Hydra is a Windows-native library for process manipulation, memory management, PE parsing, and disassembly. The project has recently undergone significant refactoring to migrate from a legacy build system to CMake. While the migration is complete, several critical issues remain that prevent the codebase from building and reaching a professional, stable state.

**Current State:** ~2,500 lines of code across 15 header files and 8 source files. Build system migrated to CMake with Clang-CL/MSVC toolchain support.

---

## Critical Issues (Must Fix Immediately)

### 1. Broken Include Paths ⚠️ BLOCKING
**Priority:** P0  
**Status:** Not Started  
**Estimated Effort:** 2-4 hours

**Problem:** Multiple files reference non-existent directory structures:
- Files reference `hydra/mem/core.hpp`, `hydra/mem/module.hpp`, `hydra/mem/disasm.hpp`
- Files reference `hydra/sys/process.hpp`, `hydra/sys/window.hpp`, `hydra/sys/thread.hpp`, `hydra/sys/handle.hpp`, `hydra/sys/toolhelp.hpp`
- Files reference `hydra/detail/pch.hpp` which doesn't exist
- These directories (`mem/`, `sys/`, `detail/pch.hpp`) do not exist in the current structure

**Impact:** Build is completely broken - project cannot compile.

**Solution Options:**
1. **Option A (Recommended):** Create missing directories and reorganize headers
   - Create `include/hydra/mem/` directory
   - Move `memory.hpp` → `mem/core.hpp`
   - Create `mem/module.hpp` and `mem/disasm.hpp` by extracting from current headers
   - Create `include/hydra/sys/` directory
   - Move `process.hpp`, `window.hpp`, `thread.hpp` → `sys/`
   - Move `handle.hpp` → `sys/handle.hpp`
   - Create `detail/pch.hpp` with common includes

2. **Option B (Simpler):** Update all include paths to match current structure
   - Replace all `hydra/mem/` references with `hydra/`
   - Replace all `hydra/sys/` references with `hydra/`
   - Remove `hydra/detail/pch.hpp` includes or create the file

**Files Affected:**
- `include/hydra/process.hpp`
- `include/hydra/disasm.hpp`
- `include/hydra/module.hpp`
- `include/hydra/remote/toolhelp.hpp`
- `include/hydra/window.hpp`
- `src/module.cpp`
- `src/disasm.cpp`
- `src/io.cpp`
- `src/user/process.cpp`
- `src/user/thread.cpp`
- `src/user/window.cpp`

---

### 2. Missing Precompiled Header (pch.hpp)
**Priority:** P0  
**Status:** Not Started  
**Estimated Effort:** 1 hour

**Problem:** 6 files include `<hydra/detail/pch.hpp>` but this file doesn't exist.

**Files Referencing pch.hpp:**
- `include/hydra/process.hpp`
- `include/hydra/disasm.hpp`
- `include/hydra/remote/toolhelp.hpp`
- `include/hydra/window.hpp`
- `src/module.cpp`
- `src/user/process.cpp`

**Solution:** Create `include/hydra/detail/pch.hpp` with common Windows and STL includes:
```cpp
#pragma once

// Windows headers
#include <phnt_windows.h>
#include <phnt.h>

// STL headers
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <filesystem>
#include <optional>
#include <functional>
#include <ranges>
#include <algorithm>
```

---

### 3. Inconsistent Type Definitions
**Priority:** P1  
**Status:** Not Started  
**Estimated Effort:** 2 hours

**Problem:** The codebase uses both `buffer` and `region` types inconsistently.

**Observations:**
- `memory.hpp` defines `region` class
- `io/memory.hpp` references `buffer` type
- `module.hpp` uses `buffer` type
- `process.hpp` uses both `buffer` and `region`

**Solution:** 
1. Determine canonical type name (`buffer` vs `region`)
2. Create type alias if needed: `using buffer = region;`
3. Update documentation to clarify usage

---

## Phase 1: Build System Stabilization

### 1.1 Fix Build Configuration
**Priority:** P1  
**Status:** Not Started  
**Estimated Effort:** 2-3 hours

**Tasks:**
- [ ] Verify CMakeLists.txt properly links all dependencies (Zydis, Zycore, OpenSSL, phnt)
- [ ] Remove hardcoded Boost path (`C:\\Boost`) and make it configurable
- [ ] Add proper error handling for missing dependencies
- [ ] Test build on clean environment
- [ ] Document build prerequisites in README.md

**Notes:**
- Current CMakeLists.txt has hardcoded comment: "I fucking hate boost" at line 66
- Boost path is hardcoded to `C:\\Boost`

---

### 1.2 Create Proper Build Documentation
**Priority:** P2  
**Status:** Not Started  
**Estimated Effort:** 2 hours

**Tasks:**
- [ ] Create README.md with project overview
- [ ] Document build prerequisites (CMake version, compiler requirements, dependencies)
- [ ] Document build instructions for both MSVC and Clang-CL
- [ ] Add troubleshooting section
- [ ] Document PowerShell build script usage

---

### 1.3 Fix Test Infrastructure
**Priority:** P2  
**Status:** Not Started  
**Estimated Effort:** 1-2 hours

**Tasks:**
- [ ] Verify tests build correctly with `-DHYDRA_BUILD_TESTS=ON`
- [ ] Fix CMakeLists.txt line 84: test executable has same output name as library (`libhydra`)
- [ ] Create basic test cases for core functionality
- [ ] Add CI/CD configuration (GitHub Actions)

**Current Issue:**
```cmake
# Line 84 - Test executable has same name as library
set_target_properties(Substrant.HydraTests PROPERTIES
    OUTPUT_NAME "libhydra"  # Should be "hydra-tests" or similar
```

---

## Phase 2: Code Architecture Improvements

### 2.1 Reorganize Header Structure
**Priority:** P2  
**Status:** Not Started  
**Estimated Effort:** 4-6 hours

**Proposed Structure:**
```
include/hydra/
├── detail/           # Internal implementation details
│   ├── pch.hpp      # Precompiled header
│   └── ...
├── mem/             # Memory management
│   ├── core.hpp     # addr, region/buffer classes
│   ├── module.hpp   # PE image, remote_module
│   └── disasm.hpp   # Disassembly utilities
├── sys/             # System APIs
│   ├── process.hpp  # Process management
│   ├── thread.hpp   # Thread management
│   ├── window.hpp   # Window management
│   ├── handle.hpp   # Handle wrappers
│   └── toolhelp.hpp # Toolhelp32 wrappers
├── io/              # I/O subsystem
│   ├── stream.hpp   # Stream abstraction
│   ├── memory.hpp   # Memory stream
│   └── comm.hpp     # IPC communication
├── local/           # Local process operations
│   └── stream.hpp   # Local memory stream
└── remote/          # Remote process operations
    ├── stream.hpp   # Remote memory stream
    └── toolhelp.hpp # Remote process enumeration
```

**Benefits:**
- Clearer module boundaries
- Easier navigation
- Better encapsulation
- Consistent with include paths already in code

---

### 2.2 Standardize Naming Conventions
**Priority:** P2  
**Status:** Not Started  
**Estimated Effort:** 3-4 hours

**Tasks:**
- [ ] Audit all public API names for consistency
- [ ] Create naming convention guide (snake_case for functions, PascalCase for classes, etc.)
- [ ] Rename inconsistent identifiers
- [ ] Update documentation

**Current Issues:**
- Mix of `snake_case` and `camelCase`
- Some cryptic abbreviations (e.g., `mm_read`, `pe_mmap`)
- Inconsistent prefix usage

---

### 2.3 Improve Error Handling
**Priority:** P2  
**Status:** Not Started  
**Estimated Effort:** 4-5 hours

**Tasks:**
- [ ] Audit all functions that can fail
- [ ] Replace boolean returns with proper error types where appropriate
- [ ] Consider using `std::expected<T, Error>` (C++23) or custom Result type
- [ ] Document error conditions
- [ ] Add proper error messages

**Current Issues:**
- Many functions return `bool` with no error information
- Some throw exceptions, some return `std::optional`, some return `bool`
- No consistent error handling strategy

---

### 2.4 Add Comprehensive Documentation
**Priority:** P2  
**Status:** Not Started  
**Estimated Effort:** 8-10 hours

**Tasks:**
- [ ] Add Doxygen-style comments to all public APIs
- [ ] Document pre/post conditions
- [ ] Add usage examples
- [ ] Create architecture documentation
- [ ] Document thread-safety guarantees
- [ ] Add performance considerations

**Current State:**
- Minimal documentation exists
- Some comments are present but inconsistent
- No architecture overview

---

## Phase 3: Code Quality Improvements

### 3.1 Memory Safety Audit
**Priority:** P1  
**Status:** Not Started  
**Estimated Effort:** 6-8 hours

**Tasks:**
- [ ] Audit all raw pointer usage
- [ ] Verify proper RAII for resources
- [ ] Check for memory leaks (use AddressSanitizer)
- [ ] Verify proper lifetime management
- [ ] Add static analysis (clang-tidy, cppcheck)

**Areas of Concern:**
- `addr` union with raw pointers
- Manual memory management in `region` class
- Buffer operations in I/O subsystem

---

### 3.2 Thread Safety Review
**Priority:** P1  
**Status:** Not Started  
**Estimated Effort:** 4-6 hours

**Tasks:**
- [ ] Audit all shared state
- [ ] Document thread-safety guarantees
- [ ] Add proper synchronization where needed
- [ ] Consider atomic operations for performance-critical paths
- [ ] Add thread-safety tests

**Current State:**
- `comm_peer` uses atomics but unclear if properly synchronized
- Process/module caching in `process` class may have race conditions
- No documented thread-safety guarantees

---

### 3.3 Add Comprehensive Testing
**Priority:** P2  
**Status:** Not Started  
**Estimated Effort:** 10-12 hours

**Tasks:**
- [ ] Unit tests for memory management (`addr`, `region`)
- [ ] Unit tests for PE parsing
- [ ] Integration tests for process operations
- [ ] Tests for disassembly engine
- [ ] Tests for I/O streams
- [ ] Add code coverage measurement

**Current State:**
- Only placeholder test files exist (`tests/src/entry.cpp`)
- No actual test cases

---

### 3.4 Performance Optimization
**Priority:** P3  
**Status:** Not Started  
**Estimated Effort:** 6-8 hours

**Tasks:**
- [ ] Profile hot paths
- [ ] Optimize memory allocations
- [ ] Consider caching frequently accessed data
- [ ] Optimize pattern scanning algorithms
- [ ] Add benchmarks

**Potential Optimizations:**
- `region::scan_aob` could be vectorized (SIMD)
- Module caching is already implemented but may need tuning
- Stream operations could batch I/O

---

## Phase 4: Feature Completeness

### 4.1 Complete I/O Communication System
**Priority:** P2  
**Status:** Partial Implementation  
**Estimated Effort:** 8-10 hours

**Current State:**
- `io/comm.hpp` has detailed design documentation
- Only basic `comm_peer::send()` implemented
- Missing: receive, master peer, peer registry, round-robin scheduling

**Tasks:**
- [ ] Implement `comm_peer::receive()`
- [ ] Implement master peer class
- [ ] Implement peer registry and discovery
- [ ] Implement round-robin scheduling
- [ ] Add synchronization primitives
- [ ] Add tests and examples

---

### 4.2 Enhance PE Module Features
**Priority:** P3  
**Status:** Basic Implementation  
**Estimated Effort:** 6-8 hours

**Tasks:**
- [ ] Implement full import table parsing
- [ ] Add export table parsing
- [ ] Add relocation support
- [ ] Implement manual mapping (complete `pe_mmap`)
- [ ] Add PE file writing/modification
- [ ] Add digital signature verification

**Current State:**
- Basic PE header parsing works
- Section enumeration works
- Import table parsing incomplete
- Manual mapping incomplete (`map_status` enum exists)

---

### 4.3 Enhance Disassembly Features
**Priority:** P3  
**Status:** Basic Implementation  
**Estimated Effort:** 4-6 hours

**Tasks:**
- [ ] Add more pattern matching utilities
- [ ] Add control flow analysis
- [ ] Add function boundary detection
- [ ] Add instruction emulation for simple cases
- [ ] Improve error handling

**Current State:**
- Basic disassembly works (Zydis integration)
- Pattern matching exists
- Code query API exists

---

### 4.4 Add Process Injection Capabilities
**Priority:** P3  
**Status:** Basic Implementation  
**Estimated Effort:** 8-10 hours

**Tasks:**
- [ ] Complete manual mapping implementation
- [ ] Add shellcode injection utilities
- [ ] Add thread hijacking support
- [ ] Add stealth injection techniques
- [ ] Document security implications

**Current State:**
- `mm_inject` exists for basic injection
- Manual mapping incomplete
- No advanced injection techniques

---

## Phase 5: Polish and Release

### 5.1 Code Cleanup
**Priority:** P2  
**Status:** Not Started  
**Estimated Effort:** 4-6 hours

**Tasks:**
- [ ] Remove profanity from comments (e.g., "I fucking hate boost", "bruh")
- [ ] Standardize comment style
- [ ] Remove dead code
- [ ] Fix TODO comments
- [ ] Run code formatter (clang-format)

**Current Issues:**
```cpp
// Line 66, CMakeLists.txt
PUBLIC include "C:\\Boost" # I fucking hate boost

// Line 39, src/module.cpp
if (!proc->mm_read(base, buffer)) throw std::runtime_error("bruh");
```

---

### 5.2 Security Audit
**Priority:** P1  
**Status:** Not Started  
**Estimated Effort:** 6-8 hours

**Tasks:**
- [ ] Review all unsafe operations
- [ ] Audit privilege requirements
- [ ] Document security considerations
- [ ] Add input validation
- [ ] Consider adding security safeguards (e.g., prevent self-injection)
- [ ] Add NTSTATUS error checking everywhere

**Areas of Concern:**
- Process manipulation capabilities can be dangerous
- Memory injection features need careful review
- Remote memory access validation

---

### 5.3 Create Examples and Samples
**Priority:** P2  
**Status:** Not Started  
**Estimated Effort:** 6-8 hours

**Tasks:**
- [ ] Create "Hello World" example
- [ ] Add process enumeration example
- [ ] Add memory scanning example
- [ ] Add PE parsing example
- [ ] Add disassembly example
- [ ] Add IPC communication example

---

### 5.4 Licensing and Legal
**Priority:** P1  
**Status:** Not Started  
**Estimated Effort:** 1-2 hours

**Tasks:**
- [ ] Add LICENSE file
- [ ] Add copyright headers to all files
- [ ] Document third-party licenses
- [ ] Add NOTICE file for dependencies
- [ ] Add ethical use disclaimer

---

### 5.5 Release Preparation
**Priority:** P1  
**Status:** Not Started  
**Estimated Effort:** 2-3 hours

**Tasks:**
- [ ] Version numbering scheme
- [ ] Create CHANGELOG.md
- [ ] Create release checklist
- [ ] Package release binaries
- [ ] Create GitHub release

---

## Dependencies and External Libraries

### Current Dependencies
- **Zydis** (v4.1.1): Disassembly engine
- **phnt**: Windows Native API headers
- **OpenSSL**: Cryptography (used but unclear where)
- **Boost**: Listed but unclear usage (needs audit)

### Dependency Management Tasks
- [ ] Audit actual Boost usage - can it be removed?
- [ ] Verify OpenSSL usage - is it needed?
- [ ] Document minimum dependency versions
- [ ] Consider vendoring small dependencies
- [ ] Create dependency installation guide

---

## Technical Debt Items

### High Priority
1. Resolve `buffer` vs `region` naming inconsistency
2. Fix hardcoded Boost path
3. Standardize error handling approach
4. Add proper logging/tracing infrastructure

### Medium Priority
5. Consider replacing raw NTSTATUS with wrapper types
6. Audit all `reinterpret_cast` usage for safety
7. Add static analysis to CI/CD
8. Consider using C++23 features (std::expected, std::mdspan)

### Low Priority
9. Optimize include dependencies (reduce compile times)
10. Consider module boundaries for future C++20 modules support
11. Profile and optimize allocator usage
12. Add support for ARM64 Windows

---

## Estimated Timeline

### Immediate (Week 1-2)
- Fix broken includes and build system
- Create pch.hpp
- Fix critical bugs preventing compilation
- Basic documentation

### Short Term (Month 1)
- Complete Phase 1 (Build System Stabilization)
- Complete Phase 2 (Code Architecture Improvements)
- Begin Phase 3 (Code Quality Improvements)

### Medium Term (Months 2-3)
- Complete Phase 3 (Code Quality Improvements)
- Complete Phase 4 (Feature Completeness)
- Begin Phase 5 (Polish and Release)

### Long Term (Months 4-6)
- Complete Phase 5 (Polish and Release)
- Ongoing maintenance and feature additions
- Community feedback and iteration

---

## Success Criteria

### Version 1.0 Requirements
- ✅ Migrated to CMake build system (DONE)
- [ ] Builds cleanly with MSVC and Clang-CL
- [ ] Zero compiler warnings at `/W3`
- [ ] Comprehensive documentation
- [ ] Test coverage > 70%
- [ ] All public APIs documented
- [ ] Example code for all major features
- [ ] Security audit complete
- [ ] License and legal compliance

### Quality Metrics
- No memory leaks (verified with sanitizers)
- No undefined behavior (verified with UBSan)
- Clean static analysis (clang-tidy, cppcheck)
- Consistent code style
- Professional-quality documentation

---

## Open Questions

1. **Target Audience**: Who is the primary user of this library? Internal tools? External developers?
2. **Stability Guarantees**: What API stability guarantees will 1.0 provide?
3. **Platform Support**: Windows 10+ only? Windows 7/8 support needed?
4. **Architecture Support**: x64 only? x86 support needed? ARM64?
5. **Boost Dependency**: Is Boost actually used? Can it be removed?
6. **OpenSSL Usage**: Where is OpenSSL used? Can it be made optional?
7. **IPC Design**: Should the `comm` system be completed or deferred to 1.1?
8. **Injection Features**: Should advanced injection be in 1.0 or later?

---

## Notes

- This roadmap is based on analysis of the current codebase (~2,500 LOC)
- Estimates are rough and may need adjustment based on actual implementation
- Priority levels: P0 (blocking), P1 (critical), P2 (important), P3 (nice-to-have)
- The codebase shows evidence of significant refactoring work already done
- Build system migration appears complete but untested
- Code quality is generally good but needs polish for production use

---

**Last Updated:** 2025-11-12  
**Document Version:** 1.0
