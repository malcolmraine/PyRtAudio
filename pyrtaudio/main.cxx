#include <RtAudio.h>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace py = pybind11;

namespace {

class PyRtAudio {
 public:
  explicit PyRtAudio(RtAudio::Api api = RtAudio::UNSPECIFIED,
                     py::object errorCallback = py::none())
      : errorCallback_(std::move(errorCallback)) {
    audio_ = std::make_unique<RtAudio>(api, makeErrorCallback());
  }

  ~PyRtAudio() {
    if (audio_ && audio_->isStreamOpen()) {
      py::gil_scoped_release release;
      audio_->closeStream();
    }
  }

  RtAudio::Api getCurrentApi() { return audio_->getCurrentApi(); }
  unsigned int getDeviceCount() { return audio_->getDeviceCount(); }
  std::vector<unsigned int> getDeviceIds() { return audio_->getDeviceIds(); }
  std::vector<std::string> getDeviceNames() { return audio_->getDeviceNames(); }
  RtAudio::DeviceInfo getDeviceInfo(unsigned int deviceId) {
    return audio_->getDeviceInfo(deviceId);
  }
  unsigned int getDefaultOutputDevice() { return audio_->getDefaultOutputDevice(); }
  unsigned int getDefaultInputDevice() { return audio_->getDefaultInputDevice(); }

  py::tuple openStream(RtAudio::StreamParameters* outputParameters,
                       RtAudio::StreamParameters* inputParameters,
                       RtAudioFormat format, unsigned int sampleRate,
                       unsigned int bufferFrames, py::object callback,
                       py::object userData, RtAudio::StreamOptions* options) {
    if (!PyCallable_Check(callback.ptr())) {
      throw py::type_error("callback must be callable");
    }

    if (audio_->isStreamOpen()) {
      unsigned int actualBufferFrames = bufferFrames;
      const RtAudioErrorType result = audio_->openStream(
          outputParameters, inputParameters, format, sampleRate,
          &actualBufferFrames, nullptr, nullptr, options);
      return py::make_tuple(result, actualBufferFrames);
    }

    format_ = format;
    outputChannels_ = outputParameters ? outputParameters->nChannels : 0;
    inputChannels_ = inputParameters ? inputParameters->nChannels : 0;
    streamCallback_ = std::move(callback);
    userData_ = std::move(userData);

    unsigned int actualBufferFrames = bufferFrames;
    const RtAudioErrorType result = audio_->openStream(
        outputParameters, inputParameters, format, sampleRate,
        &actualBufferFrames, makeStreamCallback(), nullptr, options);
    if (result != RTAUDIO_NO_ERROR) {
      streamCallback_ = py::none();
      userData_ = py::none();
    }
    return py::make_tuple(result, actualBufferFrames);
  }

  void closeStream() {
    {
      py::gil_scoped_release release;
      audio_->closeStream();
    }
    streamCallback_ = py::none();
    userData_ = py::none();
  }

  RtAudioErrorType startStream() {
    py::gil_scoped_release release;
    return audio_->startStream();
  }

  RtAudioErrorType stopStream() {
    py::gil_scoped_release release;
    return audio_->stopStream();
  }

  RtAudioErrorType abortStream() {
    py::gil_scoped_release release;
    return audio_->abortStream();
  }

  const std::string& getErrorText() { return audio_->getErrorText(); }
  bool isStreamOpen() const { return audio_->isStreamOpen(); }
  bool isStreamRunning() const { return audio_->isStreamRunning(); }
  double getStreamTime() { return audio_->getStreamTime(); }
  void setStreamTime(double time) { audio_->setStreamTime(time); }
  long getStreamLatency() { return audio_->getStreamLatency(); }
  unsigned int getStreamSampleRate() { return audio_->getStreamSampleRate(); }

  void setErrorCallback(py::object callback) {
    if (!callback.is_none() && !PyCallable_Check(callback.ptr())) {
      throw py::type_error("errorCallback must be callable or None");
    }
    errorCallback_ = std::move(callback);
    audio_->setErrorCallback(makeErrorCallback());
  }

  void showWarnings(bool value = true) { audio_->showWarnings(value); }

 private:
  unsigned int formatBytes() const {
    switch (format_) {
      case RTAUDIO_SINT8:
        return 1;
      case RTAUDIO_SINT16:
        return 2;
      case RTAUDIO_SINT24:
      case RTAUDIO_SINT32:
      case RTAUDIO_FLOAT32:
        return 4;
      case RTAUDIO_FLOAT64:
        return 8;
      default:
        return 0;
    }
  }

  py::object bufferObject(void* buffer, unsigned int channels,
                          unsigned int frames, bool readOnly) const {
    if (buffer == nullptr || channels == 0) {
      return py::none();
    }
    const auto bytes = static_cast<py::ssize_t>(
        static_cast<size_t>(frames) * channels * formatBytes());
    return py::memoryview::from_memory(buffer, bytes, readOnly);
  }

  RtAudioCallback makeStreamCallback() {
    return [this](void* outputBuffer, void* inputBuffer, unsigned int nFrames,
                  double streamTime, RtAudioStreamStatus status,
                  void*) -> int {
      py::gil_scoped_acquire acquire;
      try {
        py::object result = streamCallback_(
            bufferObject(outputBuffer, outputChannels_, nFrames, false),
            bufferObject(inputBuffer, inputChannels_, nFrames, true), nFrames,
            streamTime, status, userData_);
        return result.cast<int>();
      } catch (py::error_already_set& error) {
        error.restore();
        PyErr_WriteUnraisable(streamCallback_.ptr());
        PyErr_Clear();
        return 2;
      }
    };
  }

  RtAudioErrorCallback makeErrorCallback() {
    if (errorCallback_.is_none()) {
      return {};
    }
    return [this](RtAudioErrorType type, const std::string& message) {
      py::gil_scoped_acquire acquire;
      try {
        errorCallback_(type, message);
      } catch (py::error_already_set& error) {
        error.restore();
        PyErr_WriteUnraisable(errorCallback_.ptr());
        PyErr_Clear();
      }
    };
  }

  py::object errorCallback_;
  py::object streamCallback_ = py::none();
  py::object userData_ = py::none();
  std::unique_ptr<RtAudio> audio_;
  RtAudioFormat format_ = RTAUDIO_FLOAT32;
  unsigned int outputChannels_ = 0;
  unsigned int inputChannels_ = 0;
};

std::vector<RtAudio::Api> getCompiledApis() {
  std::vector<RtAudio::Api> apis;
  RtAudio::getCompiledApi(apis);
  return apis;
}

}  // namespace

PYBIND11_MODULE(pyrtaudio, module) {
  module.doc() = "Python bindings for RtAudio 6.0.1";

  py::enum_<RtAudioErrorType>(module, "RtAudioErrorType")
      .value("RTAUDIO_NO_ERROR", RTAUDIO_NO_ERROR)
      .value("RTAUDIO_WARNING", RTAUDIO_WARNING)
      .value("RTAUDIO_UNKNOWN_ERROR", RTAUDIO_UNKNOWN_ERROR)
      .value("RTAUDIO_NO_DEVICES_FOUND", RTAUDIO_NO_DEVICES_FOUND)
      .value("RTAUDIO_INVALID_DEVICE", RTAUDIO_INVALID_DEVICE)
      .value("RTAUDIO_DEVICE_DISCONNECT", RTAUDIO_DEVICE_DISCONNECT)
      .value("RTAUDIO_MEMORY_ERROR", RTAUDIO_MEMORY_ERROR)
      .value("RTAUDIO_INVALID_PARAMETER", RTAUDIO_INVALID_PARAMETER)
      .value("RTAUDIO_INVALID_USE", RTAUDIO_INVALID_USE)
      .value("RTAUDIO_DRIVER_ERROR", RTAUDIO_DRIVER_ERROR)
      .value("RTAUDIO_SYSTEM_ERROR", RTAUDIO_SYSTEM_ERROR)
      .value("RTAUDIO_THREAD_ERROR", RTAUDIO_THREAD_ERROR)
      .export_values();

  py::class_<RtAudio::DeviceInfo>(module, "DeviceInfo")
      .def(py::init<>())
      .def_readwrite("ID", &RtAudio::DeviceInfo::ID)
      .def_readwrite("name", &RtAudio::DeviceInfo::name)
      .def_readwrite("outputChannels",
                     &RtAudio::DeviceInfo::outputChannels)
      .def_readwrite("inputChannels", &RtAudio::DeviceInfo::inputChannels)
      .def_readwrite("duplexChannels", &RtAudio::DeviceInfo::duplexChannels)
      .def_readwrite("isDefaultOutput",
                     &RtAudio::DeviceInfo::isDefaultOutput)
      .def_readwrite("isDefaultInput",
                     &RtAudio::DeviceInfo::isDefaultInput)
      .def_readwrite("sampleRates", &RtAudio::DeviceInfo::sampleRates)
      .def_readwrite("currentSampleRate",
                     &RtAudio::DeviceInfo::currentSampleRate)
      .def_readwrite("preferredSampleRate",
                     &RtAudio::DeviceInfo::preferredSampleRate)
      .def_readwrite("nativeFormats", &RtAudio::DeviceInfo::nativeFormats);

  py::class_<RtAudio::StreamParameters>(module, "StreamParameters")
      .def(py::init<>())
      .def_readwrite("deviceId", &RtAudio::StreamParameters::deviceId)
      .def_readwrite("nChannels", &RtAudio::StreamParameters::nChannels)
      .def_readwrite("firstChannel",
                     &RtAudio::StreamParameters::firstChannel);

  py::class_<RtAudio::StreamOptions>(module, "StreamOptions")
      .def(py::init<>())
      .def_readwrite("flags", &RtAudio::StreamOptions::flags)
      .def_readwrite("numberOfBuffers",
                     &RtAudio::StreamOptions::numberOfBuffers)
      .def_readwrite("streamName", &RtAudio::StreamOptions::streamName)
      .def_readwrite("priority", &RtAudio::StreamOptions::priority);

  auto rtAudio = py::class_<PyRtAudio>(module, "RtAudio");
  py::enum_<RtAudio::Api>(rtAudio, "Api")
      .value("UNSPECIFIED", RtAudio::UNSPECIFIED)
      .value("MACOSX_CORE", RtAudio::MACOSX_CORE)
      .value("LINUX_ALSA", RtAudio::LINUX_ALSA)
      .value("UNIX_JACK", RtAudio::UNIX_JACK)
      .value("LINUX_PULSE", RtAudio::LINUX_PULSE)
      .value("LINUX_OSS", RtAudio::LINUX_OSS)
      .value("WINDOWS_ASIO", RtAudio::WINDOWS_ASIO)
      .value("WINDOWS_WASAPI", RtAudio::WINDOWS_WASAPI)
      .value("WINDOWS_DS", RtAudio::WINDOWS_DS)
      .value("RTAUDIO_DUMMY", RtAudio::RTAUDIO_DUMMY)
      .value("NUM_APIS", RtAudio::NUM_APIS)
      .export_values();

  rtAudio
      .def(py::init<RtAudio::Api, py::object>(),
           py::arg("api") = RtAudio::UNSPECIFIED,
           py::arg("errorCallback") = py::none())
      .def_static("getVersion", &RtAudio::getVersion)
      .def_static("getCompiledApi", &getCompiledApis)
      .def_static("getApiName", &RtAudio::getApiName)
      .def_static("getApiDisplayName", &RtAudio::getApiDisplayName)
      .def_static("getCompiledApiByName", &RtAudio::getCompiledApiByName)
      .def_static("getCompiledApiByDisplayName",
                  &RtAudio::getCompiledApiByDisplayName)
      .def("getCurrentApi", &PyRtAudio::getCurrentApi)
      .def("getDeviceCount", &PyRtAudio::getDeviceCount)
      .def("getDeviceIds", &PyRtAudio::getDeviceIds)
      .def("getDeviceNames", &PyRtAudio::getDeviceNames)
      .def("getDeviceInfo", &PyRtAudio::getDeviceInfo, py::arg("deviceId"))
      .def("getDefaultOutputDevice", &PyRtAudio::getDefaultOutputDevice)
      .def("getDefaultInputDevice", &PyRtAudio::getDefaultInputDevice)
      .def("openStream", &PyRtAudio::openStream,
           py::arg("outputParameters") = nullptr,
           py::arg("inputParameters") = nullptr, py::arg("format"),
           py::arg("sampleRate"), py::arg("bufferFrames"), py::arg("callback"),
           py::arg("userData") = py::none(), py::arg("options") = nullptr)
      .def("closeStream", &PyRtAudio::closeStream)
      .def("startStream", &PyRtAudio::startStream)
      .def("stopStream", &PyRtAudio::stopStream)
      .def("abortStream", &PyRtAudio::abortStream)
      .def("getErrorText", &PyRtAudio::getErrorText)
      .def("isStreamOpen", &PyRtAudio::isStreamOpen)
      .def("isStreamRunning", &PyRtAudio::isStreamRunning)
      .def("getStreamTime", &PyRtAudio::getStreamTime)
      .def("setStreamTime", &PyRtAudio::setStreamTime, py::arg("time"))
      .def("getStreamLatency", &PyRtAudio::getStreamLatency)
      .def("getStreamSampleRate", &PyRtAudio::getStreamSampleRate)
      .def("setErrorCallback", &PyRtAudio::setErrorCallback,
           py::arg("errorCallback"))
      .def("showWarnings", &PyRtAudio::showWarnings, py::arg("value") = true);

  module.attr("Api") = module.attr("RtAudio").attr("Api");
  module.attr("RtAudio").attr("DeviceInfo") = module.attr("DeviceInfo");
  module.attr("RtAudio").attr("StreamParameters") =
      module.attr("StreamParameters");
  module.attr("RtAudio").attr("StreamOptions") = module.attr("StreamOptions");
  module.attr("RTAUDIO_SINT8") = RTAUDIO_SINT8;
  module.attr("RTAUDIO_SINT16") = RTAUDIO_SINT16;
  module.attr("RTAUDIO_SINT24") = RTAUDIO_SINT24;
  module.attr("RTAUDIO_SINT32") = RTAUDIO_SINT32;
  module.attr("RTAUDIO_FLOAT32") = RTAUDIO_FLOAT32;
  module.attr("RTAUDIO_FLOAT64") = RTAUDIO_FLOAT64;
  module.attr("RTAUDIO_NONINTERLEAVED") = RTAUDIO_NONINTERLEAVED;
  module.attr("RTAUDIO_MINIMIZE_LATENCY") = RTAUDIO_MINIMIZE_LATENCY;
  module.attr("RTAUDIO_HOG_DEVICE") = RTAUDIO_HOG_DEVICE;
  module.attr("RTAUDIO_SCHEDULE_REALTIME") = RTAUDIO_SCHEDULE_REALTIME;
  module.attr("RTAUDIO_ALSA_USE_DEFAULT") = RTAUDIO_ALSA_USE_DEFAULT;
  module.attr("RTAUDIO_JACK_DONT_CONNECT") = RTAUDIO_JACK_DONT_CONNECT;
  module.attr("RTAUDIO_INPUT_OVERFLOW") = RTAUDIO_INPUT_OVERFLOW;
  module.attr("RTAUDIO_OUTPUT_UNDERFLOW") = RTAUDIO_OUTPUT_UNDERFLOW;

  module.attr("RTAUDIO_VERSION_MAJOR") = RTAUDIO_VERSION_MAJOR;
  module.attr("RTAUDIO_VERSION_MINOR") = RTAUDIO_VERSION_MINOR;
  module.attr("RTAUDIO_VERSION_PATCH") = RTAUDIO_VERSION_PATCH;
  module.attr("RTAUDIO_VERSION_BETA") = RTAUDIO_VERSION_BETA;
  module.attr("RTAUDIO_VERSION") = RTAUDIO_VERSION;
}
