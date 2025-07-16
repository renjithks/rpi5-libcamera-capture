#include "ZeroCopyCamera.hpp"
#include <iostream>
#include <sys/mman.h>
#include <unistd.h>

ZeroCopyCamera::ZeroCopyCamera() : camera_(nullptr), stream_(nullptr) {}

ZeroCopyCamera::~ZeroCopyCamera() {
    shutdown();
}

bool ZeroCopyCamera::initialize() {
    manager_ = std::make_unique<libcamera::CameraManager>();
    manager_->start();

    if (manager_->cameras().empty()) {
        std::cerr << "[ERROR] No cameras found." << std::endl;
        return false;
    }

    camera_ = manager_->cameras()[0];
    camera_->acquire();

    config_ = camera_->generateConfiguration({ libcamera::StreamRole::VideoRecording });
    config_->at(0).pixelFormat = libcamera::formats::YUYV;
    config_->at(0).size = { 640, 480 };
    config_->at(0).bufferCount = 4;
    config_->validate();
    camera_->configure(config_.get());

    stream_ = config_->at(0).stream();
    allocator_ = std::make_unique<libcamera::FrameBufferAllocator>(camera_);
    allocator_->allocate(stream_);

    std::cout << "[INIT] Allocated " << allocator_->buffers(stream_).size() << " buffers." << std::endl;

    for (const std::unique_ptr<libcamera::FrameBuffer>& buffer : allocator_->buffers(stream_)) {
        std::unique_ptr<libcamera::Request> request = camera_->createRequest();
        if (!request) {
            std::cerr << "[ERROR] Failed to create request." << std::endl;
            continue;
        }

        if (request->addBuffer(stream_, buffer.get()) < 0) {
            std::cerr << "[ERROR] Failed to add buffer to request." << std::endl;
            continue;
        }

        std::cout << "[REQ] Created and added buffer to request ID: " << request->cookie() << std::endl;
        requests_.emplace_back(std::move(request));
    }

    camera_->requestCompleted.connect(this, &ZeroCopyCamera::requestComplete);

    return true;
}

void ZeroCopyCamera::shutdown() {
    if (camera_) {
        camera_->stop();
        camera_->release();
    }
    if (manager_)
        manager_->stop();
}

void ZeroCopyCamera::start() {
    camera_->start();
    std::cout << "Camera initialized and started." << std::endl;

    for (auto& request : requests_) {
        std::cout << "[QUEUE] Queueing request ID: " << request->cookie() << std::endl;
        camera_->queueRequest(request.get());
    }
}

void ZeroCopyCamera::requestComplete(libcamera::Request* request) {
    if (request->status() == libcamera::Request::RequestCancelled) {
        std::cerr << "[WARN] Request ID: " << request->cookie() << " was cancelled." << std::endl;
        return;
    }

    const auto& buffers = request->buffers();
    if (buffers.empty()) {
        std::cerr << "[ERROR] Request ID: " << request->cookie() << " has no buffers." << std::endl;
        return;
    }

    for (const auto& [stream, buffer] : buffers) {
        if (buffer->planes().empty()) {
            std::cerr << "[ERROR] Buffer has no planes." << std::endl;
            continue;
        }

        const auto& plane = buffer->planes()[0];
        std::cout << "[COMPLETE] Request ID: " << request->cookie()
                  << ", Buffer ID: " << buffer->cookie()
                  << ", Plane Length: " << plane.length << std::endl;

        void* memory = mmap(nullptr, plane.length, PROT_READ, MAP_SHARED, plane.fd.get(), 0);
        if (memory == MAP_FAILED) {
            std::cerr << "[ERROR] mmap failed." << std::endl;
            continue;
        }

        std::cout << "[INFO] Frame received. Length: " << plane.length << std::endl;
        munmap(memory, plane.length);
    }

    std::cout << "[QUEUE] Requeueing request ID: " << request->cookie() << std::endl;
    request->reuse(libcamera::Request::ReuseBuffers);
    camera_->queueRequest(request);
}
