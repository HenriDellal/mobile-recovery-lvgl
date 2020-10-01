#define DEVICE_DEBUG
#ifdef DEVICE_DEBUG
#define SYSFS_BAT_PATH "/sys/class/power_supply/ACAD/online"
#endif
void mount_partitions();
void umount_partitions();

void mount_partitions() {
    if (system("ubiattach /dev/ubi_ctrl -m 3")) {
        system("mkdir -p /system && mount -t ubifs ubi0:system /system");
        system("mkdir -p /data && mount -t ubifs ubi0:userdata /data");
        system("mkdir -p /cache && mount -t ubifs ubi0:cache /cache");
        system("mkdir -p /boot && mount -t ubifs ubi0:boot /boot");
        system("mkdir -p /recovery && mount -t ubifs ubi0:recovery /recovery");
    }
}

void umount_partitions() {
    system("umount /system");
    system("umount /data");
    system("umount /cache");
    system("umount /boot");
    system("umount /recovery");
    system("ubidetach -m 3");
}
