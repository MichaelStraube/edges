# edges
Command-line tool to set up hot-corners.

With edges you can configure hot-corners independently from your Desktop Environment
or Window Manager. The commands to run on edge or corner hits can be passed as arguments
or read from a configuration file.

## Installation
### Install compiler and required libraries
#### debian / ubuntu
```
sudo apt install build-essential libx11-dev libxi-dev libxrandr-dev
```

### Build and install
Run make and make install from the project root directory. The default install
prefix is `/usr/local`.
```
make
sudo make install
```

### Install with different prefix
```
sudo make PREFIX=/usr install
```

## Usage
Basic usage example.
```
edges --top-left skippy-xd
```

To read commands from a file with the `--use-config` option you have to create
the file manually.
```
$HOME/.config/edges/edges.rc

# This is a comment
top-left = skippy-xd
top-right =
bottom-right =
bottom-left =
left =
top =
right =
bottom =
```
See `edges --help` or the man page for more info.

## Multi monitor
The multi monitor support is only experimental and not much tested.
Do not expect it to work properly in all setups.
