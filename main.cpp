#include <iostream>
#include <iostream>
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <libusb.h>
#include <chrono>

long long currentTimeMillis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

void ToggleMute(bool mute) {
    HRESULT hr;
    IMMDeviceEnumerator* deviceEnumerator = NULL;
    IMMDevice* defaultDevice = NULL;
    IAudioEndpointVolume* endpointVolume = NULL;

    CoInitialize(NULL);

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER, __uuidof(IMMDeviceEnumerator), (LPVOID*)&deviceEnumerator);
    if (FAILED(hr)) {
        std::cerr << "Failed to create device enumerator." << std::endl;
        goto cleanup;
    }

    hr = deviceEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &defaultDevice);
    if (FAILED(hr)) {
        std::cerr << "Failed to get default audio capture device." << std::endl;
        goto cleanup;
    }

    hr = defaultDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, NULL, (LPVOID*)&endpointVolume);
    if (FAILED(hr)) {
        std::cerr << "Failed to get audio endpoint volume." << std::endl;
        goto cleanup;
    }

    hr = endpointVolume->SetMute(mute, NULL);
    if (FAILED(hr)) {
        std::cerr << "Failed to set mute state." << std::endl;
        goto cleanup;
    }

cleanup:
    if (endpointVolume) endpointVolume->Release();
    if (defaultDevice) defaultDevice->Release();
    if (deviceEnumerator) deviceEnumerator->Release();
    CoUninitialize();
}

int main() {
    HWND hWnd = GetConsoleWindow();
    ShowWindow(hWnd, SW_HIDE);

    libusb_context* ctx = nullptr;
    libusb_device_handle* handle = nullptr;
    int res = libusb_init(&ctx);

    if (res < 0) {
        std::cerr << "Failed to initialize libusb." << std::endl;
        return 1;
    }

    const uint16_t vendor_id = 0x1532;
    const uint16_t product_id = 0x0520;

    handle = libusb_open_device_with_vid_pid(ctx, vendor_id, product_id);
    if (!handle) {
        std::cerr << "Could not open USB device." << std::endl;
        libusb_exit(ctx);
        return 1;
    }

    if (libusb_kernel_driver_active(handle, 5) == 1) {
        if (libusb_detach_kernel_driver(handle, 5) != 0) {
            std::cerr << "Could not detach kernel driver." << std::endl;
            libusb_close(handle);
            libusb_exit(ctx);
            return 1;
        }
    }

    res = libusb_claim_interface(handle, 5);
    if (res < 0) {
        std::cerr << "Failed to claim interface." << std::endl;
        libusb_close(handle);
        libusb_exit(ctx);
        return 1;
    }

    unsigned char data[37];
    int actual_length;
    int endpoint_address = 0x84;

    long long lastButtonPressTime = 0;
    const long long doublePressInterval = 250;
    bool doublePressDetected = false;

    while (true) {
        res = libusb_interrupt_transfer(handle, endpoint_address, data, sizeof(data), &actual_length, 5000);
        if (res == 0) {
            if ((data[0] == 2 && data[1] == 0 && data[2] == 0) || (data[0] == 2 && data[1] == 8 && data[2] == 0)) {
                long long currentTime = currentTimeMillis();
                if (currentTime - lastButtonPressTime <= doublePressInterval) {
                    doublePressDetected = true;
                    
                }
                else {
                    doublePressDetected = false;
                }
                lastButtonPressTime = currentTime;

                if (!doublePressDetected) {
                    if (data[1] == 0) {
                        ToggleMute(false);
                    }
                    else if (data[1] == 8) {
                        ToggleMute(true);
                        Beep(1000, 10);
                    }
                }
            }
        }
        else if (res == LIBUSB_ERROR_TIMEOUT) {
            // pass
        }
        else {
            // error with reading device
            break;
        }
    }

    libusb_release_interface(handle, 5);
    libusb_close(handle);
    libusb_exit(ctx);
    return 0;
}
