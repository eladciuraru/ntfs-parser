# NTFS Parser
Simple single header NTFS read-only parser library


# Compile
```shell
$ shell.bat
$ build.bat
```


# Resources
* [NTFS Overview](http://ntfs.com/ntfs_basics.htm)
* [NTFS - Wikipedia](https://en.wikipedia.org/wiki/NTFS)
* [NTFS Documentation](http://ftp.kolibrios.org/users/Asper/docs/NTFS/ntfsdoc.html)


# Create NTFS VHD
```shell
$ diskpart
$ create vdisk file=... type=fixed maximum=32
$ select vdisk file=...
$ attach vdisk
$ create partition primary
$ format fs=ntfs quick
$ exit
```
