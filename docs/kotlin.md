# Kotlin / Android SDK

Module: `sdk/kotlin`  
JNI bridge: `native/jni/codecore_jni.cpp`  
Native build: `native/android/CMakeLists.txt`

## Design

The Kotlin SDK is a thin, typed layer over the C ABI:

```text
CodeCore (object)
    └── CodeCoreDocument
            └── NativeCodeCore (JNI external methods)
                    └── libcodecore_jni.so
                            └── shared C++ core
```

The JNI bridge converts standard UTF-8 to UTF-16 explicitly. JNI's
`NewStringUTF`/`GetStringUTFChars` use modified UTF-8, which would corrupt
supplementary characters (emoji, rare CJK), so they are not used for document
text.

All offsets are UTF-8 bytes, matching the C ABI and Dart API. Kotlin callers must
keep byte offsets; `String.length` is UTF-16 code units and is not equivalent
for non-ASCII documents.

## Build

The module uses Android Gradle Plugin 8.7.3, Kotlin 2.1.0, compile SDK 35, min SDK
24, and the NDK through CMake.

```bash
cd sdk/kotlin
gradle :assembleDebug        # requires Android SDK + NDK + Gradle
```

Gradle then builds `libcodecore_jni.so` from `native/android/CMakeLists.txt`. No
prebuilt binary is checked in.

The JNI bridge can be built standalone on a desktop with a JDK:

```bash
cmake -S . -B build/native-jni -DCODECORE_BUILD_JNI=ON -DCODECORE_BUILD_TESTS=OFF
cmake --build build/native-jni
```

On Windows this produces `build/native-jni/codecore_jni.dll`.

Because no Kotlin compiler or Gradle installation was available on the
development machine, the Kotlin module itself has **not** been compiled. The JNI
ABI it calls is compiled and exercised by `tests/jni/JniSmokeTest.java`:

```bash
mkdir -p build/jni-test
javac -d build/jni-test \
  tests/jni/dev/codecore/nativebridge/NativeCodeCore.java \
  tests/jni/JniSmokeTest.java
java -Djava.library.path=build/native-jni -cp build/jni-test JniSmokeTest
```

Result: 27 checks, 0 failures.

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

document.tokenize().forEach { token ->
    println("${token.start}..${token.end} ${token.kind}")
}

document.complete().take(10).forEach { item ->
    println("${item.label} (${item.kind})")
}

document.diagnose().forEach { diagnostic ->
    println("${diagnostic.severity}: ${diagnostic.message}")
}

document.close()
```

`CodeCoreDocument` implements `AutoCloseable`; always close documents. Calls are
synchronized on the instance.

## API surface

- `CodeCore.version`, `CodeCore.createDocument`
- `CodeCoreDocument.text`, `length`, `version`, `lineCount`, `languageId`,
  `readOnly`
- `insert`, `erase`, `replace`, `undo`, `redo`, `clearHistory`
- `tokenize`, `complete`, `diagnose`, `find`, `foldingRanges`, `matchBracket`
- `close`

Not exposed yet: selection/cursor movement, change listeners, language switching,
execution. Those exist in the C ABI and Dart SDK; adding them to the Kotlin
wrapper is mechanical and belongs with the Android UI milestone.

## Limitations

- Kotlin module uncompiled (see above).
- No Compose or Android View widgets yet.
- Offsets are `Int` in the JNI bridge; the configured document limit is far below
  the 2 GB boundary.
- No editor rendering, theme integration, or lifecycle-aware controller.
- No semantic intelligence or execution.

Kotlin UI integration is the next Android milestone, not part of this engine
release.
