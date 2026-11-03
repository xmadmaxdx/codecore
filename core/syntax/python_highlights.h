#pragma once

/*
 * Python highlight query for the bundled tree-sitter-python grammar.
 *
 * Deliberately predicate-free: the C query API does not evaluate #match?
 * predicates, so patterns that rely on them would highlight every identifier.
 * Order does not matter; captures are ranked by specificity in syntax.cpp.
 */
namespace codecore {
namespace syntax_queries {

inline constexpr const char* kPythonHighlights = R"QUERY(
(comment) @comment
(string) @string
(integer) @number
(float) @number
[(none) (true) (false)] @constant.builtin

(function_definition name: (identifier) @function)
(class_definition name: (identifier) @type)
(decorator) @function
(call function: (attribute attribute: (identifier) @function.method))
(call function: (identifier) @function.call)
(attribute attribute: (identifier) @property)
(type (identifier) @type)
(parameters (identifier) @variable.parameter)
(identifier) @variable

[
  "as" "assert" "async" "await" "break" "class" "continue" "def" "del"
  "elif" "else" "except" "finally" "for" "from" "global" "if" "import"
  "lambda" "match" "case" "nonlocal" "pass" "raise" "return" "try"
  "while" "with" "yield"
] @keyword

[
  "-" "-=" "!=" "*" "**" "**=" "*=" "/" "//" "//=" "/=" "&" "&=" "%" "%="
  "^" "^=" "+" "+=" "<" "<<" "<<=" "<=" "==" ">" ">=" ">>" ">>=" "|" "|="
  "~" "=" ":=" "->" "@="
] @operator

[
  "(" ")" "[" "]" "{" "}"
] @punctuation.bracket

[
  "." "," ":" ";"
] @punctuation.delimiter
)QUERY";

}  // namespace syntax_queries
}  // namespace codecore
