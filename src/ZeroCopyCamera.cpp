#include "ZeroCopyCamera.hpp"
#include <iostream>
#include <libcamera/formats.h>
#include <libcamera/control_ids.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

using namespace libcamera;

bool ZeroCopyCamera::initialize()
{
    manager_ = std::make_unique<CameraManager>();
    manager_->start();

    if (manager_->cameras().empty()) {
        std::cerr << "No cameras found.\n";
        return false;
    }

    camera_ = manager_->cameras()[0];
    camera_->acquire();

    config_ = camera_->generateConfiguration({ StreamRole::Viewfinder });
    config_->at(0).pixelFormat = formats::YUYV;
    config_->at(0).size = { 640, 480 };
    config_->validate();

    if (camera_->configure(config_.get()) < 0) {
        std::cerr << "Failed to configure camera.\n";
        return false;
    }

    allocator_ = std::make_unique<FrameBufferAllocator>(camera_);
    Stream *stream = config_->at(0).stream();
    if (allocator_->allocate(stream) < 0) {
        std::cerr << "Failed to allocate buffers.\n";
        return false;
    }

    for (const std::unique_ptr<FrameBuffer> &buffer : allocator_->buffers(stream)) {
        std::unique_ptr<Request> request = camera_->createRequest();
        if (!request) continue;

        if (request->addBuffer(stream, buffer.get()) < 0) continue;
        requests_.push_back(std::move(request));
    }

    // Connect signal properly
    camera_->requestCompleted.connect(this, &ZeroCopyCamera::requestComplete);

    if (camera_->start() < 0) {
        std::cerr << "Camera start failed.\n";
        return false;
    }

    for (auto &req : requests_) {
        if (camera_->queueRequest(req.get()) < 0)
            std::cerr << "Failed to queue request.\n";
    }

    std::cout << "Camera initialized and started.\n";
    return true;
}

void ZeroCopyCamera::requestComplete(Request *request)
{
    if (request->status() == Request::RequestCancelled)
        return;

    const auto &buffers = request->buffers();
    for (const auto &[stream, buffer] : buffers) {
        if (buffer->planes().empty()) continue;
        const auto &plane = buffer->planes()[0];

        void *memory = mmap(nullptr, plane.length, PROT_READ, MAP_SHARED, plane.fd.get(), 0);
        if (memory == MAP_FAILED) {
            std::cerr << "mmap failed.\n";
            continue;
        }

        std::cout << "[INFO] Frame received. Length: " << plane.length << "\n";

        munmap(memory, plane.length);
    }

    // Reset request and reattach buffer before requeuing
    request->reuse();  // clears internal state and prepares for reuse
    camera_->queueRequest(request);  // safe now
}

void ZeroCopyCamera::shutdown()
{
    if (camera_) {
        camera_->stop();
        camera_->release();
    }
    manager_->stop();
}
