<a id="top"></a>
# Clara (Command Line Parser)

**Contents**<br>
[Overview](#overview)<br>
[Basic Usage](#basic-usage)<br>
[Options (`Opt`)](#options-opt)<br>
[Positional Arguments (`Arg`)](#positional-arguments-arg)<br>
[Executable Name (`ExeName`)](#executable-name-exename)<br>
[Help Option (`Help`)](#help-option-help)<br>
[Composing Parsers](#composing-parsers)<br>
[Extending Catch2's Command Line](#extending-catch2s-command-line)<br>
[Standalone Parsing](#standalone-parsing)<br>
[Automatic Help Output](#automatic-help-output)<br>


## Overview

Catch2 uses an embedded command line parser library called **Clara**. Clara was
originally developed by Phil Nash as a separate standalone single-header library,
but was later integrated directly into Catch2's codebase and evolved alongside it.

Clara is accessible under the `Catch::Clara` namespace when including
`<catch2/catch_session.hpp>`.

Key characteristics of Clara:
* **Composable**: Individual options and positional arguments are independent
  parser objects combined using the pipe (`|`) operator.
* **Direct binding**: Options bind directly to variables (primitives, strings,
  containers, or lambdas) that receive the parsed values — without needing intermediate
  dictionary lookups.
* **Type deduced**: Type conversions are handled automatically based on the bound
  variable type, with built-in error checking.
* **Self-documenting**: Option names, hints, and descriptions defined in code are
  used to automatically generate formatted usage/help output.


## Basic Usage

A parser for a single option is created by specifying the bound variable, a hint
string, the flag names in square brackets `[]`, and a description in parentheses `()`:

```cpp
#include <catch2/catch_session.hpp>
#include <iostream>

using namespace Catch::Clara;

int width = 0;
auto cli = Opt( width, "width" )
             ["-w"]["--width"]
             ("how wide should it be?");
```

Multiple options and positional arguments are combined using `operator|`:

```cpp
int width = 0;
std::string outputName;
bool verbose = false;

auto cli = Opt( width, "width" )
             ["-w"]["--width"]
             ("output width in pixels")
         | Opt( outputName, "filename" )
             ["-o"]["--output"]
             ("destination file")
         | Opt( verbose )
             ["-v"]["--verbose"]
             ("enable verbose output");
```


## Options (`Opt`)

`Opt` represents a named command-line flag or parameter. On POSIX systems, options
start with `-` (short) or `--` (long). On Windows, forward slashes `/` are also
accepted and interpreted as option switches.

### Boolean flags

For pure toggle flags that take no argument, bind directly to a `bool`:

```cpp
bool debugMode = false;
auto opt = Opt( debugMode )
             ["-d"]["--debug"]
             ("run in debug mode");
```

When specified on the command line (e.g. `--debug` or `-d`), `debugMode` is set to `true`.

### Value-taking options

Options that take an argument accept a second parameter in the `Opt` constructor:
a **hint string** displayed in help text.

```cpp
int timeoutSeconds = 30;
auto opt = Opt( timeoutSeconds, "seconds" )
             ["-t"]["--timeout"]
             ("operation timeout in seconds");
```

Values can be supplied with space separation (`--timeout 60`) or using an equals sign
or colon (`--timeout=60`, `-t:60`).

Clara deduces the target type from the bound variable and converts the input string
automatically (supporting integers, floating-point numbers, strings, and any type with
an appropriate stream extraction operator).

### Multi-value options

If an option can be specified multiple times, bind it to an `std::vector<T>`:

```cpp
std::vector<std::string> includePaths;
auto opt = Opt( includePaths, "dir" )
             ["-I"]["--include"]
             ("directory to add to the include search path");
```

Each appearance on the command line appends a new element to the vector:
`-I /usr/include -I /opt/local/include` results in two elements.

### Lambda bindings and custom validation

For custom validation, side effects, or complex conversions, pass a unary lambda
instead of a variable.

The lambda receives the parsed argument string and returns a `ParserResult`:

```cpp
int port = 8080;

auto opt = Opt( [&]( std::string const& arg ) {
    int p = std::stoi( arg );
    if ( p < 1 || p > 65535 ) {
        return ParserResult::runtimeError( "Port must be between 1 and 65535" );
    }
    port = p;
    return ParserResult::ok( ParseResultType::Matched );
}, "port" )
    ["-p"]["--port"]
    ("port number to listen on (1-65535)");
```

If a lambda-bound option should accept multiple occurrences, pass `accept_many` as the
first parameter:

```cpp
std::vector<std::string> tags;

auto opt = Opt( accept_many, [&]( std::string const& tag ) {
    tags.push_back( tag );
    return ParserResult::ok( ParseResultType::Matched );
}, "tag" )
    ["--tag"]
    ("tag to filter by");
```

### Required vs. Optional

By default, all options are optional. You can explicitly mark an option as required:

```cpp
std::string configPath;
auto opt = Opt( configPath, "path" )
             ["-c"]["--config"]
             ("path to configuration file")
             .required();
```

If a required option is omitted on the command line, parsing will fail with an
appropriate error message.


## Positional Arguments (`Arg`)

`Arg` represents a positional argument that is not preceded by a flag or option name.

```cpp
std::string inputFile;
auto arg = Arg( inputFile, "input-file" )
             ("file to process");
```

Positional arguments can also be bound to `std::vector<std::string>` to collect all
remaining positional arguments:

```cpp
std::vector<std::string> inputFiles;
auto args = Arg( inputFiles, "files..." )
              ("one or more input files to process");
```

Positional arguments also support `.required()` and `.optional()`.


## Executable Name (`ExeName`)

`ExeName` allows capturing or specifying the process/executable name:

```cpp
std::string processName;
auto exe = ExeName( processName );
```


## Help Option (`Help`)

`Help` is a convenience wrapper for standard help flags (`-h`, `--help`, `-?`).
It binds to a `bool` flag indicating whether help was requested:

```cpp
bool showHelp = false;
auto help = Help( showHelp );
```


## Composing Parsers

Individual `Opt`, `Arg`, `Help`, and `ExeName` instances are combined using `operator|`
to form a complete `Parser`:

```cpp
using namespace Catch::Clara;

bool showHelp = false;
bool verbose = false;
int threads = 1;
std::string outputDir;
std::vector<std::string> inputFiles;

auto cli
    = Help( showHelp )
    | Opt( verbose )
        ["-v"]["--verbose"]
        ("enable verbose logging")
    | Opt( threads, "count" )
        ["-j"]["--threads"]
        ("number of worker threads")
    | Opt( outputDir, "path" )
        ["-o"]["--out-dir"]
        ("output directory")
    | Arg( inputFiles, "inputs..." )
        ("input files to process");
```


## Extending Catch2's Command Line

The most common reason to use Clara directly with Catch2 is to add custom command-line
flags to test binaries when [supplying your own main()](own-main.md#top).

Catch2's `Session` class exposes its internal Clara parser via `session.cli()`. You
can extend it by piping your custom options into it, and then passing the composed
parser back to the session:

```cpp
#include <catch2/catch_session.hpp>
#include <iostream>

int main( int argc, char* argv[] ) {
    Catch::Session session;

    // Define custom variables to populate from the CLI
    int customSeed = 0;
    std::string testEnv = "staging";

    // Compose custom options on top of Catch2's default parser
    using namespace Catch::Clara;
    auto cli
        = session.cli()
        | Opt( customSeed, "seed" )
            ["--custom-seed"]
            ("random seed for custom generators")
        | Opt( testEnv, "env" )
            ["--env"]
            ("target environment (staging, production, local)");

    // Pass the composite parser back to the session
    session.cli( cli );

    // Let Catch2 parse the combined command line
    int returnCode = session.applyCommandLine( argc, argv );
    if ( returnCode != 0 ) {
        return returnCode;
    }

    // Custom variables are now populated
    std::cout << "Running tests against environment: " << testEnv << '\n';

    return session.run();
}
```


## Standalone Parsing

Clara can also be used as a standalone parser independent of Catch2's test runner
using the `Args` class:

```cpp
using namespace Catch::Clara;

auto cli = /* ... define options ... */;

auto result = cli.parse( Args( argc, argv ) );
if ( !result ) {
    std::cerr << "Command line error: " << result.errorMessage() << std::endl;
    return 1;
}
```


## Automatic Help Output

Streaming a `Parser` into any `std::ostream` (such as `std::cout`) automatically
prints a neatly formatted, column-aligned help page formatted for the console width:

```cpp
if ( showHelp ) {
    std::cout << cli << std::endl;
    return 0;
}
```

This output displays all option names, parameter hints, and description strings
automatically wrapped and indented.


---

[Home](Readme.md#top)
