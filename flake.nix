{
  description = "C Template";

  inputs = {
    nixpkgs.url = "nixpkgs";
    systems.url = "github:nix-systems/x86_64-linux";
    flake-utils = {
      url = "github:numtide/flake-utils";
      inputs.systems.follows = "systems";
    };
  };

  outputs =
    { self
    , nixpkgs
    , flake-utils
    , ...
    }:
    # For more information about the C/C++ infrastructure in nixpkgs: https://nixos.wiki/wiki/C
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        pname = "hello-world"; #package name
        version = "0.0.1";
        src = ./.;
        buildInputs = with pkgs; [
          alsa-lib
          libsndfile
        ];
        nativeBuildInputs = with pkgs; [
          pkg-config
          xxd
          portaudio
        ];
      in
        {
          devShells.default = pkgs.mkShell {
            inherit buildInputs nativeBuildInputs;

            # You can use NIX_CFLAGS_COMPILE to set the default CFLAGS for the shell
            #NIX_CFLAGS_COMPILE = "-g";
            # You can use NIX_LDFLAGS to set the default linker flags for the shell
            #NIX_LDFLAGS = "-L${lib.getLib zstd}/lib -lzstd";
          };

          # Pinned gcc: remain on gcc10 even after `nix flake update`
          #default = pkgs.mkShell.override { stdenv = pkgs.gcc10Stdenv; } {
          #  inherit buildInputs nativeBuildInputs;
          #};

          # Clang example:
          #default = pkgs.mkShell.override { stdenv = pkgs.clangStdenv; } {
          #  inherit buildInputs nativeBuildInputs;
          #};

          packages.default = pkgs.stdenv.mkDerivation {
            inherit buildInputs nativeBuildInputs pname version src;
            dontConfigure = true;
            installPhase = ''
               make clean
               make -j$(nproc) install BINDIR=$out/bin/ INCLUDEDIR=$out/include/
            '';

          };

          apps.genWavFiles = import ./gen-wav-files.nix {inherit pkgs;};
        });
}
