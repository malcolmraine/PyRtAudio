import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(__file__))
import pyrtaudio


class PyRtAudioTest(unittest.TestCase):
    def test_version_and_api_helpers(self):
        self.assertEqual(pyrtaudio.RTAUDIO_VERSION, "6.0.1")
        self.assertEqual(
            pyrtaudio.RtAudio.getVersion(), pyrtaudio.RTAUDIO_VERSION
        )

        compiled = pyrtaudio.RtAudio.getCompiledApi()
        self.assertTrue(compiled)
        for api in compiled:
            name = pyrtaudio.RtAudio.getApiName(api)
            display_name = pyrtaudio.RtAudio.getApiDisplayName(api)
            self.assertTrue(name)
            self.assertTrue(display_name)
            self.assertEqual(
                pyrtaudio.RtAudio.getCompiledApiByName(name), api
            )
            self.assertEqual(
                pyrtaudio.RtAudio.getCompiledApiByDisplayName(display_name),
                api,
            )

    def test_public_parameter_types_are_mutable(self):
        self.assertIs(
            pyrtaudio.RtAudio.DeviceInfo, pyrtaudio.DeviceInfo
        )
        device = pyrtaudio.RtAudio.DeviceInfo()
        device.ID = 7
        device.name = "test device"
        device.sampleRates = [44100, 48000]
        self.assertEqual(device.ID, 7)
        self.assertEqual(device.name, "test device")
        self.assertEqual(device.sampleRates, [44100, 48000])

        parameters = pyrtaudio.RtAudio.StreamParameters()
        parameters.deviceId = 7
        parameters.nChannels = 2
        parameters.firstChannel = 1
        self.assertEqual(parameters.nChannels, 2)

        options = pyrtaudio.RtAudio.StreamOptions()
        options.flags = (
            pyrtaudio.RTAUDIO_NONINTERLEAVED
            | pyrtaudio.RTAUDIO_MINIMIZE_LATENCY
        )
        options.numberOfBuffers = 3
        options.streamName = "pytest"
        self.assertEqual(options.numberOfBuffers, 3)

    def test_callback_and_open_stream_surface(self):
        audio = pyrtaudio.RtAudio()
        required_methods = {
            "getVersion",
            "getCompiledApi",
            "getApiName",
            "getApiDisplayName",
            "getCompiledApiByName",
            "getCompiledApiByDisplayName",
            "getCurrentApi",
            "getDeviceCount",
            "getDeviceIds",
            "getDeviceNames",
            "getDeviceInfo",
            "getDefaultOutputDevice",
            "getDefaultInputDevice",
            "openStream",
            "closeStream",
            "startStream",
            "stopStream",
            "abortStream",
            "getErrorText",
            "isStreamOpen",
            "isStreamRunning",
            "getStreamTime",
            "setStreamTime",
            "getStreamLatency",
            "getStreamSampleRate",
            "setErrorCallback",
            "showWarnings",
        }
        self.assertTrue(required_methods <= set(dir(audio)))
        self.assertIsInstance(audio.getCurrentApi(), pyrtaudio.Api)
        self.assertIsInstance(audio.getDeviceIds(), list)
        self.assertIsInstance(audio.getDeviceNames(), list)
        self.assertFalse(audio.isStreamOpen())

        result = audio.openStream(
            pyrtaudio.StreamParameters(),
            None,
            pyrtaudio.RTAUDIO_FLOAT32,
            48000,
            256,
            lambda *_: 0,
        )
        self.assertNotEqual(result[0], pyrtaudio.RTAUDIO_NO_ERROR)
        self.assertEqual(result[1], 256)


if __name__ == "__main__":
    unittest.main()
