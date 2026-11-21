#include "utils/ums.hpp"

#ifdef USE_LIBUSBHSFS
#include <usbhsfs.h>

int Ums::init() {
    Result rc = usbHsFsInitialize(0);
    if (R_FAILED(rc)) return rc;
    usbHsFsSetPopulateCallback(
        [](const UsbHsFsDevice *devices, u32 device_count, void *user_data) {
            auto *self = static_cast<Ums *>(user_data);
            DeviceList ndev;
            ndev.reserve(device_count + 1);
            ndev.push_back({.id = -1, .name = "SD Card", .mount = "sdmc:"});

            for (u32 i = 0; i < device_count; ++i) {
                auto &d = devices[i];

                std::string name;
                if (auto sv = std::string_view(d.product_name); !sv.empty())
                    name = sv;
                else if (sv = std::string_view(d.manufacturer); !sv.empty())
                    name = sv;
                else if (sv = std::string_view(d.serial_number); !sv.empty())
                    name = sv;
                else
                    name = "Unnamed device";
                ndev.push_back({.id = d.usb_if_id, .name = std::move(name), .mount = d.name});
            }
            self->devices = std::move(ndev);
            self->event.fire(self->devices);
        },
        this);

    if (!usbHsFsGetMountedDeviceCount()) {
        this->devices.push_back({.id = -1, .name = "SD Card", .mount = "sdmc:"});
    }

    brls::Application::getExitEvent()->subscribe([this]() {
        usbHsFsSetPopulateCallback(nullptr, nullptr);
        for (auto &dev : this->devices)
            if (dev.id >= 0) this->unmount(dev);
        this->devices.clear();
        usbHsFsExit();
    });
    return 0;
}

bool Ums::unmount(const Device &dev) {
    UsbHsFsDevice d = {.usb_if_id = dev.id};
    return usbHsFsUnmountDevice(&d, true);
}

#else

#include <cstdlib>

// generic desktop (macOS/Linux): expose the user's home directory as a
// browsable root so the Files tab is usable off-Switch (Windows already
// pins "My Videos" above). Pinned folders (config) are appended on top.
int Ums::init() {
    const char* home = std::getenv("HOME");
    if (home && *home) this->devices.push_back({.id = -1, .name = "Home", .mount = home});
    return 0;
}

bool Ums::unmount(const Device& dev) { return false; }

#endif