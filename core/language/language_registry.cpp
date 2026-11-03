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

  {
    [[maybe_unused]] Language& language = add(
        23, "perl", "Perl", ".pl,.pm,.t",
        splitWords("break continue do else elsif for foreach given goto if last local my next no "
                   "our package redo require return sub unless until use when while and or not eq "
                   "ne lt gt le ge cmp"),
        splitWords("scalar array hash"),
        splitWords("print printf say chomp chop split join push pop shift unshift keys values each "
                   "sort reverse map grep length substr index"),
        {"#"}, "", "", "'\"");
  }
  {
    [[maybe_unused]] Language& language = add(
        24, "r", "R", ".r,.R,.Rmd",
        splitWords("break else for function if in next repeat return while TRUE FALSE NULL NA Inf "
                   "NaN"),
        splitWords("numeric character logical integer double complex list matrix data.frame factor"),
        splitWords("print cat paste c mean median sd sum length seq rep library require"),
        {"#"}, "", "", "'\"");
  }
  {
    [[maybe_unused]] Language& language = add(
        25, "scala", "Scala", ".scala,.sc",
        splitWords("abstract case catch class def do else extends final finally for forSome if "
                   "implicit import lazy match new object override package private protected return "
                   "sealed super this throw trait try type val var while with yield given using enum "
                   "export then"),
        splitWords("Any AnyRef AnyVal Boolean Byte Char Double Float Int Long Nothing Short String "
                   "Unit List Map Set Option Either Future"),
        splitWords("println print require assert map flatMap filter fold"),
        {"//"}, "/*", "*/", "'\"");
    language.tripleQuotes = true;
  }
  {
    [[maybe_unused]] Language& language = add(
        26, "groovy", "Groovy", ".groovy,.gradle,.gvy",
        splitWords("as assert break case catch class const continue def default do else enum extends "
                   "finally for goto if implements import in instanceof interface native new package "
                   "private protected public return static super switch synchronized this throw "
                   "throws trait try while"),
        splitWords("Boolean Byte Char Double Float Integer Long Object Short String List Map Set "
                   "Closure"),
        splitWords("println print each collect find findAll inject"),
        {"//"}, "/*", "*/", "'\"");
    language.tripleQuotes = true;
  }
  {
    [[maybe_unused]] Language& language = add(
        27, "haskell", "Haskell", ".hs,.lhs",
        splitWords("case class data default deriving do else if import in infix infixl infixr "
                   "instance let module newtype of then type where qualified as hiding forall "
                   "foreign"),
        splitWords("Bool Char Double Either Float IO Int Integer Maybe Ordering String Word"),
        splitWords("map filter foldl foldr head tail null length print putStrLn show read"),
        {"--"}, "{-", "-}", "'\"");
  }
  {
    [[maybe_unused]] Language& language = add(
        28, "elixir", "Elixir", ".ex,.exs",
        splitWords("after alias and case catch cond def defmodule defp defmacro defstruct do else "
                   "end fn for if import in not or quote raise receive require rescue try unless "
                   "unquote use when with"),
        splitWords("Atom BitString Float Function Integer List Map PID Port Reference Tuple"),
        splitWords("IO.puts IO.inspect Enum.map Enum.filter Enum.reduce String.length Kernel"),
        {"#"}, "", "", "'\"");
    language.tripleQuotes = true;
  }
  {
    [[maybe_unused]] Language& language = add(
        29, "erlang", "Erlang", ".erl,.hrl",
        splitWords("after and andalso begin case catch cond end fun if let not of or orelse "
                   "receive try when bnot div rem band bor bxor"),
        splitWords("atom binary bitstring boolean byte char float function integer list map nil "
                   "number pid port reference tuple"),
        splitWords("io format lists map length spawn self"),
        {"%"}, "", "", "'\"");
  }
  {
    [[maybe_unused]] Language& language = add(
        30, "clojure", "Clojure", ".clj,.cljs,.cljc,.edn",
        splitWords("def defn defmacro defmulti defmethod if if-not let letfn loop recur fn do when "
                   "when-not when-let cond case try catch finally throw new ns require import quote "
                   "var"),
        splitWords("long int double float boolean string keyword symbol vector list map set"),
        splitWords("println print str map filter reduce apply assoc conj get"),
        {";"}, "", "", "'\"");
  }
  {
    [[maybe_unused]] Language& language = add(
        31, "ocaml", "OCaml", ".ml,.mli",
        splitWords("and as assert begin class constraint do done downto else end exception external "
                   "false for fun function functor if in include inherit initializer lazy let match "
                   "method module mutable new object of open or private rec sig struct then to true "
                   "try type val virtual when while with"),
        splitWords("bool char float int int32 int64 list nativeint option string unit bytes"),
        splitWords("print_endline print_string List.map List.filter Option.map"),
        {}, "(*", "*)", "'\"");
  }
  {
    [[maybe_unused]] Language& language = add(
        32, "fsharp", "F#", ".fs,.fsi,.fsx",
        splitWords("abstract and as assert base begin class default delegate do done downcast downto "
                   "elif else end exception extern false finally fixed for fun function global if in "
                   "inherit inline interface internal lazy let match member module mutable namespace "
                   "new null of open or override private public rec return select static struct then "
                   "to true try type upcast use val void when while with yield"),
        splitWords("bool byte char decimal double float int int64 list map option seq string uint"),
        splitWords("printfn printf failwith List.map Seq.map Option.map"),
        {"//"}, "(*", "*)", "'\"");
    language.tripleQuotes = true;
  }
  {
    [[maybe_unused]] Language& language = add(
        33, "visualbasic", "Visual Basic", ".vb",
        splitWords("AddHandler AddressOf And AndAlso As ByRef ByVal Call Case Catch Class Const "
                   "Continue Declare Default Delegate Dim DirectCast Do Each Else ElseIf End Enum Erase "
                   "Error Event Exit False Finally For Friend Function Get GetType Global GoSub GoTo "
                   "Handles If Implements Imports In Inherits Interface Is IsNot Let Lib Like Loop Me "
                   "Mod Module MustInherit MustOverride MyBase MyClass Namespace New Next Not Nothing "
                   "Object Of On Operator Option Optional Or OrElse Out Overloads Overridable "
                   "Overrides ParamArray Partial Private Property Protected Public RaiseEvent ReadOnly "
                   "ReDim REM RemoveHandler Resume Return Select Set Shadows Shared Static Step Stop "
                   "Structure Sub SyncLock Then Throw To True Try TryCast TypeOf Using Variant Wend "
                   "When While Widening With WithEvents WriteOnly Xor"),
        splitWords("Boolean Byte Char Date Decimal Double Integer Long Object SByte Short Single String "
                   "UInteger ULong UShort"),
        splitWords("Console.WriteLine Console.ReadLine MsgBox"),
        {"'"}, "", "", "\"");
    language.caseInsensitive = true;
  }
  {
    [[maybe_unused]] Language& language = add(
        34, "powershell", "PowerShell", ".ps1,.psm1,.psd1",
        splitWords("begin break catch class continue data define do dynamicparam else elseif end enum "
                   "exit filter finally for foreach from function hidden if in param process return "
                   "static switch throw trap try until using var while workflow"),
        splitWords("bool byte char datetime decimal double float int long string array hashtable "
                   "pscustomobject"),
        splitWords("Write-Host Write-Output Get-ChildItem Get-Content Set-Content Where-Object "
                   "ForEach-Object Select-Object"),
        {"#"}, "<#", "#>", "'\"");
  }
  {
    [[maybe_unused]] Language& language = add(
        35, "objectivec", "Objective-C", ".m,.mm",
        splitWords("auto break case char const continue default do double else enum extern float for "
                   "goto if inline int long register restrict return short signed sizeof static struct "
                   "switch typedef union unsigned void volatile while interface implementation end "
                   "class protocol property synthesize dynamic public private protected try catch "
                   "finally throw selector encode id self super nil YES NO"),
        splitWords("NSInteger NSUInteger CGFloat BOOL NSString NSArray NSDictionary NSObject"),
        splitWords("NSLog alloc init new copy mutableCopy"),
        {"//"}, "/*", "*/", "'\"");
  }
  {
    [[maybe_unused]] Language& language = add(
        36, "assembly", "Assembly", ".asm,.s,.inc",
        splitWords("mov add sub mul imul div idiv inc dec and or xor not shl shr sal sar cmp test jmp "
                   "je jne jz jnz jg jge jl jle call ret push pop lea nop int syscall hlt"),
        splitWords("rax rbx rcx rdx rsi rdi rbp rsp r8 r9 r10 r11 r12 r13 r14 r15 eax ebx ecx edx "
                   "ax bx cx dx al bl cl dl ah bh ch dh"),
        splitWords("section global extern db dw dd dq equ times"),
        {";", "#"}, "", "", "'\"");
  }
  {
    [[maybe_unused]] Language& language = add(
        37, "matlab", "MATLAB", ".m",
        splitWords("break case catch classdef continue else elseif end for function global if "
                   "otherwise parfor persistent return spmd switch try while"),
        splitWords("double single int8 int16 int32 int64 uint8 uint16 uint32 uint64 logical char "
                   "string cell struct table function_handle"),
        splitWords("disp fprintf sprintf plot figure zeros ones size length numel sum mean"),
        {"%"}, "%{", "%}", "'\"");
  }
  {
    [[maybe_unused]] Language& language = add(
        38, "julia", "Julia", ".jl",
        splitWords("abstract baremodule begin break catch const continue do else elseif end export "
                   "finally for function global if import in let local macro module quote return "
                   "struct try using while where mutable primitive type"),
        splitWords("Any Bool Char Float16 Float32 Float64 Int Int8 Int16 Int32 Int64 Integer "
                   "Nothing String Symbol Tuple Vector Matrix Dict"),
        splitWords("println print show length push pop map filter reduce sum"),
        {"#"}, "#=", "=#", "'\"");
    language.tripleQuotes = true;
  }
  {
    [[maybe_unused]] Language& language = add(
        39, "fortran", "Fortran", ".f,.f90,.f95,.f03,.f08",
        splitWords("allocatable allocate assign associate asynchronous backspace bind block call case "
                   "class close common contains continue cycle data deallocate default dimension do "
                   "else elseif elsewhere end enddo endif entry enum enumerator equivalence exit "
                   "extends external final flush forall format function generic goto if implicit "
                   "import include inquire intent interface intrinsic module namelist none nullify "
                   "only open operator optional parameter pause pointer print private procedure "
                   "program public pure read real rec recursive result return rewind save select "
                   "sequence stop subroutine target then type use value volatile wait where while write"),
        splitWords("integer real double precision complex logical character"),
        splitWords("print write read open close allocate deallocate"),
        {"!"}, "", "", "'\"");
    language.caseInsensitive = true;
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
