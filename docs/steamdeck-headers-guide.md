# Finding Steam Deck System Headers

This guide documents how to find the correct system header files for a Steam Deck system.

## System Information Needed

To find matching headers for your Steam Deck, you need to gather the following system information:

### 1. Kernel Version
```bash
uname -r
```
Example output: `6.11.11-valve14-1-neptune-611`

### 2. Distribution Information
```bash
cat /etc/os-release
```
Look for:
- `NAME` (should be "SteamOS")
- `VERSION_ID` (e.g., "3.7")
- `BUILD_ID` (e.g., "jupiter-3.7")

### 3. Architecture
```bash
uname -m
```
Expected output: `x86_64`

### 4. Package Manager Information
For SteamOS (Arch-based):
```bash
pacman -Q linux-neptune
```
This shows the exact kernel package version installed.

## Steps to Find Headers

1. **Identify your kernel version**: Use `uname -r` to get the exact kernel version
2. **Check SteamOS version**: Use `cat /etc/os-release` to identify the SteamOS build
3. **Construct the source URL**: The pattern is:
   ```
   https://steamdeck-packages.steamos.cloud/archlinux-mirror/sources/{BUILD_ID}/linux-neptune-{VERSION}.src.tar.gz
   ```

## Example

For a Steam Deck running:
- Kernel: `6.11.11-valve14-1-neptune-611`
- SteamOS: `jupiter-3.7`

The headers would be found at:
```
https://steamdeck-packages.steamos.cloud/archlinux-mirror/sources/jupiter-3.7/linux-neptune-611-6.11.11.valve14-1.src.tar.gz
```

## Download Command
```bash
wget https://steamdeck-packages.steamos.cloud/archlinux-mirror/sources/jupiter-3.7/linux-neptune-611-6.11.11.valve14-1.src.tar.gz
```

## Using the Headers

After downloading the source archive, extract and prepare it:

```bash
# Set repo root for clear paths
REPO_ROOT=$(pwd)

# Extract the archive
tar xvf linux-neptune-61-6.1.52.valve9-1.src.tar.gz

# Enter the directory
cd linux-neptune-61/

# Set up git repository
mv archlinux-linux-neptune .git
git --git-dir=.git --work-tree=. checkout -f master

# Export headers for userspace programs
make headers_install ARCH=x86_64 INSTALL_HDR_PATH=./usr

# Create external directory in project root and symlink headers
mkdir -p $REPO_ROOT/external
ln -sf $(pwd)/usr/include $REPO_ROOT/external/linux-headers
```

## Notes

- The source archive contains the full kernel source, including headers
- Extract the archive and look for header files in the appropriate directories
- Make sure the kernel version in the filename matches your `uname -r` output exactly
- The BUILD_ID from `/etc/os-release` determines which package repository to use
