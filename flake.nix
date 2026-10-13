# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
{
  description = "speak5220 - TMS5220 speech synthesiser hardware";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
    # Pre-commit hooks for the application (docs-tech/application-plan.md §1.3)
    git-hooks = {
      url = "github:cachix/git-hooks.nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs =
    {
      self,
      nixpkgs,
      git-hooks,
    }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "aarch64-darwin"
      ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});

      # The Speak5220 application (app/): its dependencies, package, linters
      # and pre-commit hooks
      appFor =
        pkgs:
        let
          inherit (pkgs) lib;
          system = pkgs.stdenv.hostPlatform.system;

          # The version is the git commit hash, with "-dirty" for uncommitted
          # changes, as app/cmake/GitVersion.cmake finds it outside Nix
          version = self.shortRev or self.dirtyShortRev or "unknown";

          # pocketfft's header-only C++ version, which nixpkgs does not package
          pocketfft = pkgs.stdenvNoCC.mkDerivation {
            pname = "pocketfft";
            version = "0-unstable-2026-06-30";
            src = pkgs.fetchFromGitHub {
              owner = "mreineck";
              repo = "pocketfft";
              rev = "c90e55b3d529f8efa40ed01a20de22405f45fc65";
              hash = "sha256-K3yvh6i5f9VbAiQPyixgnlDCj4eBWPU8m3BNn+fScdM=";
            };
            dontConfigure = true;
            dontBuild = true;
            installPhase = ''
              install -Dm644 pocketfft_hdronly.h $out/include/pocketfft_hdronly.h
              install -Dm644 LICENSE.md $out/share/licenses/pocketfft/LICENSE.md
            '';
            meta = {
              description = "Header-only C++ FFT library";
              homepage = "https://github.com/mreineck/pocketfft";
              license = lib.licenses.bsd3;
            };
          };

          # libfvad, the WebRTC voice activity detector as a C library, which
          # nixpkgs does not package
          libfvad = pkgs.stdenv.mkDerivation {
            pname = "libfvad";
            version = "1.0.1-unstable-2024-02-19";
            src = pkgs.fetchFromGitHub {
              owner = "dpirch";
              repo = "libfvad";
              rev = "532ab666c20d3cfda38bca63abbb0f152706c369";
              hash = "sha256-TS/2ywq88AAPJEfYD5b0H7tzpAoigynsctzZodvPFzU=";
            };
            nativeBuildInputs = [ pkgs.cmake ];
            postInstall = ''
              install -Dm644 ../LICENSE $out/share/licenses/libfvad/LICENSE
              install -Dm644 ../PATENTS $out/share/licenses/libfvad/PATENTS
            '';
            meta = {
              description = "Voice activity detection library, from the WebRTC code";
              homepage = "https://github.com/dpirch/libfvad";
              license = lib.licenses.bsd3;
            };
          };

          # SPTK compiled as the application is, without fused multiply-add
          # contraction (app/cmake/CompilerWarnings.cmake), so its REAPER
          # and Harvest give the same pitch tracks on every platform
          sptk = pkgs.sptk.overrideAttrs (old: {
            env = (old.env or { }) // {
              NIX_CFLAGS_COMPILE = toString [
                (old.env.NIX_CFLAGS_COMPILE or "")
                "-ffp-contract=off"
              ];
            };
          });

          # Libraries the application links (plan §2.4). SPTK 4 brings its
          # own copies of REAPER and WORLD (HLD §10).
          libraries = [
            pkgs.qt6.qtbase
            pkgs.qt6.qtmultimedia
          ]
          ++ lib.optional pkgs.stdenv.hostPlatform.isLinux pkgs.qt6.qtwayland
          ++ [
            pkgs.libsndfile
            pkgs.libsamplerate
            pocketfft
            pkgs.libzip
            pkgs.yaml-cpp
            pkgs.spdlog
            libfvad
            sptk
          ];

          # Format and lint tools (plan §1.3)
          linters = [
            pkgs.clang-tools
            pkgs.cpplint
            pkgs.ruff
            pkgs.shellcheck
            pkgs.nixfmt
            pkgs.gersemi
            pkgs.actionlint
          ];

          speak5220 = pkgs.stdenv.mkDerivation (finalAttrs: {
            pname = "speak5220";
            inherit version;
            src = lib.fileset.toSource {
              root = ./app;
              fileset = ./app;
            };

            nativeBuildInputs = [
              pkgs.cmake
              pkgs.ninja
              pkgs.pkg-config
              pkgs.qt6.wrapQtAppsHook
            ];
            buildInputs = libraries;
            checkInputs = [ pkgs.gtest ];

            # app/CMakeLists.txt refuses a build directory inside the source
            # tree, so build next to it rather than in source/build
            cmakeBuildDir = "../build";
            cmakeDir = "../source";

            # The sources carry no .git, so the version is passed in
            cmakeFlags = [
              (lib.cmakeBool "SPEAK5220_BUILD_TESTS" finalAttrs.doCheck)
              (lib.cmakeFeature "SPEAK5220_GIT_VERSION" version)
            ];

            # The packaged build runs the fast, mocked unit lane only (TESTING.md)
            doCheck = true;
            checkPhase = ''
              runHook preCheck
              # Somewhere writable for the offscreen GUI tests' font cache
              export HOME=$(mktemp -d)
              ctest -L unit --output-on-failure
              runHook postCheck
            '';

            # On macOS, wrapQtAppsHook's search for programs to wrap
            # intermittently crashes bash in the build sandbox, so the two
            # programs are wrapped by name instead
            dontWrapQtApps = pkgs.stdenv.hostPlatform.isDarwin;
            postFixup = lib.optionalString pkgs.stdenv.hostPlatform.isDarwin ''
              wrapQtApp $out/bin/speak5220-cli
              wrapQtApp $out/speak5220.app/Contents/MacOS/speak5220
            '';

            meta = {
              description = "Creates TMS5220 LPC speech data and builds PHROMs";
              homepage = "https://github.com/simoninns/speak5220";
              license = lib.licenses.gpl3Plus;
              mainProgram = "speak5220";
              platforms = lib.platforms.linux ++ lib.platforms.darwin;
            };
          });

          # A hook that runs a command with its tools on PATH, so the hooks
          # also work from a git client started outside the dev shell.
          # Commands are run from the repository root.
          toolHook =
            {
              name,
              command,
              files,
              tools,
              extra ? { },
            }:
            let
              # Repository scripts run through bash: the flake check's
              # sandbox has no /usr/bin/env for their shebang lines
              runner = lib.optionalString (lib.hasPrefix "app/scripts/" command) "${pkgs.bash}/bin/bash ";
            in
            {
              enable = true;
              inherit name files;
              entry = toString (
                pkgs.writeShellScript "hook-${name}" ''
                  export PATH=${lib.makeBinPath tools}:$PATH
                  exec ${runner}${command} "$@"
                ''
              );
              extraPackages = tools;
              # Hooks that share a built-in hook's name (clang-format,
              # clang-tidy) would otherwise bring the built-in's package, a
              # second LLVM version, into the dev shell
              package = lib.mkForce (if tools == [ ] then null else builtins.head tools);
            }
            // extra;

          cppFiles = "^app/.*\\.(cpp|h)$";

          # The hooks of plan §1.3. clang-tidy needs a configured build
          # directory, so it runs in the dev shell's hooks and in CI's build
          # job, but not in the sandboxed `nix flake check`.
          hooks =
            { withClangTidy }:
            {
              clang-format = toolHook {
                name = "clang-format";
                command = "app/scripts/clang-format-check.sh";
                files = cppFiles;
                tools = [ pkgs.clang-tools ];
              };
              cpplint = toolHook {
                name = "cpplint";
                command = "cpplint --quiet";
                files = cppFiles;
                tools = [ pkgs.cpplint ];
              };
              check-spdx = toolHook {
                name = "check-spdx";
                command = "app/scripts/check-spdx.sh";
                files = "^(app/|flake\\.nix$|\\.github/workflows/)";
                tools = [ ];
              };
              check-header-guards = toolHook {
                name = "check-header-guards";
                command = "app/scripts/check-header-guards.sh";
                files = "^app/.*\\.h$";
                tools = [ ];
              };
              ruff = toolHook {
                name = "ruff";
                command = "ruff check";
                files = "^app/.*\\.py$";
                tools = [ pkgs.ruff ];
              };
              ruff-format = toolHook {
                name = "ruff-format";
                command = "ruff format --check";
                files = "^app/.*\\.py$";
                tools = [ pkgs.ruff ];
              };
              shellcheck = toolHook {
                name = "shellcheck";
                command = "shellcheck";
                files = "^app/.*\\.sh$";
                tools = [ pkgs.shellcheck ];
              };
              nixfmt = toolHook {
                name = "nixfmt";
                command = "nixfmt --check";
                files = "^(flake\\.nix|app/.*\\.nix)$";
                tools = [ pkgs.nixfmt ];
              };
              gersemi = toolHook {
                name = "gersemi";
                command = "gersemi --check";
                files = "^app/(.*/)?(CMakeLists\\.txt|[^/]*\\.cmake)$";
                tools = [ pkgs.gersemi ];
              };
              actionlint = toolHook {
                name = "actionlint";
                command = "actionlint";
                files = "^\\.github/workflows/.*\\.ya?ml$";
                # actionlint checks the workflows' shell steps with shellcheck
                tools = [
                  pkgs.actionlint
                  pkgs.shellcheck
                ];
              };
            }
            // lib.optionalAttrs withClangTidy {
              clang-tidy = toolHook {
                name = "clang-tidy";
                command = "app/scripts/clang-tidy.sh";
                files = cppFiles;
                tools = [ pkgs.clang-tools ];
                # The script runs its files in parallel itself
                extra.require_serial = true;
              };
            };

          # The sources the hooks look at, so the check does not copy the
          # investigation captures into the store
          hookSrc = lib.fileset.toSource {
            root = ./.;
            fileset = lib.fileset.unions [
              ./app
              ./flake.nix
              ./flake.lock
              ./.github
            ];
          };

          shellHooks = git-hooks.lib.${system}.run {
            src = hookSrc;
            hooks = hooks { withClangTidy = true; };
          };
          checkHooks = git-hooks.lib.${system}.run {
            src = hookSrc;
            hooks = hooks { withClangTidy = false; };
          };

          devShell = pkgs.mkShell {
            packages = [
              pkgs.cmake
              pkgs.ninja
              pkgs.pkg-config
              pkgs.gtest
              pkgs.qt6.qttools
            ]
            ++ libraries
            ++ linters
            ++ shellHooks.enabledPackages;

            # Lets the GUI built in build/app find its Qt plugins
            QT_PLUGIN_PATH = lib.makeSearchPath pkgs.qt6.qtbase.qtPluginPrefix (
              [
                pkgs.qt6.qtbase
                pkgs.qt6.qtmultimedia
              ]
              ++ lib.optional pkgs.stdenv.hostPlatform.isLinux pkgs.qt6.qtwayland
            );

            # Installs the pre-commit hooks into .git/hooks
            shellHook = shellHooks.shellHook;
          };
        in
        {
          inherit
            speak5220
            pocketfft
            libfvad
            checkHooks
            devShell
            ;
        };
    in
    {
      packages = forAllSystems (
        pkgs:
        let
          app = appFor pkgs;
        in
        {
          inherit (app) speak5220 pocketfft libfvad;
          default = app.speak5220;
        }
      );

      checks = forAllSystems (pkgs: {
        pre-commit = (appFor pkgs).checkHooks;
      });

      devShells = forAllSystems (
        pkgs:
        let
          # Pico SDK with its submodules (TinyUSB is needed for USB stdio)
          pico-sdk = pkgs.pico-sdk.override { withSubmodules = true; };

          # Google's REAPER pitch tracker as its own command-line tool, for the
          # quality reference data (app/scripts/golden/quality.py); the
          # application uses SPTK's copy
          reaper-f0 = pkgs.stdenv.mkDerivation {
            pname = "reaper-f0";
            version = "0-unstable-2019-12-21";
            src = pkgs.fetchFromGitHub {
              owner = "google";
              repo = "REAPER";
              rev = "1d6e9b95e6b08b500fccbc9a043989dbda747276";
              hash = "sha256-sLvY7YOAcxslNJ0miW124iGxhwM0njU7DxAzP/jhi44=";
            };
            nativeBuildInputs = [ pkgs.cmake ];
            # Its CMakeLists.txt asks for a CMake older than current ones
            # accept, and has no install rule
            cmakeFlags = [ "-DCMAKE_POLICY_VERSION_MINIMUM=3.5" ];
            # Without fused multiply-add contraction, as SPTK above, so its
            # tracks are the same on every platform (Clang contracts on
            # arm64, which moves a frame of the reference tracks)
            env.NIX_CFLAGS_COMPILE = "-ffp-contract=off";
            installPhase = ''
              install -Dm755 reaper $out/bin/reaper
            '';
            meta = {
              description = "Google's REAPER epoch and pitch tracker";
              homepage = "https://github.com/google/REAPER";
              license = pkgs.lib.licenses.asl20;
              mainProgram = "reaper";
            };
          };

          # Capture and analysis tools (investigation/tools/), and the
          # reference tools of app/scripts/golden/
          python = pkgs.python3.withPackages (p: [
            p.numpy
            p.scipy
            p.matplotlib
            p.pyserial
            p.soundfile
            p.pesq # ITU-T P.862 speech quality (investigation/tools/speechqual.py)
            # The quality reference data (app/scripts/golden/quality.py): the
            # original WebRTC voice activity detector, ESTOI and WORLD's
            # Harvest
            p.webrtcvad
            (p.buildPythonPackage rec {
              pname = "pystoi";
              version = "0.4.1";
              pyproject = true;
              src = p.fetchPypi {
                inherit pname version;
                hash = "sha256-HG9Q1vv+5GsAySJFjNvScijZgwyoHOp4j9YA/C995uQ=";
              };
              build-system = [ p.setuptools ];
              dependencies = [
                p.numpy
                p.scipy
              ];
              meta.license = pkgs.lib.licenses.mit;
            })
            p.pyworld
            # The documentation site (mkdocs.yml, docs/)
            p.mkdocs
            p.mkdocs-material
          ]);

          # The application's tools and libraries alone (used by CI)
          app = (appFor pkgs).devShell;
        in
        {
          inherit app;

          default = pkgs.mkShell {
            # Everything in the application's shell, its hooks included
            inputsFrom = [ app ];
            inherit (app) QT_PLUGIN_PATH;

            packages =
              with pkgs;
              [
                python
                reaper-f0
                git

                # Speech recognition, for the intelligibility check (investigation/tools/asrcheck.py)
                whisper-cpp

                # Raspberry Pi Pico 2 (RP2350) firmware
                cmake
                ninja
                gcc-arm-embedded
                picotool
                openocd-rp2040 # Raspberry Pi's OpenOCD fork (supports RP2350)
                minicom
              ]
              # Not available on macOS in nixpkgs (install KiCad separately there)
              ++ lib.optionals stdenv.hostPlatform.isLinux [
                # Hardware design
                kicad
                # Text to speech for the encoder's test inputs (investigation/tools/ttsgen.py)
                piper-tts
              ];

            PICO_SDK_PATH = "${pico-sdk}/lib/pico-sdk";
            picotool_DIR = "${pkgs.picotool}/lib/cmake/picotool";

            # Runtime dependencies of PicoScope 7's libps3000a.so, preloaded by
            # investigation/tools/ps3000a.py (the driver itself comes from the PicoScope install)
            # (Linux only)
            PICO_DRIVER_DEPS = pkgs.lib.optionalString pkgs.stdenv.hostPlatform.isLinux "${pkgs.libusb1}/lib/libusb-1.0.so.0:${pkgs.stdenv.cc.cc.lib}/lib/libstdc++.so.6";

            shellHook = ''
              if [ ! -e investigation/kicad/libraries/ki-lime-pi-pico/MCU_Module_RaspberryPi_Pico.kicad_sym ]; then
                echo "Note: ki-lime-pi-pico submodule missing; run: git submodule update --init"
              fi
            '';
          };
        }
      );

      formatter = forAllSystems (pkgs: pkgs.nixfmt);
    };
}
