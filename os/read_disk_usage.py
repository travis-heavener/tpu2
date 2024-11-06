import os
import sys

"""

This script just prints how much of the given disk is in use.

"""

SECTOR_SIZE = 64

if __name__ == "__main__":
    SIZE = 1024 * 64 # 64 KiB max
    # generate enough zeros to fill the file
    if len(sys.argv) < 2:
        print("Invalid usage: <executable> <drive-name>")
        exit(1)

    # read first 128 bytes of file
    drive_name = sys.argv[1]
    script_dir = os.path.dirname( os.path.realpath(__file__) )
    drive_path = script_dir + "/" + drive_name + ".dsk"

    sectors_used = 0
    with open(drive_path, "rb") as f:
        for i in range(128):
            byte = f.read(1)[0] # read 1 byte

            for _ in range(8):
                if byte & 1 == 1:
                    sectors_used += 1
                byte >>= 1

        # close up
        f.close()

    # log results
    drive_size = os.path.getsize(drive_path)
    space_used = sectors_used * SECTOR_SIZE
    perc_used = round(space_used / drive_size * 100, 1)

    print(f"{'Disk Name:': <12} {drive_name: >8}")
    print(f"{'Total Size:': <12} {drive_size: >6} B")
    print(f"{'Space Used:': <12} {space_used: >6} B")
    print(f"{'Perc. Used:': <12} {perc_used: >7}%")