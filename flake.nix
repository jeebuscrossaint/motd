{
  description = "A lightweight system information fetch tool written in C++";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "motd";
          version = "0.1.0";

          src = ./.;

          nativeBuildInputs = with pkgs; [
            xmake
            pkg-config
          ];

          buildInputs = with pkgs; [
            pciutils
          ];

          buildPhase = ''
            # Ensure xmake can find pci headers
            export C_INCLUDE_PATH="${pkgs.pciutils}/include:$C_INCLUDE_PATH"
            export CPLUS_INCLUDE_PATH="${pkgs.pciutils}/include:$CPLUS_INCLUDE_PATH"
            export LIBRARY_PATH="${pkgs.pciutils}/lib:$LIBRARY_PATH"

            xmake f -m release -p linux -y
            xmake
          '';

          installPhase = ''
            mkdir -p $out/bin
            cp build/linux/*/release/motd $out/bin/
          '';

          meta = with pkgs.lib; {
            description = "A lightweight system information fetch tool";
            homepage = "https://github.com/amarnath/motd";
            license = licenses.bsd3;
            platforms = platforms.linux ++ platforms.openbsd;
            mainProgram = "motd";
          };
        };

        apps.default = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/motd";
        };

        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            xmake
            pkg-config
            pciutils
            gcc
          ];

          shellHook = ''
            echo "motd development environment"
            echo "Run 'xmake' to build, 'xmake run motd' to test"
          '';
        };
      }
    );
}
