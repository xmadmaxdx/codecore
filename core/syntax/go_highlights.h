#pragma once

/*
 * Go highlight query for the bundled tree-sitter-go grammar.
 * Predicate-free: the C query API does not evaluate #match? predicates.
 */
namespace codecore {
namespace syntax_queries {

inline constexpr const char* kGoHighlights = R"QUERY(
(comment) @comment
[(interpreted_string_literal) (raw_string_literal) (rune_literal)] @string
[(int_literal) (float_literal) (imaginary_literal)] @number
[(true) (false) (nil)] @constant.builtin

(function_declaration name: (identifier) @function)
(method_declaration name: (field_identifier) @function.method)
(type_declaration (type_spec name: (type_identifier) @type))
(call_expression function: (identifier) @function.call)
(call_expression function: (selector_expression field: (field_identifier) @function.method))
(parameter_declaration name: (identifier) @variable.parameter)
(field_identifier) @property
(identifier) @variable

[
  "break" "case" "chan" "const" "continue" "default" "defer" "else"
  "fallthrough" "for" "func" "go" "goto" "if" "import" "interface" "map"
  "package" "range" "return" "select" "struct" "switch" "type" "var"
] @keyword

["(" ")" "[" "]" "{" "}"] @punctuation.bracket
)QUERY";

}  // namespace syntax_queries
}  // namespace codecore
