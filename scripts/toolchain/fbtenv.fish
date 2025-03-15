#!/usr/bin/env fish

# Fish shell version of fbtenv.sh for Flipper Zero development
# This version automatically fixes PATH issues and handles missing commands

# First, ensure we have standard system paths in our PATH
set -l standard_paths /usr/local/bin /usr/bin /bin /opt/homebrew/bin /opt/local/bin
for path_entry in $standard_paths
    if not contains $path_entry $PATH
        set -gx PATH $path_entry $PATH
    end
end

# Set default values
set DEFAULT_SCRIPT_PATH (pwd)
set -q FBT_TOOLCHAIN_VERSION; or set FBT_TOOLCHAIN_VERSION "39"

# Set system type defaults that will be used if detection fails
set -g SYS_TYPE "darwin"  # Default to macOS
set -g ARCH_TYPE "arm64"  # Default to ARM64

if set -q FBT_TOOLCHAIN_PATH
    set FBT_TOOLCHAIN_PATH_WAS_SET 1
else
    set FBT_TOOLCHAIN_PATH_WAS_SET 0
    set FBT_TOOLCHAIN_PATH $DEFAULT_SCRIPT_PATH
end

# Set up toolchain paths based on defaults first
set -g TOOLCHAIN_ARCH_DIR "$FBT_TOOLCHAIN_PATH/toolchain/$ARCH_TYPE-$SYS_TYPE"
set -g TOOLCHAIN_URL "https://update.flipperzero.one/builds/toolchain/gcc-arm-none-eabi-12.3-$ARCH_TYPE-$SYS_TYPE-flipper-$FBT_TOOLCHAIN_VERSION.tar.gz"

# Function to check if a command is available
function has_command
    command -s $argv[1] >/dev/null 2>&1
end

function fbtenv_show_usage
    echo "Running this script manually is wrong, please source it"
    echo "Example:"
    printf "\tsource scripts/toolchain/fbtenv.fish\n"
    echo "To restore your environment, source fbtenv.fish with '--restore'."
    echo "Example:"
    printf "\tsource scripts/toolchain/fbtenv.fish --restore\n"
end

function fbtenv_restore_env
    # Reset PATH if we saved it
    if set -q SAVED_PATH
        set PATH $SAVED_PATH
    end
    
    # Reset prompt
    if set -q SAVED_PS1
        set PS1 $SAVED_PS1
    end
    
    if set -q SAVED_PROMPT
        set PROMPT $SAVED_PROMPT
    end

    # Reset environment variables
    if set -q SAVED_SSL_CERT_FILE
        set -gx SSL_CERT_FILE $SAVED_SSL_CERT_FILE
    else
        set -e SSL_CERT_FILE
    end
    
    if set -q SAVED_REQUESTS_CA_BUNDLE
        set -gx REQUESTS_CA_BUNDLE $SAVED_REQUESTS_CA_BUNDLE
    else
        set -e REQUESTS_CA_BUNDLE
    end

    if set -q SAVED_PYTHONNOUSERSITE
        set -gx PYTHONNOUSERSITE $SAVED_PYTHONNOUSERSITE
    else
        set -e PYTHONNOUSERSITE
    end
    
    if set -q SAVED_PYTHONPATH
        set -gx PYTHONPATH $SAVED_PYTHONPATH
    else
        set -e PYTHONPATH
    end
    
    if set -q SAVED_PYTHONHOME
        set -gx PYTHONHOME $SAVED_PYTHONHOME
    else
        set -e PYTHONHOME
    end

    if set -q SYS_TYPE; and test "$SYS_TYPE" = "linux"
        if set -q SAVED_TERMINFO_DIRS
            set -gx TERMINFO_DIRS $SAVED_TERMINFO_DIRS
        else
            set -e TERMINFO_DIRS
        end
    end
    
    # Unset all saved variables
    set -e SAVED_PATH
    set -e SAVED_PS1
    set -e SAVED_PROMPT
    set -e SAVED_SSL_CERT_FILE
    set -e SAVED_REQUESTS_CA_BUNDLE
    set -e SAVED_PYTHONNOUSERSITE
    set -e SAVED_PYTHONPATH
    set -e SAVED_PYTHONHOME
    set -e SAVED_TERMINFO_DIRS
    
    echo "Environment restored"
end

function fbtenv_set_shell_prompt
    # Save original prompts
    if set -q PS1
        set -g SAVED_PS1 $PS1
        set PS1 "[fbt]$PS1"
    else if set -q PROMPT
        set -g SAVED_PROMPT $PROMPT
        set PROMPT "[fbt]$PROMPT"
    end
end

function fbtenv_check_if_sourced_multiple_times
    if set -q PS1; and string match -q "*[fbt]*" "$PS1"
        return 1
    else if set -q PROMPT; and string match -q "*[fbt]*" "$PROMPT"
        return 1
    end
    return 0
end

function fbtenv_detect_system
    # Try to detect system type, with robust error handling
    if has_command uname
        # Use command to ensure we use system binaries
        if has_command tr
            set -g SYS_TYPE (command uname -s | command tr '[:upper:]' '[:lower:]')
        else
            # If tr is not available, do a simpler detection
            set -l sys (command uname -s)
            if string match -q "*darwin*" "$sys"
                set -g SYS_TYPE "darwin"
            else if string match -q "*linux*" "$sys"
                set -g SYS_TYPE "linux"
            else
                echo "Warning: Unable to properly convert system type. Using default."
            end
        end
        
        set -g ARCH_TYPE (command uname -m)
    else
        echo "Warning: 'uname' command not found, using defaults (darwin/arm64)"
    end
    
    # Update paths with detected system type
    set -g TOOLCHAIN_ARCH_DIR "$FBT_TOOLCHAIN_PATH/toolchain/$ARCH_TYPE-$SYS_TYPE"
    set -g TOOLCHAIN_URL "https://update.flipperzero.one/builds/toolchain/gcc-arm-none-eabi-12.3-$ARCH_TYPE-$SYS_TYPE-flipper-$FBT_TOOLCHAIN_VERSION.tar.gz"
    
    echo "System: $SYS_TYPE, Architecture: $ARCH_TYPE"
end

function fbtenv_setup_env
    # Save PATH before modification
    set -g SAVED_PATH $PATH
    
    # Update PATH if toolchain exists
    if test -d "$TOOLCHAIN_ARCH_DIR/bin"
        set -gx PATH "$TOOLCHAIN_ARCH_DIR/bin" $PATH
        
        # Save and set environment variables
        set -g SAVED_SSL_CERT_FILE "$SSL_CERT_FILE"
        set -g SAVED_REQUESTS_CA_BUNDLE "$REQUESTS_CA_BUNDLE"
        set -g SAVED_PYTHONNOUSERSITE "$PYTHONNOUSERSITE"
        set -g SAVED_PYTHONPATH "$PYTHONPATH"
        set -g SAVED_PYTHONHOME "$PYTHONHOME"

        # Set new environment variables
        if test -f "$TOOLCHAIN_ARCH_DIR/lib/python3.11/site-packages/certifi/cacert.pem"
            set -gx SSL_CERT_FILE "$TOOLCHAIN_ARCH_DIR/lib/python3.11/site-packages/certifi/cacert.pem"
            set -gx REQUESTS_CA_BUNDLE "$SSL_CERT_FILE"
        end
        
        set -gx PYTHONNOUSERSITE 1
        set -gx PYTHONPATH ""
        set -e PYTHONHOME

        # Set terminfo dirs on Linux
        if test "$SYS_TYPE" = "linux"; and test -d "$TOOLCHAIN_ARCH_DIR/share/terminfo"
            set -g SAVED_TERMINFO_DIRS "$TERMINFO_DIRS"
            set -gx TERMINFO_DIRS "$TOOLCHAIN_ARCH_DIR/share/terminfo"
        end
        
        echo "✓ FBT environment set up with toolchain in $TOOLCHAIN_ARCH_DIR/bin"
    else
        echo "⚠️  Toolchain not found at $TOOLCHAIN_ARCH_DIR"
        echo "💡 Using system binaries instead"
    end
end

function fbtenv_download_instructions
    echo "📥 To download the toolchain, run these commands in bash/zsh:"
    echo 
    echo "mkdir -p \"$FBT_TOOLCHAIN_PATH/toolchain\""
    echo "curl -L \"$TOOLCHAIN_URL\" -o \"$FBT_TOOLCHAIN_PATH/toolchain/toolchain.tar.gz\""
    echo "tar -xf \"$FBT_TOOLCHAIN_PATH/toolchain/toolchain.tar.gz\" -C \"$FBT_TOOLCHAIN_PATH/toolchain/\""
    
    # Extract expected directory name from URL 
    set -l expected_dir "gcc-arm-none-eabi-12.3-$ARCH_TYPE-$SYS_TYPE-flipper-$FBT_TOOLCHAIN_VERSION"
    
    echo "mv \"$FBT_TOOLCHAIN_PATH/toolchain/$expected_dir\" \"$TOOLCHAIN_ARCH_DIR\""
    echo 
    echo "🔄 After running these commands, re-source this script."
end

function fbtenv_check_toolchain
    if test -d "$TOOLCHAIN_ARCH_DIR/bin"
        return 0
    end
    return 1
end

# Main execution
if count $argv > 0; and test "$argv[1]" = "--restore"
    fbtenv_restore_env
else if fbtenv_check_if_sourced_multiple_times
    echo "FBT environment already set up"
else
    # Detect system
    fbtenv_detect_system
    
    # Check if toolchain exists
    if fbtenv_check_toolchain
        echo "✅ Found toolchain at $TOOLCHAIN_ARCH_DIR"
    else
        echo "⚠️  Toolchain not found at $TOOLCHAIN_ARCH_DIR"
        fbtenv_download_instructions
    end
    
    # Set up shell prompt
    fbtenv_set_shell_prompt
    
    # Set up environment
    fbtenv_setup_env
end
