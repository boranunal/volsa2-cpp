#!/usr/bin/env bash
# ==============================================================================
# VolSa 2 - Universal Cross-Distribution Linux Installer
# ==============================================================================
# Supports: Debian, Ubuntu, Fedora, Arch, openSUSE, Manjaro, Alpine, Pop!_OS, etc.
# Supports both:
#   - User-level install (default for non-root: ~/.local/bin, ~/Desktop, no sudo needed)
#   - System-wide install (when run with sudo or --system: /usr/local/bin, /etc/udev/...)
# Supports custom icon replacement anytime via:
#   ./install.sh --icon /path/to/custom_icon.png
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

# Default settings
INSTALL_MODE="auto" # "auto", "user", or "system"
CUSTOM_ICON=""
PREFER_BINARY="appimage" # "appimage" or "native"
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
    echo "  --user                 Install for current user only (~/.local, no root needed)"
    echo "  --system               Install system-wide (/usr/local, requires sudo/root)"
    echo "  --icon <path.png>      Install with or update to a custom icon image"
    echo "  --native               Prefer native compiled binaries over AppImage"
    echo "  --appimage             Prefer standalone portable AppImage"
    echo "  --desktop-only         Only install/update desktop launcher and icon"
    echo "  --uninstall, -r        Uninstall VolSa 2 binaries, desktop launchers, and icons"
    echo "  -h, --help             Show this help message"
    echo ""
    echo -e "${BOLD}Examples:${RESET}"
    echo "  ./install.sh                      # Standard user installation with desktop launcher"
    echo "  sudo ./install.sh --system        # System-wide installation with udev rules"
    echo "  ./install.sh --icon my_icon.png   # Update application and desktop icon"
    echo "  ./install.sh --uninstall          # Remove VolSa 2 cleanly"
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
        --native)
            PREFER_BINARY="native"
            shift
            ;;
        --appimage)
            PREFER_BINARY="appimage"
            shift
            ;;
        --desktop-only)
            DESKTOP_ONLY=true
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
    exec sudo "$0" --system ${CUSTOM_ICON:+--icon "$CUSTOM_ICON"} ${UNINSTALL:+--uninstall}
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
    
    # Target user for Desktop shortcut if run via sudo
    TARGET_USER="${SUDO_USER:-$USER}"
    TARGET_HOME=$(getent passwd "$TARGET_USER" | cut -d: -f6)
    DESKTOP_DIR="$TARGET_HOME/Desktop"
else
    PREFIX="$HOME/.local"
    BIN_DIR="$PREFIX/bin"
    APP_DIR="$PREFIX/share/applications"
    ICON_DIR="$PREFIX/share/icons/hicolor/256x256/apps"
    PIXMAP_DIR="$PREFIX/share/pixmaps"
    DATA_DIR="$PREFIX/share/volsa2"
    UDEV_DIR=""
    
    # User desktop directory detection
    if [ -d "$HOME/Desktop" ]; then
        DESKTOP_DIR="$HOME/Desktop"
    elif command -v xdg-user-dir >/dev/null 2>&1; then
        xdg_dir=$(xdg-user-dir DESKTOP 2>/dev/null || true)
        if [ -n "$xdg_dir" ] && [ -d "$xdg_dir" ] && [ "$xdg_dir" != "$HOME" ]; then
            DESKTOP_DIR="$xdg_dir"
        else
            DESKTOP_DIR="$HOME/Desktop"
        fi
    else
        DESKTOP_DIR="$HOME/Desktop"
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
# LOCATE SOURCE ASSETS
# ==============================================================================
find_asset() {
    local target="$1"
    shift
    for search_dir in "$@"; do
        if [ -f "$search_dir/$target" ]; then
            echo "$search_dir/$target"
            return 0
        fi
    done
    return 1
}

SEARCH_PATHS=(
    "$SCRIPT_DIR/release"
    "$SCRIPT_DIR/release/bin"
    "$SCRIPT_DIR/build"
    "$SCRIPT_DIR/build/gui"
    "$SCRIPT_DIR"
)

SRC_APPIMAGE=$(find_asset "VolSa2-2.0.0-x86_64.AppImage" "${SEARCH_PATHS[@]}" || find_asset "VolSa2-x86_64.AppImage" "${SEARCH_PATHS[@]}" || true)
SRC_GUI_BIN=$(find_asset "volsa2-gui" "${SEARCH_PATHS[@]}" || true)
SRC_CLI_BIN=$(find_asset "volsa2-cli" "${SEARCH_PATHS[@]}" || true)
SRC_ICON=$(find_asset "volsa2-icon.png" "${SEARCH_PATHS[@]}" || find_asset "volsa2.png" "${SEARCH_PATHS[@]}" || true)
SRC_DESKTOP=$(find_asset "volsa2.desktop" "${SEARCH_PATHS[@]}" || true)
SRC_UDEV=$(find_asset "99-korg-volca.rules" "${SEARCH_PATHS[@]}" || true)

# Override icon if user passed --icon
if [ -n "$CUSTOM_ICON" ]; then
    SRC_ICON="$CUSTOM_ICON"
fi

print_banner
echo -e "${BOLD}Installation Mode:${RESET} $INSTALL_MODE"
echo -e "${BOLD}Target Binary Directory:${RESET} $BIN_DIR"
echo -e "${BOLD}Desktop Entry Directory:${RESET} $APP_DIR"
echo -e "${BOLD}Application Icon Directory:${RESET} $ICON_DIR"
[ -n "$DESKTOP_DIR" ] && echo -e "${BOLD}User Desktop Directory:${RESET} $DESKTOP_DIR"
echo ""

# Ensure required directories exist
mkdir -p "$BIN_DIR" "$APP_DIR" "$ICON_DIR" "$DATA_DIR"
[ -d "$PIXMAP_DIR" ] || mkdir -p "$PIXMAP_DIR" 2>/dev/null || true

# ==============================================================================
# [1/4] INSTALL EXECUTABLES
# ==============================================================================
if [ "$DESKTOP_ONLY" = false ]; then
    echo -e "${CYAN}[1/4] Installing Executable Binaries...${RESET}"
    
    CHOSEN_GUI=""
    if [ "$PREFER_BINARY" = "appimage" ] && [ -n "$SRC_APPIMAGE" ]; then
        echo "  -> Installing self-contained release AppImage as 'volsa2-gui'..."
        cp -f "$SRC_APPIMAGE" "$BIN_DIR/volsa2-gui"
        chmod +x "$BIN_DIR/volsa2-gui"
        CHOSEN_GUI="$BIN_DIR/volsa2-gui"
    elif [ -n "$SRC_GUI_BIN" ]; then
        echo "  -> Installing native compiled executable 'volsa2-gui'..."
        cp -f "$SRC_GUI_BIN" "$BIN_DIR/volsa2-gui"
        chmod +x "$BIN_DIR/volsa2-gui"
        CHOSEN_GUI="$BIN_DIR/volsa2-gui"
    else
        echo -e "${RED}Error: Could not locate volsa2-gui binary or AppImage.${RESET}" >&2
        exit 1
    fi

    if [ -n "$SRC_CLI_BIN" ]; then
        echo "  -> Installing 'volsa2-cli'..."
        cp -f "$SRC_CLI_BIN" "$BIN_DIR/volsa2-cli"
        chmod +x "$BIN_DIR/volsa2-cli"
    fi

    # Create convenient symlink 'volsa2' -> 'volsa2-gui'
    ln -sf "volsa2-gui" "$BIN_DIR/volsa2"
    echo -e "${GREEN}  ✓ Executables installed to $BIN_DIR${RESET}"
else
    echo -e "${CYAN}[1/4] Skipping binary installation (--desktop-only)...${RESET}"
fi

# ==============================================================================
# [2/4] INSTALL APPLICATION ICON
# ==============================================================================
echo -e "${CYAN}[2/4] Configuring Application Icon...${RESET}"

TARGET_ICON_FILE="$ICON_DIR/volsa2-icon.png"

if [ -n "$SRC_ICON" ] && [ -f "$SRC_ICON" ]; then
    echo "  -> Installing icon from $SRC_ICON..."
    cp -f "$SRC_ICON" "$TARGET_ICON_FILE"
else
    echo "  -> Extracting embedded volsa2-icon.png payload..."
    echo "$EMBEDDED_ICON_B64" | base64 -d > "$TARGET_ICON_FILE"
fi

# Ensure both volsa2-icon.png and volsa2.png exist across standard directories
cp -f "$TARGET_ICON_FILE" "$ICON_DIR/volsa2.png"
cp -f "$TARGET_ICON_FILE" "$DATA_DIR/volsa2-icon.png"
cp -f "$TARGET_ICON_FILE" "$DATA_DIR/icon.png"
if [ "$INSTALL_MODE" = "system" ] || [ -d "$PIXMAP_DIR" ]; then
    cp -f "$TARGET_ICON_FILE" "$PIXMAP_DIR/volsa2-icon.png" 2>/dev/null || true
    cp -f "$TARGET_ICON_FILE" "$PIXMAP_DIR/volsa2.png" 2>/dev/null || true
fi
echo -e "${GREEN}  ✓ Icon installed: $TARGET_ICON_FILE${RESET}"

# ==============================================================================
# [3/4] CREATE DESKTOP EXECUTABLE & APPLICATION LAUNCHER
# ==============================================================================
echo -e "${CYAN}[3/4] Generating FreeDesktop .desktop Launchers...${RESET}"

# Determine actual Exec path for launcher
if [ "$INSTALL_MODE" = "system" ]; then
    EXEC_CMD="volsa2-gui"
else
    EXEC_CMD="$BIN_DIR/volsa2-gui"
fi

# Construct standard .desktop content
# Referencing the installed path of volsa2-icon.png guarantees that desktop
# environments (GNOME Desktop icons, Cinnamon, KDE, XFCE) render the icon
# immediately without requiring system icon cache rebuilds or session logouts.
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
if [ -d "$DESKTOP_DIR" ]; then
    DESKTOP_FILE="$DESKTOP_DIR/VolSa2.desktop"
    echo "$DESKTOP_CONTENT" > "$DESKTOP_FILE"
    chmod +x "$DESKTOP_FILE"
    
    # Mark trusted on GNOME / KDE if gio is available
    if command -v gio >/dev/null 2>&1; then
        gio set "$DESKTOP_FILE" metadata::trusted true 2>/dev/null || true
    fi
    
    # If run via sudo, fix ownership for the real user
    if [ "$INSTALL_MODE" = "system" ] && [ -n "${SUDO_USER:-}" ]; then
        chown "$TARGET_USER:$TARGET_USER" "$DESKTOP_FILE" 2>/dev/null || true
    fi
    echo -e "${GREEN}  ✓ Desktop executable shortcut created: $DESKTOP_FILE${RESET}"
fi

# Refresh system desktop and icon caches if tools are present
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$APP_DIR" 2>/dev/null || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t "$(dirname "$(dirname "$ICON_DIR")")" 2>/dev/null || true
fi

# ==============================================================================
# [4/4] HARDWARE ACCESS & UDEV CONFIGURATION
# ==============================================================================
echo -e "${CYAN}[4/4] Hardware Permissions & Linux udev Rules...${RESET}"

if [ "$INSTALL_MODE" = "system" ]; then
    if [ -n "$SRC_UDEV" ] && [ -f "$SRC_UDEV" ]; then
        cp -f "$SRC_UDEV" "$UDEV_DIR/99-korg-volca.rules"
        udevadm control --reload-rules 2>/dev/null || true
        udevadm trigger 2>/dev/null || true
        echo -e "${GREEN}  ✓ udev rules installed to $UDEV_DIR/99-korg-volca.rules${RESET}"
    fi
else
    if [ -f "/etc/udev/rules.d/99-korg-volca.rules" ]; then
        echo -e "${GREEN}  ✓ udev rules already configured on system.${RESET}"
    else
        echo -e "${YELLOW}  Hardware Access Note:${RESET}"
        echo "  To allow standard users to communicate with the Volca Sample 2 over USB MIDI"
        echo "  without needing root, install the udev rule with:"
        echo -e "  ${BOLD}sudo cp release/99-korg-volca.rules /etc/udev/rules.d/ && sudo udevadm control --reload-rules && sudo udevadm trigger${RESET}"
    fi
fi

# Check audio group membership for current user
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
echo -e "• ${BOLD}Desktop Executable:${RESET}  $DESKTOP_DIR/VolSa2.desktop"
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
