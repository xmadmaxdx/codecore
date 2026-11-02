# AGENTS.md — CodeCore

## 1. Project Overview

**CodeCore** is a lightweight, modular, embeddable code editor and IDE framework designed for integration into Flutter and Kotlin/Android applications.

Its goal is to provide a powerful, customizable, offline-capable editing experience without forcing host applications to bundle a complete IDE or compiler toolchain.

CodeCore will eventually support:

- Advanced code editing.
- Syntax highlighting and incremental parsing.
- Autocomplete and code intelligence.
- Language Server Protocol (LSP).
- AI-powered inline ghost-text suggestions.
- Downloadable language support packages.
- Local offline execution and compilation.
- Terminal, debugging, and project management.
- Flutter and native Android/Kotlin integrations.

### Core principles

1. **Modular:** Features must be independently configurable and extensible.
2. **Offline-first:** Installed components should continue working without internet access.
3. **Lightweight:** Keep initial integration and startup overhead as small as possible.
4. **Cross-platform:** Maintain consistent behavior and API contracts across supported platforms.
5. **Embeddable:** Host applications must control their own UI and application-specific behavior.
6. **Extensible:** New languages, runtimes, providers, and tools must be addable without rewriting the engine.
7. **Secure:** Downloaded packages and executed code must be treated as untrusted.
8. **Performance-oriented:** Editing must remain responsive, including on low-end Android devices.

---

## 2. Development Phases

Development is divided into two major phases.

### Phase A — Editor and Intelligence

Phase A is the first development target.

Implement:

- Text editing engine.
- Document and text-buffer management.
- Cursor and selection management.
- Undo/redo.
- Syntax highlighting.
- Tree-sitter integration.
- Bracket matching and code folding.
- Local completion.
- Diagnostics infrastructure.
- Search and navigation.
- LSP client.
- AI completion provider interface.
- Ghost-text suggestions.
- Configuration system.
- Downloadable language-resource packages.
- Flutter SDK.
- Kotlin/Android SDK.
- Host-controlled execution API.

**Important:** Phase A does not include a complete built-in compiler or runtime system.

The host application supplies its own execution implementation through the public execution API.

### Phase B — Local IDE and Execution

Phase B starts only after Phase A has a stable architecture and usable release.

Potential features:

- Embedded Python interpreter.
- JavaScript runtime.
- Additional language runtimes.
- JVM and Java/Kotlin execution.
- Native compilers and toolchains.
- Terminal and process management.
- Debug Adapter Protocol (DAP).
- Build systems.
- Project management.
- Local dependency management.
- Git integration.

Do not prematurely implement Phase B functionality while foundational Phase A components remain incomplete.

---

## 3. Required Architecture

### 3.1 Shared native core

The primary shared engine should be implemented in **C++**.

Expose a stable C-compatible interface using:

`extern "C"`

Do not expose compiler-specific C++ classes or C++ ABI details directly to external SDKs.

The native core should contain platform-independent functionality such as:

- Document management.
- Text-buffer operations.
- Cursor and selection logic.
- Undo/redo.
- Parsing coordination.
- Completion infrastructure.
- Diagnostics models.
- Configuration.
- Language-provider metadata.

Keep Android-specific and Flutter-specific behavior outside the shared engine wherever practical.

### 3.2 Platform integrations

**Flutter**

- Dart SDK.
- Dart FFI for native-core communication.
- Flutter-native widgets and rendering.
- Platform channels only where genuinely necessary.

**Kotlin/Android**

- Kotlin SDK.
- JNI bridge to the native core.
- Native Android Views and/or Jetpack Compose integration.
- Android-specific lifecycle, filesystem, input, and accessibility handling.

Both integrations must expose equivalent public capabilities where technically possible.

Do not force identical UI implementations across Flutter and Android.

### 3.3 Parsing

Use Tree-sitter for incremental syntax parsing and syntax-aware editor features.

Tree-sitter is not a replacement for semantic language intelligence.

Use LSP or dedicated language providers for features such as:

- Type-aware completion.
- Go to definition.
- References.
- Rename.
- Semantic diagnostics.
- Signature help.

### 3.4 AI integration

AI completion must be provider-independent.

Do not hardcode a specific AI vendor, model, endpoint, or API key into the engine.

The provider interface should support:

- Remote providers.
- OpenAI-compatible APIs.
- Custom application-provided providers.
- Future local models.

AI suggestions must remain separate from ordinary document text until accepted.

---

## 4. Repository Structure

Follow the existing repository structure where one already exists. Do not reorganize the repository without a clear technical reason and explicit approval.

The intended organization is:

```text
codecore/
│
├── AGENTS.md
├── README.md
├── LICENSE
├── CMakeLists.txt
│
├── core/
│   ├── engine/
│   ├── document/
│   ├── text_buffer/
│   ├── cursor/
│   ├── selection/
│   ├── undo_redo/
│   ├── parsing/
│   ├── completion/
│   ├── diagnostics/
│   ├── formatting/
│   ├── navigation/
│   └── configuration/
│
├── native/
│   ├── include/
│   ├── src/
│   ├── ffi/
│   └── jni/
│
├── sdk/
│   ├── flutter/
│   └── kotlin/
│
├── languages/
│   ├── python/
│   ├── kotlin/
│   ├── java/
│   ├── javascript/
│   ├── typescript/
│   ├── cpp/
│   └── rust/
│
├── intelligence/
│   ├── lsp_client/
│   ├── symbol_index/
│   └── completion_providers/
│
├── ai/
│   ├── provider_api/
│   ├── ghost_text/
│   └── context_builder/
│
├── packages/
│   ├── manager/
│   ├── downloader/
│   ├── verifier/
│   └── resolver/
│
├── execution/
│   ├── api/
│   ├── process_manager/
│   ├── runtimes/
│   ├── terminal/
│   ├── build_system/
│   └── debugging/
│
├── examples/
│   ├── flutter_example/
│   └── kotlin_example/
│
├── tests/
│   ├── core/
│   ├── integration/
│   ├── performance/
│   ├── security/
│   └── compatibility/
│
└── docs/
    ├── architecture/
    ├── flutter/
    ├── kotlin/
    ├── api/
    └── language_support/
```

This is a target structure, not permission to create every directory immediately.

Create directories when their components are actually being implemented.

---

## 5. Coding Standards

### General

- Write production-quality, maintainable code.
- Prefer simple and explicit implementations over unnecessary abstractions.
- Use descriptive names.
- Keep functions focused.
- Avoid giant classes and files.
- Avoid duplicated business logic.
- Avoid unnecessary dependencies.
- Handle errors explicitly.
- Document non-obvious architectural decisions.
- Do not introduce placeholder implementations that appear functional.
- Do not silently swallow exceptions or errors.

### C++

- Use modern C++ supported by the project's configured toolchain.
- Prefer RAII and safe ownership.
- Avoid unnecessary raw-pointer ownership.
- Use clear header/source separation.
- Keep public headers stable and minimal.
- Expose C-compatible functions at the native boundary.
- Define memory ownership and allocation/freeing rules for every exported API.
- Never pass language-specific objects across the C ABI.

### Kotlin

- Use idiomatic Kotlin.
- Avoid unnecessary Java-style boilerplate.
- Keep JNI implementation details out of public SDK APIs.
- Handle Android lifecycle and threading correctly.
- Avoid blocking the main thread.

### Dart / Flutter

- Use idiomatic Dart.
- Keep FFI bindings isolated.
- Manage native memory and handles carefully.
- Do not block the Flutter UI isolate with expensive parsing or indexing.
- Dispose native resources correctly.
- Follow Flutter widget lifecycle conventions.

### API design

- Maintain consistent names across SDKs.
- Use stable language identifiers.
- Use consistent configuration names.
- Keep document, diagnostic, completion, and execution data models equivalent.
- Version public APIs deliberately.
- Avoid breaking changes without a migration strategy.

---

## 6. Editor Requirements

The editor must eventually support:

### Text editing

- Insert and delete text.
- Multiline editing.
- Cursor movement.
- Selection.
- Multiple cursors.
- Undo and redo.
- Copy, cut, and paste.
- Smart indentation.
- Automatic indentation.
- Configurable tabs and spaces.
- UTF-8 and Unicode handling.
- LF and CRLF line endings.
- Read-only mode.
- Efficient large-file editing.

Do not rebuild the entire document string after every keystroke.

Use an appropriate efficient text-buffer architecture, such as a piece table or rope, after evaluating the implementation requirements.

### Visual features

- Line numbers.
- Syntax highlighting.
- Current-line highlighting.
- Bracket matching.
- Automatic bracket completion.
- Indentation guides.
- Code folding.
- Search highlighting.
- Diagnostic underlines.
- Configurable themes.
- Font customization.
- Font size and line height.
- Word wrapping.
- Optional minimap.

### Navigation

- Find.
- Replace.
- Replace all.
- Regex search.
- Go to line.
- Go to symbol.
- Document outline.
- Bracket navigation.
- Cursor history.
- Workspace search.

### Completion

Support multiple independent completion sources:

1. Keywords.
2. Snippets.
3. Document symbols.
4. Imported symbols.
5. LSP results.
6. AI suggestions.

Completion sources must be independently configurable.

---

## 7. AI Ghost Text

Ghost text is an important feature.

Implement it independently from the normal completion popup.

Required behavior:

- Display suggestions as faded inline text.
- Tab accepts a suggestion.
- Escape dismisses it.
- Support partial acceptance where feasible.
- Cancel stale requests.
- Debounce requests.
- Prevent outdated responses from replacing newer suggestions.
- Do not insert suggestions into the document until accepted.
- Respect current selection and cursor position.
- Allow the host application to control appearance.

The AI provider must not receive source code without the host application's configured privacy policy permitting it.

Never embed private API keys in the SDK or native library.

---

## 8. Language Support

Potential language support includes:

- Python
- Kotlin
- Java
- JavaScript
- TypeScript
- C
- C++
- C#
- Go
- Rust
- Dart
- PHP
- Ruby
- Swift
- HTML
- CSS
- SCSS
- SQL
- JSON
- XML
- YAML
- Bash
- Lua

Do not claim that every language has identical functionality.

Maintain separate capability levels:

**Level 1 — Syntax**

- Highlighting.
- Bracket matching.
- Basic indentation.

**Level 2 — Local intelligence**

- Keywords.
- Snippets.
- Document symbols.
- Basic completion.

**Level 3 — Semantic intelligence**

- LSP.
- Diagnostics.
- Definition and references.
- Rename.
- Signature help.

**Level 4 — Execution**

- Local interpreter or compiler.
- Runtime management.
- Optional debugging.

A language must only be advertised at the level that is actually implemented and tested.

---

## 9. Package Management

Language resources should be modular.

The package manager should eventually support:

- Package manifests.
- Version compatibility.
- Dependency resolution.
- Download progress.
- Pause and resume.
- Retry.
- Integrity verification.
- Atomic installation.
- Rollback.
- Removal.
- Offline availability.
- Storage management.

Each manifest should contain appropriate metadata:

- Package ID.
- Version.
- Minimum engine version.
- Platform and architecture compatibility.
- Package type.
- Download size.
- Installed size.
- Dependencies.
- SHA-256 hash.
- Signature.
- License.

### Security requirements

- Verify downloaded package integrity.
- Authenticate trusted package publishers.
- Prevent archive path traversal.
- Reject malformed manifests.
- Avoid executing downloaded content automatically.
- Install packages atomically.
- Preserve rollback capability.
- Validate package compatibility.

**Important Android restriction:** Do not assume arbitrary downloaded executable code, native libraries, DEX files, or JAR files can be dynamically installed and executed in a Play-distributed application. Review current Google Play policies before designing executable package distribution.

Keep downloadable non-executable resources separate from executable runtimes and toolchains.

---

## 10. Execution Architecture

Execution must remain separate from the editor engine.

### Phase A

Expose a host-controlled execution interface.

Conceptually:

```text
CodeRunner
    run(language, source, stdin)
    cancel(executionId)
```

Return structured results:

```text
ExecutionResult
    stdout
    stderr
    exitCode
    executionTime
    status
```

The host application decides how execution happens.

### Phase B

Introduce independent runtime providers.

Potential responsibilities:

- Runtime discovery.
- Installation and removal.
- Version management.
- Process creation.
- Standard input.
- Standard output.
- Standard error.
- Cancellation.
- Timeouts.
- Environment variables.
- Working directories.
- Resource limits.

Never assume that running a single script is equivalent to compiling a complete project.

For example, C++ and Rust toolchains require more than a compiler executable: they may need linkers, headers, libraries, and build tools.

### Security

Never execute untrusted code with unrestricted host privileges.

An embedded interpreter is not automatically a security sandbox.

Evaluate filesystem isolation, network restrictions, process restrictions, memory limits, CPU limits, and execution timeouts.

---

## 11. Performance Requirements

Treat these as initial engineering targets, not guaranteed results:

| Metric | Initial target |
|---|---:|
| Initial editor display | Under 300 ms |
| Typical basic keystroke handling | Under 16 ms |
| Local completion | Under 100 ms |
| Typical syntax update | Under 50 ms |
| Initial integration overhead | Aim for under 5 MB |
| Offline syntax support | After required resources are installed |

Measure actual results.

Do not claim a performance target has been achieved without testing.

Test with:

- Small files.
- 100 KB files.
- 1 MB files.
- 10 MB files.
- Large line counts.
- Long lines.
- Unicode content.
- Rapid typing.
- Multiple open documents.
- Low-memory Android devices.

Measure download size, installed size, startup time, runtime memory, CPU usage, and battery consumption separately.

---

## 12. Testing Requirements

Every substantial feature must have tests.

### Core tests

- Text insertion and deletion.
- Cursor movement.
- Selection boundaries.
- Undo and redo.
- Multiple edits.
- Unicode.
- Line endings.
- Large documents.
- Buffer consistency.

### Parsing tests

- Incomplete code.
- Invalid syntax.
- Rapid document changes.
- Incremental updates.
- Multiple languages.

### Integration tests

- C++ API.
- Flutter FFI.
- Kotlin JNI.
- Resource lifecycle.
- Thread safety.
- Error propagation.

### Package tests

- Valid downloads.
- Corrupted downloads.
- Invalid signatures.
- Version conflicts.
- Interrupted downloads.
- Rollback.
- Offline operation.

### Performance tests

Use benchmarks rather than assumptions.

Do not remove tests merely to make a build pass.

---

## 13. Agentic Development Rules

These instructions apply to every AI coding agent working in this repository.

### Before making changes

1. Inspect the existing repository.
2. Read this AGENTS.md file.
3. Read relevant source files.
4. Understand existing architecture and conventions.
5. Identify affected modules.
6. Check existing tests and build commands.
7. Explain the proposed implementation before making substantial changes.

Do not assume that the repository is empty or that a proposed directory structure has already been implemented.

### During implementation

- Make focused changes.
- Preserve existing working functionality.
- Avoid unrelated refactoring.
- Do not replace entire files unnecessarily.
- Do not introduce dependencies without justification.
- Do not create duplicate implementations.
- Follow established interfaces.
- Keep platform-specific logic in the appropriate platform layer.
- Ask before making major architectural changes.
- Do not modify unrelated configuration files.

### Build and verification

After implementation:

1. Run relevant tests.
2. Run the relevant build.
3. Check compiler warnings and errors.
4. Verify affected integrations.
5. Report what was actually tested.
6. Identify anything that remains unverified.

Never claim that code works merely because it was written.

Never claim tests passed if they were not executed.

If a build cannot be run, explain why.

### Git rules

- Do not commit automatically unless explicitly requested.
- Do not push changes without permission.
- Never overwrite user changes.
- Never perform destructive Git operations without explicit approval.
- Do not rewrite repository history without permission.

### Architectural decisions

Do not independently:

- Replace C++ with another core language.
- Replace the C ABI with a C++ ABI.
- Merge the editor and runtime systems.
- Replace Tree-sitter without evaluating alternatives.
- Change public API contracts casually.
- Add a large framework to solve a small problem.
- Introduce a mandatory cloud dependency.
- Make a proprietary AI provider mandatory.

Explain the technical justification and request approval before such changes.

### Honesty

The agent must distinguish between:

- Implemented.
- Partially implemented.
- Planned.
- Mocked.
- Tested.
- Untested.
- Known limitations.

Never represent a mock as a working feature.

Never fabricate benchmark results, compatibility, or language support.

---

## 14. Implementation Priority

Follow this order unless the repository's actual state requires a justified adjustment.

### Foundation

1. Repository and build system.
2. C++ core.
3. Stable C API.
4. Native unit tests.
5. Minimal Flutter FFI integration.
6. Minimal Kotlin JNI integration.

### Editor engine

7. Document model.
8. Efficient text buffer.
9. Cursor and selection.
10. Undo and redo.
11. Basic editing operations.
12. Editor configuration.

### Language support

13. Tree-sitter integration.
14. Incremental parsing.
15. Syntax highlighting.
16. Bracket matching.
17. Code folding.
18. Keyword completion.
19. Snippets.
20. Document symbols.

### Intelligence

21. LSP client.
22. Completion integration.
23. Diagnostics.
24. Navigation.
25. Formatting and code actions.

### AI

26. AI provider interface.
27. Context builder.
28. Ghost-text rendering.
29. Acceptance and cancellation behavior.

### Distribution

30. Package manifests.
31. Package manager.
32. Download and verification.
33. Offline package management.

### SDK and release

34. Flutter SDK.
35. Kotlin SDK.
36. Example applications.
37. Integration tests.
38. Documentation.
39. Phase A release.

Only then begin Phase B.

---

## 15. Definition of Done

A feature is complete only when:

- Its intended behavior is implemented.
- It integrates with the existing architecture.
- Errors are handled.
- Relevant tests pass.
- Resource ownership is correct.
- Platform compatibility is verified where applicable.
- Documentation is updated.
- Limitations are disclosed.

A feature is not complete merely because a UI button or API method exists.

---

## 16. Final Direction

CodeCore should become a reusable editor and IDE framework, not a single-purpose code editor application.

Prioritize:

1. A reliable shared engine.
2. A stable C API.
3. Lightweight platform integrations.
4. A modular language system.
5. Excellent editing performance.
6. Offline-first functionality.
7. A clean separation between editing and execution.
8. Extensibility without architectural rewrites.

**Build a small, reliable foundation first. Expand only when the foundation is proven.**