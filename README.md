# Lab 2 Syscalls & Directory listing \Output: myrls

## Team

 - [Yuriy Pasichnyk](https://github.com/Fenix-125)

## Prerequisites

 - **C++ compiler** - needs to support **C++17** standard
 - **CMake** 3.``15+
 
The rest prerequisites (such as development libraries) can be found in the
[packages file](./apt_packages.txt) in the form of the apt package manager package names.

## Installing

1. Clone the project.
    ```bash
    git clone git@github.com:Fenix-125/myrls.git
    ```
2. Install required libraries. On Ubuntu:
   ```bash
   sed 's/\r$//' apt_packages.txt | sed 's/#.*//' | xargs sudo apt-get install -y
   ```
3. Build.
    ```bash
    cmake -G"Unix Makefiles" -Bbuild
    cmake --build build
    ```

## Usage

```bash
myrls [-h|--help] [file_path]
```

For more detail see help. Help flags: `-h`/`--help`.