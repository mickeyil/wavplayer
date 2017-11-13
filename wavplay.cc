// build with: g++ -Wall -o wp6 wp6.cc -lsoundio -lsndfile
// install packages (Ubuntu 16.04):
// $ sudo apt-get install libsndfile1-dev libsoundio-dev

#include <soundio/soundio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include	<sndfile.hh>


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

SndfileHandle file;



static void write_callback(struct SoundIoOutStream *outstream,
        int frame_count_min, int frame_count_max)
{
    const struct SoundIoChannelLayout *layout = &outstream->layout;
    struct SoundIoChannelArea *areas;
    int frames_left = frame_count_max;
    int err;

    while (frames_left > 0) {
        int frame_count = frames_left;

        if ((err = soundio_outstream_begin_write(outstream, &areas, &frame_count))) {
            fprintf(stderr, "%s\n", soundio_strerror(err));
            exit(1);
        }

        if (!frame_count)
            break;

        for (int frame = 0; frame < frame_count; frame += 1) {
            for (int channel = 0; channel < layout->channel_count; channel += 1) {
                
                short *ptr = (short*)(areas[channel].ptr + areas[channel].step * frame);
                
                // hard coded: 2 number of channels
                *ptr = buffer[pos + 2*frame + channel];
            }
        }

        // hard coded: 2 channels
        pos += 2*frame_count;

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
    const char *fname = "beautiful1.wav";
    file = SndfileHandle (fname);

    printf ("Opened file '%s'\n", fname) ;
    printf ("    Sample rate : %d\n", file.samplerate ()) ;
    printf ("    Channels    : %d\n", file.channels ()) ;
    printf ("    Frames      : %lu\n", file.frames ()) ;
    const int bytes_per_sample = 2;

    int buf_size = file.frames() * file.channels() * bytes_per_sample;
    printf("buffer size: %d\n", buf_size);

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

