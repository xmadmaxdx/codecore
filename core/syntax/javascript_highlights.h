#pragma once

/*
 * JavaScript highlight query for the bundled tree-sitter-javascript grammar.
 *
 * Deliberately predicate-free: the C query API does not evaluate #match? /
 * #eq? predicates, so upstream patterns that classify identifiers by spelling
 * (constructors, constants, builtins) are omitted. Captures are ranked by
 * specificity in syntax.cpp, so ordering does not matter here.
 */
namespace codecore {
namespace syntax_queries {

inline constexpr const char* kJavaScriptHighlights = R"QUERY(
(comment) @comment
(string) @string
(template_string) @string
(regex) @string
(number) @number
[(true) (false) (null) (undefined)] @constant.builtin
[(this) (super)] @variable.builtin

(function_declaration name: (identifier) @function)
(function_expression name: (identifier) @function)
(generator_function_declaration name: (identifier) @function)
(method_definition name: (property_identifier) @function.method)
(class_declaration name: (identifier) @type)
(class) @type
(call_expression function: (identifier) @function.call)
(call_expression function: (member_expression property: (property_identifier) @function.method))
(new_expression constructor: (identifier) @constructor)
(property_identifier) @property
(pair key: (property_identifier) @property)
(identifier) @variable

[
  "as" "async" "await" "break" "case" "catch" "class" "const" "continue"
  "debugger" "default" "delete" "do" "else" "export" "extends" "finally"
  "for" "from" "function" "get" "if" "import" "in" "instanceof" "let" "new"
  "of" "return" "set" "static" "switch" "target" "throw" "try" "typeof"
  "var" "void" "while" "with" "yield"
] @keyword

[
  "-" "--" "-=" "+" "++" "+=" "*" "*=" "**" "**=" "/" "/=" "%" "%=" "<"
  "<=" "<<" "<<=" "=" "==" "===" "!" "!=" "!==" "=>" ">" ">=" ">>" ">>="
  ">>>" ">>>=" "~" "^" "&" "|" "^=" "&=" "|=" "&&" "||" "??" "&&=" "||="
  "??="
] @operator

[
  "(" ")" "[" "]" "{" "}"
] @punctuation.bracket

[
  ";" "." "," (optional_chain)
] @punctuation.delimiter

(template_substitution
  "${" @punctuation.special
  "}" @punctuation.special) @embedded
)QUERY";

}  // namespace syntax_queries
}  // namespace codecore
