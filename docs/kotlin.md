# Kotlin / Android SDK

Module: `sdk/kotlin`  
JNI bridge: `native/jni/codecore_jni.cpp`  
Native build: `native/android/CMakeLists.txt`

Structural syntax (Tree-sitter highlights and syntax errors) is exposed through
`CodeCoreDocument.syntaxTree()` for languages with a bundled grammar.

## Design

```text
CodeCore (object)
    └── CodeCoreDocument / CodeCoreSnapshot
            └── NativeCodeCore (JNI external methods)
                    └── libcodecore_jni.so
                            └── shared C++ core
```

The JNI bridge converts standard UTF-8 to UTF-16 explicitly. JNI's
`NewStringUTF`/`GetStringUTFChars` use modified UTF-8, which would corrupt
supplementary characters (emoji, rare CJK), so they are not used for document
text.

All offsets are UTF-8 bytes, matching the C ABI and Dart API. Kotlin callers must
keep byte offsets; `String.length` is UTF-16 code units.

## Build

The module includes the Gradle wrapper (Gradle 9.3.1) and pins Android NDK
`28.2.13676358`.

```bash
cd sdk/kotlin
# local.properties: sdk.dir=D:/Android
./gradlew.bat :assembleDebug        # Windows
./gradlew :assembleDebug            # macOS / Linux
```

If your NDK is elsewhere, install 28.2.13676358 or update `ndkVersion` in
`build.gradle.kts`.

Verified: `gradlew.bat :assembleDebug` completes successfully, compiling the
Kotlin SDK and the Android/JNI native sources and producing a debug AAR. The
module targets JVM 17 bytecode with the installed JDK 21 and pins NDK
28.2.13676358.

The JNI bridge can be built and tested without Kotlin:

```bash
cmake -S . -B build/jni -DCODECORE_BUILD_JNI=ON -DCODECORE_BUILD_TESTS=OFF
cmake --build build/jni --config Release

javac -d build/jni/classes \
  tests/jni/dev/codecore/nativebridge/NativeCodeCore.java \
  tests/jni/JniSmokeTest.java
java -Djava.library.path=build/jni -cp build/jni/classes JniSmokeTest
```

Result: **175 checks, 0 failures**. The Java harness uses the same JNI symbols
the Kotlin object declares, including structural trees and errors for every
bundled grammar.

## Usage

```kotlin
import dev.codecore.CodeCore

val document = CodeCore.createDocument(
    language = "python",
    tabSize = 4,
    insertSpaces = true,
)

document.text = """
    def total(a, b):
        return a + b
""".trimIndent()

document.insert(document.length.toInt(), "\nprint(total(2, 3))\n")

document.editOrigin = EditOrigin.USER
document.setSelection(0, 4)
document.moveCursor(CursorMove.WORD_RIGHT, extendSelection = true)

document.setChangeListener { event ->
    println("${event.kind} origin=${event.origin} v${event.version}")
}

document.tokenize().forEach { token ->
    println("${token.start}..${token.end} ${token.kind}")
}

document.close()
```

## Structural syntax

Tree-sitter is bundled for `python`, `javascript` (including JSX and the
`js`/`jsx`/`mjs`/`cjs` aliases), `java`, `go`, `json`, `c`, `cpp` and `rust`.
Aliases such as `rs`, `golang` and `c++` resolve to the canonical grammar id.
`syntaxTree()` returns `null` for a language without a grammar; lexical tokens
still work for every language.

```kotlin
if (CodeCore.supportsStructuralSyntax("python")) {
    val tree = document.syntaxTree()
    if (tree != null) {
        println(tree.rootType)   // module
        println(tree.nodeCount)
        println(tree.hasErrors)  // ERROR / MISSING nodes present

        tree.highlights().forEach { highlight ->
            println("${highlight.start}..${highlight.end} ${highlight.capture}")
        }
        tree.errors().forEach { error ->
            println("${error.nodeType} at ${error.startLine}:${error.startColumn}")
        }
        tree.close()
    }
}
```

- `start`/`end` are UTF-8 byte offsets. Columns are bytes within the row,
  matching Tree-sitter, not Unicode code points.
- A tree snapshots one document version and owns native memory. Close it when
  done.
- Repeated `syntaxTree()` calls on an unchanged version reuse the native tree;
  edits reparse incrementally from the previous tree.
- Highlights are structural, not semantic. There is no name or type resolution.

## Execution

`startExecution` runs a child process. CodeCore ships no runtime; pass the
command the host wants to run. Poll output and close the session when done.

```kotlin
val run = CodeCore.startExecution(
    command = "python3",
    arguments = listOf("-c", "print('hello')"),
    timeoutMs = 10_000,
)

while (true) {
    val event = run.poll()
    if (event == null) {
        Thread.sleep(5)
        continue
    }
    if (event.kind == ExecutionEventKind.OUTPUT) {
        print(event.data)
    } else {
        println("exit ${event.exitCode} cancelled=${event.cancelled}")
        break
    }
}
run.close()
```

`write` sends stdin, `closeStdin` sends EOF, `cancel` terminates the process
tree, `waitFor` blocks with a timeout, and `exitCode` is -1 until exit. Output
is bounded (1 MiB by default); keep polling to avoid backpressure. The exit
event is delivered after all queued output. On Android, app-data executables
are blocked by platform policy; package runtimes as native libraries.

## Agent-facing API

```kotlin
// Proposed changes
document.setProposal("one\nchanged\nthree\n", EditOrigin.AI)
document.proposalDiff().forEach { line ->
    println("${line.kind} old=${line.oldLine} new=${line.newLine} ${line.text}")
}
document.acceptProposal()   // one undoable edit
document.rejectProposal()

// Ghost text
document.showGhostText(document.length.toInt(), "return 1", EditOrigin.AI)
document.acceptGhostText(maxCodePoints = 6)
document.dismissGhostText()

// Snapshots
val snapshot = document.createSnapshot()
document.text = "temporary\n"
document.restoreSnapshot(snapshot)
snapshot.close()

// Permissions and language
document.editable = false
document.languageId = 1
```

`DiffLine.oldLine`/`newLine` are `-1` on the missing side. Ghost text is never
inserted before acceptance. A snapshot remains valid after its document is
closed.

## API surface

- `CodeCore.version`, `CodeCore.createDocument`, `CodeCore.createLsp`,
  `CodeCore.startExecution`
- `CodeCoreExecution`: `poll`, `write`, `closeStdin`, `cancel`, `waitFor`,
  `isRunning`, `exitCode`, `close`,
  `CodeCore.supportsStructuralSyntax`
- `CodeCoreLsp`: `initialize`, `feed`, `takeOutgoing`, `takeResult`,
  `takeDiagnostics`, `isInitialized`, `serverCapabilities`, `lastError`,
  `didOpen`, `didChange`, `didClose`, `didSave`, `request`, `notify`, `cancel`,
  `shutdown`, `exit`, `close`
- `CodeCoreDocument`: `text`, `length`, `version`, `lineCount`, `languageId`
  (read/write), `readOnly`, `editable`, `editOrigin`, `selection`, `canUndo`,
  `canRedo`
- `insert`, `erase`, `replace`, `undo`, `redo`, `clearHistory`
- `setSelection`, `moveCursor`, `setChangeListener`
- `cursorCount`, `cursorAt`, `addCursor`, `clearCursors`, `moveCursors`
- `insertAtCursors`, `eraseAtCursors` (one undoable transaction each)
- `tokenize`, `complete`, `diagnose`, `find(query, caseSensitive, wholeWord,
  regex)`, `foldingRanges`, `matchBracket`
- `syntaxTree`, `SyntaxTree`: `language`, `rootType`, `nodeCount`, `hasErrors`,
  `highlights`, `errors`, `close`
- Invalid regex patterns throw `CodeCoreException` (status 1) instead of
  returning an empty list
- `setProposal`, `proposal`, `proposalDiff`, `acceptProposal`, `rejectProposal`,
  `hasProposal`
- `showGhostText`, `ghostText`, `acceptGhostText`, `dismissGhostText`
- `createSnapshot`, `restoreSnapshot`
- `CodeCoreSnapshot`: `version`, `length`, `languageId`, `text`, `close`
- `close`

## LSP

`CodeCore.createLsp()` returns a `CodeCoreLsp`. The host owns the language
server's transport; the client handles framing, request ids, lifecycle, sync
and diagnostics.

```kotlin
val lsp = CodeCore.createLsp()
lsp.initialize("file:///project")

while (true) {
    val frame = lsp.takeOutgoing() ?: break
    transport.write(frame)
}

lsp.feed(transport.read())
lsp.didOpen("file:///project/main.py", "python", 1, document.text)
val requestId = lsp.completion("file:///project/main.py", 2, 4)

while (true) {
    val result = lsp.takeResult() ?: break
    println("${result.method}: ${result.payload} error=${result.isError}")
}
while (true) {
    val report = lsp.takeDiagnostics() ?: break
    println("${report.uri}: ${report.payload}")
}

lsp.close()
```

Convenience methods cover `completion`, `hover`, `definition`, `signatureHelp`,
`references`, `documentSymbols`, `workspaceSymbols`, `rename`, `formatting`,
`codeAction` and `semanticTokens`. Any other method goes through `request(method,
paramsJson)` or `notify(method, paramsJson)`. `cancel`, `shutdown` and `exit`
are provided. Results are raw JSON and `LspResult.isError` marks error replies.

- Positions are LSP UTF-16 coordinates, not CodeCore byte offsets.
- Sync is full-text; no language server is bundled.
- The JNI bridge is built into `libcodecore_jni.so` with the LSP core.

## Limitations

- No Compose or Android View widgets yet.
- Offsets are `Int` in the JNI bridge; the configured document limit is far below
  the 2 GB boundary.
- Change callbacks run on the mutating thread while the native document is
  locked. Do not block or close the document inside the callback.
- Regex search uses native ECMAScript semantics and is capped at 10,000
  matches; pathological patterns are not sandboxed.
- No editor rendering, theme integration, or lifecycle-aware controller.
- Structural syntax is compiled in for Python, JavaScript, Java, Go, JSON, C,
  C++ and Rust. The other 32 languages use the lexical tokenizer and return
  `null` from `syntaxTree()`.
- No semantic intelligence or runtime management. Execution runs only what the
  host supplies; no interpreter is bundled.
