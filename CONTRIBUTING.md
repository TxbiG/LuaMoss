# Contributing to LuaMoss

Thank you for contributing to **LuaMoss**, the Lua 5.4 module for Moss Framework.

LuaMoss uses Lua's native C API directly rather than relying on generated bindings with unclear C++ ownership. Changes should therefore preserve clear ownership, lifetime, conversion, and error semantics across the Lua/native boundary.

## Before You Start

For public API, ownership, or binding-architecture changes, open an issue first when the change is substantial.

Please check existing issues and pull requests before starting work.

## Development Requirements

- Git
- CMake
- C++17
- Lua 5.4
- A local Moss Framework checkout

## Building

```bash
git clone https://github.com/TxbiG/LuaMoss.git
cd LuaMoss

cmake -S . -B build
cmake --build build
```

When Moss is not in the default location, use the repository's supported Moss framework path configuration.

## Repository Structure

- `src/` — Lua binding implementation
- `docs/` — documentation
- `examples/` — Lua examples
- `performance/` — benchmarks
- `build/workflows/` — CI/build configuration

## Areas for Contribution

- Lua API coverage
- Moss API bindings
- Type conversion
- Ownership and lifetime handling
- Error handling
- Lua userdata/resource management
- Performance
- Examples
- Documentation
- Build and CI support
- Tests and regression coverage

## Binding Guidelines

Prefer Lua-facing APIs that feel natural to Lua while preserving Moss semantics.

Pay particular attention to:

- Lua `nil` handling
- Numeric conversions
- String lifetime
- Userdata
- Native ownership
- Resource destruction
- Error propagation
- Threading restrictions

Do not expose native ownership assumptions in a way that can produce dangling userdata or double destruction.

## Public API Changes

When adding a Moss API:

1. Add the binding.
2. Document the Lua-facing behaviour.
3. Add or update an example where useful.
4. Add a regression test when practical.
5. Verify ownership and lifetime rules.

## Testing

Test both successful and invalid Lua calls, including:

- Missing arguments
- Incorrect Lua types
- Invalid handles/resources
- Resource destruction
- Repeated creation/destruction
- Error propagation
- Boundary-value conversions

## Commit Messages

Recommended prefixes:

```text
feat: expose Moss audio API
fix: handle nil texture safely
docs: add Lua rendering example
test: cover resource lifetime
perf: reduce Lua allocation overhead
build: improve Lua discovery
```

## Pull Requests

Include:

- Description of the change
- Lua usage example where appropriate
- Tests performed
- Moss commit/version used
- Platform information when relevant
- API compatibility notes

## Licence

LuaMoss is distributed under the **MIT License**. Contributions should be compatible with the repository's licence and applicable Lua/Moss/third-party licence requirements.

Thank you for contributing.
