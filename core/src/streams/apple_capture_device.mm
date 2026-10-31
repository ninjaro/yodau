#error "AVFoundation integration is deferred; see yodau/docs/apple_capture.md"

// Retained native implementation, excluded from canonical source discovery.
// Reactivation must replace the portable stubs through a verified generated
// build contract, not by hand-editing a generated CMakeLists.txt.
#include "streams/apple_capture_device.hpp"

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#include <TargetConditionals.h>

#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <exception>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace {

struct capture_state {
    std::mutex mtx;
    std::condition_variable wake;
    std::optional<yodau::core::frame> pending;
    std::string failure;
    std::size_t received { 0 };
    std::size_t dropped { 0 };
};

std::string to_string(NSString* value) {
    return value != nil ? std::string(value.UTF8String) : std::string();
}

std::string fourcc(const FourCharCode code) {
    std::string out;
    for (const int shift : { 24, 16, 8, 0 }) {
        const char ch = static_cast<char>((code >> shift) & 0xFF);
        out.push_back(ch >= 0x20 && ch < 0x7F ? ch : '?');
    }
    return out;
}

yodau::core::apple_capture_authorization
to_authorization(const AVAuthorizationStatus status) {
    using yodau::core::apple_capture_authorization;
    switch (status) {
    case AVAuthorizationStatusNotDetermined:
        return apple_capture_authorization::not_determined;
    case AVAuthorizationStatusRestricted:
        return apple_capture_authorization::restricted;
    case AVAuthorizationStatusDenied:
        return apple_capture_authorization::denied;
    case AVAuthorizationStatusAuthorized:
        return apple_capture_authorization::authorized;
    }
    return apple_capture_authorization::unavailable;
}

NSArray<AVCaptureDeviceType>* discovery_device_types() {
    NSMutableArray<AVCaptureDeviceType>* types = [NSMutableArray array];
    [types addObject:AVCaptureDeviceTypeBuiltInWideAngleCamera];
    if (@available(macOS 14.0, iOS 17.0, *)) {
        [types addObject:AVCaptureDeviceTypeExternal];
        [types addObject:AVCaptureDeviceTypeContinuityCamera];
    } else {
#if TARGET_OS_OSX
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        [types addObject:AVCaptureDeviceTypeExternalUnknown];
#pragma clang diagnostic pop
#endif
    }
#if TARGET_OS_OSX
    if (@available(macOS 13.0, *)) {
        [types addObject:AVCaptureDeviceTypeDeskViewCamera];
    }
#endif
    return types;
}

NSArray<AVCaptureDevice*>* discovered_devices() {
    AVCaptureDeviceDiscoverySession* discovery =
        [AVCaptureDeviceDiscoverySession
            discoverySessionWithDeviceTypes:discovery_device_types()
                                  mediaType:AVMediaTypeVideo
                                   position:AVCaptureDevicePositionUnspecified];
    return discovery.devices;
}

AVCaptureDeviceFormat*
closest_format(AVCaptureDevice* device, const int width, const int height) {
    const double wanted_pixels = static_cast<double>(width) * height;
    const double wanted_aspect
        = static_cast<double>(width) / static_cast<double>(height);
    AVCaptureDeviceFormat* best = nil;
    double best_pixel_error = 0.0;
    double best_aspect_error = 0.0;
    for (AVCaptureDeviceFormat* format in device.formats) {
        const CMVideoDimensions dims
            = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
        if (dims.width <= 0 || dims.height <= 0) {
            continue;
        }
        const double pixels = static_cast<double>(dims.width) * dims.height;
        const double pixel_error = std::abs(std::log(pixels / wanted_pixels));
        const double aspect_error = std::abs(std::log(
            (static_cast<double>(dims.width) / dims.height) / wanted_aspect
        ));
        if (best == nil || pixel_error < best_pixel_error - 1e-9
            || (std::abs(pixel_error - best_pixel_error) <= 1e-9
                && aspect_error < best_aspect_error)) {
            best = format;
            best_pixel_error = pixel_error;
            best_aspect_error = aspect_error;
        }
    }
    return best;
}

} // namespace

@interface yodau_capture_delegate
    : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
- (instancetype)initWithState:(std::shared_ptr<capture_state>)state;
@end

@implementation yodau_capture_delegate {
    std::shared_ptr<capture_state> state_;
}

- (instancetype)initWithState:(std::shared_ptr<capture_state>)state {
    self = [super init];
    if (self != nil) {
        state_ = std::move(state);
    }
    return self;
}

- (void)captureOutput:(AVCaptureOutput*)output
    didOutputSampleBuffer:(CMSampleBufferRef)sample
           fromConnection:(AVCaptureConnection*)connection {
    static_cast<void>(output);
    static_cast<void>(connection);
    CVImageBufferRef image = CMSampleBufferGetImageBuffer(sample);
    if (image == nullptr
        || CVPixelBufferGetPixelFormatType(image)
            != kCVPixelFormatType_32BGRA) {
        return;
    }

    yodau::core::frame frame_value;
    frame_value.format = yodau::core::pixel_format::bgra32;
    frame_value.ts = std::chrono::steady_clock::now();
    if (CVPixelBufferLockBaseAddress(image, kCVPixelBufferLock_ReadOnly)
        != kCVReturnSuccess) {
        return;
    }
    const auto* base
        = static_cast<const std::uint8_t*>(CVPixelBufferGetBaseAddress(image));
    frame_value.width = static_cast<int>(CVPixelBufferGetWidth(image));
    frame_value.height = static_cast<int>(CVPixelBufferGetHeight(image));
    frame_value.stride = static_cast<int>(CVPixelBufferGetBytesPerRow(image));
    if (base != nullptr && frame_value.height > 0 && frame_value.stride > 0) {
        const std::size_t bytes = static_cast<std::size_t>(frame_value.stride)
            * static_cast<std::size_t>(frame_value.height);
        frame_value.data.assign(base, base + bytes);
    }
    CVPixelBufferUnlockBaseAddress(image, kCVPixelBufferLock_ReadOnly);
    if (frame_value.data.empty()
        || !yodau::core::validate_frame_layout(frame_value)) {
        return;
    }

    {
        std::scoped_lock lock(state_->mtx);
        if (state_->pending.has_value()) {
            ++state_->dropped;
        }
        state_->pending = std::move(frame_value);
        ++state_->received;
    }
    state_->wake.notify_one();
}

- (void)captureOutput:(AVCaptureOutput*)output
    didDropSampleBuffer:(CMSampleBufferRef)sample
         fromConnection:(AVCaptureConnection*)connection {
    static_cast<void>(output);
    static_cast<void>(sample);
    static_cast<void>(connection);
    std::scoped_lock lock(state_->mtx);
    ++state_->dropped;
}

@end

bool yodau::core::apple_capture_available() noexcept { return true; }

yodau::core::apple_capture_authorization
yodau::core::apple_capture_authorization_status() {
    @autoreleasepool {
        return to_authorization([AVCaptureDevice
            authorizationStatusForMediaType:AVMediaTypeVideo]);
    }
}

yodau::core::apple_capture_authorization
yodau::core::request_apple_capture_authorization(
    const std::chrono::milliseconds timeout
) {
    const apple_capture_authorization current
        = apple_capture_authorization_status();
    if (current != apple_capture_authorization::not_determined) {
        return current;
    }

    struct request_state {
        std::mutex mtx;
        std::condition_variable answered;
        bool done { false };
    };

    auto state = std::make_shared<request_state>();
    @autoreleasepool {
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo
                                 completionHandler:^(BOOL granted) {
                                     static_cast<void>(granted);
                                     {
                                         std::scoped_lock lock(state->mtx);
                                         state->done = true;
                                     }
                                     state->answered.notify_all();
                                 }];
    }
    {
        std::unique_lock lock(state->mtx);
        state->answered.wait_for(lock, timeout, [&] { return state->done; });
    }
    return apple_capture_authorization_status();
}

std::vector<yodau::core::apple_capture_device_info>
yodau::core::list_apple_capture_devices() {
    std::vector<apple_capture_device_info> devices;
    @autoreleasepool {
        for (AVCaptureDevice* device in discovered_devices()) {
            apple_capture_device_info info;
            info.unique_id = to_string(device.uniqueID);
            info.name = to_string(device.localizedName);
            info.model_id = to_string(device.modelID);
            info.device_type = to_string(device.deviceType);
            info.connected = device.connected;
#if TARGET_OS_OSX
            info.suspended = device.suspended;
            info.in_use_by_other_application = device.inUseByAnotherApplication;
#endif
            for (AVCaptureDeviceFormat* format in device.formats) {
                const CMVideoDimensions dims
                    = CMVideoFormatDescriptionGetDimensions(
                        format.formatDescription
                    );
                apple_capture_format entry;
                entry.width = dims.width;
                entry.height = dims.height;
                entry.pixel_format = fourcc(
                    CMFormatDescriptionGetMediaSubType(format.formatDescription)
                );
                bool first_range = true;
                for (AVFrameRateRange* range in format
                         .videoSupportedFrameRateRanges) {
                    entry.min_fps = first_range
                        ? range.minFrameRate
                        : std::min(entry.min_fps, range.minFrameRate);
                    entry.max_fps = first_range
                        ? range.maxFrameRate
                        : std::max(entry.max_fps, range.maxFrameRate);
                    first_range = false;
                }
                info.formats.push_back(std::move(entry));
            }
            devices.push_back(std::move(info));
        }
    }
    return devices;
}

namespace {

// Runs one capture session. Returns an empty string after a requested stop,
// otherwise the failure reason. Exceptions from `on_frame` are captured in
// `consumer_error` so they never unwind through an autorelease pool.
std::string run_session(
    const std::string& path, const std::string& unique_id,
    const std::function<void(yodau::core::frame&&)>& on_frame,
    const yodau::core::stop_token& token,
    const yodau::core::apple_capture_options& options,
    std::exception_ptr& consumer_error
) {
    using namespace std::chrono;

    auto state = std::make_shared<capture_state>();
    AVCaptureSession* session = nil;
    AVCaptureVideoDataOutput* output = nil;
    // Declared before the guard so it outlives the session teardown.
    yodau_capture_delegate* delegate = nil;
    dispatch_queue_t queue = nil;
    id runtime_observer = nil;
    id disconnect_observer = nil;
    AVCaptureDevice* locked_device = nil;

    struct cleanup_guard {
        AVCaptureSession* __strong& session;
        AVCaptureVideoDataOutput* __strong& output;
        dispatch_queue_t __strong& queue;
        id __strong& runtime_observer;
        id __strong& disconnect_observer;
        AVCaptureDevice* __strong& locked_device;

        ~cleanup_guard() {
            if (locked_device != nil) {
                [locked_device unlockForConfiguration];
            }
            if (session != nil && session.running) {
                [session stopRunning];
            }
            if (output != nil) {
                [output setSampleBufferDelegate:nil queue:nil];
            }
            if (queue != nil) {
                dispatch_sync(queue, ^ {});
            }
            if (runtime_observer != nil) {
                [NSNotificationCenter.defaultCenter
                    removeObserver:runtime_observer];
            }
            if (disconnect_observer != nil) {
                [NSNotificationCenter.defaultCenter
                    removeObserver:disconnect_observer];
            }
        }
    } cleanup {
        session,      output, queue, runtime_observer, disconnect_observer,
        locked_device
    };

    AVCaptureDevice* device = [AVCaptureDevice
        deviceWithUniqueID:[NSString stringWithUTF8String:unique_id.c_str()]];
    if (device == nil) {
        return "AVFoundation camera '" + path + "' is not connected";
    }

    NSError* error = nil;
    AVCaptureDeviceInput* input =
        [AVCaptureDeviceInput deviceInputWithDevice:device error:&error];
    if (input == nil) {
        return "cannot open AVFoundation camera '"
            + to_string(device.localizedName)
            + "': " + to_string(error.localizedDescription);
    }

    session = [[AVCaptureSession alloc] init];
    output = [[AVCaptureVideoDataOutput alloc] init];
    output.videoSettings =
        @{(id)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA)};
    output.alwaysDiscardsLateVideoFrames = YES;
    queue = dispatch_queue_create(
        "yodau.core.apple_capture", DISPATCH_QUEUE_SERIAL
    );
    delegate = [[yodau_capture_delegate alloc] initWithState:state];
    [output setSampleBufferDelegate:delegate queue:queue];

    [session beginConfiguration];
    if (![session canAddInput:input] || ![session canAddOutput:output]) {
        [session commitConfiguration];
        return "AVFoundation session rejected camera '"
            + to_string(device.localizedName) + "'";
    }
    [session addInput:input];
    [session addOutput:output];
    if (options.preferred_width > 0 && options.preferred_height > 0) {
        AVCaptureDeviceFormat* format = closest_format(
            device, options.preferred_width, options.preferred_height
        );
        if (format != nil && [device lockForConfiguration:&error]) {
            device.activeFormat = format;
            // Keep the lock until the session runs so its preset cannot
            // replace the selected format.
            locked_device = device;
        }
    }
    [session commitConfiguration];

    const std::weak_ptr<capture_state> weak_state = state;
    const auto fail = [weak_state](std::string reason) {
        if (const auto locked = weak_state.lock()) {
            {
                std::scoped_lock lock(locked->mtx);
                if (locked->failure.empty()) {
                    locked->failure = std::move(reason);
                }
            }
            locked->wake.notify_all();
        }
    };
    runtime_observer = [NSNotificationCenter.defaultCenter
        addObserverForName:AVCaptureSessionRuntimeErrorNotification
                    object:session
                     queue:nil
                usingBlock:^(NSNotification* note) {
                    NSError* runtime_error
                        = note.userInfo[AVCaptureSessionErrorKey];
                    fail(
                        "AVFoundation capture failed: "
                        + to_string(runtime_error.localizedDescription)
                    );
                }];
    const std::string device_name = to_string(device.localizedName);
    disconnect_observer = [NSNotificationCenter.defaultCenter
        addObserverForName:AVCaptureDeviceWasDisconnectedNotification
                    object:device
                     queue:nil
                usingBlock:^(NSNotification* note) {
                    static_cast<void>(note);
                    fail(
                        "AVFoundation camera '" + device_name + "' disconnected"
                    );
                }];

    [session startRunning];
    if (locked_device != nil) {
        [locked_device unlockForConfiguration];
        locked_device = nil;
    }

    const auto first_frame_deadline
        = steady_clock::now() + options.first_frame_timeout;
    bool have_frame = false;
    while (!token.stop_requested()) {
        std::optional<yodau::core::frame> next;
        {
            std::unique_lock lock(state->mtx);
            state->wake.wait_for(lock, milliseconds(100), [&] {
                return state->pending.has_value() || !state->failure.empty();
            });
            if (!state->failure.empty()) {
                return state->failure;
            }
            if (state->pending.has_value()) {
                next = std::move(state->pending);
                state->pending.reset();
            }
        }
        if (!next.has_value()) {
            if (!have_frame && steady_clock::now() >= first_frame_deadline) {
                return "AVFoundation camera '" + device_name
                    + "' produced no frames";
            }
            continue;
        }
        have_frame = true;
        try {
            on_frame(std::move(*next));
        } catch (...) {
            consumer_error = std::current_exception();
            return {};
        }
    }
    return {};
}

} // namespace

void yodau::core::run_apple_capture(
    const std::string& path, const std::function<void(frame&&)>& on_frame,
    const stop_token& token, const apple_capture_options& options
) {
    const auto unique_id = resolve_apple_capture_device(path);
    if (!unique_id.has_value()) {
        throw std::runtime_error(
            "AVFoundation camera '" + path + "' is not connected"
        );
    }

    apple_capture_authorization access = apple_capture_authorization_status();
    if (access == apple_capture_authorization::not_determined) {
        access
            = request_apple_capture_authorization(options.authorization_timeout
            );
    }
    if (access != apple_capture_authorization::authorized) {
        throw std::runtime_error(
            "camera access is "
            + std::string(apple_capture_authorization_name(access))
            + "; allow it in System Settings > Privacy & Security > Camera"
        );
    }

    std::exception_ptr consumer_error;
    std::string failure;
    @autoreleasepool {
        failure = run_session(
            path, *unique_id, on_frame, token, options, consumer_error
        );
    }
    if (consumer_error) {
        std::rethrow_exception(consumer_error);
    }
    if (!failure.empty()) {
        throw std::runtime_error(failure);
    }
}
