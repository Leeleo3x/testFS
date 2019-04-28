testfs
======

user level toy file system that is similar to ext3

## HOWTO

- Create a vagrant development environment: https://spdk.io/doc/vagrant.html

- Download source `git clone https://github.com/Leeleo3x/testFS`

- Build source 

```
mkdir build 
cd build
cmake ..
make
```

- Setup ENV: `sudo ./LibSPDK-prefix/src/LibSPDK/scripts/setup.sh`

- Run NVMe testFS: 

```
cd ./src
sudo ../LibSPDK-prefix/src/LibSPDK/scripts/gen_nvme.sh > config.conf
sudo ./testfs-bdev
```
