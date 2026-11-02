/// Low-level dart:ffi bindings for the CodeCore C ABI.
///
/// This file is intentionally not exported from the package barrel. Callers use
/// [CodeCore] and [CodeCoreDocument] from `codecore.dart`.
library;

import 'dart:convert';
import 'dart:ffi';

final class CcEngineConfig extends Struct {
  @Uint32()
  external int structSize;

  @Uint32()
  external int maxDocumentBytes;
}

final class CcDocumentOptions extends Struct {
  @Uint32()
  external int structSize;

  @Int32()
  external int languageId;

  @Uint32()
  external int tabSize;

  @Uint8()
  external int insertSpaces;

  @Uint8()
  external int readOnly;
}

final class CcLanguageInfo extends Struct {
  @Int32()
  external int id;

  external Pointer<Uint8> idString;

  external Pointer<Uint8> displayName;

  external Pointer<Uint8> extensions;

  @Uint32()
  external int capabilities;
}

final class CcToken extends Struct {
  @Uint32()
  external int start;

  @Uint32()
  external int end;

  @Uint32()
  external int kind;
}

final class CcLineInfo extends Struct {
  @Uint32()
  external int start;

  @Uint32()
  external int contentEnd;

  @Uint32()
  external int end;
}

final class CcPosition extends Struct {
  @Uint32()
  external int line;

  @Uint32()
  external int column;
}

final class CcSelection extends Struct {
  @Uint32()
  external int anchor;

  @Uint32()
  external int focus;
}

final class CcCompletionItem extends Struct {
  external Pointer<Uint8> label;

  external Pointer<Uint8> insertText;

  external Pointer<Uint8> detail;

  @Uint32()
  external int kind;

  @Int32()
  external int score;
}

final class CcDiagnostic extends Struct {
  @Uint32()
  external int start;

  @Uint32()
  external int end;

  @Uint32()
  external int severity;

  external Pointer<Uint8> message;

  external Pointer<Uint8> source;
}

final class CcFoldingRange extends Struct {
  @Uint32()
  external int startLine;

  @Uint32()
  external int endLine;

  @Uint32()
  external int kind;
}

final class CcRange extends Struct {
  @Uint32()
  external int start;

  @Uint32()
  external int end;
}

final class CcSearchOptions extends Struct {
  @Uint32()
  external int structSize;

  @Uint8()
  external int caseSensitive;

  @Uint8()
  external int wholeWord;
}

final class CcChangeEvent extends Struct {
  @Uint64()
  external int version;

  @Uint32()
  external int kind;

  @Uint32()
  external int start;

  @Uint32()
  external int removedLength;

  @Uint32()
  external int insertedLength;
}

class CodeCoreException implements Exception {
  const CodeCoreException(this.code, this.message);

  final int code;
  final String message;

  @override
  String toString() => 'CodeCoreException($code): $message';
}

typedef ChangeCallbackNative = Void Function(
    Pointer<Void>, Pointer<CcChangeEvent>);

typedef _AbiVersionNative = Uint32 Function();
typedef _AbiVersionDart = int Function();

typedef _VersionStringNative = Pointer<Uint8> Function();
typedef _VersionStringDart = Pointer<Uint8> Function();

typedef _AllocNative = Pointer<Uint8> Function(IntPtr);
typedef _AllocDart = Pointer<Uint8> Function(int);

typedef _FreeNative = Void Function(Pointer<Uint8>);
typedef _FreeDart = void Function(Pointer<Uint8>);

typedef _EngineCreateNative = Pointer<Void> Function(Pointer<CcEngineConfig>);
typedef _EngineCreateDart = Pointer<Void> Function(Pointer<CcEngineConfig>);

typedef _EngineDestroyNative = Void Function(Pointer<Void>);
typedef _EngineDestroyDart = void Function(Pointer<Void>);

typedef _EngineLanguageCountNative = IntPtr Function(Pointer<Void>);
typedef _EngineLanguageCountDart = int Function(Pointer<Void>);

typedef _EngineLanguageAtNative = Pointer<CcLanguageInfo> Function(
    Pointer<Void>, IntPtr);
typedef _EngineLanguageAtDart = Pointer<CcLanguageInfo> Function(
    Pointer<Void>, int);

typedef _EngineFindLanguageNative = Int32 Function(
    Pointer<Void>, Pointer<Uint8>);
typedef _EngineFindLanguageDart = int Function(Pointer<Void>, Pointer<Uint8>);

typedef _DocumentCreateNative = Pointer<Void> Function(
    Pointer<Void>, Pointer<CcDocumentOptions>);
typedef _DocumentCreateDart = Pointer<Void> Function(
    Pointer<Void>, Pointer<CcDocumentOptions>);

typedef _DocumentDestroyNative = Void Function(Pointer<Void>);
typedef _DocumentDestroyDart = void Function(Pointer<Void>);

typedef _SetTextNative = Int32 Function(
    Pointer<Void>, Pointer<Uint8>, IntPtr);
typedef _SetTextDart = int Function(Pointer<Void>, Pointer<Uint8>, int);

typedef _GetTextNative = Pointer<Uint8> Function(
    Pointer<Void>, Pointer<IntPtr>);
typedef _GetTextDart = Pointer<Uint8> Function(Pointer<Void>, Pointer<IntPtr>);

typedef _LengthNative = IntPtr Function(Pointer<Void>);
typedef _LengthDart = int Function(Pointer<Void>);

typedef _VersionNative = Uint64 Function(Pointer<Void>);
typedef _VersionDart = int Function(Pointer<Void>);

typedef _LanguageNative = Int32 Function(Pointer<Void>);
typedef _LanguageDart = int Function(Pointer<Void>);

typedef _SetLanguageNative = Int32 Function(Pointer<Void>, Int32);
typedef _SetLanguageDart = int Function(Pointer<Void>, int);

typedef _ReadOnlyNative = Uint8 Function(Pointer<Void>);
typedef _ReadOnlyDart = int Function(Pointer<Void>);

typedef _SetReadOnlyNative = Int32 Function(Pointer<Void>, Uint8);
typedef _SetReadOnlyDart = int Function(Pointer<Void>, int);

typedef _TabSizeNative = Uint32 Function(Pointer<Void>);
typedef _TabSizeDart = int Function(Pointer<Void>);

typedef _LastErrorNative = Pointer<Uint8> Function(Pointer<Void>);
typedef _LastErrorDart = Pointer<Uint8> Function(Pointer<Void>);

typedef _InsertNative = Int32 Function(
    Pointer<Void>, IntPtr, Pointer<Uint8>, IntPtr);
typedef _InsertDart = int Function(
    Pointer<Void>, int, Pointer<Uint8>, int);

typedef _EraseNative = Int32 Function(Pointer<Void>, IntPtr, IntPtr);
typedef _EraseDart = int Function(Pointer<Void>, int, int);

typedef _ReplaceNative = Int32 Function(
    Pointer<Void>, IntPtr, IntPtr, Pointer<Uint8>, IntPtr);
typedef _ReplaceDart = int Function(
    Pointer<Void>, int, int, Pointer<Uint8>, int);

typedef _BoolNative = Uint8 Function(Pointer<Void>);
typedef _BoolDart = int Function(Pointer<Void>);

typedef _StatusNative = Int32 Function(Pointer<Void>);
typedef _StatusDart = int Function(Pointer<Void>);

typedef _HistoryLimitNative = Void Function(Pointer<Void>, IntPtr);
typedef _HistoryLimitDart = void Function(Pointer<Void>, int);

typedef _LineCountNative = IntPtr Function(Pointer<Void>);
typedef _LineCountDart = int Function(Pointer<Void>);

typedef _LineAtNative = Int32 Function(
    Pointer<Void>, IntPtr, Pointer<CcLineInfo>);
typedef _LineAtDart = int Function(Pointer<Void>, int, Pointer<CcLineInfo>);

typedef _OffsetToPositionNative = Int32 Function(
    Pointer<Void>, IntPtr, Pointer<CcPosition>);
typedef _OffsetToPositionDart = int Function(
    Pointer<Void>, int, Pointer<CcPosition>);

typedef _PositionToOffsetNative = Int32 Function(
    Pointer<Void>, Uint32, Uint32, Pointer<IntPtr>);
typedef _PositionToOffsetDart = int Function(
    Pointer<Void>, int, int, Pointer<IntPtr>);

typedef _GetSelectionNative = CcSelection Function(Pointer<Void>);
typedef _GetSelectionDart = CcSelection Function(Pointer<Void>);

typedef _SetSelectionNative = Int32 Function(
    Pointer<Void>, Uint32, Uint32);
typedef _SetSelectionDart = int Function(Pointer<Void>, int, int);

typedef _MoveCursorNative = Int32 Function(Pointer<Void>, Uint32, Uint8);
typedef _MoveCursorDart = int Function(Pointer<Void>, int, int);

typedef _MatchBracketNative = Int32 Function(
    Pointer<Void>, IntPtr, Pointer<IntPtr>);
typedef _MatchBracketDart = int Function(
    Pointer<Void>, int, Pointer<IntPtr>);

typedef _TokenizeNative = IntPtr Function(
    Pointer<Void>, IntPtr, IntPtr, Pointer<Pointer<CcToken>>);
typedef _TokenizeDart = int Function(
    Pointer<Void>, int, int, Pointer<Pointer<CcToken>>);

typedef _FreeTokensNative = Void Function(Pointer<CcToken>);
typedef _FreeTokensDart = void Function(Pointer<CcToken>);

typedef _CompleteNative = IntPtr Function(
    Pointer<Void>, IntPtr, Pointer<Pointer<CcCompletionItem>>);
typedef _CompleteDart = int Function(
    Pointer<Void>, int, Pointer<Pointer<CcCompletionItem>>);

typedef _FreeCompletionsNative = Void Function(
    Pointer<CcCompletionItem>, IntPtr);
typedef _FreeCompletionsDart = void Function(Pointer<CcCompletionItem>, int);

typedef _DiagnoseNative = IntPtr Function(
    Pointer<Void>, Pointer<Pointer<CcDiagnostic>>);
typedef _DiagnoseDart = int Function(
    Pointer<Void>, Pointer<Pointer<CcDiagnostic>>);

typedef _FreeDiagnosticsNative = Void Function(Pointer<CcDiagnostic>, IntPtr);
typedef _FreeDiagnosticsDart = void Function(Pointer<CcDiagnostic>, int);

typedef _FoldingNative = IntPtr Function(
    Pointer<Void>, Pointer<Pointer<CcFoldingRange>>);
typedef _FoldingDart = int Function(
    Pointer<Void>, Pointer<Pointer<CcFoldingRange>>);

typedef _FreeFoldingNative = Void Function(Pointer<CcFoldingRange>);
typedef _FreeFoldingDart = void Function(Pointer<CcFoldingRange>);

typedef _FindNative = IntPtr Function(
    Pointer<Void>,
    Pointer<Uint8>,
    IntPtr,
    Pointer<CcSearchOptions>,
    Pointer<Pointer<CcRange>>);
typedef _FindDart = int Function(
    Pointer<Void>,
    Pointer<Uint8>,
    int,
    Pointer<CcSearchOptions>,
    Pointer<Pointer<CcRange>>);

typedef _FreeRangesNative = Void Function(Pointer<CcRange>);
typedef _FreeRangesDart = void Function(Pointer<CcRange>);

typedef _SetChangeListenerNative = Void Function(
    Pointer<Void>, Pointer<NativeFunction<ChangeCallbackNative>>, Pointer<Void>);
typedef _SetChangeListenerDart = void Function(
    Pointer<Void>, Pointer<NativeFunction<ChangeCallbackNative>>, Pointer<Void>);

/// Direct, unmanaged access to the native library.
///
/// The Dart SDK wraps this class; applications should not normally use it.
class CodeCoreBindings {
  CodeCoreBindings(this.library) {
    abiVersion = library.lookupFunction<_AbiVersionNative, _AbiVersionDart>(
        'cc_abi_version');
    versionString =
        library.lookupFunction<_VersionStringNative, _VersionStringDart>(
            'cc_version_string');
    alloc = library.lookupFunction<_AllocNative, _AllocDart>('cc_alloc');
    free = library.lookupFunction<_FreeNative, _FreeDart>('cc_free');
    stringFree =
        library.lookupFunction<_FreeNative, _FreeDart>('cc_string_free');
    engineCreate =
        library.lookupFunction<_EngineCreateNative, _EngineCreateDart>(
            'cc_engine_create');
    engineDestroy =
        library.lookupFunction<_EngineDestroyNative, _EngineDestroyDart>(
            'cc_engine_destroy');
    engineLanguageCount = library.lookupFunction<_EngineLanguageCountNative,
        _EngineLanguageCountDart>('cc_engine_language_count');
    engineLanguageAt = library.lookupFunction<_EngineLanguageAtNative,
        _EngineLanguageAtDart>('cc_engine_language_at');
    engineFindLanguage = library.lookupFunction<_EngineFindLanguageNative,
        _EngineFindLanguageDart>('cc_engine_find_language');
    documentCreate = library.lookupFunction<_DocumentCreateNative,
        _DocumentCreateDart>('cc_document_create');
    documentDestroy = library.lookupFunction<_DocumentDestroyNative,
        _DocumentDestroyDart>('cc_document_destroy');
    documentSetText =
        library.lookupFunction<_SetTextNative, _SetTextDart>(
            'cc_document_set_text');
    documentGetText =
        library.lookupFunction<_GetTextNative, _GetTextDart>(
            'cc_document_get_text');
    documentLength =
        library.lookupFunction<_LengthNative, _LengthDart>(
            'cc_document_length');
    documentVersion =
        library.lookupFunction<_VersionNative, _VersionDart>(
            'cc_document_version');
    documentLanguage =
        library.lookupFunction<_LanguageNative, _LanguageDart>(
            'cc_document_language');
    documentSetLanguage = library.lookupFunction<_SetLanguageNative,
        _SetLanguageDart>('cc_document_set_language');
    documentIsReadOnly = library.lookupFunction<_ReadOnlyNative, _ReadOnlyDart>(
        'cc_document_is_read_only');
    documentSetReadOnly = library.lookupFunction<_SetReadOnlyNative,
        _SetReadOnlyDart>('cc_document_set_read_only');
    documentTabSize =
        library.lookupFunction<_TabSizeNative, _TabSizeDart>(
            'cc_document_tab_size');
    documentLastError =
        library.lookupFunction<_LastErrorNative, _LastErrorDart>(
            'cc_document_last_error');
    documentInsert =
        library.lookupFunction<_InsertNative, _InsertDart>(
            'cc_document_insert');
    documentErase =
        library.lookupFunction<_EraseNative, _EraseDart>(
            'cc_document_erase');
    documentReplace =
        library.lookupFunction<_ReplaceNative, _ReplaceDart>(
            'cc_document_replace');
    documentCanUndo =
        library.lookupFunction<_BoolNative, _BoolDart>(
            'cc_document_can_undo');
    documentCanRedo =
        library.lookupFunction<_BoolNative, _BoolDart>(
            'cc_document_can_redo');
    documentUndo =
        library.lookupFunction<_StatusNative, _StatusDart>(
            'cc_document_undo');
    documentRedo =
        library.lookupFunction<_StatusNative, _StatusDart>(
            'cc_document_redo');
    documentClearHistory = library.lookupFunction<
        Void Function(Pointer<Void>),
        void Function(Pointer<Void>)>('cc_document_clear_history');
    documentSetHistoryLimit = library.lookupFunction<_HistoryLimitNative,
        _HistoryLimitDart>('cc_document_set_history_limit');
    documentLineCount =
        library.lookupFunction<_LineCountNative, _LineCountDart>(
            'cc_document_line_count');
    documentLineAt =
        library.lookupFunction<_LineAtNative, _LineAtDart>(
            'cc_document_line_at');
    documentOffsetToPosition = library.lookupFunction<
        _OffsetToPositionNative,
        _OffsetToPositionDart>('cc_document_offset_to_position');
    documentPositionToOffset = library.lookupFunction<
        _PositionToOffsetNative,
        _PositionToOffsetDart>('cc_document_position_to_offset');
    documentGetSelection = library.lookupFunction<_GetSelectionNative,
        _GetSelectionDart>('cc_document_get_selection');
    documentSetSelection = library.lookupFunction<_SetSelectionNative,
        _SetSelectionDart>('cc_document_set_selection');
    documentMoveCursor =
        library.lookupFunction<_MoveCursorNative, _MoveCursorDart>(
            'cc_document_move_cursor');
    documentMatchBracket = library.lookupFunction<_MatchBracketNative,
        _MatchBracketDart>('cc_document_match_bracket');
    documentTokenize =
        library.lookupFunction<_TokenizeNative, _TokenizeDart>(
            'cc_document_tokenize');
    tokensFree =
        library.lookupFunction<_FreeTokensNative, _FreeTokensDart>(
            'cc_tokens_free');
    documentComplete =
        library.lookupFunction<_CompleteNative, _CompleteDart>(
            'cc_document_complete');
    completionsFree = library.lookupFunction<_FreeCompletionsNative,
        _FreeCompletionsDart>('cc_completions_free');
    documentDiagnose =
        library.lookupFunction<_DiagnoseNative, _DiagnoseDart>(
            'cc_document_diagnose');
    diagnosticsFree = library.lookupFunction<_FreeDiagnosticsNative,
        _FreeDiagnosticsDart>('cc_diagnostics_free');
    documentFoldingRanges =
        library.lookupFunction<_FoldingNative, _FoldingDart>(
            'cc_document_folding_ranges');
    foldingRangesFree = library.lookupFunction<_FreeFoldingNative,
        _FreeFoldingDart>('cc_folding_ranges_free');
    documentFind =
        library.lookupFunction<_FindNative, _FindDart>('cc_document_find');
    searchRangesFree = library.lookupFunction<_FreeRangesNative,
        _FreeRangesDart>('cc_search_ranges_free');
    documentSetChangeListener = library.lookupFunction<
        _SetChangeListenerNative,
        _SetChangeListenerDart>('cc_document_set_change_listener');
  }

  final DynamicLibrary library;

  late final _AbiVersionDart abiVersion;
  late final _VersionStringDart versionString;
  late final _AllocDart alloc;
  late final _FreeDart free;
  late final _FreeDart stringFree;
  late final _EngineCreateDart engineCreate;
  late final _EngineDestroyDart engineDestroy;
  late final _EngineLanguageCountDart engineLanguageCount;
  late final _EngineLanguageAtDart engineLanguageAt;
  late final _EngineFindLanguageDart engineFindLanguage;
  late final _DocumentCreateDart documentCreate;
  late final _DocumentDestroyDart documentDestroy;
  late final _SetTextDart documentSetText;
  late final _GetTextDart documentGetText;
  late final _LengthDart documentLength;
  late final _VersionDart documentVersion;
  late final _LanguageDart documentLanguage;
  late final _SetLanguageDart documentSetLanguage;
  late final _ReadOnlyDart documentIsReadOnly;
  late final _SetReadOnlyDart documentSetReadOnly;
  late final _TabSizeDart documentTabSize;
  late final _LastErrorDart documentLastError;
  late final _InsertDart documentInsert;
  late final _EraseDart documentErase;
  late final _ReplaceDart documentReplace;
  late final _BoolDart documentCanUndo;
  late final _BoolDart documentCanRedo;
  late final _StatusDart documentUndo;
  late final _StatusDart documentRedo;
  late final void Function(Pointer<Void>) documentClearHistory;
  late final _HistoryLimitDart documentSetHistoryLimit;
  late final _LineCountDart documentLineCount;
  late final _LineAtDart documentLineAt;
  late final _OffsetToPositionDart documentOffsetToPosition;
  late final _PositionToOffsetDart documentPositionToOffset;
  late final _GetSelectionDart documentGetSelection;
  late final _SetSelectionDart documentSetSelection;
  late final _MoveCursorDart documentMoveCursor;
  late final _MatchBracketDart documentMatchBracket;
  late final _TokenizeDart documentTokenize;
  late final _FreeTokensDart tokensFree;
  late final _CompleteDart documentComplete;
  late final _FreeCompletionsDart completionsFree;
  late final _DiagnoseDart documentDiagnose;
  late final _FreeDiagnosticsDart diagnosticsFree;
  late final _FoldingDart documentFoldingRanges;
  late final _FreeFoldingDart foldingRangesFree;
  late final _FindDart documentFind;
  late final _FreeRangesDart searchRangesFree;
  late final _SetChangeListenerDart documentSetChangeListener;

  Pointer<Uint8> allocate(int bytes) {
    final pointer = alloc(bytes);
    if (pointer == nullptr) {
      throw const CodeCoreException(5, 'native allocation failed');
    }
    return pointer;
  }

  void freePointer(Pointer<NativeType> pointer) => free(pointer.cast<Uint8>());

  Pointer<Uint8> allocateUtf8(String value) {
    final bytes = utf8.encode(value);
    final pointer = allocate(bytes.length + 1);
    pointer.asTypedList(bytes.length).setAll(0, bytes);
    pointer[bytes.length] = 0;
    return pointer;
  }

  String readCString(Pointer<Uint8> pointer) {
    if (pointer == nullptr) {
      return '';
    }
    final bytes = <int>[];
    var index = 0;
    while (true) {
      final byte = pointer[index];
      if (byte == 0) {
        break;
      }
      bytes.add(byte);
      index += 1;
    }
    return utf8.decode(bytes, allowMalformed: true);
  }

  String get nativeVersion => readCString(versionString());
}
