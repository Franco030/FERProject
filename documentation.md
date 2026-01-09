# Fer Standard Library Documentation

This document outlines the standard library functions available in the Fer programming language. These functions are implemented natively for performance and provide essential utilities for string manipulation, mathematics, collection management, input/output, and system operations.

## 1. String & General Utilities

### `len(container)`

Returns the length or count of elements in a container.

* **Parameters:** `container` (String | List | Dictionary)
* **Returns:** Number (Integer).
* **Edge Cases:** Returns `nil` if the argument is not a supported container type.

### `str(value)`

Converts a value to its string representation.

* **Parameters:** `value` (Any)
* **Returns:** String.
* **Supported Types:** Booleans ("true"/"false"), Nil ("nil"), Numbers (formatted as `%g`), and Strings (returns the input unchanged).

### `sub(string, start, [length])`

Extracts a substring starting at a specific index.

* **Parameters:**
* `string`: The source string.
* `start`: The zero-based starting index (Number).
* `length` (Optional): The number of characters to extract. If omitted, extracts until the end of the string.


* **Returns:** String.
* **Edge Cases:** Returns an empty string `""` if the `start` index is out of bounds or if `length` is non-positive.

### `upper(string)`

Converts a string to uppercase.

* **Parameters:** `string` (String)
* **Returns:** String.

### `lower(string)`

Converts a string to lowercase.

* **Parameters:** `string` (String)
* **Returns:** String.

### `index(haystack, needle)`

Finds the first occurrence of a substring.

* **Parameters:**
* `haystack`: The string to search within.
* `needle`: The substring to search for.


* **Returns:** Number (Index of the first occurrence) or `-1` if not found.

### `split(string, delimiter)`

Splits a string into a list of substrings based on a delimiter.

* **Parameters:**
* `string`: The string to split.
* `delimiter`: The character(s) to use as a separator.


* **Returns:** List of strings.

### `trim(string)`

Removes whitespace characters from the beginning and end of a string.

* **Parameters:** `string` (String)
* **Returns:** String.
* **Edge Cases:** Returns the original string if no whitespace is present. Returns an empty string if the input contains only whitespace.

### `chr(code)`

Converts a numeric ASCII code to a single-character string.

* **Parameters:** `code` (Number)
* **Returns:** String.

### `ord(character)`

Converts the first character of a string to its ASCII numeric value.

* **Parameters:** `character` (String)
* **Returns:** Number.
* **Edge Cases:** Returns `nil` if the string is empty.

---

## 2. Collections (Lists & Dictionaries)

### `push(list, item)`

Appends an item to the end of a list.

* **Parameters:**
* `list`: The target list.
* `item`: The value to append.


* **Returns:** The appended `item`.

### `pop(list)`

Removes and returns the last item from a list.

* **Parameters:** `list` (List)
* **Returns:** The removed item.
* **Edge Cases:** Returns `nil` if the list is empty.

### `insert(list, index, item)`

Inserts an item at a specific index in a list, shifting subsequent elements to the right.

* **Parameters:**
* `list`: The target list.
* `index`: The position to insert the item (Number).
* `item`: The value to insert.


* **Returns:** The inserted `item`.
* **Edge Cases:** Returns `nil` and performs no action if `index` is out of bounds (`index < 0` or `index > count`).

### `remove(list, index)`

Removes the item at the specified index from a list, shifting subsequent elements to the left.

* **Parameters:**
* `list`: The target list.
* `index`: The position to remove (Number).


* **Returns:** The removed item.
* **Edge Cases:** Returns `nil` if `index` is out of bounds.

### `contains(list, item)`

Checks if a list contains a specific value.

* **Parameters:**
* `list`: The list to search.
* `item`: The value to search for.


* **Returns:** Boolean (`true` if found, `false` otherwise).

### `keys(dictionary)`

Retrieves all keys from a dictionary.

* **Parameters:** `dictionary` (Dictionary)
* **Returns:** List of keys.

### `hasKey(dictionary, key)`

Checks if a dictionary contains a specific key.

* **Parameters:**
* `dictionary`: The target dictionary.
* `key`: The key to search for (String).


* **Returns:** Boolean.

### `delete(dictionary, key)`

Removes a key-value pair from a dictionary.

* **Parameters:**
* `dictionary`: The target dictionary.
* `key`: The key to remove (String).


* **Returns:** Boolean (`true` if the key existed and was deleted, `false` if the key was not found).

---

## 3. Mathematics

### `sqrt(number)`

Calculates the square root of a number.

* **Returns:** Number.

### `pow(base, exponent)`

Calculates the result of raising a base to an exponent.

* **Returns:** Number.

### `floor(number)`

Rounds a number down to the nearest integer.

* **Returns:** Number.

### `ceil(number)`

Rounds a number up to the nearest integer.

* **Returns:** Number.

### `rand()`

Generates a random floating-point number between 0.0 and 1.0.

* **Returns:** Number.

### `seed(number)`

Seeds the random number generator.

* **Parameters:** `number` (Number)
* **Returns:** `nil`.

### `sin(radians)` / `cos(radians)` / `tan(radians)`

Trigonometric functions calculating sine, cosine, and tangent respectively.

* **Parameters:** `radians` (Number)
* **Returns:** Number.

---

## 4. Input / Output (I/O)

### `input([prompt])`

Reads a line of text from standard input (stdin).

* **Parameters:** `prompt` (Optional): A string or value to print before waiting for input.
* **Returns:** String (The user input without the trailing newline) or `nil` on failure/EOF.

### `read(path)`

Reads the entire contents of a file.

* **Parameters:** `path` (String): Relative or absolute path to the file.
* **Returns:** String (File content) or `nil` if the file could not be read or does not exist.

### `write(path, content)`

Writes a string to a file, overwriting existing content.

* **Parameters:**
* `path` (String): Destination file path.
* `content` (String): The text to write.


* **Returns:** Boolean (`true` on success, `false` on failure).

### `exit([code])`

Terminates the program immediately.

* **Parameters:** `code` (Optional): Exit status code (Number). Defaults to 0.
* **Returns:** Does not return.

---

## 5. System & Types

### `clock()`

Returns the CPU time used by the program.

* **Returns:** Number (Seconds).

### `now()`

Returns the current system time (Unix timestamp).

* **Returns:** Number (Seconds since epoch).

### `typeof(value)`

Returns a string describing the data type of the value.

* **Parameters:** `value` (Any)
* **Returns:** String (e.g., "nil", "bool", "number", "string", "list", "dictionary", "function", "class", "instance").

### `assert(condition, [message])`

Aborts the program if the condition evaluates to `false` or `nil`.

* **Parameters:**
* `condition`: The expression to test.
* `message` (Optional): Custom error message (String).


* **Returns:** `true` if the assertion passes.
* **Effect:** Terminates execution with an error message if the assertion fails.