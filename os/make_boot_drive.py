import os
import sys

"""

This small script is responsible for creating a boot drive for the TPU.

The reserved boot partition spans from 0x0000 to 0x03FF (1 KiB).
Free partition starts at 0x0400 and spans to 0xFFFF (63 KiB).

"""

if __name__ == "__main__":
    SIZE = 1024 * 64 # 64 KiB max
    # generate enough zeros to fill the file
    if len(sys.argv) < 2:
        print("Invalid usage: <executable> <drive-name>")
        exit(1)

    # literally write n KiB worth of zeros
    drive_name = sys.argv[1]
    script_dir = os.path.dirname( os.path.realpath(__file__) )
    drive_path = script_dir + "/" + drive_name + ".dsk"

    # thanks to https://stackoverflow.com/a/8816154
    with open(drive_path, "wb") as f:
        f.seek(SIZE - 1)
        f.write(b"\0")
        f.close()

    # write boot instructions
    with open(drive_path, "r+b") as f:
        # mark the first sector as in use (boot process only needs one sector)
        f.seek(0)
        f.write(b"\x03") # mark the first two sectors as in use (for the sector map)

        # close the file handle
        f.close()

    # load the OS image to the drive
    # TODO

    print("Disk created: " + drive_path)