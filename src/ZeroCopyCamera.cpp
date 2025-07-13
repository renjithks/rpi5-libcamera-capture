// ZeroCopyCamera.cpp
// Goal: Capture frames on Raspberry Pi 5 using libcamera with zero-copy DMA buffers.
// This minimal example sets up the pipeline, allocates dma-buf buffers, and logs buffer details on each frame.

#include <libcamera/libcamera.h>
#include <libcamera/camera_manager.h>
#include <libcamera/framebuffer_allocator.h>
#include <libcamera/request.h>
#include <libcamera/logging.h>
#include <iostream>
#include <memory>
#include <csignal>
#include <unistd.h>
#include <sys/mman.h>

using namespace libcamera;

static volatile bool running = true;

void signalHandler(int signal)
{
    running = false;
}

class ZeroCopyCameraApp {
public:
    bool initialize();
    void run();
    void shutdown();

private:
    std::unique_ptr<CameraManager> cameraManager_;
    std::shared_ptr<Camera> camera_;
    std::unique_ptr<CameraConfiguration> config_;
    std::unique_ptr<FrameBufferAllocator> allocator_;
    std::vector<std::unique_ptr<Request>> requests_;

    void requestComplete(Request *request);
};

bool ZeroCopyCameraApp::initialize()
{
    logSetTarget(LogTargetConsole);
    logSetLevel("*", LogLevelInfo);

    cameraManager_ = std::make_unique<CameraManager>();
    if (cameraManager_->start()) {
        std::cerr << "ERROR: Failed to start CameraManager." << std::endl;
        return false;
    }

    if (cameraManager_->cameras().empty()) {
        std::cerr << "ERROR: No cameras found." << std::endl;
        return false;
    }

    camera_ = cameraManager_->cameras()[0];
    if (camera_->acquire()) {
        std::cerr << "ERROR: Failed to acquire camera." << std::endl;
        return false;
    }

    config_ = camera_->generateConfiguration({ StreamRole::Viewfinder });
    config_->at(0).pixelFormat = formats::YUYV;
    config_->at(0).size = {640, 480};
    config_->validate();

    if (camera_->configure(config_.get())) {
        std::cerr << "ERROR: Failed to configure camera." << std::endl;
        return false;
    }

    Stream *stream = config_->at(0).stream();
    allocator_ = std::make_unique<FrameBufferAllocator>(camera_);

    if (allocator_->allocate(stream) < 0) {
        std::cerr << "ERROR: Failed to allocate DMA buffers." << std::endl;
        return false;
    }

    const auto &buffers = allocator_->buffers(stream);
    for (const std::unique_ptr<FrameBuffer> &buffer : buffers) {
        std::unique_ptr<Request> request = camera_->createRequest();
        if (!request)
            continue;

        if (request->addBuffer(stream, buffer.get()) < 0)
            continue;

        request->completed.connect(this, &ZeroCopyCameraApp::requestComplete);
        requests_.push_back(std::move(request));
    }

    camera_->requestCompleted.connect(this, &ZeroCopyCameraApp::requestComplete);
    if (camera_->start() < 0) {
        std::cerr << "ERROR: Failed to start camera." << std::endl;
        return false;
    }

    for (auto &req : requests_)
        camera_->queueRequest(req.get());

    return true;
}

void ZeroCopyCameraApp::run()
{
    std::cout << "[INFO] Running... Press Ctrl+C to stop." << std::endl;
    while (running) {
        usleep(100000); // Sleep 100ms
    }
}

void ZeroCopyCameraApp::shutdown()
{
    std::cout << "[INFO] Shutting down..." << std::endl;
    camera_->stop();
    camera_->release();
    cameraManager_->stop();
}

void ZeroCopyCameraApp::requestComplete(Request *request)
{
    if (request->status() == Request::RequestCancelled)
        return;

    for (auto &[stream, buffer] : request->buffers()) {
        if (buffer->planes().empty())
            continue;

        const FrameBuffer::Plane &plane = buffer->planes()[0];
        std::cout << "[INFO] Frame - fd: " << plane.fd.get()
                  << ", length: " << plane.length << std::endl;

        // Optionally map and inspect buffer (read-only)
        void *memory = mmap(nullptr, plane.length, PROT_READ, MAP_SHARED, plane.fd.get(), 0);
        if (memory != MAP_FAILED) {
            // ... Inspect pixel data if needed (not recommended in zero-copy path)
            munmap(memory, plane.length);
        }
    }

    camera_->queueRequest(request); // Reuse the request
}

int main()
{
    signal(SIGINT, signalHandler);

    ZeroCopyCameraApp app;
    if (!app.initialize())
        return 1;

    app.run();
    app.shutdown();
    return 0;
}
