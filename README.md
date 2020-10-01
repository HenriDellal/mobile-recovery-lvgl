# mobile-recovery-lvgl

This is a simple recovery for mobile devices based on LVGL.
This repository is based on [LVGL's PC Simulator](https://github.com/littlevgl/pc_simulator_sdl_eclipse) for easier testing.

## How to compile

Clone the repo and the related sub modules:

```
git clone --recursive https://github.com/HenriDellal/mobile-recovery-lvgl.git
```

Install SDL devel package from your distro.

Edit `device_config.h` and LVGL configs to match your needs.

Run `make` to build.

Launch `recovery`

## License

mobile-recovery-lvgl is licensed under GPLv2-or-later.
LVGL and LVGL Simulator is licensed under MIT License.
