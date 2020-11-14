# Sample-trig

Sample-trig is demonstration software of how audio samples could be triggered using library ALSA, mqueue and sndfile.
Audio samples are triggered with events read from the Linux standard input or the keyboard if running on Linux shell.

## Depedency
Sample-trig uses following library:
- libsndfile to read audo file
- libasound to playback audio file into ALSA pcm device

## Compiling
```
git clone https://github.com/maxmbed/sample-trig
cd sample-trig
make
```

## Running

```
./sample-trig <path sample 1> <path sample 2> ... <path sample n>
```

## Default limitation
- Sample-trig is limited to 6 samples
- Sample-trig do only supports the audio file format: .wav signed 16 bit little-endian 
- The default ALSA pcm device name is 'default' which should be on most Linux desktop machine Pulseaudio. Use in code macro `SAMPLE_TRIG_PCM_NAME` to adapt pcm device. 
- If use more then one samples, please note the pcm device must be mutlithread to mix samples
