{
  description = "speak5220 - TMS5220 speech synthesiser hardware";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
  };

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});
    in
    {
      devShells = forAllSystems (pkgs:
        let
          # Pico SDK with its submodules (TinyUSB is needed for USB stdio)
          pico-sdk = pkgs.pico-sdk.override { withSubmodules = true; };

          # Capture and analysis tools (tools/)
          python = pkgs.python3.withPackages (p: [
            p.numpy
            p.scipy
            p.matplotlib
            p.pyserial
            p.soundfile
          ]);
        in
        {
          default = pkgs.mkShell {
            packages = with pkgs; [
              # Hardware design
              kicad
              python
              git

              # Text to speech for the encoder's test inputs (tools/ttsgen.py)
              piper-tts
              # Speech recognition, for the intelligibility check (tools/asrcheck.py)
              whisper-cpp

              # Raspberry Pi Pico 2 (RP2350) firmware
              cmake
              ninja
              gcc-arm-embedded
              picotool
              openocd-rp2040 # Raspberry Pi's OpenOCD fork (supports RP2350)
              minicom
            ];

            PICO_SDK_PATH = "${pico-sdk}/lib/pico-sdk";
            picotool_DIR = "${pkgs.picotool}/lib/cmake/picotool";

            # Runtime dependencies of PicoScope 7's libps3000a.so, preloaded by
            # tools/ps3000a.py (the driver itself comes from the PicoScope install)
            PICO_DRIVER_DEPS = "${pkgs.libusb1}/lib/libusb-1.0.so.0:${pkgs.stdenv.cc.cc.lib}/lib/libstdc++.so.6";

            shellHook = ''
              if [ ! -e kicad/libraries/ki-lime-pi-pico/MCU_Module_RaspberryPi_Pico.kicad_sym ]; then
                echo "Note: ki-lime-pi-pico submodule missing; run: git submodule update --init"
              fi
            '';
          };
        });

      formatter = forAllSystems (pkgs: pkgs.nixfmt-rfc-style);
    };
}
