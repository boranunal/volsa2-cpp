#!/usr/bin/env bash
# ==============================================================================
# VolSa 2 - Universal Cross-Distribution Linux Installer
# ==============================================================================
# Supports: Debian, Ubuntu, Fedora, Arch, openSUSE, Manjaro, Alpine, Pop!_OS, etc.
# Compiles directly from source: checks required packages, prompts to install
# missing dependencies, builds via CMake, and installs to ~/.local or /usr/local.
# Does NOT require sudo permission to run the script.
# ==============================================================================

set -euo pipefail

# Color formatting
BOLD="\033[1m"
GREEN="\033[1;32m"
YELLOW="\033[1;33m"
CYAN="\033[1;36m"
RED="\033[1;31m"
RESET="\033[0m"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Embedded base64 payload of volsa2-icon.png (KORG Volca Sample 2 hardware pixel art icon)
EMBEDDED_ICON_B64="iVBORw0KGgoAAAANSUhEUgAAACgAAAAoCAYAAACM/rhtAAAAAXNSR0IArs4c6QAAAbZJREFUWIXtlrFLAlEcx79Waw2h0SBKERISQbRGEEFDg4rREjhVqxARDlnUKNHQDboKkot4qENDFEIUERaEhohEUTQWUv9ATe953j31Tq076X2W3/fdcXff9/veezyAw+Fw/jcmAPiovH/rbUROcHsXkXDY1KO3kWZQgykxXVN/A5vVDpvVTjWryumTDlJiGh6vm44vAluKB2ZDBx0x+/r2wqwNDXq87hqTnTIjN6UFapCYknaQ0E7srPdpQfMiuTw/0aQJAxs7VJsHLU01QdU2o0cHyTZDIx4bGcWSdxmhwxAAILAZQFJM4PH5qWVzhHYmWBNxUkwwtZRWI24V2kF5p+p1bmZ+UZMmuFwuAEAmk1GlFQanHU6mobtyseEM1SL9qBpNoBGXJxeYVc5fR0wNOvKnzCqnnYhJhFLti2fhi2cVmkAj1hKl3+9veF8QBMW1elHGVuaYmmACgFKpYLjj1pEQqe6Dn5Uvvf3Uhf6DU8EIHoolw2hCLwCsr63unQ33AwCGLGYYQd/nC7jN5fa750Tti2cx4Rw3jCaYAODm+spwqzgaO66u4mjsWG8/HA6Hw+lWfgBb0w9yycgXYQAAAABJRU5ErkJggg=="

# Embedded udev rule for KORG Volca Sample 2 USB MIDI
UDEV_RULE_CONTENT='# KORG Volca Sample 2 USB MIDI Interface
SUBSYSTEM=="sound", ATTRS{idVendor}=="0944", MODE="0666", GROUP="audio"
SUBSYSTEM=="usb", ATTRS{idVendor}=="0944", MODE="0666", GROUP="audio"
'

# Default settings
INSTALL_MODE="auto" # "auto", "user", or "system"
CUSTOM_ICON=""
SKIP_BUILD=false
AUTO_YES=false
UNINSTALL=false
DESKTOP_ONLY=false

print_banner() {
    echo -e "${CYAN}${BOLD}"
    echo "=========================================================="
    echo "       VolSa 2 - Linux Desktop & Package Installer        "
    echo "      KORG Volca Sample 2 Digital Sample Librarian        "
    echo "=========================================================="
    echo -e "${RESET}"
}

print_usage() {
    echo -e "${BOLD}Usage:${RESET} $0 [options]"
    echo ""
    echo -e "${BOLD}Options:${RESET}"
    echo "  --user                 Install for current user only (~/.local, no root needed) [Default]"
    echo "  --system               Install system-wide (/usr/local, requires sudo/root)"
    echo "  --icon <path.png>      Install with or update to a custom icon image"
    echo "  --skip-build           Skip compiling from source (use existing binaries in build/)"
    echo "  --desktop-only         Only install/update desktop launcher and icon"
    echo "  -y, --yes              Automatically accept prompts (install packages, udev rules)"
    echo "  --uninstall, -r        Uninstall VolSa 2 binaries, desktop launchers, and icons"
    echo "  -h, --help             Show this help message"
    echo ""
    echo -e "${BOLD}Examples:${RESET}"
    echo "  ./install.sh                      # Standard user install: checks deps, builds, installs"
    echo "  ./install.sh -y                   # Non-interactive user install with auto-confirmed prompts"
    echo "  sudo ./install.sh --system        # System-wide installation into /usr/local"
    echo "  ./install.sh --icon my_icon.png   # Update application and desktop icon"
    echo "  ./install.sh --uninstall          # Remove VolSa 2 cleanly"
}

# Prompt user for Yes/No with default
ask_user_yn() {
    local prompt="$1"
    local default_ans="${2:-n}" # "y" or "n"

    if [ "$AUTO_YES" = true ]; then
        return 0
    fi

    # If stdin is not a terminal, return default
    if [ ! -t 0 ]; then
        [ "$default_ans" = "y" ] && return 0 || return 1
    fi

    local choices="[y/N]"
    [ "$default_ans" = "y" ] && choices="[Y/n]"

    while true; do
        read -r -p "$prompt $choices: " response
        response="${response:-$default_ans}"
        case "$response" in
            [Yy]* ) return 0 ;;
            [Nn]* ) return 1 ;;
            * ) echo "Please answer y or n." ;;
        esac
    done
}

# Parse command line options
while [[ $# -gt 0 ]]; do
    case "$1" in
        --user)
            INSTALL_MODE="user"
            shift
            ;;
        --system)
            INSTALL_MODE="system"
            shift
            ;;
        --icon)
            if [[ -n "${2:-}" ]] && [[ -f "$2" ]]; then
                CUSTOM_ICON="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
                shift 2
            else
                echo -e "${RED}Error: --icon requires a valid image file path.${RESET}" >&2
                exit 1
            fi
            ;;
        --skip-build|--no-build)
            SKIP_BUILD=true
            shift
            ;;
        --desktop-only)
            DESKTOP_ONLY=true
            shift
            ;;
        -y|--yes)
            AUTO_YES=true
            shift
            ;;
        --uninstall|-r|--remove)
            UNINSTALL=true
            shift
            ;;
        -h|--help)
            print_banner
            print_usage
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${RESET}" >&2
            print_usage
            exit 1
            ;;
    esac
done

# Determine effective user & installation scope
if [ "$INSTALL_MODE" = "auto" ]; then
    if [ "$(id -u)" -eq 0 ]; then
        INSTALL_MODE="system"
    else
        INSTALL_MODE="user"
    fi
fi

if [ "$INSTALL_MODE" = "system" ] && [ "$(id -u)" -ne 0 ]; then
    echo -e "${YELLOW}Warning: System-wide installation requested without root privileges.${RESET}"
    echo "Re-running with sudo..."
    exec sudo "$0" --system ${CUSTOM_ICON:+--icon "$CUSTOM_ICON"} ${AUTO_YES:+-y} ${UNINSTALL:+--uninstall}
fi

# Target paths
if [ "$INSTALL_MODE" = "system" ]; then
    PREFIX="/usr/local"
    BIN_DIR="$PREFIX/bin"
    APP_DIR="$PREFIX/share/applications"
    ICON_DIR="$PREFIX/share/icons/hicolor/256x256/apps"
    PIXMAP_DIR="$PREFIX/share/pixmaps"
    DATA_DIR="$PREFIX/share/volsa2"
    UDEV_DIR="/etc/udev/rules.d"

    TARGET_USER="${SUDO_USER:-$USER}"
    TARGET_HOME=$(getent passwd "$TARGET_USER" | cut -d: -f6)
    DESKTOP_DIR=""
    if [ -n "$TARGET_HOME" ] && [ -d "$TARGET_HOME/Desktop" ]; then
        DESKTOP_DIR="$TARGET_HOME/Desktop"
    fi
else
    PREFIX="$HOME/.local"
    BIN_DIR="$PREFIX/bin"
    APP_DIR="$PREFIX/share/applications"
    ICON_DIR="$PREFIX/share/icons/hicolor/256x256/apps"
    PIXMAP_DIR="$PREFIX/share/pixmaps"
    DATA_DIR="$PREFIX/share/volsa2"
    UDEV_DIR=""

    # User desktop directory detection (supports localized desktop folders or no desktop folder)
    DESKTOP_DIR=""
    if [ -d "$HOME/Desktop" ]; then
        DESKTOP_DIR="$HOME/Desktop"
    elif command -v xdg-user-dir >/dev/null 2>&1; then
        xdg_dir=$(xdg-user-dir DESKTOP 2>/dev/null || true)
        xdg_dir="${xdg_dir%/}"
        if [ -n "$xdg_dir" ] && [ -d "$xdg_dir" ] && [ "$xdg_dir" != "${HOME%/}" ]; then
            DESKTOP_DIR="$xdg_dir"
        fi
    fi
fi

# ==============================================================================
# UNINSTALLATION ROUTINE
# ==============================================================================
if [ "$UNINSTALL" = true ]; then
    print_banner
    echo -e "${YELLOW}Uninstalling VolSa 2 (${INSTALL_MODE} mode)...${RESET}"

    rm -f "$BIN_DIR/volsa2-gui" "$BIN_DIR/volsa2-cli" "$BIN_DIR/volsa2"
    rm -f "$APP_DIR/volsa2.desktop"
    rm -f "$ICON_DIR/volsa2.png" "$ICON_DIR/volsa2-icon.png"
    rm -f "$PIXMAP_DIR/volsa2.png" "$PIXMAP_DIR/volsa2-icon.png" 2>/dev/null || true
    rm -rf "$DATA_DIR"

    if [ -n "$DESKTOP_DIR" ] && [ -f "$DESKTOP_DIR/VolSa2.desktop" ]; then
        rm -f "$DESKTOP_DIR/VolSa2.desktop"
    fi

    if [ "$INSTALL_MODE" = "system" ] && [ -f "$UDEV_DIR/99-korg-volca.rules" ]; then
        echo "Removing udev rule..."
        rm -f "$UDEV_DIR/99-korg-volca.rules"
        udevadm control --reload-rules 2>/dev/null || true
    fi

    # Update desktop and icon databases
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database "$APP_DIR" 2>/dev/null || true
    fi
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        gtk-update-icon-cache -f -t "$(dirname "$(dirname "$ICON_DIR")")" 2>/dev/null || true
    fi

    echo -e "${GREEN}VolSa 2 has been cleanly uninstalled.${RESET}"
    exit 0
fi

# ==============================================================================
# STEP 1: CHECK & INSTALL REQUIRED PACKAGES
# ==============================================================================
check_and_install_dependencies() {
    echo -e "${CYAN}[1/5] Checking Required Packages & Dependencies...${RESET}"

    local PKG_MANAGER=""
    local MISSING_PKGS=()
    local INSTALL_CMD=""

    if command -v apt-get >/dev/null 2>&1; then
        PKG_MANAGER="apt"
        local PKGS=(build-essential cmake pkg-config libasound2-dev libsamplerate0-dev libsndfile1-dev libzip-dev libssl-dev qt6-base-dev)
        for pkg in "${PKGS[@]}"; do
            if ! dpkg -s "$pkg" >/dev/null 2>&1; then
                MISSING_PKGS+=("$pkg")
            fi
        done
        INSTALL_CMD="sudo apt-get update && sudo apt-get install -y"
    elif command -v dnf >/dev/null 2>&1; then
        PKG_MANAGER="dnf"
        local PKGS=(gcc-c++ cmake pkgconf alsa-lib-devel libsamplerate-devel libsndfile-devel libzip-devel openssl-devel qt6-qtbase-devel)
        for pkg in "${PKGS[@]}"; do
            if ! rpm -q "$pkg" >/dev/null 2>&1; then
                MISSING_PKGS+=("$pkg")
            fi
        done
        INSTALL_CMD="sudo dnf install -y"
    elif command -v pacman >/dev/null 2>&1; then
        PKG_MANAGER="pacman"
        local PKGS=(gcc cmake pkgconf alsa-lib libsamplerate libsndfile libzip openssl qt6-base)
        for pkg in "${PKGS[@]}"; do
            if ! pacman -Q "$pkg" >/dev/null 2>&1; then
                MISSING_PKGS+=("$pkg")
            fi
        done
        INSTALL_CMD="sudo pacman -S --noconfirm"
    elif command -v zypper >/dev/null 2>&1; then
        PKG_MANAGER="zypper"
        local PKGS=(patterns-devel-base-devel_basis cmake pkg-config alsa-devel libsamplerate-devel libsndfile-devel libzip-devel libopenssl-devel qt6-base-devel)
        for pkg in "${PKGS[@]}"; do
            if ! rpm -q "$pkg" >/dev/null 2>&1; then
                MISSING_PKGS+=("$pkg")
            fi
        done
        INSTALL_CMD="sudo zypper install -y"
    elif command -v apk >/dev/null 2>&1; then
        PKG_MANAGER="apk"
        local PKGS=(build-base cmake pkgconf alsa-lib-dev libsamplerate-dev libsndfile-dev libzip-dev openssl-dev qt6-qtbase-dev)
        for pkg in "${PKGS[@]}"; do
            if ! apk info -e "$pkg" >/dev/null 2>&1; then
                MISSING_PKGS+=("$pkg")
            fi
        done
        INSTALL_CMD="sudo apk add"
    else
        PKG_MANAGER="unknown"
    fi

    # Fallback verification: check if compiler, cmake and pkg-config libraries are already usable
    if [ ${#MISSING_PKGS[@]} -gt 0 ]; then
        if command -v cmake >/dev/null 2>&1 && (command -v g++ >/dev/null 2>&1 || command -v clang++ >/dev/null 2>&1); then
            if command -v pkg-config >/dev/null 2>&1 || command -v pkgconf >/dev/null 2>&1; then
                if pkg-config --exists alsa samplerate sndfile libzip 2>/dev/null; then
                    if [ -d "/usr/local/Qt-6.9.1" ] || pkg-config --exists Qt6Core 2>/dev/null; then
                        echo -e "${GREEN}  ✓ Development tools and libraries verified via pkg-config/custom paths.${RESET}"
                        return 0
                    fi
                fi
            fi
        fi

        echo -e "${YELLOW}  Notice: Missing required build dependencies (${#MISSING_PKGS[@]}):${RESET}"
        for p in "${MISSING_PKGS[@]}"; do
            echo -e "    - ${BOLD}$p${RESET}"
        done
        echo ""

        if [ "$PKG_MANAGER" != "unknown" ]; then
            if ask_user_yn "Would you like to install the missing packages now using '${INSTALL_CMD}'?" "y"; then
                echo -e "${CYAN}Installing missing packages...${RESET}"
                if eval "${INSTALL_CMD} ${MISSING_PKGS[*]}"; then
                    echo -e "${GREEN}  ✓ Dependencies installed successfully.${RESET}"
                else
                    echo -e "${RED}Error: Failed to install packages.${RESET}" >&2
                    echo "Please install them manually using:"
                    echo "  ${INSTALL_CMD} ${MISSING_PKGS[*]}"
                    if ! ask_user_yn "Attempt to compile anyway?" "n"; then
                        exit 1
                    fi
                fi
            else
                echo -e "${YELLOW}Skipping package installation.${RESET}"
                echo "If the build fails, install them with:"
                echo "  ${INSTALL_CMD} ${MISSING_PKGS[*]}"
                echo ""
                if ! ask_user_yn "Attempt to compile anyway?" "y"; then
                    exit 1
                fi
            fi
        else
            echo -e "${YELLOW}Unknown package manager. Please ensure required development packages are installed.${RESET}"
        fi
    else
        echo -e "${GREEN}  ✓ All required build packages and development libraries are installed.${RESET}"
    fi
}

# ==============================================================================
# STEP 2: BUILD FROM SOURCE
# ==============================================================================
build_from_source() {
    echo -e "${CYAN}[2/5] Compiling VolSa 2 from Source...${RESET}"

    local BUILD_DIR="$SCRIPT_DIR/build"
    mkdir -p "$BUILD_DIR"

    local CMAKE_ARGS=("-DCMAKE_BUILD_TYPE=Release")

    # If custom Qt6 directory exists, hint CMake
    if [ -d "/usr/local/Qt-6.9.1" ]; then
        CMAKE_ARGS+=("-DCMAKE_PREFIX_PATH=/usr/local/Qt-6.9.1")
    fi

    echo "  -> Configuring build with CMake..."
    cmake -B "$BUILD_DIR" -S "$SCRIPT_DIR" "${CMAKE_ARGS[@]}"

    echo "  -> Compiling binaries using $(nproc) parallel cores..."
    cmake --build "$BUILD_DIR" -j"$(nproc)"

    # Verify build results
    if [ ! -f "$BUILD_DIR/gui/volsa2-gui" ] || [ ! -x "$BUILD_DIR/gui/volsa2-gui" ]; then
        echo -e "${RED}Error: Build failed or volsa2-gui binary was not produced.${RESET}" >&2
        exit 1
    fi
    echo -e "${GREEN}  ✓ Build completed successfully.${RESET}"
}

print_banner
echo -e "${BOLD}Installation Mode:${RESET} $INSTALL_MODE"
echo -e "${BOLD}Target Binary Directory:${RESET} $BIN_DIR"
echo -e "${BOLD}Desktop Entry Directory:${RESET} $APP_DIR"
echo -e "${BOLD}Application Icon Directory:${RESET} $ICON_DIR"
if [ -n "$DESKTOP_DIR" ] && [ -d "$DESKTOP_DIR" ]; then
    echo -e "${BOLD}User Desktop Directory:${RESET} $DESKTOP_DIR"
fi
echo ""

# Ensure required directories exist
mkdir -p "$BIN_DIR" "$APP_DIR" "$ICON_DIR" "$DATA_DIR"
[ -d "$PIXMAP_DIR" ] || mkdir -p "$PIXMAP_DIR" 2>/dev/null || true

# Check packages and build
if [ "$DESKTOP_ONLY" = false ]; then
    check_and_install_dependencies

    if [ "$SKIP_BUILD" = false ]; then
        build_from_source
    else
        echo -e "${CYAN}[2/5] Skipping compilation (--skip-build specified)...${RESET}"
    fi

    # ==============================================================================
    # STEP 3: INSTALL EXECUTABLES
    # ==============================================================================
    echo -e "${CYAN}[3/5] Installing Executable Binaries...${RESET}"

    SRC_GUI="$SCRIPT_DIR/build/gui/volsa2-gui"
    SRC_CLI="$SCRIPT_DIR/build/volsa2-cli"

    if [ -f "$SRC_GUI" ] && [ -x "$SRC_GUI" ]; then
        echo "  -> Installing 'volsa2-gui' to $BIN_DIR..."
        cp -f "$SRC_GUI" "$BIN_DIR/volsa2-gui"
        chmod +x "$BIN_DIR/volsa2-gui"
    else
        echo -e "${RED}Error: Could not find compiled binary at $SRC_GUI.${RESET}" >&2
        exit 1
    fi

    if [ -f "$SRC_CLI" ] && [ -x "$SRC_CLI" ]; then
        echo "  -> Installing 'volsa2-cli' to $BIN_DIR..."
        cp -f "$SRC_CLI" "$BIN_DIR/volsa2-cli"
        chmod +x "$BIN_DIR/volsa2-cli"
    fi

    # Convenient symlink 'volsa2' -> 'volsa2-gui'
    ln -sf "volsa2-gui" "$BIN_DIR/volsa2"
    echo -e "${GREEN}  ✓ Executables installed to $BIN_DIR${RESET}"
else
    echo -e "${CYAN}[1/5] Skipping package check (--desktop-only)...${RESET}"
    echo -e "${CYAN}[2/5] Skipping build (--desktop-only)...${RESET}"
    echo -e "${CYAN}[3/5] Skipping executable installation (--desktop-only)...${RESET}"
fi

# ==============================================================================
# STEP 4: CONFIGURE APPLICATION ICON & DESKTOP LAUNCHERS
# ==============================================================================
echo -e "${CYAN}[4/5] Configuring Application Icon & Desktop Launchers...${RESET}"

TARGET_ICON_FILE="$ICON_DIR/volsa2-icon.png"

# Icon Source Resolution
SRC_ICON=""
if [ -n "$CUSTOM_ICON" ]; then
    SRC_ICON="$CUSTOM_ICON"
elif [ -f "$SCRIPT_DIR/volsa2-icon.png" ]; then
    SRC_ICON="$SCRIPT_DIR/volsa2-icon.png"
fi

if [ -n "$SRC_ICON" ] && [ -f "$SRC_ICON" ]; then
    echo "  -> Installing icon from $SRC_ICON..."
    cp -f "$SRC_ICON" "$TARGET_ICON_FILE"
else
    echo "  -> Extracting embedded volsa2-icon.png payload..."
    echo "$EMBEDDED_ICON_B64" | base64 -d > "$TARGET_ICON_FILE"
fi

# Standard aliases and pixmap fallbacks
cp -f "$TARGET_ICON_FILE" "$ICON_DIR/volsa2.png"
cp -f "$TARGET_ICON_FILE" "$DATA_DIR/volsa2-icon.png"
cp -f "$TARGET_ICON_FILE" "$DATA_DIR/icon.png"
if [ "$INSTALL_MODE" = "system" ] || [ -d "$PIXMAP_DIR" ]; then
    cp -f "$TARGET_ICON_FILE" "$PIXMAP_DIR/volsa2-icon.png" 2>/dev/null || true
    cp -f "$TARGET_ICON_FILE" "$PIXMAP_DIR/volsa2.png" 2>/dev/null || true
fi
echo -e "${GREEN}  ✓ Icon installed: $TARGET_ICON_FILE${RESET}"

# FreeDesktop .desktop content
if [ "$INSTALL_MODE" = "system" ]; then
    EXEC_CMD="volsa2-gui"
else
    EXEC_CMD="$BIN_DIR/volsa2-gui"
fi

DESKTOP_CONTENT="[Desktop Entry]
Version=1.0
Type=Application
Name=VolSa 2
GenericName=Sample Librarian & Sequencer Manager
Comment=Digital sample librarian for KORG Volca Sample 2
Exec=$EXEC_CMD %F
Icon=$TARGET_ICON_FILE
Terminal=false
Categories=AudioVideo;Audio;Sequencer;Midi;
Keywords=korg;volca;sample;sampler;sequencer;midi;alsa;
StartupNotify=true
StartupWMClass=volsa2-gui
"

# 1. Install to system / user application menu
echo "$DESKTOP_CONTENT" > "$APP_DIR/volsa2.desktop"
chmod +x "$APP_DIR/volsa2.desktop"
echo -e "${GREEN}  ✓ Application menu launcher created: $APP_DIR/volsa2.desktop${RESET}"

# 2. Install to Desktop if Desktop folder exists
if [ -n "$DESKTOP_DIR" ] && [ -d "$DESKTOP_DIR" ]; then
    DESKTOP_FILE="$DESKTOP_DIR/VolSa2.desktop"
    echo "$DESKTOP_CONTENT" > "$DESKTOP_FILE"
    chmod +x "$DESKTOP_FILE"

    # Mark trusted on GNOME / KDE if gio is available
    if command -v gio >/dev/null 2>&1; then
        gio set "$DESKTOP_FILE" metadata::trusted true 2>/dev/null || true
    fi

    # Fix ownership if run via sudo
    if [ "$INSTALL_MODE" = "system" ] && [ -n "${SUDO_USER:-}" ]; then
        chown "$TARGET_USER:$TARGET_USER" "$DESKTOP_FILE" 2>/dev/null || true
    fi
    echo -e "${GREEN}  ✓ Desktop executable shortcut created: $DESKTOP_FILE${RESET}"
else
    echo -e "  • No desktop directory found (skipping desktop shortcut)"
fi

# Refresh desktop & icon databases
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$APP_DIR" 2>/dev/null || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t "$(dirname "$(dirname "$ICON_DIR")")" 2>/dev/null || true
fi

# ==============================================================================
# STEP 5: HARDWARE PERMISSIONS & LINUX UDEV RULES
# ==============================================================================
echo -e "${CYAN}[5/5] Hardware Permissions & Linux udev Rules...${RESET}"

if [ -f "/etc/udev/rules.d/99-korg-volca.rules" ]; then
    echo -e "${GREEN}  ✓ udev rules already configured on system (/etc/udev/rules.d/99-korg-volca.rules).${RESET}"
else
    if [ "$INSTALL_MODE" = "system" ]; then
        echo "$UDEV_RULE_CONTENT" > "/etc/udev/rules.d/99-korg-volca.rules"
        udevadm control --reload-rules 2>/dev/null || true
        udevadm trigger 2>/dev/null || true
        echo -e "${GREEN}  ✓ udev rules installed to /etc/udev/rules.d/99-korg-volca.rules${RESET}"
    else
        echo -e "${YELLOW}  Hardware Access Notice:${RESET}"
        echo "  To allow standard users to communicate with the Volca Sample 2 over USB MIDI"
        echo "  without needing root, a udev rule can be installed."
        echo ""
        if ask_user_yn "Would you like to install the USB udev rule now (requires sudo)?" "n"; then
            if echo "$UDEV_RULE_CONTENT" | sudo tee /etc/udev/rules.d/99-korg-volca.rules >/dev/null; then
                sudo udevadm control --reload-rules 2>/dev/null || true
                sudo udevadm trigger 2>/dev/null || true
                echo -e "${GREEN}  ✓ udev rule installed to /etc/udev/rules.d/99-korg-volca.rules${RESET}"
            else
                echo -e "${YELLOW}  Notice: sudo was not granted or failed. Rule installation skipped.${RESET}"
                echo "  You can manually install it anytime with:"
                echo "    echo '$UDEV_RULE_CONTENT' | sudo tee /etc/udev/rules.d/99-korg-volca.rules"
                echo "    sudo udevadm control --reload-rules && sudo udevadm trigger"
            fi
        else
            echo "  Skipping udev rule installation."
            echo "  You can manually install it anytime with:"
            echo "    echo '$UDEV_RULE_CONTENT' | sudo tee /etc/udev/rules.d/99-korg-volca.rules"
            echo "    sudo udevadm control --reload-rules && sudo udevadm trigger"
        fi
    fi
fi

# Check audio group membership
CURRENT_USER="${SUDO_USER:-$USER}"
if ! id -nG "$CURRENT_USER" | grep -qw "audio"; then
    echo -e "${YELLOW}  Note: User '$CURRENT_USER' is not currently in the 'audio' group.${RESET}"
    echo "  For optimal ALSA performance, run: sudo usermod -aG audio $CURRENT_USER"
fi

# PATH Check for user-level install
if [ "$INSTALL_MODE" = "user" ]; then
    if [[ ":$PATH:" != *":$BIN_DIR:"* ]]; then
        echo ""
        echo -e "${YELLOW}Notice: '$BIN_DIR' is not in your current PATH.${RESET}"
        echo "To run 'volsa2-gui' or 'volsa2-cli' directly from your terminal, add it to your ~/.bashrc:"
        echo "  export PATH=\"\$HOME/.local/bin:\$PATH\""
    fi
fi

echo ""
echo -e "${GREEN}${BOLD}==========================================================${RESET}"
echo -e "${GREEN}${BOLD}       VolSa 2 Installation Completed Successfully!       ${RESET}"
echo -e "${GREEN}${BOLD}==========================================================${RESET}"
echo ""
if [ -n "$DESKTOP_DIR" ] && [ -f "$DESKTOP_DIR/VolSa2.desktop" ]; then
    echo -e "• ${BOLD}Desktop Executable:${RESET}  $DESKTOP_DIR/VolSa2.desktop"
fi
echo -e "• ${BOLD}Application Launcher:${RESET} $APP_DIR/volsa2.desktop"
echo -e "• ${BOLD}Application Icon:${RESET}     $TARGET_ICON_FILE"
echo -e "• ${BOLD}GUI Executable:${RESET}        $BIN_DIR/volsa2-gui"
echo -e "• ${BOLD}CLI Executable:${RESET}        $BIN_DIR/volsa2-cli"
echo ""
echo -e "${CYAN}${BOLD}Customizing Your Icon Later:${RESET}"
echo "When you have your custom icon ready, simply run:"
echo -e "  ${BOLD}./install.sh --icon /path/to/your/new_icon.png${RESET}"
echo "or drop it directly into:"
echo "  $ICON_DIR/volsa2-icon.png"
echo ""
