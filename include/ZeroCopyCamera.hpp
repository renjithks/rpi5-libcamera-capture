#pragma once

#include <libcamera/libcamera.h>
#include <libcamera/camera_manager.h>
#include <libcamera/framebuffer_allocator.h>

#include <memory>
#include <vector>

class ZeroCopyCamera {
public:
    ZeroCopyCamera();
    ~ZeroCopyCamera();
    bool initialize();
    void shutdown();

private:
    void requestComplete(libcamera::Request *request);

    std::unique_ptr<libcamera::CameraManager> manager_;
    std::shared_ptr<libcamera::Camera> camera_;
    std::unique_ptr<libcamera::CameraConfiguration> config_;
    std::unique_ptr<libcamera::FrameBufferAllocator> allocator_;
    std::vector<std::unique_ptr<libcamera::Request>> requests_;
};
