# edges
Command-line tool to set up hot-corners.

With edges you can configure hot-corners independently from your Desktop Environment
or Window Manager. The commands to run on edge or corner hits can be passed as arguments
or read from a configuration file.

## Installation
### Install Rust and required libraries
#### openSUSE Tumbleweed
```
sudo zypper install cargo libX11-devel libXi-devel libXrandr-devel
```

### Build and install
Run cargo build from the project root directory.
```
cargo build --release
```
On FreeBSD you need to set rustflags to avoid linking errors.
```
RUSTFLAGS="-L/usr/local/lib" cargo build --release
```

Install to `/usr/local`.
```
sudo mkdir -p /usr/local/bin
sudo cp target/release/edges /usr/local/bin

sudo mkdir -p /usr/local/man/man1
sudo cp man/edges.1 /usr/local/man/man1
```

## Usage
Basic usage example.
```
edges --topleft skippy-xd
```

To read commands from a file with the `--config` option you have to create
the file manually.
```
$HOME/.config/edges.conf

[Commands]
topleft = skippy-xd
topright =
bottomright =
bottomleft =
left =
top =
right =
bottom =
```
See `edges --help` or the man page for more info.

## Multi monitor
The multi monitor support is only experimental and not much tested.
Do not expect it to work properly in all setups.
