#pragma once

/*
 * C++ highlight query for the bundled tree-sitter-cpp grammar.
 * Predicate-free: the C query API does not evaluate #match? predicates.
 */
namespace codecore {
namespace syntax_queries {

inline constexpr const char* kCppHighlights = R"QUERY(
(comment) @comment
[(string_literal) (char_literal)] @string
(number_literal) @number
[(true) (false) "nullptr"] @constant.builtin

(function_definition declarator: (function_declarator declarator: [(identifier) (field_identifier)] @function))
(call_expression function: (identifier) @function.call)
(declaration type: (type_identifier) @type)
(primitive_type) @type
(class_specifier name: (type_identifier) @type)
(struct_specifier name: (type_identifier) @type)
(enum_specifier name: (type_identifier) @type)
(field_identifier) @property
(identifier) @variable

[
  "alignas" "alignof" "asm" (auto) "break" "case" "catch" "class" "concept"
  "const" "consteval" "constexpr" "constinit" "continue"
  "decltype" "default" "delete" "do" "else" "enum" "explicit"
  "extern" (false) "final" "for" "friend" "goto" "if" "inline"
  "mutable" "namespace" "new" "noexcept" "operator" "override"
  "private" "protected" "public" "register" "requires"
  "return" "static" "static_assert" "struct" "switch"
  "template" (this) "throw" (true) "try" "typedef" "typename"
  "union" "using" "virtual" "volatile" "while"
] @keyword

["(" ")" "[" "]" "{" "}"] @punctuation.bracket
)QUERY";

}  // namespace syntax_queries
}  // namespace codecore
