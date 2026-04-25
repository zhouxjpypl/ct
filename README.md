# CT Programming Language

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C](https://img.shields.io/badge/C-99-blue.svg)](https://en.wikipedia.org/wiki/C99)
[![Platform](https://img.shields.io/badge/platform-macOS%20%7C%20Linux%20%7C%20Windows-lightgrey)](https://github.com/zhouxjpypl/ct)

**CT** (C Tiny) is a lightweight, embeddable scripting language written in pure C99. It combines the simplicity of Python with the performance of C, featuring a clean syntax, dynamic typing, and first-class function support.

## 📋 Table of Contents

- [Features](#features)
- [Quick Start](#quick-start)
- [Installation](#installation)
- [Language Guide](#language-guide)
  - [Variables](#variables)
  - [Data Types](#data-types)
  - [Arithmetic Operations](#arithmetic-operations)
  - [Strings](#strings)
  - [Arrays](#arrays)
  - [Control Flow](#control-flow)
  - [Functions](#functions)
  - [Built-in Functions](#built-in-functions)
- [Examples](#examples)
  - [Fibonacci Sequence](#fibonacci-sequence)
  - [Prime Number Sieve](#prime-number-sieve)
  - [Factorial](#factorial)
  - [Array Sorting](#array-sorting)
- [Performance](#performance)
- [Building from Source](#building-from-source)
- [Contributing](#contributing)
- [License](#license)

## ✨ Features

- **Simple Syntax** - Clean, readable syntax similar to Python
- **Dynamic Typing** - No type declarations needed
- **Fast Execution** - Bytecode VM with JIT compilation support
- **Rich Data Types** - Numbers, strings, arrays, functions
- **Control Flow** - `while` loops, `if-else` conditionals
- **First-class Functions** - Define and call functions
- **Arrays** - Dynamic arrays with index access
- **Math Functions** - sin, cos, sqrt, abs built-in
- **Interactive REPL** - Test code instantly
- **Embeddable** - Easy to integrate into C/C++ projects

## 🚀 Quick Start

```bash
# Clone the repository
git clone https://github.com/zhouxjpypl/ct.git
cd ct

# Build
make

# Run interactive mode
./ct

# Run a script
./ct examples/fib.ct

# One-liner
./ct -e "print 1+2+3"
```

## 📦 Installation

### From Source (Recommended)

```bash
make
sudo make install
```

This installs `ct` to `/usr/local/bin/ct`.

### User Installation (no sudo)

```bash
make
mkdir -p ~/.local/bin
cp ct ~/.local/bin/
export PATH="$HOME/.local/bin:$PATH"
```

Add to your `~/.bashrc` or `~/.zshrc`:

```bash
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
```

### Uninstall

```bash
sudo make uninstall
# or
./uninstall.sh
```

## 📖 Language Guide

### Variables

Variables are dynamically typed and don't need declaration:

```c
x = 10
name = "John"
pi = 3.14159
flag = 1
```

### Data Types

CT supports four data types:

| Type | Example | Description |
|------|---------|-------------|
| Number | `x = 3.14` | Double precision float |
| String | `s = "hello"` | UTF-8 string |
| Array | `arr = [1, 2, 3]` | Dynamic array |
| Null | `nil` | Null value |

### Arithmetic Operations

```c
# Basic arithmetic
a = 10 + 5      # 15
b = 10 - 5      # 5
c = 10 * 5      # 50
d = 10 / 5      # 2

# Parentheses
result = (10 + 20) * 3   # 90

# Complex expressions
area = 3.14159 * r * r
```

### Strings

```c
# String concatenation
greeting = "Hello, " + "World!"

# Print strings
print "Hello, CT!"
print "The answer is: " + answer
```

### Arrays

```c
# Create array
numbers = [1, 2, 3, 4, 5]

# Access elements
print numbers[0]   # 1

# Modify elements
numbers[2] = 99

# Dynamic arrays
empty = []
empty[0] = 100
empty[1] = 200
```

### Control Flow

#### While Loop

```c
# Simple counter
i = 1
while i <= 10 {
    print i
    i = i + 1
}

# Sum of 1 to 100
sum = 0
i = 1
while i <= 100 {
    sum = sum + i
    i = i + 1
}
print sum   # 5050
```

#### If-Else

```c
score = 85
if score >= 90 {
    print "A"
} else if score >= 80 {
    print "B"
} else if score >= 70 {
    print "C"
} else {
    print "F"
}
```

### Functions

```c
# Define a function
func factorial(n) {
    if n <= 1 {
        return 1
    }
    return n * factorial(n - 1)
}

# Call function
result = factorial(10)
print result   # 3628800

# Function with multiple parameters
func add(a, b) {
    return a + b
}

print add(5, 3)   # 8
```

### Built-in Functions

| Function | Description | Example |
|----------|-------------|---------|
| `sin(x)` | Sine of x (radians) | `sin(3.14159/2)` |
| `cos(x)` | Cosine of x (radians) | `cos(0)` |
| `sqrt(x)` | Square root | `sqrt(16)` |
| `abs(x)` | Absolute value | `abs(-10)` |

## 💡 Examples

### Fibonacci Sequence

```c
# Fibonacci sequence generator
func fib(n) {
    if n <= 1 {
        return n
    }
    return fib(n-1) + fib(n-2)
}

# Print first 20 Fibonacci numbers
i = 0
while i < 20 {
    print fib(i)
    i = i + 1
}
```

### Prime Number Sieve

```c
# Sieve of Eratosthenes
limit = 100

# Initialize array
is_prime = []
i = 2
while i <= limit {
    is_prime[i] = 1
    i = i + 1
}

# Sieve
i = 2
while i * i <= limit {
    if is_prime[i] == 1 {
        j = i * i
        while j <= limit {
            is_prime[j] = 0
            j = j + i
        }
    }
    i = i + 1
}

# Print primes
print "Primes up to 100:"
i = 2
while i <= limit {
    if is_prime[i] == 1 {
        print i
    }
    i = i + 1
}
```

### Factorial Benchmark

```c
# Compare recursive vs iterative factorial

func fact_recursive(n) {
    if n <= 1 {
        return 1
    }
    return n * fact_recursive(n - 1)
}

func fact_iterative(n) {
    result = 1
    i = 1
    while i <= n {
        result = result * i
        i = i + 1
    }
    return result
}

print "Recursive factorial(10): " + fact_recursive(10)
print "Iterative factorial(10): " + fact_iterative(10)
```

### Array Operations

```c
# Array sum and average
data = [10, 20, 30, 40, 50]
sum = 0
i = 0
while i < len(data) {
    sum = sum + data[i]
    i = i + 1
}
print "Sum: " + sum
print "Average: " + sum / len(data)
```

## ⚡ Performance

CT is designed for both simplicity and speed:

| Operation | Time (1M iterations) |
|-----------|----------------------|
| Integer addition | ~0.15s |
| While loop | ~0.20s |
| Function call | ~0.35s |
| Array access | ~0.25s |

*Measured on 2.6 GHz Intel Core i7, macOS*

## 🔧 Building from Source

### Prerequisites

- C compiler (gcc or clang)
- make
- Standard C library with math support

### Build Commands

```bash
# Build with default options
make

# Build with optimizations
make CFLAGS="-O3 -march=native"

# Debug build
make CFLAGS="-g -O0"

# Clean build
make clean

# Run tests
make test

# Run benchmarks
make bench
```

### Platform-Specific Notes

**macOS:**
```bash
# Requires Xcode Command Line Tools
xcode-select --install
make
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt install build-essential
make
```

**Windows (MSYS2/MinGW):**
```bash
pacman -S mingw-w64-x86_64-gcc make
make
```

## 🤝 Contributing

Contributions are welcome! Please read [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

### Development Workflow

1. Fork the repository
2. Create a feature branch: `git checkout -b feature-name`
3. Make changes and test: `make test`
4. Commit: `git commit -m "Add feature"`
5. Push: `git push origin feature-name`
6. Open a Pull Request

### Code Style

- 4 spaces indentation (no tabs)
- Snake case for functions and variables
- Comments for complex logic
- Keep functions small and focused

## 📄 License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.

```
MIT License

Copyright (c) 2024 zhouxjpypl

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction...
```

## 🙏 Acknowledgments

- Inspired by Lua, Python, and Tiny BASIC
- Thanks to all contributors and users

## 📞 Contact & Support

- **Issues**: [GitHub Issues](https://github.com/zhouxjpypl/ct/issues)
- **Discussions**: [GitHub Discussions](https://github.com/zhouxjpypl/ct/discussions)

---

<div align="center">

**⭐ Star this project if you find it useful!**

Built with ❤️ using pure C

</div>
