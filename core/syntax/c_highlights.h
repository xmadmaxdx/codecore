#pragma once

/*
 * C highlight query for the bundled tree-sitter-c grammar.
 * Predicate-free: the C query API does not evaluate #match? predicates.
 */
namespace codecore {
namespace syntax_queries {

inline constexpr const char* kCHighlights = R"QUERY(
(comment) @comment
[(string_literal) (char_literal)] @string
(number_literal) @number

(function_definition declarator: (function_declarator declarator: (identifier) @function))
(call_expression function: (identifier) @function.call)
(declaration type: (type_identifier) @type)
(primitive_type) @type
(field_identifier) @property
(identifier) @variable

[
  "auto" "break" "case" "const" "continue" "default" "do" "else" "enum"
  "extern" "for" "goto" "if" "inline" "register" "restrict" "return"
  "sizeof" "static" "struct" "switch" "typedef" "union" "volatile" "while"
  "_Atomic"
] @keyword

["(" ")" "[" "]" "{" "}"] @punctuation.bracket
)QUERY";

}  // namespace syntax_queries
}  // namespace codecore
