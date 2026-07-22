# Setup OPTEE with QEMU for WSL v2

## Step 1: Update

```sudo apt update
sudo apt upgrade -y
```
## Step 2: Install dependencies

```
sudo apt install -y \
git curl wget unzip vim \
build-essential make cmake ninja-build \
gcc g++ \
python3 python3-pip python3-venv python3-cryptography python3-serial \
device-tree-compiler \
flex bison \
libssl-dev \
libelf-dev \
libncurses-dev \
libglib2.0-dev \
libpixman-1-dev \
libfdt-dev \
libgnutls28-dev \
zlib1g-dev \
uuid-dev \
expect \
cpio \
rsync \
bc \
xz-utils
```

- Verify Install using
```
gcc --version
make --version
python3 --version
git --version
```

