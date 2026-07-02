# Readline Module

The module adds support for the readline library. Implemented user input and auto-completion.

Methods:

| Name         | Description                                    |
| ------------ | ---------------------------------------------- |
| input        | Gets user input and returns it as a Lua string |
| setCompleter | Configures the auto-completion feature         |

### input()

Gets user input and returns it as a string.
It returns nil when gets EOL (user have pressed Ctrl+D, for example).

Arguments:

- Readline module object [userdata]
- Prompt [string|nil]

Results:

- User input [string|nil]

Example:

```lua
local app = ...
local name = app.readline:input("Enter your name: ")
print(string.format("Hello, %s!", name))
```

```sh
$ ./thoth script.lua
Enter your name: Bob
Hello, Bob!
```

### setCompleter()

Configures the auto-completion feature.

Arguments:

- Readline module object [userdata]
- Table with matches or handler. A nil disables the feature [table|function|nil]

The handler is called with the following arguments: text [string], start [number], end [number] and must return a table with matches.

Results:

none

Example:

```lua
local app = ...
app.readline:setCompleter({ "Bob", "Alice" })
local name = app.readline:input("Enter your name: ")
print(string.format("Hello, %s!", name))
```

```sh
$ ./thoth script.lua
Enter your name: 
Alice  Bob
```
