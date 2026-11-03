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

    static final class Listener {
        int count;
        long version;
        int kind;
        int origin;

        public void onChange(long version, int kind, int start, int removed, int inserted, int origin) {
            this.count += 1;
            this.version = version;
            this.kind = kind;
            this.origin = origin;
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
        check("0.4.0".equals(NativeCodeCore.nativeVersion()), "native version");

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
        long[] matches = NativeCodeCore.nativeFind(document, "return", false, false, false);
        check(matches.length == 3 && matches[0] == 0, "find result pair");
        long[] regexMatches = NativeCodeCore.nativeFind(document, "\\breturn\\b", false, false, true);
        check(regexMatches.length == 3 && regexMatches[0] == 0, "regex find result pair");
        long[] badRegex = NativeCodeCore.nativeFind(document, "(", false, false, true);
        check(badRegex.length == 1 && badRegex[0] == 1, "invalid regex reports status");

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

        long agent = NativeCodeCore.nativeCreate("python", 4, true);
        check(NativeCodeCore.nativeSetText(agent, "one\ntwo\n") == 0, "agent set text");
        check(NativeCodeCore.nativeEditOrigin(agent) == 2, "default host origin");
        check(NativeCodeCore.nativeSetEditOrigin(agent, 0) == 0, "set user origin");
        check(NativeCodeCore.nativeEditOrigin(agent) == 0, "read user origin");

        check(NativeCodeCore.nativeSetProposal(agent, "one\nchanged\nthree\n", 1) == 0,
                "set proposal");
        check(NativeCodeCore.nativeHasProposal(agent), "has proposal");
        check("one\ntwo\n".equals(NativeCodeCore.nativeGetText(agent)),
                "proposal does not mutate text");
        String[] diff = NativeCodeCore.nativeProposalDiff(agent);
        check(diff.length > 0, "proposal diff");
        check(NativeCodeCore.nativeAcceptProposal(agent) == 0, "accept proposal");
        check("one\nchanged\nthree\n".equals(NativeCodeCore.nativeGetText(agent)),
                "accepted proposal text");
        check(!NativeCodeCore.nativeHasProposal(agent), "proposal cleared");
        check(NativeCodeCore.nativeUndo(agent) == 0, "proposal undo");
        check("one\ntwo\n".equals(NativeCodeCore.nativeGetText(agent)), "proposal undo text");

        check(NativeCodeCore.nativeSetGhostText(agent, 8, "return 1", 1) == 0,
                "set ghost text");
        String ghost = NativeCodeCore.nativeGhostText(agent);
        check(ghost != null && ghost.endsWith("return 1"), "ghost text payload");
        check("one\ntwo\n".equals(NativeCodeCore.nativeGetText(agent)),
                "ghost text not inserted");
        check(NativeCodeCore.nativeAcceptGhostText(agent, 6) == 0, "accept partial ghost");
        check(NativeCodeCore.nativeGetText(agent).endsWith("return"), "partial ghost text");
        check(NativeCodeCore.nativeUndo(agent) == 0, "ghost undo");
        check(NativeCodeCore.nativeSetGhostText(agent, 0, "x", 1) == 0, "set ghost again");
        check(NativeCodeCore.nativeDismissGhostText(agent) == 0, "dismiss ghost");
        check(NativeCodeCore.nativeGhostText(agent) == null, "ghost cleared");

        long snapshot = NativeCodeCore.nativeCreateSnapshot(agent);
        check(snapshot != 0L, "snapshot create");
        check(NativeCodeCore.nativeSnapshotLength(snapshot) == 8, "snapshot length");
        check(NativeCodeCore.nativeSetText(agent, "replacement\n") == 0, "replace agent text");
        check(NativeCodeCore.nativeRestoreSnapshot(agent, snapshot) == 0, "restore snapshot");
        check("one\ntwo\n".equals(NativeCodeCore.nativeGetText(agent)), "restored text");
        check(NativeCodeCore.nativeUndo(agent) == 0, "snapshot restore undo");
        check("replacement\n".equals(NativeCodeCore.nativeGetText(agent)), "restore undo text");
        NativeCodeCore.nativeSnapshotDestroy(snapshot);

        check(NativeCodeCore.nativeSetEditable(agent, false) == 0, "set not editable");
        check(NativeCodeCore.nativeIsReadOnly(agent), "editable maps to read-only");
        check(NativeCodeCore.nativeSetEditable(agent, true) == 0, "set editable");

        NativeCodeCore.nativeSetText(agent, "abc\ndef");
        check(NativeCodeCore.nativeSetSelection(agent, 1, 2) == 0, "set selection");
        long selection = NativeCodeCore.nativeGetSelection(agent);
        check((selection >>> 32) == 1L && (selection & 0xFFFFFFFFL) == 2L,
                "selection round-trip");
        check(NativeCodeCore.nativeMoveCursor(agent, 0, false) == 0, "cursor left");
        long collapsed = NativeCodeCore.nativeGetSelection(agent);
        check((collapsed >>> 32) == 1L && (collapsed & 0xFFFFFFFFL) == 1L,
                "cursor collapse");
        check(NativeCodeCore.nativeSetLanguage(agent, 1) == 0, "switch language");
        check(NativeCodeCore.nativeLanguageId(agent) == 1, "language switched");
        check(NativeCodeCore.nativeSetLanguage(agent, 0) == 0, "switch language back");

        NativeCodeCore.nativeSetText(agent, "one\ntwo\nthree\n");
        check(NativeCodeCore.nativeSetSelection(agent, 0, 0) == 0, "multi primary cursor");
        check(NativeCodeCore.nativeAddCursor(agent, 4, 4) == 0, "add cursor");
        check(NativeCodeCore.nativeCursorCount(agent) == 2, "cursor count");
        long second = NativeCodeCore.nativeCursorAt(agent, 1);
        check((second >>> 32) == 4L && (second & 0xFFFFFFFFL) == 4L, "cursor at");
        check(NativeCodeCore.nativeMultiInsert(agent, "# ") == 0, "multi insert");
        check("# one\n# two\nthree\n".equals(NativeCodeCore.nativeGetText(agent)),
                "multi insert text");
        long primary = NativeCodeCore.nativeGetSelection(agent);
        check((primary & 0xFFFFFFFFL) == 2L, "multi primary caret");
        second = NativeCodeCore.nativeCursorAt(agent, 1);
        check((second & 0xFFFFFFFFL) == 8L, "multi secondary caret");
        check(NativeCodeCore.nativeUndo(agent) == 0, "multi insert undo");
        check("one\ntwo\nthree\n".equals(NativeCodeCore.nativeGetText(agent)),
                "multi insert undo text");
        check(NativeCodeCore.nativeRedo(agent) == 0, "multi insert redo");

        NativeCodeCore.nativeSetText(agent, "one\ntwo\nthree\n");
        check(NativeCodeCore.nativeSetSelection(agent, 0, 3) == 0, "multi erase primary");
        check(NativeCodeCore.nativeAddCursor(agent, 4, 7) == 0, "multi erase secondary");
        check(NativeCodeCore.nativeMultiErase(agent, true) == 0, "multi erase");
        check("\n\nthree\n".equals(NativeCodeCore.nativeGetText(agent)), "multi erase text");
        check(NativeCodeCore.nativeUndo(agent) == 0, "multi erase undo");
        check("one\ntwo\nthree\n".equals(NativeCodeCore.nativeGetText(agent)),
                "multi erase undo text");
        NativeCodeCore.nativeClearCursors(agent);
        check(NativeCodeCore.nativeCursorCount(agent) == 1, "clear cursors");

        Listener listener = new Listener();
        NativeCodeCore.nativeSetChangeListener(agent, listener);
        check(NativeCodeCore.nativeInsert(agent, 0, "#") == 0, "listener insert");
        check(listener.count == 1, "change listener fired");
        check(listener.kind == 1, "change listener kind");
        check(listener.origin == 0, "change listener origin");
        NativeCodeCore.nativeSetChangeListener(agent, null);
        check(NativeCodeCore.nativeInsert(agent, 0, "#") == 0, "listener detached insert");
        check(listener.count == 1, "change listener detached");

        NativeCodeCore.nativeDestroy(agent);

        long lsp = NativeCodeCore.nativeLspCreate();
        check(lsp != 0L, "lsp create");
        NativeCodeCore.nativeLspInitialize(lsp, "file:///project");
        String initializeFrame = NativeCodeCore.nativeLspTakeOutgoing(lsp);
        check(initializeFrame != null && initializeFrame.contains("initialize"),
                "lsp initialize frame");
        String capabilitiesBody = "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":"
                + "{\"capabilities\":{\"hoverProvider\":true}}}";
        NativeCodeCore.nativeLspFeed(lsp,
                "Content-Length: " + capabilitiesBody.length() + "\r\n\r\n" + capabilitiesBody);
        check(NativeCodeCore.nativeLspIsInitialized(lsp), "lsp initialized");
        check(NativeCodeCore.nativeLspServerCapabilities(lsp).contains("hoverProvider"),
                "lsp server capabilities");
        check(NativeCodeCore.nativeLspDidOpen(
                lsp, "file:///project/main.py", "python", 1, "print(1)\n") == 0,
                "lsp did open");
        String didOpenFrame = null;
        for (int attempt = 0; attempt < 4; attempt += 1) {
            didOpenFrame = NativeCodeCore.nativeLspTakeOutgoing(lsp);
            if (didOpenFrame != null && didOpenFrame.contains("textDocument/didOpen")) {
                break;
            }
        }
        check(didOpenFrame != null && didOpenFrame.contains("textDocument/didOpen"),
                "lsp did open frame");
        NativeCodeCore.nativeLspDestroy(lsp);

        NativeCodeCore.nativeDestroy(document);
        check(true, "destroy does not crash");

        System.out.println("JNI smoke test: " + checks + " checks, " + failures + " failures");
        if (failures > 0) {
            System.exit(1);
        }
    }
}
