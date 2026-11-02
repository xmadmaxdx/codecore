#include "core/language/language.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace codecore {
namespace {

std::vector<std::string> splitWords(const char* source) {
  std::vector<std::string> words;
  std::istringstream stream(source);
  std::string word;
  while (stream >> word) {
    words.push_back(word);
  }
  return words;
}

std::string lowercaseWord(const std::string& word) {
  std::string lower = word;
  std::transform(lower.begin(), lower.end(), lower.begin(), [](char c) {
    if (c >= 'A' && c <= 'Z') {
      return static_cast<char>(c - 'A' + 'a');
    }
    return c;
  });
  return lower;
}

constexpr uint32_t kSyntaxCapability = 0x1u;
constexpr uint32_t kCompletionCapability = 0x2u;
constexpr uint32_t kDiagnosticsCapability = 0x4u;
constexpr uint32_t kFoldingCapability = 0x8u;

}  // namespace

void Language::finalize() {
  keywordSet.clear();
  typeSet.clear();
  builtinSet.clear();
  keywordSet.reserve(keywords.size() * 2);
  typeSet.reserve(types.size() * 2);
  builtinSet.reserve(builtins.size() * 2);
  const auto normalize = [this](const std::string& word) {
    return caseInsensitive ? lowercaseWord(word) : word;
  };
  for (const std::string& word : keywords) {
    keywordSet.insert(normalize(word));
  }
  for (const std::string& word : types) {
    typeSet.insert(normalize(word));
  }
  for (const std::string& word : builtins) {
    builtinSet.insert(normalize(word));
  }
  capabilities = kSyntaxCapability | kCompletionCapability | kDiagnosticsCapability |
                 kFoldingCapability;
}

LanguageRegistry::LanguageRegistry() {
  auto add = [this](int32_t id, const char* idString, const char* displayName,
                    const char* extensions, std::vector<std::string> keywords,
                    std::vector<std::string> types, std::vector<std::string> builtins,
                    std::vector<std::string> lineComments, const char* blockStart,
                    const char* blockEnd, const char* quotes) -> Language& {
    auto language = std::make_shared<Language>();
    language->id = id;
    language->idString = idString;
    language->displayName = displayName;
    language->extensions = extensions;
    language->keywords = std::move(keywords);
    language->types = std::move(types);
    language->builtins = std::move(builtins);
    language->lineComments = std::move(lineComments);
    language->blockCommentStart = blockStart;
    language->blockCommentEnd = blockEnd;
    language->quotes = quotes;
    languages_.push_back(std::move(language));
    return *languages_.back();
  };

  {
    [[maybe_unused]] Language& language = add(
        0, "python", "Python", ".py,.pyw,.pyi",
        splitWords("False None True and as assert async await break class continue def del elif "
                   "else except finally for from global if import in is lambda nonlocal not or "
                   "pass raise return try while with yield match case"),
        splitWords("int float str bool list dict set tuple bytes bytearray complex frozenset "
                   "object type"),
        splitWords("print len range enumerate zip open isinstance issubclass super self cls "
                   "Exception ValueError TypeError KeyError IndexError"),
        {"#"}, "", "", "'\"");
    language.tripleQuotes = true;
  }
  {
    [[maybe_unused]] Language& language = add(
        1, "kotlin", "Kotlin", ".kt,.kts",
        splitWords("as break by catch class companion const constructor continue crossinline "
                   "data delegate do dynamic else enum expect external false field file final "
                   "finally for fun get if import in infix init inline inner interface internal "
                   "is lateinit noinline object open operator out override package private "
                   "protected public reified return sealed set super suspend tailrec this throw "
                   "true try typealias val var vararg when where while actual annotation"),
        splitWords("Any Array Boolean Byte Char Double Float Int Long Nothing Short String Unit "
                   "List MutableList Map MutableMap Set MutableSet Pair Triple"),
        splitWords("println print listOf mutableListOf mapOf mutableMapOf setOf mutableSetOf "
                   "require check error TODO let run apply also with takeIf takeUnless repeat"),
        {"//"}, "/*", "*/", "'\"");
    language.tripleQuotes = true;
  }
  {
    [[maybe_unused]] Language& language = add(
        2, "java", "Java", ".java",
        splitWords("abstract assert break case catch class const continue default do else enum "
                   "extends final finally for goto if implements import instanceof interface "
                   "native new package private protected public record return sealed static "
                   "strictfp super switch synchronized this throw throws transient try var "
                   "volatile while yield permits non-sealed"),
        splitWords("boolean byte char double float int long short void String Object Integer "
                   "Long Double Float Boolean Character List Map Set ArrayList HashMap HashSet "
                   "Optional Stream"),
        splitWords("System out println print main args this super"),
        {"//"}, "/*", "*/", "'\"");
    language.tripleQuotes = true;
  }
  {
    [[maybe_unused]] Language& language = add(
        3, "javascript", "JavaScript", ".js,.mjs,.cjs,.jsx",
        splitWords("async await break case catch class const continue debugger default delete "
                   "do else export extends false finally for from function get if import in "
                   "instanceof let new null of return set static super switch this throw true "
                   "try typeof undefined var void while with yield"),
        splitWords("Array Boolean Date Error Function Map Number Object Promise Proxy RegExp Set "
                   "String Symbol BigInt"),
        splitWords("console document window globalThis JSON Math process require module exports"),
        {"//"}, "/*", "*/", "'\"`");
  }
  {
    [[maybe_unused]] Language& language = add(
        4, "typescript", "TypeScript", ".ts,.tsx,.mts,.cts",
        splitWords("abstract any as asserts async await boolean break case catch class const "
                   "constructor continue declare default delete do else enum export extends "
                   "false finally for from function get if implements import in infer instanceof "
                   "interface is keyof let module namespace never new null number object of "
                   "override private protected public readonly return satisfies set static "
                   "string super switch symbol this throw true try type typeof undefined unique "
                   "unknown var void while with yield"),
        splitWords("Array Boolean Date Error Function Map Number Object Promise Record Partial "
                   "Readonly Pick Omit RegExp Set String Symbol BigInt"),
        splitWords("console document window globalThis JSON Math process require module exports"),
        {"//"}, "/*", "*/", "'\"`");
  }
  {
    [[maybe_unused]] Language& language = add(
        5, "c", "C", ".c,.h",
        splitWords("auto break case char const continue default do double else enum extern "
                   "float for goto if inline int long register restrict return short signed "
                   "sizeof static struct switch typedef union unsigned void volatile while "
                   "_Bool _Complex _Atomic"),
        splitWords("size_t ssize_t ptrdiff_t int8_t int16_t int32_t int64_t uint8_t uint16_t "
                   "uint32_t uint64_t FILE bool"),
        splitWords("NULL true false"),
        {"//"}, "/*", "*/", "'\"");
  }
  {
    [[maybe_unused]] Language& language = add(
        6, "cpp", "C++", ".cpp,.cc,.cxx,.hpp,.hxx,.hh",
        splitWords("alignas alignof and and_eq asm auto bitand bitor bool break case catch class "
                   "compl concept const consteval constexpr constinit const_cast continue "
                   "co_await co_return co_yield decltype default delete do dynamic_cast else "
                   "enum explicit export extern false final for friend goto if inline int long "
                   "mutable namespace new noexcept not not_eq nullptr operator or or_eq override "
                   "private protected public register reinterpret_cast requires return short "
                   "signed sizeof static static_assert static_cast struct switch template this "
                   "throw true try typedef typeid typename union unsigned using virtual void "
                   "volatile wchar_t while xor xor_eq"),
        splitWords("std string vector map unordered_map set unordered_set unique_ptr shared_ptr "
                   "size_t int8_t int16_t int32_t int64_t uint8_t uint16_t uint32_t uint64_t "
                   "optional variant function"),
        splitWords("cout cin cerr endl move forward make_unique make_shared printf"),
        {"//"}, "/*", "*/", "'\"");
  }
  {
    [[maybe_unused]] Language& language = add(
        7, "csharp", "C#", ".cs",
        splitWords("abstract as async await base break case catch checked class const continue "
                   "default delegate do double else enum event explicit extern false finally "
                   "fixed for foreach goto if implicit in int interface internal is lock long "
                   "namespace new null object operator out override params private protected "
                   "public readonly record ref return sbyte sealed short sizeof stackalloc "
                   "static string struct switch this throw true try typeof uint ulong unchecked "
                   "unsafe ushort using virtual void volatile while var yield"),
        splitWords("Boolean Byte Char Decimal Double Float Int32 Int64 Object SByte String UInt32 "
                   "UInt64 List Dictionary Task IEnumerable"),
        splitWords("Console WriteLine ReadLine Task"),
        {"//"}, "/*", "*/", "'\"");
  }
  {
    [[maybe_unused]] Language& language = add(
        8, "go", "Go", ".go",
        splitWords("break case chan const continue default defer else fallthrough for func go "
                   "goto if import interface map package range return select struct switch type "
                   "var"),
        splitWords("bool byte complex64 complex128 error float32 float64 int int8 int16 int32 "
                   "int64 rune string uint uint8 uint16 uint32 uint64 uintptr any"),
        splitWords("append cap close complex copy delete imag len make new panic print println "
                   "real recover"),
        {"//"}, "/*", "*/", "'\"`");
  }
  {
    [[maybe_unused]] Language& language = add(
        9, "rust", "Rust", ".rs",
        splitWords("as async await break const continue crate dyn else enum extern false fn for "
                   "if impl in let loop match mod move mut pub ref return self Self static struct "
                   "super trait true type unsafe use where while"),
        splitWords("i8 i16 i32 i64 i128 isize u8 u16 u32 u64 u128 usize f32 f64 bool char str "
                   "String Vec Option Result Box Rc Arc HashMap"),
        splitWords("println print format vec Some None Ok Err panic assert"),
        {"//"}, "/*", "*/", "'\"");
  }
  {
    [[maybe_unused]] Language& language = add(
        10, "dart", "Dart", ".dart",
        splitWords("abstract as assert async await break case catch class const continue "
                   "covariant default deferred do dynamic else enum export extends extension "
                   "external factory false final finally for Function get hide if implements "
                   "import in interface is late library mixin new null on operator part required "
                   "rethrow return set show static super switch sync this throw true try "
                   "typedef var void while with yield"),
        splitWords("int double num bool String List Map Set Future Stream Object dynamic"),
        splitWords("print debugPrint Future"),
        {"//"}, "/*", "*/", "'\"");
    language.tripleQuotes = true;
  }
  {
    [[maybe_unused]] Language& language = add(
        11, "php", "PHP", ".php",
        splitWords("abstract and array as break callable case catch class clone const continue "
                   "declare default do echo else elseif empty enddeclare endfor endforeach endif "
                   "endswitch endwhile eval exit extends final finally fn for foreach function "
                   "global goto if implements include instanceof insteadof interface isset list "
                   "match namespace new or print private protected public readonly require "
                   "return static switch throw trait try unset use var while xor yield"),
        splitWords("int float string bool array object mixed void null false true self parent"),
        splitWords("echo print_r var_dump count strlen array_map array_filter"),
        {"//", "#"}, "/*", "*/", "'\"");
  }
  {
    [[maybe_unused]] Language& language = add(
        12, "ruby", "Ruby", ".rb,.rake,Gemfile",
        splitWords("BEGIN END alias and begin break case class def defined do else elsif end "
                   "ensure false for if in module next nil not or redo rescue retry return self "
                   "super then true undef unless until when while yield"),
        splitWords("Array Hash String Symbol Integer Float Range Proc"),
        splitWords("puts print require attr_reader attr_accessor each map select"),
        {"#"}, "", "", "'\"");
  }
  {
    [[maybe_unused]] Language& language = add(
        13, "swift", "Swift", ".swift",
        splitWords("associatedtype class deinit enum extension fileprivate func import init "
                   "inout internal let open operator private protocol public rethrows static "
                   "struct subscript typealias var break case continue default defer do else "
                   "fallthrough for guard if in repeat return switch where while as catch false "
                   "is nil super self Self throw throws true try async await"),
        splitWords("Int Double Float String Bool Array Dictionary Set Optional Result"),
        splitWords("print fatalError precondition"),
        {"//"}, "/*", "*/", "'\"");
    language.tripleQuotes = true;
  }
  {
    [[maybe_unused]] Language& language = add(14, "html", "HTML", ".html,.htm,.xhtml", {}, {}, {}, {},
                             "<!--", "-->", "'\"");
    language.markup = true;
  }
  {
    [[maybe_unused]] Language& language = add(
        15, "css", "CSS", ".css",
        splitWords("media supports import keyframes font-face page chars namespace"),
        splitWords("px rem em vh vw percent solid dashed dotted bold italic"),
        splitWords("none block inline absolute relative fixed sticky inherit initial unset "
                   "important"),
        {}, "/*", "*/", "'\"");
    language.cssLike = true;
  }
  {
    [[maybe_unused]] Language& language = add(
        16, "scss", "SCSS", ".scss",
        splitWords("media supports import keyframes font-face include mixin extend use forward "
                   "if else each for while return function content"),
        splitWords("px rem em vh vw percent solid dashed dotted bold italic"),
        splitWords("none block inline absolute relative fixed sticky inherit initial unset "
                   "important"),
        {}, "/*", "*/", "'\"");
    language.cssLike = true;
  }
  {
    [[maybe_unused]] Language& language = add(
        17, "sql", "SQL", ".sql",
        splitWords("SELECT FROM WHERE INSERT INTO VALUES UPDATE SET DELETE CREATE TABLE ALTER "
                   "DROP JOIN INNER LEFT RIGHT OUTER FULL ON GROUP BY ORDER HAVING LIMIT OFFSET "
                   "UNION ALL DISTINCT AS AND OR NOT NULL IS LIKE IN BETWEEN EXISTS CASE WHEN "
                   "THEN ELSE END PRIMARY KEY FOREIGN REFERENCES DEFAULT INDEX VIEW DATABASE "
                   "TRANSACTION COMMIT ROLLBACK WITH RECURSIVE RETURNING"),
        splitWords("INT INTEGER BIGINT SMALLINT VARCHAR CHAR TEXT BOOLEAN DATE TIMESTAMP DECIMAL "
                   "NUMERIC FLOAT DOUBLE JSON JSONB UUID"),
        splitWords("COUNT SUM AVG MIN MAX COALESCE NOW CURRENT_TIMESTAMP"),
        {"--"}, "/*", "*/", "'\"");
    language.caseInsensitive = true;
  }
  {
    [[maybe_unused]] Language& language = add(18, "json", "JSON", ".json", {}, {}, splitWords("true false null"),
                             {}, "", "", "\"");
  }
  {
    [[maybe_unused]] Language& language = add(19, "xml", "XML", ".xml,.svg,.plist", {}, {}, {},
                             {}, "<!--", "-->", "'\"");
    language.markup = true;
  }
  {
    [[maybe_unused]] Language& language = add(20, "yaml", "YAML", ".yml,.yaml", {}, {},
                             splitWords("true false null yes no on off"), {"#"}, "", "", "'\"");
  }
  {
    [[maybe_unused]] Language& language = add(
        21, "bash", "Bash", ".sh,.bash,.zsh",
        splitWords("if then else elif fi for while until do done case esac function in select "
                   "time coproc return break continue local export readonly declare typeset "
                   "unset shift source alias"),
        splitWords("string int array"),
        splitWords("echo printf cd pwd export local source alias test read set unset"),
        {"#"}, "", "", "'\"`");
  }
  {
    [[maybe_unused]] Language& language = add(
        22, "lua", "Lua", ".lua",
        splitWords("and break do else elseif end false for function goto if in local nil not or "
                   "repeat return then true until while"),
        splitWords("string table number boolean nil function thread userdata"),
        splitWords("print pairs ipairs require tostring tonumber type pcall error assert"),
        {"--"}, "--[[", "]]", "'\"");
  }

  for (auto& language : languages_) {
    language->finalize();
  }
}

const Language* LanguageRegistry::at(size_t index) const {
  if (index >= languages_.size()) {
    return nullptr;
  }
  return languages_[index].get();
}

std::shared_ptr<const Language> LanguageRegistry::sharedById(int32_t id) const {
  for (const auto& language : languages_) {
    if (language->id == id) {
      return language;
    }
  }
  return nullptr;
}

std::shared_ptr<const Language> LanguageRegistry::sharedByIdString(const std::string& id) const {
  for (const auto& language : languages_) {
    if (language->idString == id) {
      return language;
    }
  }
  return nullptr;
}

int32_t LanguageRegistry::findId(const std::string& id) const {
  for (const auto& language : languages_) {
    if (language->idString == id) {
      return language->id;
    }
  }
  return -1;
}

}  // namespace codecore
