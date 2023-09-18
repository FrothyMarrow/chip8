# Chip-8 Emulator

A simple Chip-8 emulator made using SDL2.
<img width="1392" alt="Screenshot 2023-09-18 at 3 34 43 PM" src="https://github.com/FrothyMarrow/chip8/assets/76622149/51cb1547-bd68-4804-a09f-ff07399008c4">

## Requirements

Requires SDL2. If you are using homebrew you can install it with the following:

```
$ brew install sdl2
```

Nix users can kindly ignore this.

## Building

Non-nix users:

```
$ clang -o chip8 src/chip8.c `sdl2-config --cflags --libs`
```

Nix users:

```
$ nix build
```

This will generate a `result` symlink pointing to the nix-store with the binary.

> Note: Requires experimental feature [Flakes](https://nixos.wiki/wiki/Flakes).

## Usage

```
$ ./chip8 path/to/rom
```

## Resources

-   [Cowgod's Chip-8 Technical Reference](http://devernay.free.fr/hacks/chip8/C8TECH10.HTM)
-   [Chip-8 Wikipedia](https://en.wikipedia.org/wiki/CHIP-8)
