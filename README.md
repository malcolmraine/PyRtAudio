# PyRtAudio

Python bindings for the public C++ API of RtAudio 6.0.1, built as a pybind11
extension with Bazel.

## Build and test

```bash
bazel build //pyrtaudio:pyrtaudio
bazel test //pyrtaudio:pyrtaudio_test
```

The Bazel module selects the active local `python3` toolchain and additionally
configures a Python 3.15 fallback. The extension ABI must match the Python
interpreter used to import it; rebuild after changing Python environments.

The extension target is emitted as `.so` or `.pyd` for the current platform.
The native CoreAudio, ALSA, or WASAPI backend is selected on macOS, Linux, or
Windows respectively.

## Python API

The module exposes `RtAudio`, `RtAudio.Api`, `RtAudioErrorType`,
`DeviceInfo`, `StreamParameters`, and `StreamOptions`, together with RtAudio's
format, stream flag, stream status, error, and version constants.

`openStream` uses a Python callable for the stream callback. Error callbacks
can be passed to the `RtAudio` constructor or `setErrorCallback`. Buffer
arguments are flat Python `memoryview` objects in native byte order; input
buffers are read-only and output buffers are writable. Cast a view to the
selected sample type when appropriate, taking interleaving into account.

Because immutable Python integers cannot be updated like the C++
`unsigned int *bufferFrames` argument, `openStream` returns:

```python
(error, actual_buffer_frames)
```

`StreamOptions.numberOfBuffers` is updated in place when RtAudio changes it.
