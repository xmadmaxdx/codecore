#pragma once

/*
 * Rust highlight query for the bundled tree-sitter-rust grammar.
 * Predicate-free: the C query API does not evaluate #match? predicates.
 */
namespace codecore {
namespace syntax_queries {

inline constexpr const char* kRustHighlights = R"QUERY(
[(line_comment) (block_comment)] @comment
[(string_literal) (raw_string_literal) (char_literal)] @string
[(integer_literal) (float_literal)] @number
(boolean_literal) @constant.builtin

(function_item name: (identifier) @function)
(struct_item name: (type_identifier) @type)
(enum_item name: (type_identifier) @type)
(trait_item name: (type_identifier) @type)
(call_expression function: (identifier) @function.call)
(field_identifier) @property
(identifier) @variable

[
  "as" "async" "await" "break" "const" "continue" (crate) "dyn" "else"
  "enum" "extern" "false" "fn" "for" "if" "impl" "in" "let" "loop" "match"
  "mod" "move" "pub" "ref" "return" (self) "static" "struct"
  (super) "trait" "true" "type" "unsafe" "use" "where" "while"
] @keyword

["(" ")" "[" "]" "{" "}"] @punctuation.bracket
)QUERY";

}  // namespace syntax_queries
}  // namespace codecore
