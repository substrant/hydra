# C++ Syntax and Naming Rules

These instructions apply only to first-party C++ (`include/hydra/**`, `src/**`, and project tests). Do not infer style from `dependencies/**`, generated code, Windows APIs, Zydis, or other third-party declarations.

This file governs syntax, spelling, naming, and formatting only. It does not prescribe architecture, ownership models, inheritance, API design, algorithms, or project behavior.

## Precedence

1. Preserve names required by an external API or an overridden declaration exactly.
2. When editing an existing declaration or block, match its immediately surrounding syntax exactly.
3. Otherwise, follow the rules below.

Do not restyle unrelated code.

## Identifier Case

Use `lower_snake_case` for all project-defined identifiers except concepts, template parameters, macros, and status constants.

```cpp
namespace hy::example {
    enum class stream_mode : std::uint8_t {
        absolute,
        relative
    };

    struct decoded_ins {
        std::size_t operand_count;
    };

    class byte_stream {
        bool m_owner;

    public:
        std::size_t read_bytes(std::int8_t* dst, const std::size_t size);
    };
}
```

### Namespaces

- Name namespaces in lowercase: `hy`, `dtl`, `nt`, `shim`, `scn`, `impl`.
- Join nested namespaces with `::`: `namespace hy::dtl {`.
- Use `namespace {` for translation-unit-local helpers.
- Do not introduce CamelCase namespace names.

### Types

- Name classes, structs, enums, and aliases in `lower_snake_case`: `stm`, `blkstm`, `procstm`, `mem_mode`, `disasm_ins`, `pid_t`, `return_t`.
- Keep established short forms short. Do not expand names such as `blk`, `stm`, `proc`, `mod`, `seg`, `ptr`, `err`, `hnd`, `ins`, `func`, `nt`, or `dtl` in adjacent APIs.
- Use `_t` only when the name denotes a type in the same style as `pid_t`, `return_t`, `args_t`, `arg_t`, and `sig_t`. Do not append `_t` mechanically to classes or structs.
- Name scoped and unscoped enum types in `lower_snake_case`.
- Name ordinary enum values in lowercase: `begin`, `current`, `relative`, `mapped`, `read`, `write`.
- Preserve external type names exactly: `HANDLE`, `NTSTATUS`, `ZydisDecoder`, `UNICODE_STRING`.

### Concepts

Concept names are the deliberate exception to lowercase type names. Use `PascalCase` without underscores:

```cpp
template <class T>
concept PrimitivePtr = /* ... */;

template <typename F>
concept HndCal = /* ... */;
```

Follow existing acronym casing: `StmLike`, `HydraPtr`, `HndRet`, `Byte`, `Void`.

### Functions and Methods

- Name functions and methods in `lower_snake_case`: `clone_move`, `calc_size`, `step_func`, `format_ins`, `unicode_to_string`.
- Keep acronym components lowercase: `scan_aob`, `mm_read`, `open_hnd`, `is_rsp`.
- Use verb-led names for operations: `read`, `write`, `seek`, `allocate`, `reset`, `parse`.
- Use `is_` for ordinary predicates where it reads naturally: `is_padding`, `is_aligned`, `is_terminal`. Preserve established alternatives such as `not_null` and `contains`.
- Spell overloaded operators as C++ requires: `operator|`, `operator bool`, `operator*`.
- Do not introduce PascalCase or camelCase function names.

### Variables, Parameters, and Fields

- Name locals, parameters, and fields in `lower_snake_case`: `remote_base`, `physical_offset`, `result_max_size`, `bytes_read`.
- Prefix private backing state in a new non-aggregate class with `m_`: `m_owner`, `m_stream`, `m_decoder`.
- When adding a field to an existing type, copy that type's sibling-field convention exactly. Some record-like and platform-facing types intentionally use unprefixed fields such as `base`, `size`, `dos`, `nt`, `pid`, and `image_base`.
- Do not use trailing underscores as a general naming scheme. Do not use `this_`, Hungarian notation, `kConstant`, `s_`, or `g_` for project-defined names.
- Use concise established role names:
  - `src` / `dst` for source and destination.
  - `ref` for a referenced object pointer.
  - `arr` for an array.
  - `lhs` / `rhs` for binary operands.
  - `it` for an iterator.
  - `n` for a small byte or element count when the surrounding operation makes the unit obvious.
  - `count`, `size`, `offset`, `base`, `mode`, `state`, `status`, `error`, and `result` otherwise.
- Add semantic qualifiers before the role: `local_dst`, `remote_src`, `new_size`, `old_offset`, `func_end`.
- A trailing underscore may disambiguate a short-lived local from an existing name when established nearby: `maybe_`, `proc_`.
- One-letter runtime names are limited to conventional tiny scopes or union alternatives already following that convention (`i`, `p`, `d`, `u`). Do not use arbitrary one-letter names for ordinary state.

### Constants and Macros

- Name local and namespace-scope `constexpr` objects in `lower_snake_case`: `max_ins_size`, `result_max_size`, `protection_map`.
- Name project macros in uppercase snake case with the established prefix: `HY_OS_NT`, `HY_LIBRARY`, `HYDRA_INTERNAL`.
- Name status/error enumerators in uppercase snake case with their category prefix: `STA_SUCCESS`, `STA_PARTIAL_READ`, `ERR_NATIVE_ERROR`.
- Preserve third-party and platform constants exactly: `PAGE_READONLY`, `ZYDIS_MNEMONIC_NOP`.

### Template Parameters

- Use a single uppercase letter for a generic type when its role is obvious: `T`, `U`, `R`, `A`, `F`.
- Use `PascalCase` for descriptive type and non-type template parameters: `Base`, `Derived`, `Element`, `Size`, `Lhs`, `Rt`, `CloseFn`, `SuccessVal`.
- Name function parameters introduced with abbreviated templates like ordinary parameters: `auto&&... args`, `const dtl::Byte auto* in_mask`.
- Keep pack spacing consistent with the project:

```cpp
template <class R, class ...A>
struct func_traits;

template <class ...U>
ctor_shim(U&& ...x);
```

## Files and Includes

- Use lowercase file names. Separate components with underscores: `proc_nt.cpp`, `mod_nt.hpp`.
- Use `.hpp` for headers and `.cpp` for source files.
- Start headers with `#pragma once`.
- Use the path spelling `hydra/<name>.hpp`.
- Keep includes in contiguous logical groups separated by one blank line: corresponding/project headers, third-party platform headers, and standard-library headers. Match the current file's group order.
- Never rename or reformat third-party headers under `dependencies/**`.

## C++ Declarator Syntax

- Attach `*` and `&` to the type: `stm* stream`, `const blk& block`, `T&& value`.
- Put `const` before the type: `const std::size_t size`, `const ptr base`.
- Add top-level `const` to scalar/value parameters in definitions and inline implementations when the value is not reassigned. Pure virtual declarations may omit that top-level `const`, matching the existing interface style.
- Prefer `const auto` for an inferred local that is not reassigned; use `auto` when it is reassigned.
- Put the return type before the function name. Do not introduce trailing return types except where the type expression itself requires the existing `auto(...) -> R` form.
- Use explicit casts (`static_cast`, `reinterpret_cast`, `dynamic_cast`) for new code. Preserve required external/API spellings and nearby legacy casts without spreading them.
- Put attributes before the declaration: `[[nodiscard]] std::size_t read(...)`.
- Put `explicit`, `static`, `virtual`, and `constexpr` before the return/type portion; put `const`, `noexcept`, `override`, and `final` after the declarator in normal C++ order.

## Layout

- Indent with four spaces. Do not use tabs.
- Put opening braces on the same line for namespaces, types, functions, lambdas, and control statements.
- Put one space between a control keyword and `(`: `if (...)`, `for (...)`, `switch (...)`.
- Do not put a space between a function name and `(`.
- Put spaces around binary operators and after commas.
- Place access labels one indentation level less than members:

```cpp
class example {
    bool m_ready;

public:
    void reset();
};
```

- Omit braces for a single short controlled statement, matching the dominant project form:

```cpp
if (!m_owner)
    return;
```

- A short guard may remain on one line: `if (!count) return 0;`.
- Always use braces for multi-statement bodies.
- Place `else` on its own line after a closing brace, matching existing code:

```cpp
if (condition) {
    act();
}
else {
    recover();
}
```

- Indent `case` labels at the same level as the `switch` statement's body indentation used by existing files.
- Keep trivial functions, constructors, destructors, and operators on one line when the complete definition remains easy to scan:

```cpp
[[nodiscard]] bool not_null() const { return value != nullptr; }
~proc() override { }
```

- For a multiline function call, put one argument per line and align all arguments one indentation level inside the call:

```cpp
NtReadVirtualMemory(
    hnd,
    remote_src,
    local_dst,
    size,
    &bytes_read
);
```

- Keep a short initializer list on the declaration line. For a multiline initializer list, put `:` at the end of the constructor declaration line or on the following indented line as already used in the file, then place one initializer per line. Local consistency wins.
- Separate logical sections with one blank line. Do not add vertical whitespace between tightly related one-line declarations.
- Do not align unrelated declarations with manual columns. Preserve intentional local alignment in compact constructor/operator groups rather than introducing it elsewhere.

## Declarations and Definitions

- Define non-template, non-trivial methods in the matching `.cpp` with qualified `lower_snake_case` names:

```cpp
std::unique_ptr<stm> blkstm::clone_move() {
    return std::make_unique<blkstm>(std::move(*this));
}
```

- Keep templates, concepts, and short inline bodies in headers.
- Use `inline` only where the surrounding header already uses it explicitly; do not add it mechanically to every in-class definition.
- End class, struct, and enum declarations with `;`.
- Use `using` aliases, not new `typedef` declarations.

## Comments

- Use `///` for API documentation immediately above the declaration.
- Use `//` for implementation notes.
- Write comments as sentences when they explain behavior. Do not encode architecture guidance in comments added solely to satisfy this file.
- Preserve tool annotations exactly in the established form: `// NOLINT: reason` or `/* NOLINT: reason */`.

## Review Checklist

Before finishing any C++ edit, check every new or renamed identifier:

- Project type, alias, enum type: `lower_snake_case`.
- Concept: `PascalCase`.
- Function, method, namespace, variable, parameter, field: `lower_snake_case`.
- New private class state: `m_lower_snake_case`, unless sibling fields establish a different local convention.
- Ordinary enum value: lowercase.
- Status/error enumerator and macro: `UPPER_SNAKE_CASE` with the established prefix.
- Template parameter: uppercase single letter or descriptive `PascalCase`.
- Acronyms inside project identifiers: lowercase (`aob`, `mm`, `nt`, `rsp`, `hnd`).
- External identifiers: unchanged.
