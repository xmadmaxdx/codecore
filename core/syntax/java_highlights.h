#pragma once

/*
 * Java highlight query for the bundled tree-sitter-java grammar.
 * Predicate-free: the C query API does not evaluate #match? predicates.
 */
namespace codecore {
namespace syntax_queries {

inline constexpr const char* kJavaHighlights = R"QUERY(
[(line_comment) (block_comment)] @comment
(string_literal) @string
(character_literal) @string
[
  (decimal_integer_literal)
  (hex_integer_literal)
  (octal_integer_literal)
  (binary_integer_literal)
  (decimal_floating_point_literal)
  (hex_floating_point_literal)
] @number
[(true) (false) (null_literal)] @constant.builtin

(method_declaration name: (identifier) @function)
(constructor_declaration name: (identifier) @function)
(class_declaration name: (identifier) @type)
(interface_declaration name: (identifier) @type)
(enum_declaration name: (identifier) @type)
(method_invocation name: (identifier) @function.call)
(formal_parameter name: (identifier) @variable.parameter)
(identifier) @variable

[
  "abstract" "assert" "break" "case" "catch" "class" "continue"
  "default" "do" "else" "enum" "extends" "final" "finally" "for" "if"
  "implements" "import" "instanceof" "interface" "native" "new" "package"
  "private" "protected" "public" "return" "sealed" "static" "strictfp"
  (super) "switch" "synchronized" (this) "throw" "throws" "transient" "try"
  "volatile" "while" "yield" "record"
] @keyword

["(" ")" "[" "]" "{" "}"] @punctuation.bracket
)QUERY";

}  // namespace syntax_queries
}  // namespace codecore
