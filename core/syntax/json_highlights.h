#pragma once

/*
 * JSON highlight query for the bundled tree-sitter-json grammar.
 * JSON has no comments, so the query only captures strings, numbers,
 * literals and structure keys.
 */
namespace codecore {
namespace syntax_queries {

inline constexpr const char* kJsonHighlights = R"QUERY(
(string) @string
(number) @number
[(true) (false) (null)] @constant.builtin
(pair key: (string) @property)

["[" "]" "{" "}"] @punctuation.bracket
)QUERY";

}  // namespace syntax_queries
}  // namespace codecore
