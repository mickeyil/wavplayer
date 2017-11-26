#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <soundio/soundio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include	<sndfile.hh>

// this argument controls the number of frames that are copied each time
// the write callback function is called.
// larger number copies more frames thus creates more robustness against
// operating system caused delays, but gives less certainty on the exact
// play time. Smaller number increases the certainty at the expense of
// risking exposure to buffer underrun.
static const int MAX_FRAMES_QUANTA = 800;


// This pointer holds the buffer which will contain the whole
// WAV data.
// The data is stored as S16LE (singed integer 16 bits, little endian)
// Which means that it basically looks like this:
//
// byte index:                 01 23 45 67 89
//                             L |R |L |R |L |R ...
// uint16 (short) index (pos): 0  1  2  3  4
// frame index                 0     1     2

short *buffer = NULL;

// global position uint16 pointer
size_t pos = 0;

size_t buf_size = 0;

SndfileHandle file;

const char * input_filename = nullptr;

static void write_callback(struct SoundIoOutStream *outstream,
        int frame_count_min, int frame_count_max)
{
    const struct SoundIoChannelLayout *layout = &outstream->layout;
    struct SoundIoChannelArea *areas;
    int frames_left = frame_count_max;
    frames_left = std::min(frames_left, MAX_FRAMES_QUANTA);
    int err;

    int channels = layout->channel_count;

    if (2*(pos + channels * frames_left) >= buf_size) {
      printf("LOOP!");
      pos = 0;
    }

    while (frames_left > 0) {
        int frame_count = frames_left;

        if ((err = soundio_outstream_begin_write(outstream, &areas, &frame_count))) {
            fprintf(stderr, "%s\n", soundio_strerror(err));
            exit(1);
        }

        if (!frame_count)
            break;

        for (int frame = 0; frame < frame_count; frame += 1) {
            for (int channel = 0; channel < channels; channel += 1) {
                
                short *ptr = (short*)(areas[channel].ptr + areas[channel].step * frame);
                
                // hard coded: 2 number of channels
                *ptr = buffer[pos + 2*frame + channel];
            }
        }


        // update position buffer with the amount of frames played
        pos += channels * frame_count;

        double time_sec = (double) pos / 2.0 / 44100.0;
        printf("pos = %lu, time_est = %.3lf\n", pos, time_sec);

        if ((err = soundio_outstream_end_write(outstream))) {
            fprintf(stderr, "%s\n", soundio_strerror(err));
            exit(1);
        }

        frames_left -= frame_count;
    }
}

int main(int argc, char **argv) {

		if (argc <= 1) {
        printf("Usage: %s file.wav\n", argv[0]);
        exit(1);
    }
    input_filename = argv[1];
    printf("playing: %s\n", input_filename);

    int err;
    struct SoundIo *soundio = soundio_create();
    if (!soundio) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    if ((err = soundio_connect_backend(soundio, SoundIoBackendAlsa))) {
        fprintf(stderr, "error connecting: %s\n", soundio_strerror(err));
        return 1;
    }

    soundio_flush_events(soundio);

    int default_out_device_index = soundio_default_output_device_index(soundio);
    if (default_out_device_index < 0) {
        fprintf(stderr, "no output device found\n");
        return 1;
    }

    struct SoundIoDevice *device = soundio_get_output_device(soundio, default_out_device_index);
    if (!device) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    fprintf(stderr, "Output device: %s\n", device->name);

    struct SoundIoOutStream *outstream = soundio_outstream_create(device);
    if (!outstream) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    
    // pre load WAV data into a buffer
    file = SndfileHandle (input_filename);

    printf ("Opened file '%s'\n", input_filename) ;
    printf ("    Sample rate : %d\n", file.samplerate ()) ;
    printf ("    Channels    : %d\n", file.channels ()) ;
    printf ("    Frames      : %lu\n", file.frames ()) ;
    const int bytes_per_sample = 2;

    buf_size = file.frames() * file.channels() * bytes_per_sample;
    printf("buffer size: %zu\n", buf_size);

    buffer = (short*) malloc (buf_size);
    sf_count_t n_read = file.readRaw (buffer, buf_size);
    printf("n_read = %lu\n", n_read);

    outstream->format = SoundIoFormatS16LE;
    outstream->sample_rate = 44100;
    outstream->write_callback = write_callback;

    if ((err = soundio_outstream_open(outstream))) {
        fprintf(stderr, "unable to open device: %s", soundio_strerror(err));
        return 1;
    }

    if (outstream->layout_error)
        fprintf(stderr, "unable to set channel layout: %s\n", soundio_strerror(outstream->layout_error));

    if ((err = soundio_outstream_start(outstream))) {
        fprintf(stderr, "unable to start device: %s\n", soundio_strerror(err));
        return 1;
    }

    for (;;)
        soundio_wait_events(soundio);

    soundio_outstream_destroy(outstream);
    soundio_device_unref(device);
    soundio_destroy(soundio);
    free(buffer);
    return 0;
}

