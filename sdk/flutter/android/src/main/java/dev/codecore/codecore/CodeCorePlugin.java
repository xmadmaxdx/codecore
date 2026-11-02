package dev.codecore.codecore;

import io.flutter.embedding.engine.plugins.FlutterPlugin;

/**
 * Registration shim for the CodeCore Flutter plugin.
 *
 * CodeCore is used through Dart FFI, not a method channel. The plugin exists so
 * Gradle builds and packages libcodecore.so, and the Dart loader can open it by
 * name on Android.
 */
public final class CodeCorePlugin implements FlutterPlugin {
    @Override
    public void onAttachedToEngine(FlutterPluginBinding binding) {
        // Native library is loaded by the Dart FFI loader.
    }

    @Override
    public void onDetachedFromEngine(FlutterPluginBinding binding) {
        // No engine-level resources are held by the plugin.
    }
}
