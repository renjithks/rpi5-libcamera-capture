#ifndef ZERO_COPY_CAMERA_HPP
#define ZERO_COPY_CAMERA_HPP

#include <libcamera/libcamera.h>
#include <libcamera/camera_manager.h>
#include <libcamera/framebuffer_allocator.h>
#include <libcamera/request.h>
#include <memory>
#include <vector>

class ZeroCopyCamera {
public:
    ZeroCopyCamera();
    ~ZeroCopyCamera();

    void start();

private:
    void requestComplete(libcamera::Request* request);

    std::unique_ptr<libcamera::CameraManager> manager_;
    std::shared_ptr<libcamera::Camera> camera_;
    std::unique_ptr<libcamera::CameraConfiguration> config_;
    libcamera::Stream* stream_;
    std::unique_ptr<libcamera::FrameBufferAllocator> allocator_;
    std::vector<std::unique_ptr<libcamera::Request>> requests_;
};

#endif // ZERO_COPY_CAMERA_HPP
