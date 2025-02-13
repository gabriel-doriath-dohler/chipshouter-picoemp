{ pkgs ? import <nixpkgs> { } }:
let sdk = (pkgs.pico-sdk.override { withSubmodules = true; });
in
pkgs.mkShell {
  buildInputs = with pkgs; [
    cmake
    gcc-arm-embedded-8
    sdk
  ];

  shellHook = ''
    export PICO_COMPILER=${pkgs.gcc-arm-embedded-8}
    export PICO_SDK_PATH=${sdk}
  '';
}
