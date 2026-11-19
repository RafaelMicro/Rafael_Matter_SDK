# AI Agent Guidelines for the Rafael RT58x Matter SDK

This file provides guidelines and instructions for AI agents working on this
codebase: the Rafael Micro RT58x fork of the CSA Matter SDK
(project-chip/connectedhomeip).

## General Principles

- **When in Rome**: Match the prevailing style of the code being modified. See
  [docs/style/CODING_STYLE_GUIDE.md](docs/style/CODING_STYLE_GUIDE.md).
- **Atomicity**: Make small, incremental changes. Do not mix refactoring with
  feature implementation.
- **No Filler Names**: Avoid names like "support", "common", "helpers",
  "util", "core". Use concrete names.
- **Error Handling**: Use `CHIP_ERROR` as the standard return type for
  fallible operations. Prefer `VerifyOrReturnError` and `ReturnErrorOnFailure`
  macros for concise error checking and propagation.
- Ensure resources are cleaned up appropriately, especially on early returns.
  Generally prefer RAII patterns for cleanup.
- **Logging**: Use the `ChipLog*` macros (e.g., `ChipLogProgress`,
  `ChipLogError`, `ChipLogDetail`) for logging. Ensure logs are appropriately
  categorized by module (e.g., `AppServer`, `InteractionModel`).

## Ignored Directories

When searching for files or code patterns, ignore the following directories
unless explicitly asked to look there:

- `third_party/` (contains external dependencies), **except** for the
  Rafael-authored trees, which are first-party code and must be searched like
  the rest of the repository:
  - `third_party/rafael/` (RT58x SDK, drivers, BLE stack)
  - `third_party/rafael/repo/components/network/thread/` (Rafael OpenThread
    port, a submodule nested in `components`; also reachable through the
    `third_party/openthread/components-thread` symlink)
- `out/` (contains build artifacts)

Rafael-authored code also lives outside `third_party/`, mainly in
`examples/*/rt58x/`, `examples/platform/rt58x/` and `src/platform/RT58x/`. The
`upstream-sync` skill has the full inventory.

## Code Review Instructions

- Do not comment on content for XML files or .matter content for clusters.
- The SDK implements an in-progress Matter specification that may be in flux
  and may not be available to all contributors. Assume the Matter
  specification is unknown and out of scope _unless_ you have explicit access
  to the latest version (see the `matter-specification-access` skill).
- Avoid "pat on the back" style comments that just restate what the code is
  doing. Focus on suggesting concrete code improvements.
- Be concise. Do not over-explain code.
- Look for common typos and suggest fixes.
- Do not comment on whitespace or formatting (auto-formatters handle this).
- Review changes for embedded development. The RT58x is a flash- and
  RAM-constrained Cortex-M target, so this is the dominant concern here:
  - Minimize use of heap allocation.
  - Optimize for resource usage (RAM/Flash). The `binary-size-comparison`
    skill covers measuring the footprint impact of a change.
  - Be cautious with complex templates that could lead to code bloat.
  - Watch for large stack frames and blocking calls in ISR or Thread
    stack context.

## API preferences

- Prefer using `chip::Span` from `src/lib/support/Span.h` to pointer + size
  groups. Pass `Span` by value rather than const reference (treat it as a
  `string_view`)
- Use `"foo"_span` (i.e. `operator _span`) for const char spans instead of
  `fromCharString`.
- Prefer `std::optional` to `chip::Optional`
- Prefer `StringBuilder` from `src/lib/support/StringBuilder.h` to using
  `snprintf` for string formatting.

## Coding Style (Highlights)

Refer to [docs/style/CODING_STYLE_GUIDE.md](docs/style/CODING_STYLE_GUIDE.md)
for full details.

- **C++**: C++17 standard.
  - Use fixed-width integer types from `<cstdint>` for POD integer types
  - Avoid top-level `using namespace` in headers.
  - Use anonymous namespaces for file-internal classes/objects.
  - Avoid heap allocation and auto-resizing containers in core SDK.
- **Python**: Python 3.11 standard.
  - Use type hints on public APIs.
  - Include docstrings for public APIs.
- _Always_ include `{}` bracketing for control flows, even if using one liners
  (e.g. for `if`, `while`, `for` and such)

## Architectural Constraints

### Code-Driven Clusters

Code-driven clusters are implementations in `src/app/clusters` that use
`DefaultServerCluster` as a base class. When developing them:

- `ReadAttribute`, `WriteAttribute`, and `InvokeCommand` are by API contract
  only called for existent paths. Do not add path validity checks — they
  increase code size and are redundant as long as `Attributes` or
  `AcceptedCommands` are correct.
- Ember APIs and generated ZAP accessors must not be used outside the
  `CodegenIntegration` layer. `CodegenIntegration.h/cpp` is the documented
  bridge between generated configuration and code-driven cluster logic. Avoid
  types like `EmberAfStatus` or functions like `emberAfContainsServer`,
  `emberAfReadAttribute`, or `emberAfWriteAttribute` in core cluster code.
- When adding files: codegen-specific files belong in
  `app_config_dependent_sources.cmake/gni`; all others belong in `BUILD.gn`.
  Ensure every file (especially headers) is listed in one of these — there
  should be no unreferenced files.

## Common Commands

Most commands require an activated environment.

### Environment Activation

You can run commands within the environment using `scripts/run_in_build_env.sh`:
`scripts/run_in_build_env.sh "command"`

Alternatively, you can activate the environment in your shell:
`source scripts/activate.sh`

### Building RT58x Firmware

**Do not start a firmware build or flash the device unless explicitly asked.**
Builds are slow and the maintainer normally runs them (and the ISP flashing
step) themselves. Make the code change, then say what needs to be rebuilt.

When a build is explicitly requested:

```bash
./scripts/examples/gn_rt58x_example.sh ./examples/<project-name>/rt58x ./out/<project-name> <board> [--sed|--ssed] [--certs=<yes|dac-only|no>]
```

- `<project-name>`: `contact-sensor-app`, `humidity-sensor-app`,
  `light-switch-app`, `lighting-app`, `lighting-zigbee`, `lock-app`,
  `occupancy-sensor-app`, `smart-plug-app`, `smoke-alarm-app`,
  `temperature-measurement-app`, `thermostat`, `water-valve-app`,
  `window-app`
- `<board>`: `RT583`, `RT584H`, `RT584L`, `RT584HA4`
- `--sed` / `--ssed`: Sleepy / Synchronized Sleepy End Device. Without either,
  the app is built as a non-sleepy Thread FTD router.
- Per-example GN arguments live in `examples/<project-name>/rt58x/args.gni`.

To build for a different board or after a configuration change, remove
`./out/<project-name>` first.

### 

## Development Resources

### Rafael RT58x

- [rafael_docs/matter_env_setup.md](rafael_docs/matter_env_setup.md) -
  toolchain and SDK setup
- [rafael_docs/Rafael_App_File_Structure.md](rafael_docs/Rafael_App_File_Structure.md) -
  what each file of an RT58x example does
- [rafael_docs/Flash_Layout_Customer_Guide.md](rafael_docs/Flash_Layout_Customer_Guide.md) -
  flash partitioning per chip
- [rafael_docs/Rafael_Factory_Data.md](rafael_docs/Rafael_Factory_Data.md) -
  device attestation credentials
- [rafael_docs/Rafael_Software_Update.md](rafael_docs/Rafael_Software_Update.md) -
  OTA image build and delivery
- [rafael_docs/Rafael_Sign_Image.md](rafael_docs/Rafael_Sign_Image.md) -
  signing application images for secure boot
- [rafael_docs/Rafael_Submodules.md](rafael_docs/Rafael_Submodules.md) -
  submodule layout of the fork
- Each example also has its own `examples/<project-name>/rt58x/README.md`.

### Upstream

- [docs/guides/writing_clusters.md](docs/guides/writing_clusters.md)
- [docs/guides/migrating_ember_cluster_to_code_driven.md](docs/guides/migrating_ember_cluster_to_code_driven.md)
- [docs/testing/unit_testing.md](docs/testing/unit_testing.md)
- [docs/testing/integration_tests.md](docs/testing/integration_tests.md)
