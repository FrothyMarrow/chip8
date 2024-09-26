{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";
  };
  outputs = {nixpkgs, ...}: let
    system = "aarch64-darwin";
    pkgs = import nixpkgs {
      inherit system;
    };
  in {
    packages.${system}.default = pkgs.stdenv.mkDerivation {
      name = "chip8";
      src = ./src;

      buildInputs = [pkgs.SDL2];

      buildPhase = ''
        CC -o chip8 chip8.c `sdl2-config --cflags --libs`
      '';

      installPhase = ''
        mkdir -p $out/bin
        cp chip8 $out/bin
      '';
    };

    devShell.${system} = pkgs.mkShell {
      packages = [
        pkgs.SDL2
        pkgs.clang-tools
      ];
    };
  };
}
