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
      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          packages = with pkgs; [
            kicad
            python3
            git
          ];

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
