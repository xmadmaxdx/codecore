import dev.codecore.nativebridge.NativeCodeCore;

/**
 * Executes the JNI bridge without Kotlin tooling.
 *
 * Compile and run from the repository root after building with
 * -DCODECORE_BUILD_JNI=ON:
 *
 *   javac -d build/jni-test tests/jni/dev/codecore/nativebridge/NativeCodeCore.java \
 *       tests/jni/JniSmokeTest.java
 *   java -Djava.library.path=build/native-jni -cp build/jni-test JniSmokeTest
 */
public final class JniSmokeTest {
    private static int checks = 0;
    private static int failures = 0;

    private static void check(boolean condition, String label) {
        checks += 1;
        if (!condition) {
            failures += 1;
            System.out.println("FAIL: " + label);
        }
    }

    private static boolean contains(int[] packed, int kind) {
        for (int index = 0; index + 2 < packed.length; index += 3) {
            if (packed[index + 2] == kind) {
                return true;
            }
        }
        return false;
    }

    public static void main(String[] args) {
        check("0.2.0".equals(NativeCodeCore.nativeVersion()), "native version");

        long document = NativeCodeCore.nativeCreate("python", 4, true);
        check(document != 0L, "document creation");
        check(NativeCodeCore.nativeLanguageId(document) == 0, "python language id");

        String source = "# demo\ndef calculate_total(price, quantity):\n"
                + "    return price * quantity\n\nprint(calculate_total(2, 3))\n";
        check(NativeCodeCore.nativeSetText(document, source) == 0, "set text");
        check(source.equals(NativeCodeCore.nativeGetText(document)), "get text");
        check(NativeCodeCore.nativeLength(document) == source.length(), "byte length");
        check(NativeCodeCore.nativeLineCount(document) == 6, "line count");

        int[] tokens = NativeCodeCore.nativeTokenize(document, 0, source.length());
        check(tokens.length > 0 && tokens.length % 3 == 0, "token triplets");
        check(contains(tokens, 1), "comment token");
        check(contains(tokens, 2), "keyword token");
        check(contains(tokens, 6), "number token");

        String[] completions = NativeCodeCore.nativeComplete(document, source.length());
        check(completions.length > 0, "completion results");
        boolean foundTotal = false;
        for (String line : completions) {
            if (line.startsWith("calculate_total\u001F")) {
                foundTotal = true;
                break;
            }
        }
        check(foundTotal, "symbol completion");

        check(NativeCodeCore.nativeDiagnose(document).length == 0, "clean diagnostics");
        int[] matches = NativeCodeCore.nativeFind(document, "return", false, false);
        check(matches.length == 2, "find result pair");

        check(NativeCodeCore.nativeInsert(document, 0, "X") == 0, "insert");
        check(NativeCodeCore.nativeCanUndo(document), "can undo");
        check(NativeCodeCore.nativeUndo(document) == 0, "undo");
        check(source.equals(NativeCodeCore.nativeGetText(document)), "undo restores text");

        check(NativeCodeCore.nativeSetReadOnly(document, true) == 0, "set read-only");
        check(NativeCodeCore.nativeInsert(document, 0, "Y") == 3, "read-only rejected");
        check(NativeCodeCore.nativeSetReadOnly(document, false) == 0, "clear read-only");

        long script = NativeCodeCore.nativeCreate("javascript", 4, true);
        String scriptSource = "function outer() {\n  if (ready) {\n    work();\n  }\n}\n";
        check(NativeCodeCore.nativeSetText(script, scriptSource) == 0, "set script");
        int[] folds = NativeCodeCore.nativeFoldingRanges(script);
        check(folds.length >= 6 && folds.length % 3 == 0, "folding ranges");
        check(NativeCodeCore.nativeMatchBracket(script, 17) == 50, "bracket match");
        NativeCodeCore.nativeDestroy(script);

        long broken = NativeCodeCore.nativeCreate("python", 4, true);
        NativeCodeCore.nativeSetText(broken, "def f(:\n    pass\n");
        String[] diagnostics = NativeCodeCore.nativeDiagnose(broken);
        check(diagnostics.length > 0, "broken code diagnostics");
        NativeCodeCore.nativeDestroy(broken);

        NativeCodeCore.nativeDestroy(document);
        check(true, "destroy does not crash");

        System.out.println("JNI smoke test: " + checks + " checks, " + failures + " failures");
        if (failures > 0) {
            System.exit(1);
        }
    }
}
