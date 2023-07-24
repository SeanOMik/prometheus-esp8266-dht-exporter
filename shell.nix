{ pkgs ? import <nixpkgs> { } }:

with pkgs;

mkShell rec {
  nativeBuildInputs = [
    platformio
    python39
    xdg-user-dirs
  ];
  buildInputs = [
    
  ];
  LD_LIBRARY_PATH = lib.makeLibraryPath buildInputs;
}