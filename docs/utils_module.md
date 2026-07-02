# Utils Module

A set of useful utilities

Methods:

| Name      | Description                                      |
| --------- | ------------------------------------------------ |
| base64dec | Decodes a base64 string to a new string          |
| base64enc | Encodes data in a string to a base64 string      |
| trim      | Trims leading and trailing whitespace characters |


### base64dec()

Decodes a base64 string and returns the result as a string

Arguments:

- Base64 data to decode [string]

Results:

- Result string [string]

Example:

```lua
local app = ...
print(app.utils.base64dec("SGVsbG8gd29ybGQh"))
```

```sh
$ ./thoth script.lua
Hello world!
```

### base64enc()

Encodes data in the passed string to a base64 string.

Arguments:

- Data to encode [string]

Results:

- Base64 string [string]

Example:

```lua
local app = ...
print(app.utils.base64enc("Hello world!"))
```

```sh
$ ./thoth script.lua
SGVsbG8gd29ybGQh
```

### trim()

Removes leading and trailing whitespace characters from the passed string

Arguments:

- String to trim [string]

Results:

- Result string [string]

Example:

```lua
local app = ...
print(app.utils.trim("                  Hello!"))
```

```sh
$ ./thoth script.lua
Hello!
```
