{
  description = "C implementation of Tabata";

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
        pname = "cabata"; #package name
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
          };


          packages.default = pkgs.stdenv.mkDerivation {
            inherit buildInputs nativeBuildInputs pname version src;
            dontConfigure = true;
            installPhase = ''
               make clean
               make install BINDIR=$out/bin/
            '';

          };

          apps.genWavFiles = import ./gen-wav-files.nix {inherit pkgs;};
        });
}
